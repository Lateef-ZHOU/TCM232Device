#include <Tcm232Device.h>
#include <LaserTelemetryItem.h>
#include <QQmlListReference>
#include "FakeSerialPort.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QJSValue>
#include <QtQml/qqmlextensionplugin.h>
#include <memory>
#include <vector>

Q_IMPORT_QML_PLUGIN(Tcm232TypesPlugin)
Q_IMPORT_QML_PLUGIN(LidarUIPlugin)
Q_IMPORT_QML_PLUGIN(LidarChartsPlugin)

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

static void verifyPage(QQmlEngine &engine)
{
    FakeSerialPort::reset();
    QQmlComponent component(&engine, "Lidar.Tcm232", "Tcm232Module");
    std::unique_ptr<QObject> page(component.create());
    if (!page) qFatal("%s", qPrintable(component.errorString()));
    auto *device = page->property("deviceState").value<Tcm232::Tcm232Device *>();
    if (!device || device->parent() != page.get() || device->isOpen()) qFatal("page ownership");
    auto control = [&](const char *name) {
        auto *item = page->findChild<QObject *>(QString::fromLatin1(name));
        if (!item) qFatal("missing control %s", name);
        return item;
    };
    auto wait = [](auto predicate) {
        QElapsedTimer limit;
        limit.start();
        while (!predicate() && limit.elapsed() < 3000) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        if (!predicate()) qFatal("timed out waiting for page");
    };
    if (!device->open(QStringLiteral("controlled"))) qFatal("open");
    wait([] { return FakeSerialPort::writes().size() >= 10; });
    for (const auto &body : FakeSerialPort::writes())
        if (!body.endsWith("?\r")) qFatal("connect must only query");
    auto *tc = control("tcDraft");
    auto *fan = control("fanDraft");
    tc->setProperty("text", "32.5");
    fan->setProperty("text", "40");
    FakeSerialPort::enqueue("TC1:TCADJTEMP=18\rFAN1:FANSTARTTEMP=31\rTC1:TCSW=1\rTC1:TCOE=0\r"
                            "TC1:TCACTVOL=4.5\rTC1:TCACTCUR=0.6\rTC1:TCACTTEMP=22\r"
                            "FAN1:FANACTTEMP=27\rFAN1:FANSPEED=800\rTC2:TCADJTEMP=99\r"
                            "CMD:REPLY=4\rTC1:TCERRORSTATUS=254\r");
    wait([&] { return control("errors")->property("text").toString() == QStringLiteral("外部错误"); });
    using C = Tcm232::Command;
    auto report = [&](C command) {
        return page->property("reports").value<QJSValue>().property(QString::number(int(command))).toNumber();
    };
    if (report(C::TCADJTEMP) != 18 || report(C::FANSTARTTEMP) != 31 || report(C::TCSW) != 1
        || report(C::TCOE) != 0 || report(C::TCACTVOL) != 4.5 || report(C::TCACTCUR) != 0.6
        || report(C::TCACTTEMP) != 22 || report(C::FANACTTEMP) != 27 || report(C::FANSPEED) != 800)
        qFatal("wrong module, setting or measurement");
    if (tc->property("text") != "32.5" || fan->property("text") != "40") qFatal("reports changed drafts");
    QQmlListReference pages(page.get(), "mainPages"), sections(page.get(), "sections");
    if (pages.count() != 1 || sections.count() != 3
        || sections.at(1)->property("title") != QStringLiteral("温控1")
        || sections.at(2)->property("title") != QStringLiteral("风扇1")) qFatal("wrong page layout");
    auto panel = [&](const char *name, const QStringList &keys) {
        auto *item = pages.at(0)->findChild<LaserTelemetryItem *>(name);
        if (!item || item->channelKeys() != keys) qFatal("wrong chart channels");
        item->applyHover(1e12);
        if (!item->hover()) qFatal("missing chart measurement");
        return item;
    };
    auto *temps = panel("temperatures", {"tcTemperature", "fanTemperature"});
    auto *volts = panel("voltage", {"outputVoltage"});
    auto *amps = panel("currents", {"outputCurrent"});
    if (!temps->hover()->text.contains(QStringLiteral("温控1实际温度  22.00"))
        || !temps->hover()->text.contains(QStringLiteral("风扇1实际温度  27.00"))
        || !volts->hover()->text.contains(QStringLiteral("实际输出电压  4.50 V"))
        || !amps->hover()->text.contains(QStringLiteral("实际输出电流  0.60 A")))
        qFatal("wrong legend or value");
    const auto lastSample = temps->hover()->at;
    Q_EMIT device->packetReceived(RxTime(std::chrono::seconds(2000000000)), {{}, Tcm232::Module::FAN1, C::FANSPEED, 888});
    temps->applyHover(1e12);
    if (temps->hover()->at != lastSample) qFatal("RPM added a chart sample");
    Q_EMIT device->packetReceived(RxTime(std::chrono::seconds(2000000001)), {{}, Tcm232::Module::TC1, C::TCACTTEMP, 22.0});
    for (auto *item : {temps, volts, amps}) {
        item->applyHover(1e12);
        if (item->hover()->at != 2000000001) qFatal("steady reading did not advance all plots together");
    }
    auto click = [&](const char *name, std::vector<QByteArray> expected) {
        const auto before = FakeSerialPort::writes().size();
        if (!QMetaObject::invokeMethod(control(name), "clicked")) qFatal("click failed");
        const auto writes = FakeSerialPort::writes();
        if (std::vector<QByteArray>(writes.begin() + before, writes.end()) != expected)
            qFatal("unexpected serial command(s) from %s", name);
    };
    click("tcSave", {"TC1:TCADJTEMP!\r"});
    click("fanSave", {"FAN1:FANSTARTTEMP!\r"});
    click("tcSet", {"TC1:TCADJTEMP=32.5\r", "TC1:TCADJTEMP?\r"});
    click("fanSet", {"FAN1:FANSTARTTEMP=40\r", "FAN1:FANSTARTTEMP?\r"});
    click("outputOn", {"TC1:TCSW=1\r", "TC1:TCSW?\r"});
    device->close();
    wait([&] { return control("errors")->property("text").toString() == QStringLiteral("—"); });
}

void verifyQml()
{
    FakeSerialPort::reset();
    Tcm232::Tcm232Device device(nullptr);
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
    verifyPage(engine);
}

#include "qml.moc"
