#pragma once

#include <QObject>
#include <QMap>
#include <QPointer>
#include <QUrl>
#include <QVariantList>
#include <QList>

class QNetworkAccessManager;
class QNetworkReply;
class SearchSettings;

class OnlineDiscovery final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString region READ region WRITE setRegion NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QVariantList availability READ availability NOTIFY changed)

public:
    explicit OnlineDiscovery(SearchSettings *settings, QObject *parent = nullptr,
                             const QUrl &apiBase = QUrl(QStringLiteral("https://api.themoviedb.org/3")),
                             const QString &readToken = {});
    ~OnlineDiscovery() override;

    bool enabled() const;
    bool configured() const { return !m_readToken.isEmpty(); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString region() const { return m_region; }
    QVariantList results() const { return m_results; }
    QVariantList availability() const { return m_availability; }

    Q_INVOKABLE void setRegion(const QString &region);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void loadAvailability(const QString &mediaType, int id);
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    QUrl endpoint(const QString &path, const QMap<QString, QString> &query = {}) const;
    QNetworkReply *get(const QUrl &url);
    void settingsChanged();
    void setError(const QString &message);
    void parseSearchReply(QNetworkReply *reply, quint64 generation);
    void parseAvailabilityReply(QNetworkReply *reply, quint64 generation, QString region);

    SearchSettings *m_settings = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QUrl m_apiBase;
    QString m_readToken, m_region = QStringLiteral("US"), m_status = QStringLiteral("Web discovery is off.");
    QVariantList m_results, m_availability;
    QList<QPointer<QNetworkReply>> m_replies;
    quint64 m_generation = 0;
    bool m_busy = false;
};
