#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include "GameEmulators.h"
#include "GameResourceLimits.h"
#include "media/MediaGrants.h"

namespace
{
bool
protectedPath(const QString& path)
{
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path)
        return false;
    auto info = QFileInfo(path);
    while (true)
    {
        if (info.isSymLink())
            return false;
        if (info.absoluteFilePath() == "/")
            break;
        const auto parent = QFileInfo(info.absolutePath());
        if (parent.absoluteFilePath() == info.absoluteFilePath())
            break;
        info = parent;
    }
    return true;
}
bool
save(const QString& path, const QByteArray& bytes, QString* error)
{
    if (!protectedPath(path) || !QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        *error = "Cannot prepare private emulator storage.";
        return false;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        *error = "Cannot save emulator setup. Previous settings were preserved. " + file.errorString();
        return false;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}
bool
psxFirmware(const QByteArray& bytes)
{
    static const QSet<QByteArray> hashes{"b05def971d8ec59f346f2d9ac21fb742e3eb6917",
                                         "0555c6fae8906f3f09baf5988f00e55f88e9f30b",
                                         "f6bc2d1f5eb6593de7d089c425ac681d6fffd3f0"};
    return bytes.size() == 512 * 1024 &&
           hashes.contains(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}
bool
shouldRetirePsxFirmware(const QFileInfo& info, const QString& keep)
{
    if (info.fileName() == keep)
        return false;
    if (!QRegularExpression("^[a-f0-9]{64}\\.bin$").match(info.fileName()).hasMatch())
        return false;
    if (info.isSymLink() || !info.isFile() || info.size() != 512 * 1024)
        return false;
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto bytes = file.read(512 * 1024 + 1);
    if (!psxFirmware(bytes) ||
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".bin" !=
            info.fileName())
        return false;
    return true;
}
bool
prunePsxFirmware(const QString& root, const QString& keep, QString* error)
{
    const auto directory = root + "/firmware";
    if (!protectedPath(directory))
    {
        *error = "Private firmware storage is redirected.";
        return false;
    }
    QDir dir(directory);
    if (!dir.exists())
        return true;
    const auto names = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    if (names.size() > 8)
    {
        *error = "Firmware storage contains too many entries. Review it before importing more.";
        return false;
    }
    for (const auto& name : names)
    {
        const auto path = dir.filePath(name);
        const QFileInfo info(path);
        if (!shouldRetirePsxFirmware(info, keep))
            continue;
        if (!QFile::remove(path))
        {
            *error = "A previous private firmware copy could not be retired.";
            return false;
        }
    }
    return true;
}
bool
shouldRetireFirmware(const QFileInfo& info, const QString& keep)
{
    if (info.fileName() == keep)
        return false;
    if (!QRegularExpression("^[a-f0-9]{64}\\.bin$").match(info.fileName()).hasMatch())
        return false;
    if (info.isSymLink() || !info.isFile() || (info.size() != 4 * 1024 * 1024 && info.size() != 8 * 1024 * 1024))
        return false;
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto bytes = file.read(8 * 1024 * 1024 + 1);
    if (bytes.size() != info.size() || !bytes.contains("ROMDIR") || !bytes.contains("ROMVER") ||
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".bin" !=
            info.fileName())
        return false;
    return true;
}
bool
pruneFirmware(const QString& root, const QString& keep, QString* error)
{
    const auto directory = root + "/firmware";
    if (!protectedPath(directory))
    {
        *error = "Private firmware storage is redirected.";
        return false;
    }
    QDir dir(directory);
    if (!dir.exists())
        return true;
    const auto names = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (names.size() > 8)
    {
        *error = "Firmware storage contains too many entries. Review it before importing more.";
        return false;
    }
    for (const auto& name : names)
    {
        const auto path = dir.filePath(name);
        const QFileInfo info(path);
        if (!shouldRetireFirmware(info, keep))
            continue;
        if (!QFile::remove(path))
        {
            *error = "A previous private firmware copy could not be retired.";
            return false;
        }
    }
    return true;
}
namespace
{
bool
hasValidSettingsKeys(const QVariantMap& value)
{
    const QSet<QString> keys{"schemaVersion", "video", "controller", "firmware"};
    return QSet<QString>(value.keys().begin(), value.keys().end()) == keys;
}
bool
hasValidSchemaVersion(const QVariantMap& value)
{
    const auto type = value.value("schemaVersion").metaType().id();
    return type == QMetaType::Int || type == QMetaType::LongLong || type == QMetaType::Double;
}
bool
hasValidStringField(const QVariantMap& value, const QString& key)
{
    return value.value(key).metaType().id() == QMetaType::QString;
}
bool
isValidVideoMode(const QString& video)
{
    return QStringList{"automatic", "software"}.contains(video);
}
bool
isValidController(const QString& controller)
{
    return QStringList{"xbox", "xbox-wii-classic"}.contains(controller);
}
bool
isValidFirmwareName(const QString& firmware)
{
    return firmware.isEmpty() || QRegularExpression("^[a-f0-9]{64}\\.bin$").match(firmware).hasMatch();
}
bool
valid(const QVariantMap& value)
{
    if (!hasValidSettingsKeys(value))
        return false;
    if (!hasValidSchemaVersion(value))
        return false;
    if (!hasValidStringField(value, "video"))
        return false;
    if (!hasValidStringField(value, "controller"))
        return false;
    if (!hasValidStringField(value, "firmware"))
        return false;
    if (value.value("schemaVersion").toDouble() != 1)
        return false;
    if (!isValidVideoMode(value.value("video").toString()))
        return false;
    if (!isValidController(value.value("controller").toString()))
        return false;
    if (!isValidFirmwareName(value.value("firmware").toString()))
        return false;
    return true;
}
bool
setupAssetHeader(const QByteArray& header, const QString& kind)
{
    return kind == "xbox-bootset"
               ? header.startsWith("PK\003\004")
               : header.startsWith("SCEUF") || (kind == "vita-pup" && header.startsWith("PK\003\004"));
}
QString
importSetupAsset(const QString& root, const QString& sourcePath, const QString& kind, QString* error)
{
    const QFileInfo source(sourcePath);
    const qint64 maximum = kind == "xbox-bootset" ? 128LL * 1024 * 1024 : 512LL * 1024 * 1024;
    if (!MediaGrants::validate(source.absolutePath()).isEmpty() || !source.isFile() || !source.isReadable() ||
        source.canonicalFilePath() != sourcePath || source.size() < 16 || source.size() > maximum)
    {
        *error = "Choose a bounded readable firmware file in an approved folder.";
        return {};
    }
    QFile input(sourcePath);
    if (!input.open(QIODevice::ReadOnly) || !setupAssetHeader(input.peek(16), kind))
    {
        *error = "The selected file does not match this runtime's firmware format.";
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!input.atEnd())
    {
        const auto block = input.read(1024 * 1024);
        total += block.size();
        if (block.isEmpty() || total > maximum)
        {
            *error = "Firmware read failed or exceeded its byte limit.";
            return {};
        }
        hash.addData(block);
    }
    if (input.error() != QFile::NoError || total != source.size())
    {
        *error = "Firmware changed while reading; retry the import.";
        return {};
    }
    const auto name = QString::fromLatin1(hash.result().toHex()) + ".bin";
    const auto directory = root + "/firmware";
    const auto destination = directory + '/' + name;
    if (!protectedPath(destination) || !QDir().mkpath(directory) ||
        (QDir(directory).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).size() >= 8 &&
         !QFileInfo::exists(destination)))
    {
        *error = "Private firmware storage is redirected, unavailable or full.";
        return {};
    }
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!input.seek(0) || !output.open(QIODevice::WriteOnly))
    {
        *error = "Cannot stage a private firmware copy.";
        return {};
    }
    QCryptographicHash second(QCryptographicHash::Sha256);
    total = 0;
    while (!input.atEnd())
    {
        const auto block = input.read(1024 * 1024);
        total += block.size();
        if (block.isEmpty() || total > maximum || output.write(block) != block.size())
        {
            *error = "Cannot copy firmware; previous settings were preserved.";
            return {};
        }
        second.addData(block);
    }
    if (input.error() != QFile::NoError || total != source.size() || second.result() != hash.result() ||
        !output.commit())
    {
        *error = "Firmware changed or its private copy could not be saved. Previous settings were preserved.";
        return {};
    }
    QFile::setPermissions(destination, QFile::ReadOwner | QFile::WriteOwner);
    return name;
}
} // namespace
QVariantList
GameEmulators::definitions()
{
    QFile file(":/config/game-runtimes.json");
    if (!file.exists())
        file.setFileName(QStringLiteral(ISOLDE_GAME_RUNTIME_SOURCE));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
        return {};
    const auto doc = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    if (!validateEmulatorDocument(doc))
        return {};
    const auto target = doc.value("target").toMap();
    if (!validateEmulatorTarget(target))
        return {};
    const auto generations = doc.value("generations").toMap();
    if (!validateEmulatorGenerations(generations))
        return {};
    const auto runtimes = doc.value("runtimes").toList();
    const auto coverage = doc.value("generationCoverage").toMap();
    QSet<QString> actualPlatforms;
    if (!validateEmulatorRuntimes(runtimes, actualPlatforms))
        return {};
    const auto integrated = coverage.value("integratedPlatforms").toStringList();
    QSet<QString> integratedSet(integrated.begin(), integrated.end());
    if (!validateEmulatorCoverage(coverage, integratedSet))
        return {};
    if (actualPlatforms != integratedSet)
        return {};
    return runtimes;
}

