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
    void parameterReported(RxTime at, Tcm232::Protocol::Module module, QByteArray parameter,
                           QVariant value, std::optional<quint8> address);
    void commandReplied(RxTime at, int code, std::optional<quint8> address);

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
            case Protocol::Error::Syntax: Q_EMIT logMessage(QStringLiteral("温控回报语法无效")); break;
            }
            return;
        }
        const auto &packet = std::get<Protocol::Packet>(parsed);
        if (packet.operation != Protocol::Operation::Equals) {
            Q_EMIT logMessage(QStringLiteral("温控收到查询或保存命令，预期为带值回报"));
            return;
        }
        const auto value = Protocol::parseValue(packet);
        if (!value.isValid()) {
            Q_EMIT logMessage(QStringLiteral("温控回报参数值无效：") + QString::fromLatin1(packet.parameter));
            return;
        }
        if (packet.module == Protocol::Module::CMD)
            Q_EMIT commandReplied(at, value.toInt(), packet.address);
        else
            Q_EMIT parameterReported(at, packet.module, packet.parameter.toByteArray(), value, packet.address);
    }
};
}
