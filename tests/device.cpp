#include <Tcm232Device.h>
#include <Tcm232Protocol.h>
#include "FakeSerialPort.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cstdlib>
#include <iostream>

// 与 HandleCore 的串口替身共用端口对象；枚举也不访问主机硬件。
extern "C" {
sp_return sp_list_ports(sp_port ***ports)
{
    auto **list = new sp_port *[2]{};
    sp_get_port_by_name("controlled", &list[0]);
    *ports = list;
    return SP_OK;
}
char *sp_get_port_name(const sp_port *) { static char name[] = "controlled"; return name; }
char *sp_get_port_description(const sp_port *) { static char description[] = "TCM232 test"; return description; }
void sp_free_port_list(sp_port **ports) { sp_free_port(ports[0]); delete[] ports; }
}

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
    check(query.module == M::TC1 && query.value.metaType() == QMetaType::fromType<Protocol::ValueType>() && !query.address
          && query.command == C::TCACTTEMP && query.value.value<Protocol::ValueType>() == Protocol::ValueType::Query, "unaddressed query");
    auto save = packet("TC2:TCADJTEMP!@0");
    check(save.module == M::TC2 && save.value.metaType() == QMetaType::fromType<Protocol::ValueType>() && save.address == 0
          && save.command == C::TCADJTEMP && save.value.value<Protocol::ValueType>() == Protocol::ValueType::Save, "addressed save");
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
        Tcm232::Tcm232Device device(nullptr);
        QObject::connect(&device, &Tcm232::Tcm232Device::packetReceived, &device,
            [&](RxTime at, const Protocol::Packet &p) {
                check(at != RxTime{}, "arrival time");
                if (p.module == M::CMD && p.command == C::REPLY) {
                    check(p.value.toInt() == 1 && p.address == 0, "ack");
                    ++replies;
                } else {
                    if (p.module == M::CMD)
                        check(p.command == C::TCOE && p.value.toLongLong() == 0, "cross-module parameter");
                    else
                        check(p.module == M::TC2 && p.command == C::TCTSCOEFF_0
                              && p.value.toString() == "25.07" && !p.address, "parameter");
                    ++values;
                }
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
