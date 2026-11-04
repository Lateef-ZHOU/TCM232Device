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
    using C = Protocol::Command;
    auto packet = [](QByteArrayView line) {
        auto parsed = Protocol::parse(line);
        check(std::holds_alternative<Protocol::Packet>(parsed), "expected valid packet");
        return std::get<Protocol::Packet>(parsed);
    };
    for (const auto &entry : Protocol::modules) {
        check(Protocol::moduleOf(entry.name) == entry.module && Protocol::nameOf(entry.module) == entry.name,
              "module table round trip");
    }
    const auto query = packet("TC1:TCACTTEMP?");
    check(query.module == M::TC1 && query.value.metaType() == QMetaType::fromType<Protocol::Query>() && !query.address
          && query.command == C::TCACTTEMP && query.value.isValid() && query.value.isNull(), "unaddressed query");
    auto save = packet("TC2:TCADJTEMP!@0");
    check(save.module == M::TC2 && save.value.metaType() == QMetaType::fromType<Protocol::Save>() && save.address == 0
          && save.command == C::TCADJTEMP && save.value.isValid() && save.value.isNull(), "addressed save");
    auto assignment = packet("TC2:TCADJTEMP=-25.125@255");
    const auto &temperature = assignment.value;
    check(assignment.address == 255 && assignment.command == C::TCADJTEMP
          && temperature.typeId() == QMetaType::Double && temperature.toDouble() == -25.125, "temperature");
    check(packet("SYSTEM:SYSSN=00123").value.toString() == "00123", "serial text");
    check(packet("SYSTEM:SYSCOMPANY=A B=C?!").value.toString() == "A B=C?!", "preserve string value");
    check(packet("TC1:TCOE=1").value.typeId() == QMetaType::LongLong, "option value");
    // 人为交叉组合，只验证协议层的类型选择，不代表实物支持这些命令。
    auto crossModule = packet("FAN1:TCACTTEMP=-2.5").value;
    check(crossModule.typeId() == QMetaType::Double && crossModule.toDouble() == -2.5, "type follows parameter");
    check(packet("CMD:REPLY=8").value.toInt() == 8, "save acknowledgement");
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
    check(rejected("FAN1:TCACTTEMP=oops", Protocol::Error::Value), "cross-module numeric validation");
    check(rejected("TC2:TCACTTEMP=oops", Protocol::Error::Value), "invalid numeric value");
    check(rejected("CMD:REPLY=9", Protocol::Error::Value), "undefined reply code");
    check(rejected("TC1:UNKNOWN?", Protocol::Error::CommandName)
          && rejected("TC1:UNKNOWN!", Protocol::Error::CommandName)
          && rejected("TC1:UNKNOWN=1", Protocol::Error::CommandName), "all operators require a known command");
    const auto emptyText = packet("SYSTEM:SYSCOMPANY=");
    check(emptyText.value.typeId() == QMetaType::QString
          && emptyText.value.toString().isEmpty(), "empty text is a valid assignment value");
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
            [&](RxTime at, M module, C command, QVariant value, std::optional<quint8> address) {
                if (module == M::CMD) {
                    check(command == C::TCOE && value.typeId() == QMetaType::LongLong && value.toLongLong() == 0,
                          "CMD with a different parameter is not an acknowledgement");
                } else {
                    check(at != RxTime{} && module == M::TC2 && command == C::TCTSCOEFF_0 && value.typeId() == QMetaType::QString
                          && value.toString() == "25.07" && !address, "parameter");
                }
                ++values;
            }, Qt::QueuedConnection);
        check(device.open(device.parameters({QStringLiteral("controlled"), {}})), "open");
        check(waitFor([&]{ return device.isOpen(); }), "connection timeout");
        FakeSerialPort::enqueue("CMD:REPLY=1@0#7D\rTC2:TCTSCOEFF:0=25.07\rCMD:TCOE=0\r");
        check(waitFor([&]{ return replies == 1 && values == 2; }), "queued device signals");
        check(device.tryLock(lock), "acquire");
        device.close();
        check(!device.holder() && !device.isOpen(), "close releases lock");
    }
    check(FakeSerialPort::livePorts() == 0, "device teardown");
}
