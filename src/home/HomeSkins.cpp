#include "HomeLayout.h"
#include "ComponentSchema.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
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
    return ComponentSchema::validComposition({{"schemaVersion", 1}, {"wallpaper", path}});
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
        assets[path] = QString::fromLatin1(file.readAll().toBase64());
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
bool HomeLayout::applySkinDocument(QJsonObject document, const QJsonObject &assets) {
    const auto previous = documentData();
    const auto legacy = m_legacyDocument;
    const bool singlePage=!m_owner && !document.contains("pages");
    const auto wrap=[&](QJsonObject page) {
        if(!singlePage)return page;
        if(m_homePageId.isEmpty()) {
            page["pages"]=m_pages;page["homePageId"]=m_homePageId;
            if(!m_rootPage.isEmpty())page["rootPage"]=m_rootPage;
            if(!m_studioAppearance.isEmpty())page["studioAppearance"]=QJsonObject::fromVariantMap(m_studioAppearance);
            return page;
        }
        page.remove("rootPage");page.remove("homePageId");page.remove("studioAppearance");
        HomeLayout converted(m_registry);converted.m_owner=this;
        if(!converted.loadDocument(QJsonDocument(page).toJson(),false))return QJsonObject{};
        page=QJsonDocument::fromJson(converted.documentData()).object();
        auto root=QJsonDocument::fromJson(previous).object();auto pages=root["pages"].toArray();
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
            || (!bytes.startsWith("\x89PNG\r\n\x1a\n") && !bytes.startsWith("\xff\xd8\xff"))) { valid = false; return path; }
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
    return applySkinDocument(skin["layout"].toObject(), skin["assets"].toObject());
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
