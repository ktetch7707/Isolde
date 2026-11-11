#include "StudioSession.h"
#include "ThemeColors.h"
#include "HomeLayout.h"
#include "HomeDesignData.h"
#include "ComponentSchema.h"
#include "ManagedGif.h"
#include "app/AppRegistry.h"
#include <QBuffer>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
namespace {
constexpr qint64 PackageLimit = 64 * 1024 * 1024;
QString localPath(const QString &value)
{
    const QUrl url(value);
    const auto path = url.isLocalFile() ? url.toLocalFile() : url.scheme().isEmpty() ? value : QString();
    return path.size() <= 4096 && !path.contains(QChar::Null) && QFileInfo(path).isAbsolute() ? QDir::cleanPath(path) : QString();
}
bool actualParent(const QString &path)
{
    const QFileInfo parent(QFileInfo(path).absolutePath());
    return !path.isEmpty() && parent.isDir() && parent.canonicalFilePath() == parent.absoluteFilePath() && !QFileInfo(path).isSymLink();
}
bool validManagedImage(const QString &relative, const QByteArray &data)
{
    const auto suffix = QFileInfo(relative).suffix();
    if (ManagedGif::matches(data)) return suffix == "gif" && !ManagedGif::decode(data, true).isEmpty();
    QBuffer buffer; buffer.setData(data); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer); const auto format = reader.format(), expected = suffix == "png" ? QByteArray("png") : QByteArray("jpeg");
    const auto size = reader.size();
    return suffix != "gif" && format == expected && size.isValid() && size.width() <= 16384 && size.height() <= 16384
        && qint64(size.width()) * size.height() <= 32 * 1024 * 1024 && !reader.read().isNull();
}
QJsonValue remapDesignReferences(QJsonValue value,const QHash<QString,QString> &styles,const QHash<QString,QString> &symbols)
{
    if(value.isArray()){auto array=value.toArray();for(qsizetype i=0;i<array.size();++i)array[i]=remapDesignReferences(array[i],styles,symbols);return array;}
    if(value.isObject()){auto object=value.toObject();for(auto it=object.begin();it!=object.end();++it){if(it.key()=="styleId" && styles.contains(it.value().toString()))it.value()=styles[it.value().toString()];else if(it.key()=="linkedDesign" && symbols.contains(it.value().toString()))it.value()=symbols[it.value().toString()];else it.value()=remapDesignReferences(it.value(),styles,symbols);}return object;}
    return value;
}
QJsonObject mergeDesignLibrary(QJsonObject &incoming,const QJsonObject &existing)
{
    const auto source=incoming["composition"].toObject()["designLibrary"].toObject();
    if(source.isEmpty())return existing;
    auto merged=existing.isEmpty()?QJsonObject{{"schemaVersion",1},{"styles",QJsonArray{}},{"components",QJsonArray{}}}:existing;
    QHash<QString,QString> styles,symbols;
    for(const auto &kind:{"styles","components"}) {
        auto entries=merged[kind].toArray();auto &mapping=QString(kind)=="styles"?styles:symbols;
        for(const auto &value:source[kind].toArray()) {
            auto entry=value.toObject();const auto original=entry["id"].toString();QString id=original;QSet<QString> ids;bool identical=false;
            for(const auto &old:entries){ids.insert(old.toObject()["id"].toString());if(old==value)identical=true;}
            if(identical)continue;
            for(int suffix=1;ids.contains(id);++suffix)id=original.left(140)+QString("-%1").arg(suffix);
            mapping[original]=id;entry["id"]=id;entries.append(entry);
        }
        merged[kind]=entries;
    }
    // References in incoming definitions and instances move together; existing definitions never change.
    auto components=merged["components"].toArray();const int oldCount=existing["components"].toArray().size();
    for(qsizetype i=oldCount;i<components.size();++i)components[i]=remapDesignReferences(components[i],styles,symbols);
    merged["components"]=components;
    incoming=remapDesignReferences(incoming,styles,symbols).toObject();return merged;
}
QJsonObject selectedPage(QJsonObject document, const QString &id)
{
    if (!id.isEmpty()) {
        for (const auto &value : document["pages"].toArray())
            if (value.toObject()["id"] == id) return value.toObject()["layout"].toObject();
        return {};
    }
    for (const auto &key : {"pages", "homePageId", "rootPage", "studioAppearance"}) document.remove(key);
    return document;
}
bool portableValue(const QJsonValue &value, QString &error, int depth = 0)
{
    if (depth > 32) { error = QObject::tr("The design is nested too deeply."); return false; }
    if (value.isObject()) {
        const auto object = value.toObject();
        const QSet<QString> privateKeys{"password", "credentials", "cookies", "token", "secret", "grants", "accounts", "profilepath", "script", "exec", "command"};
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (privateKeys.contains(it.key().toLower()) || !portableValue(it.value(), error, depth + 1)) {
                if (error.isEmpty()) error = QObject::tr("Private or executable field '%1' is not portable.").arg(it.key());
                return false;
            }
        }
    } else if (value.isArray()) {
        for (const auto &entry : value.toArray()) if (!portableValue(entry, error, depth + 1)) return false;
    } else if (value.isString()) {
        const auto text = value.toString();
        const QUrl url(text);
        for (const auto &query : QUrlQuery(url).queryItems()) {
            if (QStringList{"token", "password", "secret", "apikey", "api_key", "access_token", "authorization"}.contains(query.first.toLower())) {
                error = QObject::tr("Credential-bearing URLs are not portable."); return false;
            }
        }
        if (QFileInfo(text).isAbsolute() || url.isLocalFile() || !url.userInfo().isEmpty()
            || text.startsWith("\\\\") || QRegularExpression("^[A-Za-z]:[/\\\\]").match(text).hasMatch()) {
            error = QObject::tr("A design contains a private path or credentialed URL. Replace its source before exporting or importing.");
            return false;
        }
    }
    return true;
}
bool portableLayout(const QJsonObject &document, QString &error, bool nestedPage = false)
{
    const QSet<QString> keys{"schemaVersion", "theme", "composition", "categories", "components", "pages", "homePageId", "rootPage"};
    if (document["schemaVersion"].toDouble(-1) != 2 || !document["components"].isArray()
        || document["components"].toArray().size() > 1024 || document["pages"].toArray().size() > 64
        || (nestedPage && (document.contains("pages") || document.contains("homePageId") || document.contains("rootPage")))
        || QJsonDocument(document).toJson(QJsonDocument::Compact).size() > StudioSession::kMaxDocumentBytes) {
        error = QObject::tr("Portable designs require layout version 2."); return false;
    }
    for (auto it = document.begin(); it != document.end(); ++it) if (!keys.contains(it.key())) {
        error = QObject::tr("Unknown portable layout field '%1'.").arg(it.key()); return false;
    }
    const QSet<QString> componentKeys{"schemaVersion", "id", "kind", "componentType", "widgetType", "family", "source", "action", "appId", "appCollection", "name", "categoryId", "parentId", "placement", "presentation", "accessibility", "hidden", "locked", "size", "opacity", "customLogo", "symbol", "accent", "showTitle", "showType", "requiredCapability", "linkedDesign", "designNode"};
    for (const auto &value : document["components"].toArray()) {
        const auto node = value.toObject();
        for (auto it = node.begin(); it != node.end(); ++it) if (!componentKeys.contains(it.key())) {
            error = QObject::tr("Unknown portable component field '%1'.").arg(it.key()); return false;
        }
        // Machine grants, local media identities and provider collections are not portable capabilities.
        const auto action = node["action"].toObject();
        const auto type = action["type"].toString();
        const auto provider = node["source"].toObject()["provider"].toString();
        if (QStringList{"open-file", "open-folder", "open-disc", "play-movie", "play-episode", "open-show", "open-collection", "play-music", "play-game"}.contains(type)
            || node["source"].toObject().contains("collectionId") || provider == "games") {
            error = QObject::tr("'%1' uses a machine-specific media or game source. Choose a portable app, page or library role first.").arg(node["name"].toString()); return false;
        }
    }
    for(const auto &entry:document["composition"].toObject()["designLibrary"].toObject()["components"].toArray()) {
        const QJsonObject prototype{{"schemaVersion",2},{"components",entry.toObject()["nodes"]}};
        if(!portableLayout(prototype,error,true))return false;
    }
    for (const auto &value : document["pages"].toArray()) {
        const auto page = value.toObject();
        if (page.size() != 3 || !page["id"].isString() || !page["name"].isString() || !page["layout"].isObject() || !portableLayout(page["layout"].toObject(), error, true)) {
            if (error.isEmpty()) error = QObject::tr("Invalid portable page entry."); return false;
        }
    }
    return portableValue(document, error);
}
QJsonValue remapApps(QJsonValue value, const QVariantMap &mapping)
{
    if (value.isObject()) {
        auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == "appId" && mapping.contains(it.value().toString())) it.value() = mapping[it.value().toString()].toString();
            else if (it.key() == "ids" && object.contains("mode")) {
                auto ids = it.value().toArray();
                for (qsizetype i = 0; i < ids.size(); ++i) if (mapping.contains(ids[i].toString())) ids[i] = mapping[ids[i].toString()].toString();
                it.value() = ids;
            } else it.value() = remapApps(it.value(), mapping);
        }
        return object;
    }
    if (value.isArray()) { auto array = value.toArray(); for (qsizetype i = 0; i < array.size(); ++i) array[i] = remapApps(array[i], mapping); return array; }
    return value;
}
}
QVariantList StudioSession::designChoices() const
{
    if(!currentProfile())return {};
    auto result=m_live->skins();
    for(auto &value:result){
        auto entry=value.toMap();const auto document=m_live->skinSnapshot(entry.value("id").toString()).value("layout").toObject();
        const auto theme=ThemeColors::resolve(m_live->appearance(),document.value("theme").toObject().toVariantMap());
        const auto base=m_live->appearance();
        const auto safeColor=[&](const QString &key){const QColor color(theme.value(key).toString());return color.isValid() && color.alpha()==255?color.name():base.value(key).toString();};
        const auto grid=document.value("composition").toObject().value("grid").toObject();
        const int columns=grid.value("columns").toInt(8),rows=grid.value("rows").toInt(5);
        const bool gridValid=ComponentSchema::validGrid(grid.toVariantMap());
        QVariantList blocks;
        for(const auto &node:document.value("components").toArray()){
            const auto placement=node.toObject().value("placement").toObject().toVariantMap();
            if(gridValid && blocks.size()<32 && ComponentSchema::validPlacement(placement,columns,ComponentSchema::MaxScrollableRows))blocks.append(placement);
        }
        entry["preview"]=QVariantMap{{"background",safeColor("backgroundStart")},{"surface",safeColor("surfaceColor")},{"text",safeColor("textColor")},{"focus",safeColor("focusColor")},{"columns",gridValid?columns:8},{"rows",gridValid?rows:5},{"blocks",blocks}};
        value=entry;
    }
    return result;
}
QVariantMap StudioSession::previewDesign(const QString &id, const QString &scope, const QString &pageId)
{
    cancelPreview();
    if (!currentProfile()) { setError(tr("Reopen Studio before choosing a design.")); return {}; }
    const auto snapshot = m_live->skinSnapshot(id);
    if (snapshot["schemaVersion"].toDouble(-1) != 1 || !snapshot["layout"].isObject() || !snapshot["assets"].isObject()) {
        setError(tr("This saved design is missing or invalid.")); return {};
    }
    return proposeDesign(snapshot["layout"].toObject(), snapshot["assets"].toObject(), scope, pageId);
}
QJsonObject StudioSession::stageDesignArtwork(QJsonObject document, const QJsonObject &assets, bool *accepted)
{
    bool valid = true;
    document = HomeDesignData::artwork(document, [&](const QString &path) {
        if (!HomeDesignData::safeArtwork(path) || !assets[path].isString()) { valid = false; return path; }
        const auto decoded = QByteArray::fromBase64Encoding(assets[path].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded || decoded.decoded.isEmpty() || decoded.decoded.size() > 20 * 1024 * 1024) { valid = false; return path; }
        if (!m_assetStage) m_assetStage = new QTemporaryDir;
        if (!m_assetStage->isValid()) { valid = false; return path; }
        const auto rawPath = QDir(m_assetStage->path()).filePath("incoming-" + QFileInfo(path).fileName());
        QSaveFile file(rawPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(decoded.decoded) != decoded.decoded.size() || !file.commit()) { valid = false; return path; }
        const auto relative = stageArtwork(rawPath);
        QFile::remove(rawPath);
        if (relative.isEmpty()) valid = false;
        return relative.isEmpty() ? path : relative;
    }).toObject();
    *accepted = valid;
    if (!valid) setError(tr("The design contains missing or invalid managed artwork. Nothing applied."));
    return document;
}

