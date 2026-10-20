#include "ComponentSchema.h"
#include "app/AppRegistry.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDir>
#include <QUrl>
#include <cmath>
#include <QColor>
#include <QSet>

namespace {
QVariantMap reject(QString *error, const QString &message) { if (error) *error = message; return {}; }
bool versionOne(const QVariantMap &map) {
    return !map.contains("schemaVersion") || map.value("schemaVersion").toDouble() == 1.0;
}
}
bool ComponentSchema::safeId(const QString &id)
{
    static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._-]{0,159}$");
    return id != "." && id != ".." && pattern.match(id).hasMatch();
}
bool ComponentSchema::validPresentation(const QVariantMap &value, bool section)
{
    for (auto it = value.cbegin(); it != value.cend(); ++it) {
        const auto key = it.key();
        if (key == "grid") {
            if (!section || it.value().metaType().id()!=QMetaType::QVariantMap || !validGrid(it.value().toMap())
                || it.value().toMap().value("mode","screen")!="screen") return false;
        } else if (key == "variant") {
            if (!QStringList{"banner", "media", "source", "summary", "launcher", "orb", "recent", "library", "text"}.contains(it.value().toString())) return false;
        } else if (key == "libraryKind" || key == "libraryView" || key == "libraryLayout" || key == "libraryValue" || key == "libraryColumns" || key == "libraryCardStyle") {
            if (!section) return false;
            if (key == "libraryKind" && !QStringList{"movies","shows"}.contains(it.value().toString())) return false;
            if (key == "libraryView" && !QStringList{"all","recent","continue","search","tag","genre","collection"}.contains(it.value().toString())) return false;
            if (key == "libraryLayout" && !QStringList{"grid","list","row"}.contains(it.value().toString())) return false;
            if (key == "libraryCardStyle" && !QStringList{"media","recent","orb","source","summary","banner"}.contains(it.value().toString())) return false;
            if (key == "libraryValue" && (it.value().metaType().id()!=QMetaType::QString || it.value().toString().size()>160)) return false;
            if (key == "libraryColumns" && (it.value().metaType().id()==QMetaType::QString || it.value().metaType().id()==QMetaType::Bool || it.value().toDouble()<1 || it.value().toDouble()>12 || std::floor(it.value().toDouble())!=it.value().toDouble())) return false;
        } else if (key == "subtitle" || key == "label" || key == "icon" || key == "trailingLabel") {
            if (it.value().metaType().id() != QMetaType::QString || it.value().toString().size() > 160) return false;
        } else if (QStringList{"surfaceColor", "panelColor", "headingColor", "iconColor", "labelColor", "selectedLabelColor"}.contains(key)) {
            const QColor color(it.value().toString()); if (!color.isValid() || color.alpha() != 255) return false;
        } else if (key == "showHeading" || key == "labelPill" || key == "headingBold") {
            if ((key == "showHeading" && !section) || it.value().metaType().id() != QMetaType::Bool) return false;
        } else if (key == "trailingAction") {
            if (!section || action(it.value().toMap()).isEmpty()) return false;
        } else if (key == "imageCrop") {
            const auto crop = it.value().toMap();
            if (crop.size() != 4 || !crop.contains("x") || !crop.contains("y") || !crop.contains("width") || !crop.contains("height")) return false;
            for (auto c = crop.cbegin(); c != crop.cend(); ++c) {
                bool ok; const double number = c.value().toDouble(&ok);
                if (!ok || !std::isfinite(number) || number < 0 || number > 16384 || ((c.key() == "width" || c.key() == "height") && number < 1)) return false;
            }
        } else if (QStringList{"columns", "span", "height", "progress", "contentWidth", "padding", "panelOpacity", "blur", "radius", "headingSize", "diameter", "labelSize", "iconSize", "surfaceOpacity", "cardGap", "artHeight"}.contains(key)) {
            if (it.value().metaType().id() == QMetaType::QString || it.value().metaType().id() == QMetaType::Bool) return false;
            bool ok; const double number = it.value().toDouble(&ok);
            if (!ok || !std::isfinite(number)) return false;
            if (key == "columns" && (!section || number < 1 || number > 12 || std::floor(number) != number)) return false;
            if (key == "span" && (!section || number < 1 || number > 2 || std::floor(number) != number)) return false;
            if (key == "height" && (!section || number < 60 || number > 800)) return false;
            if (key == "progress" && (section || number < 0 || number > 1)) return false;
            if (QStringList{"panelOpacity", "blur", "surfaceOpacity"}.contains(key) && (number < 0 || number > 1)) return false;
            if (QStringList{"padding", "radius", "headingSize", "diameter", "labelSize", "iconSize", "cardGap", "artHeight"}.contains(key) && (number < 0 || number > 400)) return false;
            if (key == "contentWidth" && (!section || number < 100 || number > 3840)) return false;
        } else return false;
    }
    return true;
}
bool ComponentSchema::validComposition(const QVariantMap &value)
{
    if (value.isEmpty()) return true;
    static const QSet<QString> keys{"schemaVersion", "enabled", "navigation", "initialTab", "margin", "gap", "fit", "showProfile", "columns", "wallpaper", "headerMode", "identity", "headerControls", "showClock", "designHeight", "pageNavigation", "sidebarWidth", "moviePage", "showsPage", "grid", "sectionHeight"};
    for (auto it = value.cbegin(); it != value.cend(); ++it) if (!keys.contains(it.key())) return false;
    if (value.contains("pageNavigation") && !QStringList{"left","top","none"}.contains(value["pageNavigation"].toString())) return false;
    if (value.contains("sidebarWidth") && (value["sidebarWidth"].metaType().id()==QMetaType::QString || value["sidebarWidth"].toDouble()<120 || value["sidebarWidth"].toDouble()>400)) return false;
    for (const auto &key : {"moviePage","showsPage"}) if(value.contains(key) && !safeId(value[key].toString())) return false;
    if (value.value("schemaVersion" ).toDouble() != 1) return false;
    if (value.contains("columns") && value["columns"].toDouble() != 1 && value["columns"].toDouble() != 2) return false;
    if (value.contains("grid") && (value["grid"].metaType().id() != QMetaType::QVariantMap || !validGrid(value["grid"].toMap()))) return false;
    if(value.value("grid").toMap().value("mode")=="sections"
        && (value.value("columns",1).toInt()!=1 || value.value("fit",false).toBool() || !value.contains("sectionHeight")))return false;
    if(value.contains("sectionHeight") && (value["sectionHeight"].metaType().id()==QMetaType::QString
        || value["sectionHeight"].metaType().id()==QMetaType::Bool || !std::isfinite(value["sectionHeight"].toDouble())
        || value["sectionHeight"].toDouble()<60 || value["sectionHeight"].toDouble()>800))return false;
    if (value.contains("wallpaper") && !QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$").match(value["wallpaper"].toString()).hasMatch()) return false;
    if (value.contains("headerMode") && !QStringList{"navigation", "identity"}.contains(value["headerMode"].toString())) return false;
    if (value.contains("identity") && (value["identity"].metaType().id() != QMetaType::QString || value["identity"].toString().size() > 80)) return false;
    if (value.contains("designHeight")) {
        bool ok; const double height = value["designHeight"].toDouble(&ok);
        if (!ok || !std::isfinite(height) || value["designHeight"].metaType().id() == QMetaType::QString
            || value["designHeight"].metaType().id() == QMetaType::Bool || height < 480 || height > 1440) return false;
    }
    if (value.contains("headerControls")) {
        const auto controls = value["headerControls"].toList(); QSet<QString> unique;
        if (controls.isEmpty() || controls.size() > 3 || value["headerControls"].metaType().id() != QMetaType::QVariantList) return false;
        for (const auto &control : controls) { const auto id = control.toString(); if (!QStringList{"search", "settings", "edit"}.contains(id) || unique.contains(id)) return false; unique.insert(id); }
        if (!unique.contains("settings") && !unique.contains("edit")) return false;
    }
    for (const auto &key : {"enabled", "fit", "showProfile", "showClock"})
        if (value.contains(key) && value[key].metaType().id() != QMetaType::Bool) return false;
    for (const auto &key : {"margin", "gap"}) if (value.contains(key)) {
        bool ok; const double number = value[key].toDouble(&ok);
        if (!ok || !std::isfinite(number) || number < 0 || number > 100) return false;
    }
    const auto tabs = value.value("navigation").toList();
    if (tabs.size() > 64 || (value.contains("navigation") && value["navigation"].metaType().id() != QMetaType::QVariantList)) return false;
    QSet<QString> ids;
    for (const auto &entry : tabs) {
        const auto tab = entry.toMap(); const auto id = tab.value("id").toString();
        if (!safeId(id) || ids.contains(id) || tab.value("name").toString().trimmed().isEmpty() || tab.value("name").toString().size() > 80) return false;
        ids.insert(id);
        for (auto it = tab.cbegin(); it != tab.cend(); ++it) if (!QStringList{"id", "name", "action", "sections"}.contains(it.key())) return false;
        if (tab.contains("action") && action(tab["action"].toMap()).isEmpty()) return false;
        if (tab.contains("sections")) {
            if (tab["sections"].metaType().id() != QMetaType::QVariantList) return false;
            for (const auto &category : tab["sections"].toList()) if (!safeId(category.toString())) return false;
        }
    }
    return !value.contains("initialTab") || ids.contains(value["initialTab"].toString());
}
bool ComponentSchema::validGrid(const QVariantMap &value)
{
    if (value.isEmpty()) return true;
    static const QSet<QString> keys{"columns", "rows", "gap", "margin", "snap", "overlap", "mode"};
    for (auto it = value.cbegin(); it != value.cend(); ++it) if (!keys.contains(it.key())) return false;
    if(value.contains("mode") && !QStringList{"screen","sections"}.contains(value["mode"].toString()))return false;
    if (value.contains("columns")) {
        bool ok; const double number = value.value("columns").toDouble(&ok);
        if (!ok || value.value("columns").metaType().id() == QMetaType::QString || value.value("columns").metaType().id() == QMetaType::Bool
            || number < 4 || number > 16 || std::floor(number) != number) return false;
    }
    if (value.contains("rows")) {
        bool ok; const double number = value.value("rows").toDouble(&ok);
        if (!ok || value.value("rows").metaType().id() == QMetaType::QString || value.value("rows").metaType().id() == QMetaType::Bool
            || number < 3 || number > 10 || std::floor(number) != number) return false;
    }
    for (const auto &key : {"gap", "margin"}) if (value.contains(key)) {
        bool ok; const double number = value.value(key).toDouble(&ok);
        if (!ok || !std::isfinite(number) || value.value(key).metaType().id() == QMetaType::QString
            || value.value(key).metaType().id() == QMetaType::Bool || number < 0 || number > 100) return false;
    }
    for (const auto &key : {"snap", "overlap"})
        if (value.contains(key) && value.value(key).metaType().id() != QMetaType::Bool) return false;
    return true;
}
QVariantMap ComponentSchema::normalizedGrid(const QVariantMap &value)
{
    QVariantMap grid{{"columns", 8}, {"rows", 5}, {"gap", 12}, {"margin", 20}, {"snap", true}, {"overlap", false}, {"mode","screen"}};
    for (auto it = value.cbegin(); it != value.cend(); ++it) grid[it.key()] = it.value();
    return grid;
}
bool ComponentSchema::validPlacement(const QVariantMap &value, int maxColumns, int maxRows)
{
    static const QSet<QString> keys{"x", "y", "width", "height"};
    if (value.size() != 4) return false;
    for (auto it = value.cbegin(); it != value.cend(); ++it) {
        if (!keys.contains(it.key())) return false;
        bool ok; const double number = it.value().toDouble(&ok);
        if (!ok || it.value().metaType().id() == QMetaType::QString || it.value().metaType().id() == QMetaType::Bool
            || !std::isfinite(number) || std::floor(number) != number) return false;
    }
    const int x = value.value("x").toInt(), y = value.value("y").toInt();
    const int width = value.value("width").toInt(), height = value.value("height").toInt();
    // Overflow is prevented: a placement always fits inside the declared grid.
    return x >= 0 && y >= 0 && width >= 1 && height >= 1
        && x + width <= maxColumns && y + height <= maxRows;
}
bool ComponentSchema::placementsOverlap(const QVariantMap &a, const QVariantMap &b)
{
    return a.value("x").toInt() < b.value("x").toInt() + b.value("width").toInt()
        && b.value("x").toInt() < a.value("x").toInt() + a.value("width").toInt()
        && a.value("y").toInt() < b.value("y").toInt() + b.value("height").toInt()
        && b.value("y").toInt() < a.value("y").toInt() + a.value("height").toInt();
}
QVariantMap ComponentSchema::action(const QVariantMap &input, QString *error)
{
    if (error) error->clear();
    if (!versionOne(input)) return reject(error, "Unsupported action version.");
    auto type = input.value("type").toString();
    // Read compatibility for the first generated Home shelves.
    if (type == "kodi-collection") type = input.value("movieId").toInt() > 0 ? "play-movie" : "open-collection";
    QVariantMap result{{"schemaVersion", 1}, {"type", type}};
    if (type == "launch-app") {
        const auto id = input.value("appId").toString();
        if (!safeId(id)) return reject(error, "Choose a valid app.");
        result["appId"] = id;
    } else if (type == "open-collection" || type == "play-movie" || type == "open-show") {
        const auto id = input.value("collectionId").toString();
        if (!safeId(id)) return reject(error, "Choose a valid collection.");
        result["collectionId"] = id;
        if (type == "play-movie") {
            const double movie = input.value("movieId").toDouble();
            if (!std::isfinite(movie) || movie < 1 || movie > 2147483647 || std::floor(movie) != movie)
                return reject(error, "Choose a valid movie.");
            result["movieId"] = static_cast<int>(movie);
        }
        if (type == "open-show") {
            const double show = input.value("tvShowId").toDouble();
            if (!std::isfinite(show) || show < 1 || show > 2147483647 || std::floor(show) != show)
                return reject(error, "Choose a valid TV show.");
            result["tvShowId"] = static_cast<int>(show);
        }
    } else if (type == "open-page") {
        const auto id=input.value("pageId").toString();
        if(!safeId(id))return reject(error,"Choose a valid page.");
        result["pageId"]=id;
    } else if (type == "app-drawer" || type == "root-page") {
        // Shell-owned drawer, never a command or provider URL.
    } else if (type == "settings") {
        const double section = input.value("section", -1).toDouble();
        if (section < -1 || section > 5 || std::floor(section) != section) return reject(error, "Invalid Settings section.");
        result["section"] = static_cast<int>(section);
    } else if (type == "switch-input") {
        const double inputNumber = input.value("input").toDouble();
        if (!std::isfinite(inputNumber) || inputNumber < 1 || inputNumber > 8 || std::floor(inputNumber) != inputNumber)
            return reject(error, "Choose an HDMI input from 1 to 8.");
        result["input"] = static_cast<int>(inputNumber);
    } else if (type == "desktop-mode") {
        result["label"] = input.value("label", "Desktop Mode").toString().left(80);
    } else if (QStringList{"open-folder", "open-file", "open-disc", "open-url"}.contains(type)) {
        const auto target = input.value("target").toString().trimmed();
        const auto handler = input.value("openWith").toString();
        if (target.isEmpty() || target.size() > 4096 || target.contains(QChar::Null)
            || (!handler.isEmpty() && !safeId(handler))) return reject(error, "Invalid target or Open With app.");
        if (type == "open-url") {
            const QUrl url(target, QUrl::StrictMode);
            if (!url.isValid() || !QStringList{"http", "https"}.contains(url.scheme()) || url.host().isEmpty()
                || !url.userInfo().isEmpty()) return reject(error, "Use an HTTP or HTTPS URL without embedded credentials.");
        } else if (!QDir::isAbsolutePath(target)) return reject(error, "Use an absolute media path.");
        result["target"] = target;
        result["openWith"] = handler.isEmpty() ? "system" : handler;
    } else return reject(error, "This action type is not supported.");
    return result;
}
QVariantMap ComponentSchema::component(const QVariantMap &input, bool legacy, QString *error)
{
    if (error) error->clear();
    if (!versionOne(input) || !safeId(input.value("id").toString())) return reject(error, "Invalid component identity or version.");
    auto result = input;
    const auto kind = input.value("kind").toString();
    auto type = input.value("componentType").toString();
    if (type.isEmpty() && legacy) type = kind == "widget" ? (input.value("widgetType")=="text"?"text":"clock") : "launcher";
    if (!QStringList{"launcher", "clock", "text", "media-shelf"}.contains(type)) return reject(error, "Unsupported component type.");
    if (!QStringList{"compact", "standard", "wide"}.contains(input.value("size", "standard").toString()))
        return reject(error, "Invalid component size.");
    if (input.contains("opacity")) {
        const auto opacity = input.value("opacity").toDouble();
        if (!std::isfinite(opacity) || opacity < 0 || opacity > 1) return reject(error, "Invalid component opacity.");
    }
    for(const auto &key:QStringList{"hidden","locked"})if(input.contains(key) && input.value(key).metaType().id()!=QMetaType::Bool)return reject(error,"Visibility and lock must be boolean values.");
    if (input.contains("placement")) {
        // Shape-only bounds here; the layout validates against its actual grid and neighbors.
        const auto placement = input.value("placement");
        if (placement.metaType().id() != QMetaType::QVariantMap
            || !validPlacement(placement.toMap(), 16, 10)) return reject(error, "Invalid component placement.");
        result["placement"] = placement;
    }
    if (input.contains("presentation") && (input.value("presentation").metaType().id() != QMetaType::QVariantMap
        || !validPresentation(input.value("presentation").toMap()))) return reject(error, "Invalid component presentation.");
    if (!input.value("customLogo").toString().isEmpty()) {
        static const QRegularExpression artwork("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$");
        if (!artwork.match(input.value("customLogo").toString()).hasMatch()) return reject(error, "Invalid saved artwork path.");
    }
    result["schemaVersion"] = 1; result["componentType"] = type;
    result["size"] = input.value("size", "standard");
    QVariantMap source;
    if (type == "clock" || type == "text") {
        if (kind != "widget" || input.value("widgetType") != type) return reject(error, "Unsupported widget.");
        source = {{"schemaVersion", 1}, {"provider", "shell"}, {"kind", type}};
        result.remove("action");
    } else {
        auto requested = input.value("action").toMap();
        if (requested.isEmpty() && legacy && kind == "app") requested = {{"type", "launch-app"}, {"appId", input.value("appId")}};
        const auto normalized = action(requested, error);
        if (normalized.isEmpty()) return {};
        const auto actionType = normalized.value("type").toString();
        if (kind != "app" && kind != "action") return reject(error, "Invalid launcher kind.");
        if (kind == "app" && (actionType != "launch-app" || input.value("appId") != normalized.value("appId")))
            return reject(error, "App binding does not match its action.");
        if (kind == "action" && input.value("name").toString().trimmed().isEmpty()) return reject(error, "A launcher needs a name.");
        source = {{"schemaVersion", 1}};
        if (actionType == "launch-app") source.insert({{"provider", "apps"}, {"kind", "app"}, {"id", normalized.value("appId")}});
        else if (actionType == "open-collection" || actionType == "play-movie" || actionType == "open-show")
            source.insert({{"provider", "kodi"}, {"kind", "collection"}, {"id", normalized.value("collectionId")}});
        else if (actionType == "open-page") source.insert({{"provider","shell"},{"kind","page"},{"id",normalized.value("pageId")}});
        else if (actionType == "switch-input") source.insert({{"provider", "isolde"}, {"kind", "capture-input"}, {"id", QString::number(normalized.value("input").toInt())}});
        else if (actionType.startsWith("open-")) source.insert({{"provider", "handler"}, {"kind", actionType}, {"id", normalized.value("openWith")}});
        else source.insert({{"provider", "shell"}, {"kind", actionType}});
        if (type == "media-shelf" && actionType != "open-collection") return reject(error, "A media shelf requires a collection source.");
        result["action"] = normalized;
    }
    // A source is a capability binding, never a second conflicting launch route.
    if (!legacy && input.value("source").toMap() != source) return reject(error, "Source binding does not match its action.");
    result["source"] = source;
    return result;
}
QString ComponentSchema::unavailable(const QVariantMap &value, const AppRegistry *apps, const QVariantList &collections)
{
    const auto type = value.value("type").toString();
    if(type=="open-page")return {}; // Page existence belongs to the shell dispatcher, never an Open With app.
    if (type == "launch-app") return apps->find(value.value("appId").toString()) ? QString() : "App missing · repair source";
    if (type == "open-collection" || type == "play-movie" || type == "open-show") {
        for (const auto &entry : collections) {
            const auto definition = entry.toMap();
            if (definition.value("id") != value.value("collectionId")) continue;
            if (type == "open-show" && definition.value("media", "movies").toString() != "shows") break;
            if (type == "play-movie" && definition.value("media", "movies").toString() != "movies") break;
            return {};
        }
        return "Collection missing · repair source";
    }
    if (type.startsWith("open-")) {
        const auto handler = apps->find(value.value("openWith").toString());
        if (!handler) return "Open With app missing · repair source";
        if (!handler->handledActions.contains(type)) return "Incompatible Open With app · repair source";
    }
    return {};
}

bool ComponentSchema::placementsFit(const QJsonArray &items, const QVariantMap &grid)
{
    const int columns = grid.value("columns").toInt(), rows = grid.value("rows").toInt();
    for (const auto &entry : items) {
        const auto item = entry.toObject();
        if (!item.contains("placement")) continue;
        if (!validPlacement(item.value("placement").toObject().toVariantMap(), columns, rows))
            return false;
    }
    if (grid.value("overlap").toBool()) return true;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const auto first = items[i].toObject();
        if (!first.contains("placement")) continue;
        for (qsizetype j = i + 1; j < items.size(); ++j) {
            const auto second = items[j].toObject();
            if (second.contains("placement") && placementsOverlap(
                    first.value("placement").toObject().toVariantMap(),
                    second.value("placement").toObject().toVariantMap())) return false;
        }
    }
    return true;
}

