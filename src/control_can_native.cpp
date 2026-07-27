#include "control_can_native.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QStringList>

static_assert(sizeof(ControlCanNative::VciCanObj) == 24);
static_assert(sizeof(ControlCanNative::VciInitConfig) == 16);
static_assert(sizeof(ControlCanNative::VciBoardInfo) == 80);

ControlCanNative& ControlCanNative::instance()
{
    static ControlCanNative native;
    return native;
}

bool ControlCanNative::ensureLoaded(QString* error)
{
    if (findUsbDevice2_) {
        return true;
    }

    if (loadAttempted_ && !library_.isLoaded()) {
        if (error) {
            *error = library_.errorString();
        }
        return false;
    }

    loadAttempted_ = true;

    const auto env = QProcessEnvironment::systemEnvironment();
    const QString configuredPath = env.value("CONTROLCAN_DLL_PATH");
    const QString appDir = QCoreApplication::applicationDirPath();

    QStringList candidates;
    if (!configuredPath.isEmpty()) {
        candidates << configuredPath;
    }
    candidates << QDir(appDir).filePath("ControlCAN.dll")
               << QDir::current().filePath("ControlCAN.dll")
               << "ControlCAN.dll";

    for (const QString& candidate : candidates) {
        library_.setFileName(candidate);
        if (library_.load() && resolve(error)) {
            return true;
        }
        library_.unload();
    }

    if (error) {
        *error = "ControlCAN.dll could not be loaded. Copy it beside the executable or set CONTROLCAN_DLL_PATH.";
    }
    return false;
}

QVector<CanDeviceInfo> ControlCanNative::findDevices(QString* error)
{
    QVector<CanDeviceInfo> devices;
    if (!ensureLoaded(error)) {
        return devices;
    }

    VciBoardInfo raw[MaxFindDevices] = {};
    const quint32 count = qMin(findUsbDevice2_(raw), static_cast<quint32>(MaxFindDevices));
    devices.reserve(static_cast<int>(count));

    for (quint32 i = 0; i < count; ++i) {
        CanDeviceInfo device;
        device.deviceIndex = i;
        device.serialNumber = cleanFixedString(raw[i].serialNumber, sizeof(raw[i].serialNumber));
        device.hardwareType = cleanFixedString(raw[i].hardwareType, sizeof(raw[i].hardwareType));
        device.channelCount = raw[i].canNum;
        if (device.serialNumber.trimmed().isEmpty()) {
            device.serialNumber = "Unknown";
        }
        if (device.hardwareType.trimmed().isEmpty()) {
            device.hardwareType = "Unknown";
        }
        devices.push_back(device);
    }

    return devices;
}

quint32 ControlCanNative::openDevice(quint32 deviceType, quint32 deviceIndex, quint32 reserved)
{
    return openDevice_(deviceType, deviceIndex, reserved);
}

quint32 ControlCanNative::closeDevice(quint32 deviceType, quint32 deviceIndex)
{
    return closeDevice_(deviceType, deviceIndex);
}

quint32 ControlCanNative::initCan(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciInitConfig* config)
{
    return initCan_(deviceType, deviceIndex, canIndex, config);
}

quint32 ControlCanNative::startCan(quint32 deviceType, quint32 deviceIndex, quint32 canIndex)
{
    return startCan_(deviceType, deviceIndex, canIndex);
}

quint32 ControlCanNative::resetCan(quint32 deviceType, quint32 deviceIndex, quint32 canIndex)
{
    return resetCan_(deviceType, deviceIndex, canIndex);
}

quint32 ControlCanNative::transmit(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciCanObj* frames, quint32 length)
{
    return transmit_(deviceType, deviceIndex, canIndex, frames, length);
}

quint32 ControlCanNative::receive(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciCanObj* frames, quint32 length, int waitTimeMs)
{
    return receive_(deviceType, deviceIndex, canIndex, frames, length, waitTimeMs);
}

quint32 ControlCanNative::getReceiveNum(quint32 deviceType, quint32 deviceIndex, quint32 canIndex)
{
    return getReceiveNum_(deviceType, deviceIndex, canIndex);
}

bool ControlCanNative::resolve(QString* error)
{
    openDevice_ = reinterpret_cast<OpenDeviceFn>(library_.resolve("VCI_OpenDevice"));
    closeDevice_ = reinterpret_cast<CloseDeviceFn>(library_.resolve("VCI_CloseDevice"));
    initCan_ = reinterpret_cast<InitCanFn>(library_.resolve("VCI_InitCAN"));
    startCan_ = reinterpret_cast<StartCanFn>(library_.resolve("VCI_StartCAN"));
    resetCan_ = reinterpret_cast<ResetCanFn>(library_.resolve("VCI_ResetCAN"));
    transmit_ = reinterpret_cast<TransmitFn>(library_.resolve("VCI_Transmit"));
    receive_ = reinterpret_cast<ReceiveFn>(library_.resolve("VCI_Receive"));
    getReceiveNum_ = reinterpret_cast<GetReceiveNumFn>(library_.resolve("VCI_GetReceiveNum"));
    findUsbDevice2_ = reinterpret_cast<FindUsbDevice2Fn>(library_.resolve("VCI_FindUsbDevice2"));

    if (openDevice_ && closeDevice_ && initCan_ && startCan_ && resetCan_ &&
        transmit_ && receive_ && getReceiveNum_ && findUsbDevice2_) {
        return true;
    }

    if (error) {
        *error = "ControlCAN.dll loaded, but one or more VCI_* exports were not found.";
    }
    return false;
}

QString ControlCanNative::cleanFixedString(const char* text, qsizetype maxLength)
{
    const QByteArray bytes(text, maxLength);
    return QString::fromLocal8Bit(bytes.constData()).remove(QChar('\0')).trimmed();
}
