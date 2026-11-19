#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include "GameLibrary.h"

#include "CompatibilityRuntime.h"
#include "GameEmulators.h"
#include "GameResourceLimits.h"
#include "ManagedGamePaths.h"
#include "media/MediaGrants.h"
#include "OpticalDiscGrants.h"
#include "OpticalDiscIdentity.h"
#include "OpticalDiscMount.h"
#include "SteamLibraryGrants.h"
#include "app/AppCatalog.h"
#include "home/ComponentSchema.h"

namespace
{
constexpr int MaxGames = GamesResourceLimits::CatalogEntries;
constexpr int PageSize = GamesResourceLimits::PageEntries;
constexpr int MaxPlatforms = GamesResourceLimits::Platforms;
}

void stopSetupProcess(QProcess& process);
bool approvedMediaDirectory(const QString& profile, const QString& path);

#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/file.h>
#endif
#endif

QString
GameLibrary::checkedTarget(const QString& root, const QString& relative) const
{
    QString validatedRoot = root;
    if (!validateOpticalTarget(root, validatedRoot))
        return {};
    const auto grantError = MediaGrants::validate(root);
    const bool managed = ManagedGamePaths::isInstallRoot(m_emulatorProfile, root);
    const bool optical = OpticalDiscIdentify::isRootReference(root);
    if (!optical && !grantError.isEmpty() && !managed)
    {
        fail(grantError);
        return {};
    }
    if (!validateRelativePath(relative))
        return {};
    return validateGameFile(validatedRoot, relative);
}

bool
GameLibrary::validateOpticalTarget(const QString& root, QString& validatedRoot) const
{
    const bool optical = OpticalDiscIdentify::isRootReference(root);
    if (!optical)
        return true;
    const auto mediaId = OpticalDiscIdentify::mediaIdFromRoot(root);
    if (!OpticalDiscGrants::approved(m_emulatorProfile, mediaId))
    {
        fail(
            tr("This profile has not approved that optical identity. Insert the disc and explicitly approve it in "
               "Games."));
        return false;
    }
    const auto media = OpticalDiscClassify::resolveRootReference(root);
    const auto disc = media.value("disc").toMap();
    if (media.value("state") != "ready")
    {
        const auto number = media.value("expectedDisc").toInt();
        fail(media.value("state") == "wrong-disc"
                 ? tr("Wrong optical disc. Insert disc %1 of %2, then retry.")
                       .arg(number)
                       .arg(media.value("discCount").toInt())
                 : tr("Insert optical disc %1 of %2, then retry.").arg(number).arg(media.value("discCount").toInt()));
        return false;
    }
    validatedRoot = disc.value("mountRoot").toString();
    return true;
}

bool
GameLibrary::validateRelativePath(const QString& relative) const
{
    return !(relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.contains('\\') ||
             relative.contains(QChar::Null) || relative.size() > 4096 || QDir::cleanPath(relative) != relative ||
             relative.split('/').contains(".."));
}

QString
GameLibrary::validateGameFile(const QString& validatedRoot, const QString& relative) const
{
    const auto path = QDir(validatedRoot).filePath(relative);
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable() || info.canonicalFilePath() != path ||
        !path.startsWith(validatedRoot + '/'))
    {
        fail(tr("The game file is missing, unreadable or redirected. Reconnect its source and retry."));
        return QString();
    }
    return path;
}

bool
GameLibrary::installEmulators(bool consent, bool update, bool offline)
{
    if (!validInstallEmulatorsArgs(consent))
        return fail(tr("Review and confirm emulator installation before continuing."));
#ifndef Q_OS_LINUX
    Q_UNUSED(update);
    Q_UNUSED(offline);
    return fail(tr("Managed emulator installation currently requires the Linux appliance."));
#else
    const auto root = findInstallerRoot();
    if (!validateInstaller(root))
        return fail(tr("Repair the Isolde emulator installer."));
    if (!prepareEmulatorState())
        return false;
    return startEmulatorInstaller(root, update, offline);
#endif
}

bool
GameLibrary::validInstallEmulatorsArgs(bool consent) const
{
    return m_ready && consent && !setupBusy() && m_catalog && !(m_sessionActiveGuard && m_sessionActiveGuard());
}

QString
GameLibrary::findInstallerRoot() const
{
    auto root = QCoreApplication::applicationDirPath() + "/share/isolde";
    if (!QFileInfo(root + "/provision-games.py").isReadable())
        root = QCoreApplication::applicationDirPath() + "/../share/isolde";
    return root;
}

bool
GameLibrary::validateInstaller(const QString& root) const
{
    const auto program = QStandardPaths::findExecutable("python3");
    return !program.isEmpty() && QFileInfo(root + "/provision-games.py").isReadable();
}

bool
GameLibrary::prepareEmulatorState()
{
    const auto state = GameEmulators::home(m_emulatorProfile, "ppsspp") + "/.local/state";
    for (auto ancestor = QFileInfo(state);; ancestor = QFileInfo(ancestor.absolutePath()))
    {
        if (ancestor.isSymLink())
        {
            fail(tr("Private emulator storage is redirected."));
            return false;
        }
        if (ancestor.absolutePath() == ancestor.absoluteFilePath())
            break;
    }
    if (!QDir().mkpath(state) || QFileInfo(state).canonicalFilePath() != state)
    {
        fail(tr("Cannot prepare private installation evidence."));
        return false;
    }
    m_setupReceipt = state + "/runtime-install-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    m_setupCanceled = false;
    m_setupOutput.clear();
    m_setupStatus =
        "Installing the verified runtimes and console cores. First-time PSP preparation may take several "
        "minutes. No game or firmware downloads.";
    m_setup.setChildProcessModifier(
        []
        {
            ::setsid();
        });
    return true;
}

bool
GameLibrary::startEmulatorInstaller(const QString& root, bool update, bool offline)
{
    QStringList arguments{root + "/provision-games.py", "--install", "--receipt", m_setupReceipt};
    if (update)
        arguments.append("--update");
    if (offline)
        arguments.append("--offline");
    const auto program = QStandardPaths::findExecutable("python3");
    m_setup.start(program, arguments);
    m_setupDeadline.start();
    emit setupChanged();
    return true;
}

bool
GameLibrary::rebuildEmulatorData(const QString& id)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(id);
    return fail(tr("Prepared emulator data is managed on the Linux appliance."));
#else
    if (!validRebuildArgs(id))
        return fail(tr("Choose an available emulator and wait for setup to finish."));
    if (m_sessionActiveGuard && m_sessionActiveGuard())
        return fail(tr("Stop the current game before rebuilding imported data."));
    const auto home = GameEmulators::home(m_emulatorProfile, id);
    const auto root = home + "/.local/share/isolde-prepared";
    if (!validPreparedRoot(root))
        return false;
    if (!QFileInfo::exists(root))
        return true;
    if (!checkRetirementLimit(home))
        return false;
    return retirePreparedData(home, root);
#endif
}

bool
GameLibrary::validRebuildArgs(const QString& id) const
{
    return m_ready && !setupBusy() && !GameEmulators::definition(id).value("setup").toMap().isEmpty();
}

