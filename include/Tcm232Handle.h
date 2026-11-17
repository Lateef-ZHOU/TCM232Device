#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <span>
#include <memory>
#include <deque>
#include <utility>
#include <QTimer>
#include "Tcm232Protocol.h"
#include <SerialHandle.h>

namespace Tcm232
{
// 成帧层最短接受“非空主体 + CR”，共两字节；业务 Packet 的合法性留到上层。
class Handle : public SerialHandle<2 / 2>
{
    Q_OBJECT
public:
    explicit Handle(QObject *parent = nullptr) : SerialHandle(parent)
    {
        transmit_.setSingleShot(true);
        transmit_.setTimerType(Qt::PreciseTimer);
        transmit_.setInterval(75);
        QObject::connect(&transmit_, &QTimer::timeout, this, [this] { sendNext(); });
        QObject::connect(this, &Handle::connectionChanged, this, [this](HandleCore::ResourcePhase phase) {
            if (phase == HandleCore::ResourcePhase::AboutToRelease) {
                transmit_.stop();
                pending_.clear();
            }
        });
    }

    [[nodiscard]] static SerialParams parameters(const std::string &portName)
    {
        SerialParams params;
        params.portName = portName;
        params.baudRate = 57600;
        return params;
    }

Q_SIGNALS:
    void connectionChanged(HandleCore::ResourcePhase phase) override;
    void logMessage(const std::string &text) const final;

public:
    // 本机缓冲上限；厂家未规定协议最大行长。
    static constexpr qsizetype MaxBodyBytes = 4096;

protected:
    // SerialConn 每次交入一个字节；回车完成本帧，输出视图保留至下次 onRX。
    RXRuling onRX(std::span<const unsigned char> view, RxTime at, std::shared_ptr<SerialConn>) final
    {
        const auto byte = view.front();
        if (byte == '\r') {
            discarding_ = false;
            if (!body_.isEmpty() && Protocol::acceptChecksum(body_)) {
                complete_.swap(body_);
                body_.clear();
                return {{reinterpret_cast<const unsigned char *>(complete_.constData()),
                         static_cast<std::size_t>(complete_.size())}, RXRuling::Decode, at};
            }
            body_.clear();
        } else if (!discarding_) {
            if (body_.size() < MaxBodyBytes)
                body_.append(static_cast<char>(byte));
            else {
                body_.clear();
                discarding_ = true;
            }
        }
        return {};
    }

private:
    std::shared_ptr<SerialConn> connOpen(const SerialParams &params) final
    {
        body_.clear();
        discarding_ = false;
        return SerialHandle::connOpen(params);
    }

    TXRuling onTX(const std::vector<unsigned char> &payload, std::shared_ptr<SerialConn> conn) const final
    {
        if (payload.empty() || std::ranges::any_of(payload, [](unsigned char byte) { return byte == '\r' || byte == '\n'; }))
        {
            Q_EMIT logMessage("温控命令须为非空单行主体，未发送");
            return {};
        }
        if (!conn) {
            Q_EMIT logMessage("设备未连接，无法发送数据");
            return {};
        }
        std::vector<unsigned char> frame;
        frame.reserve(payload.size() + 1);
        frame.insert(frame.end(), payload.begin(), payload.end());
        frame.push_back('\r');
        pending_.emplace_back(conn, std::move(frame));
        if (!transmit_.isActive())
            sendNext();
        return {{}, TXRuling::Queued};
    }

    // 仅所属主线程访问；末条发送后仍计时一次，覆盖空队列期间的新命令。
    void sendNext() const
    {
        while (!pending_.empty()) {
            auto [connection, frame] = std::move(pending_.front());
            pending_.pop_front();
            // 自动重连已释放的旧连接不再发送，也不占用新命令的冷却时隙。
            if (auto conn = connection.lock()) {
                (void)connSend(std::move(conn), frame);
                transmit_.start();
                return;
            }
        }
    }

    // 仅收包线程使用；complete_ 每次交付前由新主体交换替换，并保活裁决借用的字节。
    QByteArray body_, complete_;
    bool discarding_ = false;
    mutable QTimer transmit_{this};
    // 待发字节自有，连接只作弱关联；失联后不因排队延长旧串口寿命。
    mutable std::deque<std::pair<std::weak_ptr<SerialConn>, std::vector<unsigned char>>> pending_;
};
}
