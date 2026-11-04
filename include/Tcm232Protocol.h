#pragma once
#include <cmath>
#include <optional>
#include <variant>
#include <QVariant>
#include <QByteArray>
#include <QByteArrayView>

namespace Tcm232
{
struct Protocol
{
    enum class Module { TC1, TC2, TA1, TA2, NTS1, NTS2, PWG1, PWG2, FAN1, FAN2, SYSTEM, SYS, RS485, UIRS232, MEMORY, PARAM, PCB, MUI, BOARDCONFIG, CMD };
    enum class Operation : char { Equals = '=', Query = '?', Save = '!' };
    enum class Error { Address, ModuleName, Syntax };

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

    [[nodiscard]] static constexpr std::optional<Module> moduleOf(QByteArrayView name) noexcept
    {
        for (const auto &entry : modules)
            if (entry.name == name) return entry.module;
        return std::nullopt;
    }

    [[nodiscard]] static constexpr QByteArrayView nameOf(Module module) noexcept
    {
        for (const auto &entry : modules)
            if (entry.module == module) return entry.name;
        return {};
    }

    // 输入是 Handle 去掉 CR、校验后的主体。两个字段视图的寿命与输入相同。
    struct Packet {
        Module module;
        Operation operation;
        std::optional<quint8> address;
        QByteArrayView parameter, rawValue;
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
                return Packet{*module, static_cast<Operation>(c), address,
                              line.sliced(colon + 1, i - colon - 1), line.sliced(i + 1)};
            }
            if (c <= ' ' || c > '~' || c == '#' || (c == ':' && emptySegment)) return Error::Syntax;
            emptySegment = c == ':';
        }
        return Error::Syntax;
    }

    // 转换规则取自型号命令表。字符串和未规定数值编码的字段按原文保存。
    // 无效 QVariant 表示非等号报文或字段格式错误。
    [[nodiscard]] static QVariant parseValue(const Packet &p)
    {
        if (p.operation != Operation::Equals) return {};
        const auto value = p.rawValue;
        if (p.module == Module::CMD) {
            if (p.parameter != "REPLY" || value.size() != 1 || value[0] < '0' || value[0] > '8') return {};
            return value[0] - '0';
        }

        Module family = p.module;
        switch (p.module) {
        case Module::TC2: family = Module::TC1; break;
        case Module::TA2: family = Module::TA1; break;
        case Module::NTS2: family = Module::NTS1; break;
        case Module::PWG2: family = Module::PWG1; break;
        case Module::FAN2: family = Module::FAN1; break;
        default: break;
        }
        for (const auto &field : numericFields) {
            if (field.family != family || field.parameter != p.parameter) continue;
            bool ok;
            if (field.type == NumberType::Integer) {
                const auto number = value.toLongLong(&ok, 10);
                return ok ? QVariant(number) : QVariant();
            }
            const double number = value.toDouble(&ok);
            return ok && std::isfinite(number) ? QVariant(number) : QVariant();
        }
        return QString::fromLatin1(value);
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
    enum class NumberType { Real, Integer };
    struct NumericField { Module family; QByteArrayView parameter; NumberType type; };
    inline static constexpr NumericField numericFields[] = {
        {Module::TC1, "TCADJTEMP", NumberType::Real},
        {Module::TC1, "TCACTTEMP", NumberType::Real},
        {Module::TC1, "TCSETTEMP", NumberType::Real},
        {Module::TC1, "TCRAMPSPEED", NumberType::Real},
        {Module::TC1, "TCMAXTEMP", NumberType::Real},
        {Module::TC1, "TCMINTEMP", NumberType::Real},
        {Module::TC1, "TCSTEPTEMP", NumberType::Real},
        {Module::TC1, "TCDECIMALDIGITS", NumberType::Real},
        {Module::TC1, "TCMAXV", NumberType::Real},
        {Module::TC1, "TCCOOLVRATIO", NumberType::Real},
        {Module::TC1, "TCMODE", NumberType::Integer},
        {Module::TC1, "TCSW", NumberType::Integer},
        {Module::TC1, "TCOE", NumberType::Integer},
        {Module::TC1, "TCSETVOL", NumberType::Real},
        {Module::TC1, "TCACTVOL", NumberType::Real},
        {Module::TC1, "TCACTCUR", NumberType::Real},
        {Module::TC1, "TCLOADRES", NumberType::Real},
        {Module::TC1, "TCLOADDETECT", NumberType::Integer},
        {Module::TC1, "TCLOADRESLT", NumberType::Real},
        {Module::TC1, "TCLOADRESHT", NumberType::Real},
        {Module::TC1, "TCTSTEMP", NumberType::Real},
        {Module::TC1, "TCTSOFFSET", NumberType::Real},
        {Module::TC1, "TCTSSTEP", NumberType::Real},
        {Module::TC1, "TCAUXTSFUNC", NumberType::Integer},
        {Module::TC1, "TCAUXTSINDEX", NumberType::Integer},
        {Module::TC1, "TCAUXTSACTTEMP", NumberType::Real},
        {Module::TC1, "TCAUXTSSAFETY", NumberType::Integer},
        {Module::TC1, "TCAUXTSOTPLT", NumberType::Real},
        {Module::TC1, "TCAUXTSOTPHT", NumberType::Real},
        {Module::TC1, "TCRTOCPSTATUS", NumberType::Integer},
        {Module::TC1, "TCRTOCPLT", NumberType::Real},
        {Module::TC1, "TCRTOCPHT", NumberType::Real},
        {Module::TC1, "TCPIDTYPE", NumberType::Integer},
        {Module::TC1, "TCPIDP", NumberType::Real},
        {Module::TC1, "TCPIDTI", NumberType::Real},
        {Module::TC1, "TCPIDTD", NumberType::Real},
        {Module::TC1, "TCCTRLINTERVAL", NumberType::Real},
        {Module::TC1, "TCPIDALGO", NumberType::Integer},
        {Module::TC1, "TCTUNESTATUS", NumberType::Real},
        {Module::TC1, "TCPIDWINDOW", NumberType::Real},
        {Module::TC1, "TCPIDACCWINDOW", NumberType::Real},
        {Module::TC1, "TCOVPADJTHR", NumberType::Real},
        {Module::TC1, "TCOVPACTTHR", NumberType::Real},
        {Module::TC1, "TCOVPSTATUS", NumberType::Integer},
        {Module::TC1, "TCOCPADJTHR", NumberType::Real},
        {Module::TC1, "TCOCPACTTHR", NumberType::Real},
        {Module::TC1, "TCOCPSTATUS", NumberType::Integer},
        {Module::TC1, "TCCURLIMIT", NumberType::Real},
        {Module::TC1, "TCOTPMODE", NumberType::Integer},
        {Module::TC1, "TCOTPLT", NumberType::Real},
        {Module::TC1, "TCOTPHT", NumberType::Real},
        {Module::TC1, "TCOTPSTATUS", NumberType::Integer},
        {Module::TC1, "TCOTPLHYS", NumberType::Real},
        {Module::TC1, "TCOTPHHYS", NumberType::Real},
        {Module::TC1, "TCEXTSTATUS", NumberType::Integer},
        {Module::TC1, "TCEXTLOCK", NumberType::Integer},
        {Module::TC1, "TCEXTLOCKPOL", NumberType::Integer},
        {Module::TC1, "TCEXTCTRL", NumberType::Real},
        {Module::TC1, "TCAR", NumberType::Real},
        {Module::TC1, "TCARCNT", NumberType::Real},
        {Module::TC1, "TCARDELAY", NumberType::Real},
        {Module::TC1, "TCERRORLOG", NumberType::Integer},
        {Module::TC1, "TCERRORSTATUS", NumberType::Integer},
        {Module::TC1, "TCERRORMASK", NumberType::Integer},
        {Module::TA1, "TAACTTEMP", NumberType::Real},
        {Module::TA1, "TATSOFFSET", NumberType::Real},
        {Module::TA1, "TAOTPSTATUS", NumberType::Integer},
        {Module::TA1, "TAOTPLT", NumberType::Real},
        {Module::TA1, "TAOTPHT", NumberType::Real},
        {Module::NTS1, "NTSACTTEMP", NumberType::Real},
        {Module::NTS1, "NTSSAFETY", NumberType::Integer},
        {Module::NTS1, "NTSSTATUS", NumberType::Integer},
        {Module::NTS1, "NTSTIMEOUT", NumberType::Real},
        {Module::NTS1, "NTSDEFTEMP", NumberType::Real},
        {Module::PWG1, "PWGADJFREQ", NumberType::Real},
        {Module::PWG1, "PWGSETFREQ", NumberType::Real},
        {Module::PWG1, "PWGADJPW", NumberType::Real},
        {Module::PWG1, "PWGSETPW", NumberType::Real},
        {Module::PWG1, "PWGPOL", NumberType::Integer},
        {Module::FAN1, "FANACTTEMP", NumberType::Real},
        {Module::FAN1, "FANSTARTTEMP", NumberType::Real},
        {Module::FAN1, "FANMAXTEMP", NumberType::Real},
        {Module::FAN1, "FANHYSTEMP", NumberType::Real},
        {Module::FAN1, "FANAUXTSFUNC", NumberType::Integer},
        {Module::FAN1, "FANAUXTSINDEX", NumberType::Integer},
        {Module::FAN1, "FANAUXTSACTTEMP", NumberType::Real},
        {Module::FAN1, "FANAUXTSSAFETY", NumberType::Integer},
        {Module::FAN1, "FANAUXTSOTPLT", NumberType::Real},
        {Module::FAN1, "FANAUXTSOTPHT", NumberType::Real},
        {Module::FAN1, "FANACTDUTY", NumberType::Real},
        {Module::FAN1, "FANSTARTDUTY", NumberType::Real},
        {Module::FAN1, "FANMAXDUTY", NumberType::Real},
        {Module::FAN1, "FANIDLEDUTY", NumberType::Real},
        {Module::FAN1, "FANSAFEDUTY", NumberType::Real},
        {Module::FAN1, "FANSW", NumberType::Integer},
        {Module::FAN1, "FANOUTPOL", NumberType::Integer},
        {Module::FAN1, "FANPWMFREQ", NumberType::Real},
        {Module::FAN1, "FANSELFTEST", NumberType::Real},
        {Module::FAN1, "FANPPR", NumberType::Real},
        {Module::FAN1, "FANSPEED", NumberType::Real},
        {Module::FAN1, "FANRTOCPSTATUS", NumberType::Integer},
        {Module::FAN1, "FANRTOCPLT", NumberType::Real},
        {Module::FAN1, "FANRTOCPHT", NumberType::Real},
        {Module::SYSTEM, "SYSPVIN", NumberType::Real},
        {Module::SYSTEM, "SYSTIME", NumberType::Real},
        {Module::RS485, "COMADDR", NumberType::Real},
        {Module::RS485, "COMBAUDRATE", NumberType::Real},
        {Module::RS485, "COMMODBUS", NumberType::Real},
        {Module::UIRS232, "COMADDR", NumberType::Real},
        {Module::UIRS232, "COMMODBUS", NumberType::Real},
        {Module::MEMORY, "MEMWP", NumberType::Integer},
        {Module::PARAM, "BATCH", NumberType::Integer},
        {Module::PCB, "TMACTTEMP", NumberType::Real},
        {Module::PCB, "TMOTPSTATUS", NumberType::Integer},
        {Module::PCB, "TMOTPLT", NumberType::Real},
        {Module::PCB, "TMOTPHT", NumberType::Real},
        {Module::PCB, "TMOTPHHYS", NumberType::Real},
        {Module::PCB, "TMDTPSTATUS", NumberType::Integer},
        {Module::PCB, "TMDTPLT", NumberType::Real},
        {Module::PCB, "TMDTPHT", NumberType::Real},
        {Module::PCB, "TMDTPHHYS", NumberType::Real},
        {Module::MUI, "LEDERRMODE", NumberType::Integer},
        {Module::MUI, "LEDSWMODE", NumberType::Integer},
        {Module::BOARDCONFIG, "ASEN", NumberType::Integer},
        {Module::BOARDCONFIG, "TCINPARALLEL", NumberType::Integer},
        {Module::BOARDCONFIG, "TC1KEYPOL", NumberType::Integer},
        {Module::BOARDCONFIG, "TC2KEYPOL", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNIN1DEFVOL", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNIN2DEFVOL", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNIN1", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNIN2", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNOUT1POL", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNOUT2POL", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNOUT1", NumberType::Integer},
        {Module::BOARDCONFIG, "CONNOUT2", NumberType::Integer},
        {Module::BOARDCONFIG, "FAN1MAINTEMP", NumberType::Integer},
        {Module::BOARDCONFIG, "FAN2MAINTEMP", NumberType::Integer},
    };

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
