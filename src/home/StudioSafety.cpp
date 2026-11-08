#include "StudioSession.h"
#include "HomeLayout.h"
#include "HomeDesignData.h"
#include "ManagedGif.h"
#include <QCryptographicHash>
#include <QUrl>
#include "settings/ProfileManager.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
constexpr qint64 VersionBytes = 40 * 1024 * 1024;
constexpr int VersionCount = 10;
constexpr qint64 ArchiveBytes = 132 * 1024 * 1024;
constexpr qint64 AssetBytes = 64 * 1024 * 1024;

QJsonObject pageDocument(QJsonObject document, const QString &id)
{
    if (!id.isEmpty()) {
        for (const auto &value : document["pages"].toArray()) {
            const auto entry = value.toObject();
            if (entry["id"] == id) return entry["layout"].toObject();
        }
        return {};
    }
    for (const auto &key : {"pages", "homePageId", "rootPage", "studioAppearance"}) document.remove(key);
    return document;
}

bool profileFile(const QString &path)
{
    if (!QFileInfo(path).isAbsolute() || QFileInfo(path).isSymLink()) return false;
    QFileInfo parent(QFileInfo(path).absolutePath());
    while (!parent.exists()) {
        const QFileInfo ancestor(parent.absolutePath());
        if (ancestor.absoluteFilePath() == parent.absoluteFilePath()) return false;
        parent = ancestor;
    }
    return parent.isDir() && parent.canonicalFilePath() == parent.absoluteFilePath();
}
}

QVariantList StudioSession::changeSummary() const
{
    QVariantList result;
    const auto before = QJsonDocument::fromJson(m_committed).object();
    const auto after = QJsonDocument::fromJson(m_draft).object();
    const auto append = [&](const QString &id, const QString &name, const QString &change) {
        auto previous = pageDocument(before, id), next = pageDocument(after, id);
        QHash<QString, QJsonObject> oldNodes, newNodes;
        for (const auto &value : previous["components"].toArray()) oldNodes.insert(value.toObject()["id"].toString(), value.toObject());
        for (const auto &value : next["components"].toArray()) newNodes.insert(value.toObject()["id"].toString(), value.toObject());
        int added = 0, removed = 0, changed = 0;
        for (auto it = oldNodes.begin(); it != oldNodes.end(); ++it) {
            if (!newNodes.contains(it.key())) ++removed;
            else if (newNodes.value(it.key()) != it.value()) ++changed;
        }
        for (auto it = newNodes.begin(); it != newNodes.end(); ++it) if (!oldNodes.contains(it.key())) ++added;
        previous.remove("components"); next.remove("components");
        const auto detail = tr("%1 added, %2 removed, %3 changed").arg(added).arg(removed).arg(changed)
            + (previous != next ? tr("; page settings changed") : QString());
        result.append(QVariantMap{{"pageId", id}, {"name", name}, {"change", change}, {"details", detail}});
    };
    if (pageDocument(before, {}) != pageDocument(after, {}) || before["rootPage"] != after["rootPage"])
        append({}, m_model ? m_model->rootPageName() : tr("Home"), tr("Changed"));
    QSet<QString> seen;
    int previousIndex = 0;
    const auto nextPages = after["pages"].toArray();
    for (const auto &value : before["pages"].toArray()) {
        const auto entry = value.toObject();
        const auto id = entry["id"].toString();
        seen.insert(id);
        QJsonObject next;
        int nextIndex = -1;
        for (int index = 0; index < nextPages.size(); ++index)
            if (nextPages[index].toObject()["id"] == id) { next = nextPages[index].toObject(); nextIndex = index; break; }
        if (entry != next) append(id, entry["name"].toString(), next.isEmpty() ? tr("Removed") : tr("Changed"));
        else if (previousIndex != nextIndex) append(id, entry["name"].toString(), tr("Reordered"));
        ++previousIndex;
    }
    for (const auto &value : after["pages"].toArray()) {
        const auto entry = value.toObject();
        if (!seen.contains(entry["id"].toString())) append(entry["id"].toString(), entry["name"].toString(), tr("Added"));
    }
    if (before["homePageId"] != after["homePageId"]) append(after["homePageId"].toString(), tr("Startup page"), tr("Changed"));
    if (before["studioAppearance"] != after["studioAppearance"]) append({}, tr("Editor appearance"), tr("Changed"));
    return result;
}

