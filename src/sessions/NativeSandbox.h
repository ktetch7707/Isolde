#pragma once
#include <QProcessEnvironment>
#include <QStringList>

struct NativeSandboxPlan
{
    QString program;
    QStringList arguments;
    QProcessEnvironment environment;
    QString homeDirectory;
    QString actionTarget;
    QStringList steamLibraryRoots;
    QString error;
};

// Builds a fail-closed launch. Only append the application's arguments afterward.
class NativeSandbox
{
  public:
    static bool
    isGameSession(const QString& emulatorRuntime,
                  const QString& steamGameId,
                  const QString& gameSourceRoot,
                  const QString& managedGameRoot,
                  const QString& opticalMediaId);
    static bool
    hasGamepadKeyCapabilities(const QString& bitmap);
    static QStringList
    gamepadDeviceNodes(const QString& sysInputDirectory = "/sys/class/input",
                       const QString& deviceDirectory = "/dev/input");
    static NativeSandboxPlan
    prepare(const QString& profileId,
            const QString& appId,
            const QString& executable,
            const QProcessEnvironment& host,
            const QString& actionTarget = {},
            const QString& storageRoot = {},
            const QString& emulatorRuntime = {},
            const QString& gameSourceRoot = {},
            const QString& steamGameId = {},
            const QString& managedGameRoot = {},
            const QString& opticalMediaId = {},
            const QString& opticalObservationId = {});
  private:
    static bool
    validateProfileAndApp(const QString& profileId, const QString& appId);
    static bool
    validateSandboxDeps(NativeSandboxPlan& plan, QString& dbus, QString& supervisor,
                        QString& busConfig);
    static bool
    validateBinaryAndOptical(NativeSandboxPlan& plan, const QString& executable,
                              const QString& target, const QString& managedGameRoot,
                              const QString& gameSourceRoot, const QString& profileId,
                              const QString& opticalMediaId, const QString& opticalObservationId,
                              QString& binary, bool& nativeGame, bool& opticalExecutable);
    bool
    validateOpticalExecutable(const QString& binary, const QString& gameSourceRoot,
                              const QString& opticalMediaId, const QString& opticalObservationId,
                              bool& opticalExecutable);
    static bool
    validateManagedExecutable(const QString& binary, const QString& managedGameRoot,
                              const QString& gameSourceRoot, const QString& profileId,
                              bool& managedExecutable);
    static bool
    prepareStorageDirectories(NativeSandboxPlan& plan, const QString& storageRoot,
                              const QString& profileId, const QString& appId);
    static bool
    opticalDirectPlayMount(const QString& gameSourceRoot, const QString& opticalMediaId,
                           const QString& opticalObservationId);
    static bool
    validateActionTargetScheme(const QUrl& url, QString& error);
    static bool
    validateCanonicalPath(const QFileInfo& canonical, const QString& base, const QString& config,
                          const QString& data, bool& privateHomePath, bool& systemPath,
                          const QString& userHome);
    static bool
    validateGameSourceRoot(const QFileInfo& canonical, const QFileInfo& binary,
                           const QString& gameSourceRoot, const QString& opticalMediaId,
                           const QString& opticalObservationId, bool& optical,
                           QSet<QString>& opticalMediaIds, const QString& profileId,
                           const QString& base, const QString& managedGameRoot,
                           bool opticalExecutable, QString& error);
    static bool
    isActionTargetDenied(const QString& canonical, const QString& requested,
                         bool privateHomePath, bool systemPath, bool managedTarget,
                         bool opticalTarget, const QString& base, const QString& config,
                         const QString& data);
    static bool
    applyOpticalNoExec(NativeSandboxPlan& plan, bool opticalExecutable, const QString& binary);
    static bool
    bindOpticalExec(NativeSandboxPlan& plan, const QString& helperRoot);
    static void
    buildSandboxArguments(NativeSandboxPlan& plan, const QProcessEnvironment& host,
                          const QString& appId, const QString& profileId, const QString& base,
                          const QString& binary, const QString& executable, const QString& target,
                          const QString& emulatorRuntime, const QString& steamGameId,
                          const QString& gameSourceRoot, const QString& managedGameRoot,
                          const QString& opticalMediaId, const QString& opticalObservationId,
                          bool nativeGame, bool gameSession,
                          bool opticalExecutable, const QString& dbus, const QString& supervisor,
                          const QString& busConfig);
};
