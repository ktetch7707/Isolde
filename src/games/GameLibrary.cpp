#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUrl>
#include "CompatibilityRuntime.h"
#include "GameEmulators.h"
#include "GameLibrary.h"
#include "GameResourceLimits.h"
#include "ManagedGamePaths.h"
#include "OpticalDiscIdentity.h"
#include "OpticalDiscGrants.h"
#include "SteamLibraryGrants.h"
#include "app/AppCatalog.h"
#include "app/AppRegistry.h"
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/file.h>
#endif
#endif
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <cmath>
#include <limits>
#include "home/ComponentSchema.h"
#include "media/MediaGrants.h"

namespace {
QString sqlText(const QVariant& value) {
    const auto text = value.toString();
    return text.isNull() ? QStringLiteral("") : text;
}
constexpr int MaxGames = GamesResourceLimits::CatalogEntries;
constexpr int PageSize = GamesResourceLimits::PageEntries;
constexpr int MaxPlatforms = GamesResourceLimits::Platforms;
constexpr int MaxCollections = GamesResourceLimits::Collections;
constexpr int MaxProvenanceRecords = GamesResourceLimits::ProvenanceRecords;
constexpr int MaxTags = 32;
constexpr qsizetype MaxCatalogArchiveBytes = GamesResourceLimits::CatalogArchiveBytes;
constexpr qsizetype MaxArtworkSourceBytes = 16 * 1024 * 1024;
constexpr qsizetype MaxManagedArtworkBytes = 12 * 1024 * 1024;
constexpr qint64 MaxArtworkPixels = 16 * 1024 * 1024;
constexpr qint64 MaxArtworkStoreBytes = GamesResourceLimits::ArtworkStoreBytes;
constexpr qint64 MaxRestoreArtworkBytes = GamesResourceLimits::ArtworkRestoreBytes;
bool exactKeys(const QJsonObject& object, const QStringList& keys) {
    QStringList actual = object.keys();
    auto expected = keys;
    actual.sort();
    expected.sort();
    return actual == expected;
}
bool archiveInteger(const QJsonValue& value, qint64* result) {
    if (!value.isDouble() || !std::isfinite(value.toDouble()) || std::floor(value.toDouble()) != value.toDouble() ||
        value.toDouble() < 0 || value.toDouble() > 9007199254740991.0)
        return false;
    *result = qint64(value.toDouble());
    return true;
}
bool approvedMediaDirectory(const QString& profile, const QString& path) {
    if (!MediaGrants::validate(path).isEmpty())
        return false;
    for (const auto& grant : MediaGrants::folders(profile)) {
        if (!MediaGrants::validate(grant, false).isEmpty())
            continue;
        if (isGrantPathMatch(path, grant))
            return true;
    }
    return false;
}

bool isGrantPathMatch(const QString& path, const QString& grant) {
    return path == grant || path.startsWith(grant + '/');
}
QByteArray normalizeArtwork(const QByteArray& source) {
    if (!validArtworkSourceBytes(source))
        return {};
    QBuffer input;
    input.setData(source);
    if (!input.open(QIODevice::ReadOnly))
        return {};
    QImageReader reader(&input);
    reader.setAutoTransform(true);
    const auto format = reader.format().toLower();
    const auto size = reader.size();
    if (!validArtworkFormat(format, size))
        return {};
    const auto image = reader.read();
    if (!validArtworkImage(image))
        return {};
    QByteArray normalized;
    QBuffer output(&normalized);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !validManagedArtworkBytes(normalized))
        return {};
    return normalized;
}

bool validArtworkSourceBytes(const QByteArray& source) {
    return !source.isEmpty() && source.size() <= MaxArtworkSourceBytes;
}

bool validArtworkFormat(const QByteArray& format, const QSize& size) {
    return (format == "png" || format == "jpg" || format == "jpeg") && size.width() >= 1 && size.height() >= 1 &&
           size.width() <= 4096 && size.height() <= 4096 &&
           qint64(size.width()) * size.height() <= MaxArtworkPixels;
}

bool validArtworkImage(const QImage& image) {
    return !image.isNull() && image.width() <= 4096 && image.height() <= 4096 &&
           qint64(image.width()) * image.height() <= MaxArtworkPixels;
}

bool validManagedArtworkBytes(const QByteArray& normalized) {
    return !normalized.isEmpty() && normalized.size() <= MaxManagedArtworkBytes;
}

QString artworkHash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
bool persistArtwork(const QString& databasePath, const QByteArray& bytes, QString* hash, QString* error) {
    if (!validArtworkBytes(bytes)) {
        if (error)
            *error = QStringLiteral("Artwork exceeds the managed size limit.");
        return false;
    }
    const auto directory = databasePath + ".artwork";
    if (!prepareArtworkDirectory(directory)) {
        if (error)
            *error = QStringLiteral("Cannot create a private artwork folder.");
        return false;
    }
    if (!secureArtworkDirectory(directory)) {
        if (error)
            *error = QStringLiteral("Cannot secure the private artwork folder.");
        return false;
    }
    const auto digest = artworkHash(bytes), path = directory + "/" + digest + ".png";
    const auto info = QFileInfo(path);
    if (info.exists()) {
        if (!verifyExistingArtwork(info, path, bytes)) {
            if (error)
                *error = QStringLiteral("Managed artwork content does not match its identity.");
            return false;
        }
    } else if (!storeArtworkFile(directory, path, bytes)) {
        if (error)
            *error = QStringLiteral("Could not save managed artwork.");
        return false;
    }
    if (hash)
        *hash = digest;
    return true;
}

bool validArtworkBytes(const QByteArray& bytes) {
    return !bytes.isEmpty() && bytes.size() <= MaxManagedArtworkBytes;
}

bool prepareArtworkDirectory(const QString& directory) {
    if (!QDir().mkpath(directory))
        return false;
    const auto info = QFileInfo(directory);
    return !info.isSymLink() && info.canonicalFilePath() == QDir::cleanPath(directory);
}

bool secureArtworkDirectory(const QString& directory) {
    return QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

bool verifyExistingArtwork(const QFileInfo& info, const QString& path, const QByteArray& bytes) {
    if (info.isSymLink() || !info.isFile() || info.canonicalFilePath() != path || info.size() != bytes.size())
        return false;
    QFile existing(path);
    return existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes;
}

bool storeArtworkFile(const QString& directory, const QString& path, const QByteArray& bytes) {
    qint64 used = 0;
    const auto files = QDir(directory).entryInfoList({"*.png"}, QDir::Files | QDir::NoSymLinks);
    for (const auto& file : files)
        used += file.size();
    if (used + bytes.size() > MaxArtworkStoreBytes)
        return false;
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) &&
           file.write(bytes) == bytes.size() && file.commit();
}

QVariantMap row(const QSqlQuery& query, const QString& artworkDirectory) {
    QStringList tags;
    const auto tagArray = QJsonDocument::fromJson(query.value(6).toByteArray()).array();
    for (const auto& tag : tagArray)
        if (tag.isString())
            tags.append(tag.toString());
    QStringList aliases;
    const auto aliasArray = QJsonDocument::fromJson(query.value(7).toByteArray()).array();
    for (const auto& alias : aliasArray)
        if (alias.isString())
            aliases.append(alias.toString());
    const auto hash = query.value(9).toString();
    const auto artworkPath = artworkDirectory.isEmpty() ? QString() : artworkDirectory + "/" + hash + ".png";
    const auto artworkInfo = QFileInfo(artworkPath);
    const auto artwork = QRegularExpression("^[0-9a-f]{64}$").match(hash).hasMatch() && artworkInfo.isFile() &&
                                 !artworkInfo.isSymLink() && artworkInfo.canonicalFilePath() == artworkPath &&
                                 artworkInfo.size() > 0 && artworkInfo.size() <= MaxManagedArtworkBytes
                             ? QUrl::fromLocalFile(artworkPath).toString()
                             : QString();
    return {{"id", query.value(0)},
            {"title", query.value(1)},
            {"platform", query.value(2)},
            {"runtimeId", query.value(3)},
            {"favorite", query.value(4).toBool()},
            {"mediaType", "game"},
            {"mediaId", query.value(0)},
            {"franchise", query.value(5).toString()},
            {"tags", tags},
            {"aliases", aliases},
            {"summary", query.value(8).toString()},
            {"artwork", artwork}};
}
QString sourceKind(const QString& runtime, const QString& root, const QString& profile) {
    if (isSteamRuntime(runtime))
        return "steam-app";
    if (OpticalDiscIdentify::isRootReference(root))
        return "optical-media";
    if (ManagedGamePaths::isInstallRoot(profile, root, false))
        return "isolde-managed-install";
    if (!root.isEmpty())
        return "external-folder";
    return "runtime-entry";
}

bool isSteamRuntime(const QString& runtime) {
    return runtime == "steam";
}
QVariantMap sourceProvenance(const QString& runtime,
                             const QString& root,
                             const QString& target,
                             const QString& profile,
                             const QString& originOverride = {}) {
    const auto kind = sourceKind(runtime, root, profile);
    QString origin = "manual-registration";
    const auto identity = runtime + "\n" + root + "\n" + target;
    if (!originOverride.isEmpty())
        origin = originOverride;
    QString reference = target;
    if (kind == "steam-app")
        reference = target;
    else if (OpticalDiscIdentify::isRootReference(root))
        reference = OpticalDiscIdentify::mediaIdFromRoot(root);
    else if (!root.isEmpty())
        reference = QDir(root).filePath(target);
    return {{"sourceKind", kind},
            {"origin", origin},
            {"sourceReference", reference},
            {"sourceFingerprint",
             QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex())}};
}
} // namespace