QVariantList StudioSession::healthFindings() const
{
    QVariantList findings;
    if (!currentProfile() || !m_model) return findings;
    const auto document = QJsonDocument::fromJson(m_draft).object();
    QSet<QString> pages;
    for (const auto &value : document["pages"].toArray()) pages.insert(value.toObject()["id"].toString());
    const auto inspect = [&](const QJsonObject &page, const QString &pageId) {
        int reachable = 0;
        const auto components = page["components"].toArray();
        QHash<QString, QJsonObject> nodes;
        for (const auto &value : components) nodes.insert(value.toObject()["id"].toString(), value.toObject());
        const auto visible = [&](QJsonObject item) {
            for (int depth = 0; depth <= 4; ++depth) {
                if (item["hidden"].toBool()) return false;
                const auto parent = item["parentId"].toString();
                if (parent.isEmpty()) return true;
                item = nodes.value(parent);
            }
            return false;
        };
        const auto add = [&](const QString &id, const QString &property, const QString &message) {
            if (findings.size() < 256) findings.append(QVariantMap{{"pageId", pageId}, {"componentId", id},
                {"property", property}, {"message", message}, {"blocking", false}});
        };
        for (const auto &value : page["components"].toArray()) {
            const auto item = value.toObject();
            const auto id = item["id"].toString();
            const auto action = item["action"].toObject();
            const auto presentation = item["presentation"].toObject();
            const auto type = item["componentType"].toString();
            if (visible(item) && (!action.isEmpty() || item["kind"] == "app" || type == "library" || type == "carousel")) ++reachable;
            if (action["type"] == "open-page" && !pages.contains(action["pageId"].toString()))
                add(id, "action", tr("The destination page is missing. Choose another destination."));
            if (action["type"] == "root-page" && !m_model->rootPageVisible())
                add(id, "action", tr("The root page is hidden. Choose an available destination."));
            const auto appId = item["appId"].toString();
            if (!appId.isEmpty() && !m_live->registry()->find(appId))
                add(id, "source", tr("This app is unavailable. Keep the placeholder or choose another app."));
            const auto artwork = item["customLogo"].toString();
            if (!artwork.isEmpty() && !QFileInfo::exists(QUrl(m_model->managedArtworkSource(artwork)).toLocalFile()))
                add(id, "customLogo", tr("Managed artwork is missing. Choose another image or keep the placeholder."));
            if (item["name"].toString().size() > 80)
                add(id, "name", tr("This long label may be clipped. Review it at the current viewport."));
            if ((type == "button" || type == "image") && action.isEmpty())
                add(id, "action", tr("This component has no action. Intentional decoration can be kept."));
            if (type == "text" && presentation["bodyText"].toString().isEmpty())
                add(id, "bodyText", tr("This text is empty. Add text or keep it intentionally empty."));
            if (type == "library" && item["source"].toObject().isEmpty())
                add(id, "source", tr("Choose a content source or keep this empty component."));
        }
        if (!reachable) add({}, "navigation", tr("No visible content action. The permanent Settings/Edit recovery remains available."));
    };
    inspect(document, {});
    for (const auto &value : document["pages"].toArray()) {
        const auto entry = value.toObject();
        inspect(entry["layout"].toObject(), entry["id"].toString());
    }
    return findings;
}

bool StudioSession::currentProfile() const
{
    return m_active && m_live && m_profileId == qApp->property("isoldeProfile").toString();
}

bool StudioSession::profileStorageSafe() const
{
    return currentProfile() && profileFile(m_profilePath) && profileFile(m_profilePath + ".studio-versions.json");
}

bool StudioSession::diskRevisionUnchanged() const
{
    QFile disk(m_profilePath);
    return m_diskReadable && disk.exists() == m_diskExisted
        && (!m_diskExisted || (disk.size() <= kMaxDocumentBytes && disk.open(QIODevice::ReadOnly) && disk.readAll() == m_diskBaseline));
}

bool StudioSession::editingPermitted()
{
    if (m_active && !currentProfile()) {
        setError(tr("The profile changed. Reopen Page Studio.")); return false;
    }
    if (m_editorLocked || !ProfileManager::allows("edit")) {
        setError(tr("Editing is restricted. Unlock the editor or enable editing in Settings > Profiles.")); return false;
    }
    return true;
}

