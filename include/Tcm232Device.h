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
        // QML 跨线程排队按信号签名查找这个标准时间类型的别名。
        qRegisterMetaType<RxTime>("RxTime");
        static_assert(FinalDevice<Tcm232Device, decltype(&Tcm232Device::PreDestruction)>);
    }
    ~Tcm232Device() override { PreDestruction(); }

Q_SIGNALS:
    void lockChanged(HandleCore::ResourcePhase phase) override;
    void packetReceived(RxTime at, const Tcm232::Packet &packet);

private:
    using Handle::send;
    void PreDestruction() noexcept final { Device<Tcm232::Handle>::PreDestruction(); }

    void onData(const QByteArray &body, RxTime at) final
    {
        const auto parsed = Protocol::parse(body);
        if (const auto *error = std::get_if<Protocol::Error>(&parsed)) {
            switch (*error) {
            case Protocol::Error::Address: Q_EMIT logMessage(QStringLiteral("温控回报地址无效")); break;
            case Protocol::Error::ModuleName: Q_EMIT logMessage(QStringLiteral("温控回报模块未知")); break;
            case Protocol::Error::CommandName: Q_EMIT logMessage(QStringLiteral("温控回报命令未知")); break;
            case Protocol::Error::Value: Q_EMIT logMessage(QStringLiteral("温控回报值无效")); break;
            case Protocol::Error::Syntax: Q_EMIT logMessage(QStringLiteral("温控回报语法无效")); break;
            }
            return;
        }
        const auto &packet = std::get<Protocol::Packet>(parsed);
        if (packet.value.metaType() == QMetaType::fromType<Protocol::ValueType>()) {
            Q_EMIT logMessage(QStringLiteral("温控收到查询或保存命令，预期为带值回报"));
            return;
        }
        Q_EMIT packetReceived(at, packet);
    }
};
}
