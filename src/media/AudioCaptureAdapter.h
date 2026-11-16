#pragma once

#include <QObject>
#include <QVariantList>

class AudioCaptureDevices {
  public:
    virtual ~AudioCaptureDevices() = default;
    virtual QVariantList devices() const = 0;
    virtual bool monitoringSupported() const = 0;
};

class AudioCaptureControl {
  public:
    virtual ~AudioCaptureControl() = default;
    virtual QString name() const = 0;
    virtual bool configure(QObject* captureContext, const QString& id) = 0;
    virtual void stop(QObject* captureContext) = 0;
};

class AudioCaptureAdapter : public QObject, public AudioCaptureDevices, public AudioCaptureControl {
    Q_OBJECT
  public:
    using QObject::QObject;
    ~AudioCaptureAdapter() override = default;

    QVariantList devices() const override = 0;
    bool monitoringSupported() const override = 0;
    QString name() const override = 0;
    bool configure(QObject* captureContext, const QString& id) override = 0;
    void stop(QObject* captureContext) override = 0;

  signals:
    void devicesChanged();
    void errorOccurred(const QString& message);
};