GameLibrary::GameLibrary(const QString& path, AppRegistry* apps, QObject* parent)
    : QObject(parent), m_connection("games-" + QUuid::createUuid().toString(QUuid::WithoutBraces)), m_apps(apps) {
    connect(apps, &AppRegistry::appsChanged, this, &GameLibrary::changed);
    m_emulatorProfile = qApp->property("isoldeProfile").toString();
    if (m_emulatorProfile.isEmpty())
        m_emulatorProfile = "default";
    m_steamScanDeadline.setSingleShot(true);
    m_steamScanDeadline.setInterval(30000);
    connect(&m_steamScanDeadline, &QTimer::timeout, this, [this] {
        fail(tr("Steam discovery timed out. Nothing was imported; retry when storage is available."));
        cancelImport();
    });
    connect(&m_steamScan, &QProcess::errorOccurred, this, &GameLibrary::onSteamScanError);
    connect(&m_steamScan, &QProcess::readyReadStandardOutput, this, &GameLibrary::onSteamScanOutput);
    connect(&m_steamScan,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &GameLibrary::onSteamScanFinished);
    m_setupDeadline.setSingleShot(true);
    m_setupDeadline.setInterval(2400000);
    connect(&m_setupDeadline, &QTimer::timeout, this, &GameLibrary::cancelEmulatorSetup);
    connect(&m_setup, &QProcess::readyReadStandardOutput, this, &GameLibrary::onSetupOutput);
    connect(&m_setup, &QProcess::readyReadStandardError, this, &GameLibrary::onSetupOutput);
    connect(&m_setup, &QProcess::errorOccurred, this, &GameLibrary::onSetupError);
    connect(&m_setup,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &GameLibrary::onSetupFinished);
    m_portableInstallDeadline.setSingleShot(true);
    m_portableInstallDeadline.setInterval(900000);
    connect(&m_portableInstallDeadline, &QTimer::timeout, this, &GameLibrary::cancelPortableInstall);
    connect(&m_portableInstall, &QProcess::readyReadStandardOutput, this, &GameLibrary::onPortableInstallOutput);
    connect(&m_portableInstall, &QProcess::readyReadStandardError, this, &GameLibrary::onPortableInstallError);
    connect(&m_portableInstall, &QProcess::errorOccurred, this, &GameLibrary::onPortableInstallErrorOccurred);
    connect(
        &m_portableInstall,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &GameLibrary::onPortableInstallFinished);
    m_windowsInstallDeadline.setSingleShot(true);
    m_windowsInstallDeadline.setInterval(900000);
    connect(&m_windowsInstallDeadline, &QTimer::timeout, this, &GameLibrary::cancelWindowsInstall);
    connect(&m_windowsInstall, &QProcess::readyReadStandardOutput, this, &GameLibrary::onWindowsInstallOutput);
    connect(&m_windowsInstall, &QProcess::readyReadStandardError, this, &GameLibrary::onWindowsInstallError);
    connect(&m_windowsInstall, &QProcess::errorOccurred, this, &GameLibrary::onWindowsInstallErrorOccurred);
    connect(&m_windowsInstall,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &GameLibrary::onWindowsInstallFinished);
    m_opticalRefreshTimer.setInterval(2500);
    connect(&m_opticalRefreshTimer, &QTimer::timeout, this, &GameLibrary::onOpticalRefreshTimeout);
    m_scanTimer.setInterval(0);
    connect(&m_scanTimer, &QTimer::timeout, this, &GameLibrary::scanStep);
    m_catalogExisted = QFileInfo::exists(path);
    if (!validateCatalogPath()) return;
    if (!openCatalogDatabase()) return;
    QSqlQuery query(m_db);
    if (!validateCatalogVersion(query)) return;
    if (!configureCatalogDurability(query)) return;
    if (!initializeCatalogSchema(query)) return;
    if (!validateCatalogSchemas(query)) return;
    if (!validateCatalogProvenance(query)) return;
    if (!checkPendingRecoveries(query)) return;
    query.finish();

void GameLibrary::onSteamScanFinished(int code, QProcess::ExitStatus status) {
    m_steamScanDeadline.stop();
    if (m_steamScanCanceled || !m_ready)
        return;
    if (code != 0 || status != QProcess::NormalExit) {
        const auto error = QString::fromUtf8(m_steamScan.readAllStandardError()).right(1000);
        fail(tr("Steam discovery failed. Nothing was imported. %1").arg(error));
        cancelImport();
        return;
    }
    m_steamScanOutput += m_steamScan.readAllStandardOutput();
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(m_steamScanOutput, &parse).toVariant().toMap();
    const auto games = document.value("games").toList();
    if (!validSteamDiscovery(parse.error, document, games)) {
        fail(tr("Invalid Steam discovery response."));
        cancelImport();
        return;
    }
    QSet<QString> seen;
    for (const auto& entry : games) {
        const auto game = entry.toMap();
        if (!validSteamTitle(game, seen)) {
            fail(tr("Invalid or duplicate Steam title. Nothing was imported."));
            cancelImport();
            return;
        }
    }
    m_preview = games;
    m_importReady = true;
    m_importStatus = games.isEmpty() ? tr("No fully installed games were found in this profile. Open Steam "
                                          "once to sign in and install games, then retry.")
                                     : tr("%1 installed Steam games. Review before adding. Steam owns "
                                          "licensing, installation and saves.")
                                             .arg(games.size());
    const auto warnings = document.value("warnings").toStringList();
    if (!warnings.isEmpty())
        m_importStatus += ' ' + warnings.join(' ');
    emit importChanged();
}

bool GameLibrary::validSteamDiscovery(QJsonParseError::ParseError parseError, const QVariantMap& document,
                                      const QVariantList& games) const {
    return parseError == QJsonParseError::NoError &&
           document.value("games").metaType() == QMetaType::fromType<QVariantList>() && games.size() <= 5000 &&
           m_steamScanOutput.size() <= 2 * 1024 * 1024;
}

bool GameLibrary::validSteamTitle(const QVariantMap& game, QSet<QString>& seen) const {
    return !ComponentSchema::gameRegistration(game).isEmpty() && game.value("runtimeId") == "steam" &&
           game.value("root").toString().isEmpty() && !seen.contains(game.value("target").toString());
}

void GameLibrary::onSteamScanError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart && !m_steamScanCanceled) {
        fail(tr("Steam discovery could not start. Repair the Isolde Steam adapter and retry."));
        cancelImport();
    }
}

void GameLibrary::onSteamScanOutput() {
    m_steamScanOutput += m_steamScan.readAllStandardOutput();
    if (m_steamScanOutput.size() > 2 * 1024 * 1024) {
        fail(tr("Steam preview exceeds its size limit. Nothing was imported."));
        cancelImport();
    }
}

void GameLibrary::onSetupOutput(const QByteArray& output) {
    m_setupOutput = (m_setupOutput + output).right(8192);
    const auto lines = QString::fromLocal8Bit(output).split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty())
        m_setupStatus = tr("Installing verified runtimes Â· %1").arg(lines.constLast().trimmed().left(160));
    emit setupChanged();
}

void GameLibrary::onSetupError() {
    m_setupStatus = "Emulator setup could not start. " + m_setup.errorString();
    emit setupChanged();
}

void GameLibrary::onSetupFinished(int code, QProcess::ExitStatus status) {
    m_setupDeadline.stop();
    if (!m_ready || m_setupCanceled) {
        m_setupStatus = "Setup stopped. Existing games and saves were preserved. Review runtime "
                        "installation before retrying.";
        emit setupChanged();
        return;
    }
    QFile receipt(m_setupReceipt);
    QVariantMap result;
    if (receipt.open(QIODevice::ReadOnly) && receipt.size() < 65536)
        result = QJsonDocument::fromJson(receipt.readAll()).toVariant().toMap();
    if (code != 0 || status != QProcess::NormalExit || !result.value("ready").toBool()) {
        m_setupStatus = "Emulator installation failed. Existing games and saves were preserved. " +
                        QString::fromUtf8(m_setupOutput).right(1200);
        emit setupChanged();
        return;
    }
    bool activated = true;
    for (const auto& item : emulators()) {
        const auto runtime = item.toMap();
        if (!runtime.value("installed").toBool() || !m_catalog ||
            !m_catalog->install(runtime.value("id").toString())) {
            activated = false;
            break;
        }
    }
    m_setupStatus =
        activated
            ? "Emulators installed and available in Add game. Your games and firmware were not downloaded."
            : "Runtime files installed, but activation needs repair. " +
                  (m_catalog ? m_catalog->errorString() : QString("Store unavailable."));
    emit changed();
    emit setupChanged();
}

void GameLibrary::onPortableInstallOutput() {
    m_portableInstallOutput += m_portableInstall.readAllStandardOutput();
    if (m_portableInstallOutput.size() > 65536)
        cancelPortableInstall();
}

void GameLibrary::onPortableInstallError() {
    m_portableInstallError = (m_portableInstallError + m_portableInstall.readAllStandardError()).right(8192);
}

void GameLibrary::onPortableInstallErrorOccurred(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        m_portableInstallStatus =
            m_portablePhase == "uninstall-finalize"
                ? "Catalog updated. Cleanup could not start; the pending managed folder was retained."
                : "Portable game helper could not start.";
        m_portablePhase.clear();
        m_portableDirectoryCopy = false;
        emit portableInstallChanged();
    }
}

void GameLibrary::onPortableInstallFinished(int code, QProcess::ExitStatus status) {
    m_portableInstallDeadline.stop();
    if (!m_ready || m_portablePhase.isEmpty())
        return;
    if (code != 0 || status != QProcess::NormalExit) {
        onPortableInstallFailed();
        return;
    }
    m_portableInstallOutput += m_portableInstall.readAllStandardOutput();
    QJsonParseError parse;
    const auto metadata = QJsonDocument::fromJson(m_portableInstallOutput, &parse).toVariant().toMap();
    if (m_portablePhase == "recover") {
        onPortableInstallRecover(metadata, parse);
        return;
    }
    if (m_portablePhase == "uninstall" && m_portableCanceled) {
        onPortableInstallUninstallCanceled();
        return;
    }
    if (m_portablePhase == "uninstall") {
        onPortableInstallUninstall(metadata, parse);
        return;
    }
    if (m_portablePhase == "uninstall-finalize") {
        onPortableInstallUninstallFinalize(metadata, parse);
        return;
    }
    if (m_portablePhase == "windows-inspect") {
        onPortableInstallWindowsInspect(metadata, parse);
        return;
    }
    if (parse.error != QJsonParseError::NoError || metadata.value("schemaVersion").toInt() != 1 ||
        metadata.value("title").toString().isEmpty() || metadata.value("platform").toString().isEmpty()) {
        m_portableInstallStatus = "Invalid portable game adapter response. Nothing was added to Games.";
        m_portablePhase.clear();
        emit portableInstallChanged();
        return;
    }
    if (m_portablePhase == "inspect") {
        onPortableInstallInspect(metadata);
        return;
    }
    if (m_portablePhase == "install") {
        onPortableInstallInstall(metadata);
        return;
    }
}

