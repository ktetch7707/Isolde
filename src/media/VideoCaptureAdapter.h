#pragma once

#include <QObject>
#include <QVariantList>

class VideoCaptureDevices
{
  public:
    virtual ~VideoCaptureDevices() = default;
    virtual QVariantList
    devices() const = 0;
    virtual QVariantList
    formats(const QString& deviceId) const = 0;
};

class VideoCaptureControl
{
  public:
    virtual ~VideoCaptureControl() = default;
    virtual QString
    name() const = 0;
    virtual void
    setOutput(QObject* output) = 0;
    virtual bool
    start(const QString& deviceId, const QString& formatKey) = 0;
    virtual void
    stop() = 0;
};

class VideoCaptureAudio
{
  public:
    virtual ~VideoCaptureAudio() = default;
    virtual QObject*
    audioContext() = 0;
};

class VideoCaptureAdapter : public QObject,
                            public VideoCaptureDevices,
                            public VideoCaptureControl,
                            public VideoCaptureAudio
{
    Q_OBJECT
  public:
    using QObject::QObject;
    ~VideoCaptureAdapter() override = default;

  signals:
    void
    devicesChanged();
    void
    frameReceived();
    void
    errorOccurred(const QString& message);
};
