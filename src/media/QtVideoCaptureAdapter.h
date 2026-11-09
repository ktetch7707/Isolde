#pragma once

#include "VideoCaptureAdapter.h"
#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>

class QtVideoCaptureAdapter final : public VideoCaptureAdapter {
    Q_OBJECT
public:
    explicit QtVideoCaptureAdapter(QObject *parent = nullptr);
    QVariantList devices() const override;
    QVariantList formats(const QString &deviceId) const override;
    QString name() const override;
    void setOutput(QObject *output) override;
    bool start(const QString &deviceId, const QString &formatKey) override;
    void stop() override;
    QObject *audioContext() override { return &m_session; }
private:
    QMediaDevices m_devices;
    QMediaCaptureSession m_session;
    QCamera m_camera;
    QMetaObject::Connection m_frameConnection;
};
