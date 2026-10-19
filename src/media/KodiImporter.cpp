#include "KodiImporter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QDebug>

KodiImporter::KodiImporter(const QString &profile, QObject *parent)
    : QObject(parent), m_profile(profile) {
    loadImportState();
}

KodiImporter::~KodiImporter() = default;

QString KodiImporter::kodiUserdataDir() const {
    return QDir::homePath() + QLatin1String("/.kodi/userdata");
}

QString KodiImporter::backupDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QLatin1String("/kodi-import-backups/") + m_profile;
}

QString KodiImporter::importStateFile() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QLatin1String("/kodi-import-state-") + m_profile + QLatin1String(".json");
}

bool KodiImporter::loadImportState() {
    QFile file(importStateFile());
    if (!file.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return false;
    auto obj = doc.object();
    m_backupAvailable = obj.value(QStringLiteral("backupAvailable")).toBool();
    m_backupPath = obj.value(QStringLiteral("backupPath")).toString();
    m_conflicts = obj.value(QStringLiteral("conflicts")).toArray().toVariantList()
        .value<QStringList>();
    m_previewText = obj.value(QStringLiteral("previewText")).toString();
    m_canImport = obj.value(QStringLiteral("canImport")).toBool();
    m_canRollback = obj.value(QStringLiteral("canRollback")).toBool();
    m_importStatus = obj.value(QStringLiteral("importStatus")).toString();
    m_scanResults = obj.value(QStringLiteral("scanResults")).toObject().toVariantMap();
    return true;
}

bool KodiImporter::saveImportState() {
    QJsonObject obj;
    obj.insert(QStringLiteral("backupAvailable"), m_backupAvailable);
    obj.insert(QStringLiteral("backupPath"), m_backupPath);
    QJsonArray conflicts;
    for (const auto &c : m_conflicts) conflicts.append(c);
    obj.insert(QStringLiteral("conflicts"), conflicts);
    obj.insert(QStringLiteral("previewText"), m_previewText);
    obj.insert(QStringLiteral("canImport"), m_canImport);
    obj.insert(QStringLiteral("canRollback"), m_canRollback);
    obj.insert(QStringLiteral("importStatus"), m_importStatus);
    QJsonObject scanResults;
    for (auto it = m_scanResults.begin(); it != m_scanResults.end(); ++it)
        scanResults.insert(it.key(), QJsonValue::fromVariant(it.value()));
    obj.insert(QStringLiteral("scanResults"), scanResults);

    QSaveFile file(importStateFile());
    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        m_importStatus = "Could not save import state.";
        emit changed();
        return false;
    }
    return true;
}

void KodiImporter::resetState() {
    m_backupAvailable = false;
    m_backupPath.clear();
    m_importInProgress = false;
    m_importStatus = "Not started";
    m_conflicts.clear();
    m_previewText.clear();
    m_canImport = false;
    m_canRollback = false;
    m_scanResults.clear();
    emit changed();
}

QString KodiImporter::readSetting(const QString &file, const QString &key) const {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QString content = QString::fromUtf8(f.readAll());
    f.close();
    const QString marker = key + QLatin1String("=\"");
    int pos = content.indexOf(marker);
    if (pos < 0) return {};
    pos += marker.size();
    int end = content.indexOf(QLatin1String("\""), pos);
    if (end < 0) return {};
    return content.mid(pos, end - pos);
}