bool
GameLibrary::validPreparedRoot(const QString& root) const
{
    if (QFileInfo(root).isSymLink())
    {
        fail(tr("Prepared emulator storage is redirected. Nothing was moved."));
        return false;
    }
    if (QFileInfo(root).canonicalFilePath() != root)
    {
        fail(tr("Prepared emulator storage is redirected. Nothing was moved."));
        return false;
    }
    const auto lockPath = QFileInfo(root).absolutePath() + "/.config/isolde-emulator/prepare.lock";
    if (QFileInfo(lockPath).isSymLink() ||
        QFileInfo(QFileInfo(lockPath).absolutePath()).canonicalFilePath() != QFileInfo(lockPath).absolutePath())
    {
        fail(tr("Emulator preparation storage is redirected."));
        return false;
    }
    QFile lock(lockPath);
    if (!lock.open(QIODevice::ReadWrite) || ::flock(int(lock.handle()), LOCK_EX | LOCK_NB) != 0)
    {
        fail(tr("Emulator preparation is active. Return after it stops, then retry."));
        return false;
    }
    return true;
}

bool
GameLibrary::checkRetirementLimit(const QString& home) const
{
    const auto parent = QFileInfo(home).absolutePath();
    if (QDir(parent).entryList({"isolde-prepared-retired-*"}, QDir::Dirs | QDir::NoDotAndDotDot).size() >= 4)
    {
        fail(tr("Four recovery copies are already retained. Review emulator storage before rebuilding again."));
        return false;
    }
    return true;
}

bool
GameLibrary::retirePreparedData(const QString& home, const QString& root)
{
    Q_UNUSED(home);
    const auto parent = QFileInfo(root).absolutePath();
    const auto destination = parent + "/isolde-prepared-retired-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (QFileInfo(destination).exists() || QFileInfo(destination).isSymLink() || !QDir().rename(root, destination))
    {
        fail(tr("Prepared emulator data could not be retained for recovery. Existing data is unchanged."));
        return false;
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

void
GameLibrary::cancelEmulatorSetup()
{
    if (!setupBusy())
        return;
    m_setupCanceled = true;
    stopSetupProcess(m_setup);
    m_setupStatus =
        "Finishing the current package operation safely. Wait before retrying; existing games and saves are untouched.";
}

void
stopSetupProcess(QProcess& process)
{
#ifdef Q_OS_UNIX
    if (process.processId() > 0)
        ::kill(pid_t(process.processId()), SIGTERM);
#else
    process.terminate();
#endif
}

QString
GameLibrary::addGameWithOrigin(const QVariantMap& input, const QString& origin)
{
    if (!validAddGameSource())
        return {};
    QString error;
    const auto registration = ComponentSchema::gameRegistration(input, &error);
    if (registration.isEmpty())
    {
        fail(error);
        return {};
    }
    const auto runtime = registration.value("runtimeId").toString();
    const auto root = registration.value("root").toString(), target = registration.value("target").toString();
    if (!validGameRegistration(registration, runtime, root, target))
        return {};
    if (!checkEditionConflict(runtime, root, target))
        return {};
    if (!ensureGameProvenanceSchema())
        return {};
    return insertGameRecord(registration, runtime, root, target, origin);
}

bool
GameLibrary::validAddGameSource() const
{
    return m_ready && !m_portablePhase.startsWith("uninstall");
}

bool
GameLibrary::validGameRegistration(const QVariantMap& registration,
                                   const QString& runtime,
                                   const QString& root,
                                   const QString& target) const
{
    if (!platformAvailable(registration.value("platform").toString()))
    {
        fail(tr("The Games catalog supports up to 128 distinct platforms. Existing entries were preserved."));
        return false;
    }
    if (!compatibleRuntime(runtime, !root.isEmpty()))
    {
        fail(tr("Choose an installed compatible game runtime."));
        return false;
    }
    if (!root.isEmpty() && checkedTarget(root, target).isEmpty())
        return false;
    return true;
}

bool
GameLibrary::checkEditionConflict(const QString& runtime, const QString& root, const QString& target) const
{
    if (m_hasEditions && installationExists(runtime, root, target))
    {
        fail(tr("This installation already belongs to a game or edition."));
        return false;
    }
    return true;
}

bool
GameLibrary::ensureGameProvenanceSchema()
{
    return ensureProvenanceSchema();
}

QString
GameLibrary::insertGameRecord(const QVariantMap& registration,
                              const QString& runtime,
                              const QString& root,
                              const QString& target,
                              const QString& origin)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?");
    query.addBindValue(runtime);
    query.addBindValue(root);
    query.addBindValue(target);
    if (!query.exec())
    {
        fail(query.lastError().text());
        return QString();
    }
    if (query.next())
    {
        fail(tr("This installation is already in Games. Its metadata was preserved."));
        return QString();
    }
    query.finish();
    if (!m_db.transaction())
    {
        fail(m_db.lastError().text());
        return QString();
    }
    if (!query.exec("SELECT count(*) FROM games") || !query.next() || query.value(0).toInt() >= MaxGames)
    {
        m_db.rollback();
        fail(tr("The games catalog is full or unavailable."));
        return QString();
    }
    query.finish();
    const auto id = "game-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,added) VALUES(?,?,?,?,?,?,unixepoch())");
    for (const auto& value : QStringList{id,
                                         registration.value("title").toString(),
                                         registration.value("platform").toString(),
                                         runtime,
                                         root,
                                         target})
        query.addBindValue(value);
    if (!query.exec() || !recordProvenance(id, "game", runtime, root, target, origin) || !m_db.commit())
    {
        const auto error = query.lastError().text();
        m_db.rollback();
        if (m_error.isEmpty())
            fail(tr("Could not save the game: %1").arg(error));
        return QString();
    }
    m_error.clear();
    emit changed();
    return id;
}

QString
GameLibrary::addEditionWithOrigin(const QString& gameId, const QVariantMap& input, const QString& origin)
{
    if (!validAddEditionSource(gameId))
        return {};
    QString error;
    const auto value = ComponentSchema::gameRegistration(input, &error);
    if (value.isEmpty())
    {
        fail(error);
        return {};
    }
    const auto runtime = value.value("runtimeId").toString(), root = value.value("root").toString(),
               target = value.value("target").toString();
    if (!validEditionRegistration(value, runtime, root, target))
        return {};
    if (!checkEditionInstallConflict(runtime, root, target))
        return {};
    if (!ensureEditionSchemaAndTransaction())
        return {};
    return insertEditionRecord(gameId, value, runtime, root, target, origin);
}

bool
GameLibrary::validAddEditionSource(const QString& gameId) const
{
    return !m_portablePhase.startsWith("uninstall") && m_ready && !registration(gameId).isEmpty();
}

bool
GameLibrary::validEditionRegistration(const QVariantMap& value,
                                      const QString& runtime,
                                      const QString& root,
                                      const QString& target) const
{
    if (!platformAvailable(value.value("platform").toString()))
    {
        fail(tr("The Games catalog supports up to 128 distinct platforms."));
        return false;
    }
    if (!compatibleRuntime(runtime, !root.isEmpty()))
    {
        fail(tr("Choose a compatible edition runtime."));
        return false;
    }
    if (!root.isEmpty() && checkedTarget(root, target).isEmpty())
        return false;
    if (editions(value.value("id").toString()).size() >= 32)
    {
        fail(tr("A game supports up to 32 editions."));
        return false;
    }
    return true;
}

