#pragma once
#include <cmath>
#include <optional>
#include <variant>
#include <QHash>
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
        const auto type = numericTypes.constFind(p.parameter);
        if (type == numericTypes.cend()) return QString::fromLatin1(value);

        if (*type == NumberType::ReplyCode) {
            if (value.size() != 1 || value[0] < '0' || value[0] > '8') return {};
            return value[0] - '0';
        }
        bool ok;
        if (*type == NumberType::Integer) {
            const auto number = value.toLongLong(&ok, 10);
            return ok ? QVariant(number) : QVariant();
        }
        const double number = value.toDouble(&ok);
        return ok && std::isfinite(number) ? QVariant(number) : QVariant();
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
    enum class NumberType { Real, Integer, ReplyCode };
    // 键全部指向字符串字面量；查询直接使用报文中的参数名视图。
    inline static const QHash<QByteArrayView, NumberType> numericTypes = {
        {"REPLY", NumberType::ReplyCode},
        {"TCADJTEMP", NumberType::Real},
        {"TCACTTEMP", NumberType::Real},
        {"TCSETTEMP", NumberType::Real},
        {"TCRAMPSPEED", NumberType::Real},
        {"TCMAXTEMP", NumberType::Real},
        {"TCMINTEMP", NumberType::Real},
        {"TCSTEPTEMP", NumberType::Real},
        {"TCDECIMALDIGITS", NumberType::Real},
        {"TCMAXV", NumberType::Real},
        {"TCCOOLVRATIO", NumberType::Real},
        {"TCMODE", NumberType::Integer},
        {"TCSW", NumberType::Integer},
        {"TCOE", NumberType::Integer},
        {"TCSETVOL", NumberType::Real},
        {"TCACTVOL", NumberType::Real},
        {"TCACTCUR", NumberType::Real},
        {"TCLOADRES", NumberType::Real},
        {"TCLOADDETECT", NumberType::Integer},
        {"TCLOADRESLT", NumberType::Real},
        {"TCLOADRESHT", NumberType::Real},
        {"TCTSTEMP", NumberType::Real},
        {"TCTSOFFSET", NumberType::Real},
        {"TCTSSTEP", NumberType::Real},
        {"TCAUXTSFUNC", NumberType::Integer},
        {"TCAUXTSINDEX", NumberType::Integer},
        {"TCAUXTSACTTEMP", NumberType::Real},
        {"TCAUXTSSAFETY", NumberType::Integer},
        {"TCAUXTSOTPLT", NumberType::Real},
        {"TCAUXTSOTPHT", NumberType::Real},
        {"TCRTOCPSTATUS", NumberType::Integer},
        {"TCRTOCPLT", NumberType::Real},
        {"TCRTOCPHT", NumberType::Real},
        {"TCPIDTYPE", NumberType::Integer},
        {"TCPIDP", NumberType::Real},
        {"TCPIDTI", NumberType::Real},
        {"TCPIDTD", NumberType::Real},
        {"TCCTRLINTERVAL", NumberType::Real},
        {"TCPIDALGO", NumberType::Integer},
        {"TCTUNESTATUS", NumberType::Real},
        {"TCPIDWINDOW", NumberType::Real},
        {"TCPIDACCWINDOW", NumberType::Real},
        {"TCOVPADJTHR", NumberType::Real},
        {"TCOVPACTTHR", NumberType::Real},
        {"TCOVPSTATUS", NumberType::Integer},
        {"TCOCPADJTHR", NumberType::Real},
        {"TCOCPACTTHR", NumberType::Real},
        {"TCOCPSTATUS", NumberType::Integer},
        {"TCCURLIMIT", NumberType::Real},
        {"TCOTPMODE", NumberType::Integer},
        {"TCOTPLT", NumberType::Real},
        {"TCOTPHT", NumberType::Real},
        {"TCOTPSTATUS", NumberType::Integer},
        {"TCOTPLHYS", NumberType::Real},
        {"TCOTPHHYS", NumberType::Real},
        {"TCEXTSTATUS", NumberType::Integer},
        {"TCEXTLOCK", NumberType::Integer},
        {"TCEXTLOCKPOL", NumberType::Integer},
        {"TCEXTCTRL", NumberType::Real},
        {"TCAR", NumberType::Real},
        {"TCARCNT", NumberType::Real},
        {"TCARDELAY", NumberType::Real},
        {"TCERRORLOG", NumberType::Integer},
        {"TCERRORSTATUS", NumberType::Integer},
        {"TCERRORMASK", NumberType::Integer},
        {"TAACTTEMP", NumberType::Real},
        {"TATSOFFSET", NumberType::Real},
        {"TAOTPSTATUS", NumberType::Integer},
        {"TAOTPLT", NumberType::Real},
        {"TAOTPHT", NumberType::Real},
        {"NTSACTTEMP", NumberType::Real},
        {"NTSSAFETY", NumberType::Integer},
        {"NTSSTATUS", NumberType::Integer},
        {"NTSTIMEOUT", NumberType::Real},
        {"NTSDEFTEMP", NumberType::Real},
        {"PWGADJFREQ", NumberType::Real},
        {"PWGSETFREQ", NumberType::Real},
        {"PWGADJPW", NumberType::Real},
        {"PWGSETPW", NumberType::Real},
        {"PWGPOL", NumberType::Integer},
        {"FANACTTEMP", NumberType::Real},
        {"FANSTARTTEMP", NumberType::Real},
        {"FANMAXTEMP", NumberType::Real},
        {"FANHYSTEMP", NumberType::Real},
        {"FANAUXTSFUNC", NumberType::Integer},
        {"FANAUXTSINDEX", NumberType::Integer},
        {"FANAUXTSACTTEMP", NumberType::Real},
        {"FANAUXTSSAFETY", NumberType::Integer},
        {"FANAUXTSOTPLT", NumberType::Real},
        {"FANAUXTSOTPHT", NumberType::Real},
        {"FANACTDUTY", NumberType::Real},
        {"FANSTARTDUTY", NumberType::Real},
        {"FANMAXDUTY", NumberType::Real},
        {"FANIDLEDUTY", NumberType::Real},
        {"FANSAFEDUTY", NumberType::Real},
        {"FANSW", NumberType::Integer},
        {"FANOUTPOL", NumberType::Integer},
        {"FANPWMFREQ", NumberType::Real},
        {"FANSELFTEST", NumberType::Real},
        {"FANPPR", NumberType::Real},
        {"FANSPEED", NumberType::Real},
        {"FANRTOCPSTATUS", NumberType::Integer},
        {"FANRTOCPLT", NumberType::Real},
        {"FANRTOCPHT", NumberType::Real},
        {"SYSPVIN", NumberType::Real},
        {"SYSTIME", NumberType::Real},
        {"COMADDR", NumberType::Real},
        {"COMBAUDRATE", NumberType::Real},
        {"COMMODBUS", NumberType::Real},
        {"MEMWP", NumberType::Integer},
        {"BATCH", NumberType::Integer},
        {"TMACTTEMP", NumberType::Real},
        {"TMOTPSTATUS", NumberType::Integer},
        {"TMOTPLT", NumberType::Real},
        {"TMOTPHT", NumberType::Real},
        {"TMOTPHHYS", NumberType::Real},
        {"TMDTPSTATUS", NumberType::Integer},
        {"TMDTPLT", NumberType::Real},
        {"TMDTPHT", NumberType::Real},
        {"TMDTPHHYS", NumberType::Real},
        {"LEDERRMODE", NumberType::Integer},
        {"LEDSWMODE", NumberType::Integer},
        {"ASEN", NumberType::Integer},
        {"TCINPARALLEL", NumberType::Integer},
        {"TC1KEYPOL", NumberType::Integer},
        {"TC2KEYPOL", NumberType::Integer},
        {"CONNIN1DEFVOL", NumberType::Integer},
        {"CONNIN2DEFVOL", NumberType::Integer},
        {"CONNIN1", NumberType::Integer},
        {"CONNIN2", NumberType::Integer},
        {"CONNOUT1POL", NumberType::Integer},
        {"CONNOUT2POL", NumberType::Integer},
        {"CONNOUT1", NumberType::Integer},
        {"CONNOUT2", NumberType::Integer},
        {"FAN1MAINTEMP", NumberType::Integer},
        {"FAN2MAINTEMP", NumberType::Integer},
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
