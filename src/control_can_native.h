#pragma once

#include "can_types.h"

#include <QLibrary>
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#define WINAPI
#endif

class ControlCanNative
{
public:
    static constexpr quint32 VciUsbCan2 = 4;
    static constexpr quint32 StatusOk = 1;
    static constexpr int MaxFindDevices = 50;

    struct VciBoardInfo
    {
        quint16 hwVersion;
        quint16 fwVersion;
        quint16 drVersion;
        quint16 inVersion;
        quint16 irqNum;
        quint8 canNum;
        char serialNumber[20];
        char hardwareType[40];
        quint16 reserved[4];
    };

    struct VciCanObj
    {
        quint32 id;
        quint32 timeStamp;
        quint8 timeFlag;
        quint8 sendType;
        quint8 remoteFlag;
        quint8 externFlag;
        quint8 dataLen;
        quint8 data[8];
        quint8 reserved[3];
    };

    struct VciInitConfig
    {
        quint32 accCode;
        quint32 accMask;
        quint32 reserved;
        quint8 filter;
        quint8 timing0;
        quint8 timing1;
        quint8 mode;
    };

    static ControlCanNative& instance();

    bool ensureLoaded(QString* error = nullptr);
    QVector<CanDeviceInfo> findDevices(QString* error = nullptr);

    quint32 openDevice(quint32 deviceType, quint32 deviceIndex, quint32 reserved);
    quint32 closeDevice(quint32 deviceType, quint32 deviceIndex);
    quint32 initCan(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciInitConfig* config);
    quint32 startCan(quint32 deviceType, quint32 deviceIndex, quint32 canIndex);
    quint32 resetCan(quint32 deviceType, quint32 deviceIndex, quint32 canIndex);
    quint32 transmit(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciCanObj* frames, quint32 length);
    quint32 receive(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciCanObj* frames, quint32 length, int waitTimeMs);
    quint32 getReceiveNum(quint32 deviceType, quint32 deviceIndex, quint32 canIndex);

private:
    using OpenDeviceFn = quint32 (WINAPI*)(quint32, quint32, quint32);
    using CloseDeviceFn = quint32 (WINAPI*)(quint32, quint32);
    using InitCanFn = quint32 (WINAPI*)(quint32, quint32, quint32, VciInitConfig*);
    using StartCanFn = quint32 (WINAPI*)(quint32, quint32, quint32);
    using ResetCanFn = quint32 (WINAPI*)(quint32, quint32, quint32);
    using TransmitFn = quint32 (WINAPI*)(quint32, quint32, quint32, VciCanObj*, quint32);
    using ReceiveFn = quint32 (WINAPI*)(quint32, quint32, quint32, VciCanObj*, quint32, int);
    using GetReceiveNumFn = quint32 (WINAPI*)(quint32, quint32, quint32);
    using FindUsbDevice2Fn = quint32 (WINAPI*)(VciBoardInfo*);

    ControlCanNative() = default;

    bool resolve(QString* error);
    static QString cleanFixedString(const char* text, qsizetype maxLength);

    QLibrary library_;
    bool loadAttempted_ = false;

    OpenDeviceFn openDevice_ = nullptr;
    CloseDeviceFn closeDevice_ = nullptr;
    InitCanFn initCan_ = nullptr;
    StartCanFn startCan_ = nullptr;
    ResetCanFn resetCan_ = nullptr;
    TransmitFn transmit_ = nullptr;
    ReceiveFn receive_ = nullptr;
    GetReceiveNumFn getReceiveNum_ = nullptr;
    FindUsbDevice2Fn findUsbDevice2_ = nullptr;
};
