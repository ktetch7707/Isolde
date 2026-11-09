#pragma once

#include <QObject>
#include <QVariantList>

class QMediaCaptureSession;

class AudioCaptureAdapter : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QVariantList devices() const = 0;
    virtual bool monitoringSupported() const = 0;
    virtual QString name() const = 0;
    virtual bool configure(QMediaCaptureSession *session, const QString &id) = 0;
    virtual void stop(QMediaCaptureSession *session) = 0;
signals:
    void devicesChanged();
    void errorOccurred(const QString &message);
};