void StudioSession::setEditorLocked(bool locked)
{
    cancelPreview();
    m_editorLocked = locked;
    setError({});
    emit changed();
}

bool StudioSession::readVersions(QJsonArray &entries, QJsonObject *assets)
{
    entries = {};
    const auto path = m_profilePath + ".studio-versions.json";
    if (!profileStorageSafe()) {
        setError(tr("Versions must remain inside the active profile.")); return false;
    }
    QFile input(path);
    if (!input.exists()) return true;
    if (!input.open(QIODevice::ReadOnly) || input.size() > ArchiveBytes) {
        setError(tr("The version archive cannot be read or exceeds its bounded storage budget. Nothing was replaced.")); return false;
    }
    QJsonParseError error;
    const auto object = QJsonDocument::fromJson(input.readAll(), &error).object();
    entries = object["versions"].toArray();
    if (error.error != QJsonParseError::NoError || object.size() != 3 || object["schemaVersion"].toInt() != 1
        || !object["assets"].isObject() || !object["versions"].isArray() || entries.size() > VersionCount) {
        setError(tr("Unsupported or damaged version archive. Nothing was replaced.")); return false;
    }
    const auto storedAssets = object["assets"].toObject();
    if (storedAssets.size() > 128) { setError(tr("The version artwork budget is 128 images.")); return false; }
    qint64 decodedBytes = 0;
    for (auto it = storedAssets.begin(); it != storedAssets.end(); ++it) {
        if (!HomeDesignData::safeArtwork(it.key()) || !it.value().isString()) { setError(tr("Invalid version artwork.")); return false; }
        const auto bytes = QByteArray::fromBase64Encoding(it.value().toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!bytes || bytes.decoded.isEmpty() || bytes.decoded.size() > 20 * 1024 * 1024) { setError(tr("Invalid version image.")); return false; }
        decodedBytes += bytes.decoded.size();
        if (decodedBytes > AssetBytes) { setError(tr("The version artwork budget is 64 MiB.")); return false; }
    }
    if (assets) *assets = storedAssets;
    QSet<QString> ids;
    qint64 layoutBytes = 0;
    for (const auto &value : entries) {
        const auto entry = value.toObject();
        const auto id = entry["id"].toString();
        if (entry.size() != 4 || QUuid(id).isNull() || ids.contains(id) || !entry["document"].isObject()
            || entry["name"].toString().trimmed().isEmpty() || entry["name"].toString().size() > 80
            || !entry["created"].isString() || QJsonDocument(entry["document"].toObject()).toJson().size() > kMaxDocumentBytes) {
            setError(tr("A saved version is invalid. Nothing was replaced.")); return false;
        }
        layoutBytes += QJsonDocument(entry["document"].toObject()).toJson(QJsonDocument::Compact).size();
        if (layoutBytes > VersionBytes) { setError(tr("Version layouts exceed 40 MiB.")); return false; }
        ids.insert(id);
    }
    return true;
}

QVariantList StudioSession::versions()
{
    QJsonArray entries;
    if (!readVersions(entries)) return {};
    QVariantList result;
    for (const auto &value : entries) {
        auto entry = value.toObject(); entry.remove("document"); result.append(entry.toVariantMap());
    }
    return result;
}

bool StudioSession::archiveVersion(const QString &name, const QByteArray &document)
{
    QJsonArray entries;
    QJsonObject assets;
    if (!readVersions(entries, &assets)) return false;
    auto object = QJsonDocument::fromJson(document).object();
    if (object.isEmpty() || document.size() > kMaxDocumentBytes) { setError(tr("The version document is invalid.")); return false; }
    bool valid = true;
    object = HomeDesignData::artwork(object, [&](const QString &relative) {
        if (!HomeDesignData::safeArtwork(relative)) { valid = false; return relative; }
        const auto source = m_stagedAssets.contains(relative) ? m_stagedAssets.value(relative)
            : QUrl(m_live->managedArtworkSource(relative)).toLocalFile();
        QFile input(source);
        // Missing artwork stays an honest repairable placeholder, never an invented image.
        if (!QFileInfo::exists(source)) return relative;
        if (QFileInfo(source).isSymLink() || !input.open(QIODevice::ReadOnly) || input.size() > 20 * 1024 * 1024) { valid = false; return relative; }
        const auto bytes = input.readAll();
        const auto key = "artwork/studio-" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())
            + (ManagedGif::matches(bytes) ? ".gif" : relative.endsWith(".png") ? ".png" : ".jpg");
        assets[key] = QString::fromLatin1(bytes.toBase64());
        return key;
    }).toObject();
    if (!valid) { setError(tr("Could not preserve version artwork. Your draft is unchanged.")); return false; }
    if (!entries.isEmpty() && entries.first().toObject()["name"] == name && entries.first().toObject()["document"].toObject() == object) return true;
    entries.prepend(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"name", name},
        {"created", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"document", object}});
    while (entries.size() > VersionCount) entries.removeLast();
    return writeVersions(entries, assets);
}

