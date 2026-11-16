#pragma once

#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include "VideoCaptureAdapter.h"

class QtVideoCaptureAdapter final : public VideoCaptureAdapter {
    Q_OBJECT
  public:
    explicit QtVideoCaptureAdapter(QObject* parent = nullptr);

  private:
    QMediaDevices m_devices;
    QMediaCaptureSession m_session;
    QCamera m_camera;
    QMetaObject::Connection m_frameConnection;
};
