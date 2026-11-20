#pragma once
#include <QProcessEnvironment>
#include <QStringList>

class QFileInfo;
class QUrl;

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
    static bool
    validateOpticalExecutable(const QString& binary, const QString& gameSourceRoot,
                              const QString& opticalMediaId, const QString& opticalObservationId,
                              const QString& profileId, bool& opticalExecutable);
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
    validateCanonicalPath(const QString& canonical, bool& privateHomePath, bool& systemPath,
                          const QString& userHome);
    static bool
    validateGameSourceRoot(const QString& canonical,
                           const QString& gameSourceRoot, const QString& opticalMediaId,
                           const QString& opticalObservationId, bool& optical, bool& managed,
                           const QString& profileId, const QString& base, QString& error);
    static bool
    isActionTargetDenied(const QString& canonical, const QString& requested,
                         bool privateHomePath, bool systemPath, bool managedTarget,
                         bool opticalTarget, const QString& base, const QString& config,
                         const QString& data);
    static bool
    applyOpticalNoExec(NativeSandboxPlan& plan, bool opticalExecutable, const QString& binary, bool& opticalNoExec);
    static bool
    bindOpticalExec(NativeSandboxPlan& plan, QString helperRoot);
    static void bindSystemDirectories(QStringList& args, const QString& runtime);
    static void setupEnvironment(NativeSandboxPlan& plan, const QProcessEnvironment& host,
                                 const QString& userHome, const QString& runtime, const QString& hostRuntime);
    static bool bindGamepadDevices(NativeSandboxPlan& plan, bool gameSession);
    static bool bindKodiGrants(NativeSandboxPlan& plan, const QString& profileId, const QString& base);
    static bool bindSteam(NativeSandboxPlan& plan, const QString& appId, const QString& steamGameId,
                          const QString& binary, const QString& target);
    static bool bindSteamLibraries(NativeSandboxPlan& plan, const QString& profileId);
    static bool bindEmulator(NativeSandboxPlan& plan, const QString& appId, const QString& emulatorRuntime,
                             const QString& binary, const QString& target);
    static bool resolveActionTarget(NativeSandboxPlan& plan, const QString& target, const QString& userHome,
                                    const QString& managedGameRoot, const QString& profileId,
                                    const QString& gameSourceRoot, const QString& opticalMediaId,
                                    const QString& opticalObservationId, bool opticalExecutable,
                                    const QString& binary, const QString& base);
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
