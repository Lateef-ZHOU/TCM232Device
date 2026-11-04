#include <Tcm232Handle.h>
#include "FakeSerialPort.h"
#include <QCoreApplication>
#include <QSemaphore>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
void require(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
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
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
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

    FakeSerialPort::reset();
    Probe handle;
    require(handle.open(params), "open rejected");
    FakeSerialPort::enqueue("TC1:TCACTTEMP=");
    FakeSerialPort::enqueue("25.01\rCMD:REPLY=1@0#7");
    FakeSerialPort::enqueue("D\rCMD:REPLY=1@0#70\rCMD:REPLY=1@0#7d\rCMD:REPLY=1@0#7\r"
                            "TC2:TCACTTEMP=24.90@0\r");
    require(handle.available.tryAcquire(3, 3000), "serial read timed out");
    require(handle.send(QByteArray("TC1:TCACTTEMP?")), "send failed");
    require(handle.send(QByteArray("TC2:TCOE?")), "consecutive send failed");
    require(!handle.send(QByteArray("TC1:TCACTTEMP?\r")) && !handle.send(QByteArray()), "invalid body sent");
    handle.close();
    require(handle.received == std::vector<QByteArray>{"TC1:TCACTTEMP=25.01", "CMD:REPLY=1@0",
                                                       "TC2:TCACTTEMP=24.90@0"}, "serial bodies");
    require(FakeSerialPort::writes() == std::vector<QByteArray>{"TC1:TCACTTEMP?\r", "TC2:TCOE?\r"}, "wire framing");
    require(FakeSerialPort::livePorts() == 0, "port not released");
#ifdef TEST_TCM232_DEVICE
    verifyDevice();
#endif
}
