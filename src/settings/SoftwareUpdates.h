#pragma once

#include <QObject>
#include <QUrl>
#include <QVersionNumber>
#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class SearchSettings;

// Discovers candidate releases without granting unsigned metadata installation authority.
class SoftwareUpdates final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

  public:
    explicit SoftwareUpdates(SearchSettings* settings, const QString& installedVersion,
                             QObject* parent = nullptr, QNetworkAccessManager* network = nullptr,
                             const QUrl& endpoint = QUrl(QStringLiteral("https://api.github.com/repos/ktetch7707/Isolde/releases/latest")),
                             const QString& stateFile = {}, const QString& platform = {});

    bool available() const;
    bool checking() const;
    QString latestVersion() const;
    QString status() const;

    Q_INVOKABLE void check();
    Q_INVOKABLE void dismiss();

  signals:
    void changed();

  private:
    void webChanged();
    void finished();
    void scheduleNext();
    void fail(const QString& message);

    SearchSettings* m_settings;
    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply = nullptr;
    QTimer* m_timer;
    QUrl m_endpoint;
    QString m_stateFile;
    QString m_platform;
    QString m_latest;
    QString m_dismissed;
    QString m_status;
    QVersionNumber m_installed;
    QDateTime m_retryAfter;
    bool m_webEnabled = false;
};
