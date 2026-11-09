#pragma once

#include "AudioCaptureAdapter.h"
#include <QAudioInput>
#include <QAudioOutput>
#include <QMediaDevices>

class QtAudioCaptureAdapter final : public AudioCaptureAdapter {
    Q_OBJECT
public:
    explicit QtAudioCaptureAdapter(QObject *parent = nullptr);
    QVariantList devices() const override;
    bool monitoringSupported() const override;
    QString name() const override;
    bool configure(QMediaCaptureSession *session, const QString &id) override;
    void stop(QMediaCaptureSession *session) override;
private:
    QMediaDevices m_devices;
    QAudioInput m_input;
    QAudioOutput m_output;
};
