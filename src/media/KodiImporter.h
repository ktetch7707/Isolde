#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QProcess>
#include <QTimer>
#include <QLockFile>
#include <functional>
#include <memory>

class KodiImporter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool backupAvailable READ backupAvailable NOTIFY changed)
    Q_PROPERTY(QString backupPath READ backupPath NOTIFY changed)
    Q_PROPERTY(bool importInProgress READ importInProgress NOTIFY changed)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY changed)
    Q_PROPERTY(QStringList conflicts READ conflicts NOTIFY changed)
    Q_PROPERTY(QString previewText READ previewText NOTIFY changed)
    Q_PROPERTY(bool canImport READ canImport NOTIFY changed)
    Q_PROPERTY(bool canRollback READ canRollback NOTIFY changed)
    Q_PROPERTY(bool recoveryPending READ recoveryPending NOTIFY changed)
public:
    explicit KodiImporter(const QString &profile, QObject *parent = nullptr);
    ~KodiImporter() override;

    bool backupAvailable() const { return m_backupAvailable; }
    QString backupPath() const { return m_backupPath; }
    bool importInProgress() const { return m_importInProgress; }
    QString importStatus() const { return m_importStatus; }
    QStringList conflicts() const { return m_conflicts; }
    QString previewText() const { return m_previewText; }
    bool canImport() const { return m_canImport; }
    bool canRollback() const { return m_canRollback; }
    bool recoveryPending() const;
    void setBackendGuard(std::function<bool()> prepare,std::function<void()> release,std::function<bool()> preparing={}){m_prepareBackend=std::move(prepare);m_releaseBackend=std::move(release);m_backendPreparing=std::move(preparing);}
    void continueWhenBackendReady();
    void shutdown();

    Q_INVOKABLE void scanExistingKodi();
    Q_INVOKABLE void createBackup();
    Q_INVOKABLE void previewImport();
    Q_INVOKABLE void import();
    Q_INVOKABLE void rollback();
    Q_INVOKABLE void clearConflicts();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void releasePreview();

signals:
    void changed();
    void backupCreated(const QString &path);
    void importCompleted(bool success, const QString &message);
    void rollbackCompleted(bool success, const QString &message);

private:
    QString backupDir() const;
    void loadImportState();
    void runAction(const QString &action);
    void finish(bool success);
    void releaseBackend();
    bool m_valid=false;
    QProcess m_process;
    QTimer m_deadline;
    QByteArray m_output;
    QString m_action;
    QString m_deferredAction;
    bool m_cancelled=false;
    bool m_backendPrepared=false;
    bool m_releaseRequested=false;
    quint64 m_generation=0;
    std::unique_ptr<QLockFile> m_lock;
    std::function<bool()> m_prepareBackend;
    std::function<void()> m_releaseBackend;
    std::function<bool()> m_backendPreparing;

    QString m_profile;
    bool m_backupAvailable = false;
    QString m_backupPath;
    bool m_importInProgress = false;
    QString m_importStatus = "Not started";
    QStringList m_conflicts;
    QString m_previewText;
    bool m_canImport = false;
    bool m_canRollback = false;
};
