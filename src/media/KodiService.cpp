#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include "KodiService.h"
#include "MediaGrants.h"
#include "sessions/NativeSandbox.h"

namespace
{
QString
managedKodiExecutable()
{
#ifdef Q_OS_LINUX
    const QString root = "/opt/isolde-managed-kodi";
    QFile manifest(root + "/manifest.json");
    QFile binary(root + "/kodi.bin");
    for (const auto& path : QStringList{root, manifest.fileName(), binary.fileName(), root + "/bin/kodi"})
    {
        const QFileInfo info(path);
        if (info.isSymLink() || info.ownerId() != 0 ||
            (info.permissions() & (QFileDevice::WriteGroup | QFileDevice::WriteOther)))
            return {};
    }
    if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 8192 || !binary.open(QIODevice::ReadOnly))
        return {};
    const auto metadata = QJsonDocument::fromJson(manifest.readAll()).object();
    if (metadata["feature"] != "isolde-sources-v1" || metadata["sourceApiBaseline"] != "21.2-Omega")
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&binary) || metadata["binarySha256"] != QString::fromLatin1(hash.result().toHex()))
        return {};
    return QFileInfo(root + "/bin/kodi").isExecutable() ? root + "/bin/kodi" : QString{};
#else
    return {};
#endif
}
} // namespace

QVariantList
KodiService::capabilityInventory() const
{
    static const bool resourcesReady = []
    {
        Q_INIT_RESOURCE(kodi_capabilities);
        return true;
    }();
    Q_UNUSED(resourcesReady);
    QFile file(":/config/kodi-capabilities.json");
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
        return {};
    const auto document = QJsonDocument::fromJson(file.readAll()).object();
    if (document["schemaVersion"].toInt() != 1 || !document["items"].isArray())
        return {};
    const auto items = document["items"].toArray();
    if (items.size() > 64)
        return {};
    QSet<QString> ids;
    for (const auto& item : items)
    {
        const auto row = item.toObject();
        if (!isValidCapabilityItem(row))
            return {};
        ids.insert(row["id"].toString());
    }
    return items.toVariantList();
}

static bool
isValidCapabilityItem(const QJsonObject& row)
{
    const auto id = row["id"].toString();
    if (row.size() != 4 || id.isEmpty() || id.size() > 64 || row["name"].toString().isEmpty() ||
        row["name"].toString().size() > 160 || row["description"].toString().isEmpty() ||
        row["description"].toString().size() > 2048 ||
        !QStringList{"sources", "folders", "videoscan", "musicscan", "playback", "import", "restart", "none"}
             .contains(row["route"].toString()))
        return false;
    return true;
}

QVariantList
KodiService::libraryViews() const
{
    if (capabilityInventory().isEmpty())
        return {};
    QFile file(":/config/kodi-capabilities.json");
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
        return {};
    const auto value = QJsonDocument::fromJson(file.readAll()).object().value("libraryViews");
    if (!value.isArray() || value.toArray().isEmpty() || value.toArray().size() > 16)
        return {};
    QSet<QString> kinds;
    for (const auto& item : value.toArray())
    {
        const auto row = item.toObject();
        const auto kind = row["kind"].toString();
        if (row.size() != 3 || !QStringList{"movies", "shows", "albums"}.contains(kind) || kinds.contains(kind) ||
            row["name"].toString().isEmpty() || row["name"].toString().size() > 160 ||
            !QStringList{"primary", "trailing"}.contains(row["position"].toString()))
            return {};
        kinds.insert(kind);
    }
    return value.toArray().toVariantList();
}

