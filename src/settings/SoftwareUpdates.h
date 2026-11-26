#pragma once

#include <QObject>
#include <QProcess>
#include <QUrl>
#include <QVersionNumber>
#include <QDateTime>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QProcess;
class SearchSettings;

// Discovers candidate releases and, when trusted artifacts are published,
// downloads, verifies, and installs them. Web OFF blocks all network activity.
class SoftwareUpdates final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString releaseDownloadUrl READ releaseDownloadUrl NOTIFY changed)
    Q_PROPERTY(bool installing READ installing NOTIFY changed)
    Q_PROPERTY(QString publicKeyPath READ publicKeyPath NOTIFY changed)

  public:
    explicit SoftwareUpdates(SearchSettings* settings, const QString& installedVersion,
                             QObject* parent = nullptr, QNetworkAccessManager* network = nullptr,
                             const QUrl& endpoint = QUrl(QStringLiteral("https://api.github.com/repos/ktetch7707/Isolde/releases/latest")),
                             const QString& stateFile = {}, const QString& platform = {},
                             const QString& publicKeyPath = {}, const QString& prefix = {});

    bool available() const;
    bool checking() const;
    QString latestVersion() const;
    QString status() const;
    QString releaseDownloadUrl() const;
    bool installing() const;
    QString publicKeyPath() const;

    Q_INVOKABLE void check();
    Q_INVOKABLE void dismiss();
    // Download, verify, and install the latest candidate update. Requires
    // available() == true and Web ON. Progress and completion are reported via
    // the changed() signal and status() text.
    Q_INVOKABLE void install();
    // Roll back to the previous release (requires a previous release to exist).
    Q_INVOKABLE void rollback();
    // Saved restore points: up to 3 pinned versions the user can return to
    // after trying an update. Each entry is { version, current }.
    Q_INVOKABLE QVariantList savedVersions() const;
    // Version of the currently installed release, for saving as a restore point.
    Q_INVOKABLE QString currentInstalledVersion() const;
    // Pin an installed version as a saved restore point (max 3).
    Q_INVOKABLE void saveVersion(const QString& version);
    // Unpin a saved version.
    Q_INVOKABLE void dropVersion(const QString& version);
    // Switch the current release to a saved version.
    Q_INVOKABLE void restoreVersion(const QString& version);

  signals:
    void changed();
    // Emitted when a manual update check is requested while web search is off,
    // so the UI can offer to enable web and check in one flow instead of
    // rerouting to settings and back.
    void webEnableRequested();
    void installFinished(bool success, const QString& message);

  private:
    void webChanged();
    void finished();
    void scheduleNext();
    void fail(const QString& message);
    void startInstall();
    void installProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void installProcessReadyRead();
    QString downloadBaseForTag(const QString& tag) const;
    QStringList pinnedVersions() const;
    void runApplianceTool(const QStringList& args, bool installOperation);
    QString applianceScriptPath() const;

    SearchSettings* m_settings;
    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply = nullptr;
    QTimer* m_timer;
    QUrl m_endpoint;
    QString m_stateFile;
    QString m_prefix;
    QString m_platform;
    QString m_publicKeyPath;
    QString m_latest;
    QString m_downloadBase;
    QString m_dismissed;
    QString m_status;
    QVersionNumber m_installed;
    QDateTime m_retryAfter;
    QProcess* m_installProcess = nullptr;
    QString m_installVersion;
    QString m_installArch;
    bool m_webEnabled = false;
    bool m_installing = false;
};
