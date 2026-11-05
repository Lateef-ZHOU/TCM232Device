#pragma once
#include <memory>
#include <deque>
#include <utility>
#include <QTimer>
#include "Tcm232Protocol.h"
#include <SerialHandle.h>

namespace Tcm232
{
class Framer
{
public:
    // 设备没有同步头；收齐 CR 时使用本次读取的到达时刻。
    void feed(const QByteArray &chunk, RxTime at, const auto &deliver)
    {
        for (const char byte : chunk)
        {
            if (byte == '\r')
            {
                if (!body_.isEmpty() && Protocol::acceptChecksum(body_))
                    deliver(body_, at);
                body_.clear();
                discarding_ = false;
            }
            else if (!discarding_)
            {
                if (body_.size() < MaxBodyBytes)
                    body_.append(byte);
                else
                {
                    body_.clear();
                    discarding_ = true;
                }
            }
        }
    }

    // 本机收包缓冲上限，厂家没有规定协议最大行长。
    static constexpr qsizetype MaxBodyBytes = 4096;

private:
    QByteArray body_;
    bool discarding_ = false;
};

class Handle : public SerialHandle<Framer>
{
    Q_OBJECT
public:
    Handle()
    {
        transmit_.setSingleShot(true);
        transmit_.setTimerType(Qt::PreciseTimer);
        transmit_.setInterval(100);
        QObject::connect(&transmit_, &QTimer::timeout, this, [this] { sendNext(); });
        QObject::connect(this, &Handle::connectionChanged, this, [this](HandleCore::ResourcePhase phase) {
            if (phase == HandleCore::ResourcePhase::AboutToRelease || phase == HandleCore::ResourcePhase::Released) {
                transmit_.stop();
                pending_.clear();
            }
        });
    }

    [[nodiscard]] static SerialParams parameters(const SerialPortInfo &port)
    {
        SerialParams params;
        params.portInfo = port;
        params.baudRate = qToUnderlying(BaudRate::B57600);
        return params;
    }

Q_SIGNALS:
    void connectionChanged(HandleCore::ResourcePhase phase) override;
    void logMessage(const QString &text) const final;

private:
    RXRuling onRX(const QByteArray &body, RxTime at, std::shared_ptr<SerialConn<Framer>>) final
    {
        return {body, RXRuling::Decode, at};
    }

    TXRuling onTX(const QByteArray &body, std::shared_ptr<SerialConn<Framer>> conn) const final
    {
        if (body.isEmpty() || body.contains('\r') || body.contains('\n'))
        {
            Q_EMIT logMessage(QStringLiteral("温控命令须为非空单行主体，未发送"));
            return {};
        }
        if (!conn) {
            Q_EMIT logMessage(QStringLiteral("设备未连接，无法发送数据"));
            return {};
        }
        pending_.emplace_back(std::move(conn), body + '\r');
        if (!transmit_.isActive())
            sendNext();
        return {{}, TXRuling::Queued};
    }

    // 仅所属主线程访问；末条发送后仍计时一次，覆盖空队列期间的新命令。
    void sendNext() const
    {
        if (pending_.empty())
            return;
        auto [conn, frame] = std::move(pending_.front());
        pending_.pop_front();
        (void)connSend(std::move(conn), frame);
        transmit_.start();
    }

    mutable QTimer transmit_{this};
    mutable std::deque<std::pair<std::shared_ptr<SerialConn<Framer>>, QByteArray>> pending_;
};
}
