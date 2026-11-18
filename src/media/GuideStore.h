#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <functional>

// Profile-local presentation/schedule data only. No media scan, network,
// playback, account secrets or writes to Kodi. All times are UTC seconds.
class GuideStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
  public:
    explicit GuideStore(const QString& path, QObject* parent = nullptr);
    ~GuideStore() override;
    bool
    ready() const
    {
        return m_ready;
    }
    QString
    errorString() const
    {
        return m_error;
    }
    QVariantList
    channels() const;
    Q_INVOKABLE QVariantMap
    channel(const QString& id) const;
    Q_INVOKABLE QVariantList
    programs(const QString& id, qint64 fromUtc, qint64 toUtc, int limit = 200) const;
    Q_INVOKABLE QVariantMap
    tuneTarget(const QString& id, qint64 nowUtc) const;
    // Candidates are bounded Kodi metadata snapshots; they are never scanned
    // from the filesystem or hydrated independently of Kodi's catalog.
    // -1 selects the validated channel policy, or its legacy seven-day default.
    bool
    createLocal(const QVariantMap& definition,
                const QVariantList& candidates,
                qint64 nowUtc,
                int horizonDays = -1,
                const QVariantMap& snapshot = {},
                const QVariantMap& remapping = {});
    QVariantMap
    portableChannel(const QString& id, bool includeSchedule);
    static QVariantMap
    portableDefinition(const QVariantMap& package, QString* error = nullptr);
    bool
    extend(const QString& id, qint64 untilUtc, int expectedRevision);
    // Worker-only rolling maintenance. Retains committed future slots, prunes
    // bounded expired history and skips unscheduled outage history by cursor.
    // -1 uses channel policy; legacy definitions retain seven/two day defaults.
    bool
    maintain(const QString& id, qint64 nowUtc, int expectedRevision, int horizonDays = -1, int retentionDays = -1);
    bool
    importPvr(const QString& instance,
              const QVariantList& channels,
              qint64 refreshedUtc,
              const QVariantMap& expectedRevisions);
    bool
    setSourceEnabled(const QString& instance, bool enabled, const QVariantMap& expectedRevisions);
    QVariantMap
    diagnostics() const;
    bool
    updateRules(const QVariantMap& definition,
                const QVariantList& candidates,
                int expectedRevision,
                qint64 nowUtc = -1);
    bool
    updateRulesStateValid(
        const QVariantMap& state, const QVariantMap& definition, int expectedRevision, qint64 wall, bool& clockChanged);
    QVariantMap
    definition(const QString& id) const;
    QVariantList
    candidateSnapshot(const QString& id) const;
    QVariantMap
    regenerationPreview(const QString& id, qint64 nowUtc) const;
    bool
    regenerate(const QString& id, const QVariantMap& review, qint64 nowUtc);
    QVariantMap
    repairPreview(const QString& id,
                  const QString& slotId,
                  int expectedRevision,
                  qint64 nowUtc,
                  int first = 0,
                  bool fitInterval = false) const;
    bool
    repairPreviewLoad(const QString& id,
                      const QString& slotId,
                      int revision,
                      qint64 wall,
                      int first,
                      QSqlQuery& q,
                      qint64& start,
                      qint64& end,
                      QVariantMap& program,
                      qint64& delta) const;
    bool
    slotRowValid(const QSqlQuery& q) const;
    void
    repairPreviewAlternatives(const QString& id,
                              qint64 start,
                              qint64 end,
                              int first,
                              bool fitInterval,
                              QVariantList& choices,
                              int& total,
                              int& incompatible) const;
    bool
    repairSlot(const QString& id,
               const QString& slotId,
               const QString& mediaType,
               int mediaId,
               int expectedRevision,
               bool fitInterval = false);
    bool
    repairSlotLoad(
        const QString& id, const QString& slotId, int revision, qint64& start, qint64& end, qint64& duration);
    QVariantMap
    findReplacement(const QString& id, const QString& kind, int mediaId, qint64 duration, bool fitInterval);
    bool
    repairSlotReplace(const QString& id,
                      const QString& slotId,
                      int revision,
                      qint64 start,
                      qint64 end,
                      qint64 duration,
                      bool unavailable,
                      const QString& kind,
                      int mediaId,
                      const QVariantMap& replacement,
                      QSqlQuery& q);
    bool
    setActive(bool active, qint64 nowUtc);
    bool
    setPreferences(const QString& id, const QVariantMap& preferences, int expectedRevision);
    bool
    setMetadata(const QString& id, const QVariantMap& metadata, int expectedRevision);
    bool
    setMetadataUpdate(const QString& id, const QVariantMap& metadata, int expectedRevision, QSqlQuery& q);
    bool
    swapNumbers(const QString& id, const QString& otherId, const QVariantMap& expectedRevisions);
    bool
    swapNumbersArgsValid(const QString& id, const QString& otherId, const QVariantMap& revisions);
    bool
    swapNumbersValidateRevisions(const QVariantMap& a, const QVariantMap& b, const QVariantMap& revisions);
    int
    swapNumbersTemporary(const QSet<int>& used);
    bool
    reorder(const QStringList& ids, const QVariantMap& expectedRevisions);
    bool
    resumeClocks(qint64 nowUtc);
    qint64
    broadcastTime(const QString& id, qint64 nowUtc) const;
    QVariantMap
    clockAdjustmentPreview(const QString& id, qint64 deltaSeconds, int expectedRevision, qint64 nowUtc) const;
    bool
    adjustClock(const QVariantMap& review, qint64 nowUtc);
    bool
    clockAdjustmentPreviewArgsValid(const QString& id, qint64 deltaSeconds, int expectedRevision, qint64 nowUtc) const;
    bool
    adjustClockArgsValid(const QVariantMap& review, qint64 nowUtc);
    void
    setCancellationCheck(std::function<bool()> check)
    {
        m_cancelled = std::move(check);
    }
    bool
    cancellationRequested() const
    {
        return m_cancelled && m_cancelled();
    }
    bool
    remove(const QString& id, int expectedRevision);
    // Only affected slots change; an unavailable slate retains the interval.
    bool
    markUnavailable(const QString& kind, int mediaId);
    void
    invalidate();
    // Deterministic failure injection, scoped to this private store instance.
    void
    failNextCommitForTest()
    {
        m_failCommit = true;
    }
  signals:
    void
    changed();
    void
    errorChanged();

  private:
    bool
    fail(const QString& message);
    bool
    commit();
    bool
    validateGroups(const QVariantMap& definition);
    bool
    appendSlots(const QString& id, qint64 untilUtc, qint64 notBeforeUtc = 0, qint64 continuationUtc = -1);
    QVariantList
    eligible(const QVariantMap& definition, const QVariantList& candidates, QString* error) const;
    qint64
    clockTime(const QVariantMap& channel, qint64 nowUtc) const;
    // GuideStore construction helpers
    bool
    validatePath(const QString& path);
    bool
    openDatabase(const QString& path);
    void
    readIdentity(int& version, int& owner);
    bool
    validateIdentity(int version, int owner);
    bool
    validateExistingChannels();
    bool
    savedChannelRowIsValid(QSqlQuery& q);
    bool
    configureDurableStorage();
    bool
    initializeSchema();
    bool
    validateSchemas();
    // Creation / import helpers
    bool
    executeChannelQuery(QSqlQuery& q) const;
    bool
    isReady() const;
    bool
    candidatesAreValid(const QVariantList& candidates, QString* error) const;
    bool
    validateEligibleItem(const QVariantMap& item,
                         const QSet<QString>& fields,
                         QSet<QString>& seen,
                         QString* error) const;
    bool
    createLocalPrepare(const QVariantMap& definition,
                       const QVariantList& candidates,
                       qint64 now,
                       int& days,
                       QVariantMap& valid,
                       QVariantList& selected,
                       qint64& initial,
                       QString& error);
    bool
    createLocalRestoreSnapshot(const QVariantMap& valid,
                               const QVariantList& selected,
                               const QVariantMap& snapshot,
                               const QVariantMap& remapping);
    bool
    createLocalValidateRemapping(const QVariantList& selected,
                                 const QVariantMap& remapping,
                                 const QMap<QString, QVariantMap>& oldItems,
                                 QMap<QString, QVariantMap>& explicitItems,
                                 QString& error);
    static bool
    portablePackageValid(const QVariantMap& package, QVariantMap& def, QString* error);
    static bool
    portableAssetsValid(const QVariantMap& package, const QVariantMap& def, QString* error);
    static bool
    portableSlotsValid(const QVariantMap& package, QSet<QString>& identities, QString* error);
    static bool
    portableItemsValid(const QVariantList& items, QSet<QString>& identities, QString* error);
    static bool
    portableSlotsListValid(const QVariantList& slots, const QSet<QString>& identities, QString* error);
    static bool
    portableSlotShapeValid(const QVariantMap& slot);
    static bool
    portableSlotTimingValid(const QVariantMap& slot, qint64 previous, const QSet<QString>& identities);
    bool
    restoreSnapshotSlots(const QVariantMap& snapshot,
                         const QVariantMap& valid,
                         const QMap<QString, QVariantMap>& oldItems,
                         const QMap<QString, QVariantMap>& explicitItems,
                         const QMap<QString, QVariantMap>& remap,
                         const QSet<QString>& ambiguous,
                         QSqlQuery& insert,
                         qint64& previousEnd,
                         QString& error);
    // Maintenance / import helpers
    bool
    maintainValidate(const QString& id,
                     qint64 wall,
                     int revision,
                     int& horizon,
                     int& retention,
                     QVariantMap& state,
                     qint64& clock,
                     qint64& until,
                     qint64& cutoff);
    bool
    importPvrEntries(const QString& instance,
                     const QVariantList& entries,
                     qint64 refreshed,
                     const QVariantList& previous,
                     QSet<QString>& seen,
                     int& additions);
    bool
    importPvrValidateEntry(const QVariantMap& entry,
                           qint64 refreshed,
                           const QString& instance,
                           const QSet<QString>& seen,
                           QVariantMap& def,
                           QVariantMap& source,
                           bool& feed,
                           int& kodiId,
                           QString& id);
    bool
    importPvrProcessEntry(const QString& instance,
                          const QVariantMap& entry,
                          qint64 refreshed,
                          const QVariantList& previous,
                          QSet<QString>& seen,
                          int& additions,
                          QSqlQuery& q);
    bool
    importPvrPrograms(const QString& id, int kodiId, const QVariantList& programs, qint64 refreshed, QSqlQuery& q);
    bool
    importPvrProgramValid(const QVariantMap& program, qint64 end, qint64 refreshed, QSet<QString>& broadcasts);
    bool
    broadcastAndTitleValid(const QVariantMap& program, const QSet<QString>& broadcasts);
    bool
    importPvrDormant(const QString& instance,
                     qint64 refreshed,
                     const QSet<QString>& seen,
                     const QVariantList& previous);
    // Scheduling helpers
    bool
    appendSlotsInitialize(const QString& id,
                          qint64 until,
                          qint64 notBefore,
                          qint64 continuation,
                          QVariantMap& def,
                          QVariantList& items,
                          qint64& cursor,
                          qint64& start,
                          int& generation,
                          int& count,
                          QString& sequence,
                          QVariantMap& policy,
                          bool& constrained,
                          qint64& cooldown,
                          QMap<QString, qint64>& lastEnds,
                          qint64& cycleDuration,
                          QSqlQuery& insert);
    bool
    appendSlotsLoadCooldown(
        const QString& id, qint64 start, qint64 cooldown, QMap<QString, qint64>& lastEnds, QSqlQuery& q);
    void
    advanceCycle(const QVariantMap& def,
                 const QVariantList& items,
                 const QString& sequence,
                 qint64 cycleDuration,
                 qint64& cursor,
                 qint64& start,
                 qint64 notBefore,
                 bool constrained,
                 QVariantList& ordered,
                 qint64& currentCycle);
    QVariantMap
    selectConstrainedCandidate(const QVariantMap& match,
                               bool respectCooldown,
                               const QVariantList& ordered,
                               qint64 cursor,
                               qint64 start,
                               qint64 boundary,
                               const QMap<QString, qint64>& lastEnds,
                               qint64 cooldown);
    bool
    chooseConstrainedItem(const QVariantMap& def,
                          const QVariantMap& policy,
                          const QMap<QString, qint64>& lastEnds,
                          const QVariantList& ordered,
                          qint64 cursor,
                          qint64 start,
                          QVariantMap& item,
                          qint64& duration,
                          QString& status);
    void
    retryConstrainedGap(const QVariantMap& def,
                        const QVariantMap& policy,
                        const QMap<QString, qint64>& lastEnds,
                        const QVariantList& ordered,
                        qint64 cursor,
                        qint64 start,
                        qint64 boundary,
                        const QVariantMap& rule,
                        qint64 cooldown,
                        qint64& retry);
    bool
    appendOneSlot(QSqlQuery& insert,
                  const QString& id,
                  const QVariantMap& def,
                  const QVariantList& items,
                  QVariantList& ordered,
                  qint64& currentCycle,
                  int& iterations,
                  qint64& cursor,
                  qint64& start,
                  qint64 until,
                  qint64 notBefore,
                  int generation,
                  int& count,
                  bool constrained,
                  const QVariantMap& policy,
                  const QString& sequence,
                  qint64 cooldown,
                  QMap<QString, qint64>& lastEnds,
                  qint64 cycleDuration);
    bool
    appendSlotWindow(QSqlQuery& insert,
                     const QString& id,
                     const QVariantMap& item,
                     qint64 start,
                     qint64 duration,
                     const QString& status,
                     int& count,
                     int generation,
                     qint64& cursor,
                     qint64& startRef,
                     QMap<QString, qint64>& lastEnds);
    // Repair helpers
    bool
    repairPreviewLoad(const QString& id,
                      const QString& slotId,
                      int revision,
                      qint64 wall,
                      QSqlQuery& q,
                      qint64& start,
                      qint64& end,
                      QVariantMap& program,
                      qint64& delta) const;
    bool
    repairSlotReplace(const QString& id,
                      const QString& slotId,
                      int revision,
                      qint64 start,
                      qint64 end,
                      qint64 duration,
                      bool fitInterval,
                      QSqlQuery& q);
    // Metadata / clock helpers
    bool
    clockAdjustmentApply(const QVariantMap& review, qint64 wall, QSqlQuery& q);
    bool
    adjustClockReview(const QVariantMap& review,
                      qint64 wall,
                      QSqlQuery& q,
                      QVariantMap& expected,
                      QVariantMap& program,
                      qint64& from,
                      qint64& to);
    qint64
    adjustClockElapsed(const QVariantMap& state, qint64 wall, qint64 reviewed);
    QSqlDatabase m_db;
    QString m_connection, m_error;
    bool m_ready = false, m_failCommit = false;
    std::function<bool()> m_cancelled;
};
