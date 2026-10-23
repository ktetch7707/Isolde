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
    if(!map.contains("schemaVersion"))return true;
    const auto version=map.value("schemaVersion");
    return version.metaType().id()!=QMetaType::QString && version.metaType().id()!=QMetaType::Bool && version.toDouble()==1.0;
}

// Identity, text and cache bounds shared by the schema validator and its
// clients. Changing a bound changes the accepted document format; keep
// docs/components.md in sync with any change here.
constexpr int MaxComponentIdLength = 160;      // safeId allows 1 leading + 159 trail characters
constexpr int MaxNameLength = 80;              // display names, labels, tab names
constexpr int MaxAccessibleTextLength = 160;   // accessibility name / hint
constexpr int MaxComponentNameLength = 160;    // component display name
constexpr int MaxSymbolLength = 80;            // tile symbol text
constexpr int MaxActionTargetSize = 4096;      // open-folder / open-file / open-disc / open-url target

// Child layout grid bounds. The home grid itself is 4-16 columns by 3-10 rows;
// nested child layouts may start at a single column or row.
constexpr int MinChildColumns = 1;
constexpr int MaxChildColumns = 16;
constexpr int MinChildRows = 1;
constexpr int MaxChildRows = 10;
constexpr int MinChildGapOrPadding = 0;
constexpr int MaxChildGapOrPadding = 100;

// Default home grid (screen mode): 8 columns x 5 rows, 12 gap, 20 margin.
constexpr int DefaultGridColumns = 8;
constexpr int DefaultGridRows = 5;
constexpr int DefaultGridGap = 12;
constexpr int DefaultGridMargin = 20;

// PageData runtime caches, per active profile.
constexpr int MaxPageDataClientKeySize = 330;      // full client key length
constexpr int MaxPageDataClientIdLength = 160;     // id prefix validated with safeId
constexpr int MaxPageDataClients = 128;            // library clients
constexpr int MaxPageDataArtwork = 512;            // artwork items

// Action payload limits.
constexpr int MinMediaId = 1;                      // media ids are 1-based
constexpr int MaxMediaId = 2147483647;             // INT_MAX, bounded for safe JSON double decoding
constexpr int MinSettingsSection = -1;             // "no section" sentinel
constexpr int MaxSettingsSection = 5;

// HDMI input bounds are declared in ComponentSchema.h (shared with the shell).

// Home grid bounds (stricter than child layout bounds).
constexpr int MinGridColumns = 4;
constexpr int MaxGridColumns = 16;
constexpr int MinGridRows = 3;
constexpr int MaxGridRows = 10;

// Home layout columns: a page composition is single or dual column.
constexpr int MinHomeLayoutColumns = 1;
constexpr int MaxHomeLayoutColumns = 2;

// Opacity bounds are declared in ComponentSchema.h (shared with the shell).
// MaxComponentNestingDepth is declared in ComponentSchema.h (shared with HomeLayout).