void KodiImporter::scanExistingKodi() {
    resetState();
    const QString userdata = kodiUserdataDir();
    QDir dir(userdata);
    if (!dir.exists()) {
        m_importStatus = "No existing Kodi userdata found at " + userdata;
        emit changed();
        return;
    }

    QVariantMap results;

    QString skin = readSetting(userdata + QLatin1String("/guisettings.xml"), QStringLiteral("skin"));
    results.insert(QStringLiteral("skin"), skin.isEmpty() ? QStringLiteral("(default)") : skin);

    QString sourcesFile = userdata + QLatin1String("/sources.xml");
    int sourceCount = 0;
    QFile sf(sourcesFile);
    if (sf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString src = QString::fromUtf8(sf.readAll());
        sf.close();
        sourceCount = src.count(QLatin1String("<source"));
    }
    results.insert(QStringLiteral("sourceCount"), sourceCount);
    results.insert(QStringLiteral("sourcesFile"), QFileInfo::exists(sourcesFile));

    QString addonsDir = userdata + QLatin1String("/addons");
    int addonCount = 0;
    QDir ad(addonsDir);
    if (ad.exists()) addonCount = ad.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
    results.insert(QStringLiteral("addonCount"), addonCount);
    results.insert(QStringLiteral("addonsDir"), ad.exists());

    QString dbFile = userdata + QLatin1String("/Database/MyVideos.db");
    results.insert(QStringLiteral("watchStateDb"), QFileInfo::exists(dbFile));

    QString keymapsDir = userdata + QLatin1String("/keymaps");
    results.insert(QStringLiteral("keymapsDir"), QDir(keymapsDir).exists());

    m_scanResults = results;

    QStringList lines;
    lines << "Existing Kodi setup found at " + userdata;
    lines << QString("Skin: %1").arg(results.value(QStringLiteral("skin")).toString());
    lines << QString("Configured media sources: %1").arg(results.value(QStringLiteral("sourceCount")).toInt());
    lines << QString("Installed add-ons: %1").arg(results.value(QStringLiteral("addonCount")).toInt());
    lines << QString("Watch/resume database: %1")
        .arg(results.value(QStringLiteral("watchStateDb")).toBool() ? "Yes" : "No");
    lines << QString("Keymaps: %1")
        .arg(results.value(QStringLiteral("keymapsDir")).toBool() ? "Present" : "None");
    m_previewText = lines.join(QLatin1String("\n"));

    m_conflicts.clear();
    if (results.value(QStringLiteral("sourcesFile")).toBool())
        m_conflicts << "Existing sources.xml will be merged with Isolde-managed sources.";
    if (results.value(QStringLiteral("watchStateDb")).toBool())
        m_conflicts << "Existing watch/resume state will be preserved; Isolde does not edit Kodi databases.";
    if (results.value(QStringLiteral("addonCount")).toInt() > 0)
        m_conflicts << "Existing add-ons detected; skin/add-on settings are read-only references.";

    m_canImport = true;
    m_canRollback = false;
    m_importStatus = "Scan complete. Ready to backup and import.";
    emit changed();
    saveImportState();
}

void KodiImporter::createBackup() {
    if (!m_canImport) {
        m_importStatus = "Scan existing Kodi setup first before backing up.";
        emit changed();
        return;
    }

    const QString userdata = kodiUserdataDir();
    const QString dest = backupDir();
    QDir().mkpath(dest);

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString backupPath = dest + QLatin1String("/kodi-backup-") + timestamp;

    QStringList filesToBackup{
        QStringLiteral("guisettings.xml"),
        QStringLiteral("sources.xml"),
        QStringLiteral("keymaps"),
        QStringLiteral("addons"),
        QStringLiteral("Database")
    };

    bool ok = true;
    for (const auto &name : filesToBackup) {
        QFileInfo srcInfo(userdata + QLatin1String("/") + name);
        if (!srcInfo.exists()) continue;
        if (srcInfo.isDir()) {
            if (!QDir(backupPath).mkpath(name)) { ok = false; break; }
            QDir srcDir(srcInfo.absoluteFilePath());
            for (const auto &entry : srcDir.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
                if (!QFile::copy(srcDir.absoluteFilePath(entry),
                                 backupPath + QLatin1String("/") + name + QLatin1String("/") + entry)) {
                    ok = false; break;
                }
            }
            if (!ok) break;
        } else {
            if (!QFile::copy(srcInfo.absoluteFilePath(), backupPath + QLatin1String("/") + name)) {
                ok = false; break;
            }
        }
    }

    if (!ok) {
        m_importStatus = "Backup failed: could not copy all Kodi config files.";
        emit changed();
        return;
    }

    m_backupPath = backupPath;
    m_backupAvailable = true;
    m_canRollback = true;
    m_importStatus = "Backup created at " + backupPath;
    emit backupCreated(backupPath);
    emit changed();
    saveImportState();
}

