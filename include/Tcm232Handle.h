#pragma once
#include <memory>
#include <SerialHandle.h>

namespace Tcm232
{
class Framer
{
public:
    // 设备没有同步头；收齐 CR 时使用本次读取的到达时刻。
    void feed(const QByteArray &chunk, RxTime at, const auto &deliver)
    {
        for (const char byte : chunk)
        {
            if (byte == '\r')
            {
                if (!body_.isEmpty() && acceptChecksum())
                    deliver(body_, at);
                body_.clear();
                discarding_ = false;
            }
            else if (!discarding_)
            {
                if (body_.size() < MaxBodyBytes)
                    body_.append(byte);
                else
                {
                    body_.clear();
                    discarding_ = true;
                }
            }
        }
    }

    // 本机收包缓冲上限，厂家没有规定协议最大行长。
    static constexpr qsizetype MaxBodyBytes = 4096;

private:
    // 有校验的行必须以 @地址#YY 收尾。异或包括 '#'，上传时保留地址。
    bool acceptChecksum()
    {
        const auto hash = body_.indexOf('#');
        if (hash < 0)
            return true;
        if (body_.size() - hash != 3)
            return false;
        const auto address = body_.lastIndexOf('@', hash);
        if (address <= 0 || hash - address < 2 || hash - address > 4)
            return false;
        int value = 0;
        for (auto i = address + 1; i < hash; ++i)
        {
            const char digit = body_.at(i);
            if (digit < '0' || digit > '9')
                return false;
            value = 10 * value + (digit - '0');
        }
        if (value > 255)
            return false;

        unsigned char sum = 0;
        for (qsizetype i = 0; i <= hash; ++i)
            sum ^= static_cast<unsigned char>(body_.at(i));
        constexpr char hex[] = "0123456789ABCDEF";
        if (body_.at(hash + 1) != hex[sum >> 4] || body_.at(hash + 2) != hex[sum & 15])
            return false;
        body_.truncate(hash);
        return true;
    }

    QByteArray body_;
    bool discarding_ = false;
};

class Handle : public SerialHandle<Framer>
{
    Q_OBJECT
public:
    [[nodiscard]] static SerialParams parameters(const SerialPortInfo &port)
    {
        SerialParams params;
        params.portInfo = port;
        params.baudRate = qToUnderlying(BaudRate::B57600);
        return params;
    }

Q_SIGNALS:
    void connectionChanged(HandleCore::ResourcePhase phase) override;
    void logMessage(const QString &text) const final;

private:
    RXRuling onRX(const QByteArray &body, RxTime at, std::shared_ptr<SerialConn<Framer>>) final
    {
        return {body, RXRuling::Decode, at};
    }

    TXRuling onTX(const QByteArray &body, std::shared_ptr<SerialConn<Framer>>) const final
    {
        if (body.isEmpty() || body.contains('\r') || body.contains('\n'))
        {
            Q_EMIT logMessage(QStringLiteral("温控命令须为非空单行主体，未发送"));
            return {};
        }
        return {body + '\r', TXRuling::Encode};
    }
};
}