// Page composition bounds.
constexpr int MaxNavigationTabs = 64;
constexpr int MaxHeaderControls = 3;
// Min/MaxSidebarWidth, Min/MaxSectionHeight, Min/MaxDesignHeight are declared
// in ComponentSchema.h (shared with HomeLayout).
}
bool ComponentSchema::safeId(const QString &id)
{
    // A valid id starts with an alphanumeric and holds up to
    // MaxComponentIdLength characters total. "." and ".." are rejected even
    // though the pattern would otherwise admit them.
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,%1}$")
                                            .arg(MaxComponentIdLength - 1));
    return id != QLatin1String(".") && id != QLatin1String("..") && pattern.match(id).hasMatch();
}
bool ComponentSchema::validChildLayout(const QVariantMap &value)
{
    static const QSet<QString> keys{"schemaVersion","mode","columns","rows","gap","padding","visibleColumns","wrap"};
    for(auto it=value.cbegin();it!=value.cend();++it)if(!keys.contains(it.key()))return false;
    if(value.value("schemaVersion").metaType().id()==QMetaType::QString || value.value("schemaVersion").metaType().id()==QMetaType::Bool || value.value("schemaVersion").toDouble()!=1)return false;
    if(!QStringList{"grid","row","column"}.contains(value.value("mode").toString()))return false;
    for(const auto &key:QStringList{"columns","rows","gap","padding"}) {
        if(!value.contains(key) || value[key].metaType().id()==QMetaType::QString || value[key].metaType().id()==QMetaType::Bool)return false;
        bool ok=false;const auto number=value[key].toDouble(&ok);
        const double maximum = key == "columns" ? MaxChildColumns
                            : key == "rows" ? MaxChildRows
                            : MaxChildGapOrPadding;
        const double minimum = key == "columns" ? MinChildColumns
                            : key == "rows" ? MinChildRows
                            : MinChildGapOrPadding;
        if(!ok || !std::isfinite(number) || number<minimum || number>maximum || ((key=="columns" || key=="rows") && std::floor(number)!=number))return false;
    }
    if(value.contains("wrap") && value["wrap"].metaType().id()!=QMetaType::Bool)return false;
    if(value.contains("visibleColumns")){
        const auto n=value["visibleColumns"];if(value["mode"]!="row" || n.metaType().id()==QMetaType::QString || n.metaType().id()==QMetaType::Bool || !std::isfinite(n.toDouble()) || std::floor(n.toDouble())!=n.toDouble() || n.toInt()<MinChildColumns || n.toInt()>value["columns"].toInt())return false;
    }
    return !(value["mode"]=="row" && value["rows"].toInt()!=1) && !(value["mode"]=="column" && value["columns"].toInt()!=1);
}
bool ComponentSchema::validPresentation(const QVariantMap &value, bool section, bool container)
{
    for (auto it = value.cbegin(); it != value.cend(); ++it) {
        const auto key = it.key();
        if(key=="states") {
            if(it.value().metaType().id()!=QMetaType::QVariantMap)return false;
            const auto states=it.value().toMap();
            for(auto state=states.cbegin();state!=states.cend();++state){
                if(!QStringList{"normal","focused","selected","pressed","disabled","unavailable"}.contains(state.key()) || state.value().metaType().id()!=QMetaType::QVariantMap)return false;
                const auto fields=state.value().toMap();for(auto f=fields.cbegin();f!=fields.cend();++f)if(!QStringList{"surfaceColor","surfaceOpacity","labelColor","iconColor","focusColor","borderColor","borderWidth","focusWidth","radius","labelSize","labelBold","labelSurfaceColor"}.contains(f.key()))return false;
                if(!validPresentation(fields))return false;
            }
        } else if(key=="layout") {
            if(!container || it.value().metaType().id()!=QMetaType::QVariantMap || !validChildLayout(it.value().toMap()))return false;
        } else if (key == "grid") {
            if (!section || it.value().metaType().id()!=QMetaType::QVariantMap || !validGrid(it.value().toMap())
                || it.value().toMap().value("mode","screen")!="screen") return false;
        } else if (key == "variant") {
            if (!QStringList{"banner", "media", "source", "summary", "launcher", "orb", "recent", "library", "text", "image"}.contains(it.value().toString())) return false;
        } else if(key=="artFit" || key=="labelPosition" || key=="textAlign") {
            const auto choices=key=="artFit"?QStringList{"cover","contain"}:key=="labelPosition"?QStringList{"inside","below","overlay","hidden"}:QStringList{"left","center","right"};
            if(it.value().metaType().id()!=QMetaType::QString || !choices.contains(it.value().toString()))return false;
        } else if(key=="labelBold" || key=="labelWrap" || key=="focusEnabled") {
            if(it.value().metaType().id()!=QMetaType::Bool)return false;
        } else if(key=="artFocalX" || key=="artFocalY" || key=="artZoom" || key=="focusWidth" || key=="borderWidth" || key=="labelLines" || key=="libraryLimit") {
            if(it.value().metaType().id()==QMetaType::QString || it.value().metaType().id()==QMetaType::Bool)return false;
            bool ok=false;const auto n=it.value().toDouble(&ok);const double minimum=key=="artZoom" || key=="labelLines" || key=="libraryLimit"?1:0,maximum=key=="artZoom"?8:key=="focusWidth" || key=="borderWidth"?12:key=="labelLines"?4:key=="libraryLimit"?50:1;
            if(!ok || !std::isfinite(n) || n<minimum || n>maximum || ((key=="labelLines" || key=="libraryLimit") && std::floor(n)!=n))return false;
        } else if(key=="borderColor" || key=="focusColor" || key=="labelSurfaceColor") {
            if(it.value().metaType().id()!=QMetaType::QString || !QColor(it.value().toString()).isValid() || QColor(it.value().toString()).alpha()!=255)return false;
        } else if (key == "libraryKind" || key == "libraryView" || key == "libraryLayout" || key == "libraryValue" || key == "libraryColumns" || key == "libraryCardStyle") {
            if (!section) return false;
            if (key == "libraryKind" && !QStringList{"movies","shows"}.contains(it.value().toString())) return false;
            if (key == "libraryView" && !QStringList{"all","recent","watched","continue","search","tag","genre","collection"}.contains(it.value().toString())) return false;
            if (key == "libraryLayout" && !QStringList{"grid","list","row"}.contains(it.value().toString())) return false;
            if (key == "libraryCardStyle" && !QStringList{"media","recent","orb","source","summary","banner"}.contains(it.value().toString())) return false;
            if (key == "libraryValue" && (it.value().metaType().id()!=QMetaType::QString || it.value().toString().size()>160)) return false;
            if (key == "libraryColumns" && (it.value().metaType().id()==QMetaType::QString || it.value().metaType().id()==QMetaType::Bool || it.value().toDouble()<1 || it.value().toDouble()>12 || std::floor(it.value().toDouble())!=it.value().toDouble())) return false;
        } else if (key == "subtitle" || key == "label" || key == "icon" || key == "trailingLabel") {
            if (it.value().metaType().id() != QMetaType::QString || it.value().toString().size() > 160) return false;
        } else if (QStringList{"surfaceColor", "panelColor", "headingColor", "iconColor", "labelColor", "selectedLabelColor"}.contains(key)) {
            const QColor color(it.value().toString()); if (!color.isValid() || color.alpha() != 255) return false;
        } else if (key == "libraryChrome") {
            if(!section || it.value().metaType().id()!=QMetaType::Bool)return false;
        } else if (key == "showHeading" || key == "labelPill" || key == "headingBold") {
            if ((key == "showHeading" && !section && !container) || it.value().metaType().id() != QMetaType::Bool) return false;
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
    static const QSet<QString> keys{"schemaVersion", "enabled", "navigation", "initialTab", "margin", "gap", "fit", "showProfile", "columns", "wallpaper", "headerMode", "identity", "headerControls", "showClock", "designHeight", "pageNavigation", "sidebarWidth", "moviePage", "showsPage", "grid", "sectionHeight", "background", "verticalScrolling"};
    for (auto it = value.cbegin(); it != value.cend(); ++it) if (!keys.contains(it.key())) return false;
    if(value.contains("background")) {
        const auto background=value["background"].toMap();
        for(auto it=background.cbegin();it!=background.cend();++it)if(!QStringList{"mode","dim","artFit","artFocalX","artFocalY","artZoom","imageCrop","tintColor"}.contains(it.key()))return false;
        if(value["background"].metaType().id()!=QMetaType::QVariantMap || !QStringList{"solid","gradient","image"}.contains(background.value("mode").toString()))return false;
        auto art=background;art.remove("mode");const auto dim=art.take("dim");const auto tint=art.take("tintColor");if(tint.isValid() && (tint.metaType().id()!=QMetaType::QString || !QColor(tint.toString()).isValid() || QColor(tint.toString()).alpha()!=255))return false;
        if(dim.isValid() && (dim.metaType().id()==QMetaType::QString || dim.metaType().id()==QMetaType::Bool || !std::isfinite(dim.toDouble()) || dim.toDouble()<0 || dim.toDouble()>1))return false;
        if(!validPresentation(art))return false;
    }
    if (value.contains("pageNavigation") && !QStringList{"left","top","none"}.contains(value["pageNavigation"].toString())) return false;
    if (value.contains("sidebarWidth") && (value["sidebarWidth"].metaType().id()==QMetaType::QString || value["sidebarWidth"].toDouble()<120 || value["sidebarWidth"].toDouble()>400)) return false;
    for (const auto &key : {"moviePage","showsPage"}) if(value.contains(key) && !safeId(value[key].toString())) return false;
    if (value.value("schemaVersion" ).toDouble() != 1) return false;
    if (value.contains("columns") && value["columns"].toDouble() != 1 && value["columns"].toDouble() != 2) return false;
    if (value.contains("grid") && (value["grid"].metaType().id() != QMetaType::QVariantMap || !validGrid(value["grid"].toMap()))) return false;
    // A sections grid needs exactly one column, no fit scaling and an explicit section height.
    if(value.value("grid").toMap().value("mode")=="sections"
        && (value.value("columns",1).toInt()!=MinHomeLayoutColumns || value.value("fit",false).toBool() || !value.contains("sectionHeight")))return false;
    if(value.contains("sectionHeight") && (value["sectionHeight"].metaType().id()==QMetaType::QString
        || value["sectionHeight"].metaType().id()==QMetaType::Bool || !std::isfinite(value["sectionHeight"].toDouble())
        || value["sectionHeight"].toDouble()<MinSectionHeight || value["sectionHeight"].toDouble()>MaxSectionHeight))return false;
    if (value.contains("wallpaper") && !QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$").match(value["wallpaper"].toString()).hasMatch()) return false;
    if (value.contains("headerMode") && !QStringList{"navigation", "identity"}.contains(value["headerMode"].toString())) return false;
    if (value.contains("identity") && (value["identity"].metaType().id() != QMetaType::QString || value["identity"].toString().size() > MaxNameLength)) return false;
    if (value.contains("designHeight")) {
        bool ok; const double height = value["designHeight"].toDouble(&ok);
        if (!ok || !std::isfinite(height) || value["designHeight"].metaType().id() == QMetaType::QString
            || value["designHeight"].metaType().id() == QMetaType::Bool || height < MinDesignHeight || height > MaxDesignHeight) return false;
    }
    if (value.contains("headerControls")) {
        const auto controls = value["headerControls"].toList(); QSet<QString> unique;
        if (controls.isEmpty() || controls.size() > MaxHeaderControls || value["headerControls"].metaType().id() != QMetaType::QVariantList) return false;
        for (const auto &control : controls) { const auto id = control.toString(); if (!QStringList{"search", "settings", "edit"}.contains(id) || unique.contains(id)) return false; unique.insert(id); }
        if (!unique.contains("settings") && !unique.contains("edit")) return false;
    }
    if(value.value("verticalScrolling").toBool() && (!value.value("enabled").toBool() || !value.contains("grid") || normalizedGrid(value.value("grid").toMap()).value("mode")!="screen"))return false;
    for (const auto &key : {"enabled", "fit", "showProfile", "showClock", "verticalScrolling"})
        if (value.contains(key) && value[key].metaType().id() != QMetaType::Bool) return false;
    for (const auto &key : {"margin", "gap"}) if (value.contains(key)) {
        bool ok; const double number = value[key].toDouble(&ok);
        if (!ok || !std::isfinite(number) || number < MinChildGapOrPadding || number > MaxChildGapOrPadding) return false;
    }
    const auto tabs = value.value("navigation").toList();
    if (tabs.size() > MaxNavigationTabs || (value.contains("navigation") && value["navigation"].metaType().id() != QMetaType::QVariantList)) return false;
    QSet<QString> ids;
    for (const auto &entry : tabs) {
        const auto tab = entry.toMap(); const auto id = tab.value("id").toString();
        if (!safeId(id) || ids.contains(id) || tab.value("name").toString().trimmed().isEmpty() || tab.value("name").toString().size() > MaxNameLength) return false;
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
            || number < MinGridColumns || number > MaxGridColumns || std::floor(number) != number) return false;
    }
    if (value.contains("rows")) {
        bool ok; const double number = value.value("rows").toDouble(&ok);
        if (!ok || value.value("rows").metaType().id() == QMetaType::QString || value.value("rows").metaType().id() == QMetaType::Bool
            || number < MinGridRows || number > MaxGridRows || std::floor(number) != number) return false;
    }
    for (const auto &key : {"gap", "margin"}) if (value.contains(key)) {
        bool ok; const double number = value.value(key).toDouble(&ok);
        if (!ok || !std::isfinite(number) || value.value(key).metaType().id() == QMetaType::QString
            || value.value(key).metaType().id() == QMetaType::Bool || number < MinChildGapOrPadding || number > MaxChildGapOrPadding) return false;
    }
    for (const auto &key : {"snap", "overlap"})
        if (value.contains(key) && value.value(key).metaType().id() != QMetaType::Bool) return false;
    return true;
}
QVariantMap ComponentSchema::normalizedGrid(const QVariantMap &value)
{
    // Screen-mode default grid: 8 x 5 cells with a 12 gap and 20 margin.
    QVariantMap grid{{"columns", DefaultGridColumns}, {"rows", DefaultGridRows}, {"gap", DefaultGridGap}, {"margin", DefaultGridMargin}, {"snap", true}, {"overlap", false}, {"mode","screen"}};
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
            if (!std::isfinite(movie) || movie < MinMediaId || movie > MaxMediaId || std::floor(movie) != movie)
                return reject(error, "Choose a valid movie.");
            result["movieId"] = static_cast<int>(movie);
        }
        if (type == "open-show") {
            const double show = input.value("tvShowId").toDouble();
            if (!std::isfinite(show) || show < MinMediaId || show > MaxMediaId || std::floor(show) != show)
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
        const double section = input.value("section", MinSettingsSection).toDouble();
        if (section < MinSettingsSection || section > MaxSettingsSection || std::floor(section) != section) return reject(error, "Invalid Settings section.");
        result["section"] = static_cast<int>(section);
    } else if (type == "switch-input") {
        const double inputNumber = input.value("input").toDouble();
        if (!std::isfinite(inputNumber) || inputNumber < MinHdmiInput || inputNumber > MaxHdmiInput || std::floor(inputNumber) != inputNumber)
            return reject(error, "Choose an HDMI input from 1 to 8.");
        result["input"] = static_cast<int>(inputNumber);
    } else if (type == "desktop-mode") {
        result["label"] = input.value("label", "Desktop Mode").toString().left(MaxNameLength);
    } else if (QStringList{"open-folder", "open-file", "open-disc", "open-url"}.contains(type)) {
        const auto target = input.value("target").toString().trimmed();
        const auto handler = input.value("openWith").toString();
        if (target.isEmpty() || target.size() > MaxActionTargetSize || target.contains(QChar::Null)
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
    if (type.isEmpty() && legacy) type = kind == "widget" ? (QStringList{"clock","text","container"}.contains(input.value("widgetType").toString())?input.value("widgetType").toString():"clock") : "launcher";
    if (!QStringList{"launcher", "clock", "text", "media-shelf","container","button","image","library"}.contains(type)) return reject(error, "Unsupported component type.");
    if(input.contains("name") && (input["name"].metaType().id()!=QMetaType::QString || input["name"].toString().size()>MaxComponentNameLength || input["name"].toString().contains(QChar::Null)))return reject(error,"Use a component name of at most 160 characters.");
    if(input.contains("family") && (type!="container" || !QStringList{"carousel","app-bar"}.contains(input["family"].toString())))return reject(error,"Unsupported group family.");
    if(input.contains("appCollection")) {
        if(type!="container" || input.value("family")!="app-bar" || input["appCollection"].metaType().id()!=QMetaType::QVariantMap)return reject(error,"Choose an app carousel for an app collection.");
        const auto collection=input["appCollection"].toMap();
        if(collection.size()!=2 || !QStringList{"all","selected"}.contains(collection.value("mode").toString()) || collection.value("ids").metaType().id()!=QMetaType::QVariantList || collection.value("ids").toList().size()>128)return reject(error,"Choose All installed or up to 128 selected apps.");
        QSet<QString> ids;
        for(const auto &id:collection.value("ids").toList()) {if(id.metaType().id()!=QMetaType::QString || !safeId(id.toString()) || ids.contains(id.toString()))return reject(error,"Choose distinct safe app identities.");ids.insert(id.toString());}
        if(collection.value("mode")=="all" && !ids.isEmpty())return reject(error,"All installed apps does not store selected membership.");
    }
    if(input.contains("accessibility")){
        if(input["accessibility"].metaType().id()!=QMetaType::QVariantMap)return reject(error,"Invalid accessible description.");
        const auto a=input["accessibility"].toMap();for(auto it=a.cbegin();it!=a.cend();++it)if(!QStringList{"name","hint"}.contains(it.key()) || it.value().metaType().id()!=QMetaType::QString || it.value().toString().size()>MaxAccessibleTextLength || it.value().toString().contains(QChar::Null))return reject(error,"Use bounded accessible text.");
    }
    if(input.contains("parentId")) {
        if(input["parentId"].metaType().id()!=QMetaType::QString || (!input["parentId"].toString().isEmpty() && !safeId(input["parentId"].toString())))return reject(error,"Invalid parent identity.");
        if(input["parentId"].toString().isEmpty())result.remove("parentId");
    }
    if(input.contains("children"))return reject(error,"Group children use validated component parent identities.");
    if(input.contains("requiredCapability") && (type!="container" || input["requiredCapability"]!="capture-inputs"))return reject(error,"Unsupported component capability.");
    if (!QStringList{"compact", "standard", "wide"}.contains(input.value("size", "standard").toString()))
        return reject(error, "Invalid component size.");
    if (input.contains("opacity")) {
        if(input["opacity"].metaType().id()==QMetaType::QString || input["opacity"].metaType().id()==QMetaType::Bool)return reject(error,"Opacity must be numeric.");
        const auto opacity = input.value("opacity").toDouble();
        if (!std::isfinite(opacity) || opacity < MinOpacity || opacity > MaxOpacity) return reject(error, "Invalid component opacity.");
    }
    for(const auto &key:QStringList{"hidden","locked"})if(input.contains(key) && input.value(key).metaType().id()!=QMetaType::Bool)return reject(error,"Visibility and lock must be boolean values.");
    for(const auto &key:QStringList{"showTitle","showType","showLogo","logoFill"})if(input.contains(key) && input[key].metaType().id()!=QMetaType::Bool)return reject(error,"Presentation toggles must be boolean values.");
    if(input.contains("accent") && (input["accent"].metaType().id()!=QMetaType::QString || !QColor(input["accent"].toString()).isValid() || QColor(input["accent"].toString()).alpha()!=255))return reject(error,"Choose an opaque accent color.");
    if(input.contains("symbol") && (input["symbol"].metaType().id()!=QMetaType::QString || input["symbol"].toString().size()>MaxSymbolLength))return reject(error,"Use a short symbol.");
    if (input.contains("placement")) {
        // Shape-only bounds here; the layout validates against its actual grid and neighbors.
        const auto placement = input.value("placement");
        if (placement.metaType().id() != QMetaType::QVariantMap
            || !validPlacement(placement.toMap(), MaxChildColumns, MaxScrollableRows)
            || placement.toMap().value("height").toInt()>MaxChildRows) return reject(error, "Invalid component placement.");
        result["placement"] = placement;
    }
    if (input.contains("presentation") && (input.value("presentation").metaType().id() != QMetaType::QVariantMap
        || !validPresentation(input.value("presentation").toMap(),type=="library",type=="container"))) return reject(error, "Invalid component presentation.");
    if (!input.value("customLogo").toString().isEmpty()) {
        static const QRegularExpression artwork("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$");
        if (!artwork.match(input.value("customLogo").toString()).hasMatch()) return reject(error, "Invalid saved artwork path.");
    }
    result["schemaVersion"] = 1; result["componentType"] = type;
    result["size"] = input.value("size", "standard");
    QVariantMap source;
    if(type=="button" || type=="image" || type=="library") {
        if(kind!="widget" || input.value("widgetType")!=type || input.value("name").toString().trimmed().isEmpty())return reject(error,"A visual component needs its registered type and a name.");
        if(type=="library") {
            if(input.contains("action"))return reject(error,"Kodi owns media activation.");
            const auto style=input.value("presentation").toMap();
            if(!style.contains("libraryKind") || !style.contains("libraryView"))return reject(error,"Choose a supported Kodi view.");
            if(QStringList{"collection","genre","tag"}.contains(style.value("libraryView").toString()) && style.value("libraryValue").toString().trimmed().isEmpty())return reject(error,"Choose a named Kodi source.");
            if(style.value("libraryKind")=="shows" && QStringList{"genre","tag"}.contains(style.value("libraryView").toString()))return reject(error,"This Kodi view is available for movies only.");
            if(style.value("libraryView")=="collection" && !safeId(style.value("libraryValue").toString()))return reject(error,"Choose a saved collection.");
            source={{"schemaVersion",1},{"provider","kodi"},{"kind","library"},{"libraryKind",style.value("libraryKind")},{"libraryView",style.value("libraryView")},{"libraryValue",style.value("libraryValue","")}};
        } else {
            if(input.contains("action")) {const auto link=action(input["action"].toMap(),error);if(link.isEmpty())return {};result["action"]=link;}
            source={{"schemaVersion",1},{"provider","shell"},{"kind",type}};
        }
    } else if (type == "clock" || type == "text" || type=="container") {
        if (kind != "widget" || input.value("widgetType") != type) return reject(error, "Unsupported widget.");
        if(type=="container" && (input.value("name").toString().trimmed().isEmpty() || input.value("name").toString().size()>MaxNameLength || input.value("name").toString().contains(QChar::Null) || !validChildLayout(input.value("presentation").toMap().value("layout").toMap())))return reject(error,"A group needs a name and a bounded internal layout.");
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
    if(grid.value("mode")=="screen" && composition.value("verticalScrolling").toBool())grid["rows"]=MaxScrollableRows;
    if(grid.value("mode")!="sections")return grid;
    for(const auto &entry:categories) {
        const auto category=entry.toObject();
        if(category["id"]!=categoryId)continue;
        const auto override=category["presentation"].toObject()["grid"].toObject().toVariantMap();
        for(auto it=override.cbegin();it!=override.cend();++it)grid[it.key()]=it.value();
    }
    return grid;
}

QVariantMap ComponentSchema::componentGrid(const QJsonArray &items,const QVariantMap &composition,const QJsonArray &categories,const QString &parentId,const QString &categoryId)
{
    if(parentId.isEmpty())return sectionGrid(composition,categories,categoryId);
    for(const auto &entry:items){const auto item=entry.toObject();if(item["id"]!=parentId)continue;
        if(item["componentType"]!="container")return {};
        const auto layout=item["presentation"].toObject()["layout"].toObject();
        return {{"columns",layout["columns"].toInt()},{"rows",layout["rows"].toInt()},{"gap",layout["gap"].toDouble()},{"margin",layout["padding"].toDouble()},{"overlap",false},{"snap",true},{"mode","screen"}};
    }
    return {};
}
QString ComponentSchema::effectiveCategory(const QJsonArray &items,const QString &id)
{
    auto current=id;QSet<QString> visited;
    for(int depth=0;depth<=MaxComponentNestingDepth && !current.isEmpty();++depth){
        if(visited.contains(current))return {};visited.insert(current);bool found=false;
        for(const auto &entry:items){const auto item=entry.toObject();if(item["id"]!=current)continue;found=true;const auto parent=item.value("parentId").toString();if(parent.isEmpty())return item.value("categoryId").toString();current=parent;break;}
        if(!found)return {};
    }
    return {};
}
bool ComponentSchema::positionLocked(const QJsonArray &items,const QString &id)
{
    auto current=id;QSet<QString> visited;
    for(int depth=0;depth<=MaxComponentNestingDepth && !current.isEmpty();++depth){
        if(visited.contains(current))return true;visited.insert(current);bool found=false;
        for(const auto &entry:items){const auto item=entry.toObject();if(item["id"]!=current)continue;found=true;if(item.value("locked").toBool())return true;current=item.value("parentId").toString();break;}
        if(!found)return true;
    }
    return !current.isEmpty();
}
bool ComponentSchema::hierarchyFits(const QJsonArray &items,const QVariantMap &composition,const QJsonArray &categories,QString *error)
{
    const auto fail=[&](const QString &message){if(error)*error=message;return false;};
    if(error)error->clear();
    QHash<QString,QJsonObject> byId;QHash<QString,QJsonArray> children;QJsonArray roots;bool containers=false;
    for(const auto &entry:items){const auto item=entry.toObject();const auto id=item["id"].toString();if(!safeId(id) || byId.contains(id))return fail("A component identity is invalid or duplicated.");byId[id]=item;if(item["componentType"]=="container")containers=true;}
    for(const auto &entry:items){
        const auto item=entry.toObject();const auto id=item["id"].toString(),parent=item.value("parentId").toString();
        if(parent.isEmpty())roots.append(item);
        else{
            if(!item.value("categoryId").toString().isEmpty())return fail("A nested component inherits its section from its parent.");
            if(!item.contains("placement"))return fail("Every nested component needs a local placement.");
            children[parent].append(item);
        }
        QSet<QString> visited{id};auto ancestor=parent;int depth=item["componentType"]=="container"?1:0;
        while(!ancestor.isEmpty()){
            if(visited.contains(ancestor))return fail("A group cannot contain itself or one of its parents.");
            if(!byId.contains(ancestor) || byId[ancestor]["componentType"]!="container")return fail("Choose an existing group as the parent.");
            visited.insert(ancestor);if(++depth>MaxComponentNestingDepth)return fail("Groups support at most four nested container levels.");
            ancestor=byId[ancestor].value("parentId").toString();
        }
    }
    if(containers && (!composition.value("enabled").toBool() || !composition.contains("grid")))return fail("Groups require an enabled page grid. Preview grid conversion first.");
    for(auto it=byId.cbegin();it!=byId.cend();++it)if(it.value()["componentType"]=="container"){
        if(it.value().contains("appCollection") && !children.value(it.key()).isEmpty())return fail("An app collection uses registry membership, not placed children.");
        if(!it.value().contains("placement") || !validChildLayout(it.value()["presentation"].toObject()["layout"].toObject().toVariantMap()))return fail("A group needs a bounded placement and internal layout.");
        if(!placementsFit(children.value(it.key()),componentGrid(items,composition,categories,it.key(),{})))return fail("Group contents overlap or do not fit their local grid.");
    }
    const auto grid=sectionGrid(composition,categories,{});
    if(grid.value("mode")!="sections")return placementsFit(roots,grid)?true:fail("Page components overlap or do not fit the grid.");
    QHash<QString,QJsonArray> groups;
    for(const auto &entry:roots) {
        if(!entry.toObject().contains("placement"))return fail("Every section component needs a placement.");
        groups[entry.toObject()["categoryId"].toString()].append(entry);
    }
    for(const auto &entry:categories)if(entry.toObject()["presentation"].toObject()["variant"]=="library"
        && !groups.value(entry.toObject()["id"].toString()).isEmpty())return fail("A media shelf manages its own contents.");
    for(auto it=groups.cbegin();it!=groups.cend();++it)
        if(!placementsFit(it.value(),sectionGrid(composition,categories,it.key())))return fail("Section components overlap or do not fit their grid.");
    return true;
}
bool ComponentSchema::layoutPlacementsFit(const QJsonArray &items,const QVariantMap &composition,const QJsonArray &categories)
{
    return hierarchyFits(items,composition,categories);
}