KodiService::KodiService(const QString& profile, QObject* parent)
    : QObject(parent), m_profile(profile), m_modeClient(profile)
{
    m_frontend = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                     .value("profiles/" + profile + "/kodi/frontend", "isolde")
                     .toString();
    if (m_frontend != "kodi")
        m_frontend = "isolde";
    m_modeDeadline.setSingleShot(true);
    m_modeDeadline.setInterval(20000);
    connect(&m_modeDeadline,
            &QTimer::timeout,
            this,
            [this]
            {
                if (m_pendingFrontend == "folders")
                    m_folderStatus =
                        "Kodi has not finished saving. Folder permissions are unchanged; stop playback/scanning "
                        "and press Menu to retry.";
                m_pendingFrontend.clear();
                m_status =
                    "Kodi has not exited cleanly. Mode unchanged; no forced shutdown was used. Exit Kodi normally, "
                    "then retry.";
                emit changed();
            });
    connect(&m_modeClient,
            &KodiMedia::quitReply,
            this,
            [this](bool accepted)
            {
                if (!switching() || accepted)
                    return;
                if (m_pendingFrontend == "folders")
                    m_folderStatus =
                        "Stop playback and wait for scans to finish, then press Menu to retry. Folder permissions "
                        "are unchanged.";
                m_modeDeadline.stop();
                m_pendingFrontend.clear();
                m_status = "Could not switch safely. Stop playback, wait for scans, and retry. Mode unchanged.";
                emit changed();
            });
    m_enabled = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                    .value("profiles/" + profile + "/kodi/autoStart", true)
                    .toBool();
    m_alAlwaysPlayLocally = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                                .value("profiles/" + profile + "/kodi/alwaysPlayLocally", true)
                                .toBool();
    if (!m_enabled)
        m_status = "Automatic startup is off.";
    m_probe.setInterval(1000);
    connect(&m_probe,
            &QTimer::timeout,
            this,
            [this]
            {
                if (++m_checks > 30)
                {
                    fail("Kodi did not become ready. Retry or open Kodi directly.");
                    return;
                }
                emit probeRequested();
            });
    connect(&m_process,
            &QProcess::readyReadStandardOutput,
            this,
            [this]
            {
                m_process.readAllStandardOutput();
            });
    connect(&m_process,
            &QProcess::readyReadStandardError,
            this,
            [this]
            {
                m_process.readAllStandardError();
            });
    connect(&m_process,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError error)
            {
                if (!m_stopping && error == QProcess::FailedToStart)
                    fail("Kodi could not start. Check the installation and retry.");
            });
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus exitStatus)
            {
                m_probe.stop();
                m_ready = false;
                if (switching())
                {
                    const auto requested = m_pendingFrontend;
                    m_pendingFrontend.clear();
                    m_modeDeadline.stop();
                    m_lock.reset();
                    m_overlay.reset();
                    emit unavailable("Kodi closed for frontend switching.");
                    if (requested == "folders")
                    {
                        if (code == 0 && exitStatus == QProcess::NormalExit)
                        {
                            m_folderEditing = !m_cancelFolderPreparing;
                            m_folderStatus =
                                m_folderEditing
                                    ? "Ready · Kodi saved and paused safely. Folder access changes stay read-only."
                                    : "Folder editing cancelled. No permissions changed.";
                            if (m_cancelFolderPreparing && m_resumeAfterFolders && m_enabled)
                                start();
                        }
                        else
                            m_folderStatus =
                                "Kodi did not exit cleanly. Folder permissions are unchanged; retry when ready.";
                        emit changed();
                        return;
                    }
                    if (code == 0 && exitStatus == QProcess::NormalExit)
                        commitFrontend(requested);
                    else
                    {
                        m_status = "Kodi did not exit normally. Mode unchanged.";
                        emit changed();
                    }
                    return;
                }
                if (!m_stopping)
                {
                    m_status = "Kodi stopped. Retry to start it again.";
                    emit unavailable(m_status);
                    emit changed();
                }
                m_lock.reset();
            });
}
KodiService::~KodiService()
{
    stop();
}
QStringList
KodiService::libraryFolders() const
{
    return MediaGrants::folders(m_profile);
}
bool
KodiService::beginFolderEditing()
{
    if (m_folderEditing)
        return true;
    if (m_pendingFrontend == "folders")
        return true;
    if (m_suspended || switching())
    {
        m_folderStatus = "Close the active Kodi app or wait for the current operation, then press Menu to retry.";
        emit changed();
        return false;
    }
    m_resumeAfterFolders = m_enabled && m_frontend == "isolde";
    m_cancelFolderPreparing = false;
    if (m_process.state() == QProcess::NotRunning)
    {
        m_folderEditing = true;
        m_folderStatus = "Ready · folder access changes are read-only.";
        emit changed();
        return true;
    }
    if (!m_ready)
    {
        m_folderStatus = "Wait for the library to become ready, then press Menu to retry.";
        emit changed();
        return false;
    }
    m_pendingFrontend = "folders";
    m_folderStatus = "Waiting for Kodi to save and pause safely. Playback and scans prevent folder changes.";
    m_modeDeadline.start();
    emit changed();
    m_modeClient.quitIfIdle();
    return true;
}
void
KodiService::endFolderEditing()
{
    if (m_pendingFrontend == "folders")
    {
        m_cancelFolderPreparing = true;
        return;
    }
    if (!m_folderEditing)
        return;
    m_folderEditing = false;
    const bool resume = m_resumeAfterFolders;
    m_resumeAfterFolders = false;
    if (resume && m_enabled && !m_suspended)
        start();
    emit changed();
}
void
KodiService::browseLibraryFolder(const QString& path)
{
    const QStringList roots{QDir::homePath(), "/media", "/mnt"};
    if (path.isEmpty())
    {
        m_browsePath.clear();
        m_folderChoices = roots;
        emit changed();
        return;
    }
    bool allowed = false;
    for (const auto& root : roots)
        allowed |= path == root || path.startsWith(root + '/');
    if (!allowed || QFileInfo(path).canonicalFilePath() != path || !QFileInfo(path).isDir() ||
        (!roots.contains(path) && !MediaGrants::validate(path).isEmpty()))
    {
        m_folderStatus = "This folder cannot be browsed. Choose a local media folder or connect its drive.";
        emit changed();
        return;
    }
    m_browsePath = path;
    m_folderChoices.clear();
    const auto entries = QDir(path).entryInfoList(QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                                                  QDir::Name | QDir::IgnoreCase);
    for (const auto& entry : entries)
    {
        if (MediaGrants::validate(entry.absoluteFilePath()).isEmpty())
            m_folderChoices.append(entry.absoluteFilePath());
        if (m_folderChoices.size() >= 256)
            break;
    }
    m_folderStatus =
        entries.size() > 256
            ? "Showing up to 256 folders. You can grant the current collection root, including its subfolders."
            : QString();
    emit changed();
}
void
KodiService::browseLibraryParent()
{
    if (QStringList{QDir::homePath(), "/media", "/mnt"}.contains(m_browsePath))
        browseLibraryFolder({});
    else
        browseLibraryFolder(QFileInfo(m_browsePath).absolutePath());
}
void
KodiService::allowBrowsedFolder()
{
    addLibraryFolder(QUrl::fromLocalFile(m_browsePath));
}
void
KodiService::addLibraryFolder(const QUrl& folder)
{
    if (!canEditFolders())
    {
        m_folderStatus =
            "Wait for safe folder editing. Stop playback/scanning, close any Kodi app and press Menu to retry.";
        emit changed();
        return;
    }
    const auto path = QDir::cleanPath(folder.toLocalFile());
    const auto error = folder.isLocalFile() ? MediaGrants::validate(path) : QString("Choose a local folder.");
    if (!error.isEmpty())
    {
        m_folderStatus = error;
        emit changed();
        return;
    }
    auto folders = libraryFolders();
    if (folders.contains(path))
    {
        m_folderStatus = "This folder is already allowed.";
        emit changed();
        return;
    }
    folders.append(path);
    const auto saved = MediaGrants::save(m_profile, folders);
    m_folderStatus = saved.isEmpty() ? "Read-only access saved. Kodi reconnects when you leave. Choose Source setup to "
                                       "configure this folder’s content and metadata provider."
                                     : saved;
    emit changed();
}
void
KodiService::removeLibraryFolder(const QString& folder)
{
    if (!canEditFolders())
    {
        m_folderStatus =
            "Wait for safe folder editing. Stop playback/scanning, close any Kodi app and press Menu to retry.";
        emit changed();
        return;
    }
    auto folders = libraryFolders();
    if (!folders.removeAll(folder))
        return;
    const auto saved = MediaGrants::save(m_profile, folders);
    m_folderStatus =
        saved.isEmpty() ? "Folder permission removed. No media files or Kodi library entries were deleted." : saved;
    emit changed();
}
void
KodiService::commitFrontend(const QString& frontend)
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi");
    settings.setValue("profiles/" + m_profile + "/kodi/frontend", frontend);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        m_status = "Could not save frontend preference.";
        emit changed();
        return;
    }
    m_frontend = frontend;
    m_status = frontend == "kodi" ? "Kodi application mode. Open Kodi to use your existing skin and configuration."
                                  : "Isolde frontend selected. Kodi configuration is unchanged.";
    emit changed();
    if (frontend == "isolde")
        start();
}
void
KodiService::setFrontend(const QString& frontend)
{
    if ((frontend != "isolde" && frontend != "kodi") || frontend == m_frontend || switching())
        return;
    if (m_suspended)
    {
        m_status = "Exit Kodi normally before changing frontend.";
        emit changed();
        return;
    }
    if (m_process.state() == QProcess::NotRunning)
    {
        commitFrontend(frontend);
        return;
    }
    if (!m_ready)
    {
        m_status = "Wait for Kodi to become ready before changing frontend.";
        emit changed();
        return;
    }
    m_pendingFrontend = frontend;
    m_status = "Waiting for Kodi to save and exit. Playback or scans prevent switching.";
    m_modeDeadline.start();
    emit changed();
    m_modeClient.quitIfIdle();
}
void
KodiService::stop()
{
    m_modeDeadline.stop();
    m_pendingFrontend.clear();
    m_stopping = true;
    m_probe.stop();
    m_ready = false;
    emit unavailable("Kodi is stopped. Start it from Settings → System → Media Library.");
    if (m_process.state() != QProcess::NotRunning)
    {
        m_process.write("quit\n");
        if (!m_process.waitForFinished(4000))
        {
            m_process.kill();
            m_process.waitForFinished(3000);
        }
    }
    m_lock.reset();
    m_overlay.reset();
    m_stopping = false;
}
void
KodiService::fail(const QString& reason)
{
    stop();
    m_status = reason;
    emit unavailable(reason);
    emit changed();
}
void
KodiService::setEnabled(bool enabled)
{
    if (switching())
        return;
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi");
    settings.setValue("profiles/" + m_profile + "/kodi/autoStart", enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        m_status = "Could not save Kodi startup preference.";
        emit changed();
        return;
    }
    m_enabled = enabled;
    if (enabled)
        start();
    else
    {
        stop();
        m_status = "Automatic startup is off.";
    }
    emit changed();
}
void
KodiService::suspend(bool suspended)
{
    m_suspended = suspended;
    if (suspended)
    {
        stop();
        m_status = "Kodi is open as an app.";
        emit changed();
    }
    else
        start();
}
void
KodiService::retry()
{
    if (!switching())
    {
        stop();
        start();
    }
}
void
KodiService::setAlwaysPlayLocally(bool value)
{
    if (m_alAlwaysPlayLocally == value)
        return;
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi");
    settings.beginGroup(QStringLiteral("profiles/") + m_profile + QStringLiteral("/kodi"));
    settings.setValue(QStringLiteral("alwaysPlayLocally"), value);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        settings.setValue("profiles/" + m_profile + "/kodi/alwaysPlayLocally", m_alAlwaysPlayLocally);
        settings.sync();
        m_status = "Could not save Always Play Locally preference. Previous selection retained.";
        emit changed();
        return;
    }
    m_alAlwaysPlayLocally = value;
    m_status = "Playback preference saved.";
    emit alwaysPlayLocallyChanged();
    emit changed();
}

