#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include "games/GameEmulators.h"
#include "games/ManagedGamePaths.h"
#include "games/OpticalDiscGrants.h"
#include "games/OpticalDiscIdentity.h"
#include "games/SteamLibraryGrants.h"
#include "media/MediaGrants.h"
#include "sessions/GamepadTopology.h"
#include "sessions/NativeSandbox.h"
#ifdef Q_OS_LINUX
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace
{
bool
inside(const QString& path, const QString& directory)
{
    return path == directory || path.startsWith(directory + '/');
}

bool
privateDirectory(const QString& path)
{
    // Reject redirects before creating anything below an existing directory.
    auto ancestor = QFileInfo(path);
    while (!ancestor.exists())
    {
        if (ancestor.isSymLink())
            return false;
        ancestor = QFileInfo(ancestor.absolutePath());
    }
    if (ancestor.canonicalFilePath() != ancestor.absoluteFilePath())
        return false;
    if (!QDir().mkpath(path))
        return false;
    return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}
} // namespace
bool
NativeSandbox::isGameSession(const QString& emulatorRuntime,
                             const QString& steamGameId,
                             const QString& gameSourceRoot,
                             const QString& managedGameRoot,
                             const QString& opticalMediaId)
{
    return !emulatorRuntime.isEmpty() || !steamGameId.isEmpty() || !gameSourceRoot.isEmpty() ||
           !managedGameRoot.isEmpty() || !opticalMediaId.isEmpty();
}
bool
NativeSandbox::hasGamepadKeyCapabilities(const QString& bitmap)
{
    return GamepadTopology::hasGamepadKeyCapabilities(bitmap);
}
QStringList
NativeSandbox::gamepadDeviceNodes(const QString& sysInputDirectory, const QString& deviceDirectory)
{
    const QDir sys(sysInputDirectory), devices(deviceDirectory);
    auto nodes = sys.entryList({"event*", "js*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
    QStringList result;
    for (const auto& node : nodes)
    {
        if (!QRegularExpression("^(event|js)[0-9]{1,3}$").match(node).hasMatch())
            continue;
        const auto capabilityPath = sys.filePath(node + "/device/capabilities/key");
        const auto devicePath = sys.filePath(node + "/dev");
        QFile capabilities(capabilityPath);
        QFile deviceNumber(devicePath);
        if (!capabilities.open(QIODevice::ReadOnly) || !deviceNumber.open(QIODevice::ReadOnly))
            continue;
        const auto bitmap = QString::fromLatin1(capabilities.read(4097));
        const auto sysDeviceNumber = QString::fromLatin1(deviceNumber.read(32));
        if (bitmap.size() > 4096 || sysDeviceNumber.size() >= 32 ||
            !GamepadTopology::verifiedGamepadNode(node, bitmap.trimmed(), sysDeviceNumber.trimmed()))
            continue;
        const auto device = devices.filePath(node);
        const QFileInfo info(device);
        if (info.isSymLink() || !info.exists() || info.canonicalFilePath() != QDir::cleanPath(device))
            continue;
        result.append(device);
        if (result.size() > 32)
            break;
    }
    return result;
}
NativeSandboxPlan

bool
NativeSandbox::validateProfileAndApp(const QString& profileId, const QString& appId)
{
    return !((profileId != "default" &&
              (QUuid(profileId).isNull() || QUuid(profileId).toString(QUuid::WithoutBraces) != profileId)) ||
             !QRegularExpression("^[a-zA-Z0-9][a-zA-Z0-9._-]{0,79}$").match(appId).hasMatch());
}

bool
NativeSandbox::validateSandboxDeps(NativeSandboxPlan& plan, QString& dbus, QString& supervisor,
                                   QString& busConfig)
{
    plan.program = QStandardPaths::findExecutable("bwrap");
    const QFileInfo applianceWrapper("/usr/lib/isolde-native/bwrap");
    if (applianceWrapper.exists() || applianceWrapper.isSymLink())
    {
        auto path = applianceWrapper;
        while (true)
        {
            if (path.isSymLink() || path.ownerId() != 0 ||
                (path.permissions() & (QFile::WriteGroup | QFile::WriteOther)))
            {
                plan.error =
                    "Repair the root-owned Isolde native sandbox installation. Shared-account launch is disabled.";
                return false;
            }
            if (path.absoluteFilePath() == "/")
                break;
            path = QFileInfo(path.absolutePath());
        }
        if (!applianceWrapper.isFile() || !applianceWrapper.isExecutable())
        {
            plan.error = "The Isolde native sandbox executable is unavailable.";
            return false;
        }
        plan.program = applianceWrapper.absoluteFilePath();
    }
    dbus = QStandardPaths::findExecutable("dbus-run-session");
    supervisor = QCoreApplication::applicationDirPath() + "/isolde-native-host";
    busConfig = QCoreApplication::applicationDirPath() + "/config/native-session-bus.conf";
    if (plan.program.isEmpty() || dbus.isEmpty() || !QFileInfo(supervisor).isExecutable() ||
        !QFileInfo(busConfig).isReadable())
    {
        plan.error = QStringLiteral(
            "Native profile isolation needs bubblewrap, dbus-run-session, and the Isolde "
            "native host installed. Shared-account launch is disabled.");
        return false;
    }
    return true;
}

bool
NativeSandbox::validateBinaryAndOptical(NativeSandboxPlan& plan, const QString& executable,
                                        const QString& target, const QString& managedGameRoot,
                                        const QString& gameSourceRoot, const QString& profileId,
                                        const QString& opticalMediaId, const QString& opticalObservationId,
                                        QString& binary, bool& nativeGame, bool& opticalExecutable)
{
    binary = QFileInfo(executable).canonicalFilePath();
    if (!validateManagedExecutable(binary, managedGameRoot, gameSourceRoot, profileId, nativeGame))
        return false;
    if (!validateOpticalExecutable(binary, gameSourceRoot, opticalMediaId, opticalObservationId, opticalExecutable))
        return false;
    nativeGame = nativeGame || opticalExecutable;
    if (!opticalMediaId.isEmpty() && !opticalExecutable)
    {
        plan.error = "The approved optical disc changed or is unavailable. Reinsert it and retry.";
        return false;
    }
    QFile nativeFile(binary);
    if (nativeGame && (target != binary || executable != binary || !nativeFile.open(QIODevice::ReadOnly) ||
                       nativeFile.read(4) != QByteArray("\x7f"
                                                        "ELF",
                                                        4)))
    {
        plan.error = "The native game executable changed or is not a Linux ELF file.";
        return false;
    }
    if (binary.isEmpty() || (!inside(binary, "/usr") && !inside(binary, "/opt") && !nativeGame))
    {
        plan.error = QStringLiteral(
            "This app must be installed in a system application directory before it can run in a separate profile.");
        return false;
    }
    if (QStringList{"flatpak", "snap", "gtk-launch", "gio", "xdg-open"}.contains(QFileInfo(binary).fileName()))
    {
        plan.error =
            QStringLiteral("This app launcher needs a dedicated profile adapter. Shared-account launch is disabled.");
        return false;
    }
    return true;
}

bool
NativeSandbox::validateManagedExecutable(const QString& binary, const QString& managedGameRoot,
                                         const QString& gameSourceRoot, const QString& profileId,
                                         bool& managedExecutable)
{
    managedExecutable = !managedGameRoot.isEmpty() && gameSourceRoot == managedGameRoot &&
                        ManagedGamePaths::isInstallRoot(profileId, managedGameRoot) &&
                        inside(binary, managedGameRoot) && QFileInfo(binary).isExecutable();
    return true;
}

bool
NativeSandbox::validateOpticalExecutable(const QString& binary, const QString& gameSourceRoot,
                                         const QString& opticalMediaId, const QString& opticalObservationId,
                                         bool& opticalExecutable)
{
    opticalExecutable = false;
    if (!opticalMediaId.isEmpty() && OpticalDiscGrants::approved(profileId, opticalMediaId))
        opticalExecutable = inside(binary, gameSourceRoot) &&
                            (QFileInfo(binary).permissions() & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)) &&
                            opticalDirectPlayMount(gameSourceRoot, opticalMediaId, opticalObservationId);
    return true;
}

bool
NativeSandbox::prepareStorageDirectories(NativeSandboxPlan& plan, const QString& storageRoot,
                                         const QString& profileId, const QString& appId)
{
    const auto base = QDir::cleanPath(
        storageRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles"
                              : storageRoot);
    if (!QDir::isAbsolutePath(base) || !privateDirectory(base))
    {
        plan.error = QStringLiteral("Cannot create protected native app storage.");
        return false;
    }
    const auto appRoot = base + '/' + profileId + '/' + appId;
    plan.homeDirectory = appRoot + "/home";
    for (const auto& path : QStringList{appRoot,
                                        plan.homeDirectory,
                                        plan.homeDirectory + "/.config",
                                        plan.homeDirectory + "/.local/share",
                                        plan.homeDirectory + "/.local/state",
                                        plan.homeDirectory + "/.cache"})
    {
        if (!privateDirectory(path))
        {
            plan.error = QStringLiteral("Cannot prepare this profile's private app folders.");
            return false;
        }
    }
    return true;
}

void
NativeSandbox::bindSystemDirectories(QStringList& args, const QString& runtime)
{
    for (const auto& path : QStringList{"/usr", "/bin", "/sbin", "/lib", "/lib64", "/etc", "/opt"})
        if (QFileInfo::exists(path))
            args << "--ro-bind" << path << path;
    args << "--proc" << "/proc" << "--dev" << "/dev" << "--tmpfs" << "/tmp"
         << "--dir" << "/run" << "--perms" << "0700" << "--dir" << runtime;
}

void
NativeSandbox::setupEnvironment(NativeSandboxPlan& plan, const QProcessEnvironment& host,
                                const QString& userHome, const QString& runtime,
                                const QString& hostRuntime)
{
    auto& args = plan.arguments;
    const auto setenv = [&](const QString& key, const QString& value)
    {
        args << "--setenv" << key << value;
    };
    setenv("HOME", userHome);
    setenv("ISOLDE_NATIVE_NAMESPACE", "1");
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin");
    setenv("USER", host.value("USER", "isolde"));
    setenv("LOGNAME", host.value("USER", "isolde"));
    setenv("LANG", host.value("LANG", "C.UTF-8"));
    setenv("XDG_CONFIG_HOME", userHome + "/.config");
    setenv("XDG_DATA_HOME", userHome + "/.local/share");
    setenv("XDG_STATE_HOME", userHome + "/.local/state");
    setenv("XDG_CACHE_HOME", userHome + "/.cache");
    setenv("XDG_RUNTIME_DIR", runtime);
    setenv("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    // Pass only display/audio endpoints, never the host bus, keyring, or agents.
    for (const auto& key : QStringList{"DISPLAY", "XDG_SESSION_TYPE"})
        if (host.contains(key))
            setenv(key, host.value(key));
    const auto wayland = host.value("WAYLAND_DISPLAY");
    if (!wayland.isEmpty() && !wayland.contains('/') && QFileInfo::exists(hostRuntime + '/' + wayland))
    {
        args << "--ro-bind" << (hostRuntime + '/' + wayland) << (runtime + '/' + wayland);
        setenv("WAYLAND_DISPLAY", wayland);
    }
    if (host.contains("DISPLAY") && QFileInfo::exists("/tmp/.X11-unix"))
        args << "--ro-bind" << "/tmp/.X11-unix" << "/tmp/.X11-unix";
    const auto authority = host.value("XAUTHORITY");
    if (!authority.isEmpty() && QFileInfo(authority).isFile())
    {
        args << "--ro-bind" << authority << "/run/isolde-xauthority";
        setenv("XAUTHORITY", "/run/isolde-xauthority");
    }
    if (QFileInfo::exists(hostRuntime + "/pulse/native"))
    {
        args << "--ro-bind" << (hostRuntime + "/pulse/native") << (runtime + "/pulse/native");
        setenv("PULSE_SERVER", "unix:" + runtime + "/pulse/native");
    }
    const auto pulseCookie = userHome + "/.config/pulse/cookie";
    if (QFileInfo(pulseCookie).isFile())
    {
        args << "--ro-bind" << pulseCookie << "/run/isolde-pulse-cookie";
        setenv("PULSE_COOKIE", "/run/isolde-pulse-cookie");
    }
    if (QFileInfo::exists(hostRuntime + "/pipewire-0"))
        args << "--ro-bind" << (hostRuntime + "/pipewire-0") << (runtime + "/pipewire-0");
    if (QFileInfo::exists("/dev/dri"))
        args << "--dev-bind" << "/dev/dri" << "/dev/dri";
}

bool
NativeSandbox::bindGamepadDevices(NativeSandboxPlan& plan, bool gameSession)
{
    if (!gameSession)
        return true;
    // Give game sessions gamepad nodes only. Do not expose keyboard, mouse
    // or the entire host input directory to an application session.
    const auto nodes = gamepadDeviceNodes();
    if (nodes.size() > 32)
    {
        plan.error = "Too many gamepad devices. Disconnect unused devices and retry.";
        return false;
    }
    auto& args = plan.arguments;
    for (const auto& device : nodes)
        args << "--dev-bind" << device << device;
    QStringList boundEventNodes;
    for (const auto& device : nodes)
    {
        const auto name = QFileInfo(device).fileName();
        if (name.startsWith("event"))
            boundEventNodes.append(name);
    }
    const auto setenv = [&](const QString& key, const QString& value)
    {
        args << "--setenv" << key << value;
    };
    setenv("ISOLDE_GAMEPAD_BOUND_EVENTS", boundEventNodes.join(','));
    return true;
}

bool
NativeSandbox::bindKodiGrants(NativeSandboxPlan& plan, const QString& profileId, const QString& base)
{
    const auto grants = MediaGrants::folders(profileId);
    if (grants.size() > 32)
    {
        plan.error = "Too many saved Kodi media grants.";
        return false;
    }
    auto& args = plan.arguments;
    for (const auto& path : grants)
    {
        const auto error = MediaGrants::validate(path, false);
        if (!error.isEmpty() || inside(base, path) || inside(path, base))
        {
            plan.error = "Review Kodi media-folder access in Settings. " + error;
            return false;
        }
        // Keep Kodi usable when a removable library drive is disconnected.
        if (QFileInfo(path).isDir())
            args << "--ro-bind" << path << path;
    }
    return true;
}


bool
NativeSandbox::isActionTargetDenied(const QString& canonical, const QString& requested,
                                    bool privateHomePath, bool systemPath, bool managedTarget,
                                    bool opticalTarget, const QString& base, const QString& config,
                                    const QString& data)
{
    return canonical.isEmpty() || !QDir::isAbsolutePath(requested) ||
           (!QFileInfo(canonical).isFile() && !QFileInfo(canonical).isDir()) ||
           (privateHomePath && !managedTarget && !opticalTarget) ||
           (systemPath && !managedTarget && !opticalTarget) ||
           (!managedTarget && (inside(base, canonical) || inside(canonical, base))) ||
           (!managedTarget && (inside(config, canonical) || inside(canonical, config) || inside(data, canonical) ||
                               inside(canonical, data)));
}

bool
NativeSandbox::validateActionTargetScheme(const QUrl& url, QString& error)
{
    if (!(url.isLocalFile() || url.scheme().isEmpty()))
    {
        error = QStringLiteral("This media address is not supported by isolated native apps.");
        return false;
    }
    return true;
}

bool
NativeSandbox::validateCanonicalPath(const QFileInfo& canonical, const QString& base,
                                     const QString& config, const QString& data,
                                     bool& privateHomePath, bool& systemPath, const QString& userHome)
{
    privateHomePath = false;
    if (inside(canonical, userHome))
    {
        const auto components = QDir(userHome).relativeFilePath(canonical).split('/');
        for (const auto& component : components)
            privateHomePath |= component.startsWith('.');
    }
    systemPath = false;
    for (const auto& system :
         QStringList{"/etc", "/usr", "/opt", "/sys", "/dev", "/root", "/var", "/run", "/proc"})
        systemPath |= inside(canonical, system);
    return true;
}

bool
NativeSandbox::validateGameSourceRoot(const QFileInfo& canonical, const QFileInfo& binary,
                                      const QString& gameSourceRoot, const QString& opticalMediaId,
                                      const QString& opticalObservationId, bool& optical,
                                      QSet<QString>& opticalMediaIds, const QString& profileId)
{
    optical = opticalDirectPlayMount(gameSourceRoot, opticalMediaId, opticalObservationId) &&
              OpticalDiscGrants::approved(profileId, opticalMediaId);
    const bool managed = ManagedGamePaths::isInstallRoot(profileId, gameSourceRoot);
    if ((!opticalMediaId.isEmpty() && !optical) ||
        !QFileInfo(canonical).isFile() || !canonical.startsWith(gameSourceRoot + '/') ||
        (!managed && (inside(base, gameSourceRoot) || inside(gameSourceRoot, base))))
    {
        plan.error = "The reviewed game folder is unavailable or redirected. Review it in Games and retry.";
        return false;
    }
    return true;
}

bool
NativeSandbox::bindSteam(NativeSandboxPlan& plan, const QString& appId, const QString& steamGameId,
                         const QString& binary, const QString& target)
{
    bool valid = false;
    const auto number = steamGameId.toULongLong(&valid);
    auto helperRoot = QCoreApplication::applicationDirPath() + "/share/isolde";
    if (!QFileInfo(helperRoot + "/steam-games.py").isReadable())
        helperRoot = QCoreApplication::applicationDirPath() + "/../share/isolde";
    const auto helper = helperRoot + "/steam-games.py";
    if (appId != "steam" || !QRegularExpression("^[1-9][0-9]{0,9}$").match(steamGameId).hasMatch() || !valid ||
        number > 4294967295ULL || QString::number(number) != steamGameId ||
        binary != QFileInfo("/usr/bin/python3").canonicalFilePath() || !target.isEmpty() ||
        !QFileInfo(helper).isReadable())
    {
        plan.error = "Repair the private Steam game adapter or import this game again.";
        return false;
    }
    plan.arguments << "--ro-bind" << helper << "/run/steam-games.py";
    }

bool
NativeSandbox::resolveActionTarget(NativeSandboxPlan& plan, const QString& target,
                                   const QString& userHome, const QString& managedGameRoot,
                                   const QString& profileId, const QString& gameSourceRoot,
                                   const QString& opticalMediaId, const QString& opticalObservationId,
                                   bool opticalExecutable, const QString& binary, const QString& base)
{
    plan.actionTarget = target;
    const QUrl url(target);
    if (!validateActionTargetScheme(url, plan.error))
        return false;
    const auto requested = url.isLocalFile() ? url.toLocalFile() : target;
    const QFileInfo canonical(requested);
    bool privateHomePath = false, systemPath = false;
    if (!validateCanonicalPath(canonical, base, QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
                               QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
                               privateHomePath, systemPath, userHome))
        return false;
    const bool managedTarget = !managedGameRoot.isEmpty() &&
                               ManagedGamePaths::isInstallRoot(profileId, managedGameRoot) &&
                               inside(canonical, managedGameRoot);
    const bool opticalTarget = opticalExecutable && canonical == binary;
    bool optical = false;
    QSet<QString> opticalMediaIds;
    if (!validateGameSourceRoot(canonical, binary, gameSourceRoot, opticalMediaId, opticalObservationId,
                                optical, opticalMediaIds, profileId))
        return false;
    if (isActionTargetDenied(canonical, requested, privateHomePath, systemPath, managedTarget,
                             opticalTarget, base, QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
                             QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)))
    {
        plan.error = QStringLiteral("Choose a media file or folder, not a profile, account, or system folder.");
        return false;
    }
    auto& args = plan.arguments;
    if (!gameSourceRoot.isEmpty())
    {
        plan.actionTarget = "/media/games/" + QDir(gameSourceRoot).relativeFilePath(canonical);
        args << "--ro-bind" << gameSourceRoot << "/media/games";
        if (opticalTarget)
            args << "--chdir" << "/media/games";
    }
    else
       {
        plan.actionTarget = "/media/selected/" + QFileInfo(canonical).fileName();
        args << "--ro-bind" << canonical << plan.actionTarget;
    }
    return true;
}

bool
NativeSandbox::validateActionTargetScheme(const QUrl& url, QString& error)
{
    if (!(url.isLocalFile() || url.scheme().isEmpty()))
    {
        error = QStringLiteral("This media address is not supported by isolated native apps.");
        return false;
    }
    return true;
}

bool
NativeSandbox::validateCanonicalPath(const QFileInfo& canonical, const QString& base,
                                     const QString& config, const QString& data,
                                     bool& privateHomePath, bool& systemPath, const QString& userHome)
{
    privateHomePath = false;
    if (inside(canonical, userHome))
    {
        const auto components = QDir(userHome).relativeFilePath(canonical).split('/');
        for (const auto& component : components)
            privateHomePath |= component.startsWith('.');
    }
    systemPath = false;
    for (const auto& system :
         QStringList{"/etc", "/usr", "/opt", "/sys", "/dev", "/root", "/var", "/run", "/proc"})
        systemPath |= inside(canonical, system);
    return true;
}

bool
NativeSandbox::validateGameSourceRoot(const QFileInfo& canonical, const QFileInfo& binary,
                                      const QString& gameSourceRoot, const QString& opticalMediaId,
                                      const QString& opticalObservationId, bool& optical,
                                      QSet<QString>& opticalMediaIds, const QString& profileId)
{
    bool opticalTarget = opticalExecutable && canonical == binary;
    optical = opticalDirectPlayMount(gameSourceRoot, opticalMediaId, opticalObservationId) &&
              OpticalDiscGrants::approved(profileId, opticalMediaId);
    if ((!opticalMediaId.isEmpty() && !optical) || (!error.isEmpty() && !managed && !optical) ||
        !QFileInfo(canonical).isFile() || !canonical.startsWith(gameSourceRoot + '/') ||
        (!managed && (inside(base, gameSourceRoot) || inside(gameSourceRoot, base))))
    {
        plan.error = "The reviewed game folder is unavailable or redirected. Review it in Games and retry.";
        return false;
    }
    return true;
}

bool
NativeSandbox::bindSteamLibraries(NativeSandboxPlan& plan, const QString& profileId)
{
    const auto libraries = SteamLibraryGrants::folders(profileId);
    const auto grantsError = SteamLibraryGrants::validateList(libraries, false);
    if (!grantsError.isEmpty())
    {
        plan.error = "Review external Steam library access in Games settings. " + grantsError;
        return false;
    }
    auto& args = plan.arguments;
    QSet<QString> createdParents;
    for (const auto& library : libraries)
    {
        if (!QFileInfo(library).isDir())
            continue;
        QString parent;
        for (const auto& part : library.split('/', Qt::SkipEmptyParts))
        {
            parent += "/" + part;
            if (parent == library)
                break;
            if (!createdParents.contains(parent))
            {
                args << "--dir" << parent;
                createdParents.insert(parent);
            }
        }
        args << "--ro-bind" << library << library;
        plan.steamLibraryRoots.append(library);
    }
    return true;
}

bool
NativeSandbox::bindEmulator(NativeSandboxPlan& plan, const QString& appId, const QString& emulatorRuntime,
                            const QString& binary, const QString& target)
{
    const auto runtimeDefinition = GameEmulators::definition(emulatorRuntime);
    auto helperRoot = QCoreApplication::applicationDirPath() + "/share/isolde";
    if (!QFileInfo(helperRoot + "/emulator-session.py").isReadable())
        helperRoot = QCoreApplication::applicationDirPath() + "/../share/isolde";
    const auto helper = helperRoot + "/emulator-session.py";
    const auto manifest = helperRoot + "/game-runtimes.json";
    if (runtimeDefinition.isEmpty() || emulatorRuntime != appId ||
        binary != QFileInfo("/usr/bin/python3").canonicalFilePath() || !QFileInfo(helper).isReadable() ||
        !QFileInfo(manifest).isReadable() || target.isEmpty())
    {
        plan.error = "Repair the managed emulator adapter or choose a game from Games.";
        return false;
    }
    plan.arguments << "--ro-bind" << helper << "/run/emulator-session.py";
    plan.arguments << "--ro-bind" << manifest << "/run/game-runtimes.json";
    if (runtimeDefinition.contains("setup"))
    {
        const auto expanded = helperRoot + "/expanded-emulators.py";
        if (!QFileInfo(expanded).isReadable())
        {
            plan.error = "Repair the expanded emulator adapter.";
            return false;
        }
        plan.arguments << "--ro-bind" << expanded << "/run/expanded-emulators.py";
        plan.arguments << "--unshare-net";
    }
    return true;
}


bool
NativeSandbox::opticalDirectPlayMount(const QString& gameSourceRoot, const QString& opticalMediaId,
                                      const QString& opticalObservationId)
{
    for (const auto& item : OpticalDiscHash::mountedDiscs())
    {
        const auto disc = item.toMap();
        if (disc.value("mountRoot") == gameSourceRoot && disc.value("mediaId") == opticalMediaId &&
            disc.value("observationId") == opticalObservationId && !opticalObservationId.isEmpty() &&
            disc.value("directPlaySupported").toBool())
            return true;
    }
    return false;
}

bool
NativeSandbox::applyOpticalNoExec(NativeSandboxPlan& plan, bool opticalExecutable, const QString& binary)
{
    bool opticalNoExec = false;
    if (!opticalExecutable)
        return true;
    struct statvfs fs{};
    const auto path = QFile::encodeName(binary);
    if (::statvfs(path.constData(), &fs) != 0)
        return false;
    opticalNoExec = fs.f_flag & ST_NOEXEC;
    if (opticalNoExec)
    {
        auto helperRoot = QCoreApplication::applicationDirPath() + "/share/isolde";
        if (!bindOpticalExec(plan, helperRoot))
            return false;
    }
    return true;
}

void
NativeSandbox::buildSandboxArguments(NativeSandboxPlan& plan, const QProcessEnvironment& host,
                                     const QString& appId, const QString& profileId, const QString& base,
                                     const QString& binary, const QString& executable, const QString& target,
                                     const QString& emulatorRuntime, const QString& steamGameId,
                                     const QString& gameSourceRoot, const QString& managedGameRoot,
                                     const QString& opticalMediaId, const QString& opticalObservationId,
                                     bool nativeGame, bool gameSession,
                                     bool opticalExecutable, const QString& dbus, const QString& supervisor,
                                     const QString& busConfig)
{
    const auto userHome = QDir::homePath();
    const auto runtime = QStringLiteral("/run/user/%1").arg(getuid());
    const auto hostRuntime = host.value("XDG_RUNTIME_DIR", runtime);
    auto& args = plan.arguments;
    const bool gameSessionLocal = gameSession;
    args = {"--unshare-user",
            "--unshare-pid",
            "--unshare-ipc",
            "--unshare-uts",
            "--die-with-parent",
            "--new-session",
            "--cap-drop",
            "ALL",
            "--clearenv"};
    if (gameSessionLocal)
        args << "--cap-add" << "CAP_MKNOD";
    bindSystemDirectories(args, runtime);
    setupEnvironment(plan, host, userHome, runtime, hostRuntime);
    if (!bindGamepadDevices(plan, gameSessionLocal))
        return;
    if (appId == "kodi" && !bindKodiGrants(plan, profileId, base))
        return;
    if (!target.isEmpty() && !resolveActionTarget(plan, target, userHome, managedGameRoot, profileId,
                                                  gameSourceRoot, opticalMediaId, opticalObservationId,
                                                  opticalExecutable, binary, base))
        return;
    if (!steamGameId.isEmpty() && !bindSteam(plan, appId, steamGameId, binary, target))
        return;
    if (appId == "steam" && !bindSteamLibraries(plan, profileId))
        return;
    args << "--ro-bind" << supervisor << "/run/isolde-native-host";
    args << "--ro-bind" << busConfig << "/run/isolde-session-bus.conf";
    if (!emulatorRuntime.isEmpty() && !bindEmulator(plan, appId, emulatorRuntime, binary, target))
        return;
    if (!applyOpticalNoExec(plan, opticalExecutable, binary))
        return;
    args << "--" << dbus << "--config-file=/run/isolde-session-bus.conf" << "--" << "/run/isolde-native-host";
    if (gameSessionLocal)
        args << "--game-session";
    if (!steamGameId.isEmpty())
        args << "--steam-session";
    args << "--";
    args << (nativeGame ? plan.actionTarget : executable);
}

bool
NativeSandbox::bindOpticalExec(NativeSandboxPlan& plan, const QString& helperRoot)
{
    if (!QFileInfo(helperRoot + "/optical-exec.py").isReadable())
        helperRoot = QCoreApplication::applicationDirPath() + "/../share/isolde";
    const auto helper = helperRoot + "/optical-exec.py";
    if (!QFileInfo(helper).isReadable() || !QFileInfo("/usr/bin/python3").isExecutable())
    {
        plan.error = "The optical no-exec adapter is unavailable. Repair Isolde and retry.";
        return false;
    }
    args << "--ro-bind" << helper << "/run/isolde-optical-exec.py";
    return true;
}
NativeSandboxPlan
NativeSandbox::prepare(const QString& profileId,
                       const QString& appId,
                       const QString& executable,
                       const QProcessEnvironment& host,
                       const QString& target,
                       const QString& storageRoot,
                       const QString& emulatorRuntime,
                       const QString& gameSourceRoot,
                       const QString& steamGameId,
                       const QString& managedGameRoot,
                       const QString& opticalMediaId,
                       const QString& opticalObservationId)
{
    NativeSandboxPlan plan;
#ifndef Q_OS_LINUX
    plan.error = QStringLiteral("Separate native app accounts currently require Linux. This app was not opened.");
    return plan;
#else
    if (!validateProfileAndApp(profileId, appId))
    {
        plan.error = QStringLiteral("A valid profile and app identity are required for a separate app session.");
        return plan;
    }

    QString dbus, supervisor, busConfig;
    if (!validateSandboxDeps(plan, dbus, supervisor, busConfig))
        return plan;

    QString binary;
    bool nativeGame = false, opticalExecutable = false;
    if (!validateBinaryAndOptical(plan, executable, target, managedGameRoot, gameSourceRoot,
                                  profileId, opticalMediaId, opticalObservationId,
                                  binary, nativeGame, opticalExecutable))
        return plan;

    const bool gameSession =
        isGameSession(emulatorRuntime, steamGameId, gameSourceRoot, managedGameRoot, opticalMediaId);

    if (!prepareStorageDirectories(plan, storageRoot, profileId, appId))
        return plan;

    const QString base = QDir::cleanPath(
        storageRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles"
                              : storageRoot);
    buildSandboxArguments(plan, host, appId, profileId, base, binary, executable, target,
                          emulatorRuntime, steamGameId, gameSourceRoot, managedGameRoot,
                          opticalMediaId, opticalObservationId, nativeGame, gameSession,
                          opticalExecutable, dbus, supervisor, busConfig);

    plan.environment.insert("PATH", "/usr/bin:/bin");
    plan.environment.insert("LANG", host.value("LANG", "C.UTF-8"));
    return plan;
#endif
}

