#pragma once
#include <QtQml/qqml.h>
#include <QtQml/qqmlregistration.h>
#include <HandleBase.h>
#include "Tcm232Device.h"
#include "Tcm232UdpDevice.h"

QML_DECLARE_TYPEINFO(Tcm232::Tcm232Device, QML_HAS_ATTACHED_PROPERTIES)
struct Tcm232BackendQml
{
    Q_GADGET
    QML_FOREIGN(Tcm232::Tcm232Device)
    QML_NAMED_ELEMENT(Tcm232Device)
    QML_ATTACHED(Tcm232::Tcm232Device)
    QML_UNCREATABLE("由温控页面持有")
};

QML_DECLARE_TYPEINFO(Tcm232::Tcm232UdpDevice, QML_HAS_ATTACHED_PROPERTIES)
struct Tcm232UdpBackendQml
{
    Q_GADGET
    QML_FOREIGN(Tcm232::Tcm232UdpDevice)
    QML_NAMED_ELEMENT(Tcm232UdpDevice)
    QML_ATTACHED(Tcm232::Tcm232UdpDevice)
    QML_UNCREATABLE("由温控页面持有")
};

namespace Tcm232QmlEnums
{
Q_NAMESPACE
QML_FOREIGN_NAMESPACE(Tcm232)
QML_NAMED_ELEMENT(Tcm232Protocol)
}

struct Tcm232PacketQml
{
    Q_GADGET
    QML_FOREIGN(Tcm232::Packet)
    QML_VALUE_TYPE(tcm232Packet)
};

struct Tcm232TimeQml
{
    Q_GADGET
    QML_FOREIGN(RxTime)
    QML_ANONYMOUS
};
