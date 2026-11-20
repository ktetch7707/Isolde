#pragma once

#include "sessions/AppSession.h"
#include "sessions/NativeSandbox.h"
#include "sessions/SteamSessionOutput.h"

#include <QLockFile>
#include <QProcess>
#include <memory>

struct ResolvedDesktopLaunch;

class DesktopSession final : public AppSession
{
    Q_OBJECT
  public:
    using AppSession::AppSession;
    ~DesktopSession() override;
    QString
    diagnostics() const
    {
        return QString::fromUtf8(m_diagnostics);
    }
    void
    launch() override;
    void
    suspend() override;
    void
    resume() override;
    void
    exit() override;
    bool
    applyLaunchConfiguration(QVariantMap& configuration, const QString& steamGame, const QString& managedGame,
                             ResolvedDesktopLaunch& resolvedLaunch, const NativeSandboxPlan& sandbox,
                             QStringList& launchArguments);
    bool
    applyContainmentTimers(const QString& containmentMode);
    bool
    prepareLaunch(const QString& desktopId, const QProcessEnvironment& environment,
                  ResolvedDesktopLaunch& resolvedLaunch, QStringList& launchArguments);

  private:
    bool
    applyContainment();
    void
    stopProcess();
    void
    onLauncherFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void
    readDiagnostics();
    bool
    parseGamepadStatusLine(const QByteArray& line, QVariantMap& status) const;
    bool
    processGamepadStatusLine(const QVariantMap& status);
    void
    processSteamOutput(const QByteArray& output);
    bool m_stopping = false;
    QByteArray m_diagnostics;
    QByteArray m_protocolBuffer;
    QVariantMap m_gamepadStatus;
    std::unique_ptr<SteamSessionOutput> m_steamOutput;
    bool m_steamObserved = false;
    QProcess m_launcher;
    bool m_containmentMatched = false;
    std::unique_ptr<QLockFile> m_kodiLock;
};
