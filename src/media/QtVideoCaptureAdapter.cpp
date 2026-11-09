#include "QtVideoCaptureAdapter.h"

#include <QCameraDevice>
#include <QCameraFormat>
#include <QVariantMap>
#include <QVideoFrame>
#include <QVideoSink>

namespace {
QString formatKey(const QCameraFormat &format)
{
    const auto size = format.resolution();
    return QString("%1x%2@%3-%4#%5").arg(size.width()).arg(size.height())
        .arg(format.minFrameRate(), 0, 'f', 2).arg(format.maxFrameRate(), 0, 'f', 2)
        .arg(static_cast<int>(format.pixelFormat()));
}
}

QtVideoCaptureAdapter::QtVideoCaptureAdapter(QObject *parent) : VideoCaptureAdapter(parent)
{
    connect(&m_devices, &QMediaDevices::videoInputsChanged, this, &VideoCaptureAdapter::devicesChanged);
    connect(&m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        emit errorOccurred(message);
    });
}

QVariantList QtVideoCaptureAdapter::devices() const
{
    QVariantList result;
    for (const auto &device : QMediaDevices::videoInputs()) {
        QStringList formats;
        for (const auto &format : device.videoFormats()) {
            const auto size = format.resolution();
            formats.append(QString("%1 × %2, %3–%4 fps").arg(size.width()).arg(size.height())
                .arg(format.minFrameRate(), 0, 'f', 0).arg(format.maxFrameRate(), 0, 'f', 0));
        }
        formats.removeDuplicates();
        result.append(QVariantMap{{"id", QString::fromLatin1(device.id())}, {"name", device.description()},
            {"formats", formats.mid(0, 8).join(" · ")}});
    }
    return result;
}

QVariantList QtVideoCaptureAdapter::formats(const QString &deviceId) const
{
    QVariantList result;
    for (const auto &device : QMediaDevices::videoInputs()) {
        if (QString::fromLatin1(device.id()) != deviceId) continue;
        for (const auto &format : device.videoFormats()) {
            const auto size = format.resolution();
            const auto key = formatKey(format);
            const auto label = QString("%1 × %2 · %3–%4 fps · format %5")
                .arg(size.width()).arg(size.height()).arg(format.minFrameRate(), 0, 'f', 0)
                .arg(format.maxFrameRate(), 0, 'f', 0).arg(static_cast<int>(format.pixelFormat()));
            bool duplicate = false;
            for (const auto &entry : result) duplicate |= entry.toMap().value("key").toString() == key;
            if (!duplicate) result.append(QVariantMap{{"key", key}, {"name", label}});
        }
        break;
    }
    return result;
}

QString QtVideoCaptureAdapter::name() const { return QStringLiteral("Qt Multimedia"); }

void QtVideoCaptureAdapter::setOutput(QObject *output)
{
    if (m_frameConnection) disconnect(m_frameConnection);
    m_session.setVideoOutput(output);
    if (auto *sink = m_session.videoSink())
        m_frameConnection = connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (frame.isValid()) emit frameReceived();
        });
}

bool QtVideoCaptureAdapter::start(const QString &deviceId, const QString &formatKeyValue)
{
    for (const auto &device : QMediaDevices::videoInputs()) {
        if (QString::fromLatin1(device.id()) != deviceId) continue;
        QCameraFormat selectedFormat;
        bool found = formatKeyValue.isEmpty();
        for (const auto &format : device.videoFormats()) {
            if (formatKey(format) == formatKeyValue) { selectedFormat = format; found = true; break; }
        }
        if (!found) return false;
        m_camera.setCameraDevice(device);
        m_camera.setCameraFormat(selectedFormat);
        m_session.setCamera(&m_camera);
        m_camera.start();
        return true;
    }
    return false;
}

void QtVideoCaptureAdapter::stop()
{
    if (m_camera.isActive()) m_camera.stop();
    m_session.setCamera(nullptr);
}
