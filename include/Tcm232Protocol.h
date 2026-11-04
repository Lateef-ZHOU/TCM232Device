#pragma once
#include <variant>
#include <QByteArray>
#include <QByteArrayView>

namespace Tcm232
{
struct Protocol
{
    // 返回视图只借用本次回报主体；跨线程交付时转为自持的 QByteArray。
    struct Parameter { QByteArrayView name, value; int address; };
    struct CommandReply { int code, address; };
    enum class Error { Address, ParameterName, ReplyCode };

    [[nodiscard]] static std::variant<Parameter, CommandReply, Error> parseReply(QByteArrayView line)
    {
        int address = -1;
        const auto at = line.lastIndexOf('@');
        if (at >= 0) {
            address = addressValue(line.sliced(at + 1));
            if (address < 0)
                return Error::Address;
            line = line.first(at);
        }
        const auto eq = line.indexOf('=');
        if (eq < 1)
            return Error::ParameterName;
        const auto name = line.first(eq);
        const auto value = line.sliced(eq + 1);
        if (name == "CMD:REPLY") {
            if (value.size() != 1 || value.front() < '0' || value.front() > '8')
                return Error::ReplyCode;
            return CommandReply{value.front() - '0', address};
        }
        // 至少有“模块:参数”两段，部分参数另带系数下标（如 TC2:TCTSCOEFF:0）。
        if (!name.contains(':') || name.startsWith(':') || name.endsWith(':') || name.contains("::"))
            return Error::ParameterName;
        for (const char c : name)
            if (c <= ' ' || c > '~' || c == '?' || c == '!' || c == '#')
                return Error::ParameterName;
        return Parameter{name, value, address};
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
        if (at <= 0 || addressValue(QByteArrayView(line).sliced(at + 1, hash - at - 1)) < 0)
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
    static int addressValue(QByteArrayView digits) noexcept
    {
        if (digits.isEmpty() || digits.size() > 3)
            return -1;
        int value = 0;
        for (const char digit : digits) {
            if (digit < '0' || digit > '9')
                return -1;
            value = 10 * value + (digit - '0');
        }
        return value <= 255 ? value : -1;
    }
};
}
