#pragma once
#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <QMetaType>
#include <QHash>
#include <QVariant>
#include <QByteArray>
#include <QByteArrayView>

namespace Tcm232
{
struct Protocol
{
    enum class Module { TC1, TC2, TA1, TA2, NTS1, NTS2, PWG1, PWG2, FAN1, FAN2, SYSTEM, SYS, RS485, UIRS232, MEMORY, PARAM, PCB, MUI, BOARDCONFIG, CMD };
    struct Query {};
    struct Save {};
    enum class Error { Address, ModuleName, Syntax, CommandName, Value };

    enum class Command {
        TCADJTEMP, TCACTTEMP, TCSETTEMP, TCRAMPSPEED, TCMAXTEMP, TCMINTEMP, TCSTEPTEMP, TCDECIMALDIGITS,
        TCMAXV, TCCOOLVRATIO, TCMODE, TCSW, TCOE, TCSETVOL, TCACTVOL, TCACTCUR, TCLOADRES, TCLOADDETECT,
        TCLOADRESLT, TCLOADRESHT, TCTSTEMP, TCTSOFFSET, TCTSOUTPUT, TCTSTYPE, TCTSCOEFF_0, TCTSCOEFF_1,
        TCTSCOEFF_2, TCTSCOEFF_3, TCTSCOEFF_4, TCTSSTEP, TCAUXTSFUNC, TCAUXTSINDEX, TCAUXTSACTTEMP,
        TCAUXTSSAFETY, TCAUXTSOTPLT, TCAUXTSOTPHT, TCRTOCPSTATUS, TCRTOCPLT, TCRTOCPHT, TCPIDTYPE, TCPIDP,
        TCPIDTI, TCPIDTD, TCCTRLINTERVAL, TCPIDALGO, TCTUNESTATUS, TCPIDWINDOW, TCPIDACCWINDOW, TCOVPADJTHR,
        TCOVPACTTHR, TCOVPSTATUS, TCOCPADJTHR, TCOCPACTTHR, TCOCPSTATUS, TCCURLIMIT, TCOTPMODE, TCOTPLT,
        TCOTPHT, TCOTPSTATUS, TCOTPLHYS, TCOTPHHYS, TCEXTSTATUS, TCEXTLOCK, TCEXTLOCKPOL, TCEXTCTRL, TCAR,
        TCARCNT, TCARDELAY, TCERRORLOG, TCERRORSTATUS, TCERRORMASK, TAACTTEMP, TATSOFFSET, TATSOUTPUT,
        TATSTYPE, TATSCOEFF_0, TATSCOEFF_1, TATSCOEFF_2, TATSCOEFF_3, TATSCOEFF_4, TAOTPSTATUS, TAOTPLT,
        TAOTPHT, NTSACTTEMP, NTSSAFETY, NTSSTATUS, NTSTIMEOUT, NTSDEFTEMP, PWGADJFREQ, PWGSETFREQ, PWGADJPW,
        PWGSETPW, PWGPOL, FANACTTEMP, FANSTARTTEMP, FANMAXTEMP, FANHYSTEMP, FANAUXTSFUNC, FANAUXTSINDEX,
        FANAUXTSACTTEMP, FANAUXTSSAFETY, FANAUXTSOTPLT, FANAUXTSOTPHT, FANACTDUTY, FANSTARTDUTY, FANMAXDUTY,
        FANIDLEDUTY, FANSAFEDUTY, FANSW, FANOUTPOL, FANPWMFREQ, FANSELFTEST, FANPPR, FANSPEED, FANRTOCPSTATUS,
        FANRTOCPLT, FANRTOCPHT, SYSSN, SYSPVIN, SYSTIME, SYSMODEL, SYSVERSION, SYSCOMPANY, SYSWEBSITE, RESET,
        COMADDR, COMBAUDRATE, COMMODBUS, MEMWP, BATCH, BATCHDEFAULT, BATCHSAVE, TMACTTEMP, TMOTPSTATUS,
        TMOTPLT, TMOTPHT, TMOTPHHYS, TMDTPSTATUS, TMDTPLT, TMDTPHT, TMDTPHHYS, LEDERRMODE, LEDSWMODE, ASEN,
        TCINPARALLEL, TC1KEYPOL, TC2KEYPOL, CONNIN1DEFVOL, CONNIN2DEFVOL, CONNIN1, CONNIN2, CONNOUT1POL,
        CONNOUT2POL, CONNOUT1, CONNOUT2, FAN1MAINTEMP, FAN2MAINTEMP, CONNIN, CONNINDEFVOL, CONNOUT,
        CONNOUTPOL, TCTSRELOAD, REPLY
    };

