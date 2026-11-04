#include <Tcm232Device.h>
#include "FakeSerialPort.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
void check(bool ok, const char *why)
{
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
bool waitFor(auto predicate)
{
    QElapsedTimer limit;
    limit.start();
    while (!predicate() && limit.elapsed() < 3000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return predicate();
}
}

void verifyDevice()
{
    static_assert(std::derived_from<Tcm232::Tcm232Device, Device<Tcm232::Handle>>);
    FakeSerialPort::reset();
    const auto lock = std::make_shared<DeviceLock>(QStringLiteral("test"));
    int replies = 0, values = 0;
    {
        Tcm232::Tcm232Device device;
        QObject::connect(&device, &Tcm232::Tcm232Device::commandReplied, &device,
            [&](RxTime at, int code, int address) {
                check(at != RxTime{} && code == 1 && address == 0, "ack");
                ++replies;
            }, Qt::QueuedConnection);
        QObject::connect(&device, &Tcm232::Tcm232Device::parameterReported, &device,
            [&](RxTime at, QByteArray name, QByteArray value, int address) {
                check(at != RxTime{} && name == "TC2:TCTSCOEFF:0" && value == "25.07" && address == -1, "parameter");
                ++values;
            }, Qt::QueuedConnection);
        check(device.open(device.parameters({QStringLiteral("controlled"), {}})), "open");
        check(waitFor([&]{ return device.isOpen(); }), "connection timeout");
        check(device.tryLock(lock) && !device.sendCommand("TC1:TCOE?"), "lock admission");
        device.releaseLock(lock);
        check(device.sendCommand("TC1:TCOE?") && device.sendCommand("TC2:TCOE?"), "consecutive queries");
        check(FakeSerialPort::writes() == std::vector<QByteArray>{"TC1:TCOE?\r", "TC2:TCOE?\r"}, "query order on wire");
        FakeSerialPort::enqueue("CMD:REPLY=1@0#7D\rTC2:TCTSCOEFF:0=25.07\r");
        check(waitFor([&]{ return replies == 1 && values == 1; }), "queued device signals");
        check(device.tryLock(lock), "reacquire");
        device.close();
        check(!device.holder() && !device.isOpen(), "close releases lock");
    }
    check(FakeSerialPort::livePorts() == 0, "device teardown");
}
