#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>

class KodiArtwork final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit KodiArtwork(const QString &profile, QObject *parent = nullptr, const QString &testDirectory = {});
    ~KodiArtwork() override;
    QString source() const { return m_source; }
    QString status() const { return m_status; }
    Q_INVOKABLE void load(const QString &kind, int id);
    Q_INVOKABLE void clear();
signals:
    void changed();
private:
    void begin();
    void finish(const QString &status);
    void cancel();
    QLocalSocket m_socket;
    QTimer m_debounce, m_timeout;
    QString m_directory, m_kind, m_source, m_status;
    int m_item = -1, m_requestId = 0, m_descriptor = -1;
    bool m_active = false;
    QByteArray m_received, m_request;
};