bool StudioSession::writeVersions(const QJsonArray &entries, QJsonObject assets)
{
    qint64 layoutBytes = 0;
    QSet<QString> referenced;
    for (const auto &value : entries) {
        const auto layout = value.toObject()["document"].toObject();
        layoutBytes += QJsonDocument(layout).toJson(QJsonDocument::Compact).size();
        HomeDesignData::artwork(layout, [&](const QString &path) { referenced.insert(path); return path; });
    }
    qint64 assetBytes = 0;
    for (auto it = assets.begin(); it != assets.end();) {
        if (!referenced.contains(it.key())) it = assets.erase(it);
        else { assetBytes += QByteArray::fromBase64(it.value().toString().toLatin1()).size(); ++it; }
    }
    if (layoutBytes > VersionBytes || assets.size() > 128 || assetBytes > AssetBytes) {
        setError(tr("Versions are limited to 40 MiB of layouts and 128 images / 64 MiB. Apply or save a smaller artwork set.")); return false;
    }
    const auto bytes = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"versions", entries}, {"assets", assets}}).toJson(QJsonDocument::Compact);
    if (!QDir().mkpath(QFileInfo(m_profilePath).absolutePath())) {
        setError(tr("Could not create this profile's version directory.")); return false;
    }
    QSaveFile output(m_profilePath + ".studio-versions.json");
    if (bytes.size() > ArchiveBytes || !output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        setError(tr("Could not save the version archive. Your draft and applied design are retained.")); return false;
    }
    setError({}); return true;
}

bool StudioSession::saveVersion(const QString &name)
{
    if (!editingPermitted()) return false;
    if (name.trimmed().isEmpty() || name.size() > 80 || name.contains(QChar::Null)) { setError(tr("Name the version using 1–80 characters.")); return false; }
    return archiveVersion(name.trimmed(), m_draft);
}

bool StudioSession::deleteVersion(const QString &id)
{
    if (!editingPermitted()) return false;
    QJsonArray entries;
    QJsonObject assets;
    if (!readVersions(entries, &assets)) return false;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        if (entries[i].toObject()["id"] != id) continue;
        entries.removeAt(i);
        return writeVersions(entries, assets);
    }
    setError(tr("This version no longer exists. Refresh the list.")); return false;
}

bool StudioSession::resetVersions()
{
    if (!editingPermitted() || !currentProfile()) return false;
    if (!profileFile(m_profilePath + ".studio-versions.json")) {
        setError(tr("Version history must remain inside this profile.")); return false;
    }
    // Explicit destructive recovery also works for malformed archives; applied data is never reset here.
    return writeVersions({}, {});
}

bool StudioSession::previewDefaults(const QString &scope, const QString &pageId)
{
    cancelPreview();
    if (!editingPermitted() || !currentProfile()) return false;
    if (scope == "page") return previewCommand("restorePageDefaults", {}, pageId);
    if (scope != "interface") { setError(tr("Choose Current page or Whole interface.")); return false; }
    QFile defaults(m_live->m_defaultPath);
    if (!defaults.open(QIODevice::ReadOnly) || defaults.size() > kMaxDocumentBytes) {
        setError(tr("Packaged defaults are unavailable. Your draft is retained.")); return false;
    }
    HomeLayout worker(m_live->registry()); configureWorker(worker);
    if (!worker.loadDocument(defaults.readAll(), false)) { setError(worker.errorString()); return false; }
    auto document = QJsonDocument::fromJson(worker.documentData()).object();
    QJsonObject assets;
    bool valid = true;
    qint64 total = 0;
    HomeDesignData::artwork(document, [&](const QString &relative) {
        if (!HomeDesignData::safeArtwork(relative)) { valid = false; return relative; }
        if (assets.contains(relative)) return relative;
        QFile input(QDir(QFileInfo(m_live->m_defaultPath).absolutePath()).filePath(relative));
        if (!input.open(QIODevice::ReadOnly) || input.size() > 20 * 1024 * 1024) { valid = false; return relative; }
        const auto bytes = input.readAll(); total += bytes.size();
        if (assets.size() >= 128 || total > AssetBytes) { valid = false; return relative; }
        assets[relative] = QString::fromLatin1(bytes.toBase64()); return relative;
    });
    if (!valid) { setError(tr("Packaged default artwork is missing or exceeds the image budget.")); return false; }
    document = stageDesignArtwork(document, assets, &valid);
    return valid && proposeDocument(QJsonDocument(document).toJson());
}

