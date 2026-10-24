#include "sessions/NativeSandbox.h"
#include "media/MediaGrants.h"
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#ifdef Q_OS_LINUX
#include <unistd.h>
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

NativeSandboxPlan NativeSandbox::prepare(const QString &profileId, const QString &appId,
                                        const QString &executable, const QProcessEnvironment &host,
                                        const QString &target, const QString &storageRoot)
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
    if (binary.isEmpty() || (!inside(binary, "/usr") && !inside(binary, "/opt"))) {
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
                || privateHomePath || systemPath
                || inside(base, canonical) || inside(canonical, base)
                || inside(config, canonical) || inside(canonical, config)
                || inside(data, canonical) || inside(canonical, data)
                ) {
                plan.error = QStringLiteral("Choose a media file or folder, not a profile, account, or system folder.");
                return plan;
            }
            plan.actionTarget = "/media/selected/" + QFileInfo(canonical).fileName();
            bind(canonical, plan.actionTarget);
        } else if (!QStringList{"https", "http", "smb", "nfs"}.contains(url.scheme())) {
            plan.error = QStringLiteral("This media address is not supported by isolated native apps.");
            return plan;
        }
    }
    // Start a new bus inside the sandbox. Host service activation is never used.
    bind(supervisor, "/run/isolde-native-host");
    bind(busConfig, "/run/isolde-session-bus.conf");
    args << "--" << dbus << "--config-file=/run/isolde-session-bus.conf" << "--" << "/run/isolde-native-host" << "--" << executable;
    plan.environment.insert("PATH", "/usr/bin:/bin");
    plan.environment.insert("LANG", host.value("LANG", "C.UTF-8"));
    return plan;
#endif
}
