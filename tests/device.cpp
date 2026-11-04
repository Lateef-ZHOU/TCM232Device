#include <Tcm232Device.h>
#include <Tcm232Protocol.h>
#include "FakeSerialPort.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cstdlib>
#include <iostream>

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
    using Tcm232::Protocol;
    const auto parameter = Protocol::parseReply("SYSTEM:SYSCOMPANY=A B=C@255");
    const auto *p = std::get_if<Protocol::Parameter>(&parameter);
    check(p && p->name == "SYSTEM:SYSCOMPANY" && p->value == "A B=C" && p->address == 255, "raw string value");
    const auto saved = Protocol::parseReply("CMD:REPLY=8");
    const auto *ack = std::get_if<Protocol::CommandReply>(&saved);
    check(ack && ack->code == 8 && ack->address == -1, "save acknowledgement");
    const auto rejected = [](QByteArrayView line, Protocol::Error expected) {
        const auto result = Protocol::parseReply(line);
        const auto *error = std::get_if<Protocol::Error>(&result);
        return error && *error == expected;
    };
    check(rejected("CMD:REPLY=9", Protocol::Error::ReplyCode), "undefined reply code");
    check(rejected("TC1:TCOE=1@256", Protocol::Error::Address), "invalid address");
    check(rejected("TC1::X=1", Protocol::Error::ParameterName), "empty name segment");
    check(rejected("TC1:TCOE?", Protocol::Error::ParameterName), "query is not a reply");
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
        FakeSerialPort::enqueue("CMD:REPLY=1@0#7D\rTC2:TCTSCOEFF:0=25.07\r");
        check(waitFor([&]{ return replies == 1 && values == 1; }), "queued device signals");
        check(device.tryLock(lock), "acquire");
        device.close();
        check(!device.holder() && !device.isOpen(), "close releases lock");
    }
    check(FakeSerialPort::livePorts() == 0, "device teardown");
}
