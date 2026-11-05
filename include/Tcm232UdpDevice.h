#pragma once
#include <PcapHandle.h>
#include "Tcm232DeviceT.h"

namespace Tcm232 {
// 桥接 UDP 的载荷就是协议主体，每包一条温控命令或回报，CR 归板端串口 Handle。
class Tcm232UdpDevice final
#ifdef Q_MOC_RUN
    : public PcapHandle
#else
    : public Tcm232DeviceT<PcapHandle>
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString displayName MEMBER displayName CONSTANT)
    Q_PROPERTY(QString subtitle MEMBER subtitle CONSTANT)
    Q_PROPERTY(bool isOpen READ isOpen)
    Q_PROPERTY(QVariant holder READ qmlHolder)
    Q_PROPERTY(QVariantMap connectionParameters READ connectionParameters CONSTANT)
    using Base = Tcm232DeviceT<PcapHandle>;
public:
    static Tcm232UdpDevice *qmlAttachedProperties(QObject *owner) { return new Tcm232UdpDevice(owner); }
    explicit Tcm232UdpDevice(QObject *parent) : Base(QStringLiteral("TCM232"), parent)
    { static_assert(FinalDevice<Tcm232UdpDevice, decltype(&Tcm232UdpDevice::PreDestruction)>); }
    ~Tcm232UdpDevice() override { PreDestruction(); }
    [[nodiscard]] QVariantMap connectionParameters() const
    {
        QStringList names, tips;
        for (const auto &entry : interfaces()) { names << entry.friendlyName; tips << entry.description; }
        return {{"networkInterfaceOptions", names}, {"networkInterfaceTips", tips}};
    }
    Q_INVOKABLE bool open(int interfaceIndex)
    {
        if (interfaceIndex < 0 || interfaceIndex >= interfaces().size()) {
            Q_EMIT logMessage(QStringLiteral("未选择有效网卡，连接请求未提交"));
            return false;
        }
        return PcapHandle::open(parameters(interfaces()[interfaceIndex]));
    }
    Q_INVOKABLE void close() { Base::close(); }
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

    // 选项与下标共用同一次枚举；不缓存连接表单或第二份运行参数。
    static const QList<NetworkInterfaceInfo> &interfaces()
    {
        static const auto result = availableInterfaces();
        return result;
    }
    static PcapParams parameters(const NetworkInterfaceInfo &network)
    {
        return {network, IPv4(QByteArray::fromHex("c0a864c8")), IPv4(QByteArray::fromHex("c0a86464")), {},
            55300, 55300}; // Windows 192.168.100.200 ↔ PetaLinux 192.168.100.100；不要求系统配置这些 IP。
    }
};
}
