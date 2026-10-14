#include "control_can_native.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QSet>
#include <QStringList>

static_assert(sizeof(ControlCanNative::VciCanObj) == 24);
static_assert(sizeof(ControlCanNative::VciInitConfig) == 16);
static_assert(sizeof(ControlCanNative::VciBoardInfo) == 80);
static_assert(sizeof(ControlCanNative::VciErrInfo) == 8);

ControlCanNative& ControlCanNative::instance()
{
    static ControlCanNative native;
    return native;
}

bool ControlCanNative::ensureLoaded(QString* error)
{
    if (functionsResolved()) {
        return true;
    }

    loadError_.clear();
    clearFunctions();

    const auto env = QProcessEnvironment::systemEnvironment();
    const QString configuredPath = env.value("CONTROLCAN_DLL_PATH");
    const QString appDir = QCoreApplication::applicationDirPath();

    QStringList candidates;
    QSet<QString> seenCandidates;
    const auto addCandidate = [&candidates, &seenCandidates](const QString& path) {
        if (path.trimmed().isEmpty()) {
            return;
        }

        const QString normalized = QDir::isAbsolutePath(path) ? QDir::cleanPath(path) : path;
        const QString key = normalized.toCaseFolded();
        if (seenCandidates.contains(key)) {
            return;
        }

        seenCandidates.insert(key);
        candidates << normalized;
    };

    if (!configuredPath.isEmpty()) {
        addCandidate(configuredPath);
    }
    addCandidate(QDir(appDir).filePath("ControlCAN.dll"));
    addCandidate(QDir(appDir).filePath("../ControlCAN.dll"));
    addCandidate(QDir(appDir).filePath("../../ControlCAN.dll"));
    addCandidate(QDir::current().filePath("ControlCAN.dll"));
    addCandidate("ControlCAN.dll");

    QStringList attempts;

    for (const QString& candidate : candidates) {
        clearFunctions();
        library_.setFileName(candidate);
        if (!library_.load()) {
            attempts << QString("%1: %2").arg(candidate, library_.errorString());
            continue;
        }

        QString resolveError;
        if (resolve(&resolveError)) {
            loadError_.clear();
            return true;
        }

        attempts << QString("%1: %2").arg(candidate, resolveError);
        library_.unload();
        clearFunctions();
    }

    loadError_ = QString("ControlCAN.dll could not be loaded. Copy it beside the executable or set CONTROLCAN_DLL_PATH. Tried: %1")
                     .arg(attempts.join("; "));
    if (error) {
        *error = loadError_;
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

bool ControlCanNative::readErrInfo(quint32 deviceType, quint32 deviceIndex, quint32 canIndex, VciErrInfo* errorInfo)
{
    if (!readErrInfo_) {
        return false;
    }
    return readErrInfo_(deviceType, deviceIndex, canIndex, errorInfo) == StatusOk;
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
    readErrInfo_ = reinterpret_cast<ReadErrInfoFn>(library_.resolve("VCI_ReadErrInfo"));
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

bool ControlCanNative::functionsResolved() const
{
    return openDevice_ && closeDevice_ && initCan_ && startCan_ && resetCan_ &&
           transmit_ && receive_ && getReceiveNum_ && findUsbDevice2_;
}

void ControlCanNative::clearFunctions()
{
    openDevice_ = nullptr;
    closeDevice_ = nullptr;
    initCan_ = nullptr;
    startCan_ = nullptr;
    resetCan_ = nullptr;
    transmit_ = nullptr;
    receive_ = nullptr;
    getReceiveNum_ = nullptr;
    readErrInfo_ = nullptr;
    findUsbDevice2_ = nullptr;
}

QString ControlCanNative::cleanFixedString(const char* text, qsizetype maxLength)
{
    const QByteArray bytes(text, maxLength);
    return QString::fromLocal8Bit(bytes.constData()).remove(QChar('\0')).trimmed();
}