void
KodiService::resetAlwaysPlayLocally()
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi");
    settings.beginGroup(QStringLiteral("profiles/") + m_profile + QStringLiteral("/kodi"));
    settings.remove(QStringLiteral("alwaysPlayLocally"));
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        settings.setValue("profiles/" + m_profile + "/kodi/alwaysPlayLocally", m_alAlwaysPlayLocally);
        settings.sync();
        m_status = "Could not reset Always Play Locally preference. Previous selection retained.";
        emit changed();
        return;
    }
    m_alAlwaysPlayLocally = true;
    m_status = "Playback preference reset to default.";
    emit alwaysPlayLocallyChanged();
    emit changed();
}

void
KodiService::markReady()
{
    if (m_ready || m_process.state() != QProcess::Running)
        return;
    m_probe.stop();
    m_ready = true;
    m_status = "Running in the background for this profile.";
    emit changed();
    emit ready();
}
void
KodiService::start()
{
    if (m_frontend == "kodi")
    {
        m_status = "Kodi application mode. Background startup is paused; your setup is unchanged.";
        emit changed();
        return;
    }
    if (!m_enabled || m_suspended || m_folderEditing || switching() || m_process.state() != QProcess::NotRunning)
        return;
    QString executable;
    QProcessEnvironment environment;
    NativeSandboxPlan plan;
    if (!prepareKodiEnvironment(executable, environment, plan))
    {
        m_status = "Install Kodi from the App Store to use the media library.";
        emit changed();
        return;
    }
    if (!environment.value("DISPLAY").isEmpty() && !QStandardPaths::findExecutable("wmctrl").isEmpty())
        plan.arguments.append(QStringList{"--windowing=x11", "-fs"});
    m_lock = std::make_unique<QLockFile>(plan.homeDirectory + "/.isolde-managed.lock");
    if (!m_lock->tryLock())
    {
        fail("Kodi is already managed by another Isolde instance.");
        return;
    }
    QByteArray manifest;
    QString runtimeId, addonsOverlay;
    if (!initializeKodiBridge(manifest, runtimeId, addonsOverlay))
    {
        fail("Cannot prepare Kodi startup files.");
        return;
    }
    const int separator = plan.arguments.indexOf("--");
    if (separator < 0)
    {
        fail("Invalid sandbox configuration.");
        return;
    }
    const auto originalKeys = plan.homeDirectory + "/.kodi/userdata/keymaps";
    const auto runtimeKeys = m_overlay->path() + "/keymaps";
    if (!setupKeymaps(originalKeys, runtimeKeys))
    {
        fail("Cannot preserve Kodi keymaps for managed playback.");
        return;
    }
    const auto extra = buildSandboxExtras(addonsOverlay, m_overlay->path(), libraryFolders());
    for (int i = 0; i < extra.size(); ++i)
        plan.arguments.insert(separator + i, extra[i]);
    m_process.setProcessEnvironment(plan.environment);
    m_process.start(plan.program, plan.arguments);
    m_checks = 0;
    m_ready = false;
    m_status = "Starting local Kodi�";
    m_probe.start();
    emit changed();
}