void GameLibrary::onPortableInstallFailed() {
    const auto reason = QString::fromUtf8(m_portableInstallError).right(1000);
    const auto stage = m_portableDestination + ".staging";
    if (m_portablePhase == "install" && QFileInfo(stage).isDir() &&
        QFileInfo(stage).canonicalFilePath() == stage)
        QDir(stage).removeRecursively();
    const auto snapshot = m_portableDestination + ".staging-source";
    if (m_portablePhase == "install" && QFileInfo(snapshot).isFile() && !QFileInfo(snapshot).isSymLink())
        QFile::remove(snapshot);
    const auto recovery =
        m_portablePhase == "uninstall"
            ? ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, m_portableDestination)
            : QString();
    m_portableInstallStatus =
        m_portablePhase == "uninstall"
            ? "Safe uninstall stopped or failed. The catalog entry was preserved. " + recovery + " " + reason
        : m_portablePhase == "uninstall-finalize"
            ? "Catalog removal completed, but cleanup stopped. The pending managed folder was retained for recovery. " +
                  reason
        : m_portablePhase == "recover"
            ? "Managed install verification failed. Nothing was added to the catalog; files were preserved. " + reason
        : m_portablePhase == "windows-inspect"
            ? "Windows executable inspection stopped or failed. No installer was run. " + reason
        : m_portableCanceled
            ? "Install canceled. Existing files and saves were preserved."
            : "Portable game preflight/install failed. Nothing was added to the catalog. " + reason;
    m_portablePhase.clear();
    m_portableDirectoryCopy = false;
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallRecover(const QVariantMap& metadata, const QJsonParseError& parse) {
    const auto root = m_portableDestination;
    bool referenced = false;
    const bool valid =
        parse.error == QJsonParseError::NoError && metadata.value("schemaVersion").toInt() == 1 &&
        !metadata.value("title").toString().trimmed().isEmpty() &&
        !metadata.value("platform").toString().trimmed().isEmpty() &&
        metadata.value("title").toString().size() <= 256 &&
        metadata.value("platform").toString().size() <= 80 &&
        QRegularExpression("^[0-9a-f]{64}$").match(metadata.value("archiveSha256").toString()).hasMatch() &&
        ManagedGamePaths::isInstallRoot(m_emulatorProfile, root) &&
        compatibleRuntime(m_portableRuntime, true) && catalogReferencesRoot(root, &referenced) && !referenced;
    if (valid) {
        const auto id = addGameWithOrigin({{"schemaVersion", 1},
                                           {"title", metadata.value("title")},
                                           {"platform", metadata.value("platform")},
                                           {"runtimeId", m_portableRuntime},
                                           {"root", root},
                                           {"target", metadata.value("entrypoint")}},
                                          "portable-install");
        m_portableInstallStatus = id.isEmpty() ? "Verified install remains unregistered. Resolve the "
                                                   "catalog error and retry recovery; files were preserved."
                                               : "Verified managed install was restored to the catalog. No "
                                                 "game files or saves were changed.";
    } else {
        m_portableInstallStatus =
            "Install verification or catalog checks failed. Nothing was added; files were preserved.";
    }
    m_portablePhase.clear();
    m_portableDirectoryCopy = false;
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallUninstallCanceled() {
    const auto recovery =
        ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, m_portableDestination);
    m_portableInstallStatus = "Uninstall canceled. The catalog entry was preserved. " + recovery;
    m_portableGameId.clear();
    m_portablePhase.clear();
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallUninstall(const QVariantMap& metadata, const QJsonParseError& parse) {
    if (parse.error != QJsonParseError::NoError || metadata.value("schemaVersion").toInt() != 1 ||
        !metadata.value("staged").toBool()) {
        const auto recovery =
            ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, m_portableDestination);
        m_portableInstallStatus =
            "The ownership verification response was invalid. The catalog entry was preserved. " + recovery;
    } else {
        m_portablePhase.clear();
        if (removeGame(m_portableGameId)) {
            auto helper =
                QCoreApplication::applicationDirPath() + "/share/isolde/portable-game-installer.py";
            if (!QFileInfo(helper).isReadable())
                helper =
                    QCoreApplication::applicationDirPath() + "/../share/isolde/portable-game-installer.py";
            const auto python = QStandardPaths::findExecutable("python3");
            if (!python.isEmpty() && QFileInfo(helper).isReadable()) {
                m_portablePhase = "uninstall-finalize";
                m_portableInstallOutput.clear();
                m_portableInstallError.clear();
                m_portableInstallStatus =
                    "Catalog updated. Removing the verified staged installation; saves are preserved.";
                m_portableInstall.start(python,
                                        {helper, "--finalize-uninstall", "--root", m_portableDestination});
                m_portableInstallDeadline.start();
                emit portableInstallChanged();
                return;
            }
            m_portableInstallStatus = "Catalog updated. The staged install was retained because the "
                                      "cleanup helper is unavailable.";
        } else {
            const auto recovery =
                ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, m_portableDestination);
            m_portableInstallStatus =
                "Catalog update failed. The managed installation was retained. " + recovery;
        }
    }
    m_portableGameId.clear();
    m_portablePhase.clear();
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallUninstallFinalize(const QVariantMap& metadata, const QJsonParseError& parse) {
    m_portableInstallStatus =
        parse.error == QJsonParseError::NoError && metadata.value("schemaVersion").toInt() == 1 &&
                metadata.value("removed").toBool()
            ? "Verified Isolde-owned files removed. Runtime profile and saves were preserved."
            : "Catalog updated, but cleanup could not be confirmed. Inspect the pending managed folder.";
    m_portableGameId.clear();
    m_portablePhase.clear();
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallWindowsInspect(const QVariantMap& metadata, const QJsonParseError& parse) {
    const auto digest = metadata.value("sha256").toString();
    if (parse.error != QJsonParseError::NoError || metadata.value("schemaVersion").toInt() != 1 ||
        !QRegularExpression("^[0-9a-f]{64}$").match(digest).hasMatch() ||
        !QStringList{"PE32", "PE32+"}.contains(metadata.value("format").toString()) ||
        !QStringList{"x86", "x86-64", "arm64", "other"}.contains(
            metadata.value("architecture").toString()) ||
        metadata.value("supported").toBool()) {
        m_portableInstallStatus = "Windows executable inspection failed. No installer was run.";
    } else {
        m_portableInstallStatus =
            tr("Windows executable: %1 Â· %2 Â· %3 bytes\nSHA-256: %4\n"
               "Inspection does not verify its publisher, ownership, safety or installer type. "
               "Installation remains unsupported; no code was run and no files were installed.")
                .arg(metadata.value("format").toString(), metadata.value("architecture").toString())
                .arg(metadata.value("fileSize").toLongLong())
                .arg(digest);
    }
    m_portablePhase.clear();
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallInspect(const QVariantMap& metadata) {
    const auto* runtime = m_apps->find(m_portableRuntime);
    const auto digest = metadata.value("archiveSha256").toString();
    const auto format = metadata.value("archiveFormat").toString();
    const auto entrySize = metadata.value("entrySize").toLongLong();
    const auto entries = metadata.value("archiveEntries").toInt();
    const auto unpacked = metadata.value("unpackedBytes").toLongLong();
    if (!runtime || !compatibleRuntime(m_portableRuntime, true)) {
        m_portableInstallStatus =
            "The selected runtime is no longer available. Choose a runtime and inspect the bundle again.";
        m_portablePhase.clear();
        emit portableInstallChanged();
        return;
    }
    if (!QRegularExpression("^[0-9a-f]{64}$").match(digest).hasMatch() ||
        (m_portableDirectoryCopy &&
         (format != "directory-copy" ||
          metadata.value("entrypoint").toString() != m_portableDirectoryEntrypoint || entrySize < 1 ||
          entrySize > 512LL * 1024 * 1024 || entries < 1 || entries > 10000 || unpacked < 1 ||
          unpacked > 2LL * 1024 * 1024 * 1024))) {
        m_portableInstallStatus =
            "Portable game preflight returned invalid bounded metadata. No files were copied.";
        m_portablePhase.clear();
        m_portableDirectoryCopy = false;
        emit portableInstallChanged();
        return;
    }
    m_portableInstallPreview = metadata;
    m_portableInstallPreview["destination"] = m_portableDestination;
    m_portableInstallPreview["runtimeName"] = runtime->name;
    m_portablePhase = "review";
    m_portableInstallStatus =
        "Preflight complete. Review runtime, destination and side effects before Install.";
    emit portableInstallChanged();
}

void GameLibrary::onPortableInstallInstall(const QVariantMap& metadata) {
    if (metadata.value("archiveSha256") != m_portableInstallPreview.value("archiveSha256") ||
        metadata.value("title") != m_portableInstallPreview.value("title") ||
        metadata.value("platform") != m_portableInstallPreview.value("platform") ||
        metadata.value("entrypoint") != m_portableInstallPreview.value("entrypoint") ||
        metadata.value("archiveFormat") != m_portableInstallPreview.value("archiveFormat") ||
        metadata.value("entrySize") != m_portableInstallPreview.value("entrySize") ||
        metadata.value("archiveEntries") != m_portableInstallPreview.value("archiveEntries") ||
        metadata.value("unpackedBytes") != m_portableInstallPreview.value("unpackedBytes")) {
        m_portableInstallStatus =
            "The selected package changed after preflight. Nothing was added to Games.";
        m_portablePhase.clear();
        emit portableInstallChanged();
        return;
    }
    const auto root = m_portableDestination;
    if (!ManagedGamePaths::isInstallRoot(m_emulatorProfile, root)) {
        m_portableInstallStatus =
            "Installed package failed private destination verification; it was not added to Games.";
        m_portablePhase.clear();
        emit portableInstallChanged();
        return;
    }
    const auto id = addGameWithOrigin({{"schemaVersion", 1},
                                       {"title", metadata.value("title")},
                                       {"platform", metadata.value("platform")},
                                       {"runtimeId", m_portableRuntime},
                                       {"root", root},
                                       {"target", metadata.value("entrypoint")}},
                                      "portable-install");
    m_portableInstallStatus =
        id.isEmpty() ? "Package installed but catalog registration failed. Preserve the private recovery "
                       "folder and retry from Games."
                     : m_portableDirectoryCopy
                         ? "Game directory and assets copied into a private installation and added to Games. Launch "
                           "uses an isolated session; no setup program was run."
                         : "Installed, verified and added to Games. Launch uses an isolated session; setup scripts are "
                           "never executed.";
    m_portablePhase.clear();
    m_portableDirectoryCopy = false;
    m_portableDirectoryEntrypoint.clear();
    m_portableInstallPreview.clear();
    emit portableInstallChanged();
}

void GameLibrary::onWindowsInstallOutput() {
    m_portableInstallOutput += m_windowsInstall.readAllStandardOutput();
    if (m_portableInstallOutput.size() > 65536)
        cancelWindowsInstall();
}

void GameLibrary::onWindowsInstallError() {
    m_portableInstallError = (m_portableInstallError + m_windowsInstall.readAllStandardError()).right(8192);
}

void GameLibrary::onWindowsInstallErrorOccurred(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        m_windowsInstallStatus = "Windows installer helper could not start.";
        m_portablePhase.clear();
        emit windowsInstallChanged();
    }
}

void GameLibrary::onWindowsInstallFinished(int code, QProcess::ExitStatus status) {
    m_windowsInstallDeadline.stop();
    if (!m_ready || m_portablePhase != "windows-install")
        return;
    if (code != 0 || status != QProcess::NormalExit) {
        const auto reason = QString::fromUtf8(m_portableInstallError).right(1000);
        m_windowsInstallStatus = "Windows installer stopped or failed. No files were installed. " + reason;
        m_portablePhase.clear();
        emit windowsInstallChanged();
        return;
    }
    m_portableInstallOutput += m_windowsInstall.readAllStandardOutput();
    QJsonParseError parse;
    const auto metadata = QJsonDocument::fromJson(m_portableInstallOutput, &parse).toVariant().toMap();
    if (parse.error != QJsonParseError::NoError || metadata.value("schemaVersion").toInt() != 1 ||
        metadata.value("title").toString().isEmpty() || metadata.value("platform").toString().isEmpty() ||
        metadata.value("entrypoint").toString().isEmpty() ||
        !QRegularExpression("^[0-9a-f]{64}$").match(metadata.value("archiveSha256").toString()).hasMatch()) {
        m_windowsInstallStatus = "Invalid Windows install adapter response. Nothing was added to Games.";
        m_portablePhase.clear();
        emit windowsInstallChanged();
        return;
    }
    const auto root = metadata.value("root").toString();
    const auto id = addGameWithOrigin({{"schemaVersion", 1},
                                       {"title", metadata.value("title")},
                                       {"platform", metadata.value("platform")},
                                       {"runtimeId", m_portableRuntime},
                                       {"root", root},
                                       {"target", metadata.value("entrypoint")}},
                                      "windows-install");
    m_windowsInstallStatus = id.isEmpty()
                                 ? "Package installed but catalog registration failed. Preserve the private "
                                   "recovery folder and retry from Games."
                                 : "Installed, verified and added to Games. Launch uses an isolated session.";
    m_windowsInstallPreview = metadata;
    m_portablePhase.clear();
    m_portableInstallPreview.clear();
    emit windowsInstallChanged();
}

void GameLibrary::onOpticalRefreshTimeout() {
    if (!m_ready)
        return;
    const auto discs = OpticalDiscHash::mountedDiscs();
    QStringList ids;
    for (const auto& item : discs)
        ids.append(item.toMap().value("observationId").toString());
    ids.sort();
    if (ids == m_lastOpticalObservationIds)
        return;
    bool inserted = false, removed = false;
    for (const auto& id : ids)
        if (!m_lastOpticalObservationIds.contains(id))
            inserted = true;
    for (const auto& id : m_lastOpticalObservationIds)
        if (!ids.contains(id))
            removed = true;
    m_lastOpticalObservationIds = ids;
    const auto change = inserted && removed ? QStringLiteral("changed")
                                            : (inserted ? QStringLiteral("inserted") : QStringLiteral("removed"));
    emit opticalDiscsChanged(discs, change);
}

bool GameLibrary::validateCatalogPath() {
    if (!QDir::isAbsolutePath(path) || QFileInfo(path).isSymLink()) {
        fail(tr("Choose a private games catalog path."));
        return false;
    }
    return true;
}

bool GameLibrary::openCatalogDatabase() {
    const bool exists = QFileInfo::exists(path);
    if (!exists && !QDir().mkpath(QFileInfo(path).absolutePath())) {
        fail(tr("Cannot create the games catalog folder."));
        return false;
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(path);
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
    if (!m_db.open()) {
        fail(tr("Cannot open the games catalog: %1").arg(m_db.lastError().text()));
        return false;
    }
    return true;
}

bool GameLibrary::validateCatalogVersion(QSqlQuery& query) {
    if (!query.exec("PRAGMA user_version") || !query.next()) {
        fail(tr("Cannot read the games catalog version."));
        return false;
    }
    const int version = query.value(0).toInt();
    query.finish();
    if (m_catalogExisted && version != 1 && version != 2 && version != 3 && version != 4 && version != 5 &&
        version != 6 && version != 7) {
        fail(tr("Unsupported games catalog version. Original data was preserved."));
        return false;
    }
    return true;
}

bool GameLibrary::configureCatalogDurability(QSqlQuery& query) {
    if (!query.exec("PRAGMA foreign_keys=ON") || !query.exec("PRAGMA synchronous=FULL")) {
        fail(query.lastError().text());
        return false;
    }
    return true;
}

bool GameLibrary::initializeCatalogSchema(QSqlQuery& query) {
    if (!m_catalogExisted)
        return true;
    if (!m_db.transaction()) {
        fail(m_db.lastError().text());
        return false;
    }
    const bool created =
        query.exec("CREATE TABLE games (id TEXT PRIMARY KEY, title TEXT NOT NULL, platform TEXT NOT NULL, runtime "
                   "TEXT NOT NULL, root TEXT NOT NULL, target TEXT NOT NULL, favorite INTEGER NOT NULL DEFAULT 0 "
                   "CHECK(favorite IN (0,1)), added INTEGER NOT NULL, UNIQUE(runtime,root,target))") &&
        query.exec("CREATE INDEX games_title ON games(title COLLATE NOCASE)") &&
        query.exec("PRAGMA user_version=1");
    if (!created || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        fail(tr("Cannot initialize the games catalog: %1").arg(error));
        return false;
    }
    return true;
}

bool GameLibrary::validateCatalogVersion(QSqlQuery& query) {
    if (!query.exec("PRAGMA user_version") || !query.next()) {
        fail(tr("Cannot read the games catalog version."));
        return false;
    }
    m_catalogVersion = query.value(0).toInt();
    query.finish();
    if (m_catalogExisted && m_catalogVersion != 1 && m_catalogVersion != 2 && m_catalogVersion != 3 &&
        m_catalogVersion != 4 && m_catalogVersion != 5 && m_catalogVersion != 6 && m_catalogVersion != 7) {
        fail(tr("Unsupported games catalog version. Original data was preserved."));
        return false;
    }
    return true;
}

bool GameLibrary::validateCatalogSchemas(QSqlQuery& query) {
    if (!query.exec("SELECT id,title,platform,runtime,root,target,favorite,added FROM games LIMIT 0")) {
        fail(tr("Invalid games catalog. Original data was preserved."));
        return false;
    }
    if (!m_catalogExisted)
        return true;
    if (m_catalogVersion == 2 || m_catalogVersion >= 3) {
        if (!validateEditionsSchema(query))
            return false;
    }
    if (m_catalogVersion >= 3) {
        if (!validateActivitySchema(query))
            return false;
    }
    if (m_catalogVersion >= 4) {
        if (!validateOrganizationSchema(query))
            return false;
    }
    if (m_catalogVersion >= 5) {
        if (!validateRichMetadataSchema(query))
            return false;
    }
    if (m_catalogVersion >= 6) {
        if (!validateArtworkSchema(query))
            return false;
    }
    return true;
}

bool GameLibrary::validateEditionsSchema(QSqlQuery& query) {
    if (!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions LIMIT 0")) {
        fail(tr("Invalid edition catalog. Original data was preserved."));
        return false;
    }
    m_hasEditions = true;
    return true;
}

bool GameLibrary::validateActivitySchema(QSqlQuery& query) {
    if (!query.exec("SELECT game_id,last_played FROM game_activity LIMIT 0")) {
        fail(tr("Invalid game activity catalog. Original data was preserved."));
        return false;
    }
    m_hasActivity = true;
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='game_editions'")) {
        fail(tr("Cannot inspect the editions catalog."));
        return false;
    }
    if (query.next()) {
        if (!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions LIMIT 0")) {
            fail(tr("Invalid edition catalog. Original data was preserved."));
            return false;
        }
        m_hasEditions = true;
    }
    return true;
}

bool GameLibrary::validateOrganizationSchema(QSqlQuery& query) {
    if (!query.exec("SELECT game_id,franchise,tags FROM game_metadata LIMIT 0") ||
        !query.exec("SELECT id,name,kind,filter_type,filter_value FROM game_collections LIMIT 0") ||
        !query.exec("SELECT collection_id,game_id,position FROM game_collection_members LIMIT 0")) {
        fail(tr("Invalid Games organization catalog. Original data was preserved."));
        return false;
    }
    m_hasMetadata = true;
    m_hasCollections = true;
    return true;
}

bool GameLibrary::validateRichMetadataSchema(QSqlQuery& query) {
    if (!query.exec("SELECT aliases,summary FROM game_metadata LIMIT 0")) {
        fail(tr("Invalid rich game metadata catalog. Original data was preserved."));
        return false;
    }
    m_hasRichMetadata = true;
    return true;
}

bool GameLibrary::validateArtworkSchema(QSqlQuery& query) {
    if (!query.exec("SELECT artwork_hash FROM game_metadata LIMIT 0")) {
        fail(tr("Invalid managed artwork catalog. Original data was preserved."));
        return false;
    }
    m_hasArtwork = true;
    return true;
}

bool GameLibrary::validateCatalogProvenance(QSqlQuery& query) {
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='game_provenance'")) {
        fail(tr("Cannot inspect catalog provenance."));
        return false;
    }
    if (query.next()) {
        if (!query.exec(
                "SELECT entity_id,entity_type,source_kind,origin,source_reference,source_fingerprint,recorded_at FROM "
                "game_provenance LIMIT 0") ||
            !query.exec("SELECT name FROM sqlite_master WHERE type='trigger' AND name IN "
                        "('game_provenance_no_update','game_provenance_no_delete')")) {
            fail(tr("Invalid immutable catalog provenance. Original data was preserved."));
            return false;
        }
        int triggers = 0;
        while (query.next())
            ++triggers;
        if (triggers != 2) {
            fail(tr("Catalog provenance immutability guards are missing. Original data was preserved."));
            return false;
        }
        m_hasProvenance = true;
    }
    return true;
}

