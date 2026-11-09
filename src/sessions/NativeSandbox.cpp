#include "sessions/NativeSandbox.h"
#include "media/MediaGrants.h"
#include "games/GameEmulators.h"
#include "games/SteamLibraryGrants.h"
#include "games/ManagedGamePaths.h"
#include "games/OpticalDiscAdapter.h"
#include "games/OpticalDiscGrants.h"
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QSet>
#ifdef Q_OS_LINUX
#include <unistd.h>
#include <sys/statvfs.h>
#endif

namespace {
bool inside(const QString &path, const QString &directory)
{
    return path == directory || path.startsWith(directory + '/');
}

bool privateDirectory(const QString &path)
{
    // Reject redirects before creating anything below an existing directory.
    auto ancestor = QFileInfo(path);
    while (!ancestor.exists()) {
        if (ancestor.isSymLink()) return false;
        ancestor = QFileInfo(ancestor.absolutePath());
    }
    if (ancestor.canonicalFilePath() != ancestor.absoluteFilePath()) return false;
    if (!QDir().mkpath(path)) return false;
    return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}
}
bool NativeSandbox::isGameSession(const QString &emulatorRuntime,const QString &steamGameId,
                                  const QString &gameSourceRoot,const QString &managedGameRoot,
                                  const QString &opticalMediaId)
{
    return !emulatorRuntime.isEmpty() || !steamGameId.isEmpty() || !gameSourceRoot.isEmpty()
        || !managedGameRoot.isEmpty() || !opticalMediaId.isEmpty();
}
bool NativeSandbox::hasGamepadKeyCapabilities(const QString &bitmap)
{
    if(bitmap.isEmpty() || bitmap.size()>4096)return false;
    const auto words=bitmap.split(' ',Qt::SkipEmptyParts);
    if(words.isEmpty() || words.size()>512)return false;
    for(const auto &word:words){
        if(word.isEmpty() || word.size()>16 || !QRegularExpression("^[0-9a-fA-F]+$").match(word).hasMatch())return false;
    }
    for(int code=0x120;code<0x140;++code){
        const auto word=words.size()-1-code/64;
        if(word<0)continue;
        bool ok=false;
        const auto bits=words.at(word).toULongLong(&ok,16);
        if(ok && (bits&(quint64(1)<<(code%64))))return true;
    }
    return false;
}
QStringList NativeSandbox::gamepadDeviceNodes(const QString &sysInputDirectory,const QString &deviceDirectory)
{
    const QDir sys(sysInputDirectory),devices(deviceDirectory);
    auto nodes=sys.entryList({"event*","js*"},QDir::Dirs|QDir::System|QDir::NoDotAndDotDot,QDir::Name);
    QStringList result;
    for(const auto &node:nodes){
        if(!QRegularExpression("^(event|js)[0-9]{1,3}$").match(node).hasMatch())continue;
        const auto capabilityPath=sys.filePath(node+"/device/capabilities/key");
        QFile capabilities(capabilityPath);
        if(!capabilities.open(QIODevice::ReadOnly))continue;
        const auto bitmap=QString::fromLatin1(capabilities.read(4097));
        if(bitmap.size()>4096 || !hasGamepadKeyCapabilities(bitmap.trimmed()))continue;
        const auto device=devices.filePath(node);
        const QFileInfo info(device);
        if(info.isSymLink() || !info.exists() || info.canonicalFilePath()!=QDir::cleanPath(device))continue;
        result.append(device);
        if(result.size()>32)break;
    }
    return result;
}
NativeSandboxPlan NativeSandbox::prepare(const QString &profileId, const QString &appId,
                                        const QString &executable, const QProcessEnvironment &host,
                                        const QString &target, const QString &storageRoot, const QString &emulatorRuntime,
                                        const QString &gameSourceRoot, const QString &steamGameId, const QString &managedGameRoot,
                                        const QString &opticalMediaId, const QString &opticalObservationId)
{
    NativeSandboxPlan plan;
#ifndef Q_OS_LINUX
    plan.error = QStringLiteral("Separate native app accounts currently require Linux. This app was not opened.");
    return plan;
#else
    if ((profileId != "default" && (QUuid(profileId).isNull()
         || QUuid(profileId).toString(QUuid::WithoutBraces) != profileId))
        || !QRegularExpression("^[a-zA-Z0-9][a-zA-Z0-9._-]{0,79}$").match(appId).hasMatch()) {
        plan.error = QStringLiteral("A valid profile and app identity are required for a separate app session.");
        return plan;
    }
    plan.program = QStandardPaths::findExecutable("bwrap");
    const QFileInfo applianceWrapper("/usr/lib/isolde-native/bwrap");
    if(applianceWrapper.exists() || applianceWrapper.isSymLink()){
        auto path=applianceWrapper;
        while(true){
            if(path.isSymLink() || path.ownerId()!=0 || (path.permissions() & (QFile::WriteGroup|QFile::WriteOther))){
                plan.error="Repair the root-owned Isolde native sandbox installation. Shared-account launch is disabled.";
                return plan;
            }
            if(path.absoluteFilePath()=="/")break;
            path=QFileInfo(path.absolutePath());
        }
        if(!applianceWrapper.isFile() || !applianceWrapper.isExecutable()){
            plan.error="The Isolde native sandbox executable is unavailable.";
            return plan;
        }
        plan.program=applianceWrapper.absoluteFilePath();
    }
    const auto dbus = QStandardPaths::findExecutable("dbus-run-session");
    const auto supervisor = QCoreApplication::applicationDirPath() + "/isolde-native-host";
    const auto busConfig = QCoreApplication::applicationDirPath() + "/config/native-session-bus.conf";
    if (plan.program.isEmpty() || dbus.isEmpty() || !QFileInfo(supervisor).isExecutable() || !QFileInfo(busConfig).isReadable()) {
        plan.error = QStringLiteral("Native profile isolation needs bubblewrap, dbus-run-session, and the Isolde native host installed. Shared-account launch is disabled.");
        return plan;
    }
    const auto binary = QFileInfo(executable).canonicalFilePath();
    const bool managedExecutable=!managedGameRoot.isEmpty()
        && gameSourceRoot==managedGameRoot
        && ManagedGamePaths::isInstallRoot(profileId,managedGameRoot)
        && inside(binary,managedGameRoot) && QFileInfo(binary).isExecutable();
    bool opticalExecutable=false;
    if(!opticalMediaId.isEmpty() && OpticalDiscGrants::approved(profileId,opticalMediaId)){
        for(const auto &item:OpticalDiscAdapter::mountedDiscs()){
            const auto disc=item.toMap();
            if(disc.value("mountRoot")==gameSourceRoot && disc.value("mediaId")==opticalMediaId
                && disc.value("observationId")==opticalObservationId && !opticalObservationId.isEmpty()
                && disc.value("directPlaySupported").toBool() && inside(binary,gameSourceRoot)
                && (QFileInfo(binary).permissions() & (QFile::ExeOwner|QFile::ExeGroup|QFile::ExeOther)))opticalExecutable=true;
        }
    }
    const bool nativeGame=managedExecutable || opticalExecutable;
    if(!opticalMediaId.isEmpty() && !opticalExecutable){
        plan.error="The approved optical disc changed or is unavailable. Reinsert it and retry.";return plan;
    }
    QFile nativeFile(binary);
    if(nativeGame && (target!=binary || executable!=binary || !nativeFile.open(QIODevice::ReadOnly)
        || nativeFile.read(4)!=QByteArray("\x7f""ELF",4))){
        plan.error="The native game executable changed or is not a Linux ELF file.";return plan;
    }
    if (binary.isEmpty() || (!inside(binary, "/usr") && !inside(binary, "/opt") && !nativeGame)) {
        plan.error = QStringLiteral("This app must be installed in a system application directory before it can run in a separate profile.");
        return plan;
    }
    if (QStringList{"flatpak", "snap", "gtk-launch", "gio", "xdg-open"}.contains(QFileInfo(binary).fileName())) {
        plan.error = QStringLiteral("This app launcher needs a dedicated profile adapter. Shared-account launch is disabled.");
        return plan;
    }

    const auto base = QDir::cleanPath(storageRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles"
        : storageRoot);
    if (!QDir::isAbsolutePath(base) || !privateDirectory(base)) {
        plan.error = QStringLiteral("Cannot create protected native app storage.");
        return plan;
    }
    const auto appRoot = base + '/' + profileId + '/' + appId;
    plan.homeDirectory = appRoot + "/home";
    for (const auto &path : QStringList{appRoot, plan.homeDirectory, plan.homeDirectory + "/.config",
             plan.homeDirectory + "/.local/share", plan.homeDirectory + "/.local/state", plan.homeDirectory + "/.cache"}) {
        if (!privateDirectory(path)) {
            plan.error = QStringLiteral("Cannot prepare this profile's private app folders.");
            return plan;
        }
    }
    const auto userHome = QDir::homePath();
    const auto runtime = QStringLiteral("/run/user/%1").arg(getuid());
    const auto hostRuntime = host.value("XDG_RUNTIME_DIR", runtime);
    auto &args = plan.arguments;
    args = {"--unshare-user", "--unshare-pid", "--unshare-ipc", "--unshare-uts",
            "--die-with-parent", "--new-session", "--cap-drop", "ALL", "--clearenv"};
    const auto bind = [&](const QString &source, const QString &destination, const QString &mode = "--ro-bind") {
        args << mode << source << destination;
    };
    for (const auto &path : QStringList{"/usr", "/bin", "/sbin", "/lib", "/lib64", "/etc", "/opt"})
        if (QFileInfo::exists(path)) bind(path, path);
    args << "--proc" << "/proc" << "--dev" << "/dev" << "--tmpfs" << "/tmp"
         << "--dir" << "/run" << "--perms" << "0700" << "--dir" << runtime;
    bind(plan.homeDirectory, userHome, "--bind");
    args << "--chdir" << userHome;
    const auto setenv = [&](const QString &key, const QString &value) { args << "--setenv" << key << value; };
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
    for (const auto &key : QStringList{"DISPLAY", "XDG_SESSION_TYPE"})
        if (host.contains(key)) setenv(key, host.value(key));
    const auto wayland = host.value("WAYLAND_DISPLAY");
    if (!wayland.isEmpty() && !wayland.contains('/') && QFileInfo::exists(hostRuntime + '/' + wayland)) {
        bind(hostRuntime + '/' + wayland, runtime + '/' + wayland);
        setenv("WAYLAND_DISPLAY", wayland);
    }
    if (host.contains("DISPLAY") && QFileInfo::exists("/tmp/.X11-unix")) bind("/tmp/.X11-unix", "/tmp/.X11-unix");
    const auto authority = host.value("XAUTHORITY");
    if (!authority.isEmpty() && QFileInfo(authority).isFile()) {
        bind(authority, "/run/isolde-xauthority");
        setenv("XAUTHORITY", "/run/isolde-xauthority");
    }
    if (QFileInfo::exists(hostRuntime + "/pulse/native")) {
        bind(hostRuntime + "/pulse/native", runtime + "/pulse/native");
        setenv("PULSE_SERVER", "unix:" + runtime + "/pulse/native");
    }
    const auto pulseCookie = userHome + "/.config/pulse/cookie";
    if (QFileInfo(pulseCookie).isFile()) {
        bind(pulseCookie, "/run/isolde-pulse-cookie");
        setenv("PULSE_COOKIE", "/run/isolde-pulse-cookie");
    }
    if (QFileInfo::exists(hostRuntime + "/pipewire-0")) bind(hostRuntime + "/pipewire-0", runtime + "/pipewire-0");
    if (QFileInfo::exists("/dev/dri")) bind("/dev/dri", "/dev/dri", "--dev-bind");
    if(isGameSession(emulatorRuntime,steamGameId,gameSourceRoot,managedGameRoot,opticalMediaId)){
        // Give game sessions gamepad nodes only. Do not expose keyboard, mouse
        // or the entire host input directory to an application session.
        const auto nodes=gamepadDeviceNodes();
        if(nodes.size()>32){plan.error="Too many gamepad devices. Disconnect unused devices and retry.";return plan;}
        for(const auto &device:nodes)bind(device,device,"--dev-bind");
        QStringList boundEventNodes;
        for(const auto &device:nodes){
            const auto name=QFileInfo(device).fileName();
            if(name.startsWith("event"))boundEventNodes.append(name);
        }
        setenv("ISOLDE_GAMEPAD_BOUND_EVENTS",boundEventNodes.join(','));
    }
    if (QFileInfo::exists("/sys")) bind("/sys", "/sys");
    // resolv.conf is often a symlink into /run, which otherwise stays private.
    const auto resolver = QFileInfo("/etc/resolv.conf").canonicalFilePath();
    if (resolver.startsWith("/run/")) bind(resolver, resolver);

    // Both foreground and managed Kodi use the same persisted grants, mounted
    // at their original paths so existing Kodi sources are not rewritten.
    if (appId == "kodi") {
        const auto grants = MediaGrants::folders(profileId);
        if (grants.size() > 32) { plan.error = "Too many saved Kodi media grants."; return plan; }
        for (const auto &path : grants) {
            const auto error = MediaGrants::validate(path, false);
            if (!error.isEmpty() || inside(base, path) || inside(path, base)) {
                plan.error = "Review Kodi media-folder access in Settings. " + error;
                return plan;
            }
            // Keep Kodi usable when a removable library drive is disconnected.
            if (QFileInfo(path).isDir()) bind(path, path);
        }
    }
    // Explicit file/folder actions expose only that target, read-only.
    plan.actionTarget = target;
    if (!target.isEmpty()) {
        const QUrl url(target);
        if (url.isLocalFile() || url.scheme().isEmpty()) {
            const auto requested = url.isLocalFile() ? url.toLocalFile() : target;
            const auto canonical = QFileInfo(requested).canonicalFilePath();
            const auto config = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            const auto data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            const bool managedTarget=!managedGameRoot.isEmpty()
                && ManagedGamePaths::isInstallRoot(profileId,managedGameRoot)
                && inside(canonical,managedGameRoot);
            const bool opticalTarget=opticalExecutable && canonical==binary;
            bool privateHomePath = false;
            if (inside(canonical, userHome)) {
                const auto components = QDir(userHome).relativeFilePath(canonical).split('/');
                for (const auto &component : components) privateHomePath |= component.startsWith('.');
            }
            bool systemPath = false;
            for (const auto &system : QStringList{"/etc", "/usr", "/opt", "/sys", "/dev", "/root", "/var", "/run", "/proc"})
                systemPath |= inside(canonical, system);
            if (canonical.isEmpty() || !QDir::isAbsolutePath(requested)
                || (!QFileInfo(canonical).isFile() && !QFileInfo(canonical).isDir())
                || (privateHomePath && !managedTarget && !opticalTarget) || (systemPath && !managedTarget && !opticalTarget)
                || (!managedTarget && (inside(base, canonical) || inside(canonical, base)))
                || (!managedTarget && (inside(config, canonical) || inside(canonical, config)
                    || inside(data, canonical) || inside(canonical, data)))
                ) {
                plan.error = QStringLiteral("Choose a media file or folder, not a profile, account, or system folder.");
                return plan;
            }
            if(!gameSourceRoot.isEmpty()){
                const auto error=MediaGrants::validate(gameSourceRoot);
                const bool managed=ManagedGamePaths::isInstallRoot(profileId,gameSourceRoot);
                bool optical=false;
                for(const auto &disc:OpticalDiscAdapter::mountedDiscs()){
                    const auto value=disc.toMap();
                    if(value.value("mountRoot")==gameSourceRoot && value.value("mediaId")==opticalMediaId
                        && value.value("observationId")==opticalObservationId && !opticalObservationId.isEmpty()
                        && value.value("directPlaySupported").toBool()
                        && OpticalDiscGrants::approved(profileId,opticalMediaId))optical=true;
                }
                if((!opticalMediaId.isEmpty() && !optical) || (!error.isEmpty() && !managed && !optical) || !QFileInfo(canonical).isFile() || !canonical.startsWith(gameSourceRoot+'/')
                    || (!managed && (inside(base,gameSourceRoot) || inside(gameSourceRoot,base)))){
                    plan.error="The reviewed game folder is unavailable or redirected. Review it in Games and retry.";return plan;
                }
                plan.actionTarget="/media/games/"+QDir(gameSourceRoot).relativeFilePath(canonical);
                bind(gameSourceRoot,"/media/games");
                if(managed || optical)args << "--chdir" << "/media/games";
            } else {
                plan.actionTarget = "/media/selected/" + QFileInfo(canonical).fileName();
                bind(canonical, plan.actionTarget);
            }
        } else if (!QStringList{"https", "http", "smb", "nfs"}.contains(url.scheme())) {
            plan.error = QStringLiteral("This media address is not supported by isolated native apps.");
            return plan;
        }
    }
    // Start a new bus inside the sandbox. Host service activation is never used.
    if(!steamGameId.isEmpty()){
        bool valid=false;const auto number=steamGameId.toULongLong(&valid);
        auto helperRoot=QCoreApplication::applicationDirPath()+"/share/isolde";
        if(!QFileInfo(helperRoot+"/steam-games.py").isReadable())helperRoot=QCoreApplication::applicationDirPath()+"/../share/isolde";
        const auto helper=helperRoot+"/steam-games.py";
        if(appId!="steam" || !QRegularExpression("^[1-9][0-9]{0,9}$").match(steamGameId).hasMatch() || !valid || number>4294967295ULL || QString::number(number)!=steamGameId
            || binary!=QFileInfo("/usr/bin/python3").canonicalFilePath() || !target.isEmpty() || !QFileInfo(helper).isReadable()){
            plan.error="Repair the private Steam game adapter or import this game again.";return plan;
        }
        bind(helper,"/run/steam-games.py");
    }
    if(appId=="steam"){
        const auto libraries=SteamLibraryGrants::folders(profileId);
        const auto grantsError=SteamLibraryGrants::validateList(libraries,false);
        if(!grantsError.isEmpty()){
            plan.error="Review external Steam library access in Games settings. "+grantsError;
            return plan;
        }
        QSet<QString> createdParents;
        for(const auto &library:libraries){
            if(!QFileInfo(library).isDir())continue;
            QString parent;
            for(const auto &part:library.split('/',Qt::SkipEmptyParts)){
                parent+="/"+part;
                if(parent==library)break;
                if(!createdParents.contains(parent)){
                    args << "--dir" << parent;
                    createdParents.insert(parent);
                }
            }
            bind(library,library);
            plan.steamLibraryRoots.append(library);
        }
    }
    bind(supervisor, "/run/isolde-native-host");
    bind(busConfig, "/run/isolde-session-bus.conf");
    if(!emulatorRuntime.isEmpty()){
        const auto runtimeDefinition=GameEmulators::definition(emulatorRuntime);
        auto helperRoot=QCoreApplication::applicationDirPath()+"/share/isolde";
        if(!QFileInfo(helperRoot+"/emulator-session.py").isReadable())helperRoot=QCoreApplication::applicationDirPath()+"/../share/isolde";
        const auto helper=helperRoot+"/emulator-session.py";
        const auto manifest=helperRoot+"/game-runtimes.json";
        if(runtimeDefinition.isEmpty() || emulatorRuntime!=appId || binary!=QFileInfo("/usr/bin/python3").canonicalFilePath()
            || !QFileInfo(helper).isReadable() || !QFileInfo(manifest).isReadable() || target.isEmpty()){
            plan.error="Repair the managed emulator adapter or choose a game from Games.";return plan;
        }
        bind(helper,"/run/emulator-session.py");bind(manifest,"/run/game-runtimes.json");
        // The packaged script accepts identities, never commands from a game.
    }
    bool opticalNoExec=false;
    if(opticalExecutable){
        struct statvfs fs{};
        const auto path=QFile::encodeName(binary);
        if(::statvfs(path.constData(),&fs)!=0){
            plan.error="Cannot inspect optical mount execution permissions. Refresh the disc and retry.";return plan;
        }
        opticalNoExec=fs.f_flag & ST_NOEXEC;
    }
    if(opticalNoExec){
        auto helperRoot=QCoreApplication::applicationDirPath()+"/share/isolde";
        if(!QFileInfo(helperRoot+"/optical-exec.py").isReadable())helperRoot=QCoreApplication::applicationDirPath()+"/../share/isolde";
        const auto helper=helperRoot+"/optical-exec.py";
        if(!QFileInfo(helper).isReadable() || !QFileInfo("/usr/bin/python3").isExecutable()){
            plan.error="The optical no-exec adapter is unavailable. Repair Isolde and retry.";return plan;
        }
        bind(helper,"/run/isolde-optical-exec.py");
    }
    args << "--" << dbus << "--config-file=/run/isolde-session-bus.conf" << "--" << "/run/isolde-native-host";
    if(isGameSession(emulatorRuntime,steamGameId,gameSourceRoot,managedGameRoot,opticalMediaId))
        args << "--game-session";
    if(!steamGameId.isEmpty()) args << "--steam-session";
    args << "--";
    if(opticalNoExec)args << "/usr/bin/python3" << "/run/isolde-optical-exec.py" << plan.actionTarget;
    else args << (nativeGame ? plan.actionTarget : executable);
    plan.environment.insert("PATH", "/usr/bin:/bin");
    plan.environment.insert("LANG", host.value("LANG", "C.UTF-8"));
    return plan;
#endif
}
