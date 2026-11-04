#include <Tcm232Device.h>
#include "FakeSerialPort.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <memory>
#include <vector>

Q_IMPORT_QML_PLUGIN(Tcm232TypesPlugin)

class ReceiverProbe final : public QObject
{
    Q_OBJECT
public:
    struct Event { RxTime at; QString route; QVariant value, address; };
    std::vector<Event> events;

    Q_INVOKABLE void record(RxTime at, QString route, QVariant value, QVariant address)
    {
        if (QThread::currentThread() != qApp->thread()) qFatal("QML ran outside the main thread");
        events.push_back({at, route, value, address});
    }
};

void verifyQml()
{
    FakeSerialPort::reset();
    Tcm232::Tcm232Device device;
    ReceiverProbe probe;
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml
        import Lidar.Tcm232
        Connections {
            required property QtObject backend
            required property QtObject probe
            target: backend
            function onPacketReceived(at, packet: tcm232Packet) {
                let route = "unexpected"
                if (packet.module === Tcm232Protocol.CMD && packet.command === Tcm232Protocol.REPLY && packet.address === 0)
                    route = "ack"
                if (packet.command === Tcm232Protocol.TCACTTEMP) {
                    if (packet.module === Tcm232Protocol.TC1 && packet.address === undefined) route = "first"
                    if (packet.module === Tcm232Protocol.TC2 && packet.address === 255) route = "second"
                }
                probe.record(at, route, packet.value, packet.address)
            }
        })", QUrl());
    std::unique_ptr<QObject> listener(component.createWithInitialProperties({
        {"backend", QVariant::fromValue(&device)}, {"probe", QVariant::fromValue(&probe)}}));
    if (!listener) qFatal("%s", qPrintable(component.errorString()));

    std::vector<RxTime> sent;
    QObject::connect(&device, &Tcm232::Tcm232Device::packetReceived, &device,
        [&](RxTime at, const Tcm232::Packet &) {
            if (QThread::currentThread() == qApp->thread()) qFatal("expected serial worker");
            sent.push_back(at);
        }, Qt::DirectConnection);
    if (!device.open(device.parameters({QStringLiteral("controlled"), {}}))) qFatal("open rejected");
    FakeSerialPort::enqueue("TC1:TCACTTEMP?\rTC2:TCACTTEMP!\rTC2:TCACTTEMP=oops\r"
                            "TC1:TCACTTEMP=24.5\rCMD:REPLY=4@0\rTC2:TCACTTEMP=-1.5@255\r");
    QElapsedTimer timeout;
    timeout.start();
    while (probe.events.size() < 3 && timeout.elapsed() < 3000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    device.close();
    QCoreApplication::processEvents();
    const auto &e = probe.events;
    if (e.size() != 3 || sent.size() != 3) qFatal("expected 3 valid packets only");
    for (size_t i = 0; i < sent.size(); ++i)
        if (sent[i] == RxTime{} || sent[i] != e[i].at) qFatal("RxTime changed in QML");
    if (e[0].route != "first" || e[0].value.toDouble() != 24.5 || e[0].address.isValid()
        || e[1].route != "ack" || e[1].value.toInt() != 4 || !e[1].address.isValid() || e[1].address.toInt() != 0
        || e[2].route != "second" || e[2].value.toDouble() != -1.5 || e[2].address.toInt() != 255)
        qFatal("wrong QML route or fields");
}

#include "qml.moc"
