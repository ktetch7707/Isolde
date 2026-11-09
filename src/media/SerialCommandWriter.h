#pragma once

#include <QIODevice>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QByteArray>

class SerialCommandWriter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY stateChanged)
public:
    explicit SerialCommandWriter(QObject *parent = nullptr);

    QString state() const { return m_state; }
    qint64 elapsedMs() const;
    qint64 pendingBytes() const { return m_pendingBytes; }
    Q_INVOKABLE bool isSending() const { return m_state == "sending"; }
    bool start(QIODevice *device, const QByteArray &payload, int timeoutMs = 2500);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void fail(const QString &reason);

signals:
    void stateChanged();
    void completed(qint64 elapsedMs);
    void failed(const QString &reason);

private:
    void finish(const QString &state);
    void disconnectDevice();
    void completeIfWritten();

    QPointer<QIODevice> m_device;
    QMetaObject::Connection m_bytesConnection;
    QMetaObject::Connection m_destroyedConnection;
    QMetaObject::Connection m_closedConnection;
    QTimer m_timeout;
    QElapsedTimer m_clock;
    qint64 m_pendingBytes = 0;
    bool m_writeAccepted = false;
    QString m_state = QStringLiteral("idle");
};