bool
KodiService::resolveKodiExecutable(QString& executable)
{
    executable = managedKodiExecutable();
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable("kodi");
    return !executable.isEmpty();
}

bool
KodiService::setupSandboxEnvironment(QProcessEnvironment& environment)
{
    environment = QProcessEnvironment::systemEnvironment();
    const QDir runtime(environment.value("XDG_RUNTIME_DIR"));
    const auto auth = runtime.entryList({".mutter-Xwaylandauth.*"}, QDir::Files | QDir::Hidden, QDir::Time);
    if (!auth.isEmpty())
        environment.insert("XAUTHORITY", runtime.filePath(auth.first()));
    return true;
}

bool
KodiService::prepareKodiEnvironment(QString& executable, QProcessEnvironment& environment, NativeSandboxPlan& plan)
{
    executable = managedKodiExecutable();
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable("kodi");
    if (executable.isEmpty())
        return false;
    environment = QProcessEnvironment::systemEnvironment();
    const QDir runtime(environment.value("XDG_RUNTIME_DIR"));
    const auto auth = runtime.entryList({".mutter-Xwaylandauth.*"}, QDir::Files | QDir::Hidden, QDir::Time);
    if (!auth.isEmpty())
        environment.insert("XAUTHORITY", runtime.filePath(auth.first()));
    plan = NativeSandbox::prepare(m_profile, "kodi", executable, environment);
    return plan.error.isEmpty();
}

