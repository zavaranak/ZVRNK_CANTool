#include "gvret_protocol.h"

namespace gvret {
namespace {

void appendU32(QByteArray& out, quint32 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

constexpr quint32 BusConfigValid = 0x80000000;
constexpr quint32 BusConfigEnabled = 0x40000000;
constexpr quint32 BusConfigListenOnly = 0x20000000;
constexpr quint32 BusConfigBitrateMask = 0x000FFFFF;

}

QByteArray command(Command value)
{
    QByteArray out;
    out.append(static_cast<char>(CommandStart));
    out.append(static_cast<char>(value));
    return out;
}

QByteArray handshake()
{
    // Two 0xE7 bytes switch GVRET firmware from its text console into binary
    // mode; firmware that is already binary ignores bytes outside a command.
    QByteArray out;
    out.append(static_cast<char>(EnableBinary));
    out.append(static_cast<char>(EnableBinary));
    out += command(GetDeviceInfo);
    out += command(GetCanBusParams);
    return out;
}

QByteArray setupBus(quint8 bus, quint32 bitrate, bool listenOnly)
{
    quint32 config = BusConfigValid | BusConfigEnabled | (bitrate & BusConfigBitrateMask);
    if (listenOnly) {
        config |= BusConfigListenOnly;
    }

    // A word without the valid bit leaves that bus untouched.
    QByteArray out = command(SetupCanBus);
    appendU32(out, bus == 0 ? config : 0);
    appendU32(out, bus == 1 ? config : 0);
    out.append('\0');
    return out;
}

QByteArray encodeFrame(quint32 id, bool extended, quint8 bus, const QByteArray& data)
{
    QByteArray out = command(CanFrame);
    appendU32(out, extended ? (id | 0x80000000) : id);
    out.append(static_cast<char>(bus & 3));
    out.append(static_cast<char>(data.size()));
    out += data;
    out.append('\0');
    return out;
}

void Parser::reset()
{
    state_ = State::Idle;
    step_ = 0;
    skipRemaining_ = 0;
}

void Parser::feed(const QByteArray& bytes, ParseResult& result)
{
    for (const char value : bytes) {
        feedByte(static_cast<quint8>(value), result);
    }
}

void Parser::feedByte(quint8 value, ParseResult& result)
{
    switch (state_) {
    case State::Idle:
        if (value == CommandStart) {
            state_ = State::Command;
        }
        return;

    case State::Command:
        step_ = 0;
        word_ = 0;
        switch (value) {
        case CanFrame:
            frame_ = Frame {};
            state_ = State::Frame;
            return;
        case GetCanBusParams:
            busParams_ = BusParams {};
            state_ = State::BusParams;
            return;
        case GetDeviceInfo:
            deviceInfo_ = DeviceInfo {};
            state_ = State::DeviceInfo;
            return;
        case TimeSync:
            skipRemaining_ = 4;
            state_ = State::Skip;
            return;
        case GetNumBuses:
            skipRemaining_ = 1;
            state_ = State::Skip;
            return;
        default:
            // Keep-alive replies carry trailing bytes that Idle discards.
            state_ = State::Idle;
            return;
        }

    case State::Frame:
        if (step_ < 4) {
            frame_.timestampUs |= static_cast<quint32>(value) << (8 * step_);
        } else if (step_ < 8) {
            word_ |= static_cast<quint32>(value) << (8 * (step_ - 4));
            if (step_ == 7) {
                frame_.extended = (word_ & 0x80000000) != 0;
                frame_.id = word_ & 0x1FFFFFFF;
            }
        } else if (step_ == 8) {
            frameLength_ = value & 0x0F;
            frame_.bus = static_cast<quint8>(value >> 4);
            if (frameLength_ > 8) {
                state_ = State::Idle;
                return;
            }
            frame_.data.reserve(frameLength_);
        } else {
            frame_.data.append(static_cast<char>(value));
        }

        if (step_ >= 8 && frame_.data.size() == frameLength_) {
            result.frames.push_back(frame_);
            state_ = State::Idle;
            return;
        }
        ++step_;
        return;

    case State::BusParams:
        if (step_ == 0) {
            busParams_.enabled = (value & 0x0F) != 0;
            busParams_.listenOnly = (value >> 4) != 0;
        } else if (step_ <= 4) {
            busParams_.bitrate |= static_cast<quint32>(value) << (8 * (step_ - 1));
        }
        if (step_ == 9) {
            result.busParams = busParams_;
            state_ = State::Idle;
            return;
        }
        ++step_;
        return;

    case State::DeviceInfo:
        if (step_ < 2) {
            deviceInfo_.buildNumber |= static_cast<quint16>(value << (8 * step_));
        }
        if (step_ == 5) {
            result.deviceInfo = deviceInfo_;
            state_ = State::Idle;
            return;
        }
        ++step_;
        return;

    case State::Skip:
        if (--skipRemaining_ <= 0) {
            state_ = State::Idle;
        }
        return;
    }
}

}
