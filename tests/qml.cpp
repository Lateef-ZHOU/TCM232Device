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
#include <algorithm>
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

// 页面验证只观察提交边界；真实串口节奏在 serial_delivery 中单独验证。
class PageDeviceProbe final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(QString subtitle READ subtitle CONSTANT)
    Q_PROPERTY(bool isOpen READ isOpen)
public:
    std::vector<QByteArray> commands;
    bool connected = false;
    QString displayName() const { return QStringLiteral("温控器"); }
    QString subtitle() const { return QStringLiteral("测试"); }
    bool isOpen() const { return connected; }
    Q_INVOKABLE bool query(Tcm232::Module module, Tcm232::Command command)
    { return record({{}, module, command, QVariant::fromValue(Tcm232::ValueType::Query)}); }
    Q_INVOKABLE bool setParameter(Tcm232::Module module, Tcm232::Command command, QVariant value)
    { return record({{}, module, command, value}); }
    Q_INVOKABLE bool saveParameter(Tcm232::Module module, Tcm232::Command command)
    { return record({{}, module, command, QVariant::fromValue(Tcm232::ValueType::Save)}); }
    void open() { connected = true; Q_EMIT connectionChanged(HandleCore::ResourcePhase::Acquired); }
    void close()
    {
        Q_EMIT connectionChanged(HandleCore::ResourcePhase::AboutToRelease);
        connected = false;
        Q_EMIT connectionChanged(HandleCore::ResourcePhase::Released);
    }
Q_SIGNALS:
    void connectionChanged(HandleCore::ResourcePhase phase);
    void packetReceived(RxTime at, const Tcm232::Packet &packet);
    void logMessage(const QString &text);
private:
    bool record(const Tcm232::Packet &packet)
    {
        if (!connected) return false;
        commands.push_back(Tcm232::Protocol::pack(packet));
        return true;
    }
};

