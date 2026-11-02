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

// Per-profile read-only media/artwork handle cache for Home pages.
// PageData owns no media: Kodi owns playback and progress; Isolde only binds
// a page component to a KodiMedia client and its artwork cache. All caches
// are bounded (ComponentSchema.h) and invalidated when Kodi becomes unavailable.
class PageData final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    // @param profile active profile id (isolates caches from other profiles)
    // @param player playback client used for activate(); not owned here
    // @param directory profile-specific directory for artwork/logo storage
    PageData(const QString &profile, KodiMedia *player, QObject *parent = nullptr, const QString &directory = {});
    // Returns a bounded, cached KodiMedia client for source, creating it on
    // first use. Returns nullptr when invalidated, when key/target fail
    // validation, or when cache bounds would be exceeded.
    Q_INVOKABLE QObject *client(const QString &key, const QVariantMap &source);
    Q_INVOKABLE QObject *playlistCatalog();
    // Returns a cached KodiArtwork loader for media item id of kind. Returns
    // nullptr for unknown kinds, non-positive ids, or when the artwork cache
    // bound would be exceeded.
    Q_INVOKABLE QObject *artwork(const QString &kind, int id);
    // Activates the item at index on client (tvshow/season deep-link, other
    // kinds resume playback on m_player). No-op when invalid, unowned, busy
    // or out of range.
    Q_INVOKABLE void activate(QObject *client, int index, bool resume = true, const QString &originId = {});
    // Refreshes the artwork cache and the client matching object; bumps
    // revision so QML clients reload.
    Q_INVOKABLE void refresh(QObject *client);
    int revision() const { return m_revision; }
    // Marks all clients and artwork invalid (e.g. Kodi unavailable) with the
    // given human-readable reason.
    void invalidate(const QString &reason);
    void unavailable(const QString &reason);
    void setApplications(AppRegistry *registry, SessionManager *sessions, ActionDispatcher *dispatcher);
signals:
    void changed();
private:
    QString m_profile, m_directory;
    KodiMedia *m_player;
    int m_revision = 0;
    bool m_valid = true;
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
    QHash<QString, Entry> m_clients;
    QHash<QString, KodiArtwork *> m_artwork;
};
