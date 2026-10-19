#include "KodiService.h"
#include "MediaGrants.h"
#include "sessions/NativeSandbox.h"
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

KodiService::KodiService(const QString &profile, QObject *parent) : QObject(parent), m_profile(profile), m_modeClient(profile)
{
    m_frontend = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi").value("profiles/" + profile + "/kodi/frontend", "isolde").toString();
    if (m_frontend != "kodi") m_frontend = "isolde";
    m_modeDeadline.setSingleShot(true);
    m_modeDeadline.setInterval(20000);
    connect(&m_modeDeadline, &QTimer::timeout, this, [this] {
        m_pendingFrontend.clear();
        m_status = "Kodi has not exited cleanly. Mode unchanged; no forced shutdown was used. Exit Kodi normally, then retry.";
        emit changed();
    });
    connect(&m_modeClient, &KodiMedia::quitReply, this, [this](bool accepted) {
        if (!switching() || accepted) return;
        m_modeDeadline.stop(); m_pendingFrontend.clear();
        m_status = "Could not switch safely. Stop playback, wait for scans, and retry. Mode unchanged.";
        emit changed();
    });
    m_enabled = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi").value("profiles/" + profile + "/kodi/autoStart", true).toBool();
    if (!m_enabled) m_status = "Automatic startup is off.";
    m_probe.setInterval(1000);
    connect(&m_probe, &QTimer::timeout, this, [this] {
        if (++m_checks > 30) { fail("Kodi did not become ready. Retry or open Kodi directly."); return; }
        emit probeRequested();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] { m_process.readAllStandardOutput(); });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_stopping && error == QProcess::FailedToStart) fail("Kodi could not start. Check the installation and retry.");
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus exitStatus) {
        m_probe.stop();
        m_ready = false;
        if (switching()) {
            const auto requested = m_pendingFrontend;
            m_pendingFrontend.clear(); m_modeDeadline.stop();
            m_lock.reset(); m_overlay.reset();
            emit unavailable("Kodi closed for frontend switching.");
            if (code == 0 && exitStatus == QProcess::NormalExit) commitFrontend(requested);
            else { m_status = "Kodi did not exit normally. Mode unchanged."; emit changed(); }
            return;
        }
        if (!m_stopping) {
            m_status = "Kodi stopped. Retry to start it again.";
            emit unavailable(m_status);
            emit changed();
        }
        m_lock.reset();
    });
}
KodiService::~KodiService() { stop(); }
QStringList KodiService::libraryFolders() const { return MediaGrants::folders(m_profile); }
void KodiService::browseLibraryFolder(const QString &path)
{
    const QStringList roots{QDir::homePath(), "/media", "/mnt"};
    if (path.isEmpty()) { m_browsePath.clear(); m_folderChoices = roots; emit changed(); return; }
    bool allowed = false;
    for (const auto &root : roots) allowed |= path == root || path.startsWith(root + '/');
    if (!allowed || QFileInfo(path).canonicalFilePath() != path || !QFileInfo(path).isDir()
        || (!roots.contains(path) && !MediaGrants::validate(path).isEmpty())) {
        m_folderStatus = "This folder cannot be browsed. Choose a local media folder or connect its drive.";
        emit changed(); return;
    }
    m_browsePath = path;
    m_folderChoices.clear();
    const auto entries = QDir(path).entryInfoList(QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);
    for (const auto &entry : entries) {
        if (MediaGrants::validate(entry.absoluteFilePath()).isEmpty()) m_folderChoices.append(entry.absoluteFilePath());
        if (m_folderChoices.size() >= 256) break;
    }
    m_folderStatus = entries.size() > 256 ? "Showing up to 256 folders. You can grant the current collection root, including its subfolders." : QString();
    emit changed();
}
void KodiService::browseLibraryParent()
{
    if (QStringList{QDir::homePath(), "/media", "/mnt"}.contains(m_browsePath)) browseLibraryFolder({});
    else browseLibraryFolder(QFileInfo(m_browsePath).absolutePath());
}
void KodiService::allowBrowsedFolder() { addLibraryFolder(QUrl::fromLocalFile(m_browsePath)); }
void KodiService::addLibraryFolder(const QUrl &folder)
{
    if (!canEditFolders()) { m_folderStatus = "Switch to Kodi application mode and exit Kodi normally before changing folder access."; emit changed(); return; }
    const auto path = QDir::cleanPath(folder.toLocalFile());
    const auto error = folder.isLocalFile() ? MediaGrants::validate(path) : QString("Choose a local folder.");
    if (!error.isEmpty()) { m_folderStatus = error; emit changed(); return; }
    auto folders = libraryFolders();
    if (folders.contains(path)) { m_folderStatus = "This folder is already allowed."; emit changed(); return; }
    folders.append(path);
    const auto saved = MediaGrants::save(m_profile, folders);
    m_folderStatus = saved.isEmpty() ? "Read-only access saved. Open Kodi, add this same folder as a video source, and set its content to Movies with subfolder scanning. Existing sources need no remapping." : saved;
    emit changed();
}
void KodiService::removeLibraryFolder(const QString &folder)
{
    if (!canEditFolders()) { m_folderStatus = "Exit Kodi and switch to Kodi application mode before changing access."; emit changed(); return; }
    auto folders = libraryFolders();
    if (!folders.removeAll(folder)) return;
    const auto saved = MediaGrants::save(m_profile, folders);
    m_folderStatus = saved.isEmpty() ? "Folder permission removed. No media files or Kodi library entries were deleted." : saved;
    emit changed();
}
void KodiService::commitFrontend(const QString &frontend)
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi");
    settings.setValue("profiles/" + m_profile + "/kodi/frontend", frontend);
    settings.sync();
    if (settings.status() != QSettings::NoError) { m_status = "Could not save frontend preference."; emit changed(); return; }
    m_frontend = frontend;
    m_status = frontend == "kodi" ? "Kodi application mode. Open Kodi to use your existing skin and configuration."
                                  : "Isolde frontend selected. Kodi configuration is unchanged.";
    emit changed();
    if (frontend == "isolde") start();
}
void KodiService::setFrontend(const QString &frontend)
{
    if ((frontend != "isolde" && frontend != "kodi") || frontend == m_frontend || switching()) return;
    if (m_suspended) { m_status = "Exit Kodi normally before changing frontend."; emit changed(); return; }
    if (m_process.state() == QProcess::NotRunning) { commitFrontend(frontend); return; }
    if (!m_ready) { m_status = "Wait for Kodi to become ready before changing frontend."; emit changed(); return; }
    m_pendingFrontend = frontend;
    m_status = "Waiting for Kodi to save and exit. Playback or scans prevent switching.";
    m_modeDeadline.start();
    emit changed();
    m_modeClient.quitIfIdle();
}
void KodiService::stop()
{
    m_modeDeadline.stop(); m_pendingFrontend.clear();
    m_stopping = true;
    m_probe.stop();
    m_ready = false;
    emit unavailable("Kodi is stopped. Start it from Settings → System → Media Library.");
    if (m_process.state() != QProcess::NotRunning) {
        m_process.write("quit\n");
        if (!m_process.waitForFinished(4000)) { m_process.kill(); m_process.waitForFinished(3000); }
    }
    m_lock.reset();
    m_overlay.reset();
    m_stopping = false;
}
void KodiService::fail(const QString &reason) { stop(); m_status = reason; emit unavailable(reason); emit changed(); }
void KodiService::setEnabled(bool enabled)
{
    if (switching()) return;
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi");
    settings.setValue("profiles/" + m_profile + "/kodi/autoStart", enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) { m_status = "Could not save Kodi startup preference."; emit changed(); return; }
    m_enabled = enabled;
    if (enabled) start();
    else { stop(); m_status = "Automatic startup is off."; }
    emit changed();
}
void KodiService::suspend(bool suspended)
{
    m_suspended = suspended;
    if (suspended) { stop(); m_status = "Kodi is open as an app."; emit changed(); }
    else start();
}
void KodiService::retry() { if (!switching()) { stop(); start(); } }
void KodiService::markReady()
{
    if (m_ready || m_process.state() != QProcess::Running) return;
    m_probe.stop(); m_ready = true;
    m_status = "Running in the background for this profile.";
    emit changed(); emit ready();
}
void KodiService::start()
{
    if (m_frontend == "kodi") { m_status = "Kodi application mode. Background startup is paused; your setup is unchanged."; emit changed(); return; }
    if (!m_enabled || m_suspended || switching() || m_process.state() != QProcess::NotRunning) return;
    const auto executable = QStandardPaths::findExecutable("kodi");
    if (executable.isEmpty()) { m_status = "Install Kodi from the App Store to use the media library."; emit changed(); return; }
    auto environment = QProcessEnvironment::systemEnvironment();
    const QDir runtime(environment.value("XDG_RUNTIME_DIR"));
    const auto auth = runtime.entryList({".mutter-Xwaylandauth.*"}, QDir::Files | QDir::Hidden, QDir::Time);
    if (!auth.isEmpty()) environment.insert("XAUTHORITY", runtime.filePath(auth.first()));
    auto plan = NativeSandbox::prepare(m_profile, "kodi", executable, environment);
    if (!plan.error.isEmpty()) { fail(plan.error); return; }
    // Managed foreground activation currently uses EWMH. Do not change native
    // application-mode launch preferences or persistent Kodi configuration.
    if (!environment.value("DISPLAY").isEmpty() && !QStandardPaths::findExecutable("wmctrl").isEmpty())
        plan.arguments.append(QStringList{"--windowing=x11", "-fs"});
    m_lock = std::make_unique<QLockFile>(plan.homeDirectory + "/.isolde-managed.lock");
    if (!m_lock->tryLock()) { fail("Kodi is already managed by another Isolde instance."); return; }
    const auto applicationDir = QCoreApplication::applicationDirPath();
    auto addon = applicationDir + "/../share/isolde/kodi/service.isolde.bridge";
    if (!QFileInfo::exists(addon + "/addon.xml")) addon = applicationDir + "/share/isolde/kodi/service.isolde.bridge";
    QFile original("/usr/share/kodi/system/addon-manifest.xml");
    if (!original.open(QIODevice::ReadOnly)) { fail("This Kodi installation needs a supported system add-on manifest."); return; }
    auto manifest = original.readAll();
    if (!manifest.contains("</addons>")) { fail("Kodi's add-on manifest is invalid."); return; }
    m_overlay = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/isolde-kodi-XXXXXX");
    if (!m_overlay->isValid()) { fail("Cannot create Kodi startup files."); return; }
    const QString runtimeId = "service.isolde.bridge.runtime";
    const auto addonsOverlay = m_overlay->path() + "/addons";
    if (!QDir().mkpath(addonsOverlay)) { fail("Cannot create Kodi startup files."); return; }
    // Preserve every distro add-on without writing beneath the read-only /usr.
    // The original directory is mounted under /run inside this namespace only.
    for (const auto &entry : QDir("/usr/share/kodi/addons").entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (entry == runtimeId) { fail("A conflicting Kodi bridge is already installed."); return; }
        if (!QFile::link("/run/isolde-original-addons/" + entry, addonsOverlay + '/' + entry)) {
            fail("Cannot prepare Kodi's bundled add-ons."); return;
        }
    }
    const auto destination = addonsOverlay + '/' + runtimeId;
    if (!QDir().mkpath(destination)) { fail("Cannot create Kodi startup files."); return; }
    for (const auto &name : QStringList{"addon.xml", "service.py", "bridge.py", "playback.py", "artwork.py"}) {
        QFile source(addon + '/' + name), target(destination + '/' + name);
        if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) { fail("The Isolde Kodi bridge is missing. Reinstall Isolde."); return; }
        auto data = source.readAll();
        if (name == "addon.xml") data.replace("id=\"service.isolde.bridge\"", "id=\"service.isolde.bridge.runtime\"");
        if (target.write(data) != data.size()) { fail("Could not prepare the Kodi bridge."); return; }
    }
    manifest.replace("</addons>", "  <addon>service.isolde.bridge.runtime</addon>\n</addons>");
    QFile overlayManifest(m_overlay->path() + "/addon-manifest.xml");
    if (!overlayManifest.open(QIODevice::WriteOnly) || overlayManifest.write(manifest) != manifest.size()) { fail("Could not prepare Kodi's runtime manifest."); return; }
    overlayManifest.close();
    const int separator = plan.arguments.indexOf("--");
    if (separator < 0) { fail("Invalid sandbox configuration."); return; }
    const auto originalKeys = plan.homeDirectory + "/.kodi/userdata/keymaps";
    const auto runtimeKeys = m_overlay->path() + "/keymaps";
    if (!QDir().mkpath(originalKeys) || !QDir().mkpath(runtimeKeys)) { fail("Cannot prepare managed playback keys."); return; }
    const QString keyName = "zzzz-isolde-managed.xml";
    for (const auto &entry : QDir(originalKeys).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (entry == keyName || !QFile::link("/run/isolde-original-keymaps/" + entry, runtimeKeys + '/' + entry)) {
            fail("Cannot preserve Kodi keymaps for managed playback."); return;
        }
    }
    QFile keys(runtimeKeys + '/' + keyName);
    const QByteArray keyData = "<keymap><global><keyboard><home>NotifyAll(Isolde,home)</home><h>NotifyAll(Isolde,home)</h></keyboard></global>"
        "<FullscreenVideo><keyboard><escape>NotifyAll(Isolde,stop)</escape><backspace>NotifyAll(Isolde,stop)</backspace>"
        "<return>NotifyAll(Isolde,pause)</return><space>NotifyAll(Isolde,pause)</space>"
        "<left>NotifyAll(Isolde,backward)</left><right>NotifyAll(Isolde,forward)</right>"
        "</keyboard></FullscreenVideo></keymap>";
    if (!keys.open(QIODevice::WriteOnly) || keys.write(keyData) != keyData.size()) { fail("Cannot prepare managed playback keys."); return; }
    keys.close();
    const QStringList extra{"--ro-bind", "/usr/share/kodi/addons", "/run/isolde-original-addons",
        "--ro-bind", addonsOverlay, "/usr/share/kodi/addons",
        "--ro-bind", overlayManifest.fileName(), "/usr/share/kodi/system/addon-manifest.xml",
        "--ro-bind", originalKeys, "/run/isolde-original-keymaps",
        "--ro-bind", runtimeKeys, QDir::homePath() + "/.kodi/userdata/keymaps",
        "--setenv", "ISOLDE_MANAGED_KODI", "1"};
    for (int i = 0; i < extra.size(); ++i) plan.arguments.insert(separator + i, extra[i]);
    m_process.setProcessEnvironment(plan.environment);
    m_process.start(plan.program, plan.arguments);
    m_checks = 0; m_ready = false;
    m_status = "Starting local Kodi…";
    m_probe.start();
    emit changed();
}
