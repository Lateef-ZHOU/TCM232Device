#pragma once
#include <chrono>
#include <optional>
#include <DeviceBase.h>
#include "Tcm232Handle.h"

namespace Tcm232
{
class Tcm232Device final
#ifdef Q_MOC_RUN
    : public Tcm232::Handle
#else
    : public Device<Tcm232::Handle>
#endif
{
    Q_OBJECT
public:
    explicit Tcm232Device(QObject *parent = nullptr)
        : Device<Tcm232::Handle>(QStringLiteral("温控器"), QStringLiteral("串口 · TCM232"), parent)
    {
        static_assert(FinalDevice<Tcm232Device, decltype(&Tcm232Device::PreDestruction)>);
    }
    ~Tcm232Device() override { PreDestruction(); }

    // 同一设备的所有命令共用 >50 ms 的串口间隔；调用者可根据返回值安排下一次发送。
    [[nodiscard]] bool sendCommand(const QByteArray &body)
    {
        if (!canEngage({})) {
            Q_EMIT logMessage(QStringLiteral("温控器未连接或正被占用，命令未发送"));
            return false;
        }
        if (lastWrite_ && std::chrono::steady_clock::now() - *lastWrite_ <= std::chrono::milliseconds(50)) {
            Q_EMIT logMessage(QStringLiteral("温控命令间隔须大于 50 ms，命令未发送"));
            return false;
        }
        const bool ok = Handle::send(body);
        lastWrite_ = std::chrono::steady_clock::now();
        return ok;
    }

Q_SIGNALS:
    void lockChanged(HandleCore::ResourcePhase phase) override;
    // address 为 -1 表示这条回报未携带地址。参数名保留全部层级，值保留设备原文。
    void parameterReported(RxTime at, QByteArray name, QByteArray value, int address);
    void commandReplied(RxTime at, int code, int address);

private:
    using Handle::send;
    void PreDestruction() noexcept final { Device<Tcm232::Handle>::PreDestruction(); }

    void onData(const QByteArray &body, RxTime at) final
    {
        QByteArrayView line(body);
        int address = -1;
        const auto suffix = line.lastIndexOf('@');
        if (suffix >= 0) {
            address = detail::addressValue(line.sliced(suffix + 1));
            if (address < 0) {
                Q_EMIT logMessage(QStringLiteral("温控回报地址无效"));
                return;
            }
            line = line.first(suffix);
        }
        const auto eq = line.indexOf('=');
        if (eq < 1 || !line.first(eq).contains(':')) {
            Q_EMIT logMessage(QStringLiteral("温控回报缺少模块参数名或等号"));
            return;
        }
        const auto name = line.first(eq);
        const auto value = line.sliced(eq + 1);
        if (name == "CMD:REPLY") {
            if (value.size() != 1 || value.front() < '0' || value.front() > '8') {
                Q_EMIT logMessage(QStringLiteral("温控命令应答码无效"));
                return;
            }
            Q_EMIT commandReplied(at, value.front() - '0', address);
        } else {
            Q_EMIT parameterReported(at, name.toByteArray(), value.toByteArray(), address);
        }
    }

    std::optional<std::chrono::steady_clock::time_point> lastWrite_;
};
}
