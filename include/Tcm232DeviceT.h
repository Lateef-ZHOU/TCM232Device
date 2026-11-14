#pragma once
#include <vector>
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
        const auto body = Protocol::pack({{}, module, command, QVariant::fromValue(ValueType::Query)});
        return Transport::send(std::vector<unsigned char>{reinterpret_cast<const unsigned char *>(body.constData()),
                              reinterpret_cast<const unsigned char *>(body.constData()) + body.size()});
    }
    bool setParameter(Tcm232::Module module, Tcm232::Command command, const QVariant &value)
    {
        const auto body = Protocol::pack({{}, module, command, value});
        return Transport::send(std::vector<unsigned char>{reinterpret_cast<const unsigned char *>(body.constData()),
                              reinterpret_cast<const unsigned char *>(body.constData()) + body.size()});
    }
    bool saveParameter(Tcm232::Module module, Tcm232::Command command)
    {
        const auto body = Protocol::pack({{}, module, command, QVariant::fromValue(ValueType::Save)});
        return Transport::send(std::vector<unsigned char>{reinterpret_cast<const unsigned char *>(body.constData()),
                              reinterpret_cast<const unsigned char *>(body.constData()) + body.size()});
    }

private:
    using Transport::send;
    void onData(std::span<const unsigned char> bytes, RxTime at) final
    {
        const auto body = QByteArray::fromRawData(reinterpret_cast<const char *>(bytes.data()),
                                                static_cast<qsizetype>(bytes.size()));
        const auto parsed = Protocol::parse(body);
        if (const auto *error = std::get_if<Protocol::Error>(&parsed)) {
            switch (*error) {
            case Protocol::Error::Address: Q_EMIT this->logMessage("温控回报地址无效"); break;
            case Protocol::Error::ModuleName: Q_EMIT this->logMessage("温控回报模块未知"); break;
            case Protocol::Error::CommandName: Q_EMIT this->logMessage("温控回报命令未知"); break;
            case Protocol::Error::Value: Q_EMIT this->logMessage("温控回报值无效"); break;
            case Protocol::Error::Syntax: Q_EMIT this->logMessage("温控回报语法无效"); break;
            }
            return;
        }
        const auto &packet = std::get<Protocol::Packet>(parsed);
        if (packet.value.metaType() == QMetaType::fromType<Protocol::ValueType>()) {
            Q_EMIT this->logMessage("温控收到查询或保存命令，预期为带值回报");
            return;
        }
        Q_EMIT packetReceived(at, packet);
    }
};
}
