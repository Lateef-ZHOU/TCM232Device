#pragma once
#include "Tcm232Packet.h"
#include <cmath>
#include <optional>
#include <iterator>
#include <limits>
#include <QLocale>
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
    using Module = Tcm232::Module;
    using Command = Tcm232::Command;
    using ValueType = Tcm232::ValueType;
    using Packet = Tcm232::Packet;
    enum class Error { Address, ModuleName, Syntax, CommandName, Value };

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

private:
    struct CommandSpec { Command command; QByteArrayView name; ValueType type; };
    // 按枚举顺序登记一次。下行按下标读取，上行哈希只索引同一条记录。
    inline static constexpr CommandSpec commands[] = {
        {Command::TCADJTEMP, "TCADJTEMP", ValueType::Real},
        {Command::TCACTTEMP, "TCACTTEMP", ValueType::Real},
        {Command::TCSETTEMP, "TCSETTEMP", ValueType::Real},
        {Command::TCRAMPSPEED, "TCRAMPSPEED", ValueType::Real},
        {Command::TCMAXTEMP, "TCMAXTEMP", ValueType::Real},
        {Command::TCMINTEMP, "TCMINTEMP", ValueType::Real},
        {Command::TCSTEPTEMP, "TCSTEPTEMP", ValueType::Real},
        {Command::TCDECIMALDIGITS, "TCDECIMALDIGITS", ValueType::Real},
        {Command::TCMAXV, "TCMAXV", ValueType::Real},
        {Command::TCCOOLVRATIO, "TCCOOLVRATIO", ValueType::Real},
        {Command::TCMODE, "TCMODE", ValueType::Integer},
        {Command::TCSW, "TCSW", ValueType::Integer},
        {Command::TCOE, "TCOE", ValueType::Integer},
        {Command::TCSETVOL, "TCSETVOL", ValueType::Real},
        {Command::TCACTVOL, "TCACTVOL", ValueType::Real},
        {Command::TCACTCUR, "TCACTCUR", ValueType::Real},
        {Command::TCLOADRES, "TCLOADRES", ValueType::Real},
        {Command::TCLOADDETECT, "TCLOADDETECT", ValueType::Integer},
        {Command::TCLOADRESLT, "TCLOADRESLT", ValueType::Real},
        {Command::TCLOADRESHT, "TCLOADRESHT", ValueType::Real},
        {Command::TCTSTEMP, "TCTSTEMP", ValueType::Real},
        {Command::TCTSOFFSET, "TCTSOFFSET", ValueType::Real},
        {Command::TCTSOUTPUT, "TCTSOUTPUT", ValueType::Text},
        {Command::TCTSTYPE, "TCTSTYPE", ValueType::Text},
        {Command::TCTSCOEFF_0, "TCTSCOEFF:0", ValueType::Text},
        {Command::TCTSCOEFF_1, "TCTSCOEFF:1", ValueType::Text},
        {Command::TCTSCOEFF_2, "TCTSCOEFF:2", ValueType::Text},
        {Command::TCTSCOEFF_3, "TCTSCOEFF:3", ValueType::Text},
        {Command::TCTSCOEFF_4, "TCTSCOEFF:4", ValueType::Text},
        {Command::TCTSSTEP, "TCTSSTEP", ValueType::Real},
        {Command::TCAUXTSFUNC, "TCAUXTSFUNC", ValueType::Integer},
        {Command::TCAUXTSINDEX, "TCAUXTSINDEX", ValueType::Integer},
        {Command::TCAUXTSACTTEMP, "TCAUXTSACTTEMP", ValueType::Real},
        {Command::TCAUXTSSAFETY, "TCAUXTSSAFETY", ValueType::Integer},
        {Command::TCAUXTSOTPLT, "TCAUXTSOTPLT", ValueType::Real},
        {Command::TCAUXTSOTPHT, "TCAUXTSOTPHT", ValueType::Real},
        {Command::TCRTOCPSTATUS, "TCRTOCPSTATUS", ValueType::Integer},
        {Command::TCRTOCPLT, "TCRTOCPLT", ValueType::Real},
        {Command::TCRTOCPHT, "TCRTOCPHT", ValueType::Real},
        {Command::TCPIDTYPE, "TCPIDTYPE", ValueType::Integer},
        {Command::TCPIDP, "TCPIDP", ValueType::Real},
        {Command::TCPIDTI, "TCPIDTI", ValueType::Real},
        {Command::TCPIDTD, "TCPIDTD", ValueType::Real},
        {Command::TCCTRLINTERVAL, "TCCTRLINTERVAL", ValueType::Real},
        {Command::TCPIDALGO, "TCPIDALGO", ValueType::Integer},
        {Command::TCTUNESTATUS, "TCTUNESTATUS", ValueType::Real},
        {Command::TCPIDWINDOW, "TCPIDWINDOW", ValueType::Real},
        {Command::TCPIDACCWINDOW, "TCPIDACCWINDOW", ValueType::Real},
        {Command::TCOVPADJTHR, "TCOVPADJTHR", ValueType::Real},
        {Command::TCOVPACTTHR, "TCOVPACTTHR", ValueType::Real},
        {Command::TCOVPSTATUS, "TCOVPSTATUS", ValueType::Integer},
        {Command::TCOCPADJTHR, "TCOCPADJTHR", ValueType::Real},
        {Command::TCOCPACTTHR, "TCOCPACTTHR", ValueType::Real},
        {Command::TCOCPSTATUS, "TCOCPSTATUS", ValueType::Integer},
        {Command::TCCURLIMIT, "TCCURLIMIT", ValueType::Real},
        {Command::TCOTPMODE, "TCOTPMODE", ValueType::Integer},
        {Command::TCOTPLT, "TCOTPLT", ValueType::Real},
        {Command::TCOTPHT, "TCOTPHT", ValueType::Real},
        {Command::TCOTPSTATUS, "TCOTPSTATUS", ValueType::Integer},
        {Command::TCOTPLHYS, "TCOTPLHYS", ValueType::Real},
        {Command::TCOTPHHYS, "TCOTPHHYS", ValueType::Real},
        {Command::TCEXTSTATUS, "TCEXTSTATUS", ValueType::Integer},
        {Command::TCEXTLOCK, "TCEXTLOCK", ValueType::Integer},
        {Command::TCEXTLOCKPOL, "TCEXTLOCKPOL", ValueType::Integer},
        {Command::TCEXTCTRL, "TCEXTCTRL", ValueType::Real},
        {Command::TCAR, "TCAR", ValueType::Real},
        {Command::TCARCNT, "TCARCNT", ValueType::Real},
        {Command::TCARDELAY, "TCARDELAY", ValueType::Real},
        {Command::TCERRORLOG, "TCERRORLOG", ValueType::Integer},
        {Command::TCERRORSTATUS, "TCERRORSTATUS", ValueType::Integer},
        {Command::TCERRORMASK, "TCERRORMASK", ValueType::Integer},
        {Command::TAACTTEMP, "TAACTTEMP", ValueType::Real},
        {Command::TATSOFFSET, "TATSOFFSET", ValueType::Real},
        {Command::TATSOUTPUT, "TATSOUTPUT", ValueType::Text},
        {Command::TATSTYPE, "TATSTYPE", ValueType::Text},
        {Command::TATSCOEFF_0, "TATSCOEFF:0", ValueType::Text},
        {Command::TATSCOEFF_1, "TATSCOEFF:1", ValueType::Text},
        {Command::TATSCOEFF_2, "TATSCOEFF:2", ValueType::Text},
        {Command::TATSCOEFF_3, "TATSCOEFF:3", ValueType::Text},
        {Command::TATSCOEFF_4, "TATSCOEFF:4", ValueType::Text},
        {Command::TAOTPSTATUS, "TAOTPSTATUS", ValueType::Integer},
        {Command::TAOTPLT, "TAOTPLT", ValueType::Real},
        {Command::TAOTPHT, "TAOTPHT", ValueType::Real},
        {Command::NTSACTTEMP, "NTSACTTEMP", ValueType::Real},
        {Command::NTSSAFETY, "NTSSAFETY", ValueType::Integer},
        {Command::NTSSTATUS, "NTSSTATUS", ValueType::Integer},
        {Command::NTSTIMEOUT, "NTSTIMEOUT", ValueType::Real},
        {Command::NTSDEFTEMP, "NTSDEFTEMP", ValueType::Real},
        {Command::PWGADJFREQ, "PWGADJFREQ", ValueType::Real},
        {Command::PWGSETFREQ, "PWGSETFREQ", ValueType::Real},
        {Command::PWGADJPW, "PWGADJPW", ValueType::Real},
        {Command::PWGSETPW, "PWGSETPW", ValueType::Real},
        {Command::PWGPOL, "PWGPOL", ValueType::Integer},
        {Command::FANACTTEMP, "FANACTTEMP", ValueType::Real},
        {Command::FANSTARTTEMP, "FANSTARTTEMP", ValueType::Real},
        {Command::FANMAXTEMP, "FANMAXTEMP", ValueType::Real},
        {Command::FANHYSTEMP, "FANHYSTEMP", ValueType::Real},
        {Command::FANAUXTSFUNC, "FANAUXTSFUNC", ValueType::Integer},
        {Command::FANAUXTSINDEX, "FANAUXTSINDEX", ValueType::Integer},
        {Command::FANAUXTSACTTEMP, "FANAUXTSACTTEMP", ValueType::Real},
        {Command::FANAUXTSSAFETY, "FANAUXTSSAFETY", ValueType::Integer},
        {Command::FANAUXTSOTPLT, "FANAUXTSOTPLT", ValueType::Real},
        {Command::FANAUXTSOTPHT, "FANAUXTSOTPHT", ValueType::Real},
        {Command::FANACTDUTY, "FANACTDUTY", ValueType::Real},
        {Command::FANSTARTDUTY, "FANSTARTDUTY", ValueType::Real},
        {Command::FANMAXDUTY, "FANMAXDUTY", ValueType::Real},
        {Command::FANIDLEDUTY, "FANIDLEDUTY", ValueType::Real},
        {Command::FANSAFEDUTY, "FANSAFEDUTY", ValueType::Real},
        {Command::FANSW, "FANSW", ValueType::Integer},
        {Command::FANOUTPOL, "FANOUTPOL", ValueType::Integer},
        {Command::FANPWMFREQ, "FANPWMFREQ", ValueType::Real},
        {Command::FANSELFTEST, "FANSELFTEST", ValueType::Real},
        {Command::FANPPR, "FANPPR", ValueType::Real},
        {Command::FANSPEED, "FANSPEED", ValueType::Real},
        {Command::FANRTOCPSTATUS, "FANRTOCPSTATUS", ValueType::Integer},
        {Command::FANRTOCPLT, "FANRTOCPLT", ValueType::Real},
        {Command::FANRTOCPHT, "FANRTOCPHT", ValueType::Real},
        {Command::SYSSN, "SYSSN", ValueType::Text},
        {Command::SYSPVIN, "SYSPVIN", ValueType::Real},
        {Command::SYSTIME, "SYSTIME", ValueType::Real},
        {Command::SYSMODEL, "SYSMODEL", ValueType::Text},
        {Command::SYSVERSION, "SYSVERSION", ValueType::Text},
        {Command::SYSCOMPANY, "SYSCOMPANY", ValueType::Text},
        {Command::SYSWEBSITE, "SYSWEBSITE", ValueType::Text},
        {Command::RESET, "RESET", ValueType::Text},
        {Command::COMADDR, "COMADDR", ValueType::Real},
        {Command::COMBAUDRATE, "COMBAUDRATE", ValueType::Real},
        {Command::COMMODBUS, "COMMODBUS", ValueType::Real},
        {Command::MEMWP, "MEMWP", ValueType::Integer},
        {Command::BATCH, "BATCH", ValueType::Integer},
        {Command::BATCHDEFAULT, "BATCHDEFAULT", ValueType::Text},
        {Command::BATCHSAVE, "BATCHSAVE", ValueType::Text},
        {Command::TMACTTEMP, "TMACTTEMP", ValueType::Real},
        {Command::TMOTPSTATUS, "TMOTPSTATUS", ValueType::Integer},
        {Command::TMOTPLT, "TMOTPLT", ValueType::Real},
        {Command::TMOTPHT, "TMOTPHT", ValueType::Real},
        {Command::TMOTPHHYS, "TMOTPHHYS", ValueType::Real},
        {Command::TMDTPSTATUS, "TMDTPSTATUS", ValueType::Integer},
        {Command::TMDTPLT, "TMDTPLT", ValueType::Real},
        {Command::TMDTPHT, "TMDTPHT", ValueType::Real},
        {Command::TMDTPHHYS, "TMDTPHHYS", ValueType::Real},
        {Command::LEDERRMODE, "LEDERRMODE", ValueType::Integer},
        {Command::LEDSWMODE, "LEDSWMODE", ValueType::Integer},
        {Command::ASEN, "ASEN", ValueType::Integer},
        {Command::TCINPARALLEL, "TCINPARALLEL", ValueType::Integer},
        {Command::TC1KEYPOL, "TC1KEYPOL", ValueType::Integer},
        {Command::TC2KEYPOL, "TC2KEYPOL", ValueType::Integer},
        {Command::CONNIN1DEFVOL, "CONNIN1DEFVOL", ValueType::Integer},
        {Command::CONNIN2DEFVOL, "CONNIN2DEFVOL", ValueType::Integer},
        {Command::CONNIN1, "CONNIN1", ValueType::Integer},
        {Command::CONNIN2, "CONNIN2", ValueType::Integer},
        {Command::CONNOUT1POL, "CONNOUT1POL", ValueType::Integer},
        {Command::CONNOUT2POL, "CONNOUT2POL", ValueType::Integer},
        {Command::CONNOUT1, "CONNOUT1", ValueType::Integer},
        {Command::CONNOUT2, "CONNOUT2", ValueType::Integer},
        {Command::FAN1MAINTEMP, "FAN1MAINTEMP", ValueType::Integer},
        {Command::FAN2MAINTEMP, "FAN2MAINTEMP", ValueType::Integer},
        {Command::CONNIN, "CONNIN", ValueType::Integer},
        {Command::CONNINDEFVOL, "CONNINDEFVOL", ValueType::Integer},
        {Command::CONNOUT, "CONNOUT", ValueType::Integer},
        {Command::CONNOUTPOL, "CONNOUTPOL", ValueType::Integer},
        {Command::TCTSRELOAD, "TCTSRELOAD", ValueType::Text},
        {Command::REPLY, "REPLY", ValueType::ReplyCode},
    };
    static_assert([] {
        for (std::size_t i = 0; i < std::size(commands); ++i)
            if (static_cast<std::size_t>(commands[i].command) != i) return false;
        return true;
    }());

    static constexpr const CommandSpec *specOf(Command command) noexcept
    {
        const auto i = static_cast<std::size_t>(command);
        return i < std::size(commands) ? &commands[i] : nullptr;
    }

    static const CommandSpec *specOf(QByteArrayView name)
    {
        static const auto byName = [] {
            QHash<QByteArrayView, const CommandSpec *> index;
            index.reserve(std::size(commands));
            for (const auto &entry : commands) index.insert(entry.name, &entry);
            return index;
        }();
        return byName.value(name, nullptr);
    }

    static bool appendValue(QByteArray &body, ValueType type, const QVariant &value, bool hasAddress)
    {
        if (type == ValueType::Text) {
            if (value.typeId() != QMetaType::QString) return false;
            for (QChar ch : value.toString()) {
                // '#' 与 CR 为串口保留字符；无地址时 '@' 会被当作地址后缀。
                if (ch.unicode() < 0x20 || ch.unicode() > 0x7e || ch == '#' || (ch == '@' && !hasAddress))
                    return false;
                body += ch.toLatin1();
            }
            return true;
        }

        if (value.isNull()) return false;
        QByteArray digits;
        switch (value.typeId()) {
        case QMetaType::Double: case QMetaType::Float: {
            const double n = value.toDouble();
            if (type != ValueType::Real || !std::isfinite(n)) return false;
            digits = QByteArray::number(n, 'g', QLocale::FloatingPointShortest);
            break;
        }
        case QMetaType::UInt: case QMetaType::ULong: case QMetaType::ULongLong:
        case QMetaType::UShort: case QMetaType::UChar: {
            const auto n = value.toULongLong();
            if (type == ValueType::Integer && n > static_cast<qulonglong>(std::numeric_limits<qlonglong>::max())) return false;
            digits = QByteArray::number(n);
            break;
        }
        case QMetaType::Int: case QMetaType::Long: case QMetaType::LongLong:
        case QMetaType::Short: case QMetaType::SChar:
            digits = QByteArray::number(value.toLongLong());
            break;
        default:
            return false;
        }
        if (type == ValueType::ReplyCode && (digits.size() != 1 || digits[0] > '8')) return false;
        body += digits;
        return true;
    }

    static QVariant decode(ValueType type, QByteArrayView value)
    {
        bool ok;
        switch (type) {
        case ValueType::Query:
        case ValueType::Save:
            return QVariant::fromValue(type);
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

public:
    // 名称查询与串口帧校验

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

    [[nodiscard]] static constexpr QByteArrayView nameOf(Command command) noexcept
    {
        const auto *spec = specOf(command);
        return spec ? spec->name : QByteArrayView{};
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

    // 整包收发

    // 返回 Handle 接受的 ASCII 主体。空结果表示不能封装，回车由串口 Handle 添加。
    [[nodiscard]] static QByteArray pack(const Packet &packet)
    {
        const auto moduleName = nameOf(packet.module);
        const auto *spec = specOf(packet.command);
        if (moduleName.isEmpty() || !spec) return {};

        QByteArray body = moduleName.toByteArray();
        body += ':';
        body.append(spec->name);
        if (packet.value.metaType() == QMetaType::fromType<ValueType>()) {
            switch (packet.value.value<ValueType>()) {
            case ValueType::Query: body += '?'; break;
            case ValueType::Save: body += '!'; break;
            default: return {};
            }
        } else {
            body += '=';
            if (!appendValue(body, spec->type, packet.value, packet.address.has_value())) return {};
        }
        if (packet.address) {
            body += '@';
            body += QByteArray::number(*packet.address);
        }
        return body;
    }

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
                const auto *field = specOf(line.sliced(colon + 1, i - colon - 1));
                if (!field) return Error::CommandName;

                auto type = field->type;
                if (c == '?') type = ValueType::Query;
                if (c == '!') type = ValueType::Save;
                auto value = decode(type, line.sliced(i + 1));
                if (!value.isValid()) return Error::Value;
                return Packet{address, *module, field->command, std::move(value)};
            }
            if (c <= ' ' || c > '~' || c == '#' || (c == ':' && emptySegment)) return Error::Syntax;
            emptySegment = c == ':';
        }
        return Error::Syntax;
    }
};
}