bool StudioSession::proposeDocument(const QByteArray &bytes)
{
    if (!validDraft(bytes)) return false;
    if (!m_proposalModel) m_proposalModel = new HomeLayout(m_live->registry(), this);
    configureWorker(*m_proposalModel, true);
    if (!m_proposalModel->loadDocument(bytes, false)) { setError(m_proposalModel->errorString()); cancelPreview(); return false; }
    m_proposed = bytes; m_proposalBaseline = m_draft;
    emit m_proposalModel->layoutChanged(); setError({}); emit changed(); return true;
}

bool StudioSession::previewVersion(const QString &id, const QString &scope, const QString &pageId)
{
    cancelPreview();
    if (!editingPermitted()) return false;
    if (scope != "page" && scope != "interface") { setError(tr("Choose Current page or Whole interface.")); return false; }
    QJsonArray entries;
    QJsonObject assets;
    if (!readVersions(entries, &assets)) return false;
    QJsonObject archived;
    for (const auto &value : entries) if (value.toObject()["id"] == id) archived = value.toObject()["document"].toObject();
    if (archived.isEmpty()) { setError(tr("This version no longer exists. Refresh the list.")); return false; }
    auto replacement = scope == "page" ? pageDocument(archived, pageId) : archived;
    if (replacement.isEmpty()) { setError(tr("This page did not exist in that version. Choose Whole interface or another version.")); return false; }
    // Stage only the restored scope. Other pages retain their exact authored image paths and data.
    bool valid = false;
    replacement = stageDesignArtwork(replacement, assets, &valid);
    if (!valid) return false;
    auto candidate = replacement;
    if (scope == "page") {
        candidate = QJsonDocument::fromJson(m_draft).object();
        if (pageId.isEmpty()) {
            for (const auto &key : {"pages", "homePageId", "rootPage", "studioAppearance"}) if (candidate.contains(key)) replacement[key] = candidate[key];
            candidate = replacement;
        } else {
            auto pages = candidate["pages"].toArray(); bool found = false;
            for (qsizetype i = 0; i < pages.size(); ++i) {
                auto entry = pages[i].toObject();
                if (entry["id"] != pageId) continue;
                entry["layout"] = replacement; pages[i] = entry; found = true; break;
            }
            if (!found) { setError(tr("The current page no longer exists.")); return false; }
            candidate["pages"] = pages;
        }
    }
    // Unlike a template, a version restores its own interface/editor appearance.
    return proposeDocument(QJsonDocument(candidate).toJson());
}

QByteArray StudioSession::readAppliedDocument()
{
    if (!profileStorageSafe()) { setError(tr("Reopen the active profile editor using its actual local layout path.")); return {}; }
    QFile input(QFile::exists(m_profilePath) ? m_profilePath : m_live->m_defaultPath);
    if (!input.open(QIODevice::ReadOnly) || input.size() > kMaxDocumentBytes) {
        setError(tr("The saved design cannot be read. Your current draft is retained.")); return {};
    }
    const auto bytes = input.readAll();
    return validDraft(bytes) ? bytes : QByteArray();
}

bool StudioSession::previewSavedDesign()
{
    cancelPreview();
    const auto bytes = readAppliedDocument();
    if (bytes.isEmpty()) return false;
    return proposeDocument(bytes);
}

bool StudioSession::reloadApplied()
{
    if (!editingPermitted()) return false;
    const auto bytes = readAppliedDocument();
    if (bytes.isEmpty()) return false;
    auto *layout = m_live.data();
    if (!layout->loadDocument(bytes, QFile::exists(m_profilePath))) { setError(layout->errorString()); return false; }
    emit layout->layoutChanged();
    invalidate();
    return bind(layout);
}