bool
KodiService::resolveBridgeAddon(QString& addon)
{
    const auto applicationDir = QCoreApplication::applicationDirPath();
    addon = applicationDir + "/../share/isolde/kodi/service.isolde.bridge";
    if (!QFileInfo::exists(addon + "/addon.xml"))
        addon = applicationDir + "/share/isolde/kodi/service.isolde.bridge";
    return QFileInfo::exists(addon + "/addon.xml");
}

bool
KodiService::validateKodiManifest(QByteArray& manifest)
{
    QFile original("/usr/share/kodi/system/addon-manifest.xml");
    if (!original.open(QIODevice::ReadOnly))
        return false;
    manifest = original.readAll();
    return manifest.contains("</addons>");
}

bool
KodiService::initializeKodiBridge(QByteArray& manifest, QString& runtimeId, QString& addonsOverlay)
{
    QString addon;
    if (!resolveBridgeAddon(addon))
        return false;
    if (!validateKodiManifest(manifest))
        return false;
    m_overlay = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/isolde-kodi-XXXXXX");
    if (!m_overlay->isValid())
        return false;
    runtimeId = "service.isolde.bridge.runtime";
    addonsOverlay = m_overlay->path() + "/addons";
    if (!linkDistroAddons(addonsOverlay, runtimeId))
        return false;
    const auto destination = addonsOverlay + '/' + runtimeId;
    if (!QDir().mkpath(destination))
        return false;
    if (!copyBridgeFiles(addon, destination))
        return false;
    if (!updateRuntimeManifest(manifest))
        return false;
    return true;
}

