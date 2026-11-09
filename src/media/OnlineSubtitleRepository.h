#pragma once

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <QPointer>
#include <QUrl>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;
class SearchSettings;

class OnlineSubtitleRepository final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)

public:
    explicit OnlineSubtitleRepository(SearchSettings *settings, QObject *parent = nullptr,
                                      const QUrl &apiBase = QUrl(QStringLiteral("https://api.opensubtitles.com/api/v1")),
                                      const QString &apiKey = {}, const QString &bearerToken = {});
    ~OnlineSubtitleRepository() override;

    bool enabled() const;
    bool configured() const { return !m_apiKey.isEmpty(); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QVariantList results() const { return m_results; }

    Q_INVOKABLE void search(const QString &query, const QString &language, const QString &type = QStringLiteral("movie"));
    Q_INVOKABLE void download(int fileId);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void subtitleDownloaded(const QString &title, const QString &language, const QString &srtText,
                            const QString &repository, int fileId);

private:
    QUrl endpoint(const QString &path, const QMap<QString, QString> &query = {}) const;
    QNetworkReply *request(const QUrl &url, const QByteArray &method, const QByteArray &body = {},
                           bool sendApiKey = true, bool sendBearerToken = true);
    void settingsChanged();
    void parseSearchReply(QNetworkReply *reply, quint64 generation);
    void parseDownloadLink(QNetworkReply *reply, quint64 generation, int fileId);
    void parseSubtitleReply(QNetworkReply *reply, quint64 generation, int fileId);
    void setError(const QString &message);

    SearchSettings *m_settings = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QUrl m_apiBase;
    QString m_apiKey;
    QString m_bearerToken;
    QString m_status = QStringLiteral("Online subtitle repository is off.");
    QString m_pendingTitle, m_pendingLanguage, m_pendingFileName;
    QVariantList m_results;
    QList<QPointer<QNetworkReply>> m_replies;
    quint64 m_generation = 0;
    bool m_busy = false;
};