bool
GameLibrary::checkEditionInstallConflict(const QString& runtime, const QString& root, const QString& target) const
{
    if (installationExists(runtime, root, target))
    {
        fail(tr("This installation already belongs to a game or edition."));
        return false;
    }
    return true;
}

bool
GameLibrary::ensureEditionSchemaAndTransaction()
{
    if (!ensureProvenanceSchema())
        return false;
    QSqlQuery query(m_db);
    if (!m_hasEditions)
    {
        const auto backup = m_db.databaseName() + ".v1-backup.sqlite";
        if (QFileInfo::exists(backup) || QFileInfo(backup).isSymLink())
            return fail(tr("A pre-edition backup already exists. Preserve it before retrying the catalog upgrade."));
        query.prepare("VACUUM main INTO ?");
        query.addBindValue(backup);
        if (!query.exec())
            return fail(tr("Cannot back up the catalog before adding editions."));
    }
    if (!m_db.transaction())
    {
        fail(m_db.lastError().text());
        return false;
    }
    if (!m_hasEditions &&
        (!query.exec("CREATE TABLE game_editions(id TEXT PRIMARY KEY,game_id TEXT NOT NULL REFERENCES games(id) ON "
                     "DELETE CASCADE,title TEXT NOT NULL,platform TEXT NOT NULL,runtime TEXT NOT NULL,root TEXT NOT "
                     "NULL,target TEXT NOT NULL,UNIQUE(runtime,root,target))") ||
         !query.exec("CREATE INDEX game_editions_owner ON game_editions(game_id)") ||
         (!m_hasActivity && !query.exec("PRAGMA user_version=2"))))
    {
        m_db.rollback();
        fail(query.lastError().text());
        return false;
    }
    return true;
}

QString
GameLibrary::insertEditionRecord(const QString& gameId,
                                 const QVariantMap& value,
                                 const QString& runtime,
                                 const QString& root,
                                 const QString& target,
                                 const QString& origin)
{
    QSqlQuery query(m_db);
    const auto edition = "edition-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.prepare("INSERT INTO game_editions(id,game_id,title,platform,runtime,root,target) VALUES(?,?,?,?,?,?,?)");
    query.addBindValue(edition);
    query.addBindValue(gameId);
    for (const auto& key : QStringList{"title", "platform", "runtimeId", "root", "target"})
        query.addBindValue(value.value(key));
    if (!query.exec() || !recordProvenance(edition, "edition", runtime, root, target, origin) || !m_db.commit())
    {
        m_db.rollback();
        if (m_error.isEmpty())
            fail(tr("Edition could not be saved; the previous catalog was preserved."));
        return {};
    }
    m_hasEditions = true;
    m_error.clear();
    emit errorChanged();
    emit changed();
    return edition;
}

void
GameLibrary::cancelImport()
{
    m_steamScanCanceled = true;
    m_steamScanDeadline.stop();
    if (m_steamScan.state() != QProcess::NotRunning)
        m_steamScan.kill();
    m_steamScanOutput.clear();
    m_importStatus.clear();
    m_scanTimer.stop();
    m_scan.reset();
    m_preview.clear();
    m_importReady = false;
    m_importRoot.clear();
    m_importRuntime.clear();
    m_importPlatform.clear();
    m_importFormats.clear();
    m_scanEntries = 0;
    emit importChanged();
}

bool
GameLibrary::beginSteamImport()
{
    if (!m_ready || scanning() || m_importReady)
        return fail(tr("Finish or cancel the current import first."));
    if (!compatibleRuntime("steam", false))
        return fail(tr("Install Steam from the Isolde Store first."));
    if (m_emulatorProfile != "default" &&
        (QUuid(m_emulatorProfile).isNull() ||
         QUuid(m_emulatorProfile).toString(QUuid::WithoutBraces) != m_emulatorProfile))
        return fail(tr("Invalid active profile."));
    auto helper = QCoreApplication::applicationDirPath() + "/share/isolde/steam-games.py";
    if (!QFileInfo(helper).isReadable())
        helper = QCoreApplication::applicationDirPath() + "/../share/isolde/steam-games.py";
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde Steam adapter."));
    const auto home = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles/" +
                      m_emulatorProfile + "/steam/home";
    const auto libraries = SteamLibraryGrants::folders(m_emulatorProfile);
    const auto grantsError = SteamLibraryGrants::validateList(libraries, false);
    if (!grantsError.isEmpty())
        return fail(tr("Review external Steam library access in Games settings. %1").arg(grantsError));
    QStringList arguments{helper, "--discover", "--home", home};
    for (const auto& library : libraries)
    {
        if (QFileInfo(library).isDir())
            arguments << "--library" << library;
    }
    m_preview.clear();
    m_steamScanOutput.clear();
    m_steamScanCanceled = false;
    m_importRuntime = "steam";
    m_importRoot = QStringLiteral("");
    m_importReady = false;
    m_error.clear();
    emit errorChanged();
    m_importStatus = tr("Reading installed games in this profile's approved Steam libraries. Nothing is saved yet.");
    m_steamScan.start(python, arguments);
    m_steamScanDeadline.start();
    emit importChanged();
    return true;
}

QStringList
GameLibrary::steamLibraries() const
{
    return SteamLibraryGrants::folders(m_emulatorProfile);
}

