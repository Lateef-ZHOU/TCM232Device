#pragma once
#include <DeviceBase.h>
#include "Tcm232Protocol.h"

namespace Tcm232
{
template <HandleLike Transport>
class Tcm232DeviceT : public Device<Transport>
{
protected:
    Tcm232DeviceT(QString subtitle, QObject *parent)
        : Device<Transport>(QStringLiteral("温控·业贤"), std::move(subtitle), parent)
    { qRegisterMetaType<RxTime>("RxTime"); }
    virtual void packetReceived(RxTime at, const Tcm232::Packet &packet) = 0;

public:
    QVariant qmlHolder() const
    {
        const auto owner = this->holder();
        return owner ? QVariant(*owner) : QVariant();
    }
    bool query(Tcm232::Module module, Tcm232::Command command)
    {
        return Transport::send(Protocol::pack({{}, module, command, QVariant::fromValue(ValueType::Query)}));
    }
    bool setParameter(Tcm232::Module module, Tcm232::Command command, const QVariant &value)
    {
        return Transport::send(Protocol::pack({{}, module, command, value}));
    }
    bool saveParameter(Tcm232::Module module, Tcm232::Command command)
    {
        return Transport::send(Protocol::pack({{}, module, command, QVariant::fromValue(ValueType::Save)}));
    }

private:
    using Transport::send;
    void onData(const QByteArray &body, RxTime at) final
    {
        const auto parsed = Protocol::parse(body);
        if (const auto *error = std::get_if<Protocol::Error>(&parsed)) {
            switch (*error) {
            case Protocol::Error::Address: Q_EMIT this->logMessage(QStringLiteral("温控回报地址无效")); break;
            case Protocol::Error::ModuleName: Q_EMIT this->logMessage(QStringLiteral("温控回报模块未知")); break;
            case Protocol::Error::CommandName: Q_EMIT this->logMessage(QStringLiteral("温控回报命令未知")); break;
            case Protocol::Error::Value: Q_EMIT this->logMessage(QStringLiteral("温控回报值无效")); break;
            case Protocol::Error::Syntax: Q_EMIT this->logMessage(QStringLiteral("温控回报语法无效")); break;
            }
            return;
        }
        const auto &packet = std::get<Protocol::Packet>(parsed);
        if (packet.value.metaType() == QMetaType::fromType<Protocol::ValueType>()) {
            Q_EMIT this->logMessage(QStringLiteral("温控收到查询或保存命令，预期为带值回报"));
            return;
        }
        Q_EMIT packetReceived(at, packet);
    }
};
}
