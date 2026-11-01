#include "HomeLayout.h"
#include "ComponentSchema.h"
#include "ManagedGif.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QUuid>
#include <functional>

namespace {
constexpr qint64 limit = 64 * 1024 * 1024;
QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool writeObject(const QString &path, const QJsonObject &object) {
    const auto data = QJsonDocument(object).toJson();
    if (data.size() > limit) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
bool validName(const QString &name) {
    return !name.trimmed().isEmpty() && name.trimmed().size() <= 80 && !name.contains(QChar::Null);
}
// Only managed artwork paths are traversed. Skins never read arbitrary paths or URLs.
QJsonValue artwork(QJsonValue value, const std::function<QString(const QString &)> &visit) {
    if (value.isObject()) {
        auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if ((it.key() == "wallpaper" || it.key() == "customLogo") && it.value().isString() && !it.value().toString().isEmpty()) it.value() = visit(it.value().toString());
            else it.value() = artwork(it.value(), visit);
        }
        return object;
    }
    if (value.isArray()) {
        auto array = value.toArray();
        for (qsizetype i = 0; i < array.size(); ++i) array[i] = artwork(array[i], visit);
        return array;
    }
    return value;
}
bool safeArtwork(const QString &path) {
    return QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg|gif)$").match(path).hasMatch();
}
}