bool GameLibrary::checkPendingRecoveries(QSqlQuery& query) {
    const auto managedPrefix = ManagedGamePaths::gamesRoot(m_emulatorProfile) + '/';
    const auto recoveryQuery =
        QString("SELECT root FROM games") + QString(m_hasEditions ? " UNION SELECT root FROM game_editions" : "");
    if (!query.exec(recoveryQuery)) {
        fail(tr("Cannot inspect managed uninstall recovery. The catalog was preserved."));
        return false;
    }
    while (query.next()) {
        const auto root = query.value(0).toString();
        if (!root.startsWith(managedPrefix))
            continue;
        const auto recovery = ManagedGamePaths::recoverPendingUninstall(m_emulatorProfile, root);
        if (!recovery.isEmpty()) {
            fail(recovery);
            return false;
        }
    }
    return true;
}

GameLibrary::~GameLibrary() {
    cancelImport();
    if (m_steamScan.state() != QProcess::NotRunning)
        m_steamScan.waitForFinished(1000);
    cancelPortableInstall();
    cancelEmulatorSetup();
    if (setupBusy())
        m_setup.waitForFinished(5000);
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}

int GameLibrary::catalogLimit() const {
    return MaxGames;
}

bool GameLibrary::fail(const QString& error) {
    m_error = error;
    emit errorChanged();
    return false;
}

void GameLibrary::invalidate() {
    cancelImport();
    cancelEmulatorSetup();
    cancelPortableInstall();
    m_ready = false;
    m_db.close();
    fail(tr("This games profile is no longer active."));
    emit changed();
}