bool
GameLibrary::addSteamLibrary(const QString& path)
{
    if (!m_ready)
        return fail(tr("Games is unavailable in this profile."));
    auto libraries = steamLibraries();
    if (libraries.contains(path))
        return fail(tr("That external Steam library is already approved."));
    const auto validation = SteamLibraryGrants::validate(path);
    if (!validation.isEmpty())
        return fail(validation);
    libraries.append(path);
    const auto error = SteamLibraryGrants::save(m_emulatorProfile, libraries);
    if (!error.isEmpty())
        return fail(error);
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::removeSteamLibrary(const QString& path)
{
    if (!m_ready)
        return fail(tr("Games is unavailable in this profile."));
    auto libraries = steamLibraries();
    if (!libraries.removeOne(path))
        return fail(tr("That external Steam library is not approved in this profile."));
    const auto error = SteamLibraryGrants::save(m_emulatorProfile, libraries);
    if (!error.isEmpty())
        return fail(error);
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

QVariantList
GameLibrary::mountedOpticalDiscs() const
{
    return m_ready ? OpticalDiscHash::mountedDiscs() : QVariantList{};
}

QVariantList
GameLibrary::refreshOpticalDiscs() const
{
    if (!m_ready)
        return {};
    for (const auto& item : OpticalDiscHash::mountedDiscs())
        OpticalDiscProbe::retryIdentity(item.toMap().value("device").toString());
    return OpticalDiscHash::mountedDiscs();
}

QVariantMap
GameLibrary::revalidateOpticalDisc(const QString& observationId, const QString& device) const
{
    if (!m_ready || !QRegularExpression("^[0-9a-f]{64}$").match(observationId).hasMatch() ||
        !QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch())
        return {{"schemaVersion", 1}, {"state", "invalid"}};
    return OpticalDiscProbe::revalidate(observationId, device);
}

QString
GameLibrary::registerOpticalGame(const QString& title,
                                 const QString& runtimeId,
                                 const QString& relativeExecutable,
                                 const QString& observationId,
                                 const QString& device)
{
    if (!m_ready || !compatibleRuntime(runtimeId, true))
    {
        fail(tr("Choose an installed file-based game runtime."));
        return {};
    }
    if (title.trimmed().isEmpty() || title != title.trimmed() || title.size() > 256)
    {
        fail(tr("Enter a game title."));
        return {};
    }
    const auto media = revalidateOpticalDisc(observationId, device);
    if (media.value("state") != "ready")
    {
        fail(tr("Optical media changed. Refresh disc details and review it again."));
        return {};
    }
    const auto disc = media.value("disc").toMap();
    const auto root = OpticalDiscIdentify::rootReference(disc);
    if (root.isEmpty() || !disc.value("directPlaySupported").toBool())
    {
        fail(disc.value("mountTracking") == "unavailable"
                 ? tr("This system cannot identify the mounted disc instance. Direct Play is unavailable; update the "
                      "kernel or use another supported system.")
                 : tr("The disc's full-content identity is not ready. Wait for verification or refresh to retry."));
        return {};
    }
    const auto grantError = OpticalDiscGrants::authorize(m_emulatorProfile, disc.value("mediaId").toString());
    if (!grantError.isEmpty())
    {
        fail(grantError);
        return {};
    }
    const auto target = checkedTarget(root, relativeExecutable);
    if (target.isEmpty())
        return {};
    QFile executable(target);
    if (!(QFileInfo(target).permissions() & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)) ||
        !executable.open(QIODevice::ReadOnly) ||
        executable.read(20).left(4) != QByteArray("\x7f"
                                                  "ELF",
                                                  4))
    {
        fail(tr("Choose a native Linux ELF executable on the selected disc. Installers and scripts are not run."));
        return {};
    }
    const QVariantMap registration{{"schemaVersion", 1},
                                   {"title", title},
                                   {"platform", "PC Optical"},
                                   {"runtimeId", runtimeId},
                                   {"root", root},
                                   {"target", relativeExecutable}};
    return addGameWithOrigin(registration, "optical-registration");
}

bool
GameLibrary::approveOpticalMedia(const QString& observationId, const QString& device)
{
    if (!m_ready)
        return fail(tr("Games is unavailable."));
    const auto media = revalidateOpticalDisc(observationId, device);
    const auto disc = media.value("disc").toMap();
    if (media.value("state") != "ready" || !disc.value("directPlaySupported").toBool())
        return fail(
            tr("Only a mounted disc with a verified full-content identity can be approved. Wait or refresh to retry."));
    const auto error = OpticalDiscGrants::authorize(m_emulatorProfile, disc.value("mediaId").toString());
    if (!error.isEmpty())
        return fail(error);
    m_error.clear();
    emit errorChanged();
    return true;
}

bool
GameLibrary::revokeOpticalMedia(const QString& observationId, const QString& device)
{
    if (!m_ready)
        return fail(tr("Games is unavailable."));
    const auto media = revalidateOpticalDisc(observationId, device);
    const auto disc = media.value("disc").toMap();
    if (media.value("state") != "ready")
        return fail(tr("The disc changed. Refresh optical details before revoking its access."));
    const auto error = OpticalDiscGrants::revoke(m_emulatorProfile, disc.value("mediaId").toString());
    if (!error.isEmpty())
        return fail(error);
    m_error.clear();
    emit errorChanged();
    return true;
}

bool
GameLibrary::mountOpticalDisc(const QString& observationId, const QString& device)
{
    if (!m_ready)
        return fail(tr("Games is unavailable."));
    if (revalidateOpticalDisc(observationId, device).value("state") != "unmounted")
        return fail(
            tr("The selected disc or drive changed. Refresh optical media and review it again before mounting."));
    QString error;
    if (!OpticalDiscMount::mountReadOnly(device, &error))
        return fail(error);
    m_error.clear();
    emit errorChanged();
    return true;
}

bool
GameLibrary::ejectOpticalDisc(const QString& observationId, const QString& device)
{
    if (!m_ready)
        return fail(tr("Games is unavailable."));
    if (m_sessionActiveGuard && m_sessionActiveGuard())
        return fail(tr("A session is active. Return Home before safely ejecting optical media."));
    if (portableInstallBusy() && m_portableOpticalDevice == device)
        return fail(tr("The disc is being used by the game installer. Finish or cancel it before ejecting."));
    const auto state = revalidateOpticalDisc(observationId, device).value("state").toString();
    if (state != "ready" && state != "unmounted")
        return fail(
            tr("The selected disc or drive changed. Refresh optical media and review it again before ejecting."));
    QString error;
    if (!OpticalDiscMount::eject(device, &error))
        return fail(error);
    m_error.clear();
    emit errorChanged();
    return true;
}

bool
GameLibrary::beginPortableInstall(const QString& source,
                                  const QString& runtimeId,
                                  const QString& opticalObservationId,
                                  const QString& opticalDevice)
{
    if (!validBeginPortableInstallArgs(runtimeId))
        return fail(tr("Finish or cancel the current game installation first."));
    const QFileInfo file(source);
    if (!validPortableSourceFile(file, opticalObservationId, opticalDevice))
        return fail(
            tr("Choose a regular native Linux ELF executable or ZIP, USTAR, gzip- or bzip2-compressed USTAR "
               "bundle inside an approved media folder."));
    if (!opticalObservationId.isEmpty() && !validateOpticalSource(file, opticalObservationId, opticalDevice))
        return fail(tr("The selected optical disc changed. Refresh disc details and choose its package again."));
    const auto helper = findInstallerHelper("portable-game-installer.py");
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde portable-game installer."));
    initializePortableInstallState(file, runtimeId, opticalObservationId, opticalDevice);
    return startPortableInspect(python, helper, m_portableSource);
}

bool
GameLibrary::validBeginPortableInstallArgs(const QString& runtimeId) const
{
    return m_ready && !portableInstallBusy() && m_portablePhase.isEmpty() && compatibleRuntime(runtimeId, true);
}

bool
GameLibrary::validPortableSourceFile(const QFileInfo& file,
                                     const QString& opticalObservationId,
                                     const QString& opticalDevice) const
{
    if (opticalObservationId.isEmpty() != opticalDevice.isEmpty())
        return false;
    return file.isFile() && !file.isSymLink() && file.canonicalFilePath() == file.absoluteFilePath() &&
           file.size() >= 1 && file.size() <= 512LL * 1024 * 1024 &&
           (opticalObservationId.isEmpty() || approvedMediaDirectory(m_emulatorProfile, file.absolutePath()));
}

bool
GameLibrary::validateOpticalSource(const QFileInfo& file,
                                   const QString& opticalObservationId,
                                   const QString& opticalDevice) const
{
    const auto media = revalidateOpticalDisc(opticalObservationId, opticalDevice);
    const auto disc = media.value("disc").toMap();
    return media.value("state") == "ready" &&
           file.canonicalFilePath().startsWith(disc.value("mountRoot").toString() + '/');
}

QString
GameLibrary::findInstallerHelper(const QString& scriptName) const
{
    auto helper = QCoreApplication::applicationDirPath() + "/share/isolde/" + scriptName;
    if (!QFileInfo(helper).isReadable())
        helper = QCoreApplication::applicationDirPath() + "/../share/isolde/" + scriptName;
    return helper;
}

void
GameLibrary::initializePortableInstallState(const QFileInfo& file,
                                            const QString& runtimeId,
                                            const QString& opticalObservationId,
                                            const QString& opticalDevice)
{
    m_portableDirectoryCopy = false;
    m_portableDirectoryEntrypoint.clear();
    m_portableSource = file.canonicalFilePath();
    m_portableRuntime = runtimeId;
    m_portableOpticalObservationId = opticalObservationId;
    m_portableOpticalDevice = opticalDevice;
    m_portableDestination = ManagedGamePaths::gamesRoot(m_emulatorProfile) + "/" +
                            QUuid::createUuid().toString(QUuid::WithoutBraces) + "/install";
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableInstallPreview.clear();
    m_portableCanceled = false;
    m_portablePhase = "inspect";
    m_portableInstallStatus = "Checking the selected file; nothing will be executed or installed during preflight.";
    m_error.clear();
    emit errorChanged();
    emit portableInstallChanged();
}

bool
GameLibrary::startPortableInspect(const QString& python, const QString& helper, const QString& source)
{
    m_portableInstall.start(python, {helper, "--inspect", "--source", source});
    m_portableInstallDeadline.start();
    return true;
}

bool
GameLibrary::beginPortableDirectoryInstall(const QString& source, const QString& entrypoint, const QString& runtimeId)
{
    if (!validBeginPortableDirectoryInstallArgs(runtimeId))
        return fail(tr("Finish or cancel the current game installation first."));
    const QFileInfo directory(source);
    if (!validPortableSourceDirectory(directory))
        return fail(tr("Choose a real game directory inside a folder approved for this profile."));
    if (!validPortableEntryPoint(entrypoint))
        return fail(tr("Choose a canonical relative entry-point path inside the game directory."));
    const auto helper = findInstallerHelper("portable-game-installer.py");
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde portable-game installer."));
    initializePortableDirectoryInstallState(directory, entrypoint, runtimeId);
    return startPortableDirectoryInspect(python, helper, m_portableSource, m_portableDirectoryEntrypoint);
}

bool
GameLibrary::validBeginPortableDirectoryInstallArgs(const QString& runtimeId) const
{
    return m_ready && !portableInstallBusy() && m_portablePhase.isEmpty() && compatibleRuntime(runtimeId, true);
}

bool
GameLibrary::validPortableSourceDirectory(const QFileInfo& directory) const
{
    return directory.isDir() && !directory.isSymLink() &&
           directory.canonicalFilePath() == directory.absoluteFilePath() &&
           approvedMediaDirectory(m_emulatorProfile, directory.canonicalFilePath());
}

bool
GameLibrary::validPortableEntryPoint(const QString& entrypoint) const
{
    return !(entrypoint.isEmpty() || entrypoint.size() > 4096 || QDir::isAbsolutePath(entrypoint) ||
             QDir::cleanPath(entrypoint) != entrypoint || entrypoint.contains('\\') ||
             entrypoint.contains(QChar::Null) || entrypoint.split('/').contains("..") ||
             entrypoint.split('/').contains("."));
}

void
GameLibrary::initializePortableDirectoryInstallState(const QFileInfo& directory,
                                                     const QString& entrypoint,
                                                     const QString& runtimeId)
{
    m_portableDirectoryCopy = true;
    m_portableDirectoryEntrypoint = entrypoint;
    m_portableSource = directory.canonicalFilePath();
    m_portableRuntime = runtimeId;
    m_portableOpticalObservationId.clear();
    m_portableOpticalDevice.clear();
    m_portableDestination = ManagedGamePaths::gamesRoot(m_emulatorProfile) + "/" +
                            QUuid::createUuid().toString(QUuid::WithoutBraces) + "/install";
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableInstallPreview.clear();
    m_portableCanceled = false;
    m_portablePhase = "inspect";
    m_portableInstallStatus = "Checking the selected game folder without executing it or modifying its files.";
    m_error.clear();
    emit errorChanged();
    emit portableInstallChanged();
}

bool
GameLibrary::startPortableDirectoryInspect(const QString& python,
                                           const QString& helper,
                                           const QString& source,
                                           const QString& entrypoint)
{
    m_portableInstall.start(python, {helper, "--inspect-directory", "--source", source, "--entrypoint", entrypoint});
    m_portableInstallDeadline.start();
    return true;
}

bool
GameLibrary::confirmPortableInstall()
{
    if (!validConfirmPortableInstallArgs())
        return fail(tr("No portable game package is awaiting confirmation."));
    if (!validatePortableSource())
        return fail(tr("The package source or selected runtime changed. Review preflight again."));
    if (!m_portableOpticalObservationId.isEmpty() && !validateOpticalMediaForConfirm())
        return fail(tr("The selected optical disc changed or was removed. Refresh its details before installing."));
    if (!validateInstallDestination())
        return fail(tr("Cannot prepare a new private game destination; existing files were preserved."));
    if (!secureInstallDestination())
        return fail(tr("Cannot secure the private game install destination."));
    const auto helper = findInstallerHelper("portable-game-installer.py");
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde portable-game installer."));
    initializePortableInstallForConfirm();
    const auto arguments = buildInstallArguments(helper);
    m_portableInstall.start(python, arguments);
    m_portableInstallDeadline.start();
    emit portableInstallChanged();
    return true;
}

bool
GameLibrary::validConfirmPortableInstallArgs() const
{
    return m_ready && m_portablePhase == "review" && !portableInstallBusy();
}

bool
GameLibrary::validatePortableSource() const
{
    const QFileInfo source(m_portableSource);
    const bool sourceValid =
        m_portableDirectoryCopy
            ? source.isDir() && !source.isSymLink() && source.canonicalFilePath() == m_portableSource &&
                  approvedMediaDirectory(m_emulatorProfile, m_portableSource)
            : source.isFile() && !source.isSymLink() && source.canonicalFilePath() == m_portableSource &&
                  (!m_portableOpticalObservationId.isEmpty() ||
                   approvedMediaDirectory(m_emulatorProfile, source.absolutePath()));
    return compatibleRuntime(m_portableRuntime, true) && sourceValid;
}

bool
GameLibrary::validateOpticalMediaForConfirm() const
{
    const auto media = revalidateOpticalDisc(m_portableOpticalObservationId, m_portableOpticalDevice);
    const auto disc = media.value("disc").toMap();
    return media.value("state") == "ready" && m_portableSource.startsWith(disc.value("mountRoot").toString() + '/');
}

bool
GameLibrary::validateInstallDestination() const
{
    const auto parent = QFileInfo(m_portableDestination).absolutePath();
    const auto gamesRoot = ManagedGamePaths::gamesRoot(m_emulatorProfile);
    return !gamesRoot.isEmpty() && QDir().mkpath(parent) && QFileInfo(gamesRoot).canonicalFilePath() == gamesRoot &&
           QFileInfo(parent).canonicalFilePath() == parent && !QFileInfo(parent).isSymLink() &&
           !QFileInfo::exists(m_portableDestination) && !QFileInfo::exists(m_portableDestination + ".staging");
}

bool
GameLibrary::secureInstallDestination() const
{
    const auto parent = QFileInfo(m_portableDestination).absolutePath();
    const auto gamesRoot = ManagedGamePaths::gamesRoot(m_emulatorProfile);
    return QFile::setPermissions(gamesRoot, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner) &&
           QFile::setPermissions(parent, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void
GameLibrary::initializePortableInstallForConfirm()
{
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableCanceled = false;
    m_portablePhase = "install";
    m_portableInstallStatus =
        m_portableDirectoryCopy
            ? "Copying the reviewed game directory into a new private profile folder. Existing installs and saves will "
              "not be overwritten."
            : "Installing into a new private profile folder. Existing installs and saves will not be overwritten.";
}

QStringList
GameLibrary::buildInstallArguments(const QString& helper) const
{
    QStringList arguments{helper,
                          m_portableDirectoryCopy ? "--install-directory" : "--install",
                          "--source",
                          m_portableSource,
                          "--destination",
                          m_portableDestination,
                          "--expected-sha256",
                          m_portableInstallPreview.value("archiveSha256").toString()};
    if (m_portableDirectoryCopy)
        arguments << "--entrypoint" << m_portableDirectoryEntrypoint;
    return arguments;
}

void
GameLibrary::cancelPortableInstall()
{
    m_portableInstallDeadline.stop();
    if (m_portablePhase.isEmpty())
        return;
    const auto phase = m_portablePhase;
    m_portableCanceled = true;
    if (m_portableInstall.state() != QProcess::NotRunning)
    {
        m_portableInstall.terminate();
        if (!m_portableInstall.waitForFinished(2000))
        {
            m_portableInstall.kill();
            m_portableInstall.waitForFinished(2000);
        }
    }
    const auto stage = m_portableDestination + ".staging";
    if (m_portablePhase == "install" && QFileInfo(stage).isDir() && QFileInfo(stage).canonicalFilePath() == stage)
        QDir(stage).removeRecursively();
    const auto snapshot = m_portableDestination + ".staging-source";
    if (m_portablePhase == "install" && QFileInfo(snapshot).isFile() && !QFileInfo(snapshot).isSymLink())
        QFile::remove(snapshot);
    m_portablePhase.clear();
    m_portableDirectoryCopy = false;
    m_portableDirectoryEntrypoint.clear();
    m_portableInstallPreview.clear();
    m_portableGameId.clear();
    if (phase == "uninstall")
        ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, m_portableDestination);
    m_portableInstallStatus =
        phase == "uninstall-finalize"
            ? "Catalog removal completed. Cleanup stopped; inspect the pending managed folder."
        : phase == "uninstall"
            ? "Uninstall canceled. The catalog entry was preserved; inspect the managed folder before retrying."
        : phase == "windows-inspect" ? "Windows executable inspection canceled. No installer was run."
                                     : "Installation canceled. Existing installations and saves were preserved.";
    emit portableInstallChanged();
}

bool
GameLibrary::inspectWindowsInstaller(const QString& source)
{
    if (!m_ready || portableInstallBusy() || !m_portablePhase.isEmpty())
        return fail(tr("Finish or cancel the current game operation first."));
    const QFileInfo file(source);
    if (!file.isFile() || file.isSymLink() || file.canonicalFilePath() != file.absoluteFilePath() || file.size() < 64 ||
        file.size() > 512LL * 1024 * 1024 || !approvedMediaDirectory(m_emulatorProfile, file.absolutePath()))
        return fail(tr("Choose a regular Windows executable under an approved media folder, up to 512 MiB."));
    auto helper = QCoreApplication::applicationDirPath() + "/share/isolde/portable-game-installer.py";
    if (!QFileInfo(helper).isReadable())
        helper = QCoreApplication::applicationDirPath() + "/../share/isolde/portable-game-installer.py";
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde installer inspection helper."));
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableInstallPreview.clear();
    m_portableOpticalObservationId.clear();
    m_portableOpticalDevice.clear();
    m_portableDirectoryCopy = false;
    m_portableDirectoryEntrypoint.clear();
    m_portableCanceled = false;
    m_portablePhase = "windows-inspect";
    m_portableInstallStatus = "Inspecting the executable without executing it. No files will be installed.";
    m_portableInstall.start(python, {helper, "--inspect-windows", "--source", file.canonicalFilePath()});
    m_portableInstallDeadline.start();
    m_error.clear();
    emit errorChanged();
    emit portableInstallChanged();
    return true;
}

bool
GameLibrary::beginWindowsInstall(const QString& source)
{
    if (!m_ready || portableInstallBusy() || !m_portablePhase.isEmpty())
        return fail(tr("Finish or cancel the current game operation first."));
    const QFileInfo file(source);
    if (!file.isFile() || file.isSymLink() || file.canonicalFilePath() != file.absoluteFilePath() || file.size() < 64 ||
        file.size() > 512LL * 1024 * 1024 || !approvedMediaDirectory(m_emulatorProfile, file.absolutePath()))
        return fail(tr("Choose a regular Windows executable under an approved media folder, up to 512 MiB."));
    const auto wine = CompatibilityRuntime::discover();
    if (wine.value("state").toString() != "available" || !wine.value("supported").toBool())
        return fail(tr("Windows compatibility runtime is not available. No installer will be executed."));
    auto helper = QCoreApplication::applicationDirPath() + "/share/isolde/portable-game-installer.py";
    if (!QFileInfo(helper).isReadable())
        helper = QCoreApplication::applicationDirPath() + "/../share/isolde/portable-game-installer.py";
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde installer helper."));
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_windowsInstallPreview.clear();
    m_portableOpticalObservationId.clear();
    m_portableOpticalDevice.clear();
    m_portableDirectoryCopy = false;
    m_portableDirectoryEntrypoint.clear();
    m_portableCanceled = false;
    m_portablePhase = "windows-install";
    m_windowsInstallStatus =
        "Running the Windows installer under Wine. Files will be copied into a new private profile folder.";
    m_windowsInstall.start(python, {helper, "--install-windows", "--source", file.canonicalFilePath()});
    m_windowsInstallDeadline.start();
    m_error.clear();
    emit errorChanged();
    emit windowsInstallChanged();
    return true;
}

void
GameLibrary::cancelWindowsInstall()
{
    m_windowsInstallDeadline.stop();
    if (m_portablePhase != "windows-install")
        return;
    m_portableCanceled = true;
    if (m_windowsInstall.state() != QProcess::NotRunning)
    {
        m_windowsInstall.terminate();
        if (!m_windowsInstall.waitForFinished(2000))
        {
            m_windowsInstall.kill();
            m_windowsInstall.waitForFinished(2000);
        }
    }
    m_portablePhase.clear();
    m_windowsInstallStatus = "Windows installation canceled. Existing files and saves were preserved.";
    emit windowsInstallChanged();
}

bool
GameLibrary::beginImport(const QString& root, const QString& runtime, const QString& platform)
{
    if (!m_ready || scanning() || m_importReady)
        return fail(tr("Finish or cancel the current import first."));
    const auto error = MediaGrants::validate(root);
    if (!error.isEmpty())
        return fail(error);
    if (!compatibleRuntime(runtime, true) || platform.trimmed().isEmpty() || platform.size() > 80)
        return fail(tr("Choose a compatible runtime and platform."));
    const auto formats = m_apps->find(runtime)->launchConfiguration.value("gameFormats").toList();
    if (formats.isEmpty() || formats.size() > 32)
        return fail(tr("This runtime has no verified import formats. Add individual games instead."));
    QStringList extensions;
    for (const auto& format : formats)
    {
        const auto text = format.toString();
        if (format.metaType().id() != QMetaType::QString ||
            !QRegularExpression("^[a-z0-9]{1,8}$").match(text).hasMatch())
            return fail(tr("Invalid runtime import format."));
        extensions.append(text);
    }
    m_importRoot = root;
    m_importRuntime = runtime;
    m_importPlatform = platform;
    m_importFormats = extensions;
    m_preview.clear();
    m_scanEntries = 0;
    m_error.clear();
    emit errorChanged();
    m_scan = std::make_unique<QDirIterator>(
        root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    m_scanTimer.start();
    emit importChanged();
    return true;
}

void
GameLibrary::scanStep()
{
    if (!validScanState())
        return;
    processScanBatch();
    if (!m_scan->hasNext())
        finalizeScan();
    emit importChanged();
}

bool
GameLibrary::validScanState() const
{
    return m_ready && m_scan;
}

void
GameLibrary::processScanBatch()
{
    for (int batch = 0; batch < 32 && m_scan->hasNext(); ++batch)
    {
        const auto path = m_scan->next();
        const auto info = m_scan->fileInfo();
        if (++m_scanEntries > 10000)
        {
            cancelImport();
            fail(tr(
                "This folder exceeds the 10,000-entry import limit. Choose a smaller folder; nothing was imported."));
            return;
        }
        if (!validScanEntry(info, path))
            continue;
        if (!m_importFormats.contains(info.suffix().toLower()))
            continue;
        const auto title = info.completeBaseName();
        if (!validScanTitle(title))
        {
            cancelImport();
            fail(tr("An import title is invalid. Add it manually with a corrected title."));
            return;
        }
        m_preview.append(QVariantMap{{"schemaVersion", 1},
                                     {"title", title},
                                     {"platform", m_importPlatform},
                                     {"runtimeId", m_importRuntime},
                                     {"root", m_importRoot},
                                     {"target", QDir(m_importRoot).relativeFilePath(path)}});
    }
}

bool
GameLibrary::validScanEntry(const QFileInfo& info, const QString& path) const
{
    return info.isFile() && !info.isSymLink() && info.canonicalFilePath() == path &&
           path.startsWith(m_importRoot + '/') && info.isReadable();
}

bool
GameLibrary::validScanTitle(const QString& title) const
{
    return title.trimmed() == title && !title.isEmpty() && title.size() <= 256;
}

void
GameLibrary::finalizeScan()
{
    m_scanTimer.stop();
    m_scan.reset();
    m_importReady = true;
}

QVariantList
GameLibrary::importPage(int offset) const
{
    if (offset < 0 || offset % PageSize || offset >= 10000)
        return {};
    return m_preview.mid(offset, PageSize);
}

bool
GameLibrary::commitImport()
{
    if (!validCommitImportArgs())
        return false;
    bool steam = m_importRuntime == "steam";
    if (!validateCommitRuntime(steam))
        return false;
    if (!ensureProvenanceSchema())
        return false;
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    int count = readGameCount(query);
    bool checkedPlatform = false;
    for (const auto& entry : m_preview)
    {
        if (!processImportEntry(query, entry, steam, count, checkedPlatform))
            return false;
    }
    return finalizeCommitImport();
}

bool
GameLibrary::validCommitImportArgs() const
{
    return !m_portablePhase.startsWith("uninstall") && m_ready && !scanning() && m_importReady;
}

bool
GameLibrary::validateCommitRuntime(bool& steam)
{
    steam = m_importRuntime == "steam";
    if (!compatibleRuntime(m_importRuntime, !steam))
    {
        fail(tr("The selected runtime is no longer installed."));
        return false;
    }
    return true;
}

int
GameLibrary::readGameCount(QSqlQuery& query)
{
    if (!query.exec("SELECT count(*) FROM games") || !query.next())
    {
        query.finish();
        return -1;
    }
    const auto count = query.value(0).toInt();
    query.finish();
    return count;
}

bool
GameLibrary::processImportEntry(QSqlQuery& query, const QVariant& entry, bool steam, int& count, bool& checkedPlatform)
{
    const auto value = entry.toMap();
    QString error;
    if (ComponentSchema::gameRegistration(value, &error).isEmpty() ||
        (!steam && checkedTarget(m_importRoot, value.value("target").toString()).isEmpty()))
    {
        return rollbackImport(error.isEmpty()
                                  ? tr("A source changed after preview. Nothing was imported; review the folder again.")
                                  : error);
    }
    if (!checkExistingGame(query, value))
        return false;
    const bool existing = query.next();
    query.finish();
    if (existing)
        return true;
    if (!checkPlatformLimit(value, checkedPlatform))
        return false;
    if (++count > MaxGames)
    {
        return rollbackImport(tr("Import would exceed the catalog limit. Nothing was imported."));
    }
    return insertImportedGame(query, value, steam);
}

bool
GameLibrary::checkExistingGame(QSqlQuery& query, const QVariantMap& value)
{
    query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?" +
                  QString(m_hasEditions
                              ? " UNION ALL SELECT id FROM game_editions WHERE runtime=? AND root=? AND target=?"
                              : ""));
    query.addBindValue(m_importRuntime);
    query.addBindValue(m_importRoot);
    query.addBindValue(value.value("target"));
    if (m_hasEditions)
    {
        query.addBindValue(m_importRuntime);
        query.addBindValue(m_importRoot);
        query.addBindValue(value.value("target"));
    }
    return query.exec();
}

bool
GameLibrary::checkPlatformLimit(const QVariantMap& value, bool& checkedPlatform)
{
    if (!checkedPlatform)
    {
        checkedPlatform = true;
        if (!platformAvailable(value.value("platform").toString()))
        {
            return rollbackImport(tr("The Games catalog supports up to 128 distinct platforms. Nothing was imported."));
        }
    }
    return true;
}

bool
GameLibrary::insertImportedGame(QSqlQuery& query, const QVariantMap& value, bool steam)
{
    const auto id = "game-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,added) VALUES(?,?,?,?,?,?,unixepoch())");
    query.addBindValue(id);
    for (const auto& key : QStringList{"title", "platform", "runtimeId", "root", "target"})
        query.addBindValue(value.value(key));
    if (!query.exec() || !recordProvenance(id,
                                           "game",
                                           m_importRuntime,
                                           m_importRoot,
                                           value.value("target").toString(),
                                           steam ? "steam-import" : "folder-import"))
    {
        const auto reason = query.lastError().text();
        return rollbackImport(m_error.isEmpty() ? reason : m_error);
    }
    return true;
}

bool
GameLibrary::rollbackImport(const QString& message)
{
    m_db.rollback();
    return fail(message);
}

bool
GameLibrary::finalizeCommitImport()
{
    if (!m_db.commit())
    {
        m_db.rollback();
        return fail(tr("Import could not be saved. Previous data was preserved."));
    }
    cancelImport();
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::recoverUnregisteredManagedInstall(const QString& installationId, const QString& runtimeId, bool confirmed)
{
    if (!validRecoveryArgs(confirmed))
        return fail(tr("Return Home and confirm recovery after all game operations finish."));
    if (!validateRecoveryInstallationId(installationId) || !validateRecoveryRuntime(runtimeId))
        return fail(tr("Choose a valid profile installation and installed file runtime."));
    const auto root = ManagedGamePaths::gamesRoot(m_emulatorProfile) + '/' + installationId + "/install";
    if (!validateInstallRoot(root))
        return fail(tr("The private install or ownership marker is unavailable; nothing was changed."));
    bool referenced = false;
    if (!catalogReferencesRoot(root, &referenced))
        return false;
    if (referenced)
        return fail(tr("The catalog already references this installation."));
    const auto helper = findInstallerHelper("portable-game-installer.py");
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde managed-game verifier."));
    initializeRecoveryState(installationId, runtimeId, root);
    return startRecoveryInspect(python, helper, root);
}

bool
GameLibrary::validRecoveryArgs(bool confirmed) const
{
    return m_ready && confirmed && m_portablePhase.isEmpty() && !portableInstallBusy() &&
           !(m_sessionActiveGuard && m_sessionActiveGuard());
}

bool
GameLibrary::validateRecoveryInstallationId(const QString& installationId) const
{
    const auto uuid = QUuid(installationId);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == installationId;
}

bool
GameLibrary::validateRecoveryRuntime(const QString& runtimeId) const
{
    return compatibleRuntime(runtimeId, true);
}

bool
GameLibrary::validateInstallRoot(const QString& root) const
{
    return ManagedGamePaths::isInstallRoot(m_emulatorProfile, root);
}

void
GameLibrary::initializeRecoveryState(const QString& installationId, const QString& runtimeId, const QString& root)
{
    Q_UNUSED(installationId);
    m_portableGameId.clear();
    m_portableRuntime = runtimeId;
    m_portableDestination = root;
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableCanceled = false;
    m_portableInstallPreview.clear();
    m_portablePhase = "recover";
    m_portableInstallStatus =
        "Verifying the complete managed install inventory and entry point before restoring its catalog entry.";
    m_error.clear();
    emit errorChanged();
    emit portableInstallChanged();
}

bool
GameLibrary::startRecoveryInspect(const QString& python, const QString& helper, const QString& root)
{
    m_portableInstall.start(python, {helper, "--inspect-installed", "--root", root});
    m_portableInstallDeadline.start();
    return true;
}

bool
GameLibrary::recoverOrphanedUninstall(const QString& installationId, bool confirmed)
{
    if (!m_ready || !confirmed || !m_portablePhase.isEmpty() || portableInstallBusy() ||
        (m_sessionActiveGuard && m_sessionActiveGuard()))
        return fail(tr("Return Home and confirm recovery after all game operations finish."));
    const auto uuid = QUuid(installationId);
    if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != installationId)
        return fail(tr("Invalid managed installation identity."));
    const auto root = ManagedGamePaths::gamesRoot(m_emulatorProfile) + '/' + installationId + "/install";
    bool referenced = false;
    if (!catalogReferencesRoot(root, &referenced))
        return false;
    if (referenced)
        return fail(tr("The catalog still references this installation. Reopen Games for normal recovery."));
    const auto pending = root + ".uninstall-pending";
    if (!QFileInfo(pending).isDir() || QFileInfo(pending).isSymLink())
        return fail(tr("The pending folder is missing or redirected; nothing was changed."));
    const auto error = ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, root, false);
    if (!error.isEmpty())
        return fail(error);
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::resumeOrphanedUninstall(const QString& installationId, bool confirmed)
{
    if (!validResumeUninstallArgs(confirmed))
        return fail(tr("Return Home and confirm cleanup after all game operations finish."));
    if (!validateResumeInstallationId(installationId))
        return fail(tr("Invalid managed installation identity."));
    const auto root = ManagedGamePaths::gamesRoot(m_emulatorProfile) + '/' + installationId + "/install";
    bool referenced = false;
    if (!catalogReferencesRoot(root, &referenced))
        return false;
    if (referenced)
        return fail(tr("The catalog still references this installation. Reopen Games for normal recovery."));
    if (!validateResumePendingState(root))
        return fail(
            tr("The pending folder is missing, redirected, or conflicts with an installation; nothing was changed."));
    const auto helper = findInstallerHelper("portable-game-installer.py");
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde managed-game verifier."));
    initializeResumeUninstallState(installationId, root);
    return startResumeUninstall(python, helper, root);
}

bool
GameLibrary::validResumeUninstallArgs(bool confirmed) const
{
    return m_ready && confirmed && m_portablePhase.isEmpty() && !portableInstallBusy() &&
           !(m_sessionActiveGuard && m_sessionActiveGuard());
}

bool
GameLibrary::validateResumeInstallationId(const QString& installationId) const
{
    const auto uuid = QUuid(installationId);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == installationId;
}

bool
GameLibrary::validateResumePendingState(const QString& root) const
{
    const QFileInfo pending(root + ".uninstall-pending");
    return ManagedGamePaths::isInstallRoot(m_emulatorProfile, root, false) && !QFileInfo::exists(root) &&
           !QFileInfo(root).isSymLink() && pending.isDir() && !pending.isSymLink() &&
           pending.canonicalFilePath() == pending.absoluteFilePath();
}

void
GameLibrary::initializeResumeUninstallState(const QString& installationId, const QString& root)
{
    Q_UNUSED(installationId);
    m_portableGameId.clear();
    m_portableDestination = root;
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableCanceled = false;
    m_portablePhase = "uninstall-finalize";
    m_portableInstallStatus =
        "Checking remaining managed files before resuming cleanup. Modified and unowned files are preserved.";
    m_error.clear();
    emit errorChanged();
    emit portableInstallChanged();
}

bool
GameLibrary::startResumeUninstall(const QString& python, const QString& helper, const QString& root)
{
    m_portableInstall.start(python, {helper, "--resume-uninstall", "--root", root});
    m_portableInstallDeadline.start();
    return true;
}

bool
GameLibrary::uninstallManagedGame(const QString& id, bool confirmed)
{
    if (m_sessionActiveGuard && m_sessionActiveGuard())
        return fail(tr("A session is active or starting. Return Home before uninstalling a game."));
    if (!m_ready || !confirmed || portableInstallBusy() || !m_portablePhase.isEmpty() || !canUninstallManagedGame(id))
        return fail(tr("Only a separately confirmed, verified Isolde-managed install can be uninstalled."));
    auto helper = QCoreApplication::applicationDirPath() + "/share/isolde/portable-game-installer.py";
    if (!QFileInfo(helper).isReadable())
        helper = QCoreApplication::applicationDirPath() + "/../share/isolde/portable-game-installer.py";
    const auto python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty() || !QFileInfo(helper).isReadable())
        return fail(tr("Repair the Isolde managed-game verifier."));
    const auto record = registration(id);
    const auto root = record.value("root").toString();
    if (!ManagedGamePaths::isInstallRoot(m_emulatorProfile, root))
        return fail(tr("The managed installation changed; no files were removed."));
    m_portableGameId = id;
    m_portableRuntime.clear();
    m_portableDestination = root;
    m_portableInstallOutput.clear();
    m_portableInstallError.clear();
    m_portableCanceled = false;
    m_portablePhase = "uninstall";
    m_portableInstallStatus =
        "Verifying the complete owned-file manifest before removing this install. Profile saves "
        "are outside the install folder.";
    m_portableInstall.start(python, {helper, "--prepare-uninstall", "--root", root});
    m_portableInstallDeadline.start();
    m_error.clear();
    emit errorChanged();
    emit portableInstallChanged();
    return true;
}