QVariantMap ComponentSchema::sectionGrid(const QVariantMap &composition, const QJsonArray &categories, const QString &categoryId)
{
    auto grid=normalizedGrid(composition.value("grid").toMap());
    if(grid.value("mode")!="sections")return grid;
    for(const auto &entry:categories) {
        const auto category=entry.toObject();
        if(category["id"]!=categoryId)continue;
        const auto override=category["presentation"].toObject()["grid"].toObject().toVariantMap();
        for(auto it=override.cbegin();it!=override.cend();++it)grid[it.key()]=it.value();
    }
    return grid;
}

bool ComponentSchema::layoutPlacementsFit(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories)
{
    const auto grid=normalizedGrid(composition.value("grid").toMap());
    if(grid.value("mode")!="sections")return placementsFit(items,grid);
    QHash<QString,QJsonArray> groups;
    for(const auto &entry:items) {
        if(!entry.toObject().contains("placement"))return false;
        groups[entry.toObject()["categoryId"].toString()].append(entry);
    }
    for(const auto &entry:categories)if(entry.toObject()["presentation"].toObject()["variant"]=="library"
        && !groups.value(entry.toObject()["id"].toString()).isEmpty())return false;
    for(auto it=groups.cbegin();it!=groups.cend();++it)
        if(!placementsFit(it.value(),sectionGrid(composition,categories,it.key())))return false;
    return true;
}