bool
KodiService::linkDistroAddons(const QString& addonsOverlay, const QString& runtimeId)
{
    if (!QDir().mkpath(addonsOverlay))
        return false;
    for (const auto& entry : QDir("/usr/share/kodi/addons").entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
    {
        if (entry == runtimeId)
            return false;
        if (!QFile::link("/run/isolde-original-addons/" + entry, addonsOverlay + '/' + entry))
            return false;
    }
    return true;
}

bool
KodiService::copyBridgeFiles(const QString& addon, const QString& destination)
{
    for (const auto& name :
         QStringList{"addon.xml", "service.py", "bridge.py", "playback.py", "artwork.py", "playlists.py", "sources.py"})
    {
        QFile source(addon + '/' + name), target(destination + '/' + name);
        if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly))
            return false;
        auto data = source.readAll();
        if (name == "addon.xml")
            data.replace("id=\"service.isolde.bridge\"", "id=\"service.isolde.bridge.runtime\"");
        if (target.write(data) != data.size())
            return false;
    }
    return true;
}

bool
KodiService::updateRuntimeManifest(QByteArray& manifest)
{
    manifest.replace("</addons>", "  <addon>service.isolde.bridge.runtime</addon>\n</addons>");
    QFile overlayManifest(m_overlay->path() + "/addon-manifest.xml");
    if (!overlayManifest.open(QIODevice::WriteOnly) || overlayManifest.write(manifest) != manifest.size())
        return false;
    overlayManifest.close();
    return true;
}

