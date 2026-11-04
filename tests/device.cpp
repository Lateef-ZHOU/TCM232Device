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
    using M = Protocol::Module;
    using Op = Protocol::Operation;
    auto packet = [](QByteArrayView line) {
        auto parsed = Protocol::parse(line);
        check(std::holds_alternative<Protocol::Packet>(parsed), "expected valid packet");
        return std::get<Protocol::Packet>(parsed);
    };
    for (const auto &entry : Protocol::modules) {
        check(Protocol::moduleOf(entry.name) == entry.module && Protocol::nameOf(entry.module) == entry.name,
              "module table round trip");
    }
    auto query = packet("TC1:TCACTTEMP?");
    check(query.module == M::TC1 && query.operation == Op::Query && !query.address
          && !Protocol::parseValue(query).isValid(), "unaddressed query");
    auto save = packet("TC2:TCADJTEMP!@0");
    check(save.module == M::TC2 && save.operation == Op::Save && save.address == 0, "addressed save");
    auto assignment = packet("TC2:TCADJTEMP=-25.125@255");
    auto temperature = Protocol::parseValue(assignment);
    check(assignment.operation == Op::Equals && assignment.address == 255 && assignment.parameter == "TCADJTEMP"
          && temperature.typeId() == QMetaType::Double && temperature.toDouble() == -25.125, "temperature");
    check(Protocol::parseValue(packet("SYSTEM:SYSSN=00123")).toString() == "00123", "serial text");
    check(Protocol::parseValue(packet("SYSTEM:SYSCOMPANY=A B=C?!")).toString() == "A B=C?!", "preserve string value");
    check(Protocol::parseValue(packet("TC1:TCOE=1")).typeId() == QMetaType::LongLong, "option value");
    check(!Protocol::parseValue(packet("TC2:TCACTTEMP=oops")).isValid(), "invalid numeric value");
    check(!Protocol::parseValue(packet("CMD:REPLY=9")).isValid(), "undefined reply code");
    check(Protocol::parseValue(packet("CMD:REPLY=8")).toInt() == 8, "save acknowledgement");
    const auto rejected = [](QByteArrayView line, Protocol::Error expected) {
        const auto parsed = Protocol::parse(line);
        const auto *error = std::get_if<Protocol::Error>(&parsed);
        return error && *error == expected;
    };
    check(rejected("TC1:TCOE=1@256", Protocol::Error::Address), "invalid address");
    check(rejected("TC1:TCOE?@", Protocol::Error::Address), "present but empty address");
    check(rejected("UNKNOWN:TCOE?", Protocol::Error::ModuleName), "unknown module");
    check(rejected("TC1::X=1", Protocol::Error::Syntax), "empty name segment");
    check(rejected("TC1:TCOE?=1", Protocol::Error::Syntax), "query with trailing data");
    static_assert(std::derived_from<Tcm232::Tcm232Device, Device<Tcm232::Handle>>);
    FakeSerialPort::reset();
    const auto lock = std::make_shared<DeviceLock>(QStringLiteral("test"));
    int replies = 0, values = 0;
    {
        Tcm232::Tcm232Device device;
        QObject::connect(&device, &Tcm232::Tcm232Device::commandReplied, &device,
            [&](RxTime at, int code, std::optional<quint8> address) {
                check(at != RxTime{} && code == 1 && address == 0, "ack");
                ++replies;
            }, Qt::QueuedConnection);
        QObject::connect(&device, &Tcm232::Tcm232Device::parameterReported, &device,
            [&](RxTime at, M module, QByteArray name, QVariant value, std::optional<quint8> address) {
                check(at != RxTime{} && module == M::TC2 && name == "TCTSCOEFF:0" && value.typeId() == QMetaType::QString
                      && value.toString() == "25.07" && !address, "parameter");
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
