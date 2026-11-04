#pragma once
#include "Tcm232DeviceT.h"
#include "Tcm232Handle.h"

namespace Tcm232
{
class Tcm232Device final
#ifdef Q_MOC_RUN
    : public Tcm232::Handle
#else
    : public Tcm232DeviceT<Tcm232::Handle>
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString displayName MEMBER displayName CONSTANT)
    Q_PROPERTY(QString subtitle MEMBER subtitle CONSTANT)
    Q_PROPERTY(bool isOpen READ isOpen)
    Q_PROPERTY(QVariant holder READ qmlHolder)
    using Base = Tcm232DeviceT<Tcm232::Handle>;
public:
    static Tcm232Device *qmlAttachedProperties(QObject *owner) { return new Tcm232Device(owner); }
    explicit Tcm232Device(QObject *parent)
        : Base(QStringLiteral("串口 · TCM232"), parent)
    {
        static_assert(FinalDevice<Tcm232Device, decltype(&Tcm232Device::PreDestruction)>);
    }
    ~Tcm232Device() override { PreDestruction(); }

    Q_INVOKABLE QStringList ports() const
    {
        QStringList names;
        for (const auto &port : availablePorts()) names.append(port.portName);
        return names;
    }
    using Handle::open;
    Q_INVOKABLE bool open(const QString &port)
    {
        if (port.trimmed().isEmpty()) {
            Q_EMIT logMessage(QStringLiteral("请选择串口"));
            return false;
        }
        return Handle::open(parameters({port, {}}));
    }
    Q_INVOKABLE void close() { Handle::close(); }

    Q_INVOKABLE bool query(Tcm232::Module module, Tcm232::Command command) { return Base::query(module, command); }
    Q_INVOKABLE bool setParameter(Tcm232::Module module, Tcm232::Command command, const QVariant &value)
    { return Base::setParameter(module, command, value); }
    Q_INVOKABLE bool saveParameter(Tcm232::Module module, Tcm232::Command command)
    { return Base::saveParameter(module, command); }
Q_SIGNALS:
    void lockChanged(HandleCore::ResourcePhase phase) override;
    void packetReceived(RxTime at, const Tcm232::Packet &packet) override;

private:
    void PreDestruction() noexcept final { Base::PreDestruction(); }
};
}