bool
KodiService::setupKeymaps(const QString& originalKeys, const QString& runtimeKeys)
{
    if (!QDir().mkpath(originalKeys) || !QDir().mkpath(runtimeKeys))
        return false;
    const QString keyName = "zzzz-isolde-managed.xml";
    for (const auto& entry : QDir(originalKeys).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
    {
        if (entry == keyName || !QFile::link("/run/isolde-original-keymaps/" + entry, runtimeKeys + '/' + entry))
            return false;
    }
    QFile keys(runtimeKeys + '/' + keyName);
    const QByteArray keyData =
        "<keymap><global><keyboard><home>NotifyAll(Isolde,home)</home><h>NotifyAll(Isolde,home)</h></keyboard></global>"
        "<FullscreenVideo><keyboard><escape>NotifyAll(Isolde,stop)</escape><backspace>NotifyAll(Isolde,stop)</"
        "backspace>"
        "<return>NotifyAll(Isolde,pause)</return><space>NotifyAll(Isolde,pause)</space>"
        "<left>NotifyAll(Isolde,backward)</left><right>NotifyAll(Isolde,forward)</right>"
        "<up>NotifyAll(Isolde,channel-previous)</up><down>NotifyAll(Isolde,channel-next)</down>"
        "<menu>NotifyAll(Isolde,options)</menu><m>NotifyAll(Isolde,options)</m>"
        "</keyboard></FullscreenVideo>"
        "<VideoOSD><keyboard><escape>NotifyAll(Isolde,stop)</escape><backspace>NotifyAll(Isolde,stop)</backspace>"
        "<return>NotifyAll(Isolde,pause)</return><space>NotifyAll(Isolde,pause)</space>"
        "<left>NotifyAll(Isolde,backward)</left><right>NotifyAll(Isolde,forward)</right>"
        "<up>NotifyAll(Isolde,channel-previous)</up><down>NotifyAll(Isolde,channel-next)</down>"
        "<menu>NotifyAll(Isolde,options)</menu><m>NotifyAll(Isolde,options)</m>"
        "</keyboard></VideoOSD>"
        "<Visualisation><keyboard><escape>NotifyAll(Isolde,stop)</escape><backspace>NotifyAll(Isolde,stop)</backspace>"
        "<return>NotifyAll(Isolde,pause)</return><space>NotifyAll(Isolde,pause)</space>"
        "<left>NotifyAll(Isolde,backward)</left><right>NotifyAll(Isolde,forward)</right>"
        "<menu>NotifyAll(Isolde,options)</menu><m>NotifyAll(Isolde,options)</m>"
        "</keyboard></Visualisation></keymap>";
    if (!keys.open(QIODevice::WriteOnly) || keys.write(keyData) != keyData.size())
        return false;
    keys.close();
    return true;
}

QStringList
KodiService::buildSandboxExtras(const QString& addonsOverlay, const QString& overlayPath,
                                const QStringList& libraryFolders)
{
    const QStringList extra{
        "--ro-bind",
        "/usr/share/kodi/addons",
        "/run/isolde-original-addons",
        "--ro-bind",
        addonsOverlay,
        "/usr/share/kodi/addons",
        "--ro-bind",
        overlayPath + "/addon-manifest.xml",
        "/usr/share/kodi/system/addon-manifest.xml",
        "--ro-bind",
        overlayPath + "/keymaps",
        QDir::homePath() + "/.kodi/userdata/keymaps",
        "--setenv",
        "ISOLDE_MANAGED_KODI",
        "1",
        "--setenv",
        "ISOLDE_LIBRARY_ROOTS",
        QString::fromUtf8(QJsonDocument::fromVariant(libraryFolders).toJson(QJsonDocument::Compact))};
    return extra;
}

