#include "SerialCommandWriter.h"

#include <QSerialPort>

SerialCommandWriter::SerialCommandWriter(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        fail("The serial switch did not accept the command in time.");
    });
}

qint64 SerialCommandWriter::elapsedMs() const
{
    return m_clock.isValid() ? m_clock.elapsed() : -1;
}

bool SerialCommandWriter::start(QIODevice *device, const QByteArray &payload, int timeoutMs)
{
    if (isSending() || !device || !device->isOpen() || !device->isWritable()
        || payload.isEmpty() || timeoutMs <= 0)
        return false;
    disconnectDevice();
    m_device = device;
    m_pendingBytes = payload.size();
    m_writeAccepted = false;
    m_state = QStringLiteral("sending");
    m_clock.start();
    m_bytesConnection = connect(device, &QIODevice::bytesWritten, this, [this](qint64 count) {
        if (!isSending() || count <= 0) return;
        m_pendingBytes -= count;
        completeIfWritten();
    });
    m_destroyedConnection = connect(device, &QObject::destroyed, this, [this] {
        if (isSending()) fail("The serial device disconnected before the command was fully written.");
    });
    m_closedConnection = connect(device, &QIODevice::aboutToClose, this, [this] {
        if (isSending()) fail("The serial device closed before the command was fully written.");
    });
    m_timeout.start(timeoutMs);
    emit stateChanged();
    if (!isSending()) return false;
    const auto written = device->write(payload);
    if (!isSending()) return false;
    if (written != payload.size()) {
        fail("Could not queue the complete serial switch command.");
        return false;
    }
    m_writeAccepted = true;
    completeIfWritten();
    return true;
}

void SerialCommandWriter::cancel()
{
    if (!isSending()) return;
    QPointer<QSerialPort> port = qobject_cast<QSerialPort *>(m_device.data());
    finish("cancelled");
    if (port) port->clear(QSerialPort::Output);
}

void SerialCommandWriter::fail(const QString &reason)
{
    if (!isSending()) return;
    finish("error");
    emit failed(reason);
}

void SerialCommandWriter::finish(const QString &state)
{
    m_timeout.stop();
    m_pendingBytes = 0;
    m_writeAccepted = false;
    m_state = state;
    disconnectDevice();
    emit stateChanged();
}

void SerialCommandWriter::completeIfWritten()
{
    if (!isSending() || !m_writeAccepted || m_pendingBytes > 0) return;
    const auto elapsed = m_clock.elapsed();
    finish("sent");
    emit completed(elapsed);
}

void SerialCommandWriter::disconnectDevice()
{
    if (m_bytesConnection) disconnect(m_bytesConnection);
    if (m_destroyedConnection) disconnect(m_destroyedConnection);
    if (m_closedConnection) disconnect(m_closedConnection);
    m_bytesConnection = {};
    m_destroyedConnection = {};
    m_closedConnection = {};
    m_device = nullptr;
}