bool
GameEmulators::validateEmulatorDocument(const QVariantMap& doc)
{
    const auto documentKeys = doc.keys();
    const QSet<QString> expectedKeys{"schemaVersion", "target", "generationCoverage", "generations", "runtimes"};
    const auto schemaType = doc.value("schemaVersion").metaType().id();
    if (documentKeys.size() != expectedKeys.size() ||
        !std::all_of(documentKeys.cbegin(),
                     documentKeys.cend(),
                     [&expectedKeys](const QString& key)
                     {
                         return expectedKeys.contains(key);
                     }) ||
        (schemaType != QMetaType::Int && schemaType != QMetaType::LongLong && schemaType != QMetaType::Double) ||
        doc.value("schemaVersion").toDouble() != 2)
        return false;
    return true;
}

bool
GameEmulators::validateEmulatorTarget(const QVariantMap& target)
{
    return target.value("os").toString() == "ubuntu" &&
           target.value("version").toString() == "26.04" &&
           target.value("architectures").toStringList() == QStringList{"amd64"} &&
           !target.value("supportBoundary").toString().isEmpty();
}

bool
GameEmulators::validateEmulatorGenerations(const QVariantMap& generations)
{
    if (generations.isEmpty())
        return false;
    static const QSet<QString> generationKeys{"architecture",
                                              "installer",
                                              "account",
                                              "save",
                                              "dependency",
                                              "budget",
                                              "firmwareRequired",
                                              "unsupportedReason"};
    for (const auto& key : generations.keys())
    {
        const auto value = generations.value(key).toMap();
        if (!value.isEmpty())
        {
            const auto keyList = value.keys();
            const QSet<QString> actualKeys(keyList.cbegin(), keyList.cend());
            if (!std::all_of(actualKeys.cbegin(),
                             actualKeys.cend(),
                             [&generationKeys](const QString& k)
                             {
                                 return generationKeys.contains(k);
                             }))
                return false;
            const auto budget = value.value("budget").toMap();
            if (!budget.isEmpty() && (!QJsonValue::fromVariant(budget.value("diskMiB")).isDouble() ||
                                      !QJsonValue::fromVariant(budget.value("ramMiB")).isDouble()))
                return false;
        }
    }
    return true;
}

