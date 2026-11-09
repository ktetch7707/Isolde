#pragma once

#include <QObject>
#include <QVariantList>

class QMediaCaptureSession;

class VideoCaptureAdapter : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QVariantList devices() const = 0;
    virtual QVariantList formats(const QString &deviceId) const = 0;
    virtual QString name() const = 0;
    virtual void setOutput(QObject *output) = 0;
    virtual bool start(const QString &deviceId, const QString &formatKey) = 0;
    virtual void stop() = 0;
    virtual QMediaCaptureSession *audioSession() = 0;
signals:
    void devicesChanged();
    void frameReceived();
    void errorOccurred(const QString &message);
};
