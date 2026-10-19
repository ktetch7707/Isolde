#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QLockFile>
#include <memory>
#include "KodiMedia.h"
#include <QUrl>

class KodiService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString frontend READ frontend NOTIFY changed)
    Q_PROPERTY(bool switching READ switching NOTIFY changed)
    Q_PROPERTY(QStringList libraryFolders READ libraryFolders NOTIFY changed)
    Q_PROPERTY(bool canEditFolders READ canEditFolders NOTIFY changed)
    Q_PROPERTY(QString folderStatus READ folderStatus NOTIFY changed)
    Q_PROPERTY(QString browsePath READ browsePath NOTIFY changed)
    Q_PROPERTY(QStringList folderChoices READ folderChoices NOTIFY changed)
public:
    explicit KodiService(const QString &profile, QObject *parent = nullptr);
    ~KodiService() override;
    bool enabled() const { return m_enabled; }
    QString status() const { return m_status; }
    QString frontend() const { return m_frontend; }
    bool switching() const { return !m_pendingFrontend.isEmpty(); }
    bool canOpenApplication() const { return !switching() && m_process.state() == QProcess::NotRunning; }
    bool canEditFolders() const { return m_frontend == "kodi" && !m_suspended && canOpenApplication(); }
    QStringList libraryFolders() const;
    QString folderStatus() const { return m_folderStatus; }
    QString browsePath() const { return m_browsePath; }
    QStringList folderChoices() const { return m_folderChoices; }
    Q_INVOKABLE void browseLibraryFolder(const QString &path);
    Q_INVOKABLE void browseLibraryParent();
    Q_INVOKABLE void allowBrowsedFolder();
    Q_INVOKABLE void addLibraryFolder(const QUrl &folder);
    Q_INVOKABLE void removeLibraryFolder(const QString &folder);
    Q_INVOKABLE void setFrontend(const QString &frontend);
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE void retry();
    void start();
    void stop();
    void suspend(bool suspended);
    void markReady();
signals:
    void changed();
    void probeRequested();
    void ready();
    void unavailable(const QString &reason);
private:
    void fail(const QString &reason);
    void commitFrontend(const QString &frontend);
    QString m_profile, m_status = "Not started";
    bool m_enabled = true, m_stopping = false, m_suspended = false, m_ready = false;
    QProcess m_process;
    QTimer m_probe;
    int m_checks = 0;
    std::unique_ptr<QTemporaryDir> m_overlay;
    std::unique_ptr<QLockFile> m_lock;
    QString m_frontend = "isolde", m_pendingFrontend;
    KodiMedia m_modeClient;
    QTimer m_modeDeadline;
    QString m_folderStatus;
    QString m_browsePath;
    QStringList m_folderChoices;
};
