#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QLocalSocket>
#include <QVariantList>
#include <QSet>
#include <atomic>
#include <functional>
#include <memory>
class GuideStore;
class GuideWorker;

// One coordinator per profile. SQLite objects live exclusively on its worker;
// QML receives bounded immutable snapshots. Viewing never starts a catalog read.
class GuideController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool backgroundBusy READ backgroundBusy NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString scheduleStatus READ scheduleStatus NOTIFY changed)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList compactRows READ compactRows NOTIFY changed)
    Q_PROPERTY(QVariantList previewItems READ previewItems NOTIFY changed)
    Q_PROPERTY(int previewCount READ previewCount NOTIFY changed)
    Q_PROPERTY(int skippedCount READ skippedCount NOTIFY changed)
    Q_PROPERTY(bool previewReady READ previewReady NOTIFY changed)
    Q_PROPERTY(QVariantMap previewDefinition READ previewDefinition NOTIFY changed)
    Q_PROPERTY(QVariantList sourcePreview READ sourcePreview NOTIFY changed)
    Q_PROPERTY(bool sourcePreviewReady READ sourcePreviewReady NOTIFY changed)
    Q_PROPERTY(QVariantMap regenerationReview READ regenerationReview NOTIFY changed)
    Q_PROPERTY(QVariantMap repairReview READ repairReview NOTIFY changed)
    Q_PROPERTY(QVariantMap clockReview READ clockReview NOTIFY changed)
public:
    GuideController(const QString &databasePath,const QString &profile,QObject *parent=nullptr,const QString &bridgeDirectory={});
    ~GuideController() override;
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy || bool(m_pendingJob) || !m_pendingDefinition.isEmpty(); }
    bool backgroundBusy() const { return m_busy && m_background && !m_pendingJob && m_pendingDefinition.isEmpty(); }
    QString errorString() const { return m_error; }
    QString status() const { return m_status; }
    QString scheduleStatus() const { return m_scheduleStatus; }
    QVariantList channels() const { return m_channels; }
    QVariantList rows() const { return m_rows; }
    QVariantList compactRows() const { return m_compactRows; }
    QVariantList previewItems() const { return m_candidates.mid(0,8); }
    int previewCount() const { return int(m_candidates.size()); }
    int skippedCount() const { return m_skipped; }
    bool previewReady() const { return m_previewReady; }
    QVariantMap previewDefinition() const { return m_previewDefinition; }
    QVariantList sourcePreview() const { return m_sourcePreview; }
    bool sourcePreviewReady() const { return m_sourcePreviewReady; }
    QVariantMap regenerationReview() const { return m_regenerationReview; }
    QVariantMap repairReview() const { return m_repairReview; }
    QVariantMap clockReview() const { return m_clockReview; }
    static qint64 pvrUtc(const QVariant &value);
    Q_INVOKABLE QVariantMap editableDefinition(const QString &id) const;
    Q_INVOKABLE bool updateLocal(const QVariantMap &definition,int revision);
    Q_INVOKABLE bool previewRegeneration(const QString &id);
    Q_INVOKABLE bool regenerate();
    Q_INVOKABLE bool previewRepair(const QString &id,const QString &slotId,int revision,int first=0,bool fitInterval=false);
    Q_INVOKABLE bool previewClockAdjustment(const QString &id,const QVariant &deltaSeconds,int revision);
    Q_INVOKABLE bool adjustClock();
    Q_INVOKABLE bool repair(const QVariant &choice);
    Q_INVOKABLE bool previewDuplicate(const QString &id,int revision);
    Q_INVOKABLE bool previewPvr();
    Q_INVOKABLE bool importPvr();
    Q_INVOKABLE bool setSourceNumber(int index,const QVariant &number);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void maintainSchedules();
    Q_INVOKABLE void setWindow(qint64 fromUtc,qint64 toUtc,int firstRow=0);
    Q_INVOKABLE QVariantMap newDefinition() const;
    Q_INVOKABLE bool previewLocal(const QVariantMap &definition,const QString &mediaKind);
    Q_INVOKABLE bool createLocal(const QVariantMap &definition);
    Q_INVOKABLE bool exportChannel(const QString &id,const QString &path,bool includeSchedule=false);
    Q_INVOKABLE bool previewImport(const QString &path,bool restoreSchedule=false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool setPreferences(const QString &id,const QVariantMap &preferences,int revision);
    Q_INVOKABLE bool setMetadata(const QString &id,const QVariantMap &metadata,int revision);
    Q_INVOKABLE bool swapNumbers(const QString &id,const QString &otherId,const QVariantMap &revisions);
    Q_INVOKABLE bool reorder(const QStringList &ids,const QVariantMap &revisions);
    Q_INVOKABLE bool remove(const QString &id,int revision);
    // Execution is routed through ActionDispatcher; QML cannot launch IDs or
    // offsets obtained from an arbitrary source.
    bool tune(const QString &id,const QString &origin);
    bool markUnavailable(const QString &kind,int mediaId);
    void invalidate();
signals:
    void changed();
    void channelCreated(const QString &id);
    void duplicatePrepared(const QVariantMap &definition);
    void targetReady(const QVariantMap &target,const QString &origin);
private:
    using Job=std::function<QVariantMap(GuideStore &)>;
    bool submit(Job job,std::function<void(const QVariantMap &)> completion={},bool background=false);
    bool fail(const QString &message);
    void requestPage();
    void consume();
    void consumePvr(const QVariantMap &result);
    void closeBridge();
    void readFailure(const QString &message);
    QThread m_thread;
    GuideWorker *m_worker=nullptr;
    std::shared_ptr<std::atomic_bool> m_cancel;
    std::function<void(const QVariantMap &)> m_completion;
    Job m_pendingJob;
    std::function<void(const QVariantMap &)> m_pendingCompletion;
    QVariantMap m_pendingDefinition;
    QString m_pendingKind;
    QTimer m_checkpoint,m_maintenance,m_requestDeadline,m_catalogDeadline;
    QLocalSocket m_socket;
    QByteArray m_input,m_request;
    QString m_directory,m_error,m_status,m_scheduleStatus;
    QSet<QString> m_maintenanceAttempted;
    QVariantList m_channels,m_rows,m_compactRows,m_candidates;
    QVariantMap m_previewDefinition;
    QVariantList m_sourcePreview;
    QVariantMap m_sourceRevisions;
    QVariantMap m_regenerationReview;
    QVariantMap m_repairReview;
    QVariantMap m_clockReview;
    QVariantMap m_importPackage;
    qint64 m_sourceRefreshed=0;
    int m_sourceIndex=-1;
    bool m_pvrReading=false,m_sourcePreviewReady=false;
    QStringList m_kinds;
    qint64 m_from=0,m_to=0;
    int m_first=0,m_page=0,m_requestId=0,m_descriptor=-1,m_skipped=0,m_kindIndex=0,m_snapshotBytes=0;
    quint64 m_jobId=0;
    quint64 m_catalogGeneration=0;
    bool m_ready=false,m_busy=false,m_valid=true,m_reading=false,m_previewReady=false,m_refreshPending=false;
    bool m_background=false;
};
