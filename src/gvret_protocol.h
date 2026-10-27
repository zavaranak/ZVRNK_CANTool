#pragma once

#include <QByteArray>
#include <QVector>

#include <optional>

// Binary GVRET serial protocol, as spoken by SavvyCAN and GVRET/ESP32RET
// firmware. Commands are "F1 <command> <payload>", all integers little-endian.
namespace gvret {

constexpr quint8 CommandStart = 0xF1;
constexpr quint8 EnableBinary = 0xE7;
constexpr quint32 SerialBaudRate = 1000000;

enum Command : quint8 {
    CanFrame = 0,
    TimeSync = 1,
    SetupCanBus = 5,
    GetCanBusParams = 6,
    GetDeviceInfo = 7,
    KeepAlive = 9,
    GetNumBuses = 12,
};

struct Frame
{
    quint32 timestampUs = 0;
    quint32 id = 0;
    bool extended = false;
    quint8 bus = 0;
    QByteArray data;
};

struct BusParams
{
    bool enabled = false;
    bool listenOnly = false;
    quint32 bitrate = 0;
};

struct DeviceInfo
{
    quint16 buildNumber = 0;
};

struct ParseResult
{
    QVector<Frame> frames;
    std::optional<DeviceInfo> deviceInfo;
    std::optional<BusParams> busParams;
};

QByteArray command(Command value);
QByteArray handshake();
QByteArray setupBus(quint8 bus, quint32 bitrate, bool listenOnly);
QByteArray encodeFrame(quint32 id, bool extended, quint8 bus, const QByteArray& data);

class Parser
{
public:
    void reset();
    void feed(const QByteArray& bytes, ParseResult& result);

private:
    enum class State {
        Idle,
        Command,
        Frame,
        BusParams,
        DeviceInfo,
        Skip,
    };

    void feedByte(quint8 value, ParseResult& result);

    State state_ = State::Idle;
    int step_ = 0;
    int skipRemaining_ = 0;
    quint32 word_ = 0;
    Frame frame_;
    int frameLength_ = 0;
    BusParams busParams_;
    DeviceInfo deviceInfo_;
};

}