    struct ModuleName { Module module; QByteArrayView name; };
    inline static constexpr ModuleName modules[] = {
        {Module::TC1, "TC1"},
        {Module::TC2, "TC2"},
        {Module::TA1, "TA1"},
        {Module::TA2, "TA2"},
        {Module::NTS1, "NTS1"},
        {Module::NTS2, "NTS2"},
        {Module::PWG1, "PWG1"},
        {Module::PWG2, "PWG2"},
        {Module::FAN1, "FAN1"},
        {Module::FAN2, "FAN2"},
        {Module::SYSTEM, "SYSTEM"},
        {Module::SYS, "SYS"},
        {Module::RS485, "RS485"},
        {Module::UIRS232, "UIRS232"},
        {Module::MEMORY, "MEMORY"},
        {Module::PARAM, "PARAM"},
        {Module::PCB, "PCB"},
        {Module::MUI, "MUI"},
        {Module::BOARDCONFIG, "BOARDCONFIG"},
        {Module::CMD, "CMD"}
    };

    [[nodiscard]] static std::optional<Module> moduleOf(QByteArrayView name)
    {
        static const auto byName = [] {
            QHash<QByteArrayView, Module> index;
            for (const auto &entry : modules) index.insert(entry.name, entry.module);
            return index;
        }();
        const auto it = byName.constFind(name);
        if (it == byName.cend()) return std::nullopt;
        return it.value();
    }

    [[nodiscard]] static constexpr QByteArrayView nameOf(Module module) noexcept
    {
        for (const auto &entry : modules)
            if (entry.module == module) return entry.name;
        return {};
    }

    // 主体已去除回车与校验；结果自持，查询和保存的 value 是带各自元类型的空 QVariant。
    struct Packet {
        std::optional<quint8> address;
        Module module;
        Command command;
        QVariant value;
    };

    [[nodiscard]] static std::variant<Packet, Error> parse(QByteArrayView line)
    {
        std::optional<quint8> address;
        const auto at = line.lastIndexOf('@');
        if (at >= 0) {
            address = addressValue(line.sliced(at + 1));
            if (!address) return Error::Address;
            line = line.first(at);
        }
        const auto colon = line.indexOf(':');
        if (colon <= 0) return Error::Syntax;
        const auto module = moduleOf(line.first(colon));
        if (!module) return Error::ModuleName;

        bool emptySegment = true;
        for (auto i = colon + 1; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '=' || c == '?' || c == '!') {
                if (emptySegment || (c != '=' && i + 1 != line.size())) return Error::Syntax;
                const auto field = commands.constFind(line.sliced(colon + 1, i - colon - 1));
                if (field == commands.cend()) return Error::CommandName;

                if (c == '?')
                    return Packet{address, *module, field->command, QVariant(QMetaType::fromType<Query>())};
                if (c == '!')
                    return Packet{address, *module, field->command, QVariant(QMetaType::fromType<Save>())};
                auto value = decode(field->type, line.sliced(i + 1));
                if (!value.isValid()) return Error::Value;
                return Packet{address, *module, field->command, std::move(value)};
            }
            if (c <= ' ' || c > '~' || c == '#' || (c == ':' && emptySegment)) return Error::Syntax;
            emptySegment = c == ':';
        }
        return Error::Syntax;
    }