bool
GameEmulators::validateEmulatorRuntimes(const QVariantList& runtimes, QSet<QString>& actualPlatforms)
{
    static const QSet<QString> engines{
        "ppsspp", "dolphin", "pcsx2", "retroarch", "rpcs3", "shadps4", "vita3k", "xemu", "xenia", "cemu"};
    static const QMap<QString, QString> expandedKinds{{"rpcs3", "ps3-pup"},
                                                      {"shadps4", "none"},
                                                      {"vita3k", "vita-pup"},
                                                      {"xemu", "xbox-bootset"},
                                                      {"xenia", "none"},
                                                      {"cemu", "none"}};
    QSet<QString> ids;
    for (const auto& entry : runtimes)
    {
        if (!validateEmulatorRuntimeItem(item, ids, actualPlatforms))
            return false;
    }
    return !runtimes.isEmpty();
}

bool
GameEmulators::validateEmulatorRuntimeItem(const QVariantMap& item, QSet<QString>& ids, QSet<QString>& actualPlatforms)
{
    const auto itemKeys = item.keys();
    const QSet<QString> allowedFields{
        "id", "name", "engine", "platforms", "formats", "binary", "core", "packages", "firmwareRequired", "setup"};
    for (const auto& key : itemKeys)
        if (!allowedFields.contains(key))
            return false;
    const auto id = item.value("id").toString(), engine = item.value("engine").toString();
    const auto packages = item.value("packages").toStringList();
    const auto formats = item.value("formats").toStringList();
    const auto platforms = item.value("platforms").toStringList();
    static const QSet<QString> engines{
        "ppsspp", "dolphin", "pcsx2", "retroarch", "rpcs3", "shadps4", "vita3k", "xemu", "xenia", "cemu"};
    static const QMap<QString, QString> expandedKinds{{"rpcs3", "ps3-pup"},
                                                      {"shadps4", "none"},
                                                      {"vita3k", "vita-pup"},
                                                      {"xemu", "xbox-bootset"},
                                                      {"xenia", "none"},
                                                      {"cemu", "none"}};
    if (expandedKinds.contains(engine) &&
        (id != engine || !item.contains("setup") ||
         item.value("setup").toMap().value("firmwareKind").toString() != expandedKinds.value(engine)))
        return false;
    if (!validateEmulatorRuntimeSetup(item))
        return false;
    if (item.isEmpty() || !QRegularExpression("^[a-z0-9-]{1,48}$").match(id).hasMatch() || ids.contains(id) ||
        !engines.contains(engine) || item.value("binary").toString().isEmpty() ||
        !QDir::isAbsolutePath(item.value("binary").toString()) || packages.isEmpty() || platforms.isEmpty() ||
        formats.isEmpty() || item.value("firmwareRequired").metaType().id() != QMetaType::Bool ||
        (item.contains("core") && !QDir::isAbsolutePath(item.value("core").toString())))
        return false;
    if (!validateEmulatorRuntimePackages(engine, item, packages))
        return false;
    ids.insert(id);
    for (const auto& package : packages)
        if (!QRegularExpression("^[a-z0-9][a-z0-9+.-]{0,80}$").match(package).hasMatch())
            return false;
    for (const auto& format : formats)
        if (!QRegularExpression("^[a-z0-9]{1,8}$").match(format).hasMatch())
            return false;
    for (const auto& platform : platforms)
        actualPlatforms.insert(platform);
    return true;
}

