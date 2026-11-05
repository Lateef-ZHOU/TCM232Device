#include <Tcm232Handle.h>
#include "FakeSerialPort.h"
#ifdef TEST_TCM232_QML
#include <QGuiApplication>
#else
#include <QCoreApplication>
#endif
#include <QSemaphore>
#include <QElapsedTimer>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
void require(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}

bool waitFor(auto predicate, int timeout = 3000)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!predicate() && elapsed.elapsed() < timeout) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return predicate();
}

class Probe final : public Tcm232::Handle
{
public:
    ~Probe() override { close(); }
    QSemaphore available;
    std::vector<QByteArray> received;

private:
    void onData(const QByteArray &body, RxTime at) override
    {
        require(at != RxTime{}, "missing receive time");
        received.push_back(body);
        available.release();
    }
};
}

#ifdef TEST_TCM232_DEVICE
void verifyDevice();
#ifdef TEST_TCM232_QML
void verifyQml();
#endif
#endif

int main(int argc, char **argv)
{
#ifdef TEST_TCM232_QML
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
#else
    QCoreApplication app(argc, argv);
#endif
    const auto params = Tcm232::Handle::parameters({QStringLiteral("controlled"), {}});
    require(params.baudRate == 57600 && params.dataBits == DataBits::D8
            && params.parity == SP_PARITY_NONE && params.stopBits == StopBits::One
            && params.flowControl == SP_FLOWCONTROL_NONE, "serial settings");

    Tcm232::Framer framer;
    std::vector<QByteArray> bodies;
    const RxTime t1 = std::chrono::system_clock::now();
    const RxTime t2 = t1 + std::chrono::seconds(1);
    const auto record = [&](const QByteArray &b, RxTime at) {
        require(at == t2, "timestamp must follow terminator");
        bodies.push_back(b);
    };
    framer.feed("TC1:TCACT", t1, record);
    require(bodies.empty(), "partial line delivered early");
    framer.feed("TEMP=25.01\rCMD:REPLY=1\rTC2:", t2, record);
    require(bodies == std::vector<QByteArray>{"TC1:TCACTTEMP=25.01", "CMD:REPLY=1"}, "combined replies");
    framer.feed("TCOE=0\r\r", t2, record);
    require(bodies.size() == 3 && bodies.back() == "TC2:TCOE=0", "trailing partial or empty line");
    const QByteArray limit(Tcm232::Framer::MaxBodyBytes, 'x');
    framer.feed(limit, t1, record);
    framer.feed("\r", t2, record);
    framer.feed(limit + "x", t1, record);
    framer.feed("suffix\rTC1:TCOE=1\r", t2, record);
    require(bodies.size() == 5 && bodies[3] == limit && bodies.back() == "TC1:TCOE=1", "oversize resync");
    bodies.clear();
    framer.feed("CMD:REPLY=1#0D\rCMD:REPLY=1@256#7C\rCMD:REPLY=1@A#0C\rCMD:REPLY=1@255#7F\r", t2, record);
    require(bodies == std::vector<QByteArray>{"CMD:REPLY=1@255"}, "address suffix on checked frames");

    using P = Tcm232::Protocol;
    using M = P::Module;
    using C = P::Command;
    const auto query = QVariant::fromValue(P::ValueType::Query);
    const auto save = QVariant::fromValue(P::ValueType::Save);
    require(P::nameOf(C::TCTSCOEFF_0) == "TCTSCOEFF:0" && P::nameOf(static_cast<C>(-1)).isEmpty(), "command name");
    require(P::pack({{}, M::TC1, C::TCSW, 0}) == "TC1:TCSW=0"
            && P::pack({{}, M::TC1, C::TCSW, QVariant::fromValue(P::ValueType::Integer)}).isEmpty(),
            "integer payload versus type descriptor");
    require(P::pack({0, M::TC1, C::TCSW, 1}) == "TC1:TCSW=1@0", "integer and explicit zero address");
    require(P::pack({255, M::TC2, C::TCADJTEMP, -25.1}) == "TC2:TCADJTEMP=-25.1@255", "real assignment");
    require(P::pack({{}, M::SYSTEM, C::SYSSN, QString()}) == "SYSTEM:SYSSN=", "empty text");
    require(P::pack({{}, M::TC1, C::TCOE, QVariant()}).isEmpty()
            && P::pack({{}, M::TC1, C::TCOE, 1.5}).isEmpty()
            && P::pack({{}, M::TC1, C::TCADJTEMP, std::numeric_limits<double>::infinity()}).isEmpty()
            && P::pack({{}, M::CMD, C::REPLY, 9}).isEmpty()
            && P::pack({{}, M::SYSTEM, C::SYSSN, QStringLiteral("abc\r")}).isEmpty(), "invalid values");
    require(P::pack({{}, static_cast<M>(-1), C::TCOE, query}).isEmpty()
            && P::pack({{}, M::TC1, static_cast<C>(-1), save}).isEmpty(), "invalid ids");
    for (const QByteArray wire : {"SYSTEM:SYSSN=00123", "CMD:REPLY=8@254", "TC2:TCTSCOEFF:0!@0",
                                  "TC1:TCACTTEMP?", "TC2:TCADJTEMP=-0.00012345678901234567"}) {
        const auto decoded = P::parse(wire);
        const auto *pkt = std::get_if<P::Packet>(&decoded);
        require(pkt != nullptr, "parse before packing");
        const auto packed = P::pack(*pkt);
        const auto again = P::parse(packed);
        const auto *other = std::get_if<P::Packet>(&again);
        require(other && other->address == pkt->address && other->module == pkt->module
                && other->command == pkt->command && other->value.metaType() == pkt->value.metaType()
                && other->value == pkt->value,
                "pack/parse round trip");
    }

    FakeSerialPort::reset();
    Probe handle;
    require(handle.open(params), "open rejected");
    FakeSerialPort::enqueue("TC1:TCACTTEMP=");
    FakeSerialPort::enqueue("25.01\rCMD:REPLY=1@0#7");
    FakeSerialPort::enqueue("D\rCMD:REPLY=1@0#70\rCMD:REPLY=1@0#7d\rCMD:REPLY=1@0#7\r"
                            "TC2:TCACTTEMP=24.90@0\r");
    require(handle.available.tryAcquire(3, 3000), "serial read timed out");
    require(handle.send(P::pack({{}, M::TC1, C::TCACTTEMP, query})), "send failed");
    require(handle.send(P::pack({0, M::TC2, C::TCADJTEMP, save})), "consecutive send failed");
    require(!handle.send(QByteArray("TC1:TCACTTEMP?\r")) && !handle.send(QByteArray()), "invalid body sent");
    require(FakeSerialPort::writes().size() == 1, "first immediate, second must queue");
    require(waitFor([] { return FakeSerialPort::writes().size() == 2; }), "queued command not sent");
    require(FakeSerialPort::writeTimes()[1] - FakeSerialPort::writeTimes()[0] >= std::chrono::milliseconds(75), "burst interval");
    // 第二条已经清空队列；冷却未结束时提交第三条，仍不可立即发送。
    require(handle.send(QByteArray("TC1:TCSW?")), "cooldown enqueue");
    require(FakeSerialPort::writes().size() == 2, "empty queue bypassed cooldown");
    require(waitFor([] { return FakeSerialPort::writes().size() == 3; }), "cooldown lost new command");
    require(FakeSerialPort::writeTimes()[2] - FakeSerialPort::writeTimes()[1] >= std::chrono::milliseconds(75), "tail cooldown interval");
    (void)waitFor([] { return false; }, 150);
    require(handle.send(QByteArray("TC1:TCOE?")) && FakeSerialPort::writes().size() == 4, "idle first command delayed");
    require(handle.send(QByteArray("TC1:TCERRORSTATUS?")), "enqueue before close");
    handle.close();
    (void)waitFor([] { return false; }, 150);
    require(FakeSerialPort::writes().size() == 4, "close leaked queued command");
    require(!handle.send(QByteArray("TC1:TCOE?")), "offline queued command accepted");
    require(handle.received == std::vector<QByteArray>{"TC1:TCACTTEMP=25.01", "CMD:REPLY=1@0",
                                                       "TC2:TCACTTEMP=24.90@0"}, "serial bodies");
    require(FakeSerialPort::writes() == std::vector<QByteArray>{"TC1:TCACTTEMP?\r", "TC2:TCADJTEMP!@0\r", "TC1:TCSW?\r", "TC1:TCOE?\r"}, "wire framing");
    require(FakeSerialPort::livePorts() == 0, "port not released");
    require(handle.open(params), "reopen");
    require(waitFor([&] { return handle.isOpen(); }), "reopen timeout");
    require(handle.send(QByteArray("SYSTEM:SYSSN?")) && FakeSerialPort::writes().size() == 5, "reopen first command");
    require(handle.send(QByteArray("TC1:TCOE?")), "reconnect queue");
    FakeSerialPort::failRead();
    require(waitFor([&] { return !handle.isOpen(); }), "disconnect timeout");
    require(waitFor([&] { return handle.isOpen(); }), "auto reconnect timeout");
    require(FakeSerialPort::writes().size() == 5, "old command crossed reconnection");
    int failures = 0;
    QObject::connect(&handle, &Tcm232::Handle::logMessage, &handle, [&](const QString &message) {
        if (message == QStringLiteral("串口发送失败")) ++failures;
    });
    FakeSerialPort::writeResult(-1);
    require(handle.send(QByteArray("TC1:TCOE?")), "Queued must report acceptance");
    require(failures == 1, "actual send failure not logged");
    handle.close();
    require(FakeSerialPort::livePorts() == 0, "reconnect port not released");
    FakeSerialPort::reset();
    {
        Probe dying;
        require(dying.open(params) && waitFor([&] { return dying.isOpen(); }), "destruction open");
        require(dying.send(QByteArray("TC1:TCOE?")) && dying.send(QByteArray("TC1:TCSW?")), "destruction queue");
    }
    (void)waitFor([] { return false; }, 150);
    require(FakeSerialPort::writes().size() == 1 && FakeSerialPort::livePorts() == 0, "destruction leaked pending send");
#ifdef TEST_TCM232_DEVICE
    verifyDevice();
#ifdef TEST_TCM232_QML
    verifyQml();
#endif
#endif
}