    // 带校验的行以 @地址#YY 结尾，异或包含 '#'。通过后只剥掉校验。
    [[nodiscard]] static bool acceptChecksum(QByteArray &line)
    {
        const auto hash = line.indexOf('#');
        if (hash < 0)
            return true;
        if (line.size() - hash != 3)
            return false;
        const auto at = line.lastIndexOf('@', hash);
        if (at <= 0 || !addressValue(QByteArrayView(line).sliced(at + 1, hash - at - 1)))
            return false;
        unsigned char sum = 0;
        for (qsizetype i = 0; i <= hash; ++i)
            sum ^= static_cast<unsigned char>(line.at(i));
        constexpr char hex[] = "0123456789ABCDEF";
        if (line.at(hash + 1) != hex[sum >> 4] || line.at(hash + 2) != hex[sum & 15])
            return false;
        line.truncate(hash);
        return true;
    }

private:
    enum class ValueType { Real, Integer, Text, ReplyCode };
    struct CommandSpec { Command command; ValueType type; };
    // 型号表中的命令按参数名登记，不限制模块组合；哈希只负责从文本定位这条描述。
    inline static const QHash<QByteArrayView, CommandSpec> commands = {
        {"TCADJTEMP", {Command::TCADJTEMP, ValueType::Real}},
        {"TCACTTEMP", {Command::TCACTTEMP, ValueType::Real}},
        {"TCSETTEMP", {Command::TCSETTEMP, ValueType::Real}},
        {"TCRAMPSPEED", {Command::TCRAMPSPEED, ValueType::Real}},
        {"TCMAXTEMP", {Command::TCMAXTEMP, ValueType::Real}},
        {"TCMINTEMP", {Command::TCMINTEMP, ValueType::Real}},
        {"TCSTEPTEMP", {Command::TCSTEPTEMP, ValueType::Real}},
        {"TCDECIMALDIGITS", {Command::TCDECIMALDIGITS, ValueType::Real}},
        {"TCMAXV", {Command::TCMAXV, ValueType::Real}},
        {"TCCOOLVRATIO", {Command::TCCOOLVRATIO, ValueType::Real}},
        {"TCMODE", {Command::TCMODE, ValueType::Integer}},
        {"TCSW", {Command::TCSW, ValueType::Integer}},
        {"TCOE", {Command::TCOE, ValueType::Integer}},
        {"TCSETVOL", {Command::TCSETVOL, ValueType::Real}},
        {"TCACTVOL", {Command::TCACTVOL, ValueType::Real}},
        {"TCACTCUR", {Command::TCACTCUR, ValueType::Real}},
        {"TCLOADRES", {Command::TCLOADRES, ValueType::Real}},
        {"TCLOADDETECT", {Command::TCLOADDETECT, ValueType::Integer}},
        {"TCLOADRESLT", {Command::TCLOADRESLT, ValueType::Real}},
        {"TCLOADRESHT", {Command::TCLOADRESHT, ValueType::Real}},
        {"TCTSTEMP", {Command::TCTSTEMP, ValueType::Real}},
        {"TCTSOFFSET", {Command::TCTSOFFSET, ValueType::Real}},
        {"TCTSOUTPUT", {Command::TCTSOUTPUT, ValueType::Text}},
        {"TCTSTYPE", {Command::TCTSTYPE, ValueType::Text}},
        {"TCTSCOEFF:0", {Command::TCTSCOEFF_0, ValueType::Text}},
        {"TCTSCOEFF:1", {Command::TCTSCOEFF_1, ValueType::Text}},
        {"TCTSCOEFF:2", {Command::TCTSCOEFF_2, ValueType::Text}},
        {"TCTSCOEFF:3", {Command::TCTSCOEFF_3, ValueType::Text}},
        {"TCTSCOEFF:4", {Command::TCTSCOEFF_4, ValueType::Text}},
        {"TCTSSTEP", {Command::TCTSSTEP, ValueType::Real}},
        {"TCAUXTSFUNC", {Command::TCAUXTSFUNC, ValueType::Integer}},
        {"TCAUXTSINDEX", {Command::TCAUXTSINDEX, ValueType::Integer}},
        {"TCAUXTSACTTEMP", {Command::TCAUXTSACTTEMP, ValueType::Real}},
        {"TCAUXTSSAFETY", {Command::TCAUXTSSAFETY, ValueType::Integer}},
        {"TCAUXTSOTPLT", {Command::TCAUXTSOTPLT, ValueType::Real}},
        {"TCAUXTSOTPHT", {Command::TCAUXTSOTPHT, ValueType::Real}},
        {"TCRTOCPSTATUS", {Command::TCRTOCPSTATUS, ValueType::Integer}},
        {"TCRTOCPLT", {Command::TCRTOCPLT, ValueType::Real}},
        {"TCRTOCPHT", {Command::TCRTOCPHT, ValueType::Real}},
        {"TCPIDTYPE", {Command::TCPIDTYPE, ValueType::Integer}},
        {"TCPIDP", {Command::TCPIDP, ValueType::Real}},
        {"TCPIDTI", {Command::TCPIDTI, ValueType::Real}},
        {"TCPIDTD", {Command::TCPIDTD, ValueType::Real}},
        {"TCCTRLINTERVAL", {Command::TCCTRLINTERVAL, ValueType::Real}},
        {"TCPIDALGO", {Command::TCPIDALGO, ValueType::Integer}},
        {"TCTUNESTATUS", {Command::TCTUNESTATUS, ValueType::Real}},
        {"TCPIDWINDOW", {Command::TCPIDWINDOW, ValueType::Real}},
        {"TCPIDACCWINDOW", {Command::TCPIDACCWINDOW, ValueType::Real}},
        {"TCOVPADJTHR", {Command::TCOVPADJTHR, ValueType::Real}},
        {"TCOVPACTTHR", {Command::TCOVPACTTHR, ValueType::Real}},
        {"TCOVPSTATUS", {Command::TCOVPSTATUS, ValueType::Integer}},
        {"TCOCPADJTHR", {Command::TCOCPADJTHR, ValueType::Real}},
        {"TCOCPACTTHR", {Command::TCOCPACTTHR, ValueType::Real}},
        {"TCOCPSTATUS", {Command::TCOCPSTATUS, ValueType::Integer}},
        {"TCCURLIMIT", {Command::TCCURLIMIT, ValueType::Real}},
        {"TCOTPMODE", {Command::TCOTPMODE, ValueType::Integer}},
        {"TCOTPLT", {Command::TCOTPLT, ValueType::Real}},
        {"TCOTPHT", {Command::TCOTPHT, ValueType::Real}},
        {"TCOTPSTATUS", {Command::TCOTPSTATUS, ValueType::Integer}},
        {"TCOTPLHYS", {Command::TCOTPLHYS, ValueType::Real}},
        {"TCOTPHHYS", {Command::TCOTPHHYS, ValueType::Real}},
        {"TCEXTSTATUS", {Command::TCEXTSTATUS, ValueType::Integer}},
        {"TCEXTLOCK", {Command::TCEXTLOCK, ValueType::Integer}},
        {"TCEXTLOCKPOL", {Command::TCEXTLOCKPOL, ValueType::Integer}},
        {"TCEXTCTRL", {Command::TCEXTCTRL, ValueType::Real}},
        {"TCAR", {Command::TCAR, ValueType::Real}},
        {"TCARCNT", {Command::TCARCNT, ValueType::Real}},
        {"TCARDELAY", {Command::TCARDELAY, ValueType::Real}},
        {"TCERRORLOG", {Command::TCERRORLOG, ValueType::Integer}},
        {"TCERRORSTATUS", {Command::TCERRORSTATUS, ValueType::Integer}},
        {"TCERRORMASK", {Command::TCERRORMASK, ValueType::Integer}},
        {"TAACTTEMP", {Command::TAACTTEMP, ValueType::Real}},
        {"TATSOFFSET", {Command::TATSOFFSET, ValueType::Real}},
        {"TATSOUTPUT", {Command::TATSOUTPUT, ValueType::Text}},
        {"TATSTYPE", {Command::TATSTYPE, ValueType::Text}},
        {"TATSCOEFF:0", {Command::TATSCOEFF_0, ValueType::Text}},
        {"TATSCOEFF:1", {Command::TATSCOEFF_1, ValueType::Text}},
        {"TATSCOEFF:2", {Command::TATSCOEFF_2, ValueType::Text}},
        {"TATSCOEFF:3", {Command::TATSCOEFF_3, ValueType::Text}},
        {"TATSCOEFF:4", {Command::TATSCOEFF_4, ValueType::Text}},
        {"TAOTPSTATUS", {Command::TAOTPSTATUS, ValueType::Integer}},
        {"TAOTPLT", {Command::TAOTPLT, ValueType::Real}},
        {"TAOTPHT", {Command::TAOTPHT, ValueType::Real}},
        {"NTSACTTEMP", {Command::NTSACTTEMP, ValueType::Real}},
        {"NTSSAFETY", {Command::NTSSAFETY, ValueType::Integer}},
        {"NTSSTATUS", {Command::NTSSTATUS, ValueType::Integer}},
        {"NTSTIMEOUT", {Command::NTSTIMEOUT, ValueType::Real}},
        {"NTSDEFTEMP", {Command::NTSDEFTEMP, ValueType::Real}},
        {"PWGADJFREQ", {Command::PWGADJFREQ, ValueType::Real}},
        {"PWGSETFREQ", {Command::PWGSETFREQ, ValueType::Real}},
        {"PWGADJPW", {Command::PWGADJPW, ValueType::Real}},
        {"PWGSETPW", {Command::PWGSETPW, ValueType::Real}},
        {"PWGPOL", {Command::PWGPOL, ValueType::Integer}},
        {"FANACTTEMP", {Command::FANACTTEMP, ValueType::Real}},
        {"FANSTARTTEMP", {Command::FANSTARTTEMP, ValueType::Real}},
        {"FANMAXTEMP", {Command::FANMAXTEMP, ValueType::Real}},
        {"FANHYSTEMP", {Command::FANHYSTEMP, ValueType::Real}},
        {"FANAUXTSFUNC", {Command::FANAUXTSFUNC, ValueType::Integer}},
        {"FANAUXTSINDEX", {Command::FANAUXTSINDEX, ValueType::Integer}},
        {"FANAUXTSACTTEMP", {Command::FANAUXTSACTTEMP, ValueType::Real}},
        {"FANAUXTSSAFETY", {Command::FANAUXTSSAFETY, ValueType::Integer}},
        {"FANAUXTSOTPLT", {Command::FANAUXTSOTPLT, ValueType::Real}},
        {"FANAUXTSOTPHT", {Command::FANAUXTSOTPHT, ValueType::Real}},
        {"FANACTDUTY", {Command::FANACTDUTY, ValueType::Real}},
        {"FANSTARTDUTY", {Command::FANSTARTDUTY, ValueType::Real}},
        {"FANMAXDUTY", {Command::FANMAXDUTY, ValueType::Real}},
        {"FANIDLEDUTY", {Command::FANIDLEDUTY, ValueType::Real}},
        {"FANSAFEDUTY", {Command::FANSAFEDUTY, ValueType::Real}},
        {"FANSW", {Command::FANSW, ValueType::Integer}},
        {"FANOUTPOL", {Command::FANOUTPOL, ValueType::Integer}},
        {"FANPWMFREQ", {Command::FANPWMFREQ, ValueType::Real}},
        {"FANSELFTEST", {Command::FANSELFTEST, ValueType::Real}},
        {"FANPPR", {Command::FANPPR, ValueType::Real}},
        {"FANSPEED", {Command::FANSPEED, ValueType::Real}},
        {"FANRTOCPSTATUS", {Command::FANRTOCPSTATUS, ValueType::Integer}},
        {"FANRTOCPLT", {Command::FANRTOCPLT, ValueType::Real}},
        {"FANRTOCPHT", {Command::FANRTOCPHT, ValueType::Real}},
        {"SYSSN", {Command::SYSSN, ValueType::Text}},
        {"SYSPVIN", {Command::SYSPVIN, ValueType::Real}},
        {"SYSTIME", {Command::SYSTIME, ValueType::Real}},
        {"SYSMODEL", {Command::SYSMODEL, ValueType::Text}},
        {"SYSVERSION", {Command::SYSVERSION, ValueType::Text}},
        {"SYSCOMPANY", {Command::SYSCOMPANY, ValueType::Text}},
        {"SYSWEBSITE", {Command::SYSWEBSITE, ValueType::Text}},
        {"RESET", {Command::RESET, ValueType::Text}},
        {"COMADDR", {Command::COMADDR, ValueType::Real}},
        {"COMBAUDRATE", {Command::COMBAUDRATE, ValueType::Real}},
        {"COMMODBUS", {Command::COMMODBUS, ValueType::Real}},
        {"MEMWP", {Command::MEMWP, ValueType::Integer}},
        {"BATCH", {Command::BATCH, ValueType::Integer}},
        {"BATCHDEFAULT", {Command::BATCHDEFAULT, ValueType::Text}},
        {"BATCHSAVE", {Command::BATCHSAVE, ValueType::Text}},
        {"TMACTTEMP", {Command::TMACTTEMP, ValueType::Real}},
        {"TMOTPSTATUS", {Command::TMOTPSTATUS, ValueType::Integer}},
        {"TMOTPLT", {Command::TMOTPLT, ValueType::Real}},
        {"TMOTPHT", {Command::TMOTPHT, ValueType::Real}},
        {"TMOTPHHYS", {Command::TMOTPHHYS, ValueType::Real}},
        {"TMDTPSTATUS", {Command::TMDTPSTATUS, ValueType::Integer}},
        {"TMDTPLT", {Command::TMDTPLT, ValueType::Real}},
        {"TMDTPHT", {Command::TMDTPHT, ValueType::Real}},
        {"TMDTPHHYS", {Command::TMDTPHHYS, ValueType::Real}},
        {"LEDERRMODE", {Command::LEDERRMODE, ValueType::Integer}},
        {"LEDSWMODE", {Command::LEDSWMODE, ValueType::Integer}},
        {"ASEN", {Command::ASEN, ValueType::Integer}},
        {"TCINPARALLEL", {Command::TCINPARALLEL, ValueType::Integer}},
        {"TC1KEYPOL", {Command::TC1KEYPOL, ValueType::Integer}},
        {"TC2KEYPOL", {Command::TC2KEYPOL, ValueType::Integer}},
        {"CONNIN1DEFVOL", {Command::CONNIN1DEFVOL, ValueType::Integer}},
        {"CONNIN2DEFVOL", {Command::CONNIN2DEFVOL, ValueType::Integer}},
        {"CONNIN1", {Command::CONNIN1, ValueType::Integer}},
        {"CONNIN2", {Command::CONNIN2, ValueType::Integer}},
        {"CONNOUT1POL", {Command::CONNOUT1POL, ValueType::Integer}},
        {"CONNOUT2POL", {Command::CONNOUT2POL, ValueType::Integer}},
        {"CONNOUT1", {Command::CONNOUT1, ValueType::Integer}},
        {"CONNOUT2", {Command::CONNOUT2, ValueType::Integer}},
        {"FAN1MAINTEMP", {Command::FAN1MAINTEMP, ValueType::Integer}},
        {"FAN2MAINTEMP", {Command::FAN2MAINTEMP, ValueType::Integer}},
        {"CONNIN", {Command::CONNIN, ValueType::Integer}},
        {"CONNINDEFVOL", {Command::CONNINDEFVOL, ValueType::Integer}},
        {"CONNOUT", {Command::CONNOUT, ValueType::Integer}},
        {"CONNOUTPOL", {Command::CONNOUTPOL, ValueType::Integer}},
        {"TCTSRELOAD", {Command::TCTSRELOAD, ValueType::Text}},
        {"REPLY", {Command::REPLY, ValueType::ReplyCode}},
    };

    static QVariant decode(ValueType type, QByteArrayView value)
    {
        bool ok;
        switch (type) {
        case ValueType::Real: {
            const double number = value.toDouble(&ok);
            return ok && std::isfinite(number) ? QVariant(number) : QVariant();
        }
        case ValueType::Integer: {
            const auto number = value.toLongLong(&ok, 10);
            return ok ? QVariant(number) : QVariant();
        }
        case ValueType::Text:
            return QString::fromLatin1(value);
        case ValueType::ReplyCode:
            if (value.size() == 1 && value[0] >= '0' && value[0] <= '8') return value[0] - '0';
        }
        return {};
    }

    static std::optional<quint8> addressValue(QByteArrayView digits) noexcept
    {
        if (digits.isEmpty() || digits.size() > 3)
            return std::nullopt;
        int value = 0;
        for (const char digit : digits) {
            if (digit < '0' || digit > '9')
                return std::nullopt;
            value = 10 * value + (digit - '0');
        }
        if (value > 255) return std::nullopt;
        return static_cast<quint8>(value);
    }
};
}