bool
GameEmulators::validateEmulatorRuntimePackages(const QString& engine, const QVariantMap& item, const QStringList& packages)
{
    const QSet<QString> packageSet(packages.cbegin(), packages.cend());
    if (engine == "retroarch")
    {
        int corePackages = 0;
        for (const auto& package : packages)
            if (package.startsWith("libretro-"))
                ++corePackages;
        if (!item.contains("core") || !packageSet.contains("retroarch") || corePackages != 1)
            return false;
    }
    else if (engine == "rpcs3")
    {
        if (item.contains("core") || packageSet != QSet<QString>{"isolde-rpcs3"})
            return false;
    }
    else if (engine == "shadps4")
    {
        if (item.contains("core") || packageSet != QSet<QString>{"isolde-shadps4"})
            return false;
    }
    else if (engine == "vita3k")
    {
        if (item.contains("core") || packageSet != QSet<QString>{"isolde-vita3k"})
            return false;
    }
    else if (engine == "xemu")
    {
        if (item.contains("core") || packageSet != QSet<QString>{"isolde-xemu"})
            return false;
    }
    else if (engine == "xenia")
    {
        if (item.contains("core") || packageSet != QSet<QString>{"isolde-xenia"})
            return false;
    }
    else if (engine == "cemu")
    {
        if (item.contains("core") || packageSet != QSet<QString>{"isolde-cemu"})
            return false;
    }
    else
    {
        const QSet<QString> expectedPackage = engine == "ppsspp"    ? QSet<QString>{"isolde-ppsspp"}
                                              : engine == "dolphin" ? QSet<QString>{"dolphin-emu"}
                                                                      : QSet<QString>{"pcsx2"};
        if (item.contains("core") || packageSet != expectedPackage)
            return false;
    }
    return true;
}


