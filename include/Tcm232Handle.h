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
                if (!body_.isEmpty() && !discarding_)
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
