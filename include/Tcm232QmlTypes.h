#pragma once
#include <QtQml/qqmlregistration.h>
#include <HandleBase.h>
#include "Tcm232Packet.h"

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