bool
GameEmulators::validateEmulatorCoverage(const QVariantMap& coverage, const QSet<QString>& integratedSet)
{
    const auto notIntegrated = coverage.value("notIntegrated").toStringList();
    const QSet<QString> notIntegratedSet(notIntegrated.begin(), notIntegrated.end());
    if (integratedSet.size() != coverage.value("integratedPlatforms").toStringList().size() ||
        notIntegratedSet.size() != notIntegrated.size())
        return false;
    for (const auto& platform : integratedSet)
        if (notIntegratedSet.contains(platform))
            return false;
    return true;
}

QVariantMap

QVariantMap
GameEmulators::definition(const QString& id)
{
    for (const auto& item : definitions())
        if (item.toMap().value("id").toString() == id)
            return item.toMap();
    return {};
}
QVariantMap
GameEmulators::coverage()
{
    QFile file(":/config/game-runtimes.json");
    if (!file.exists())
        file.setFileName(QStringLiteral(ISOLDE_GAME_RUNTIME_SOURCE));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
        return {};
    const auto doc = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    if (doc.value("schemaVersion").toInt() != 2 || definitions().isEmpty())
        return {};
    const auto target = doc.value("target").toMap();
    const QVariantMap budgets{{"catalogEntries", GamesResourceLimits::CatalogEntries},
                              {"pageEntries", GamesResourceLimits::PageEntries},
                              {"platforms", GamesResourceLimits::Platforms},
                              {"collections", GamesResourceLimits::Collections},
                              {"editionsPerGame", GamesResourceLimits::EditionsPerGame},
                              {"provenanceRecords", GamesResourceLimits::ProvenanceRecords},
                              {"catalogArchiveBytes", GamesResourceLimits::CatalogArchiveBytes},
                              {"artworkRestoreBytes", GamesResourceLimits::ArtworkRestoreBytes},
                              {"artworkStoreBytes", GamesResourceLimits::ArtworkStoreBytes}};
    return {{"target", target},
            {"budgets", budgets},
            {"integratedPlatforms", doc.value("generationCoverage").toMap().value("integratedPlatforms")},
            {"notIntegrated", doc.value("generationCoverage").toMap().value("notIntegrated")}};
}
QString
GameEmulators::home(const QString& profile, const QString& id)
{
    if (definition(id).isEmpty() ||
        (profile != "default" && (QUuid(profile).isNull() || QUuid(profile).toString(QUuid::WithoutBraces) != profile)))
        return {};
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles/" + profile + "/" +
           id + "/home";
}
QVariantMap
GameEmulators::settings(const QString& profile, const QString& id, QString* error)
{
    const auto root = home(profile, id);
    if (root.isEmpty())
    {
        *error = "Unknown emulator or profile.";
        return {};
    }
    const auto path = root + "/.config/isolde-emulator/settings.json";
    if (!protectedPath(path))
    {
        *error = "Emulator settings are redirected. Repair storage before continuing.";
        return {};
    }
    if (!QFileInfo::exists(path))
        return {{"schemaVersion", 1}, {"video", "automatic"}, {"controller", "xbox"}, {"firmware", QStringLiteral("")}};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16384)
    {
        *error = "Cannot read emulator settings. Original data was preserved.";
        return {};
    }
    const auto result = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    if (!valid(result))
    {
        *error = "Invalid emulator settings. Original data was preserved.";
        return {};
    }
    return result;
}
bool
GameEmulators::playStationFirmwareReady(const QString& profile, QString* error)
{
    const auto value = settings(profile, "retroarch-psx", error);
    if (value.isEmpty())
        return false;
    const auto name = value.value("firmware").toString();
    if (name.isEmpty())
    {
        *error = "Import your matching regional PlayStation firmware in Emulator setup before playing.";
        return false;
    }
    const auto path = home(profile, "retroarch-psx") + "/firmware/" + name;
    if (!protectedPath(path))
    {
        *error = "PlayStation firmware storage is redirected.";
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() != 512 * 1024)
    {
        *error = "PlayStation firmware is missing or invalid. Import it again.";
        return false;
    }
    const auto bytes = file.read(512 * 1024 + 1);
    if (!psxFirmware(bytes) ||
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".bin" != name)
    {
        *error = "PlayStation firmware changed or is invalid. Import it again.";
        return false;
    }
    return true;
}
bool
GameEmulators::configure(
    const QString& profile, const QString& id, const QVariantMap& options, const QString& firmware, QString* error)
{
    auto value = settings(profile, id, error);
    if (value.isEmpty())
        return false;
    if (!validateConfigureOptions(options, error))
        return false;
    value["video"] = options.value("video");
    value["controller"] = options.value("controller");
    if (!valid(value) || (id != "dolphin" && value.value("controller").toString() != "xbox"))
    {
        *error = "Choose a supported renderer and controller profile.";
        return false;
    }
    const auto setup = definition(id).value("setup").toMap();
    if (!setup.isEmpty() && !setup.value("videoModes").toStringList().contains(value.value("video").toString()))
    {
        *error = "This runtime does not provide the selected renderer.";
        return false;
    }
    const auto root = home(profile, id);
    if (!setup.isEmpty())
    {
        if (!firmware.isEmpty())
        {
            if (!importFirmwareWithSetup(root, setup, firmware, value, error))
                return false;
        }
        return save(root + "/.config/isolde-emulator/settings.json", QJsonDocument::fromVariant(value).toJson(), error);
    }
    if (!importFirmwareWithoutSetup(root, setup, firmware, id, value, error))
        return false;
    if (!save(root + "/.config/isolde-emulator/settings.json", QJsonDocument::fromVariant(value).toJson(), error))
        return false;
    QString cleanupError;
    if (id == "retroarch-psx")
        prunePsxFirmware(root, value.value("firmware").toString(), &cleanupError);
    else
        pruneFirmware(root, value.value("firmware").toString(), &cleanupError);
    return true;
}

