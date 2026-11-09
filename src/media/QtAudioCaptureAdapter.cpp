#include "QtAudioCaptureAdapter.h"

#include <QAudioDevice>
#include <QMediaCaptureSession>
#include <QVariantMap>
#include <QtGlobal>

QtAudioCaptureAdapter::QtAudioCaptureAdapter(QObject *parent) : AudioCaptureAdapter(parent)
{
    connect(&m_devices, &QMediaDevices::audioInputsChanged, this, &AudioCaptureAdapter::devicesChanged);
}

QVariantList QtAudioCaptureAdapter::devices() const
{
    QVariantList result;
    for (const auto &device : QMediaDevices::audioInputs())
        result.append(QVariantMap{{"id", QString::fromLatin1(device.id())}, {"name", device.description()}});
    return result;
}

bool QtAudioCaptureAdapter::monitoringSupported() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    return true;
#else
    return false;
#endif
}

QString QtAudioCaptureAdapter::name() const
{
    return QStringLiteral("Qt Multimedia capture-session routing");
}

bool QtAudioCaptureAdapter::configure(QObject *captureContext, const QString &id)
{
    if (id.isEmpty()) { stop(captureContext); return true; }
    auto *session = qobject_cast<QMediaCaptureSession *>(captureContext);
    if (!session) return false;
    stop(captureContext);
    for (const auto &device : QMediaDevices::audioInputs()) {
        if (QString::fromLatin1(device.id()) != id) continue;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        m_input.setDevice(device);
        m_output.setDevice(QMediaDevices::defaultAudioOutput());
        session->setAudioInput(&m_input);
        session->setAudioOutput(&m_output);
#endif
        return true;
    }
    return false;
}

void QtAudioCaptureAdapter::stop(QObject *captureContext)
{
    auto *session = qobject_cast<QMediaCaptureSession *>(captureContext);
    if (!session) return;
    session->setAudioInput(nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    session->setAudioOutput(nullptr);
#endif
}
