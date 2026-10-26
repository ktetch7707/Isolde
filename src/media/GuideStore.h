#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <functional>

// Profile-local presentation/schedule data only. No media scan, network,
// playback, account secrets or writes to Kodi. All times are UTC seconds.
class GuideStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
public:
    explicit GuideStore(const QString &path, QObject *parent=nullptr);
    ~GuideStore() override;
    bool ready() const { return m_ready; }
    QString errorString() const { return m_error; }
    QVariantList channels() const;
    Q_INVOKABLE QVariantMap channel(const QString &id) const;
    Q_INVOKABLE QVariantList programs(const QString &id, qint64 fromUtc, qint64 toUtc, int limit=200) const;
    Q_INVOKABLE QVariantMap tuneTarget(const QString &id, qint64 nowUtc) const;
    // Candidates are bounded Kodi metadata snapshots; they are never scanned
    // from the filesystem or hydrated independently of Kodi's catalog.
    bool createLocal(const QVariantMap &definition, const QVariantList &candidates, qint64 nowUtc, int horizonDays=7);
    bool extend(const QString &id, qint64 untilUtc, int expectedRevision);
    bool updateRules(const QVariantMap &definition, const QVariantList &candidates, int expectedRevision);
    bool setActive(bool active, qint64 nowUtc);
    bool setPreferences(const QString &id, const QVariantMap &preferences, int expectedRevision);
    bool setMetadata(const QString &id,const QVariantMap &metadata,int expectedRevision);
    bool reorder(const QStringList &ids, const QVariantMap &expectedRevisions);
    bool resumeClocks(qint64 nowUtc);
    qint64 broadcastTime(const QString &id, qint64 nowUtc) const;
    void setCancellationCheck(std::function<bool()> check) { m_cancelled=std::move(check); }
    bool remove(const QString &id, int expectedRevision);
    // Only affected slots change; an unavailable slate retains the interval.
    bool markUnavailable(const QString &kind, int mediaId);
    void invalidate();
    // Deterministic failure injection, scoped to this private store instance.
    void failNextCommitForTest() { m_failCommit=true; }
signals:
    void changed();
    void errorChanged();
private:
    bool fail(const QString &message);
    bool commit();
    bool appendSlots(const QString &id, qint64 untilUtc);
    QVariantList eligible(const QVariantMap &definition, const QVariantList &candidates, QString *error) const;
    qint64 clockTime(const QVariantMap &channel, qint64 nowUtc) const;
    QSqlDatabase m_db;
    QString m_connection, m_error;
    bool m_ready=false, m_failCommit=false;
    std::function<bool()> m_cancelled;
};