bool GameLibrary::compatibleRuntime(const QString& id, bool file) const {
    const auto* app = m_apps->find(id);
    if (!app || app->type != "desktop")
        return false;
    if (id == "steam" && !file)
        return app->presentationMode == "game-hub" && app->launchConfiguration.value("desktopId") == "isolde-steam";
    if (file)
        return QStringList{"game", "game-hub"}.contains(app->presentationMode) &&
               app->handledActions.contains("open-file") &&
               app->launchConfiguration.value("actionHandlers").toMap().contains("open-file");
    return app->presentationMode == "game";
}

QVariantList GameLibrary::runtimes() const {
    QVariantList result;
    for (const auto& entry : m_apps->apps()) {
        const auto app = entry.toMap();
        const auto id = app.value("id").toString();
        if (compatibleRuntime(id, true) || compatibleRuntime(id, false))
            result.append(QVariantMap{{"id", id},
                                      {"name", app.value("name")},
                                      {"file", compatibleRuntime(id, true)},
                                      {"provider", id == "steam" ? "steam" : "local"}});
    }
    return result;
}

QVariantMap GameLibrary::compatibilityInstallerStatus() const {
    return CompatibilityRuntime::discover();
}

QVariantList GameLibrary::emulators() const {
    QVariantList result;
    if (!m_ready)
        return result;
    for (const auto& item : GameEmulators::definitions()) {
        auto value = item.toMap();
        const auto binary = value.value("binary").toString();
        const auto core = value.value("core").toString();
        value["installed"] = QFileInfo(binary).isExecutable() && (core.isEmpty() || QFileInfo(core).isFile());
        value["registered"] = compatibleRuntime(value.value("id").toString(), true);
        result.append(value);
    }
    return result;
}

QVariantMap GameLibrary::emulatorCoverage() const {
    return GameEmulators::coverage();
}

QVariantMap GameLibrary::emulatorSettings(const QString& id) {
    if (!profileReady())
        return {};
    QString error;
    const auto value = GameEmulators::settings(m_emulatorProfile, id, &error);
    if (value.isEmpty())
        fail(error);
    return value;
}

bool GameLibrary::profileReady() const {
    return m_ready;
}