QString HomeLayout::skinDirectory() const {
    return QDir(QFileInfo(m_userPath).absolutePath()).filePath(m_owner ? "page-skins/"+m_pageId : "skins");
}
QVariantList HomeLayout::skins() const {
    QVariantList result{
        QVariantMap{{"id", "builtin-default"}, {"name", tr("Isolde default")}, {"builtIn", true}, {"description", tr("The original Home layout")}},
        QVariantMap{{"id", "builtin-lounge"}, {"name", tr("Cinema lounge")}, {"builtIn", true}, {"description", tr("Dark reference design · sample artwork and cards")}},
        QVariantMap{{"id", "builtin-pastel"}, {"name", tr("Pastel sky")}, {"builtIn", true}, {"description", tr("Cloud wallpaper, glass panels, installed apps and Kodi viewing history")}}
    };
    const QDir directory(skinDirectory());
    for (const auto &file : directory.entryList({"*.json"}, QDir::Files, QDir::Name)) {
        const auto id = QFileInfo(file).completeBaseName();
        const auto object = readObject(directory.filePath(file));
        if (!id.startsWith("user-") || !ComponentSchema::safeId(id) || object["schemaVersion"].toInt() != 1 || !validName(object["name"].toString())) continue;
        result.append(QVariantMap{{"id", id}, {"name", object["name"].toString()}, {"builtIn", false}, {"description", tr("Your saved Home design")}});
    }
    return result;
}
QString HomeLayout::saveSkin(const QString &name) {
    if (!validName(name)) { setError(tr("Give your skin a name of 1–80 characters.")); return {}; }
    QJsonObject assets;
    bool valid = true;
    artwork(QJsonDocument::fromJson(documentData()).object(), [&](const QString &path) {
        if (assets.contains(path)) return path;
        QFile file(QDir(QFileInfo(m_userPath).absolutePath()).filePath(path));
        if (!safeArtwork(path) || !file.open(QIODevice::ReadOnly) || file.size() > 20 * 1024 * 1024) { valid = false; return path; }
        const auto bytes=file.readAll();
        if((QFileInfo(path).suffix()=="gif")!=ManagedGif::matches(bytes) || (ManagedGif::matches(bytes) && ManagedGif::decode(bytes,true).isEmpty())){valid=false;return path;}
        assets[path] = QString::fromLatin1(bytes.toBase64());
        return path;
    });
    if (!valid) { setError(tr("A Home image is missing or too large. Choose it again before saving a skin.")); return {}; }
    const auto id = "user-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(skinDirectory());
    if (!writeObject(QDir(skinDirectory()).filePath(id + ".json"), {{"schemaVersion", 1}, {"name", name.trimmed()},
            {"layout", QJsonDocument::fromJson(documentData()).object()}, {"assets", assets}})) {
        setError(tr("Unable to save skin. The current Home has not changed.")); return {};
    }
    setError({}); emit skinsChanged(); return id;
}
bool HomeLayout::applySkinDocument(QJsonObject document, const QJsonObject &assets, bool addMissingPages) {
    const auto previous = documentData();
    const auto legacy = m_legacyDocument;
    QJsonArray mergedPages=m_pages;
    if(addMissingPages) {
        if(m_owner){setError(tr("Apply this page-set skin from Home to add its library pages."));return false;}
        HomeLayout validator(m_registry);
        if(!validator.loadDocument(QJsonDocument(document).toJson(),false)){setError(validator.errorString());return false;}
        for(const auto &entry:document["pages"].toArray()) {
            bool exists=false;for(const auto &saved:mergedPages)if(saved.toObject()["id"]==entry.toObject()["id"])exists=true;
            if(!exists)mergedPages.append(entry);
        }
        document.remove("pages");
    }
    const bool singlePage=!m_owner && !document.contains("pages");
    const auto wrap=[&](QJsonObject page) {
        if(!singlePage)return page;
        if(m_homePageId.isEmpty()) {
            page["pages"]=mergedPages;page["homePageId"]=m_homePageId;
            if(!m_rootPage.isEmpty())page["rootPage"]=m_rootPage;
            if(!m_studioAppearance.isEmpty())page["studioAppearance"]=QJsonObject::fromVariantMap(m_studioAppearance);
            return page;
        }
        page.remove("rootPage");page.remove("homePageId");page.remove("studioAppearance");
        HomeLayout converted(m_registry);converted.m_owner=this;
        if(!converted.loadDocument(QJsonDocument(page).toJson(),false))return QJsonObject{};
        page=QJsonDocument::fromJson(converted.documentData()).object();
        auto root=QJsonDocument::fromJson(previous).object();auto pages=mergedPages;
        for(int i=0;i<pages.size();++i)if(pages[i].toObject()["id"]==m_homePageId){auto entry=pages[i].toObject();entry["layout"]=page;pages[i]=entry;}
        root["pages"]=pages;return root;
    };
    // Validate the entire document before importing artwork or replacing the current layout.
    if (!loadDocument(QJsonDocument(wrap(document)).toJson(), false)) return false;
    loadDocument(previous, false); m_legacyDocument = legacy;
    bool valid = true;
    document = artwork(document, [&](const QString &path) {
        if (!safeArtwork(path) || !assets[path].isString()) { valid = false; return path; }
        const auto decoded = QByteArray::fromBase64Encoding(assets[path].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        const auto bytes = decoded.decoded;
        if (!decoded || bytes.isEmpty() || bytes.size() > 20 * 1024 * 1024
            || (!bytes.startsWith("\x89PNG\r\n\x1a\n") && !bytes.startsWith("\xff\xd8\xff") && !ManagedGif::matches(bytes))) { valid = false; return path; }
        if((QFileInfo(path).suffix()=="gif")!=ManagedGif::matches(bytes) || (ManagedGif::matches(bytes) && ManagedGif::decode(bytes,true).isEmpty())){valid=false;return path;}
        const auto relative = "artwork/skin-" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + "." + QFileInfo(path).suffix();
        QDir base(QFileInfo(m_userPath).absolutePath()); base.mkpath("artwork");
        QSaveFile file(base.filePath(relative));
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) valid = false;
        return relative;
    }).toObject();
    if (!valid) { setError(tr("This skin has missing or invalid artwork. Your Home has not changed.")); return false; }
    document=wrap(document);
    if (!loadDocument(QJsonDocument(document).toJson(), false) || !save()) {
        const auto error = m_errorString; loadDocument(previous, false); m_legacyDocument = legacy; setError(error); return false;
    }
    emit layoutChanged(); return true;
}
bool HomeLayout::applySkin(const QString &id) {
    if (id == "builtin-default") {
        QFile file(m_defaultPath);
        if (!file.open(QIODevice::ReadOnly)) { setError(tr("Unable to read the default skin.")); return false; }
        return applySkinDocument(QJsonDocument::fromJson(file.readAll()).object(), {});
    }
    QJsonObject skin;
    if (id == "builtin-lounge" || id == "builtin-pastel") {
        const auto relative = "skins/" + id.mid(8) + ".json";
        skin = readObject(QDir(QFileInfo(m_defaultPath).absolutePath()).filePath(relative));
        if (skin.isEmpty()) skin = readObject(":/config/" + relative);
    } else if (id.startsWith("user-") && ComponentSchema::safeId(id)) skin = readObject(QDir(skinDirectory()).filePath(id + ".json"));
    if (skin["schemaVersion"].toInt() != 1 || !skin["layout"].isObject() || !skin["assets"].isObject()) {
        setError(tr("This skin is missing or invalid.")); return false;
    }
    if(skin.contains("pagePolicy") && skin["pagePolicy"]!="add-missing") {setError(tr("Unsupported skin page policy."));return false;}
    return applySkinDocument(skin["layout"].toObject(), skin["assets"].toObject(),skin["pagePolicy"]=="add-missing");
}
QVariantList HomeLayout::skinPageUpdates(const QString &id) const {
    if (m_owner || (id != "builtin-pastel" && id != "builtin-lounge")) return {};
    const auto relative = "skins/" + id.mid(8) + ".json";
    auto skin = readObject(QDir(QFileInfo(m_defaultPath).absolutePath()).filePath(relative));
    if (skin.isEmpty()) skin = readObject(":/config/" + relative);
    const auto recipes = skin["pageUpdates"].toArray();
    if (skin["schemaVersion"].toInt() != 1 || recipes.size() > MaxPages) return {};
    const auto document = QJsonDocument::fromJson(committedDocument()).object();
    auto home = document;
    if (!m_homePageId.isEmpty()) for (const auto &entry : document["pages"].toArray())
        if (entry.toObject()["id"] == m_homePageId) home = entry.toObject()["layout"].toObject();
    QVariantList result;
    for (const auto &value : recipes) {
        const auto recipe = value.toObject();
        const auto componentId = recipe["componentId"].toString();
        const auto pageId = recipe["targetPageId"].toString();
        if (recipe.size() != 3 || !ComponentSchema::safeId(componentId) || !ComponentSchema::safeId(pageId)) return {};
        QJsonObject target;
        for (const auto &entry : skin["layout"].toObject()["pages"].toArray())
            if (entry.toObject()["id"] == pageId) target = entry.toObject();
        if (target.isEmpty()) return {};
        // Updates contain only data and never import or replace managed artwork.
        bool hasArtwork = false;
        artwork(target, [&](const QString &path) { hasArtwork = true; return path; });
        if (hasArtwork) return {};
        QString actionError;
        const auto fromAction = ComponentSchema::action(recipe["fromAction"].toObject().toVariantMap(), &actionError);
        if (!actionError.isEmpty() || fromAction.isEmpty()) return {};
        for (const auto &entry : home["components"].toArray()) {
            const auto component = entry.toObject();
            if (component["id"] != componentId || component["action"].toObject().toVariantMap() != fromAction) continue;
            result.append(QVariantMap{{"componentId", componentId}, {"name", component["name"].toString()},
                {"pageId", pageId}, {"page", target.toVariantMap()}});
        }
    }
    return result;
}
bool HomeLayout::applySkinPageUpdates(const QString &id) {
    const auto updates = skinPageUpdates(id);
    if (updates.isEmpty()) { setError(tr("No unchanged starter links need updating.")); return false; }
    auto document = QJsonDocument::fromJson(committedDocument()).object();
    auto pages = document["pages"].toArray();
    auto home = document;
    int homeIndex = -1;
    if (!m_homePageId.isEmpty()) for (int i = 0; i < pages.size(); ++i)
        if (pages[i].toObject()["id"] == m_homePageId) { homeIndex = i; home = pages[i].toObject()["layout"].toObject(); }
    auto components = home["components"].toArray();
    for (const auto &value : updates) {
        const auto update = value.toMap();
        const auto pageId = update["pageId"].toString();
        bool exists = false;
        for (const auto &page : pages) if (page.toObject()["id"] == pageId) exists = true;
        if (!exists) pages.append(QJsonObject::fromVariantMap(update["page"].toMap()));
        for (int i = 0; i < components.size(); ++i) if (components[i].toObject()["id"].toString() == update["componentId"].toString()) {
            auto component = components[i].toObject();
            component["action"] = QJsonObject{{"type", "open-page"}, {"pageId", pageId}};
            if (component["componentType"].toString() != "button")
                component["source"] = QJsonObject{{"schemaVersion", 1}, {"kind", "page"}, {"provider", "shell"}, {"id", pageId}};
            components[i] = component;
        }
    }
    home["components"] = components;
    if (homeIndex >= 0) { auto entry = pages[homeIndex].toObject(); entry["layout"] = home; pages[homeIndex] = entry; }
    else document["components"] = components;
    document["pages"] = pages;
    return applyStudioDraft(QJsonDocument(document).toJson(), m_focusOrigin);
}
bool HomeLayout::renameSkin(const QString &id, const QString &name) {
    if (!id.startsWith("user-") || !ComponentSchema::safeId(id) || !validName(name)) { setError(tr("Choose a saved skin and a name of 1–80 characters.")); return false; }
    const auto path = QDir(skinDirectory()).filePath(id + ".json");
    auto skin = readObject(path);
    if (skin.isEmpty()) { setError(tr("Skin not found.")); return false; }
    skin["name"] = name.trimmed();
    if (!writeObject(path, skin)) { setError(tr("Unable to rename skin.")); return false; }
    setError({}); emit skinsChanged(); return true;
}
bool HomeLayout::deleteSkin(const QString &id) {
    if (!id.startsWith("user-") || !ComponentSchema::safeId(id) || !QFile::remove(QDir(skinDirectory()).filePath(id + ".json"))) {
        setError(tr("Unable to delete skin. Built-in skins are kept.")); return false;
    }
    setError({}); emit skinsChanged(); return true;
}
