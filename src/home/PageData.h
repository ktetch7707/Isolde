#pragma once
#include <QObject>
#include <QHash>
#include <QVariantMap>
#include <QPointer>
#include <QDateTime>
class KodiMedia;
class KodiArtwork;
class AppRegistry;
class SessionManager;
class ActionDispatcher;
class PageData final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    PageData(const QString &profile,KodiMedia *player,QObject *parent=nullptr,const QString &directory={});
    Q_INVOKABLE QObject *client(const QString &key,const QVariantMap &source);
    Q_INVOKABLE QObject *playlistCatalog();
    Q_INVOKABLE QObject *artwork(const QString &kind,int id);
    Q_INVOKABLE void activate(QObject *client,int index,bool resume=true,const QString &originId={});
    Q_INVOKABLE void refresh(QObject *client);
    int revision() const { return m_revision; }
    void invalidate(const QString &reason);
    void unavailable(const QString &reason);
    void setApplications(AppRegistry *registry, SessionManager *sessions, ActionDispatcher *dispatcher);
signals:
    void changed();
private:
    QString m_profile,m_directory;
    KodiMedia *m_player;
    int m_revision=0;
    bool m_valid=true;
    QPointer<AppRegistry> m_registry;
    QPointer<SessionManager> m_sessions;
    QPointer<ActionDispatcher> m_actions;
    QHash<QString,QDateTime> m_openings;
    QString m_pendingOpening;
    QDateTime m_pendingOpeningTime;
    QVariantList appOpenings(const QVariantMap &source) const;
    QString historyNotice(const QVariantMap &source) const;
    void updateHistoryClients();
    struct Entry { KodiMedia *client; QVariantMap source; };
    QHash<QString,Entry> m_clients;
    QHash<QString,KodiArtwork *> m_artwork;
};