bool GameLibrary::configureEmulator(const QString& id, const QVariantMap& options, const QString& firmware) {
    if (!m_ready)
        return fail(tr("This games profile is no longer active."));
    if (!firmware.isEmpty() && !approvedMediaDirectory(m_emulatorProfile, QFileInfo(firmware).absolutePath()))
        return fail(tr("Approve the firmware's media folder for this profile before importing."));
    QString error;
    if (!GameEmulators::configure(m_emulatorProfile, id, options, firmware, &error))
        return fail(error);
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

QString GameLibrary::addGame(const QVariantMap& input) {
    return addGameWithOrigin(input, "manual-registration");
}

QVariantList GameLibrary::page(const QString& view, const QString& search, int offset, const QString& platform) {
    if (!m_ready)
        return {};
    if (!validatePageQuery(view, search, offset, platform))
        return {};
    if (view == "continue" && !m_hasActivity)
        return {};
    if ((view == "franchise" || view == "tag") && !m_hasMetadata)
        return {};
    QSqlQuery query(m_db);
    query.prepare(buildSelectClause() + " " + buildWhereClause(view, search, platform) + " " +
                  buildOrderByClause(view) + " LIMIT 50 OFFSET ?");
    bindQueryParameters(query, view, search, platform, offset);
    if (!query.exec()) {
        fail(query.lastError().text());
        return {};
    }
    QVariantList result;
    while (query.next())
        result.append(row(query, artworkDirectory()));
    return result;
}

bool GameLibrary::validatePageQuery(const QString& view, const QString& search, int offset,
                                    const QString& platform) {
    if (!validPageView(view) || !validPageSearch(search) || !validPageOffset(offset) || !validPagePlatform(platform) ||
        !validPlatformView(view, platform) || !validFranchiseTagView(view, search))
        return fail(tr("Invalid games query."));
    return true;
}

bool GameLibrary::validPageView(const QString& view) const {
    return QStringList{"all", "recent", "favorites", "search", "platform", "continue", "franchise", "tag"}.contains(
        view);
}

bool GameLibrary::validPageSearch(const QString& search) const {
    return search.size() <= 160 && !search.contains(QChar::Null);
}

bool GameLibrary::validPageOffset(int offset) const {
    return offset >= 0 && offset < MaxGames && offset % PageSize == 0;
}

bool GameLibrary::validPagePlatform(const QString& platform) const {
    return platform.size() <= 80 && !platform.contains(QChar::Null);
}

bool GameLibrary::validPlatformView(const QString& view, const QString& platform) const {
    return !(view == "platform" && (platform.trimmed().isEmpty() || platform != platform.trimmed()));
}

bool GameLibrary::validFranchiseTagView(const QString& view, const QString& search) const {
    return !((view == "franchise" || view == "tag") &&
             (search.trimmed().isEmpty() || search != search.trimmed()));
}

QString GameLibrary::buildSelectClause() const {
    QString sql =
        "SELECT g.id,g.title,g.platform,g.runtime,g.favorite," +
        (m_hasMetadata ? QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')") : QString("'', '[]'")) +
        (m_hasRichMetadata ? QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')") : QString(",'[]',''")) +
        (m_hasArtwork ? QString(",COALESCE(m.artwork_hash,'')") : QString(",''")) + " FROM games g";
    if (m_hasActivity)
        sql += " LEFT JOIN game_activity a ON a.game_id=g.id";
    if (m_hasMetadata)
        sql += " LEFT JOIN game_metadata m ON m.game_id=g.id";
    return sql;
}

QString GameLibrary::buildWhereClause(const QString& view, const QString& search, const QString& platform) const {
    QString sql =
        "WHERE (?=0 OR g.favorite=1) AND (?='' OR lower(g.platform)=lower(?)";
    if (m_hasEditions)
        sql += " OR EXISTS(SELECT 1 FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(?))";
    sql += ")";
    if (view == "franchise")
        sql += " AND instr(lower(COALESCE(m.franchise,'')),lower(?))>0";
    else if (view == "tag")
        sql += " AND EXISTS(SELECT 1 FROM json_each(COALESCE(m.tags,'[]')) tag WHERE lower(tag.value)=lower(?))";
    else {
        sql += " AND (instr(lower(g.title),lower(?))>0";
        if (m_hasMetadata)
            sql += " OR instr(lower(m.franchise),lower(?))>0 OR instr(lower(m.tags),lower(?))>0";
        if (m_hasRichMetadata)
            sql += " OR instr(lower(m.aliases),lower(?))>0 OR instr(lower(m.summary),lower(?))>0";
        sql += ")";
    }
    if (view == "continue")
        sql += " AND a.game_id IS NOT NULL";
    return sql;
}

QString GameLibrary::buildOrderByClause(const QString& view) const {
    return "ORDER BY " +
           QString(view == "recent"     ? "g.added DESC,g.id"
                   : view == "continue" ? "a.last_played DESC,g.id"
                                        : "g.title COLLATE NOCASE,g.id");
}

void GameLibrary::bindQueryParameters(QSqlQuery& query, const QString& view, const QString& search,
                                       const QString& platform, int offset) const {
    query.addBindValue(view == "favorites" ? 1 : 0);
    query.addBindValue(view == "platform" ? platform : QStringLiteral(""));
    query.addBindValue(view == "platform" ? platform : QStringLiteral(""));
    if (m_hasEditions)
        query.addBindValue(view == "platform" ? platform : QStringLiteral(""));
    query.addBindValue(search.isNull() ? QStringLiteral("") : search);
    if (view != "franchise" && view != "tag") {
        if (m_hasMetadata) {
            query.addBindValue(search.isNull() ? QStringLiteral("") : search);
            query.addBindValue(search.isNull() ? QStringLiteral("") : search);
        }
        if (m_hasRichMetadata) {
            query.addBindValue(search.isNull() ? QStringLiteral("") : search);
            query.addBindValue(search.isNull() ? QStringLiteral("") : search);
        }
    }
    query.addBindValue(offset);
}

bool GameLibrary::ensureOrganizationSchema() {
    if (m_hasMetadata && m_hasCollections)
        return true;
    if (!m_db.transaction())
        return fail(tr("Could not begin the Games organization update."));
    QSqlQuery query(m_db);
    const bool created =
        (!m_hasActivity ? query.exec("CREATE TABLE game_activity(game_id TEXT PRIMARY KEY REFERENCES games(id) ON "
                                     "DELETE CASCADE,last_played INTEGER NOT NULL)") &&
                                  query.exec("CREATE INDEX game_activity_recent ON game_activity(last_played DESC)")
                        : true) &&
        query.exec("CREATE TABLE game_metadata(game_id TEXT PRIMARY KEY REFERENCES games(id) ON DELETE "
                   "CASCADE,franchise TEXT NOT NULL DEFAULT '',tags TEXT NOT NULL DEFAULT '[]')") &&
        query.exec("CREATE TABLE game_collections(id TEXT PRIMARY KEY,name TEXT NOT NULL COLLATE NOCASE UNIQUE,kind "
                   "TEXT NOT NULL CHECK(kind IN ('manual','smart')),filter_type TEXT NOT NULL DEFAULT '',filter_value "
                   "TEXT NOT NULL DEFAULT '')") &&
        query.exec("CREATE TABLE game_collection_members(collection_id TEXT NOT NULL REFERENCES game_collections(id) "
                   "ON DELETE CASCADE,game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,position INTEGER "
                   "NOT NULL,PRIMARY KEY(collection_id,game_id))") &&
        query.exec(
            "CREATE INDEX game_collection_members_order ON game_collection_members(collection_id,position,game_id)") &&
        query.exec("PRAGMA user_version=4");
    if (!created || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(
            tr("Games organization could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasActivity = true;
    m_hasMetadata = true;
    m_hasCollections = true;
    return true;
}

bool GameLibrary::ensureRichMetadataSchema() {
    if (m_hasRichMetadata)
        return true;
    if (!ensureOrganizationSchema())
        return false;
    if (!m_db.transaction())
        return fail(tr("Could not begin the rich game metadata update."));
    QSqlQuery query(m_db);
    const bool migrated = query.exec("ALTER TABLE game_metadata ADD COLUMN aliases TEXT NOT NULL DEFAULT '[]'") &&
                          query.exec("ALTER TABLE game_metadata ADD COLUMN summary TEXT NOT NULL DEFAULT ''") &&
                          query.exec("PRAGMA user_version=5");
    if (!migrated || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(
            tr("Rich game metadata could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasRichMetadata = true;
    return true;
}

QString GameLibrary::artworkDirectory() const {
    return m_db.databaseName() + ".artwork";
}

bool GameLibrary::ensureArtworkSchema() {
    if (m_hasArtwork)
        return true;
    if (!ensureRichMetadataSchema())
        return false;
    if (!m_db.transaction())
        return fail(tr("Could not begin the managed artwork update."));
    QSqlQuery query(m_db);
    const bool migrated = query.exec("ALTER TABLE game_metadata ADD COLUMN artwork_hash TEXT NOT NULL DEFAULT ''") &&
                          query.exec("PRAGMA user_version=6");
    if (!migrated || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Managed artwork could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasArtwork = true;
    return true;
}

bool GameLibrary::ensureProvenanceSchema() {
    if (m_hasProvenance)
        return true;
    if (!m_db.transaction())
        return fail(tr("Could not begin the immutable provenance update."));
    QSqlQuery query(m_db);
    const bool created =
        query.exec("CREATE TABLE game_provenance("
                   "entity_id TEXT PRIMARY KEY,entity_type TEXT NOT NULL CHECK(entity_type IN ('game','edition')),"
                   "source_kind TEXT NOT NULL CHECK(source_kind IN "
                   "('steam-app','optical-media','isolde-managed-install','external-folder','runtime-entry')),"
                   "origin TEXT NOT NULL CHECK(origin IN "
                   "('steam-import','folder-import','optical-registration','portable-install','manual-registration')),"
                   "source_reference TEXT NOT NULL CHECK(length(source_reference)<=4096),"
                   "source_fingerprint TEXT NOT NULL CHECK(length(source_fingerprint)=64),recorded_at INTEGER NOT NULL,"
                   "CHECK(origin='manual-registration' OR "
                   "(source_kind='steam-app' AND origin='steam-import') OR "
                   "(source_kind IN ('external-folder','isolde-managed-install') AND origin='folder-import') OR "
                   "(source_kind='optical-media' AND origin='optical-registration') OR "
                   "(source_kind='isolde-managed-install' AND origin='portable-install')))") &&
        query.exec("CREATE INDEX game_provenance_recorded ON game_provenance(recorded_at,entity_id)") &&
        query.exec("CREATE TRIGGER game_provenance_no_update BEFORE UPDATE ON game_provenance BEGIN SELECT "
                   "RAISE(ABORT,'game provenance is immutable'); END") &&
        query.exec("CREATE TRIGGER game_provenance_no_delete BEFORE DELETE ON game_provenance BEGIN SELECT "
                   "RAISE(ABORT,'game provenance is immutable'); END");
    if (!created || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(
            tr("Immutable provenance could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasProvenance = true;
    return true;
}

bool GameLibrary::recordProvenance(const QString& entityId,
                                   const QString& entityType,
                                   const QString& runtime,
                                   const QString& root,
                                   const QString& target,
                                   const QString& origin) {
    if (!validProvenanceIdentity(entityId, entityType))
        return fail(tr("Invalid acquisition provenance identity."));
    if (!withinProvenanceBound())
        return fail(tr("The catalog reached its immutable provenance record bound."));
    const auto value = sourceProvenance(runtime, root, target, m_emulatorProfile, origin);
    const auto kind = value.value("sourceKind").toString(), recordedOrigin = value.value("origin").toString();
    const auto expectedOrigin = provenanceExpectedOrigin(kind);
    if (!validProvenanceOrigin(recordedOrigin, kind, expectedOrigin))
        return fail(tr("Acquisition provenance source and origin do not match."));
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO "
                  "game_provenance(entity_id,entity_type,source_kind,origin,source_reference,source_fingerprint,"
                  "recorded_at) VALUES(?,?,?,?,?,?,unixepoch())");
    query.addBindValue(entityId);
    query.addBindValue(entityType);
    query.addBindValue(value.value("sourceKind"));
    query.addBindValue(value.value("origin"));
    query.addBindValue(value.value("sourceReference"));
    query.addBindValue(value.value("sourceFingerprint"));
    if (!query.exec())
        return fail(tr("Acquisition provenance could not be recorded. Catalog transaction was rolled back. %1")
                        .arg(query.lastError().text()));
    return true;
}

bool GameLibrary::validProvenanceIdentity(const QString& entityId, const QString& entityType) const {
    return m_hasProvenance && ComponentSchema::safeId(entityId) &&
           QStringList{"game", "edition"}.contains(entityType);
}

bool GameLibrary::withinProvenanceBound() const {
    QSqlQuery count(m_db);
    if (!count.exec("SELECT count(*) FROM game_provenance") || !count.next())
        return false;
    return count.value(0).toInt() < MaxProvenanceRecords;
}

QString GameLibrary::provenanceExpectedOrigin(const QString& kind) const {
    if (kind == "steam-app")
        return QString("steam-import");
    if (kind == "optical-media")
        return QString("optical-registration");
    if (kind == "isolde-managed-install")
        return QString("portable-install");
    if (kind == "external-folder")
        return QString("folder-import");
    return QString("manual-registration");
}

bool GameLibrary::validProvenanceOrigin(const QString& recordedOrigin, const QString& kind,
                                        const QString& expectedOrigin) const {
    return recordedOrigin == "manual-registration" || recordedOrigin == expectedOrigin ||
           (recordedOrigin == "folder-import" && kind == "isolde-managed-install");
}


bool GameLibrary::setArtwork(const QString& id, const QString& source) {
    if (!m_ready || details(id).isEmpty())
        return fail(tr("Game not found in this profile."));
    QFileInfo inputFile(source);
    if (!validArtworkSource(inputFile))
        return fail(tr("Choose a readable PNG or JPEG artwork file in an approved media folder."));
    const auto parent = inputFile.absolutePath();
    const auto grantError = MediaGrants::validate(parent);
    if (!grantError.isEmpty())
        return fail(grantError);
    if (!approvedMediaDirectory(m_emulatorProfile, parent))
        return fail(tr("Approve this media folder for the active profile before importing artwork."));
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly))
        return fail(tr("Cannot read the selected artwork."));
    const auto sourceBytes = input.read(MaxArtworkSourceBytes + 1);
    if (sourceBytes.size() > MaxArtworkSourceBytes)
        return fail(tr("Artwork exceeds the 16 MiB source limit."));
    const auto bytes = normalizeArtwork(sourceBytes);
    if (bytes.isEmpty())
        return fail(
            tr("Artwork must be a readable PNG or JPEG no larger than 4096 by 4096 pixels and 12 MiB when managed."));
    if (!ensureArtworkSchema())
        return false;
    QString digest, error;
    if (!persistArtwork(m_db.databaseName(), bytes, &digest, &error))
        return fail(tr("%1 Nothing was changed in the game catalog.").arg(error));
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO game_metadata(game_id,artwork_hash) VALUES(?,?) ON CONFLICT(game_id) DO UPDATE SET "
                  "artwork_hash=excluded.artwork_hash");
    query.addBindValue(id);
    query.addBindValue(digest);
    if (!query.exec() || !m_db.commit()) {
        const auto reason = query.lastError().text();
        m_db.rollback();
        return fail(tr("Artwork could not be assigned. Previous artwork was preserved. %1").arg(reason));
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool GameLibrary::validArtworkSource(const QFileInfo& inputFile) const {
    return QDir::isAbsolutePath(inputFile.absoluteFilePath()) &&
           QDir::cleanPath(inputFile.absoluteFilePath()) == inputFile.absoluteFilePath() && !inputFile.isSymLink() &&
           inputFile.isFile() && inputFile.canonicalFilePath() == inputFile.absoluteFilePath() &&
           inputFile.size() >= 1 && inputFile.size() <= MaxArtworkSourceBytes;
}


bool GameLibrary::clearArtwork(const QString& id) {
    if (!m_ready || details(id).isEmpty())
        return fail(tr("Game not found in this profile."));
    if (!m_hasArtwork) {
        m_error.clear();
        emit errorChanged();
        return true;
    }
    QSqlQuery oldQuery(m_db);
    oldQuery.prepare("SELECT artwork_hash FROM game_metadata WHERE game_id=?");
    oldQuery.addBindValue(id);
    if (!oldQuery.exec())
        return fail(oldQuery.lastError().text());
    const bool hasArtwork = oldQuery.next() && !oldQuery.value(0).toString().isEmpty();
    oldQuery.finish();
    if (!hasArtwork) {
        m_error.clear();
        emit errorChanged();
        return true;
    }
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    query.prepare("UPDATE game_metadata SET artwork_hash='' WHERE game_id=?");
    query.addBindValue(id);
    if (!query.exec() || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Artwork reference was not removed. %1").arg(error));
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

QVariantList GameLibrary::platforms() {
    QVariantList result;
    if (!m_ready)
        return result;
    QSqlQuery query(m_db);
    const auto sql = "SELECT platform FROM (SELECT platform FROM games" +
                     QString(m_hasEditions ? " UNION ALL SELECT platform FROM game_editions" : "") +
                     ") GROUP BY platform COLLATE NOCASE ORDER BY platform COLLATE NOCASE LIMIT 129";
    if (!query.exec(sql)) {
        fail(query.lastError().text());
        return {};
    }
    while (query.next())
        result.append(query.value(0).toString());
    if (result.size() > MaxPlatforms) {
        fail(tr("The Games catalog has more platform categories than supported."));
        return {};
    }
    return result;
}

bool GameLibrary::recordCompletedSession(const QString& id) {
    if (!m_ready || !ComponentSchema::safeId(id) || details(id).isEmpty())
        return fail(tr("Game not found in this profile."));
    if (m_hasMetadata && m_hasCollections) {
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO game_activity(game_id,last_played) VALUES(?,unixepoch()) ON CONFLICT(game_id) DO "
                      "UPDATE SET last_played=excluded.last_played");
        query.addBindValue(id);
        if (!query.exec())
            return fail(tr("Could not record the completed game session. %1").arg(query.lastError().text()));
        m_error.clear();
        emit errorChanged();
        emit changed();
        return true;
    }
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    if (!m_hasActivity && (!query.exec("CREATE TABLE game_activity(game_id TEXT PRIMARY KEY REFERENCES games(id) ON "
                                       "DELETE CASCADE,last_played INTEGER NOT NULL)") ||
                           !query.exec("CREATE INDEX game_activity_recent ON game_activity(last_played DESC)"))) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Could not initialize game activity. Previous catalog data was preserved. %1").arg(error));
    }
    query.prepare("INSERT INTO game_activity(game_id,last_played) VALUES(?,unixepoch()) ON CONFLICT(game_id) DO UPDATE "
                  "SET last_played=excluded.last_played");
    query.addBindValue(id);
    if (!query.exec() || (!m_hasActivity && !query.exec("PRAGMA user_version=3")) || !m_db.commit()) {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Could not record the completed game session. Previous activity was preserved. %1").arg(error));
    }
    m_hasActivity = true;
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

QVariantMap GameLibrary::registration(const QString& id) {
    if (!m_ready || !ComponentSchema::safeId(id))
        return {};
    QSqlQuery query(m_db);
    query.prepare("SELECT title,platform,runtime,root,target FROM games WHERE id=?");
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return {};
    return {{"schemaVersion", 1},
            {"title", query.value(0)},
            {"platform", query.value(1)},
            {"runtimeId", query.value(2)},
            {"root", query.value(3)},
            {"target", query.value(4)}};
}

bool GameLibrary::updateGame(const QString& id, const QVariantMap& input) {
    if (!validUpdateSource(id))
        return fail(tr("Game not found in this profile."));
    QString error;
    const auto value = ComponentSchema::gameRegistration(input, &error);
    if (value.isEmpty())
        return fail(error);
    const auto runtime = value.value("runtimeId").toString(), root = value.value("root").toString(),
               target = value.value("target").toString();
    if (!validUpdateRegistration(value, runtime, root, target))
        return false;
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    query.prepare("UPDATE games SET title=?,platform=?,runtime=?,root=?,target=? WHERE id=?");
    for (const auto& key : QStringList{"title", "platform", "runtimeId", "root", "target"})
        query.addBindValue(value.value(key));
    query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1 || !m_db.commit()) {
        const auto reason = query.lastError().text();
        m_db.rollback();
        return fail(tr("Could not update this game. Previous metadata and source were preserved. %1").arg(reason));
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool GameLibrary::validUpdateSource(const QString& id) const {
    return !m_portablePhase.startsWith("uninstall") && m_ready && !registration(id).isEmpty();
}

bool GameLibrary::validUpdateRegistration(const QVariantMap& value, const QString& runtime, const QString& root,
                                          const QString& target) const {
    if (!platformAvailable(value.value("platform").toString()))
        return fail(tr("The Games catalog supports up to 128 distinct platforms. Previous metadata was preserved."));
    if (!compatibleRuntime(runtime, !root.isEmpty()))
        return fail(tr("Choose an installed compatible game runtime."));
    if (!root.isEmpty() && checkedTarget(root, target).isEmpty())
        return false;
    if (m_hasEditions && !editionTargetUnused(runtime, root, target))
        return fail(tr("This installation already belongs to an edition. Previous metadata was preserved."));
    return true;
}

bool GameLibrary::editionTargetUnused(const QString& runtime, const QString& root, const QString& target) const {
    QSqlQuery duplicate(m_db);
    duplicate.prepare("SELECT id FROM game_editions WHERE runtime=? AND root=? AND target=?");
    duplicate.addBindValue(runtime);
    duplicate.addBindValue(root);
    duplicate.addBindValue(target);
    return duplicate.exec() && !duplicate.next();
}


QVariantMap GameLibrary::details(const QString& id) {
    if (!m_ready || !ComponentSchema::safeId(id))
        return {};
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT g.id,g.title,g.platform,g.runtime,g.favorite," +
        (m_hasMetadata ? QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')") : QString("'', '[]'")) +
        (m_hasRichMetadata ? QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')") : QString(",'[]',''")) +
        (m_hasArtwork ? QString(",COALESCE(m.artwork_hash,'')") : QString(",''")) + ",g.root,g.target" +
        " FROM games g" + QString(m_hasMetadata ? " LEFT JOIN game_metadata m ON m.game_id=g.id" : "") +
        " WHERE g.id=?");
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return {};
    auto result = row(query, artworkDirectory());
    result["sourceKind"] = sourceKind(query.value(3).toString(), query.value(10).toString(), m_emulatorProfile);
    result["sourceReference"] = OpticalDiscIdentify::isRootReference(query.value(10).toString())
                                    ? OpticalDiscIdentify::mediaIdFromRoot(query.value(10).toString())
                                    : query.value(11).toString();
    result["acquisitionProvenance"] = QVariantMap{{"recorded", false},
                                                  {"sourceKind", "not-recorded"},
                                                  {"origin", "unknown"},
                                                  {"sourceReference", QString()},
                                                  {"sourceFingerprint", QString()},
                                                  {"recordedAt", QVariant::fromValue<qlonglong>(0)}};
    if (m_hasProvenance) {
        QSqlQuery provenance(m_db);
        provenance.prepare("SELECT entity_type,source_kind,origin,source_reference,source_fingerprint,recorded_at FROM "
                           "game_provenance WHERE entity_id=?");
        provenance.addBindValue(id);
        if (provenance.exec() && provenance.next()) {
            result["acquisitionProvenance"] = QVariantMap{{"recorded", true},
                                                          {"entityType", provenance.value(0)},
                                                          {"sourceKind", provenance.value(1)},
                                                          {"origin", provenance.value(2)},
                                                          {"sourceReference", provenance.value(3)},
                                                          {"sourceFingerprint", provenance.value(4)},
                                                          {"recordedAt", provenance.value(5).toLongLong()}};
        }
    }
    return result;
}

bool GameLibrary::setOrganization(const QString& id,
                                  const QString& franchise,
                                  const QStringList& inputTags,
                                  const QStringList& inputAliases,
                                  const QString& summary) {
    if (!m_ready || details(id).isEmpty())
        return fail(tr("Game not found in this profile."));
    if (!validOrganizationInput(franchise, inputTags, inputAliases, summary))
        return fail(tr("Use a franchise name up to 160 characters, at most 32 tags and aliases, and a summary up to "
                       "4096 characters."));
    QJsonArray tags;
    if (!normalizeTags(inputTags, tags))
        return fail(tr("Tags must be distinct, trimmed names of up to 64 characters."));
    QJsonArray aliases;
    if (!normalizeAliases(inputAliases, aliases))
        return fail(tr("Aliases must be distinct, trimmed names of up to 160 characters."));
    if (!ensureRichMetadataSchema())
        return false;
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO game_metadata(game_id,franchise,tags,aliases,summary) VALUES(?,?,?,?,?) ON CONFLICT(game_id) DO "
        "UPDATE SET franchise=excluded.franchise,tags=excluded.tags,aliases=excluded.aliases,summary=excluded.summary");
    query.addBindValue(id);
    query.addBindValue(franchise.isNull() ? QStringLiteral("") : franchise);
    query.addBindValue(QJsonDocument(tags).toJson(QJsonDocument::Compact));
    query.addBindValue(QJsonDocument(aliases).toJson(QJsonDocument::Compact));
    query.addBindValue(summary.isNull() ? QStringLiteral("") : summary);
    if (!query.exec())
        return fail(
            tr("Could not save game organization. Previous metadata was preserved. %1").arg(query.lastError().text()));
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool GameLibrary::validOrganizationInput(const QString& franchise, const QStringList& inputTags,
                                         const QStringList& inputAliases, const QString& summary) const {
    return franchise.size() <= 160 && !franchise.contains(QChar::Null) && franchise == franchise.trimmed() &&
           inputTags.size() <= MaxTags && inputAliases.size() <= MaxTags && summary.size() <= 4096 &&
           !summary.contains(QChar::Null);
}

bool GameLibrary::normalizeTags(const QStringList& inputTags, QJsonArray& tags) const {
    QSet<QString> seen;
    for (const auto& tag : inputTags) {
        const auto normalized = tag.trimmed();
        if (normalized.isEmpty() || normalized.size() > 64 || normalized.contains(QChar::Null) ||
            normalized.contains(',') || normalized != tag || seen.contains(normalized.toCaseFolded()))
            return false;
        seen.insert(normalized.toCaseFolded());
        tags.append(normalized);
    }
    return true;
}

bool GameLibrary::normalizeAliases(const QStringList& inputAliases, QJsonArray& aliases) const {
    QSet<QString> aliasSet;
    for (const auto& alias : inputAliases) {
        if (alias.isEmpty() || alias.size() > 160 || alias != alias.trimmed() || alias.contains(';') ||
            alias.contains(QChar::Null) || aliasSet.contains(alias.toCaseFolded()))
            return false;
        aliasSet.insert(alias.toCaseFolded());
        aliases.append(alias);
    }
    return true;
}


bool GameLibrary::setFavorite(const QString& id, bool favorite) {
    if (!m_ready || details(id).isEmpty())
        return fail(tr("Game not found in this profile."));
    QSqlQuery query(m_db);
    query.prepare("UPDATE games SET favorite=? WHERE id=?");
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(id);
    if (!query.exec())
        return fail(query.lastError().text());
    m_error.clear();
    emit changed();
    return true;
}

bool GameLibrary::platformAvailable(const QString& platform) {
    if (!m_ready || platform.trimmed().isEmpty() || platform.size() > 80)
        return false;
    QSqlQuery query(m_db);
    query.prepare("SELECT count(DISTINCT lower(platform)) FROM (SELECT platform FROM games" +
                  QString(m_hasEditions ? " UNION ALL SELECT platform FROM game_editions" : "") + ")");
    if (!query.exec() || !query.next())
        return false;
    const int count = query.value(0).toInt();
    query.finish();
    if (count < MaxPlatforms)
        return true;
    query.prepare(
        "SELECT EXISTS(SELECT 1 FROM games WHERE lower(platform)=lower(?))" +
        QString(m_hasEditions ? " OR EXISTS(SELECT 1 FROM game_editions WHERE lower(platform)=lower(?))" : ""));
    query.addBindValue(platform);
    if (m_hasEditions)
        query.addBindValue(platform);
    return query.exec() && query.next() && query.value(0).toBool();
}

bool GameLibrary::removeGame(const QString& id) {
    if (m_portablePhase == "uninstall" || m_portablePhase == "uninstall-finalize")
        return fail(tr("Wait for the managed uninstall to finish before changing the catalog."));
    if (!m_ready || details(id).isEmpty())
        return fail(tr("Game not found in this profile."));
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM games WHERE id=?");
    query.addBindValue(id);
    if (!query.exec())
        return fail(query.lastError().text());
    m_error.clear();
    emit changed();
    return true;
}

bool GameLibrary::canUninstallManagedGame(const QString& id) const {
    if (!m_ready || !ComponentSchema::safeId(id))
        return false;
    QSqlQuery query(m_db);
    query.prepare("SELECT root,target FROM games WHERE id=?");
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return false;
    const auto root = query.value(0).toString(), target = query.value(1).toString();
    if (!validUninstallTarget(root, target))
        return false;
    if (m_hasEditions && editionReferencesTarget(id, root))
        return false;
    return !otherGameUsesRoot(root, id);
}

bool GameLibrary::validUninstallTarget(const QString& root, const QString& target) const {
    return ManagedGamePaths::isInstallRoot(m_emulatorProfile, root) && !target.isEmpty() &&
           !QDir::isAbsolutePath(target) && QDir::cleanPath(target) == target &&
           !target.split('/').contains("..") && QFileInfo(root + '/' + target).isFile();
}

bool GameLibrary::editionReferencesTarget(const QString& id, const QString& root) const {
    QSqlQuery editionsQuery(m_db);
    editionsQuery.prepare("SELECT count(*) FROM game_editions WHERE game_id=? OR root=?");
    editionsQuery.addBindValue(id);
    editionsQuery.addBindValue(root);
    return editionsQuery.exec() && editionsQuery.next() && editionsQuery.value(0).toInt() != 0;
}

bool GameLibrary::otherGameUsesRoot(const QString& root, const QString& id) const {
    QSqlQuery gamesQuery(m_db);
    gamesQuery.prepare("SELECT count(*) FROM games WHERE root=? AND id<>?");
    gamesQuery.addBindValue(root);
    gamesQuery.addBindValue(id);
    return gamesQuery.exec() && gamesQuery.next() && gamesQuery.value(0).toInt() != 0;
}


bool GameLibrary::catalogReferencesRoot(const QString& root, bool* referenced) {
    QSqlQuery query(m_db);
    query.prepare("SELECT EXISTS(SELECT 1 FROM games WHERE root=?)" +
                  QString(m_hasEditions ? " OR EXISTS(SELECT 1 FROM game_editions WHERE root=?)" : ""));
    query.addBindValue(root);
    if (m_hasEditions)
        query.addBindValue(root);
    if (!query.exec() || !query.next())
        return fail(tr("Cannot verify catalog references; no files were changed."));
    *referenced = query.value(0).toBool();
    return true;
}

QVariantList GameLibrary::pendingUninstallRecovery() {
    QVariantList result;
    if (recoveryBusy())
        return result;
    const auto base = gamesBase();
    if (base.isEmpty())
        return result;
    QDirIterator entries(base, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    int scanned = 0;
    while (entries.hasNext() && scanned++ < 10000 && result.size() < 128) {
        const auto parent = entries.next();
        const auto identity = QFileInfo(parent).fileName();
        if (!validManagedUuid(identity, parent))
            continue;
        const auto root = parent + "/install", pending = root + ".uninstall-pending";
        const QFileInfo staged(pending);
        if (!validPendingStaging(staged, pending))
            continue;
        bool referenced = false;
        if (!rootReferenced(root, &referenced))
            return {};
        if (referenced)
            continue;
        result.append(QVariantMap{{"id", identity},
                                  {"path", pending},
                                  {"markerPresent", QFileInfo(pending + "/.isolde-install.json").isFile()},
                                  {"conflict", QFileInfo::exists(root) || QFileInfo(root).isSymLink()}});
    }
    return result;
}

QVariantList GameLibrary::unregisteredManagedInstallRecovery() {
    QVariantList result;
    if (recoveryBusy())
        return result;
    const auto base = gamesBase();
    if (base.isEmpty())
        return result;
    QDirIterator entries(base, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    int scanned = 0;
    while (entries.hasNext() && scanned++ < 10000 && result.size() < 128) {
        const auto parent = entries.next();
        const auto identity = QFileInfo(parent).fileName();
        if (!validManagedUuid(identity, parent))
            continue;
        const auto root = parent + "/install";
        const QFileInfo marker(root + "/.isolde-install.json");
        if (!validUnregisteredMarker(root, marker))
            continue;
        bool referenced = false;
        if (!rootReferenced(root, &referenced))
            return {};
        if (!referenced)
            result.append(QVariantMap{{"id", identity}, {"path", root}});
    }
    return result;
}

bool GameLibrary::recoveryBusy() const {
    return !m_ready || !m_portablePhase.isEmpty();
}

QString GameLibrary::gamesBase() const {
    const auto base = ManagedGamePaths::gamesRoot(m_emulatorProfile);
    if (base.isEmpty() || !QFileInfo(base).isDir() || QFileInfo(base).canonicalFilePath() != base)
        return QString();
    return base;
}

bool GameLibrary::validManagedUuid(const QString& identity, const QString& parent) const {
    const auto id = QUuid(identity);
    return !id.isNull() && id.toString(QUuid::WithoutBraces) == identity &&
           QFileInfo(parent).canonicalFilePath() == parent;
}

bool GameLibrary::validPendingStaging(const QFileInfo& staged, const QString& pending) const {
    return staged.isDir() && !staged.isSymLink() && staged.canonicalFilePath() == pending;
}

bool GameLibrary::validUnregisteredMarker(const QString& root, const QFileInfo& marker) const {
    return ManagedGamePaths::isInstallRoot(m_emulatorProfile, root) && !marker.isSymLink() && marker.isFile() &&
           marker.canonicalFilePath() == marker.absoluteFilePath();
}

bool GameLibrary::rootReferenced(const QString& root, bool* referenced) const {
    return catalogReferencesRoot(root, referenced);
}


QVariantMap GameLibrary::resolveLaunch(const QString& id, const QString& editionId) {
    m_error.clear();
    emit errorChanged();
    if (!validLaunchSource(id))
        return {};
    if (editionId.isEmpty() && editions(id).size() > 1) {
        fail(tr("Choose which edition to play."));
        return {};
    }
    if (!editionId.isEmpty() && !ComponentSchema::safeId(editionId)) {
        fail(tr("Invalid edition identity."));
        return {};
    }
    QString runtime, root, relative;
    if (!resolveLaunchQuery(id, editionId, runtime, root, relative))
        return {};
    if (!compatibleRuntime(runtime, !root.isEmpty())) {
        fail(tr("The game runtime is missing or incompatible. Repair it and retry."));
        return {};
    }
    if (!readyLaunchRuntime(runtime))
        return {};
    if (runtime == "retroarch-psx") {
        QString error;
        if (!GameEmulators::playStationFirmwareReady(m_emulatorProfile, &error)) {
            fail(error);
            return {};
        }
        if (root.isEmpty() || QFileInfo(relative).suffix().compare("chd", Qt::CaseInsensitive) != 0) {
            fail(tr("This PlayStation route requires a single CHD disc image."));
            return {};
        }
    }
    if (runtime == "steam")
        return resolveSteamLaunch(relative);
    if (root.isEmpty())
        return {{"type", "launch-app"}, {"appId", runtime}};
    return resolveStandardLaunch(root, relative, runtime);
}

bool GameLibrary::validLaunchSource(const QString& id) const {
    return !m_portablePhase.startsWith("uninstall") && m_ready && ComponentSchema::safeId(id);
}

bool GameLibrary::resolveLaunchQuery(const QString& id, const QString& editionId, QString& runtime, QString& root,
                                     QString& relative) const {
    QSqlQuery query(m_db);
    if (editionId.isEmpty() || editionId == id) {
        query.prepare("SELECT runtime,root,target FROM games WHERE id=?");
        query.addBindValue(id);
    } else if (m_hasEditions) {
        query.prepare("SELECT runtime,root,target FROM game_editions WHERE game_id=? AND id=?");
        query.addBindValue(id);
        query.addBindValue(editionId);
    } else {
        fail(tr("Edition not found in this profile."));
        return false;
    }
    if (!query.exec() || !query.next()) {
        fail(tr("Game not found in this profile."));
        return false;
    }
    runtime = query.value(0).toString();
    root = query.value(1).toString();
    relative = query.value(2).toString();
    return true;
}

bool GameLibrary::readyLaunchRuntime(const QString& runtime) const {
    if (GameEmulators::definition(runtime).isEmpty())
        return true;
    QString error;
    return GameEmulators::setupAssetReady(m_emulatorProfile, runtime, &error);
}

QVariantMap GameLibrary::resolveSteamLaunch(const QString& relative) const {
    if (ComponentSchema::gameRegistration({{"schemaVersion", 1},
                                           {"title", "Steam game"},
                                           {"platform", "Steam"},
                                           {"runtimeId", "steam"},
                                           {"root", QString()},
                                           {"target", relative}})
            .isEmpty()) {
        fail(tr("Invalid Steam identity. Remove the entry and import it again."));
        return {};
    }
    return {{"type", "steam-game"}, {"steamAppId", relative}};
}

QVariantMap GameLibrary::resolveStandardLaunch(const QString& root, const QString& relative,
                                               const QString& runtime) {
    const auto target = checkedTarget(root, relative);
    if (target.isEmpty())
        return {};
    const bool optical = OpticalDiscIdentify::isRootReference(root);
    const auto media =
        optical ? OpticalDiscClassify::resolveRootReference(root).value("disc").toMap() : QVariantMap{};
    const auto sourceRoot = optical ? media.value("mountRoot").toString() : root;
    QVariantMap action{{"type", "open-file"}, {"openWith", runtime}, {"target", target}, {"sourceRoot", sourceRoot}};
    if (optical) {
        action["opticalMediaId"] = media.value("mediaId");
        action["opticalReference"] = root;
        action["opticalObservationId"] = media.value("observationId");
    }
    return action;
}


bool GameLibrary::installationExists(const QString& runtime, const QString& root, const QString& target) {
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?" +
                  QString(m_hasEditions
                              ? " UNION ALL SELECT id FROM game_editions WHERE runtime=? AND root=? AND target=?"
                              : ""));
    for (int i = 0; i < (m_hasEditions ? 2 : 1); ++i) {
        query.addBindValue(runtime);
        query.addBindValue(root);
        query.addBindValue(target);
    }
    if (!query.exec()) {
        fail(query.lastError().text());
        return true;
    }
    return query.next();
}

QVariantList GameLibrary::editions(const QString& id) {
    const auto base = registration(id);
    if (base.isEmpty())
        return {};
    QVariantList result{QVariantMap{{"id", id},
                                    {"title", base.value("title")},
                                    {"platform", base.value("platform")},
                                    {"runtimeId", base.value("runtimeId")}}};
    if (!m_hasEditions)
        return result;
    QSqlQuery query(m_db);
    query.prepare("SELECT id,title,platform,runtime FROM game_editions WHERE game_id=? ORDER BY id LIMIT 31");
    query.addBindValue(id);
    if (!query.exec()) {
        fail(query.lastError().text());
        return {};
    }
    while (query.next())
        result.append(QVariantMap{{"id", query.value(0)},
                                  {"title", query.value(1)},
                                  {"platform", query.value(2)},
                                  {"runtimeId", query.value(3)}});
    return result;
}

bool GameLibrary::steamGameContainsAppId(const QString& id, const QString& identity) {
    if (identity.size() > 10)
        return false;
    bool valid = false;
    const auto number = identity.toULongLong(&valid);
    if (!m_ready || !ComponentSchema::safeId(id) || !valid || number == 0 || number > 4294967295ULL ||
        QString::number(number) != identity)
        return false;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id FROM games WHERE id=? AND runtime='steam' AND target=?" +
        QString(m_hasEditions
                    ? " UNION ALL SELECT id FROM game_editions WHERE game_id=? AND runtime='steam' AND target=?"
                    : "") +
        " LIMIT 1");
    query.addBindValue(id);
    query.addBindValue(identity);
    if (m_hasEditions) {
        query.addBindValue(id);
        query.addBindValue(identity);
    }
    return query.exec() && query.next();
}

QString GameLibrary::addEdition(const QString& id, const QVariantMap& input) {
    return addEditionWithOrigin(id, input, "manual-registration");
}