static void verifyPage(QQmlEngine &engine)
{
    PageDeviceProbe device;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import Lidar.UI
        import Lidar.Tcm232
        Tcm232Page {
            required property QtObject backend
            deviceState: backend
            connection: Section { title: "测试连接" }
        })", QUrl());
    std::unique_ptr<QObject> page(component.createWithInitialProperties({{"backend", QVariant::fromValue(&device)}}));
    if (!page) qFatal("%s", qPrintable(component.errorString()));
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
    const std::vector<QByteArray> round{
        "TC1:TCADJTEMP?", "TC1:TCACTTEMP?", "TC1:TCSW?", "TC1:TCOE?", "TC1:TCACTVOL?",
        "TC1:TCACTCUR?", "TC1:TCERRORSTATUS?", "FAN1:FANSTARTTEMP?", "FAN1:FANACTTEMP?", "FAN1:FANSPEED?"};
    auto checkRound = [&](size_t start) {
        if (device.commands.size() != start + round.size()
            || !std::equal(round.begin(), round.end(), device.commands.begin() + start))
            qFatal("query collection was not submitted together");
    };
    QElapsedTimer tick;
    tick.start();
    device.open();
    if (device.commands[0] != "SYSTEM:SYSMODEL?" || device.commands[1] != "SYSTEM:SYSSN?") qFatal("identity query");
    checkRound(2);
    auto *tc = control("tcDraft");
    auto *fan = control("fanDraft");
    tc->setProperty("text", "32.5");
    fan->setProperty("text", "40");
    using C = Tcm232::Command;
    using M = Tcm232::Module;
    auto reply = [&](M module, C command, QVariant value, int at = 2000000000) {
        Q_EMIT device.packetReceived(RxTime(std::chrono::seconds(at)), {{}, module, command, value});
    };
    reply(M::SYSTEM, C::SYSMODEL, QStringLiteral("TCM232"));
    reply(M::SYSTEM, C::SYSSN, QStringLiteral("232-TEST"));
    reply(M::TC1, C::TCADJTEMP, 18.0);
    reply(M::FAN1, C::FANSTARTTEMP, 31.0);
    reply(M::TC1, C::TCSW, 1);
    reply(M::TC1, C::TCOE, 0);
    reply(M::TC1, C::TCACTVOL, 4.5);
    reply(M::TC1, C::TCACTCUR, 0.6);
    reply(M::TC1, C::TCACTTEMP, 22.0);
    reply(M::FAN1, C::FANACTTEMP, 27.0);
    reply(M::FAN1, C::FANSPEED, 800);
    reply(M::TC2, C::TCADJTEMP, 99.0);
    reply(M::CMD, C::REPLY, 4);
    reply(M::TC1, C::TCERRORSTATUS, 254);
    if (control("errors")->property("text") != QStringLiteral("外部错误")) qFatal("error report");
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
    auto *temps = pages.at(0)->findChild<LaserTelemetryItem *>("temperatures");
    auto *volts = pages.at(0)->findChild<LaserTelemetryItem *>("voltage");
    auto *amps = pages.at(0)->findChild<LaserTelemetryItem *>("currents");
    if (!temps || !volts || !amps || temps->channelKeys() != QStringList{"tcTemperature", "fanTemperature"}
        || volts->channelKeys() != QStringList{"outputVoltage"} || amps->channelKeys() != QStringList{"outputCurrent"})
        qFatal("chart channels");
    temps->applyHover(1e12);
    if (temps->hover()) qFatal("partial round plotted");
    auto click = [&](const char *name) {
        if (!QMetaObject::invokeMethod(control(name), "clicked")) qFatal("click failed");
    };
    click("tcSet"); click("tcSave"); click("fanSet"); click("fanSave"); click("outputOn");
    tc->setProperty("text", "99");
    const std::vector<QByteArray> manual{"TC1:TCADJTEMP=32.5", "TC1:TCADJTEMP!", "FAN1:FANSTARTTEMP=40", "FAN1:FANSTARTTEMP!", "TC1:TCSW=1"};
    if (device.commands.size() != 17 || !std::equal(manual.begin(), manual.end(), device.commands.begin() + 12))
        qFatal("manual commands waited for polling or changed order");
    wait([&] { return device.commands.size() > 17; });
    if (tick.elapsed() < 900) qFatal("polling still uses per-command cadence");
    checkRound(17); // 手动命令不改变查询集合，也没有设定后额外回查。
    for (auto *item : {temps, volts, amps}) item->applyHover(1e12);
    if (!temps->hover() || !volts->hover() || !amps->hover()
        || !temps->hover()->text.contains(QStringLiteral("温控1实际温度  22.00"))
        || !temps->hover()->text.contains(QStringLiteral("风扇1实际温度  27.00"))
        || !volts->hover()->text.contains(QStringLiteral("实际输出电压  4.50 V"))
        || !amps->hover()->text.contains(QStringLiteral("实际输出电流  0.60 A"))) qFatal("snapshot values");
    const auto lastSample = temps->hover()->at;
    reply(M::FAN1, C::FANSPEED, 888, 2000000001);
    reply(M::TC1, C::TCACTTEMP, 22.0, 2000000001);
    temps->applyHover(1e12);
    if (temps->hover()->at != lastSample) qFatal("early chart update");
    wait([&] { return device.commands.size() > 27; });
    checkRound(27);
    for (auto *item : {temps, volts, amps}) {
        item->applyHover(1e12);
        if (item->hover()->at != 2000000001) qFatal("plots did not advance together");
    }
    device.close();
    if (control("errors")->property("text") != QStringLiteral("—")) qFatal("close did not clear state");
    const auto before = device.commands.size();
    click("tcSave");
    QElapsedTimer stopped; stopped.start();
    while (stopped.elapsed() < 1100) { QCoreApplication::processEvents(); QThread::msleep(1); }
    if (device.commands.size() != before) qFatal("poll continued while closed");
    device.open();
    checkRound(before + 2); // 离线手动操作不在新连接重放。
    device.close();
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