QVariantMap StudioSession::proposeDesign(QJsonObject document, const QJsonObject &assets, const QString &scope, const QString &pageId)
{
    cancelPreview();
    if (!editingPermitted()) return {};
    if (!currentProfile() || (scope != "page" && scope != "interface") || (m_live->isPage() && scope == "interface")) {
        setError(tr("Choose Current page or Whole interface in an active profile.")); return {};
    }
    HomeLayout normalized(m_live->registry()); configureWorker(normalized);
    if (!normalized.loadDocument(QJsonDocument(document).toJson(), false)) { setError(normalized.errorString()); return {}; }
    document = QJsonDocument::fromJson(normalized.documentData()).object();
    document.remove("studioAppearance");
    if (scope == "page") {
        const QJsonValue library=document["composition"].toObject().value("designLibrary");
        document=selectedPage(document,document["homePageId"].toString());
        if(!library.isUndefined()){auto composition=document["composition"].toObject();composition["designLibrary"]=library;document["composition"]=composition;}
    }
    if (document.isEmpty()) { setError(tr("The design has no usable Home page.")); return {}; }
    // Stage only images referenced by the chosen scope; abandoned previews never import applied assets.
    bool valid = false;
    document = stageDesignArtwork(document, assets, &valid);
    if (!valid) return {};
    auto candidate = QJsonDocument::fromJson(m_draft).object();
    if (scope == "page") {
        const auto library=mergeDesignLibrary(document,candidate["composition"].toObject()["designLibrary"].toObject());
        auto composition=document["composition"].toObject();composition.remove("designLibrary");document["composition"]=composition;
        if(!library.isEmpty()){auto rootComposition=candidate["composition"].toObject();rootComposition["designLibrary"]=library;candidate["composition"]=rootComposition;}
        if (pageId.isEmpty()) {
            if(!library.isEmpty()){composition["designLibrary"]=library;document["composition"]=composition;}
            for (const auto &key : {"pages", "homePageId", "rootPage", "studioAppearance"}) if (candidate.contains(key)) document[key] = candidate[key];
            candidate = document;
        } else {
            auto pages = candidate["pages"].toArray(); bool found = false;
            for (qsizetype i = 0; i < pages.size(); ++i) { auto entry = pages[i].toObject(); if (entry["id"] != pageId) continue; entry["layout"] = document; pages[i] = entry; found = true; break; }
            if (!found) { setError(tr("The destination page no longer exists.")); return {}; }
            candidate["pages"] = pages;
        }
    } else {
        if (candidate.contains("studioAppearance")) document["studioAppearance"] = candidate["studioAppearance"];
        candidate = document;
    }
    const auto bytes = QJsonDocument(candidate).toJson();
    if (!validDraft(bytes)) return {};
    QSet<QString> destinations;
    for (const auto &entry : candidate["pages"].toArray()) destinations.insert(entry.toObject()["id"].toString());
    bool linksValid = true;
    std::function<void(const QJsonValue &)> checkLinks = [&](const QJsonValue &value) {
        if (value.isArray()) for (const auto &entry : value.toArray()) checkLinks(entry);
        else if (value.isObject()) {
            const auto object = value.toObject();
            if (object["type"] == "open-page" && !destinations.contains(object["pageId"].toString())) linksValid = false;
            if (object["type"] == "root-page" && candidate["rootPage"].toObject().contains("visible") && !candidate["rootPage"].toObject()["visible"].toBool()) linksValid = false;
            for (auto it = object.begin(); it != object.end(); ++it) checkLinks(it.value());
        }
    };
    checkLinks(candidate);
    if (!linksValid) { setError(tr("This scope leaves a missing page destination. Import the whole interface or repair its page links first.")); return {}; }
    const auto baseline = QJsonDocument::fromJson(m_draft).object();
    QVariantList changes;
    if (selectedPage(baseline, {}) != selectedPage(candidate, {}) || baseline["rootPage"] != candidate["rootPage"]) changes.append(QVariantMap{{"name", m_model->rootPageName()}, {"change", "replace"}});
    QSet<QString> seen;
    for (const auto &entry : baseline["pages"].toArray()) {
        const auto page = entry.toObject(); const auto id = page["id"].toString(); seen.insert(id);
        const auto next = selectedPage(candidate, id);
        if (next != page["layout"].toObject()) changes.append(QVariantMap{{"name", page["name"].toString()}, {"change", next.isEmpty() ? "remove" : "replace"}});
    }
    for (const auto &entry : candidate["pages"].toArray()) { const auto page = entry.toObject(); if (!seen.contains(page["id"].toString())) changes.append(QVariantMap{{"name", page["name"].toString()}, {"change", "add"}}); }
    if (baseline["homePageId"] != candidate["homePageId"]) changes.append(QVariantMap{{"name", tr("Startup page")}, {"change", "replace"}});
    m_proposed = bytes; m_proposalBaseline = m_draft;
    m_proposalModel = new HomeLayout(m_live->registry(), this); configureWorker(*m_proposalModel, true);
    if (!m_proposalModel->loadDocument(bytes, false)) { cancelPreview(); setError(tr("Could not render the proposed design.")); return {}; }
    setError({}); emit changed();
    return {{"valid", true}, {"scope", scope}, {"changes", changes}};
}
QVariantMap StudioSession::previewImport(const QString &value, const QString &scope, const QString &pageId, const QVariantMap &appRemap)
{
    cancelPreview();
    if (!editingPermitted()) return {};
    if (!currentProfile()) { setError(tr("Reopen Studio before importing.")); return {}; }
    const auto path = localPath(value); QFile input(path);
    if (!actualParent(path) || !QFileInfo(path).isFile() || !input.open(QIODevice::ReadOnly) || input.size() > PackageLimit) { setError(tr("Choose a regular local design file of at most 64 MiB.")); return {}; }
    QJsonParseError parseError; const auto json = QJsonDocument::fromJson(input.readAll(), &parseError);
    const auto package = json.object();
    if (parseError.error != QJsonParseError::NoError || package.size() != 4 || package["schemaVersion"].toDouble(-1) != 1
        || !QStringList{"page", "interface"}.contains(package["kind"].toString()) || !package["layout"].isObject() || !package["assets"].isObject()
        || (scope == "interface" && package["kind"] != "interface")) { setError(tr("Unknown portable design version, fields or scope.")); return {}; }
    QString error; auto document = package["layout"].toObject();
    if (package["kind"] == "page" && (document.contains("pages") || document.contains("homePageId") || document.contains("rootPage"))) { setError(tr("A portable page cannot contain an interface collection.")); return {}; }
    const auto assets = package["assets"].toObject();
    if (assets.size() > 128) { setError(tr("A portable design supports at most 128 managed images.")); return {}; }
    for (auto it = assets.begin(); it != assets.end(); ++it) if (!HomeDesignData::safeArtwork(it.key()) || !it.value().isString()) { setError(tr("Unknown or unsafe managed image entry.")); return {}; }
    if (!portableLayout(document, error)) { setError(error); return {}; }
    if (appRemap.size() > 128) { setError(tr("Remap at most 128 app identities.")); return {}; }
    for (auto it = appRemap.begin(); it != appRemap.end(); ++it) {
        if (!ComponentSchema::safeId(it.key()) || it.value().metaType().id() != QMetaType::QString || !m_live->registry()->find(it.value().toString())) { setError(tr("Remap apps to a registered app in this profile.")); return {}; }
    }
    document = remapApps(document, appRemap).toObject();
    auto result = proposeDesign(document, package["assets"].toObject(), scope, pageId);
    if (result.isEmpty()) return result;
    QStringList missing;
    std::function<void(const QJsonValue &)> inspect = [&](const QJsonValue &value) {
        if (value.isArray()) { for (const auto &entry : value.toArray()) inspect(entry); }
        else if (value.isObject()) { const auto object = value.toObject(); QStringList ids; if (object["appId"].isString()) ids.append(object["appId"].toString()); if (object.contains("mode")) for (const auto &id : object["ids"].toArray()) ids.append(id.toString()); for (const auto &appId : ids) if (!appId.isEmpty() && !m_live->registry()->find(appId) && !missing.contains(appId)) missing.append(appId); for (auto it = object.begin(); it != object.end(); ++it) inspect(it.value()); }
    };
    inspect(QJsonDocument::fromJson(m_proposed).object()); result["missingApps"] = missing;
    return result;
}
bool StudioSession::exportDesign(const QString &value, const QString &scope, const QString &pageId)
{
    if (!currentProfile() || (scope != "page" && scope != "interface") || (scope == "interface" && m_live->isPage())) { setError(tr("Choose Current page or Whole interface in an active profile.")); return false; }
    auto document = QJsonDocument::fromJson(m_draft).object();
    document.remove("studioAppearance");
    if (scope == "page") {
        const QJsonValue library=document["composition"].toObject().value("designLibrary");
        document=selectedPage(document,pageId);
        if(!library.isUndefined()){auto composition=document["composition"].toObject();composition["designLibrary"]=library;document["composition"]=composition;}
    }
    QString error;
    if (document.isEmpty() || !portableLayout(document, error)) { setError(error.isEmpty() ? tr("Choose an existing page.") : error); return false; }
    QJsonObject assets; bool valid = true; qint64 assetBytes = 0;
    HomeDesignData::artwork(document, [&](const QString &relative) {
        if (assets.contains(relative)) return relative;
        if (!HomeDesignData::safeArtwork(relative)) { valid = false; return relative; }
        const auto resolved = m_stagedAssets.contains(relative) ? m_stagedAssets[relative] : QDir(QFileInfo(m_profilePath).absolutePath()).filePath(relative);
        const QFileInfo info(resolved); QFile input(resolved);
        if (!actualParent(resolved) || !info.isFile() || info.isSymLink() || !input.open(QIODevice::ReadOnly) || input.size() > 20 * 1024 * 1024) { valid = false; return relative; }
        const auto bytes = input.readAll();
        assetBytes += bytes.size();
        if (assets.size() >= 128 || assetBytes * 4 / 3 + QJsonDocument(document).toJson().size() > PackageLimit || !validManagedImage(relative, bytes)) { valid = false; return relative; }
        assets[relative] = QString::fromLatin1(bytes.toBase64()); return relative;
    });
    if (!valid) { setError(tr("A managed image is unavailable. Choose it again before exporting.")); return false; }
    const auto data = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"kind", scope}, {"layout", document}, {"assets", assets}}).toJson();
    const auto path = localPath(value);
    if (data.size() > PackageLimit || !actualParent(path) || QFileInfo(path).suffix().toLower() != "json" || QFileInfo::exists(path)) { setError(tr("Choose a new .json filename in an existing local folder. Exports never overwrite an existing file and are limited to 64 MiB.")); return false; }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) { setError(tr("Could not save the portable design. Your draft is unchanged.")); return false; }
    setError({}); return true;
}
QVariantMap StudioSession::browseDesignFiles(const QString &directory, int page) const
{
    if (!m_active || page < 0 || page > 64) return {};
    auto path = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : localPath(directory);
    if (directory.isEmpty() && !QFileInfo(path).isDir()) path = QDir::homePath();
    const QFileInfo folder(path);
    if (!folder.isDir() || folder.isSymLink() || folder.canonicalFilePath() != folder.absoluteFilePath()) return {};
    QVariantList entries; QDirIterator files(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks); int inspected = 0;
    while (files.hasNext()) { files.next(); if (++inspected > 512) return {}; const auto info = files.fileInfo(); if (info.isDir() || (info.suffix().toLower() == "json" && info.size() <= PackageLimit)) entries.append(QVariantMap{{"name", info.fileName()}, {"path", info.absoluteFilePath()}, {"folder", info.isDir()}}); }
    std::sort(entries.begin(), entries.end(), [](const QVariant &a, const QVariant &b) { const auto left = a.toMap(), right = b.toMap(); if (left["folder"] != right["folder"]) return left["folder"].toBool(); return left["name"].toString() < right["name"].toString(); });
    return {{"directory", path}, {"parent", QDir::cleanPath(path + "/..")}, {"entries", entries.mid(page * 64, 64)}, {"page", page}, {"pages", qMax(1, int((entries.size() + 63) / 64))}};
}
bool StudioSession::setReferenceImage(const QString &value)
{
    if (!editingPermitted()) return false;
    if (!m_active) { setError(tr("Reopen Studio before choosing a reference.")); return false; }
    const auto path = localPath(value); QFile file(path);
    if (!actualParent(path) || !QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) { setError(tr("Choose a local PNG/JPEG reference of at most 8 MiB.")); return false; }
    const auto data = file.readAll(); QBuffer buffer; buffer.setData(data); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer); const auto size = reader.size();
    if (!QList<QByteArray>{"png", "jpeg", "jpg"}.contains(reader.format()) || !size.isValid() || qint64(size.width()) * size.height() > 16 * 1024 * 1024 || reader.read().isNull()) { setError(tr("Choose a valid PNG/JPEG reference of at most 16 megapixels.")); return false; }
    auto *stage = new QTemporaryDir;
    const auto target = QDir(stage->path()).filePath("reference." + QString::fromLatin1(reader.format())); QSaveFile output(target);
    if (!stage->isValid() || !output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) { delete stage; setError(tr("Could not stage the private reference.")); return false; }
    delete m_referenceStage; m_referenceStage = stage; m_referenceSource = QUrl::fromLocalFile(target).toString(); setError({}); emit changed(); return true;
}
void StudioSession::clearReferenceImage()
{
    delete m_referenceStage; m_referenceStage = nullptr; m_referenceSource.clear(); emit changed();
}