bool
GameEmulators::validateConfigureOptions(const QVariantMap& options, QString* error)
{
    if (options.size() != 2 || !options.contains("video") || !options.contains("controller"))
    {
        *error = "Choose known emulator settings only.";
        return false;
    }
    return true;
}

bool
GameEmulators::importFirmwareWithSetup(
    const QString& root, const QVariantMap& setup, const QString& firmware, QVariantMap& value, QString* error)
{
    const auto kind = setup.value("firmwareKind").toString();
    if (kind == "none")
    {
        *error = "This runtime does not require imported firmware.";
        return false;
    }
    const auto name = importSetupAsset(root, firmware, kind, error);
    if (name.isEmpty())
        return false;
    value["firmware"] = name;
    return true;
}

bool
GameEmulators::importFirmwareWithoutSetup(const QString& root,
                                           const QVariantMap& setup,
                                           const QString& firmware,
                                           const QString& id,
                                           QVariantMap& value,
                                           QString* error)
{
    if (!firmware.isEmpty())
    {
        if (!setup.value("firmwareRequired").toBool())
        {
            *error = "This runtime does not require imported firmware.";
            return false;
        }
        if (!validateFirmwareFile(root, firmware, id, value.value("firmware").toString(), error))
            return false;
        QFile file(firmware);
        if (!file.open(QIODevice::ReadOnly))
        {
            *error = "Cannot read selected firmware.";
            return false;
        }
        const QFileInfo source(firmware);
        const auto bytes = file.read(8 * 1024 * 1024 + 1);
        if (bytes.size() != source.size() ||
            (id == "retroarch-psx" ? !psxFirmware(bytes) : (!bytes.contains("ROMDIR") || !bytes.contains("ROMVER"))))
        {
            *error = id == "retroarch-psx" ? "The file is not a recognized SCPH-5500, 5501 or 5502 firmware image."
                                           : "The selected file is not recognized as PS2 firmware.";
            return false;
        }
        const auto name =
            QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".bin";
        if (!save(root + "/firmware/" + name, bytes, error))
            return false;
        value["firmware"] = name;
    }
    return true;
}
bool
GameEmulators::setupAssetReady(const QString& profile, const QString& id, QString* error)
{
    const auto route = definition(id);
    const auto setup = route.value("setup").toMap();
    if (setup.isEmpty() || !route.value("firmwareRequired").toBool())
        return true;
    const auto value = settings(profile, id, error);
    if (value.isEmpty())
        return false;
    const auto name = value.value("firmware").toString();
    if (name.isEmpty())
    {
        *error = "Import this runtime's firmware in Emulator setup before playing.";
        return false;
    }
    const auto path = home(profile, id) + "/firmware/" + name;
    const qint64 maximum = setup.value("firmwareKind") == "xbox-bootset" ? 128LL * 1024 * 1024 : 512LL * 1024 * 1024;
    QFile file(path);
    if (!protectedPath(path) || !file.open(QIODevice::ReadOnly) || file.size() < 16 || file.size() > maximum ||
        !setupAssetHeader(file.peek(16), setup.value("firmwareKind").toString()))
    {
        *error = "Firmware is missing, redirected or invalid. Import it again.";
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 size = 0;
    while (!file.atEnd())
    {
        const auto block = file.read(1024 * 1024);
        size += block.size();
        if (block.isEmpty() || size > maximum)
        {
            *error = "Firmware could not be verified.";
            return false;
        }
        hash.addData(block);
    }
    if (file.error() != QFile::NoError || QString::fromLatin1(hash.result().toHex()) + ".bin" != name)
    {
        *error = "Firmware changed. Import it again before playing.";
        return false;
    }
    return true;
}

bool
GameEmulators::validateFirmwareFile(const QString& root, const QString& firmware, const QString& id,
                                    const QString& existingFirmware, QString* error)
{
    Q_UNUSED(root);
    Q_UNUSED(existingFirmware);
    QFileInfo source(firmware);
    if (!source.isFile() || !source.isReadable())
    {
        *error = "Cannot read selected firmware.";
        return false;
    }
    const auto grant = MediaGrants::validate(source.absolutePath());
    if (!grant.isEmpty())
    {
        *error = "Choose firmware inside a game source folder.";
        return false;
    }
    return true;
}