void KodiImporter::previewImport() {
    if (!m_canImport) {
        m_importStatus = "Scan existing Kodi setup first before previewing.";
        emit changed();
        return;
    }
    m_importStatus = "Preview: " + m_previewText;
    if (!m_conflicts.isEmpty()) {
        m_importStatus += QLatin1String("\n\nConflicts:\n") + m_conflicts.join(QLatin1String("\n"));
    }
    emit changed();
}

void KodiImporter::import() {
    if (!m_canImport || m_importInProgress) {
        m_importStatus = "Cannot import: scan first, or an import is already in progress.";
        emit changed();
        return;
    }

    if (!m_backupAvailable) {
        m_importStatus = "Create a backup before importing.";
        emit changed();
        return;
    }

    m_importInProgress = true;
    m_importStatus = "Importing Kodi settings...";
    emit changed();

    const QString userdata = kodiUserdataDir();
    bool ok = true;

    const QString addonsSrc = userdata + QLatin1String("/addons");
    const QString keymapsSrc = userdata + QLatin1String("/keymaps");

    QFileInfo sourcesInfo(userdata + QLatin1String("/sources.xml"));
    if (sourcesInfo.exists()) {
        m_conflicts << "Existing sources.xml detected: Isolde preserves its own source grants; "
                     "configure Kodi sources natively in Kodi → Videos → Files → Add videos.";
    }

    if (!ok) {
        m_importStatus = "Import failed during file copy.";
        m_importInProgress = false;
        emit changed();
        return;
    }

    m_importInProgress = false;
    m_canRollback = true;
    m_importStatus = "Import complete. Kodi skin, add-ons, keymaps, and watch/resume state "
                     "are preserved by reference. Isolde does not edit Kodi databases or media files.";
    emit importCompleted(true, m_importStatus);
    emit changed();
    saveImportState();
}

void KodiImporter::rollback() {
    if (!m_canRollback || m_backupPath.isEmpty()) {
        m_importStatus = "No backup available to roll back.";
        emit changed();
        return;
    }

    m_importInProgress = true;
    m_importStatus = "Rolling back...";
    emit changed();

    const QString userdata = kodiUserdataDir();
    QDir backup(m_backupPath);
    if (!backup.exists()) {
        m_importStatus = "Backup directory not found: " + m_backupPath;
        m_importInProgress = false;
        emit changed();
        return;
    }

    bool ok = true;
    QStringList filesToRestore{
        QStringLiteral("guisettings.xml"),
        QStringLiteral("sources.xml")
    };

    for (const auto &name : filesToRestore) {
        QFileInfo backupInfo(m_backupPath + QLatin1String("/") + name);
        if (!backupInfo.exists()) continue;
        if (!QFile::copy(backupInfo.absoluteFilePath(),
                         userdata + QLatin1String("/") + name)) {
            ok = false; break;
        }
    }

    m_importInProgress = false;
    if (ok) {
        m_canRollback = false;
        m_backupAvailable = false;
        m_backupPath.clear();
        m_importStatus = "Rollback complete. Kodi config restored from backup.";
        emit rollbackCompleted(true, m_importStatus);
    } else {
        m_importStatus = "Rollback partially failed. Some files may not have been restored.";
        emit rollbackCompleted(false, m_importStatus);
    }
    emit changed();
    saveImportState();
}

void KodiImporter::clearConflicts() {
    m_conflicts.clear();
    emit changed();
    saveImportState();
}