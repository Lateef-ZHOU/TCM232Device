#pragma once
#include <DeviceBase.h>
#include "Tcm232Handle.h"
#include "Tcm232Protocol.h"

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
        const auto reply = Protocol::parseReply(body);
        if (const auto *p = std::get_if<Protocol::Parameter>(&reply)) {
            Q_EMIT parameterReported(at, p->name.toByteArray(), p->value.toByteArray(), p->address);
        } else if (const auto *ack = std::get_if<Protocol::CommandReply>(&reply)) {
            Q_EMIT commandReplied(at, ack->code, ack->address);
        } else {
            switch (std::get<Protocol::Error>(reply)) {
            case Protocol::Error::Address: Q_EMIT logMessage(QStringLiteral("温控回报地址无效")); break;
            case Protocol::Error::ParameterName: Q_EMIT logMessage(QStringLiteral("温控回报缺少有效参数名或等号")); break;
            case Protocol::Error::ReplyCode: Q_EMIT logMessage(QStringLiteral("温控命令应答码无效")); break;
            }
        }
    }
};
}
