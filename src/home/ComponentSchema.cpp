#include <QColor>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRect>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <cmath>
#include "ComponentSchema.h"
#include "DesignSystem.h"
#include "app/AppRegistry.h"

/** Validates a source policy document.
 * @brief Validates a source policy document: bounded duration, known mode, unique page/slot ids, known event types, boolean flags and optional detected source/appliance sub-maps.
 * @details Iterates every field in the policy map and dispatches to the appropriate field validator. Ensures schemaVersion and duration are valid numbers, mode is one of off/compact/widget, pages/slots are unique safe ids, events/types are known media event kinds, boolean flags are actually booleans, and optional detected source / appliance sub-maps pass their own validation.
 * @why Source policies define how Isolde interprets incoming media events; invalid policies could cause incorrect source detection or unbounded data, so they must be validated before storage.
 * @param value The source policy document to validate.
 * @return true if the document is valid, false otherwise.
 */
bool ComponentSchema::validSourcePolicy(const QVariantMap& value)
{
    for (auto it = value.cbegin(); it != value.cend(); ++it)
    {
        if (!validSourcePolicyField(it.key(), it.value()))
            return false;
    }
    return true;
}

/** Dispatches a source policy field key to its field-specific validator.
 * @brief Dispatches a source policy field key to its field-specific validator.
 * @details Routes each key in the source policy map to the appropriate field-specific validator based on the key name.
 * @why Centralizes field dispatch logic so new fields can be added in one place.
 * @param key The field name to dispatch.
 * @param v The variant value to validate.
 * @return true if the field is valid, false otherwise.
 */
bool ComponentSchema::validSourcePolicyField(const QString& key, const QVariant& v)
{
    if (key == "schemaVersion" || key == "duration")
        return validSourcePolicyNumeric(v, key);
    if (key == "mode")
        return validSourcePolicyMode(v);
    if (key == "pages" || key == "slots")
        return validSourcePolicyIdList(v, key);
    if (key == "events" || key == "types")
        return validSourcePolicyEventTypeList(v, key);
    if (key == "reconnect" || key == "suppressPlayback" || key == "suppressSetup" || key == "suppressEditing" ||
        key == "discoveryEnabled")
        return validSourcePolicyBool(v);
    if (key == "detectedSource")
        return validSourcePolicyDetectedSourceField(v);
    if (key == "appliance")
        return validSourcePolicyApplianceField(v);
    return false;
}

/** Validates a detected source field (map wrapper).
 * @brief Validates a detected source field (map wrapper).
 * @details Checks that the variant is a QVariantMap and delegates to validSourcePolicyDetectedSource for full validation of id, type, and containment fields.
 * @why Detected sources represent physical media inputs; invalid identities could cause incorrect source mapping.
 * @param v The variant value to validate.
 * @return true if the value is a valid detected source map, false otherwise.
 */
bool ComponentSchema::validSourcePolicyDetectedSourceField(const QVariant& v)
{
    if (v.metaType().id() != QMetaType::QVariantMap)
        return false;
    return validSourcePolicyDetectedSource(v.toMap());
}

/** Validates an appliance field (map wrapper).
 * @brief Validates an appliance field (map wrapper).
 * @details Checks that the variant holds a QVariantMap and delegates to validSourcePolicyAppliance for full validation of id, type, and distribution mode fields.
 * @why Appliance definitions map physical devices to Isolde; invalid maps would cause incorrect device handling.
 * @param v The variant value to validate.
 * @return true if the value is a valid appliance map, false otherwise.
 */
bool ComponentSchema::validSourcePolicyApplianceField(const QVariant& v)
{
    if (v.metaType().id() != QMetaType::QVariantMap)
        return false;
    return validSourcePolicyAppliance(v.toMap());
}

/**
 * Validates a numeric source policy field (schemaVersion or duration).
 */
bool
ComponentSchema::validSourcePolicyNumeric(const QVariant& value, const QString& key)
{
    const auto type = value.metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong &&
        type != QMetaType::ULongLong && type != QMetaType::Double)
        return false;
    const double n = value.toDouble();
    if (!std::isfinite(n) || std::floor(n) != n)
        return false;
    if (key == "schemaVersion")
        return n == 1;
    return n == 0 || (n >= 5 && n <= 60);
}

/**
 * Validates the source policy mode field ("off", "compact" or "widget").
 */
bool
ComponentSchema::validSourcePolicyMode(const QVariant& value)
{
    return value.metaType().id() == QMetaType::QString &&
           QStringList{"off", "compact", "widget"}.contains(value.toString());
}

/**
 * Validates a source policy page or slot id list: unique safe ids.
 */
bool
ComponentSchema::validSourcePolicyIdList(const QVariant& value, const QString& key)
{
    if (value.metaType().id() != QMetaType::QVariantList || value.toList().size() > 64)
        return false;
    QSet<QString> seen;
    for (const auto& entry : value.toList())
    {
        if (entry.metaType().id() != QMetaType::QString)
            return false;
        const auto id = entry.toString();
        if (seen.contains(id))
            return false;
        if (!safeId(id) && !(key == "pages" && id.isEmpty()))
            return false;
        seen.insert(id);
    }
    return true;
}

/**
 * Validates a source policy event type list: known media event kinds.
 */
bool
ComponentSchema::validSourcePolicyEventTypeList(const QVariant& value, const QString& key)
{
    Q_UNUSED(key);
    if (value.metaType().id() != QMetaType::QVariantList || value.toList().size() > 4)
        return false;
    QSet<QString> seen;
    for (const auto& entry : value.toList())
    {
        if (entry.metaType().id() != QMetaType::QString)
            return false;
        const auto id = entry.toString();
        if (seen.contains(id))
            return false;
        if (!QStringList{"movie-disc", "game-disc", "media-folder", "file-source"}.contains(id))
            return false;
        seen.insert(id);
    }
    return true;
}

/** Validates a source policy boolean flag field.
 * @brief Validates a source policy boolean flag field.
 * @details Checks that the variant holds a QMetaType::Bool value.
 * @why Boolean flags in source policies control behavior; a non-boolean value would be misinterpreted.
 * @param value The variant value to validate.
 * @return true if the value is a boolean, false otherwise.
 */
bool ComponentSchema::validSourcePolicyBool(const QVariant& value)
{
    return value.metaType().id() == QMetaType::Bool;
}

/** Validates a detected source sub-map (id, type, containment).
 * @brief Validates a detected source sub-map: id, type, containment.
 * @details Checks that the value is a QVariantMap, contains id (safeId), type (one of movie-disc/game-disc/media-folder/file-source), and optional containment fields.
 * @why Detected sources represent physical media inputs; invalid identities could cause incorrect source mapping.
 * @param value The variant map to validate.
 * @return true if the detected source sub-map is valid, false otherwise.
 */
bool ComponentSchema::validSourcePolicyDetectedSource(const QVariantMap& value)
{
    if (!value.contains(QStringLiteral("id")) || !safeId(value.value(QStringLiteral("id")).toString()))
        return false;
    if (!value.contains(QStringLiteral("type")) ||
        !QStringList{"movie-disc", "game-disc", "media-folder", "file-source"}.contains(
            value.value(QStringLiteral("type")).toString()))
        return false;
    if (value.contains(QStringLiteral("containment")))
    {
        const auto& containment = value.value(QStringLiteral("containment")).toString();
        if (containment == "remote")
        {
            if (!value.contains(QStringLiteral("remoteRef")) || value.value(QStringLiteral("remoteRef")).toString().isEmpty())
                return false;
        }
        else if (containment == "local")
        {
            if (!value.contains(QStringLiteral("localPath")))
                return false;
        }
        else
            return false;
    }
    return true;
}

/** Validates an appliance sub-map (id, type, distribution mode).
 * @brief Validates an appliance sub-map: id, type, distribution mode.
 * @details Checks that the value is a QVariantMap, contains id (safeId), type (one of whiteware/AV appliance/climate appliance), and optional distributionMode and requiresSetup fields.
 * @why Appliance definitions map physical devices to Isolde; invalid maps would cause incorrect device handling.
 * @param value The variant map to validate.
 * @return true if the appliance sub-map is valid, false otherwise.
 */
bool ComponentSchema::validSourcePolicyAppliance(const QVariantMap& value)
{
    if (!value.contains(QStringLiteral("id")) || !safeId(value.value(QStringLiteral("id")).toString()))
        return false;
    if (!value.contains(QStringLiteral("type")) ||
        !QStringList{"whiteware", "AV appliance", "climate appliance"}.contains(
            value.value(QStringLiteral("type")).toString()))
        return false;
    if (value.contains(QStringLiteral("distributionMode")) &&
        !QStringList{"direct", "partner", "self-install"}.contains(
            value.value(QStringLiteral("distributionMode")).toString()))
        return false;
    if (value.contains(QStringLiteral("requiresSetup")) &&
        value.value(QStringLiteral("requiresSetup")).metaType().id() != QMetaType::Bool)
        return false;
    return true;
}

bool
ComponentSchema::validRecentStudioColors(const QVariant& value)
{
    if (value.metaType().id() != QMetaType::QVariantList || value.toList().size() > MaxRecentStudioColors)
        return false;
    QSet<QString> seen;
    for (const auto& entry : value.toList())
    {
        const auto text = entry.toString();
        if (entry.metaType().id() != QMetaType::QString ||
            !QRegularExpression(QStringLiteral("^#[0-9a-f]{6}$")).match(text).hasMatch() || seen.contains(text))
            return false;
        seen.insert(text);
    }
    return true;
}

namespace
{
QVariantMap
reject(QString* error, const QString& message)
{
    if (error)
        *error = message;
    return {};
}
bool
versionOne(const QVariantMap& map)
{
    if (!map.contains("schemaVersion"))
        return true;
    const auto version = map.value("schemaVersion");
    return version.metaType().id() != QMetaType::QString && version.metaType().id() != QMetaType::Bool &&
           version.toDouble() == 1.0;
}

// Identity, text and cache bounds shared by the schema validator and its
// clients. Changing a bound changes the accepted document format; keep
// docs/components.md in sync with any change here.
constexpr int MaxComponentIdLength = 160;    // safeId allows 1 leading + 159 trail characters
constexpr int MaxNameLength = 80;            // display names, labels, tab names
constexpr int MaxAccessibleTextLength = 160; // accessibility name / hint
constexpr int MaxComponentNameLength = 160;  // component display name
constexpr int MaxSymbolLength = 80;          // tile symbol text
constexpr int MaxActionTargetSize = 4096;    // open-folder / open-file / open-disc / open-url target

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

// Action payload limits.
constexpr int MinMediaId = 1;          // media ids are 1-based
constexpr int MaxMediaId = 2147483647; // INT_MAX, bounded for safe JSON double decoding
constexpr int MinSettingsSection = -1; // "no section" sentinel
constexpr int MaxSettingsSection = 6;

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
constexpr int MaxHeaderControls = 4;
constexpr int MinDesignHeight = 480;
constexpr int MaxDesignHeight = 1440;
} // namespace
/**
 * Validates a safe component identity: starts with alphanumeric, bounded
 * length, rejects "." and "..".
 */
bool
ComponentSchema::safeId(const QString& id)
{
    // A valid id starts with an alphanumeric and holds up to
    // MaxComponentIdLength characters total. "." and ".." are rejected even
    // though the pattern would otherwise admit them.
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,%1}$").arg(MaxComponentIdLength - 1));
    return id != QLatin1String(".") && id != QLatin1String("..") && pattern.match(id).hasMatch();
}
bool
ComponentSchema::validChildLayout(const QVariantMap& value)
{
    if (!validChildLayoutKeys(value))
        return false;
    if (!validChildLayoutVersion(value))
        return false;
    if (!validChildLayoutMode(value))
        return false;
    for (const auto& key : QStringList{"columns", "rows", "gap", "padding"})
        if (!validChildLayoutDimension(value, key))
            return false;
    if (!validChildLayoutWrap(value))
        return false;
    if (!validChildLayoutVisibleColumns(value))
        return false;
    return validChildLayoutModeConsistency(value);
}

/**
 * Validates child layout keys only (known key set).
 */
bool
ComponentSchema::validChildLayoutKeys(const QVariantMap& value)
{
    static const QSet<QString> keys{
        "schemaVersion", "mode", "columns", "rows", "gap", "padding", "visibleColumns", "wrap"};
    for (auto it = value.cbegin(); it != value.cend(); ++it)
        if (!keys.contains(it.key()))
            return false;
    return true;
}

/**
 * Validates child layout schemaVersion (numeric 1).
 */
bool
ComponentSchema::validChildLayoutVersion(const QVariantMap& value)
{
    return value.value("schemaVersion").metaType().id() != QMetaType::QString &&
           value.value("schemaVersion").metaType().id() != QMetaType::Bool &&
           value.value("schemaVersion").toDouble() == 1;
}

/**
 * Validates child layout mode ("grid", "row" or "column").
 */
bool
ComponentSchema::validChildLayoutMode(const QVariantMap& value)
{
    return QStringList{"grid", "row", "column"}.contains(value.value("mode").toString());
}

/**
 * Validates a child layout dimension field (columns, rows, gap or padding).
 */
bool
ComponentSchema::validChildLayoutDimension(const QVariantMap& value, const QString& key)
{
    if (!validChildLayoutDimensionType(value, key))
        return false;
    return validChildLayoutDimensionRange(value, key);
}

/**
 * Validates child layout dimension type (not string or bool).
 */
bool
ComponentSchema::validChildLayoutDimensionType(const QVariantMap& value, const QString& key)
{
    return value.contains(key) && value[key].metaType().id() != QMetaType::QString &&
           value[key].metaType().id() != QMetaType::Bool;
}

/**
 * Validates child layout dimension range (bounded to grid limits).
 */
bool
ComponentSchema::validChildLayoutDimensionRange(const QVariantMap& value, const QString& key)
{
    bool ok = false;
    const auto number = value[key].toDouble(&ok);
    const double maximum = key == "columns" ? (value["mode"] == "row" ? 128 : MaxChildColumns)
                               : key == "rows"  ? MaxChildRows
                                                : MaxChildGapOrPadding;
    const double minimum = key == "columns" ? MinChildColumns : key == "rows" ? MinChildRows : MinChildGapOrPadding;
    if (!ok || !std::isfinite(number) || number < minimum || number > maximum)
        return false;
    return !(key == "columns" || key == "rows") || std::floor(number) == number;
}

/**
 * Validates child layout wrap field (boolean or absent).
 */
bool
ComponentSchema::validChildLayoutWrap(const QVariantMap& value)
{
    return !value.contains("wrap") || value["wrap"].metaType().id() == QMetaType::Bool;
}

/**
 * Validates child layout visibleColumns field (row mode only).
 */
bool
ComponentSchema::validChildLayoutVisibleColumns(const QVariantMap& value)
{
    if (!value.contains("visibleColumns"))
        return true;
    const auto n = value["visibleColumns"];
    return value["mode"] == "row" &&
           n.metaType().id() != QMetaType::QString && n.metaType().id() != QMetaType::Bool &&
           std::isfinite(n.toDouble()) && std::floor(n.toDouble()) == n.toDouble() &&
           n.toInt() >= MinChildColumns && n.toInt() <= value["columns"].toInt();
}

/**
 * Validates child layout mode/columns/rows consistency.
 */
bool
ComponentSchema::validChildLayoutModeConsistency(const QVariantMap& value)
{
    return !(value["mode"] == "row" && value["rows"].toInt() != 1) &&
           !(value["mode"] == "column" && value["columns"].toInt() != 1);
}

bool
ComponentSchema::validPresentation(const QVariantMap& value, bool section, bool container)
{
    for (auto it = value.cbegin(); it != value.cend(); ++it)
    {
        if (!validPresentationField(it.key(), it.value(), value, section, container))
            return false;
    }
    if (value.value("libraryView") == "playlist" && !value.value("libraryValue").toString().isEmpty())
    {
        const auto name = value.value("libraryValue").toString();
        if (name.size() > 128 || name.contains(QRegularExpression("[/\\\\:%\\x00-\\x1f]")) ||
            !(name.endsWith(".xsp", Qt::CaseInsensitive) || name.endsWith(".m3u", Qt::CaseInsensitive) ||
              name.endsWith(".m3u8", Qt::CaseInsensitive)))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationField(const QString& key, const QVariant& v, const QVariantMap& value,
                                        bool section, bool container)
{
    if (key == "historyApps")
        return validPresentationHistoryApps(value, section);
    if (key == "historySourceBadges")
        return validPresentationHistorySourceBadges(value, section);
    if (key == "responsive")
        return validPresentationResponsive(value, section);
    if (key == "libraryChrome")
        return validPresentationLibraryChrome(value, section);
    if (key == "showHeading" || key == "labelPill" || key == "headingBold")
        return validPresentationHeadingFlags({{key, v}}, section, container);
    if (key == "trailingAction")
        return validPresentationTrailingAction(value, section);
    const QVariantMap presentationMap = (key == "states" || key == "layout" || key == "grid")
                                            ? v.toMap()
                                            : QVariantMap{{key, v}};
    if ((key == "states" || key == "layout" || key == "grid") &&
        v.metaType().id() != QMetaType::QVariantMap)
        return false;
    static const QMap<QString, std::function<bool(const QVariantMap&, bool, bool)>> handlers{
        {"states", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationStates(m); }},
        {"layout", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); return validPresentationLayout(m, c); }},
        {"grid", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationGrid(m, s); }},
        {"variant", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationVariant(m); }},
        {"fillMode", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationFill(m); }},
        {"fillDirection", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationFill(m); }},
        {"shape", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationShape(m); }},
        {"sourceMaximum", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSourceMaximum(m); }},
        {"sourceVariant", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSourceVariant(m); }},
        {"sourceDetails", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBoolFlag(m, "sourceDetails"); }},
        {"sourceBadge", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBoolFlag(m, "sourceBadge"); }},
        {"visibleLabel", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationStringField(m, "visibleLabel"); }},
        {"bodyText", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationStringField(m, "bodyText"); }},
        {"moreLabel", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationStringField(m, "moreLabel"); }},
        {"galleryTitle", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationStringField(m, "galleryTitle"); }},
        {"folderOnly", [](const QVariantMap& m, bool, bool c) { return c && validPresentationBoolFlag(m, "folderOnly"); }},
        {"appGallery", [](const QVariantMap& m, bool, bool c) { return c && validCompositionAppGallery(m); }},
        {"appIconStyle", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationIconStyle(m); }},
        {"iconFocus", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationIconStyle(m); }},
        {"iconBacking", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationIconStyle(m); }},
        {"iconGlow", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationIconGlow(m); }},
        {"iconGlowOpacity", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationIconGlow(m); }},
        {"iconPadding", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationIconGlow(m); }},
        {"artFit", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFit(m); }},
        {"labelPosition", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFit(m); }},
        {"textAlign", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFit(m); }},
        {"logoMode", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFit(m); }},
        {"bodyAlign", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBodyAlign(m); }},
        {"bodyColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBodyColor(m); }},
        {"bodySize", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBodyMetrics(m); }},
        {"bodyPadding", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBodyMetrics(m); }},
        {"bodyLines", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBodyMetrics(m); }},
        {"bodyLineHeight", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBodyMetrics(m); }},
        {"labelBold", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"labelWrap", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"artAspectLocked", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"sizeAspectLocked", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"bodyBold", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"bodyWrap", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"focusEnabled", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"animateArtwork", [](const QVariantMap& m, bool s, bool c) { return validPresentationBoolToggle(m, s, c); }},
        {"artFocalX", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"artFocalY", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"artZoom", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"focusWidth", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"borderWidth", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"labelLines", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"libraryLimit", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationArtFocal(m); }},
        {"borderColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBorderColor(m); }},
        {"focusColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBorderColor(m); }},
        {"labelSurfaceColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBorderColor(m); }},
        {"libraryKind", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationLibraryFields(m, s); }},
        {"libraryView", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationLibraryFields(m, s); }},
        {"libraryLayout", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationLibraryFields(m, s); }},
        {"libraryValue", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationLibraryFields(m, s); }},
        {"libraryColumns", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationLibraryFields(m, s); }},
        {"libraryCardStyle", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationLibraryFields(m, s); }},
        {"subtitle", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationShortString(m); }},
        {"label", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationShortString(m); }},
        {"icon", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationShortString(m); }},
        {"trailingLabel", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationShortString(m); }},
        {"surfaceColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"fillEndColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"panelColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"headingColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"iconColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"labelColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"selectedLabelColor", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationSurfaceColor(m); }},
        {"guideArtSource", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideFields(m); }},
        {"guideVariant", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideFields(m); }},
        {"guideFilter", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideFields(m); }},
        {"guideGroup", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideFields(m); }},
        {"guideChannels", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideFields(m); }},
        {"styleId", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationStyleId(m); }},
        {"guideSurface", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideFields(m); }},
        {"guideStart", [](const QVariantMap& m, bool, bool) { return validPresentationGuideFields(m); }},
        {"guideOverlayPosition", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideOverlay(m); }},
        {"guideOverlayWidth", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideOverlay(m); }},
        {"guideOverlayHeight", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideOverlay(m); }},
        {"guideOverlayRows", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideOverlay(m); }},
        {"guideRows", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideTiming(m); }},
        {"guideMinutes", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideTiming(m); }},
        {"guideChannelWidth", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideTiming(m); }},
        {"guideLabels", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideBool(m); }},
        {"guideSourceBadges", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationGuideBool(m); }},
        {"imageCrop", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationImageCrop(m); }},
        {"panelPageId", [](const QVariantMap& m, bool s, bool c) {
            const auto pageId = m.value("panelPageId");
            return !s && c && pageId.metaType().id() == QMetaType::QString && safeId(pageId.toString());
        }},
        {"columns", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "columns", s); }},
        {"span", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "span", s); }},
        {"height", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "height", s); }},
        {"progress", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "progress", s); }},
        {"contentWidth", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "contentWidth", s); }},
        {"padding", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "padding", s); }},
        {"panelOpacity", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "panelOpacity", s); }},
        {"blur", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "blur", s); }},
        {"radius", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "radius", s); }},
        {"headingSize", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "headingSize", s); }},
        {"diameter", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "diameter", s); }},
        {"labelSize", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "labelSize", s); }},
        {"iconSize", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "iconSize", s); }},
        {"surfaceOpacity", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "surfaceOpacity", s); }},
        {"cardGap", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "cardGap", s); }},
        {"artHeight", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "artHeight", s); }},
        {"reflection", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBoolFlag(m, "reflection"); }},
        {"libraryHero", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationBoolFlag(m, "libraryHero"); }},
        {"libraryHeroWidth", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "libraryHeroWidth", s); }},
        {"libraryHeroHeight", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(c); return validPresentationNumericField(m, "libraryHeroHeight", s); }},
        {"libraryHeroBackground", [](const QVariantMap& m, bool, bool) { return QStringList{"theme", "fixed", "artwork"}.contains(m.value("libraryHeroBackground").toString()); }},
        {"libraryHeroColor", [](const QVariantMap& m, bool, bool) { return validPresentationSurfaceColor(m); }},
        {"libraryShowLabel", [](const QVariantMap& m, bool, bool) { return validPresentationBoolFlag(m, "libraryShowLabel"); }},
        {"libraryPosterWidth", [](const QVariantMap& m, bool s, bool) { return validPresentationNumericField(m, "libraryPosterWidth", s); }},
        {"libraryPosterHeight", [](const QVariantMap& m, bool s, bool) { return validPresentationNumericField(m, "libraryPosterHeight", s); }},
        {"libraryShelfHeight", [](const QVariantMap& m, bool s, bool) { return validPresentationNumericField(m, "libraryShelfHeight", s); }},
        {"reflectionHeight", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "reflectionHeight", s); }},
        {"reflectionOpacity", [](const QVariantMap& m, bool s, bool c) { Q_UNUSED(s); Q_UNUSED(c); return validPresentationNumericField(m, "reflectionOpacity", s); }}};
    auto it = handlers.find(key);
    if (it != handlers.cend())
        return it.value()(presentationMap, section, container);
    return false;
}

bool
ComponentSchema::validPresentationHistoryApps(const QVariantMap& value, bool section)
{
    const auto v = value.value("historyApps");
    if (!section || v.metaType().id() != QMetaType::QVariantList || v.toList().size() > 50)
        return false;
    QSet<QString> seen;
    for (const auto& app : v.toList())
    {
        if (app.metaType().id() != QMetaType::QString || !safeId(app.toString()) || seen.contains(app.toString()))
            return false;
        seen.insert(app.toString());
    }
    return true;
}

bool
ComponentSchema::validPresentationHistorySourceBadges(const QVariantMap& value, bool section)
{
    return !section || value.value("historySourceBadges").metaType().id() == QMetaType::Bool;
}

bool
ComponentSchema::validPresentationStates(const QVariantMap& states)
{
    static const QSet<QString> stateKeys{"normal", "focused", "selected", "pressed", "disabled", "unavailable"};
    static const QSet<QString> fieldKeys{"surfaceColor", "surfaceOpacity", "labelColor", "iconColor",
                                         "focusColor", "borderColor", "borderWidth", "focusWidth", "radius",
                                         "labelSize", "labelBold", "labelSurfaceColor"};
    for (auto state = states.cbegin(); state != states.cend(); ++state)
    {
        if (!stateKeys.contains(state.key()) || state.value().metaType().id() != QMetaType::QVariantMap)
            return false;
        const auto fields = state.value().toMap();
        for (auto f = fields.cbegin(); f != fields.cend(); ++f)
            if (!fieldKeys.contains(f.key()))
                return false;
        if (!validPresentation(fields))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationLayout(const QVariantMap& layout, bool container)
{
    return container && validChildLayout(layout);
}

bool
ComponentSchema::validPresentationGrid(const QVariantMap& grid, bool section)
{
    return section && validGrid(grid) && grid.value("mode", "screen") == "screen";
}

bool
ComponentSchema::validPresentationVariant(const QVariantMap& v)
{
    return QStringList{"banner", "media", "source", "summary", "launcher", "orb", "recent", "library", "text",
                       "image"}.contains(v.value("variant").toString());
}

bool
ComponentSchema::validPresentationFill(const QVariantMap& v)
{
    // Called per-key; the key is encoded in the map's single entry.
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString)
            return false;
        if (key == "fillMode")
        {
            if (!QStringList{"solid", "gradient"}.contains(val.toString()))
                return false;
        }
        else if (key == "fillDirection")
        {
            if (!QStringList{"vertical", "horizontal"}.contains(val.toString()))
                return false;
        }
    }
    return true;
}

bool
ComponentSchema::validPresentationShape(const QVariantMap& v)
{
    return v.value("shape").metaType().id() == QMetaType::QString &&
           QStringList{"rectangle", "rounded", "ellipse", "pill", "divider"}.contains(v.value("shape").toString());
}

bool
ComponentSchema::validPresentationSourceMaximum(const QVariantMap& v)
{
    const auto val = v.value("sourceMaximum");
    if (val.metaType().id() == QMetaType::QString || val.metaType().id() == QMetaType::Bool)
        return false;
    const auto n = val.toDouble();
    return std::isfinite(n) && std::floor(n) == n && n >= 1 && n <= 6;
}

bool
ComponentSchema::validPresentationSourceVariant(const QVariantMap& v)
{
    return v.value("sourceVariant").metaType().id() == QMetaType::QString &&
           QStringList{"compact", "list", "cards"}.contains(v.value("sourceVariant").toString());
}

bool
ComponentSchema::validPresentationBoolFlag(const QVariantMap& v, const QString& key)
{
    return v.value(key).metaType().id() == QMetaType::Bool;
}

bool
ComponentSchema::validPresentationStringField(const QVariantMap& v, const QString& key)
{
    const auto val = v.value(key);
    if (val.metaType().id() != QMetaType::QString)
        return false;
    const auto text = val.toString();
    return text.size() <= (key == "bodyText" ? 4096 : 160) && !text.contains(QChar::Null);
}

bool
ComponentSchema::validPresentationIconStyle(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString)
            return false;
        if (key == "appIconStyle")
        {
            if (!QStringList{"signature", "tiles", "rail"}.contains(val.toString()))
                return false;
        }
        else if (key == "iconFocus")
        {
            if (!QStringList{"glow", "frame"}.contains(val.toString()))
                return false;
        }
        else if (key == "iconBacking")
        {
            if (!QStringList{"auto", "none", "surface"}.contains(val.toString()))
                return false;
        }
    }
    return true;
}

bool
ComponentSchema::validPresentationIconGlow(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (val.metaType().id() == QMetaType::QString || val.metaType().id() == QMetaType::Bool)
            return false;
        bool ok = false;
        const auto n = val.toDouble(&ok);
        if (!ok || !std::isfinite(n) || n < 0 ||
            n > (key == "iconGlowOpacity" ? 1 : key == "iconPadding" ? 40 : 32))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationArtFit(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString)
            return false;
        QStringList choices;
        if (key == "logoMode")
            choices = {"cutout", "original", "tile", "monochrome"};
        else if (key == "artFit")
            choices = {"cover", "contain"};
        else if (key == "labelPosition")
            choices = {"inside", "below", "overlay", "hidden"};
        else
            choices = {"left", "center", "right"};
        if (!choices.contains(val.toString()))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationBodyAlign(const QVariantMap& v)
{
    return v.value("bodyAlign").metaType().id() == QMetaType::QString &&
           QStringList{"left", "center", "right"}.contains(v.value("bodyAlign").toString());
}

bool
ComponentSchema::validPresentationBodyColor(const QVariantMap& v)
{
    const auto val = v.value("bodyColor");
    if (val.metaType().id() != QMetaType::QString)
        return false;
    const QColor color(val.toString());
    return color.isValid() && color.alpha() == 255;
}

bool
ComponentSchema::validPresentationBodyMetrics(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        const double low = key == "bodySize"      ? 8
                           : key == "bodyPadding" ? 0
                                                  : 1;
        const double high = key == "bodySize"      ? 160
                            : key == "bodyPadding" ? 100
                            : key == "bodyLines"   ? 40
                                                   : 3;
        if (val.metaType().id() == QMetaType::QString || val.metaType().id() == QMetaType::Bool)
            return false;
        bool ok = false;
        const auto n = val.toDouble(&ok);
        if (!ok || !std::isfinite(n) || n < low || n > high ||
            (key == "bodyLines" && std::floor(n) != n))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationBoolToggle(const QVariantMap& value, bool section, bool container)
{
    for (auto it = value.cbegin(); it != value.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (key == "animateArtwork" && (section || container))
            return false;
        if (val.metaType().id() != QMetaType::Bool)
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationArtFocal(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        if (!validPresentationArtFocalType(it.value()))
            return false;
        bool ok = false;
        const auto n = it.value().toDouble(&ok);
        double minimum = 0, maximum = 1;
        if (!validPresentationArtFocalBounds(it.key(), n, minimum, maximum))
            return false;
        if (!ok || !std::isfinite(n) || n < minimum || n > maximum)
            return false;
        if (!validPresentationArtFocalInteger(it.key(), n))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationArtFocalType(const QVariant& val)
{
    return val.metaType().id() != QMetaType::QString && val.metaType().id() != QMetaType::Bool;
}

bool
ComponentSchema::validPresentationArtFocalBounds(const QString& key, double n, double& minimum,
                                                 double& maximum)
{
    Q_UNUSED(n);
    minimum = key == "artZoom" || key == "labelLines" || key == "libraryLimit" ? 1 : 0;
    maximum = key == "artZoom"                              ? 8
                        : key == "focusWidth" || key == "borderWidth" ? 12
                        : key == "labelLines"                         ? 4
                        : key == "libraryLimit"                       ? 50
                                                                      : 1;
    return true;
}

bool
ComponentSchema::validPresentationArtFocalInteger(const QString& key, double n)
{
    return !(key == "labelLines" || key == "libraryLimit") || std::floor(n) == n;
}

bool
ComponentSchema::validPresentationBorderColor(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString)
            return false;
        const QColor color(val.toString());
        if (!color.isValid() || color.alpha() != 255)
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationLibraryFields(const QVariantMap& v, bool section)
{
    if (!section)
        return false;
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        if (!validPresentationLibraryField(it.key(), it.value()))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationLibraryField(const QString& key, const QVariant& val)
{
    if (key == "libraryKind")
        return validPresentationLibraryKind(val);
    if (key == "libraryView")
        return validPresentationLibraryView(val);
    if (key == "libraryLayout")
        return validPresentationLibraryLayout(val);
    if (key == "libraryCardStyle")
        return validPresentationLibraryCardStyle(val);
    if (key == "libraryValue")
        return validPresentationLibraryValue(val);
    if (key == "libraryColumns")
        return validPresentationLibraryColumns(val);
    return false;
}

bool
ComponentSchema::validPresentationLibraryKind(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"movies", "shows", "history", "albums", "channels", "games"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationLibraryView(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"all", "recent", "watched", "continue", "search", "favorites",
                        "platform", "tag", "genre", "collection", "playlist"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationLibraryLayout(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"grid", "list", "row"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationLibraryCardStyle(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"media", "recent", "orb", "source", "summary", "banner"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationLibraryValue(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString && val.toString().size() <= 160;
}

bool
ComponentSchema::validPresentationLibraryColumns(const QVariant& val)
{
    return val.metaType().id() != QMetaType::QString && val.metaType().id() != QMetaType::Bool &&
           val.toDouble() >= 1 && val.toDouble() <= 12 && std::floor(val.toDouble()) == val.toDouble();
}

bool
ComponentSchema::validPresentationShortString(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString || val.toString().size() > 160)
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationSurfaceColor(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString)
            return false;
        const QColor color(val.toString());
        if (!color.isValid() || color.alpha() != 255)
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationGuideFields(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        if (!validPresentationGuideField(it.key(), it.value()))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationGuideField(const QString& key, const QVariant& val)
{
    if (val.metaType().id() != QMetaType::QString)
        return false;
    if (key == "guideArtSource")
        return validPresentationGuideArtSource(val);
    if (key == "guideVariant")
        return validPresentationGuideVariant(val);
    if (key == "guideFilter")
        return validPresentationGuideFilter(val);
    if (key == "guideGroup")
        return validPresentationGuideGroup(val);
    if (key == "guideChannels")
        return validPresentationGuideChannels(val);
    if (key == "guideSurface")
        return validPresentationGuideSurface(val);
    if (key == "guideStart")
        return val.toString() == "manage";
    return false;
}

bool
ComponentSchema::validPresentationGuideArtSource(const QVariant& val)
{
    return QStringList{"program", "station", "none"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationGuideVariant(const QVariant& val)
{
    return QStringList{"mini", "now-playing", "on-now", "favorites", "preview", "upcoming",
                       "guide"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationGuideFilter(const QVariant& val)
{
    return QStringList{"all", "local", "connected", "favorites"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationGuideGroup(const QVariant& val)
{
    return val.toString().size() <= 80;
}

bool
ComponentSchema::validPresentationGuideChannels(const QVariant& val)
{
    if (val.toString().size() > 8192)
        return false;
    if (val.toString().trimmed().isEmpty())
        return true;
    const auto ids = val.toString().split(',');
    if (ids.size() > 128)
        return false;
    QSet<QString> seen;
    for (const auto& id : ids)
    {
        const auto trimmed = id.trimmed();
        if (!safeId(trimmed) || seen.contains(trimmed))
            return false;
        seen.insert(trimmed);
    }
    return true;
}

bool
ComponentSchema::validPresentationGuideSurface(const QVariant& val)
{
    return QStringList{"full", "overlay"}.contains(val.toString());
}

bool
ComponentSchema::validPresentationStyleId(const QVariantMap& v)
{
    return v.value("styleId").metaType().id() == QMetaType::QString &&
           safeId(v.value("styleId").toString());
}

bool
ComponentSchema::validPresentationResponsive(const QVariantMap& value, bool section)
{
    if (!section)
        return false;
    const auto v = value.value("responsive");
    return v.metaType().id() == QMetaType::QVariantMap &&
           DesignSystem::validResponsive(v.toMap());
}

bool
ComponentSchema::validPresentationGuideOverlay(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (key == "guideOverlayPosition")
        {
            if (val.metaType().id() != QMetaType::QString ||
                !QStringList{"top-left", "top-center", "top-right", "center-left", "center",
                             "center-right", "bottom-left", "bottom-center", "bottom-right"}.contains(val.toString()))
                return false;
        }
        else
        {
            if (val.metaType().id() == QMetaType::QString || val.metaType().id() == QMetaType::Bool)
                return false;
            const int low = key == "guideOverlayRows" ? 3 : 65;
            const int high = key == "guideOverlayRows" ? 5 : 100;
            bool ok = false;
            const auto n = val.toDouble(&ok);
            if (!ok || !std::isfinite(n) || n < low || n > high || n != std::floor(n))
                return false;
        }
    }
    return true;
}

bool
ComponentSchema::validPresentationGuideTiming(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (val.metaType().id() != QMetaType::QString && val.metaType().id() != QMetaType::Bool)
        {
            bool ok = false;
            const auto n = val.toDouble(&ok);
            const int low = key == "guideRows"      ? 3
                            : key == "guideMinutes" ? 30
                                                    : 120;
            const int high = key == "guideRows"      ? 12
                             : key == "guideMinutes" ? 240
                                                     : 400;
            if (!ok || !std::isfinite(n) || n < low || n > high || n != std::floor(n))
                return false;
        }
        else
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationGuideBool(const QVariantMap& v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        if (it.value().metaType().id() != QMetaType::Bool)
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationLibraryChrome(const QVariantMap& value, bool section)
{
    if (!section)
        return false;
    return value.value("libraryChrome").metaType().id() == QMetaType::Bool;
}

bool
ComponentSchema::validPresentationHeadingFlags(const QVariantMap& value, bool section, bool container)
{
    for (auto it = value.cbegin(); it != value.cend(); ++it)
    {
        const auto key = it.key();
        const auto val = it.value();
        if (key == "showHeading" && !section && !container)
            return false;
        if (val.metaType().id() != QMetaType::Bool)
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationTrailingAction(const QVariantMap& value, bool section)
{
    if (!section)
        return false;
    return !action(value.value("trailingAction").toMap(), nullptr).isEmpty();
}

bool
ComponentSchema::validPresentationImageCrop(const QVariantMap& v)
{
    const auto crop = v.value("imageCrop").toMap();
    if (crop.size() != 4 || !crop.contains("x") || !crop.contains("y") || !crop.contains("width") ||
        !crop.contains("height"))
        return false;
    for (auto c = crop.cbegin(); c != crop.cend(); ++c)
    {
        bool ok;
        const double number = c.value().toDouble(&ok);
        if (!ok || !std::isfinite(number) || number < 0 || number > 16384 ||
            ((c.key() == "width" || c.key() == "height") && number < 1))
            return false;
    }
    return true;
}

bool
ComponentSchema::validPresentationNumericField(const QVariantMap& v, const QString& key, bool section)
{
    const auto val = v.value(key);
    if (val.metaType().id() == QMetaType::QString || val.metaType().id() == QMetaType::Bool)
        return false;
    bool ok;
    const double number = val.toDouble(&ok);
    if (!ok || !std::isfinite(number))
        return false;
    if (key == "columns")
        return validPresentationNumericColumns(number, section);
    if (key == "span")
        return validPresentationNumericSpan(number, section);
    if (key == "height")
        return validPresentationNumericHeight(number, section);
    if (key == "progress")
        return validPresentationNumericProgress(number, section);
    if (QStringList{"panelOpacity", "blur", "surfaceOpacity"}.contains(key))
        return validPresentationNumericOpacity(number);
    if (QStringList{"padding", "radius", "headingSize", "diameter", "labelSize", "iconSize", "cardGap",
                    "artHeight"}.contains(key))
        return validPresentationNumericSize(number);
    if (key == "reflectionHeight")
        return number >= 0 && number <= 400;
    if (key == "libraryHeroWidth")
        return number >= 40 && number <= 100;
    if (key == "libraryHeroHeight")
        return number >= 24 && number <= 100;
    if (key == "libraryPosterWidth")
        return number == 0 || (number >= 64 && number <= 480);
    if (key == "libraryPosterHeight")
        return number == 0 || (number >= 80 && number <= 600);
    if (key == "libraryShelfHeight")
        return number == 0 || (number >= 96 && number <= 720);
    if (key == "reflectionOpacity")
        return number >= 0 && number <= 1;
    if (key == "contentWidth")
        return validPresentationNumericContentWidth(number, section);
    return false;
}

bool
ComponentSchema::validPresentationNumericColumns(double number, bool section)
{
    return section && number >= 1 && number <= 12 && std::floor(number) == number;
}

bool
ComponentSchema::validPresentationNumericSpan(double number, bool section)
{
    return section && number >= 1 && number <= 2 && std::floor(number) == number;
}

bool
ComponentSchema::validPresentationNumericHeight(double number, bool section)
{
    return section && number >= 60 && number <= 800;
}

bool
ComponentSchema::validPresentationNumericProgress(double number, bool section)
{
    return !section && number >= 0 && number <= 1;
}

bool
ComponentSchema::validPresentationNumericOpacity(double number)
{
    return number >= 0 && number <= 1;
}

bool
ComponentSchema::validPresentationNumericSize(double number)
{
    return number >= 0 && number <= 400;
}

bool
ComponentSchema::validPresentationNumericContentWidth(double number, bool section)
{
    return section && number >= 100 && number <= 3840;
}

bool
ComponentSchema::validSettingsNavigation(const QVariantMap& value)
{
    if (!validSettingsNavigationVersion(value))
        return false;
    const QSet<QString> categoryIds{
        "network", "picture", "home", "apps", "inputs", "remotes", "profiles", "media", "system", "display-skins"};
    const QSet<QString> routes{"network",
                               "vpn",
                               "display",
                               "audio",
                               "interface-sounds",
                               "studio",
                               "ambient",
                               "store",
                               "store-preferences",
                               "capture",
                               "sources",
                               "remotes",
                               "profiles",
                               "media",
                               "system",
                               "skins"};
    const auto categories = value.value("categories").toList();
    if (categories.size() != categoryIds.size())
        return false;
    QSet<QString> seen, seenRoutes;
    for (const auto& entry : categories)
    {
        if (!validSettingsNavigationCategory(entry.toMap(), seen, seenRoutes))
            return false;
    }
    return seen == categoryIds && seenRoutes == routes;
}

// Navigation documents have exactly one numeric version and a category list.
bool
ComponentSchema::validSettingsNavigationVersion(const QVariantMap& value)
{
    return value.size() == 2 && QJsonValue::fromVariant(value.value("schemaVersion")).isDouble() &&
           value.value("schemaVersion").toDouble() == 1 &&
           value.value("categories").metaType().id() == QMetaType::QVariantList;
}

/**
 * Validates settings navigation schemaVersion (numeric 1).
 * Validates settings navigation categories list (exact set of 10 categories).
 */
bool
ComponentSchema::validSettingsNavigationCategories(const QVariantMap& value)
{
    return value.size() == 2 &&
           value.value("schemaVersion").metaType().id() != QMetaType::QString &&
           value.value("schemaVersion").metaType().id() != QMetaType::Bool &&
           value.value("schemaVersion").toDouble() == 1 &&
           value.value("categories").metaType().id() == QMetaType::QVariantList;
}

/**
 * Validates a single settings navigation category (id, name, description,
 * rows or route).
 */
bool
ComponentSchema::validSettingsNavigationCategory(const QVariantMap& category, QSet<QString>& seen,
                                                 QSet<QString>& seenRoutes)
{
    static const QSet<QString> categoryIds{
        "network", "picture", "home", "apps", "inputs", "remotes", "profiles", "media", "system", "display-skins"};
    const auto id = category.value("id").toString();
    if (category.size() != 4 || !categoryIds.contains(id) || seen.contains(id) ||
        !validSettingsNavigationLabel(category.value("name"), 48) ||
        !validSettingsNavigationLabel(category.value("description"), 160))
        return false;
    seen.insert(id);
    QVariantList rows;
    if (category.contains("route"))
        rows.append(QVariantMap{{"name", category.value("name")},
                                {"description", category.value("description")},
                                {"route", category.value("route")}});
    else
    {
        if (category.value("rows").metaType().id() != QMetaType::QVariantList)
            return false;
        rows = category.value("rows").toList();
        if (rows.isEmpty() || rows.size() > 8)
            return false;
    }
    static const QSet<QString> routes{"network",
                                      "vpn",
                                      "display",
                                      "audio",
                                      "interface-sounds",
                                      "studio",
                                      "ambient",
                                      "store",
                                      "store-preferences",
                                      "capture",
                                      "sources",
                                      "remotes",
                                      "profiles",
                                      "media",
                                      "system",
                                      "skins"};
    for (const auto& rowValue : rows)
    {
        if (!validSettingsNavigationRow(rowValue.toMap(), seenRoutes))
            return false;
    }
    return true;
}

/**
 * Validates a settings navigation row (name, description, unique route).
 */
bool
ComponentSchema::validSettingsNavigationRow(const QVariantMap& row, QSet<QString>& seenRoutes)
{
    const auto route = row.value("route").toString();
    static const QSet<QString> routes{
        "network", "vpn", "display", "audio", "interface-sounds", "studio", "ambient", "store",
        "store-preferences", "capture", "sources", "remotes", "profiles", "media", "system", "skins"};
    if (row.size() != 3 || !validSettingsNavigationLabel(row.value("name"), 48) ||
        !validSettingsNavigationLabel(row.value("description"), 160) || !routes.contains(route) ||
        seenRoutes.contains(route))
        return false;
    seenRoutes.insert(route);
    return true;
}

/**
 * Validates a settings navigation label (non-empty, bounded size).
 */
bool
ComponentSchema::validSettingsNavigationLabel(const QVariant& label, int limit)
{
    return label.metaType().id() == QMetaType::QString && !label.toString().trimmed().isEmpty() &&
           label.toString().size() <= limit && !label.toString().contains(QChar::Null);
}

bool
ComponentSchema::validComposition(const QVariantMap& value)
{
    if (value.isEmpty())
        return true;
    return validCompositionKeys(value) &&
           validCompositionOptionalFields(value) &&
           validCompositionLayout(value) &&
           validCompositionGrid(value) &&
           (!value.contains("identity") || validCompositionIdentity(value)) &&
           (!value.contains("headerMode") || validCompositionHeaderModeIdentity(value)) &&
           (!value.contains("headerControls") || validCompositionHeaderControls(value)) &&
           validCompositionScrolling(value) &&
           validCompositionNavigationTabs(value) &&
           validCompositionInitialTab(value);
}

bool
ComponentSchema::validCompositionOptionalFields(const QVariantMap& value)
{
    return (!value.contains("settingsCategoryOrder") ||
            validCompositionSettingsCategoryOrder(value)) &&
           (!value.contains("captureInputArtwork") ||
            validCompositionCaptureInputArtwork(value)) &&
           (!value.contains("appGallery") || validCompositionAppGallery(value)) &&
           (!value.contains("designLibrary") ||
            (value["designLibrary"].metaType().id() == QMetaType::QVariantMap &&
             DesignSystem::valid(value["designLibrary"].toMap())));
}

bool
ComponentSchema::validCompositionLayout(const QVariantMap& value)
{
    if (value.contains("designHeight"))
    {
        const auto heightValue = value.value("designHeight");
        bool ok = false;
        const auto height = heightValue.toDouble(&ok);
        if (!ok || !std::isfinite(height) || heightValue.metaType().id() == QMetaType::QString ||
            heightValue.metaType().id() == QMetaType::Bool || height < MinDesignHeight || height > MaxDesignHeight)
            return false;
    }
    return (!value.contains("header") ||
            (value["header"].metaType().id() == QMetaType::QVariantMap &&
             validCompositionHeader(value["header"].toMap(), value))) &&
           (!value.contains("background") || validCompositionBackground(value)) &&
           (!value.contains("pageNavigation") || validCompositionPageNavigation(value)) &&
           (!value.contains("sidebarWidth") || validCompositionSidebarWidth(value));
}

bool
ComponentSchema::validCompositionGrid(const QVariantMap& value)
{
    return validCompositionMovieShowsPage(value) &&
           validCompositionVersionColumnsGrid(value) &&
           (!value.contains("sectionHeight") || validCompositionSectionHeight(value)) &&
           (!value.contains("wallpaper") || validCompositionWallpaper(value));
}

bool
ComponentSchema::validCompositionScrolling(const QVariantMap& value)
{
    return (!value.value("verticalScrolling").toBool() ||
            validCompositionVerticalScrolling(value)) &&
           validCompositionBoolKeys(value) &&
           validCompositionMarginGap(value);
}

/**
 * Validates composition keys only (known key set).
 * Validates composition header keys only.
 */
bool
ComponentSchema::validCompositionKeys(const QVariantMap& header)
{
    static const QSet<QString> keys{"schemaVersion",
                                    "enabled",
                                    "navigation",
                                    "initialTab",
                                    "margin",
                                    "gap",
                                    "fit",
                                    "showProfile",
                                    "columns",
                                    "wallpaper",
                                    "headerMode",
                                    "identity",
                                    "headerControls",
                                    "showClock",
                                    "designHeight",
                                    "pageNavigation",
                                    "sidebarWidth",
                                    "moviePage",
                                    "showsPage",
                                    "grid",
                                    "sectionHeight",
                                    "background",
                                    "verticalScrolling",
                                    "header",
                                    "designLibrary",
                                    "appGallery",
                                    "captureInputArtwork",
                                    "settingsCategoryOrder"};
    for (auto it = header.cbegin(); it != header.cend(); ++it)
        if (!keys.contains(it.key()))
            return false;
    return true;
}

/**
 * Validates settingsCategoryOrder (exact category set, unique).
 */
bool
ComponentSchema::validCompositionSettingsCategoryOrder(const QVariantMap& value)
{
    const QSet<QString> ids{
        "network", "picture", "home", "apps", "inputs", "remotes", "profiles", "media", "system", "display-skins"};
    const auto order = value.value("settingsCategoryOrder").toList();
    // Accept saved orders from before Display & Skins was consolidated into Skins.
    if (order.size() != ids.size() && order.size() != ids.size() - 1)
        return false;
    QSet<QString> seen;
    for (const auto& id : order)
    {
        if (id.metaType().id() != QMetaType::QString || !ids.contains(id.toString()) ||
            seen.contains(id.toString()))
            return false;
        seen.insert(id.toString());
    }
    return seen.size() == ids.size() || !seen.contains("display-skins");
}

/**
 * Validates captureInputArtwork (bounded artwork map).
 */
bool
ComponentSchema::validCompositionCaptureInputArtwork(const QVariantMap& value)
{
    const auto artwork = value.value("captureInputArtwork");
    if (artwork.metaType().id() != QMetaType::QVariantMap || artwork.toMap().size() > 8)
        return false;
    const QRegularExpression path("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg|gif)$");
    const auto entries = artwork.toMap();
    for (auto it = entries.cbegin(); it != entries.cend(); ++it)
        if (!QStringList{"1", "2", "3", "4", "5", "6", "7", "8"}.contains(it.key()) ||
            it.value().metaType().id() != QMetaType::QString || !path.match(it.value().toString()).hasMatch())
            return false;
    return true;
}

/**
 * Validates appGallery (bounded gallery presentation map).
 */
bool
ComponentSchema::validCompositionAppGallery(const QVariantMap& value)
{
    if (value.value("appGallery").metaType().id() != QMetaType::QVariantMap)
        return false;
    const auto gallery = value.value("appGallery").toMap();
    if (gallery.size() > 19)
        return false;
    for (auto it = gallery.cbegin(); it != gallery.cend(); ++it)
    {
        if (!validCompositionAppGalleryField(it.key(), it.value()))
            return false;
    }
    return true;
}

bool
ComponentSchema::validCompositionAppGalleryField(const QString& key, const QVariant& val)
{
    static const QSet<QString> keys{"layout",
                                    "columns",
                                    "density",
                                    "sort",
                                    "showArtwork",
                                    "showLabels",
                                    "filterEnabled",
                                    "favoritesEnabled",
                                    "favoriteIds",
                                    "orderIds",
                                    "hiddenIds",
                                    "addedIds",
                                    "tileHeight",
                                    "tileGap",
                                    "surfaceOpacity",
                                    "radius",
                                    "surfaceColor",
                                    "focusColor",
                                    "title"};
    if (!keys.contains(key))
        return false;
    if (key == "layout")
        return validCompositionAppGalleryLayout(val);
    if (key == "columns")
        return validCompositionAppGalleryColumns(val);
    if (key == "density")
        return validCompositionAppGalleryDensity(val);
    if (key == "sort")
        return validCompositionAppGallerySort(val);
    if (key == "showArtwork" || key == "showLabels" || key == "filterEnabled" || key == "favoritesEnabled")
        return validCompositionAppGalleryBool(val);
    if (key == "title")
        return validCompositionAppGalleryTitle(val);
    if (key == "favoriteIds" || key == "orderIds" || key == "hiddenIds" || key == "addedIds")
        return validCompositionAppGalleryFavoriteIds(val);
    if (key == "tileHeight")
        return validCompositionHeaderNumber(val, {56, 320});
    if (key == "tileGap" || key == "radius")
        return validCompositionHeaderNumber(val, {0, 48});
    if (key == "surfaceOpacity")
        return validCompositionHeaderNumber(val, {0, 1});
    if (key == "surfaceColor" || key == "focusColor")
        return validCompositionHeaderColor(val);
    return false;
}

bool
ComponentSchema::validCompositionAppGalleryLayout(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"grid", "list"}.contains(val.toString());
}

bool
ComponentSchema::validCompositionAppGalleryColumns(const QVariant& val)
{
    const auto type = val.metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong &&
        type != QMetaType::ULongLong && type != QMetaType::Double)
        return false;
    bool ok = false;
    const double columns = val.toDouble(&ok);
    return ok && std::isfinite(columns) && std::floor(columns) == columns && columns >= 2 && columns <= 8;
}

bool
ComponentSchema::validCompositionAppGalleryDensity(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"compact", "standard", "spacious"}.contains(val.toString());
}

bool
ComponentSchema::validCompositionAppGallerySort(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString &&
           QStringList{"name", "favorites"}.contains(val.toString());
}

bool
ComponentSchema::validCompositionAppGalleryBool(const QVariant& val)
{
    return val.metaType().id() == QMetaType::Bool;
}

bool
ComponentSchema::validCompositionAppGalleryTitle(const QVariant& val)
{
    return val.metaType().id() == QMetaType::QString && !val.toString().trimmed().isEmpty() &&
           val.toString().size() <= 48 && !val.toString().contains(QChar::Null);
}

bool
ComponentSchema::validCompositionAppGalleryFavoriteIds(const QVariant& val)
{
    if (val.metaType().id() != QMetaType::QVariantList || val.toList().size() > 256)
        return false;
    QSet<QString> ids;
    const QRegularExpression idPattern("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$");
    for (const auto& idValue : val.toList())
    {
        if (idValue.metaType().id() != QMetaType::QString ||
            !idPattern.match(idValue.toString()).hasMatch() || ids.contains(idValue.toString()))
            return false;
        ids.insert(idValue.toString());
    }
    return true;
}

/**
 * Validates composition header map with geometry bounds.
 * Validates composition header map with geometry bounds.
 */
bool
ComponentSchema::validCompositionHeader(const QVariantMap& header, const QVariantMap& value)
{
    if (!versionOne(header))
        return false;
    for (auto it = header.cbegin(); it != header.cend(); ++it)
    {
        if (!validCompositionHeaderField(it.key(), it.value()))
            return false;
    }
    return validCompositionHeaderGeometry(header, value);
}

/**
 * Validates composition header field (typed bounds by field name).
 */
bool
ComponentSchema::validCompositionHeaderField(const QString& key, const QVariant& entry)
{
    if (key == "schemaVersion")
        return true;
    static const QMap<QString, QPair<double, double>> numbers{{"height", {42, 180}},
                                                              {"gap", {0, 64}},
                                                              {"padding", {0, 24}},
                                                              {"titleSize", {16, 96}},
                                                              {"clockSize", {12, 48}},
                                                              {"dateSize", {10, 32}},
                                                              {"clockWidth", {100, 320}},
                                                              {"controlSize", {12, 32}},
                                                              {"controlWidth", {64, 240}},
                                                              {"iconSize", {12, 64}},
                                                              {"radius", {0, 80}},
                                                              {"opacity", {0, 1}},
                                                              {"focusWidth", {1, 8}}};
    if (numbers.contains(key))
        return validCompositionHeaderNumber(entry, numbers.value(key));
    if (key == "position")
        return validCompositionHeaderPosition(entry);
    if (key == "showTitle" || key == "showDate")
        return validCompositionHeaderBool(entry);
    if (key == "textColor" || key == "backgroundColor" || key == "focusColor" || key == "clockColor")
        return validCompositionHeaderColor(entry);
    if (key == "customLogo")
        return validCompositionHeaderCustomLogo(entry);
    if (key == "controlDisplay")
        return validCompositionHeaderControlDisplay(entry);
    if (key == "titleAlignment")
        return validCompositionHeaderTitleAlignment(entry);
    if (key == "timeFormat")
        return validCompositionHeaderTimeFormat(entry);
    if (key == "dateFormat")
        return validCompositionHeaderDateFormat(entry);
    if (key == "order")
        return validCompositionHeaderOrder(entry);
    return false;
}

bool
ComponentSchema::validCompositionHeaderNumber(const QVariant& entry, const QPair<double, double>& bounds)
{
    if (entry.metaType().id() == QMetaType::QString || entry.metaType().id() == QMetaType::Bool)
        return false;
    bool ok = false;
    const double n = entry.toDouble(&ok);
    return ok && std::isfinite(n) && n >= bounds.first && n <= bounds.second;
}

bool
ComponentSchema::validCompositionHeaderPosition(const QVariant& entry)
{
    return entry.metaType().id() == QMetaType::QString &&
           QStringList{"top", "bottom"}.contains(entry.toString());
}

bool
ComponentSchema::validCompositionHeaderBool(const QVariant& entry)
{
    return entry.metaType().id() == QMetaType::Bool;
}

bool
ComponentSchema::validCompositionHeaderColor(const QVariant& entry)
{
    if (entry.metaType().id() != QMetaType::QString)
        return false;
    const QColor color(entry.toString());
    return color.isValid() && color.alpha() == 255;
}

bool
ComponentSchema::validCompositionHeaderCustomLogo(const QVariant& entry)
{
    if (entry.metaType().id() != QMetaType::QString)
        return false;
    return QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$")
        .match(entry.toString())
        .hasMatch();
}

bool
ComponentSchema::validCompositionHeaderControlDisplay(const QVariant& entry)
{
    return entry.metaType().id() == QMetaType::QString &&
           QStringList{"both", "icon", "label"}.contains(entry.toString());
}

bool
ComponentSchema::validCompositionHeaderTitleAlignment(const QVariant& entry)
{
    return entry.metaType().id() == QMetaType::QString &&
           QStringList{"left", "center", "right"}.contains(entry.toString());
}

bool
ComponentSchema::validCompositionHeaderTimeFormat(const QVariant& entry)
{
    return entry.metaType().id() == QMetaType::QString &&
           QStringList{"h:mm AP", "HH:mm", "h:mm:ss AP", "HH:mm:ss"}.contains(entry.toString());
}

bool
ComponentSchema::validCompositionHeaderDateFormat(const QVariant& entry)
{
    return entry.metaType().id() == QMetaType::QString &&
           QStringList{"ddd, MMM d", "dddd, MMMM d", "yyyy-MM-dd", "dd/MM/yyyy", "MM/dd/yyyy"}.contains(
               entry.toString());
}

bool
ComponentSchema::validCompositionHeaderOrder(const QVariant& entry)
{
    if (entry.metaType().id() != QMetaType::QVariantList || entry.toList().size() != 3)
        return false;
    QSet<QString> seen;
    for (const auto& part : entry.toList())
    {
        if (part.metaType().id() != QMetaType::QString ||
            !QStringList{"title", "controls", "clock"}.contains(part.toString()) ||
            seen.contains(part.toString()))
            return false;
        seen.insert(part.toString());
    }
    return true;
}

bool
ComponentSchema::validCompositionHeaderGeometry(const QVariantMap& header, const QVariantMap& value)
{
    const auto height = header.value("height", value.value("headerMode") == "identity" ? 58 : 42).toDouble();
    const auto padding = header.value("padding", 0).toDouble();
    if (height - 2 * padding <
        header.value("controlSize", value.value("headerMode") == "identity" ? 14 : 15).toDouble() + 8)
        return false;
    if (value.value("headerMode") == "identity" && header.value("showTitle", true).toBool() &&
        height - 2 * padding < header.value("titleSize", 48).toDouble())
        return false;
    if (value.value("showClock").toBool() &&
        height - 2 * padding <
            header.value("clockSize", 18).toDouble() +
                (header.value("showDate", true).toBool() ? header.value("dateSize", 12).toDouble() + 3 : 0))
        return false;
    return true;
}

bool
ComponentSchema::validCompositionBackground(const QVariantMap& value)
{
    const auto background = value["background"].toMap();
    if (value["background"].metaType().id() != QMetaType::QVariantMap ||
        !validCompositionBackgroundKeys(background) || !validCompositionBackgroundMode(background))
        return false;
    auto art = background;
    art.remove("mode");
    const auto dim = art.take("dim");
    const auto tint = art.take("tintColor");
    if (tint.isValid() && !validCompositionBackgroundTint(tint))
        return false;
    if (dim.isValid() && !validCompositionBackgroundDim(dim))
        return false;
    return validPresentation(art);
}

bool
ComponentSchema::validCompositionBackgroundKeys(const QVariantMap& background)
{
    for (auto it = background.cbegin(); it != background.cend(); ++it)
        if (!QStringList{"mode",
                         "dim",
                         "artFit",
                         "artFocalX",
                         "artFocalY",
                         "artZoom",
                         "artAspectLocked",
                         "imageCrop",
                         "tintColor"}.contains(it.key()))
            return false;
    return true;
}

bool
ComponentSchema::validCompositionBackgroundMode(const QVariantMap& background)
{
    return QStringList{"solid", "gradient", "image"}.contains(background.value("mode").toString());
}

bool
ComponentSchema::validCompositionBackgroundTint(const QVariant& tint)
{
    return tint.metaType().id() == QMetaType::QString && QColor(tint.toString()).isValid() &&
           QColor(tint.toString()).alpha() == 255;
}

bool
ComponentSchema::validCompositionBackgroundDim(const QVariant& dim)
{
    return dim.metaType().id() != QMetaType::QString && dim.metaType().id() != QMetaType::Bool &&
           std::isfinite(dim.toDouble()) && dim.toDouble() >= 0 && dim.toDouble() <= 1;
}

bool
ComponentSchema::validCompositionPageNavigation(const QVariantMap& value)
{
    return QStringList{"left", "top", "none"}.contains(value["pageNavigation"].toString());
}

bool
ComponentSchema::validCompositionSidebarWidth(const QVariantMap& value)
{
    const auto v = value["sidebarWidth"];
    return v.metaType().id() != QMetaType::QString && v.toDouble() >= 120 && v.toDouble() <= 400;
}

bool
ComponentSchema::validCompositionMovieShowsPage(const QVariantMap& value)
{
    for (const auto& key : {"moviePage", "showsPage"})
        if (value.contains(key) && !safeId(value[key].toString()))
            return false;
    return true;
}

bool
ComponentSchema::validCompositionVersionColumnsGrid(const QVariantMap& value)
{
    if (value.value("schemaVersion").toDouble() != 1)
        return false;
    if (value.contains("columns") && value["columns"].toDouble() != 1 && value["columns"].toDouble() != 2)
        return false;
    if (value.contains("grid") &&
        (value["grid"].metaType().id() != QMetaType::QVariantMap || !validGrid(value["grid"].toMap())))
        return false;
    // A sections grid needs exactly one column, no fit scaling and an explicit section height.
    if (value.value("grid").toMap().value("mode") == "sections" &&
        (value.value("columns", 1).toInt() != MinHomeLayoutColumns || value.value("fit", false).toBool() ||
         !value.contains("sectionHeight")))
        return false;
    return true;
}

bool
ComponentSchema::validCompositionSectionHeight(const QVariantMap& value)
{
    const auto v = value["sectionHeight"];
    return v.metaType().id() != QMetaType::QString && v.metaType().id() != QMetaType::Bool &&
           std::isfinite(v.toDouble()) && v.toDouble() >= MinSectionHeight && v.toDouble() <= MaxSectionHeight;
}

bool
ComponentSchema::validCompositionWallpaper(const QVariantMap& value)
{
    return QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$")
        .match(value["wallpaper"].toString())
        .hasMatch();
}

bool
ComponentSchema::validCompositionHeaderModeIdentity(const QVariantMap& value)
{
    return QStringList{"navigation", "identity"}.contains(value["headerMode"].toString());
}

bool
ComponentSchema::validCompositionIdentity(const QVariantMap& value)
{
    const auto v = value["identity"];
    return v.metaType().id() == QMetaType::QString && v.toString().size() <= MaxNameLength &&
           !v.toString().contains(QChar::Null);
}

bool
ComponentSchema::validCompositionHeaderControls(const QVariantMap& value)
{
    const auto controls = value["headerControls"].toList();
    QSet<QString> unique;
    if (controls.isEmpty() || controls.size() > MaxHeaderControls ||
        value["headerControls"].metaType().id() != QMetaType::QVariantList)
        return false;
    for (const auto& control : controls)
    {
        const auto id = control.toString();
        if (!QStringList{"sources", "search", "settings", "edit"}.contains(id) || unique.contains(id))
            return false;
        unique.insert(id);
    }
    return unique.contains("settings") || unique.contains("edit");
}

bool
ComponentSchema::validCompositionVerticalScrolling(const QVariantMap& value)
{
    return value.value("enabled").toBool() && value.contains("grid") &&
           normalizedGrid(value.value("grid").toMap()).value("mode") == "screen";
}

bool
ComponentSchema::validCompositionBoolKeys(const QVariantMap& value)
{
    for (const auto& key : {"enabled", "fit", "showProfile", "showClock", "verticalScrolling"})
        if (value.contains(key) && value[key].metaType().id() != QMetaType::Bool)
            return false;
    return true;
}

bool
ComponentSchema::validCompositionMarginGap(const QVariantMap& value)
{
    for (const auto& key : {"margin", "gap"})
        if (value.contains(key))
        {
            bool ok;
            const double number = value[key].toDouble(&ok);
            if (!ok || !std::isfinite(number) || number < MinChildGapOrPadding || number > MaxChildGapOrPadding)
                return false;
        }
    return true;
}

bool
ComponentSchema::validCompositionNavigationTabs(const QVariantMap& value)
{
    if (!validCompositionNavigationTabsType(value))
        return false;
    const auto tabs = value.value("navigation").toList();
    QSet<QString> ids;
    for (const auto& entry : tabs)
        if (!validNavigationTab(entry.toMap(), ids))
            return false;
    return true;
}

bool
ComponentSchema::validCompositionNavigationTabsType(const QVariantMap& value)
{
    if (!value.contains("navigation"))
        return true;
    const auto tabs = value.value("navigation");
    return tabs.toList().size() <= MaxNavigationTabs &&
           tabs.metaType().id() == QMetaType::QVariantList;
}

bool
ComponentSchema::validNavigationTab(const QVariantMap& tab, QSet<QString>& ids)
{
    const auto id = tab.value("id").toString();
    if (!safeId(id) || ids.contains(id) || tab.value("name").toString().trimmed().isEmpty() ||
        tab.value("name").toString().size() > MaxNameLength)
        return false;
    ids.insert(id);
    return validNavigationTabKeys(tab) && validNavigationTabActions(tab) &&
           validNavigationTabSections(tab);
}

bool
ComponentSchema::validNavigationTabKeys(const QVariantMap& tab)
{
    for (auto it = tab.cbegin(); it != tab.cend(); ++it)
        if (!QStringList{"id", "name", "action", "actions", "sections"}.contains(it.key()))
            return false;
    return true;
}

bool
ComponentSchema::validNavigationTabActions(const QVariantMap& tab)
{
    // Legacy tabs carry one action. Keep a single unambiguous dispatch representation per tab.
    if (tab.contains("action"))
        return !tab.contains("actions") && !action(tab.value("action").toMap()).isEmpty();
    if (!tab.contains("actions"))
        return true;
    const auto list = tab["actions"].toList();
    return !actions(list).isEmpty();
}

bool
ComponentSchema::validNavigationTabSections(const QVariantMap& tab)
{
    if (!tab.contains("sections"))
        return true;
    if (tab["sections"].metaType().id() != QMetaType::QVariantList)
        return false;
    for (const auto& category : tab["sections"].toList())
        if (!safeId(category.toString()))
            return false;
    return true;
}

bool
ComponentSchema::validCompositionInitialTab(const QVariantMap& value)
{
    QSet<QString> ids;
    const auto tabs = value.value("navigation").toList();
    for (const auto& entry : tabs)
        ids.insert(entry.toMap().value("id").toString());
    return !value.contains("initialTab") || ids.contains(value["initialTab"].toString());
}

bool
ComponentSchema::validGrid(const QVariantMap& value)
{
    if (value.isEmpty())
        return true;
    if (!validGridKeys(value))
        return false;
    if (!validGridMode(value))
        return false;
    if (value.contains("columns") && !validGridColumnsRows(value, "columns"))
        return false;
    if (value.contains("rows") && !validGridColumnsRows(value, "rows"))
        return false;
    if (!validGridGapMargin(value))
        return false;
    return validGridBoolKeys(value);
}

bool
ComponentSchema::validGridKeys(const QVariantMap& value)
{
    static const QSet<QString> keys{"columns", "rows", "gap", "margin", "snap", "overlap", "mode"};
    for (auto it = value.cbegin(); it != value.cend(); ++it)
        if (!keys.contains(it.key()))
            return false;
    return true;
}

bool
ComponentSchema::validGridMode(const QVariantMap& value)
{
    return !value.contains("mode") ||
           QStringList{"screen", "sections"}.contains(value["mode"].toString());
}

bool
ComponentSchema::validGridColumnsRows(const QVariantMap& value, const QString& key)
{
    bool ok;
    const double number = value.value(key).toDouble(&ok);
    const double minimum = key == "columns" ? MinGridColumns : MinGridRows;
    const double maximum = key == "columns" ? MaxGridColumns : MaxGridRows;
    return ok && value.value(key).metaType().id() != QMetaType::QString &&
           value.value(key).metaType().id() != QMetaType::Bool && std::isfinite(number) &&
           std::floor(number) == number && number >= minimum && number <= maximum;
}

bool
ComponentSchema::validGridGapMargin(const QVariantMap& value)
{
    for (const auto& key : {"gap", "margin"})
        if (value.contains(key))
        {
            bool ok;
            const double number = value.value(key).toDouble(&ok);
            if (!ok || !std::isfinite(number) || value.value(key).metaType().id() == QMetaType::QString ||
                value.value(key).metaType().id() == QMetaType::Bool || number < MinChildGapOrPadding ||
                number > MaxChildGapOrPadding)
                return false;
        }
    return true;
}

bool
ComponentSchema::validGridBoolKeys(const QVariantMap& value)
{
    for (const auto& key : {"snap", "overlap"})
        if (value.contains(key) && value.value(key).metaType().id() != QMetaType::Bool)
            return false;
    return true;
}

QVariantMap
ComponentSchema::normalizedGrid(const QVariantMap& value)
{
    // Screen-mode default grid: 8 x 5 cells with a 12 gap and 20 margin.
    QVariantMap grid{{"columns", DefaultGridColumns},
                     {"rows", DefaultGridRows},
                     {"gap", DefaultGridGap},
                     {"margin", DefaultGridMargin},
                     {"snap", true},
                     {"overlap", false},
                     {"mode", "screen"}};
    for (auto it = value.cbegin(); it != value.cend(); ++it)
        grid[it.key()] = it.value();
    return grid;
}
bool
ComponentSchema::validPlacement(const QVariantMap& value, int maxColumns, int maxRows)
{
    if (!validPlacementKeys(value))
        return false;
    for (auto it = value.cbegin(); it != value.cend(); ++it)
        if (!validPlacementDimension(value, it.key(), maxColumns, maxRows))
            return false;
    const int x = value.value("x").toInt(), y = value.value("y").toInt();
    const int width = value.value("width").toInt(), height = value.value("height").toInt();
    return x >= 0 && y >= 0 && width >= 1 && height >= 1 && x + width <= maxColumns && y + height <= maxRows;
}

bool
ComponentSchema::validPlacementKeys(const QVariantMap& value)
{
    static const QSet<QString> keys{"x", "y", "width", "height"};
    if (value.size() != 4)
        return false;
    for (auto it = value.cbegin(); it != value.cend(); ++it)
        if (!keys.contains(it.key()))
            return false;
    return true;
}

bool
ComponentSchema::validPlacementDimension(const QVariantMap& value, const QString& key,
                                         int maxColumns, int maxRows)
{
    bool ok;
    const double number = value.value(key).toDouble(&ok);
    const double maximum = key == "x" || key == "width" ? maxColumns : maxRows;
    return ok && value.value(key).metaType().id() != QMetaType::QString &&
           value.value(key).metaType().id() != QMetaType::Bool && std::isfinite(number) &&
           std::floor(number) == number && number >= 0 && number <= maximum;
}

bool
ComponentSchema::placementsOverlap(const QVariantMap& a, const QVariantMap& b)
{
    return a.value("x").toInt() < b.value("x").toInt() + b.value("width").toInt() &&
           b.value("x").toInt() < a.value("x").toInt() + a.value("width").toInt() &&
           a.value("y").toInt() < b.value("y").toInt() + b.value("height").toInt() &&
           b.value("y").toInt() < a.value("y").toInt() + a.value("height").toInt();
}
QVariantMap
ComponentSchema::gameRegistration(const QVariantMap& input, QString* error)
{
    if (error)
        error->clear();
    if (!gameRegistrationKeys(input) || !gameRegistrationVersion(input) || !gameRegistrationTextFields(input) ||
        !gameRegistrationTitlePlatform(input))
        return reject(error, "Choose a title, platform and valid game runtime.");
    if (input.value("runtimeId") == "steam")
        return gameRegistrationSteam(input, error) ? input : QVariantMap();
    return gameRegistrationRootTarget(input) ? input : QVariantMap();
}

bool
ComponentSchema::gameRegistrationKeys(const QVariantMap& input)
{
    static const QSet<QString> keys{"schemaVersion", "title", "platform", "runtimeId", "root", "target"};
    if (input.size() != keys.size())
        return false;
    for (auto it = input.keyBegin(); it != input.keyEnd(); ++it)
        if (!keys.contains(*it))
            return false;
    return true;
}

bool
ComponentSchema::gameRegistrationVersion(const QVariantMap& input)
{
    return input.value("schemaVersion").metaType().id() != QMetaType::QString &&
           input.value("schemaVersion").metaType().id() != QMetaType::Bool &&
           input.value("schemaVersion").toDouble() == 1;
}

bool
ComponentSchema::gameRegistrationTextFields(const QVariantMap& input)
{
    for (const auto& key : QStringList{"title", "platform", "runtimeId", "root", "target"})
        if (input.value(key).metaType().id() != QMetaType::QString ||
            input.value(key).toString().contains(QChar::Null))
            return false;
    return true;
}

bool
ComponentSchema::gameRegistrationTitlePlatform(const QVariantMap& input)
{
    const auto title = input.value("title").toString();
    const auto platform = input.value("platform").toString();
    return !title.trimmed().isEmpty() && title == title.trimmed() && title.size() <= 256 &&
           !platform.trimmed().isEmpty() && platform.size() <= 80 &&
           safeId(input.value("runtimeId").toString());
}

bool
ComponentSchema::gameRegistrationSteam(const QVariantMap& input, QString* error)
{
    bool valid = false;
    const auto id = input.value("target").toULongLong(&valid);
    if (!input.value("root").toString().isEmpty() ||
        !QRegularExpression("^[1-9][0-9]{0,9}$").match(input.value("target").toString()).hasMatch() || !valid ||
        id > 4294967295ULL || QString::number(id) != input.value("target").toString())
    {
        reject(error, "Steam games contain a positive Steam App ID, never a path or launch command.");
        return false;
    }
    return true;
}

bool
ComponentSchema::gameRegistrationRootTarget(const QVariantMap& input)
{
    const auto root = input.value("root").toString();
    const auto target = input.value("target").toString();
    if (root.size() > 4096 || target.size() > 4096 || root.isEmpty() != target.isEmpty())
        return false;
    if (!root.isEmpty() &&
        (!QDir::isAbsolutePath(root) || QDir::cleanPath(root) != root || QDir::isAbsolutePath(target) ||
         target.contains('\\') || QDir::cleanPath(target) != target || target.split('/').contains("..")))
        return false;
    return true;
}

QVariantMap
ComponentSchema::action(const QVariantMap& input, QString* error)
{
    if (error)
        error->clear();
    if (!versionOne(input))
        return reject(error, "Unsupported action version.");
    auto type = input.value("type").toString();
    if (type == "kodi-collection")
        type = input.value("movieId").toInt() > 0 ? "play-movie" : "open-collection";
    QMap<QString, std::function<QVariantMap(const QVariantMap&, QString*)>> handlers;
    handlers.insert("launch-game", [](const QVariantMap& in, QString* err) { return actionLaunchGame(in, err); });
    handlers.insert("open-guide", [](const QVariantMap& in, QString* err) { return actionGuide(in, "open-guide", err); });
    handlers.insert("tune-channel", [](const QVariantMap& in, QString* err) { return actionGuide(in, "tune-channel", err); });
    handlers.insert("launch-app", [](const QVariantMap& in, QString* err) { return actionLaunchApp(in, err); });
    handlers.insert("open-collection", [](const QVariantMap& in, QString* err) { return actionCollection(in, "open-collection", err); });
    handlers.insert("play-movie", [](const QVariantMap& in, QString* err) { return actionCollection(in, "play-movie", err); });
    handlers.insert("open-show", [](const QVariantMap& in, QString* err) { return actionCollection(in, "open-show", err); });
    handlers.insert("open-page", [](const QVariantMap& in, QString* err) { return actionOpenPage(in, err); });
    handlers.insert("show-page-in-panel", [](const QVariantMap& in, QString* err) {
        // Only named pages and component IDs can be addressed by a local panel action.
        auto result = actionOpenPage(in, err);
        if (result.isEmpty()) return result;
        const auto panel = in.value("panelId").toString();
        if (!safeId(panel)) return reject(err, "Choose a valid page panel.");
        result["type"] = "show-page-in-panel";
        result["panelId"] = panel;
        return result;
    });
    handlers.insert("app-drawer", [](const QVariantMap& in, QString* err) { return actionShellRoute(in, "app-drawer", err); });
    handlers.insert("open-apps", [](const QVariantMap& in, QString* err) { return actionShellRoute(in, "open-apps", err); });
    handlers.insert("open-search", [](const QVariantMap& in, QString* err) { return actionShellRoute(in, "open-search", err); });
    handlers.insert("root-page", [](const QVariantMap& in, QString* err) { return actionShellRoute(in, "root-page", err); });
    handlers.insert("settings", [](const QVariantMap& in, QString* err) { return actionSettings(in, err); });
    handlers.insert("switch-input", [](const QVariantMap& in, QString* err) { return actionSwitchInput(in, err); });
    handlers.insert("desktop-mode", [](const QVariantMap& in, QString* err) { return actionDesktopMode(in, err); });
    handlers.insert("wait", [](const QVariantMap& in, QString* err) { return actionWait(in, err); });
    handlers.insert("media.play", [](const QVariantMap& in, QString* err) { return actionMedia(in, "media.play", err); });
    handlers.insert("media.resume", [](const QVariantMap& in, QString* err) { return actionMedia(in, "media.resume", err); });
    handlers.insert("media.markWatched", [](const QVariantMap& in, QString* err) { return actionMedia(in, "media.markWatched", err); });
    handlers.insert("open-folder", [](const QVariantMap& in, QString* err) { return actionOpenTarget(in, "open-folder", err); });
    handlers.insert("open-file", [](const QVariantMap& in, QString* err) { return actionOpenTarget(in, "open-file", err); });
    handlers.insert("open-disc", [](const QVariantMap& in, QString* err) { return actionOpenTarget(in, "open-disc", err); });
    handlers.insert("open-url", [](const QVariantMap& in, QString* err) { return actionOpenTarget(in, "open-url", err); });
    auto it = handlers.find(type);
    if (it != handlers.cend())
        return it.value()(input, error);
    return reject(error, "This action type is not supported.");
}

QVariantMap
ComponentSchema::actionLaunchGame(const QVariantMap& input, QString* error)
{
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!QStringList{"schemaVersion", "type", "gameId", "editionId"}.contains(it.key()))
            return reject(error, "Game actions contain identities only.");
    if (input.value("gameId").metaType().id() != QMetaType::QString || !safeId(input.value("gameId").toString()))
        return reject(error, "Choose a valid game.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "launch-game"}, {"gameId", input.value("gameId")}};
    if (input.contains("editionId"))
    {
        if (input.value("editionId").metaType().id() != QMetaType::QString ||
            !safeId(input.value("editionId").toString()))
            return reject(error, "Choose a valid edition.");
        result["editionId"] = input.value("editionId");
    }
    return result;
}

QVariantMap
ComponentSchema::actionGuide(const QVariantMap& input, const QString& type, QString* error)
{
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!QStringList{"schemaVersion", "type", "channelId", "focusChannelId", "presentation"}.contains(it.key()))
            return reject(error, "Guide actions contain identities and validated presentation only.");
    if (type == "tune-channel")
    {
        if (input.value("channelId").metaType().id() != QMetaType::QString ||
            !safeId(input.value("channelId").toString()) || input.contains("presentation") ||
            input.contains("focusChannelId"))
            return reject(error, "Choose a valid guide channel.");
        QVariantMap result{{"schemaVersion", 1}, {"type", "tune-channel"}, {"channelId", input.value("channelId")}};
        return result;
    }
    QVariantMap result{{"schemaVersion", 1}, {"type", "open-guide"}};
    if (input.contains("focusChannelId"))
    {
        if (input.value("focusChannelId").metaType().id() != QMetaType::QString ||
            !safeId(input.value("focusChannelId").toString()))
            return reject(error, "Choose a valid guide focus channel.");
        result["focusChannelId"] = input.value("focusChannelId");
    }
    if (input.contains("presentation"))
    {
        if (input.value("presentation").metaType().id() != QMetaType::QVariantMap ||
            !validPresentation(input.value("presentation").toMap(), true))
            return reject(error, "Invalid guide presentation.");
        result["presentation"] = input.value("presentation");
    }
    return result;
}

QVariantMap
ComponentSchema::actionLaunchApp(const QVariantMap& input, QString* error)
{
    const auto id = input.value("appId").toString();
    if (!safeId(id))
        return reject(error, "Choose a valid app.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "launch-app"}, {"appId", id}};
    return result;
}

QVariantMap
ComponentSchema::actionCollection(const QVariantMap& input, const QString& type, QString* error)
{
    const auto id = input.value("collectionId").toString();
    if (!safeId(id))
        return reject(error, "Choose a valid collection.");
    QVariantMap result{{"schemaVersion", 1}, {"type", type}, {"collectionId", id}};
    if (type == "play-movie")
    {
        const double movie = input.value("movieId").toDouble();
        if (!std::isfinite(movie) || movie < MinMediaId || movie > MaxMediaId || std::floor(movie) != movie)
            return reject(error, "Choose a valid movie.");
        result["movieId"] = static_cast<int>(movie);
    }
    if (type == "open-show")
    {
        const double show = input.value("tvShowId").toDouble();
        if (!std::isfinite(show) || show < MinMediaId || show > MaxMediaId || std::floor(show) != show)
            return reject(error, "Choose a valid TV show.");
        result["tvShowId"] = static_cast<int>(show);
    }
    return result;
}

QVariantMap
ComponentSchema::actionOpenPage(const QVariantMap& input, QString* error)
{
    const auto id = input.value("pageId").toString();
    if (!safeId(id))
        return reject(error, "Choose a valid page.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "open-page"}, {"pageId", id}};
    return result;
}

QVariantMap
ComponentSchema::actionShellRoute(const QVariantMap& input, const QString& type, QString* error)
{
    Q_UNUSED(input);
    Q_UNUSED(error);
    QVariantMap result{{"schemaVersion", 1}, {"type", type}};
    return result;
}

QVariantMap
ComponentSchema::actionMedia(const QVariantMap& input, const QString& type, QString* error)
{
    // Media action descriptors: stable typed IDs for media operations,
    // validated at the boundary and dispatched through ActionDispatcher.
    // media.play starts an item from the beginning; media.resume resumes at the
    // saved point; media.markWatched sets the watched intent (bridge write).
    const auto kind = input.value("mediaKind").toString();
    if (type == "media.markWatched")
    {
        if (!QStringList{"movie", "episode"}.contains(kind))
            return reject(error, "Watched state applies to movies and episodes only.");
    }
    else if (!QStringList{"movie", "episode", "song", "channel"}.contains(kind))
    {
        return reject(error, "Choose a supported media kind.");
    }
    const double id = input.value("mediaId").toDouble();
    if (!std::isfinite(id) || id < 1 || id > 2147483647 || std::floor(id) != id)
        return reject(error, "Choose a valid media ID.");
    QVariantMap result{{"schemaVersion", 1}, {"type", type}, {"mediaKind", kind}, {"mediaId", static_cast<int>(id)}};
    if (type == "media.resume")
        result["resume"] = true;
    else if (type == "media.play")
        result["resume"] = false;
    else if (type == "media.markWatched")
    {
        const bool watched = input.value("watched", true).toBool();
        result["watched"] = watched;
    }
    return result;
}

bool
ComponentSchema::validateSlotComponent(const QVariantList& permitted, const QString& componentType, QString* error)
{
    // A slot's permitted list restricts which component kinds may occupy it.
    // An empty permitted list accepts any component; otherwise the component's
    // kind must appear in the list. Used when resolving skin slot overrides
    // and when placing components into slots during editing.
    if (permitted.isEmpty())
        return true;
    for (const auto& value : permitted)
    {
        const auto kind = value.toString();
        if (kind == componentType)
            return true;
    }
    if (error)
        *error = "Component kind is not permitted in this slot.";
    return false;
}

bool
ComponentSchema::validateInspectorField(const QVariantMap& field, QString* error)
{
    // Validates a single inspector field definition: bounded schemaVersion plus
    // known field keys, a safe key id, a non-empty label, and a known field type.
    // Fields are the editable controls exposed by Studio; malformed definitions
    // would make fields unreachable or unsafe to edit.
    static const QSet<QString> validKeys{"schemaVersion", "tab", "scope", "key", "label", "type",
                                         "choices", "types", "families", "min", "max", "step",
                                         "fallback", "advanced", "excludeTypes", "excludeFamilies",
                                         "control", "displayScale", "displayUnit", "needsNamedPage",
                                         "needsComposition"};
    static const QSet<QString> validTypes{"text", "number", "choice", "toggle", "color", "action",
                                          "repair", "section", "visibility", "lock", "order",
                                          "typography", "historyApps", "editorColorsReset",
                                          "responsive", "sourcePolicy", "grid", "editableMenu",
                                          "header", "headerEditor"};
    if (!field.contains("schemaVersion") || !versionOne(field))
        return reject(error, "Inspector field requires schemaVersion 1.");
    if (!field.contains("key") || !safeId(field.value("key").toString()))
        return reject(error, "Inspector field key must be a safe id.");
    if (!field.contains("label") || field.value("label").toString().trimmed().isEmpty()
        || field.value("label").toString().size() > 80)
        return reject(error, "Inspector field label must be non-empty and at most 80 characters.");
    if (!field.contains("type") || !validTypes.contains(field.value("type").toString()))
        return reject(error, "Inspector field type must be a known type.");
    for (const auto& k : field.keys())
        if (!validKeys.contains(k))
            return reject(error, "Inspector field contains unknown key.");
    return true;
}

bool
ComponentSchema::validateInspectorFields(const QVariantList& fields, QString* error)
{
    // Validates a list of inspector field definitions: bounded size, unique keys,
    // and each field passes validateInspectorField. Studio surface editor depends
    // on these definitions being well formed before any field is rendered.
    if (fields.isEmpty() || fields.size() > 256)
        return reject(error, "Inspector fields list must be non-empty and at most 256 entries.");
    QSet<QString> seenKeys;
    for (const auto& value : fields)
    {
        const auto field = value.toMap();
        const auto key = field.value("key").toString();
        if (seenKeys.contains(key))
            return reject(error, "Duplicate inspector field key.");
        seenKeys.insert(key);
        if (!validateInspectorField(field, error))
            return false;
    }
    return true;
}

bool
ComponentSchema::validateRoleDefinition(const QVariantMap& role, QString* error)
{
    // Validates a single theme role definition: bounded schemaVersion, safe key,
    // and non-empty label. Roles map theme colors to UI surfaces; invalid role
    // definitions would leave surfaces unstyled or miscolored.
    if (!role.contains("schemaVersion") || !versionOne(role))
        return reject(error, "Theme role requires schemaVersion 1.");
    if (!role.contains("key") || !safeId(role.value("key").toString()))
        return reject(error, "Theme role key must be a safe id.");
    if (!role.contains("label") || role.value("label").toString().trimmed().isEmpty()
        || role.value("label").toString().size() > 80)
        return reject(error, "Theme role label must be non-empty and at most 80 characters.");
    return true;
}

bool
ComponentSchema::validateRoleDefinitions(const QVariantList& roles, QString* error)
{
    // Validates a list of theme role definitions: bounded size, unique keys,
    // and each role passes validateRoleDefinition. Theme groups depend on these
    // definitions for color assignment.
    if (roles.isEmpty() || roles.size() > 64)
        return reject(error, "Theme roles list must be non-empty and at most 64 entries.");
    QSet<QString> seenKeys;
    for (const auto& value : roles)
    {
        const auto role = value.toMap();
        const auto key = role.value("key").toString();
        if (seenKeys.contains(key))
            return reject(error, "Duplicate theme role key.");
        seenKeys.insert(key);
        if (!validateRoleDefinition(role, error))
            return false;
    }
    return true;
}

bool
ComponentSchema::validateActionDefinition(const QVariantMap& action, QString* error)
{
    // Validates a single action definition: bounded schemaVersion, non-empty name,
    // safe action id, and optional bounded aliases. Actions are the commands Studio
    // can invoke on a component; invalid actions would be unreachable or unsafe.
    if (!action.contains("schemaVersion") || !versionOne(action))
        return reject(error, "Action definition requires schemaVersion 1.");
    if (!action.contains("name") || action.value("name").toString().trimmed().isEmpty()
        || action.value("name").toString().size() > 80)
        return reject(error, "Action definition name must be non-empty and at most 80 characters.");
    if (!action.contains("action") || !safeId(action.value("action").toString()))
        return reject(error, "Action definition id must be a safe id.");
    if (action.contains("aliases"))
    {
        const auto aliases = action.value("aliases").toList();
        if (aliases.size() > 8)
            return reject(error, "Action definition aliases must be at most 8 entries.");
        for (const auto& alias : aliases)
        {
            const auto s = alias.toString();
            if (s.isEmpty() || s.size() > 40)
                return reject(error, "Action alias must be non-empty and at most 40 characters.");
        }
    }
    return true;
}

bool
ComponentSchema::validateActionDefinitions(const QVariantList& actions, QString* error)
{
    // Validates a list of action definitions: bounded size, unique ids, and each
    // action passes validateActionDefinition. Action dispatcher routes commands by
    // id; duplicates would cause ambiguous command resolution.
    if (actions.isEmpty() || actions.size() > 128)
        return reject(error, "Action definitions list must be non-empty and at most 128 entries.");
    QSet<QString> seenIds;
    for (const auto& value : actions)
    {
        const auto action = value.toMap();
        const auto id = action.value("action").toString();
        if (seenIds.contains(id))
            return reject(error, "Duplicate action definition id.");
        seenIds.insert(id);
        if (!validateActionDefinition(action, error))
            return false;
    }
    return true;
}

bool
ComponentSchema::validateActionCategoryDefinition(const QVariantMap& category, QString* error)
{
    // Validates a single action category definition: bounded schemaVersion,
    // non-empty name, and a non-empty list of known action type ids. Categories
    // group actions in Studio pickers; empty or unknown type lists break selection.
    if (!category.contains("schemaVersion") || !versionOne(category))
        return reject(error, "Action category requires schemaVersion 1.");
    if (!category.contains("name") || category.value("name").toString().trimmed().isEmpty()
        || category.value("name").toString().size() > 80)
        return reject(error, "Action category name must be non-empty and at most 80 characters.");
    if (!category.contains("types") || !category.value("types").isList()
        || category.value("types").toList().isEmpty())
        return reject(error, "Action category must list at least one action type.");
    return true;
}

bool
ComponentSchema::validateActionCategoryDefinitions(const QVariantList& categories, QString* error)
{
    // Validates a list of action category definitions: bounded size, unique names,
    // and each category passes validateActionCategoryDefinition.
    if (categories.isEmpty() || categories.size() > 12)
    {
        reject(error, "Action categories list must be non-empty and at most 12 entries.");
        return false;
    }
    QSet<QString> seenNames;
    for (const auto& value : categories)
    {
        const auto category = value.toMap();
        const auto name = category.value("name").toString();
        if (seenNames.contains(name))
        {
            reject(error, "Duplicate action category name.");
            return false;
        }
        seenNames.insert(name);
        if (!validateActionCategoryDefinition(category, error))
            return false;
    }
    return true;
}

QVariantMap
ComponentSchema::actionSettings(const QVariantMap& input, QString* error)
{
    const double section = input.value("section", MinSettingsSection).toDouble();
    if (section < MinSettingsSection || section > MaxSettingsSection || std::floor(section) != section)
        return reject(error, "Invalid Settings section.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "settings"}, {"section", static_cast<int>(section)}};
    return result;
}

QVariantMap
ComponentSchema::actionSwitchInput(const QVariantMap& input, QString* error)
{
    const double inputNumber = input.value("input").toDouble();
    if (!std::isfinite(inputNumber) || inputNumber < MinHdmiInput || inputNumber > MaxHdmiInput ||
        std::floor(inputNumber) != inputNumber)
        return reject(error, "Choose an HDMI input from 1 to 8.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "switch-input"}, {"input", static_cast<int>(inputNumber)}};
    return result;
}

QVariantMap
ComponentSchema::actionDesktopMode(const QVariantMap& input, QString* error)
{
    Q_UNUSED(error);
    QVariantMap result{{"schemaVersion", 1}, {"type", "desktop-mode"},
                       {"label", input.value("label", "Desktop Mode").toString().left(MaxNameLength)}};
    return result;
}

QVariantMap
ComponentSchema::actionWait(const QVariantMap& input, QString* error)
{
    const double duration = input.value("duration", 500).toDouble();
    if (!std::isfinite(duration) || duration < 50 || duration > 30000 || std::floor(duration) != duration)
        return reject(error, "Wait duration must be 50-30000 ms.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "wait"}, {"duration", static_cast<int>(duration)}};
    return result;
}

QVariantMap
ComponentSchema::actionOpenTarget(const QVariantMap& input, const QString& type, QString* error)
{
    const auto target = input.value("target").toString().trimmed();
    const auto handler = input.value("openWith").toString();
    if (target.isEmpty() || target.size() > MaxActionTargetSize || target.contains(QChar::Null) ||
        (!handler.isEmpty() && !safeId(handler)))
        return reject(error, "Invalid target or Open With app.");
    if (type == "open-url")
    {
        const QUrl url(target, QUrl::StrictMode);
        if (!url.isValid() || !QStringList{"http", "https"}.contains(url.scheme()) || url.host().isEmpty() ||
            !url.userInfo().isEmpty())
            return reject(error, "Use an HTTP or HTTPS URL without embedded credentials.");
    }
    else if (!QDir::isAbsolutePath(target))
        return reject(error, "Use an absolute media path.");
    QVariantMap result{{"schemaVersion", 1}, {"type", type}, {"target", target},
                       {"openWith", handler.isEmpty() ? "system" : handler}};
    return result;
}

QVariantList
ComponentSchema::actions(const QVariantList& input, QString* error)
{
    if (error)
        error->clear();
    QVariantList result;
    for (const auto& entry : input)
    {
        if (entry.metaType().id() != QMetaType::QVariantMap)
        {
            if (error)
                *error = "Each action in a sequence must be an action map.";
            return {};
        }
        const auto normalized = action(entry.toMap(), error);
        if (normalized.isEmpty())
            return {};
        if (input.size() != 1 && normalized.value("type") == "show-page-in-panel")
        {
            if (error)
                *error = "A page panel switch must be the only action on a component.";
            return {};
        }
        result.append(normalized);
    }
    return result;
}

QVariantMap
ComponentSchema::editorAction(const QVariantMap& input, QString* error)
{
    if (error)
        error->clear();
    if (!versionOne(input))
        return reject(error, "Unsupported editor action version.");

    const auto typeValue = input.value("type");
    if (typeValue.metaType().id() != QMetaType::QString)
        return reject(error, "Choose a supported editor action.");
    const auto type = typeValue.toString();
    QVariantMap result{{"schemaVersion", 1}, {"type", type}};
    QMap<QString, std::function<QVariantMap(const QVariantMap&, QString*)>> handlers;
    handlers.insert("capture-artwork-apply", editorActionCaptureArtworkApply);
    handlers.insert("capture-apply", editorActionCaptureApply);
    handlers.insert("capture-switch-input", editorActionCaptureSwitchInput);
    handlers.insert("capture-preview-draft", editorActionCapturePreviewDraft);
    handlers.insert("capture-preview-start",
                    [](const QVariantMap& in, QString* err)
                    { return editorActionCaptureControl(in, "capture-preview-start", err); });
    handlers.insert("capture-preview-stop",
                    [](const QVariantMap& in, QString* err)
                    { return editorActionCaptureControl(in, "capture-preview-stop", err); });
    handlers.insert("capture-switch-cancel",
                    [](const QVariantMap& in, QString* err)
                    { return editorActionCaptureControl(in, "capture-switch-cancel", err); });
    handlers.insert("capture-refresh",
                    [](const QVariantMap& in, QString* err)
                    { return editorActionCaptureControl(in, "capture-refresh", err); });
    handlers.insert("audio-output-select", editorActionAudioOutputSelect);
    auto it = handlers.find(type);
    if (it != handlers.cend())
        return it.value()(input, error);
    return reject(error, "This editor action type is not supported.");
}

QVariantMap
ComponentSchema::editorActionCaptureArtworkApply(const QVariantMap& input, QString* error)
{
    const auto exactKeys = QStringList{"schemaVersion", "type", "artwork"};
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!exactKeys.contains(it.key()))
        {
            if (error && error->isEmpty())
                *error = "Editor actions contain only validated fields.";
            return {};
        }
    if (input.value("artwork").metaType().id() != QMetaType::QVariantMap ||
        !validComposition({{"schemaVersion", 1}, {"captureInputArtwork", input.value("artwork")}}))
    {
        if (error && error->isEmpty())
            *error = "Choose valid managed artwork for HDMI inputs 1–8.";
        return {};
    }
    QVariantMap result{{"schemaVersion", 1}, {"type", "capture-artwork-apply"}};
    result.insert("artwork", input.value("artwork").toMap());
    return result;
}

QVariantMap
ComponentSchema::normalizeConfiguration(const QVariant& value, QString* error)
{
    if (value.metaType().id() != QMetaType::QVariantMap)
    {
        reject(error, "Choose a valid capture configuration.");
        return {};
    }
    const auto configuration = value.toMap();
    const QStringList keys{"audioDeviceId", "baudRate", "captureDeviceId", "commandTemplate", "inputLabels",
                           "selectedInput", "serialPortName", "sessionReturn", "sessionTransition",
                           "transitionDuration", "videoFormatKey"};
    if (configuration.size() != keys.size())
        return reject(error, "Capture settings must include every staged field exactly once.");
    for (const auto& key : keys)
        if (!configuration.contains(key))
            return reject(error, "Capture settings must include every staged field exactly once.");
    QVariantMap normalized = configuration;
    const QMap<QString, int> limits{{"audioDeviceId", 512}, {"captureDeviceId", 512}, {"serialPortName", 256},
                                    {"videoFormatKey", 256}, {"commandTemplate", 128}, {"sessionTransition", 16},
                                    {"sessionReturn", 16}};
    for (auto it = limits.cbegin(); it != limits.cend(); ++it)
    {
        const auto field = configuration.value(it.key());
        if (field.metaType().id() != QMetaType::QString || field.toString().size() > it.value() ||
            field.toString().contains(QChar::Null))
            return reject(error, "Capture settings contain an invalid device identity or command.");
    }
    for (const auto& key : QStringList{"baudRate", "selectedInput", "transitionDuration"})
    {
        const auto field = configuration.value(key);
        const auto type = field.metaType().id();
        bool ok = false;
        const double number = field.toDouble(&ok);
        if ((type != QMetaType::Int && type != QMetaType::Double) || !ok || !std::isfinite(number) ||
            std::floor(number) != number ||
            (key == "selectedInput" && (number < MinHdmiInput || number > MaxHdmiInput)) ||
            (key == "baudRate" && !QList<double>{9600, 19200, 38400, 57600, 115200}.contains(number)) ||
            (key == "transitionDuration" && !QList<double>{200, 250, 300, 350}.contains(number)))
            return reject(error, "Choose a supported serial speed, HDMI input and transition duration.");
        normalized[key] = static_cast<int>(number);
    }
    if (!QStringList{"origin", "home"}.contains(configuration.value("sessionReturn").toString()) ||
        !QStringList{"instant", "fade", "slide", "expand"}.contains(configuration.value("sessionTransition").toString()))
        return reject(error, "Choose a supported session return and transition.");
    const auto labelsValue = configuration.value("inputLabels");
    const auto labels = labelsValue.toMap();
    if (labelsValue.metaType().id() != QMetaType::QVariantMap ||
        labels.size() != static_cast<qsizetype>(MaxHdmiInput))
        return reject(error, "Capture input labels must name all eight HDMI inputs.");
    QVariantMap normalizedLabels;
    for (int input = MinHdmiInput; input <= MaxHdmiInput; ++input)
    {
        const auto key = QString::number(input);
        const auto label = labels.value(key);
        const auto text = label.toString();
        if (label.metaType().id() != QMetaType::QString || text.trimmed().isEmpty() ||
            text.size() > 40 || text.contains(QChar::Null))
            return reject(error, "Each HDMI input label must contain 1 to 40 safe characters.");
        for (const auto character : text)
        {
            const auto code = character.unicode();
            if ((character.isSpace() && character != QLatin1Char(' ')) || code < 0x20 ||
                (code >= 0x7f && code <= 0x9f))
                return reject(error, "HDMI input labels cannot contain control characters.");
        }
        normalizedLabels.insert(key, text.trimmed());
    }
    normalized["inputLabels"] = normalizedLabels;
    return normalized;
}

QVariantMap
ComponentSchema::editorActionCaptureApply(const QVariantMap& input, QString* error)
{
    const auto exactKeys = QStringList{"schemaVersion", "type", "configuration"};
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!exactKeys.contains(it.key()))
            return {};
    const auto configuration = normalizeConfiguration(input.value("configuration"), error);
    if (configuration.isEmpty())
        return {};
    QVariantMap result;
    result["schemaVersion"] = 1;
    result["type"] = "capture-apply";
    result["configuration"] = configuration;
    return result;
}

QVariantMap
ComponentSchema::editorActionCaptureSwitchInput(const QVariantMap& input, QString* error)
{
    const auto exactKeys = QStringList{"schemaVersion", "type", "input"};
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!exactKeys.contains(it.key()))
            return {};
    const auto number = input.value("input");
    if ((number.metaType().id() != QMetaType::Int && number.metaType().id() != QMetaType::Double) ||
        !std::isfinite(number.toDouble()) || std::floor(number.toDouble()) != number.toDouble() ||
        number.toDouble() < MinHdmiInput || number.toDouble() > MaxHdmiInput)
        return reject(error, "Choose an HDMI input from 1 to 8.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "capture-switch-input"}, {"input", number.toInt()}};
    return result;
}

QVariantMap
ComponentSchema::editorActionCapturePreviewDraft(const QVariantMap& input, QString* error)
{
    const auto exactKeys = QStringList{"schemaVersion", "type", "captureDeviceId", "videoFormatKey", "audioDeviceId"};
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!exactKeys.contains(it.key()))
            return {};
    QVariantMap result{{"schemaVersion", 1}, {"type", "capture-preview-draft"}};
    for (const auto& key : QStringList{"captureDeviceId", "videoFormatKey", "audioDeviceId"})
    {
        const auto field = input.value(key);
        if (field.metaType().id() != QMetaType::QString || field.toString().size() > 512 ||
            field.toString().contains(QChar::Null))
            return reject(error, "Choose valid capture preview devices and format.");
        result.insert(key, field.toString());
    }
    return result;
}

QVariantMap
ComponentSchema::editorActionCaptureControl(const QVariantMap& input, const QString& type, QString* error)
{
    Q_UNUSED(error);
    const auto exactKeys = QStringList{"schemaVersion", "type"};
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!exactKeys.contains(it.key()))
            return {};
    QVariantMap result{{"schemaVersion", 1}, {"type", type}};
    return result;
}

QVariantMap
ComponentSchema::editorActionAudioOutputSelect(const QVariantMap& input, QString* error)
{
    const auto exactKeys = QStringList{"schemaVersion", "type", "outputId"};
    for (auto it = input.cbegin(); it != input.cend(); ++it)
        if (!exactKeys.contains(it.key()))
        {
            if (error)
                *error = "Editor actions contain only validated fields.";
            return {};
        }
    const auto id = input.value("outputId");
    if (id.metaType().id() != QMetaType::QString || id.toString().size() > 256 ||
        !QRegularExpression("^[A-Za-z0-9_.:-]+$").match(id.toString()).hasMatch())
        return reject(error, "Choose a valid available audio output.");
    QVariantMap result{{"schemaVersion", 1}, {"type", "audio-output-select"}, {"outputId", id.toString()}};
    return result;
}

QVariantMap
ComponentSchema::component(const QVariantMap& input, bool deriveSource, QString* error)
{
    if (error)
        error->clear();
    if (!versionOne(input) || !safeId(input.value("id").toString()))
        return reject(error, "Invalid component identity or version.");
    QVariantMap result = input;
    const auto type = input.value("componentType").toString();
    if (!componentValidateAll(input, type, error))
        return {};
    result["schemaVersion"] = 1;
    result["componentType"] = type;
    result["size"] = input.value("size", "standard");
    QVariantList normalizedActions;
    const bool canHaveActions = type != "clock" && type != "text" && type != "shape" && type != "container";
    if (input.contains("actions") && canHaveActions)
    {
        normalizedActions = actions(input.value("actions").toList(), error);
        if (normalizedActions.isEmpty())
            return {};
        result["actions"] = normalizedActions;
    }
    else if (input.contains("action") && canHaveActions)
    {
        normalizedActions = actions(QVariantList{input.value("action").toMap()}, error);
        if (normalizedActions.isEmpty())
            return {};
        result["actions"] = normalizedActions;
    }
    QVariantMap source;
    if (type == "button" || type == "image" || type == "library")
        source = componentBuildLibrarySource(input, input.value("presentation").toMap(), error);
    else if (type == "clock" || type == "text" || type == "shape" || type == "container")
        source = componentBuildShellSource(input, type, error);
    else
    {
        const auto kind = input.value("kind").toString();
        const auto actionType = normalizedActions.isEmpty() ? QString() : normalizedActions.first().toMap().value("type").toString();
        if (kind != "app" && kind != "action")
            return reject(error, "Invalid launcher kind.");
        if (kind == "app" && !normalizedActions.isEmpty() &&
            (actionType != "launch-app" || input.value("appId") != normalizedActions.first().toMap().value("appId")))
            return reject(error, "App binding does not match its action.");
        if (kind == "action" && normalizedActions.isEmpty() && input.value("name").toString().trimmed().isEmpty())
            return reject(error, "A launcher needs a name.");
        if (type == "media-shelf" && !normalizedActions.isEmpty() && actionType != "open-collection")
            return reject(error, "A media shelf requires a collection source.");
        source = componentBuildLauncherSource(input, normalizedActions, actionType, error);
    }
    if (source.isEmpty())
        return {};
    if (!deriveSource && input.value("source").toMap() != source)
        return reject(error, "Source binding does not match its action.");
    // Reject unknown component keys; only documented fields may appear.
    static const QSet<QString> validComponentKeys{"schemaVersion", "id", "kind",
                                                  "componentType", "size", "source",
                                                  "actions", "action", "name", "presentation",
                                                  "placement", "accent", "symbol", "linkedDesign",
                                                  "designNode", "accessibility", "requiredCapability",
                                                  "customLogo", "widgetType", "family",
                                                  "appCollection", "appId", "opacity",
                                                  "categoryId", "parentId", "children",
                                                  "animateArtwork", "hidden", "locked",
                                                  "showTitle", "showType", "showLogo", "logoFill", "contentSlot",
                                                  "permitted"};
    for (auto it = input.constBegin(); it != input.constEnd(); ++it)
        if (!validComponentKeys.contains(it.key()))
            return reject(error, "Component contains unknown fields.");
    // Retain the tagged slots in packaged templates while keeping their schema bounded.
    if (input.contains("contentSlot"))
    {
        const auto slot = input.value("contentSlot").toMap();
        const auto label = slot.value("label");
        const auto kind = slot.value("kind").toString();
        if ((slot.size() != 4 && slot.size() != 5) || !slot.contains("schemaVersion") || !versionOne(slot) ||
            !safeId(slot.value("id").toString()) || label.metaType().id() != QMetaType::QString ||
            label.toString().trimmed().isEmpty() || label.toString().size() > 80 ||
            (kind != "buttons" && kind != "apps" && kind != "media"))
            return reject(error, "Invalid template content slot.");
        if (slot.contains("permitted"))
        {
            const auto permitted = slot.value("permitted").toList();
            for (const auto& value : permitted)
            {
                const auto permittedKind = value.toString();
                if (permittedKind.isEmpty() || permittedKind.size() > 40)
                    return reject(error, "Slot permitted component kind must be a non-empty name up to 40 characters.");
            }
        }
    }
    result["source"] = source;
    return result;
}

bool
ComponentSchema::componentValidateAll(const QVariantMap& input, const QString& type, QString* error)
{
    return componentValidateBasic(input, type, error) &&
           componentValidateStructure(input, type, error) &&
           componentValidateCapabilities(input, type, error) &&
           componentValidatePresentationSettings(input, type, error);
}

bool
ComponentSchema::componentValidateBasic(const QVariantMap& input, const QString& type, QString* error)
{
    return componentValidateTypeAndShape(input, type, error) &&
           componentValidateName(input, error);
}

bool
ComponentSchema::componentValidateStructure(const QVariantMap& input, const QString& type, QString* error)
{
    return componentValidateFamily(input, type, error) &&
           componentValidateAppCollection(input, type, error) &&
           componentValidateAccessibility(input, error) &&
           componentValidateParentId(input, error) &&
           componentValidateChildren(input, error);
}

bool
ComponentSchema::componentValidateCapabilities(const QVariantMap& input, const QString& type, QString* error)
{
    return componentValidateRequiredCapability(input, type, error) &&
           componentValidateSize(input, error) &&
           componentValidateOpacity(input, error);
}

bool
ComponentSchema::componentValidatePresentationSettings(const QVariantMap& input, const QString& type, QString* error)
{
    return componentValidateBooleans(input, error) &&
           componentValidateAccent(input, error) &&
           componentValidateSymbol(input, error) &&
           componentValidateLinkedDesign(input, error) &&
           componentValidatePlacement(input, error) &&
           componentValidatePresentation(input, type, error) &&
           componentValidateAnimateArtwork(input, type, error) &&
           componentValidateCustomLogo(input, error);
}

bool
ComponentSchema::componentValidateTypeAndShape(const QVariantMap& input, const QString& type, QString* error)
{
    if (!QStringList{"launcher", "clock", "text", "shape", "media-shelf", "container", "button", "image", "library"}
             .contains(type))
    {
        reject(error, "Unsupported component type.");
        return false;
    }
    if (type == "shape" && (input.contains("action") || input.contains("actions") || input.contains("customLogo")))
    {
        reject(error, "A decorative shape does not launch an action or contain image artwork.");
        return false;
    }
    if (type == "shape" && input.value("name").toString().trimmed().isEmpty())
    {
        reject(error, "Give this shape an editor name.");
        return false;
    }
    const auto shape = input.value("presentation").toMap().value("shape").toString();
    if (!shape.isEmpty() && type != "shape" && (type != "container" || shape == "ellipse" || shape == "divider"))
    {
        reject(error, "Choose a supported decorative shape or rectangular container.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateName(const QVariantMap& input, QString* error)
{
    if (input.contains("name") &&
        (input["name"].metaType().id() != QMetaType::QString ||
         input["name"].toString().size() > MaxComponentNameLength || input["name"].toString().contains(QChar::Null)))
    {
        reject(error, "Use a component name of at most 160 characters.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateFamily(const QVariantMap& input, const QString& type, QString* error)
{
    if (input.contains("family") &&
        !((type == "container" &&
           QStringList{"carousel", "app-bar", "page-menu", "page-panel", "detected-sources", "source-notice"}.contains(
               input["family"].toString())) ||
          (type == "library" &&
           QStringList{"recently-viewed", "music-library", "tv-guide"}.contains(input["family"].toString()))))
    {
        reject(error, "Unsupported component family.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateAppCollection(const QVariantMap& input, const QString& type, QString* error)
{
    if (!input.contains("appCollection"))
        return true;
    if (type != "container" || input.value("family") != "app-bar" ||
        input["appCollection"].metaType().id() != QMetaType::QVariantMap)
    {
        reject(error, "Choose an app carousel for an app collection.");
        return false;
    }
    const auto collection = input["appCollection"].toMap();
    return componentValidateAppCollectionShape(collection, error) &&
           componentValidateAppCollectionShowStore(collection, error) &&
           componentValidateAppCollectionSection(collection, error) &&
           componentValidateAppCollectionIds(collection, error) &&
           componentValidateAppCollectionMode(collection, error);
}

bool
ComponentSchema::componentValidateAppCollectionShape(const QVariantMap& collection, QString* error)
{
    if (collection.size() != 2 + int(collection.contains("section")) + int(collection.contains("showStore")) ||
        !QStringList{"all", "selected"}.contains(collection.value("mode").toString()) ||
        collection.value("ids").metaType().id() != QMetaType::QVariantList ||
        collection.value("ids").toList().size() > 128)
    {
        reject(error, "Choose All installed or up to 128 selected apps.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateAppCollectionShowStore(const QVariantMap& collection, QString* error)
{
    if (!collection.contains("showStore"))
        return true;
    if (collection["showStore"].metaType().id() != QMetaType::Bool)
    {
        reject(error, "Choose whether More apps shows the App Store.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateAppCollectionSection(const QVariantMap& collection, QString* error)
{
    if (!collection.contains("section"))
        return true;
    if (collection["section"].metaType().id() != QMetaType::QString ||
        collection["section"].toString().trimmed().isEmpty() || collection["section"].toString().size() > 80 ||
        collection["section"].toString().contains(QChar::Null))
    {
        reject(error, "Choose a named app group of up to 80 characters.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateAppCollectionIds(const QVariantMap& collection, QString* error)
{
    QSet<QString> ids;
    for (const auto& id : collection.value("ids").toList())
    {
        if (id.metaType().id() != QMetaType::QString || !safeId(id.toString()) || ids.contains(id.toString()))
        {
            reject(error, "Choose distinct safe app identities.");
            return false;
        }
        ids.insert(id.toString());
    }
    return true;
}

bool
ComponentSchema::componentValidateAppCollectionMode(const QVariantMap& collection, QString* error)
{
    if (collection.value("mode") != "all")
        return true;
    if (!collection.value("ids").toList().isEmpty())
    {
        reject(error, "All installed apps does not store selected membership.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateAccessibility(const QVariantMap& input, QString* error)
{
    if (!input.contains("accessibility"))
        return true;
    if (input["accessibility"].metaType().id() != QMetaType::QVariantMap)
    {
        reject(error, "Invalid accessible description.");
        return false;
    }
    const auto a = input["accessibility"].toMap();
    for (auto it = a.cbegin(); it != a.cend(); ++it)
        if (!QStringList{"name", "hint"}.contains(it.key()) || it.value().metaType().id() != QMetaType::QString ||
            it.value().toString().size() > MaxAccessibleTextLength || it.value().toString().contains(QChar::Null))
        {
            reject(error, "Use bounded accessible text.");
            return false;
        }
    return true;
}

bool
ComponentSchema::componentValidateParentId(const QVariantMap& input, QString* error)
{
    if (!input.contains("parentId"))
        return true;
    if (input["parentId"].metaType().id() != QMetaType::QString ||
        (!input["parentId"].toString().isEmpty() && !safeId(input["parentId"].toString())))
    {
        reject(error, "Invalid parent identity.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateChildren(const QVariantMap& input, QString* error)
{
    if (input.contains("children"))
    {
        reject(error, "Group children use validated component parent identities.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateRequiredCapability(const QVariantMap& input, const QString& type, QString* error)
{
    if (!input.contains("requiredCapability"))
        return true;
    if (type != "container" || input["requiredCapability"] != "capture-inputs")
    {
        reject(error, "Unsupported component capability.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateSize(const QVariantMap& input, QString* error)
{
    if (!QStringList{"compact", "standard", "wide"}.contains(input.value("size", "standard").toString()))
    {
        reject(error, "Invalid component size.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateOpacity(const QVariantMap& input, QString* error)
{
    if (!input.contains("opacity"))
        return true;
    if (input["opacity"].metaType().id() == QMetaType::QString ||
        input["opacity"].metaType().id() == QMetaType::Bool)
    {
        reject(error, "Opacity must be numeric.");
        return false;
    }
    const auto opacity = input.value("opacity").toDouble();
    if (!std::isfinite(opacity) || opacity < MinOpacity || opacity > MaxOpacity)
    {
        reject(error, "Invalid component opacity.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateBooleans(const QVariantMap& input, QString* error)
{
    for (const auto& key : QStringList{"hidden", "locked"})
        if (input.contains(key) && input.value(key).metaType().id() != QMetaType::Bool)
        {
            reject(error, "Visibility and lock must be boolean values.");
            return false;
        }
    for (const auto& key : QStringList{"showTitle", "showType", "showLogo", "logoFill"})
        if (input.contains(key) && input[key].metaType().id() != QMetaType::Bool)
        {
            reject(error, "Presentation toggles must be boolean values.");
            return false;
        }
    return true;
}

bool
ComponentSchema::componentValidateAccent(const QVariantMap& input, QString* error)
{
    if (!input.contains("accent"))
        return true;
    if (input["accent"].metaType().id() != QMetaType::QString || !QColor(input["accent"].toString()).isValid() ||
        QColor(input["accent"].toString()).alpha() != 255)
    {
        reject(error, "Choose an opaque accent color.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateSymbol(const QVariantMap& input, QString* error)
{
    if (!input.contains("symbol"))
        return true;
    if (input["symbol"].metaType().id() != QMetaType::QString || input["symbol"].toString().size() > MaxSymbolLength)
    {
        reject(error, "Use a short symbol.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateLinkedDesign(const QVariantMap& input, QString* error)
{
    for (const auto& key : {"linkedDesign", "designNode"})
        if (input.contains(key) && (input[key].metaType().id() != QMetaType::QString || !safeId(input[key].toString())))
        {
            reject(error, "Invalid reusable component identity.");
            return false;
        }
    return true;
}

bool
ComponentSchema::componentValidatePlacement(const QVariantMap& input, QString* error)
{
    if (!input.contains("placement"))
        return true;
    const auto placement = input.value("placement");
    if (placement.metaType().id() != QMetaType::QVariantMap ||
        !validPlacement(placement.toMap(),
                        input.value("parentId").toString().isEmpty() ? MaxChildColumns : 128,
                        MaxScrollableRows) ||
        placement.toMap().value("height").toInt() > MaxChildRows)
    {
        reject(error, "Invalid component placement.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidatePresentation(const QVariantMap& input, const QString& type, QString* error)
{
    if (!input.contains("presentation"))
        return true;
    if (input.value("presentation").metaType().id() != QMetaType::QVariantMap ||
        !validPresentation(input.value("presentation").toMap(), type == "library", type == "container"))
    {
        reject(error, "Invalid component presentation.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateAnimateArtwork(const QVariantMap& input, const QString& type, QString* error)
{
    if (type != "image" && input.value("presentation").toMap().contains("animateArtwork"))
    {
        reject(error, "GIF motion belongs to an Image component.");
        return false;
    }
    return true;
}

bool
ComponentSchema::componentValidateCustomLogo(const QVariantMap& input, QString* error)
{
    if (input.value("customLogo").toString().isEmpty())
        return true;
    static const QRegularExpression artwork("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg|gif)$");
    if (!artwork.match(input.value("customLogo").toString()).hasMatch())
    {
        reject(error, "Invalid saved artwork path.");
        return false;
    }
    return true;
}

QVariantMap
ComponentSchema::componentBuildLibrarySource(const QVariantMap& input, const QVariantMap& style, QString* error)
{
    QVariantMap source;
    if (input.value("kind").toString() != "widget" || input.value("widgetType") != input.value("componentType").toString() ||
        input.value("name").toString().trimmed().isEmpty())
    {
        reject(error, "A visual component needs its registered type and a name.");
        return {};
    }
    const auto type = input.value("componentType").toString();
    if (type == "library")
        return componentBuildLibrarySourceLibrary(input, style, error);
    QVariantList normalizedList;
    if (input.contains("actions"))
    {
        const auto list = input.value("actions").toList();
        normalizedList = actions(list, error);
        if (normalizedList.isEmpty())
            return {};
    }
    source = {{"schemaVersion", 1}, {"provider", "shell"}, {"kind", type}};
    return source;
}

// Keep Guide shelves bound to configured PVR channels, not arbitrary library views.
bool
ComponentSchema::componentValidateLibraryGuide(bool guide, const QVariantMap& style, QString* error)
{
    if (guide && (style.value("libraryKind") != "channels" || style.value("libraryView") != "all" ||
                  !style.value("libraryValue").toString().isEmpty()))
    {
        reject(error, "TV Guide uses this profile's configured Kodi PVR TV channels.");
        return false;
    }
    if (!guide && style.value("libraryKind") == "channels")
    {
        reject(error, "Choose a TV Guide component for TV channels.");
        return false;
    }
    return true;
}

// Music shelves cannot impersonate other library families or specify another source.
bool
ComponentSchema::componentValidateLibraryMusic(bool music, const QVariantMap& style, QString* error)
{
    if (music && (style.value("libraryKind") != "albums" || style.value("libraryView") != "all" ||
                  !style.value("libraryValue").toString().isEmpty()))
    {
        reject(error, "Music uses this profile's Kodi albums and tracks.");
        return false;
    }
    if (!music && style.value("libraryKind") == "albums")
    {
        reject(error, "Choose a Music library component for albums and tracks.");
        return false;
    }
    return true;
}

// History-only options belong to the profile's Recently viewed component.
bool
ComponentSchema::componentValidateLibraryHistory(bool history, const QVariantMap& style, QString* error)
{
    if (history && (style.value("libraryKind") != "history" || style.value("libraryView") != "watched" ||
                    !style.value("libraryValue").toString().isEmpty()))
    {
        reject(error, "Recently viewed uses local Kodi viewing history, not a library or recently added view.");
        return false;
    }
    if (!history && style.value("libraryKind") == "history")
    {
        reject(error, "Choose a Recently viewed carousel for viewing history.");
        return false;
    }
    if (!history && (style.contains("historyApps") || style.contains("historySourceBadges")))
    {
        reject(error, "App opening sources belong to Recently viewed.");
        return false;
    }
    return true;
}

// Validate named source requirements before dispatching a shelf to Kodi.
bool
ComponentSchema::componentValidateLibraryViews(const QVariantMap& style, QString* error)
{
    const auto kind = style.value("libraryKind").toString();
    const auto view = style.value("libraryView").toString();
    const auto source = style.value("libraryValue").toString();
    if (!style.contains("libraryKind") || !style.contains("libraryView"))
        reject(error, "Choose a supported Kodi view.");
    else if (QStringList{"collection", "genre", "tag"}.contains(view) && source.trimmed().isEmpty())
        reject(error, "Choose a named Kodi source.");
    else if (kind == "shows" && QStringList{"genre", "tag"}.contains(view))
        reject(error, "This Kodi view is available for movies only.");
    else if (view == "collection" && !safeId(source))
        reject(error, "Choose a saved collection.");
    else
        return true;
    return false;
}

// Limit Games shelves to supported local views and bounded platform names.
bool
ComponentSchema::componentValidateLibraryGames(const QVariantMap& style, QString* error)
{
    if (style.value("libraryKind") != "games")
        return true;
    const auto view = style.value("libraryView").toString();
    if (!QStringList{"all", "recent", "search", "favorites", "platform", "continue"}.contains(view))
        reject(error, "Choose a supported local Games view.");
    else if (view == "platform" &&
             (style.value("libraryValue").metaType().id() != QMetaType::QString ||
              style.value("libraryValue").toString().trimmed().isEmpty() ||
              style.value("libraryValue").toString().size() > 80))
        reject(error, "Choose a platform name of up to 80 characters for this Games shelf.");
    else
        return true;
    return false;
}

QVariantMap
ComponentSchema::componentBuildLibrarySourceLibrary(const QVariantMap& input, const QVariantMap& style, QString* error)
{
    if (input.contains("action"))
    {
        reject(error, "Kodi owns media activation.");
        return {};
    }
    const bool history = input.value("family") == "recently-viewed";
    const bool music = input.value("family") == "music-library";
    const bool guide = input.value("family") == "tv-guide";
    if (!componentValidateLibraryGuide(guide, style, error))
        return {};
    if (!componentValidateLibraryMusic(music, style, error))
        return {};
    if (!componentValidateLibraryHistory(history, style, error))
        return {};
    if (!componentValidateLibraryViews(style, error))
        return {};
    if (!componentValidateLibraryGames(style, error))
        return {};
    QVariantMap source;
    source = {{"schemaVersion", 1},
              {"provider", style.value("libraryKind") == "games" ? "games" : "kodi"},
              {"kind", "library"},
              {"libraryKind", style.value("libraryKind")},
              {"libraryView", style.value("libraryView")},
              {"libraryValue", style.value("libraryValue", "")}};
    if (history)
        source["kind"] = "recently-viewed";
    if (music)
        source["kind"] = "music";
    if (guide)
        source["kind"] = "guide";
    return source;
}

QVariantMap
ComponentSchema::componentBuildLauncherSource(const QVariantMap& input, const QVariantList& normalizedList,
                                                const QString& actionType)
{
    Q_UNUSED(input);
    QVariantMap source = {{"schemaVersion", 1}};
    if (normalizedList.isEmpty())
        return source;
    const auto first = normalizedList.first().toMap();
    if (actionType == "launch-app")
        source.insert({{"provider", "apps"}, {"kind", "app"}, {"id", first.value("appId")}});
    else if (actionType == "open-collection" || actionType == "play-movie" || actionType == "open-show")
        source.insert({{"provider", "kodi"}, {"kind", "collection"}, {"id", first.value("collectionId")}});
    else if (actionType == "open-page")
        source.insert({{"provider", "shell"}, {"kind", "page"}, {"id", first.value("pageId")}});
    else if (actionType == "open-guide")
        source.insert({{"provider", "isolde"}, {"kind", "guide"}});
    else if (actionType == "switch-input")
        source.insert({{"provider", "isolde"},
                       {"kind", "capture-input"},
                       {"id", QString::number(first.value("input").toInt())}});
    else if (actionType.startsWith("open-"))
        source.insert({{"provider", "handler"}, {"kind", actionType}, {"id", first.value("openWith")}});
    else
        source.insert({{"provider", "shell"}, {"kind", actionType}});
    return source;
}
QString
ComponentSchema::unavailable(const QVariantMap& value, const AppRegistry* apps, const QVariantList& collections)
{
    const auto type = value.value("type").toString();
    if (QStringList{"open-page", "open-apps", "open-search", "app-drawer", "root-page"}.contains(type))
        return {};
    if (type == "launch-app")
        return unavailableLaunchApp(value, apps);
    if (type == "open-collection" || type == "play-movie" || type == "open-show")
        return unavailableCollection(value, collections);
    if (type.startsWith("open-") && type != "open-guide")
        return unavailableOpenWith(value, apps);
    return {};
}

QVariantMap
ComponentSchema::componentBuildShellSource(const QVariantMap& input, const QString& type, QString* error)
{
    QVariantMap source;
    if (input.value("kind").toString() != "widget" || input.value("widgetType") != type)
    {
        reject(error, "Unsupported widget.");
        return {};
    }
    if (type == "container" && (input.value("name").toString().trimmed().isEmpty() ||
                                input.value("name").toString().size() > MaxNameLength ||
                                input.value("name").toString().contains(QChar::Null) ||
                                !validChildLayout(input.value("presentation").toMap().value("layout").toMap())))
    {
        reject(error, "A group needs a name and a bounded internal layout.");
        return {};
    }
    source = {{"schemaVersion", 1}, {"provider", "shell"}, {"kind", type}};
    if (QStringList{"detected-sources", "source-notice"}.contains(input.value("family").toString()))
        source = {{"schemaVersion", 1}, {"provider", "sources"}, {"kind", input.value("family")}};
    return source;
}

QVariantMap
ComponentSchema::componentBuildLauncherSource(const QVariantMap& input, const QVariantList& normalizedList,
                                                const QString& actionType, QString* error)
{
    Q_UNUSED(input);
    Q_UNUSED(error);
    QVariantMap source = {{"schemaVersion", 1}};
    if (normalizedList.isEmpty())
        return source;
    const auto first = normalizedList.first().toMap();
    if (actionType == "launch-app")
        source.insert({{"provider", "apps"}, {"kind", "app"}, {"id", first.value("appId")}});
    else if (actionType == "open-collection" || actionType == "play-movie" || actionType == "open-show")
        source.insert({{"provider", "kodi"}, {"kind", "collection"}, {"id", first.value("collectionId")}});
    else if (actionType == "open-page")
        source.insert({{"provider", "shell"}, {"kind", "page"}, {"id", first.value("pageId")}});
    else if (actionType == "open-guide")
        source.insert({{"provider", "isolde"}, {"kind", "guide"}});
    else if (actionType == "switch-input")
        source.insert({{"provider", "isolde"},
                       {"kind", "capture-input"},
                       {"id", QString::number(first.value("input").toInt())}});
    else if (actionType.startsWith("open-"))
        source.insert({{"provider", "handler"}, {"kind", actionType}, {"id", first.value("openWith")}});
    else
        source.insert({{"provider", "shell"}, {"kind", actionType}});
    return source;
}

QString
ComponentSchema::unavailableShellRoutes(const QString& type)
{
    Q_UNUSED(type);
    return {};
}

QString
ComponentSchema::unavailableLaunchApp(const QVariantMap& value, const AppRegistry* apps)
{
    return apps->find(value.value("appId").toString()) ? QString() : "App missing · repair source";
}

QString
ComponentSchema::unavailableCollection(const QVariantMap& value, const QVariantList& collections)
{
    for (const auto& entry : collections)
    {
        const auto definition = entry.toMap();
        if (definition.value("id") != value.value("collectionId"))
            continue;
        const auto type = value.value("type").toString();
        if (type == "open-show" && definition.value("media", "movies").toString() != "shows")
            break;
        if (type == "play-movie" && definition.value("media", "movies").toString() != "movies")
            break;
        return {};
    }
    return "Collection missing · repair source";
}

QString
ComponentSchema::unavailableOpenWith(const QVariantMap& value, const AppRegistry* apps)
{
    const auto type = value.value("type").toString();
    const auto handler = apps->find(value.value("openWith").toString());
    if (!handler)
        return "Open With app missing · repair source";
    if (!handler->handledActions.contains(type))
        return "Incompatible Open With app · repair source";
    return {};
}

bool
ComponentSchema::placementsFit(const QJsonArray& items, const QVariantMap& grid)
{
    const int columns = grid.value("columns").toInt(), rows = grid.value("rows").toInt();
    QList<QRect> placements;
    placements.reserve(items.size());
    for (const auto& entry : items)
    {
        const auto item = entry.toObject();
        if (!item.contains("placement"))
            continue;
        const auto placement = item.value("placement").toObject().toVariantMap();
        if (!validPlacement(placement, columns, rows))
            return false;
        placements.append(QRect(placement.value("x").toInt(),
                                placement.value("y").toInt(),
                                placement.value("width").toInt(),
                                placement.value("height").toInt()));
    }
    if (grid.value("overlap").toBool())
        return true;
    // Decode each validated rectangle once. Movement previews validate this
    // boundary every frame; converting JSON/maps inside the pair loop is costly.
    for (qsizetype i = 0; i < placements.size(); ++i)
        for (qsizetype j = i + 1; j < placements.size(); ++j)
            if (placements[i].intersects(placements[j]))
                return false;
    return true;
}

QVariantMap
ComponentSchema::sectionGrid(const QVariantMap& composition, const QJsonArray& categories, const QString& categoryId)
{
    auto grid = normalizedGrid(composition.value("grid").toMap());
    if (grid.value("mode") == "screen" && composition.value("verticalScrolling").toBool())
        grid["rows"] = MaxScrollableRows;
    if (grid.value("mode") != "sections")
        return grid;
    for (const auto& entry : categories)
    {
        const auto category = entry.toObject();
        if (category["id"] != categoryId)
            continue;
        const auto override = category["presentation"].toObject()["grid"].toObject().toVariantMap();
        for (auto it = override.cbegin(); it != override.cend(); ++it)
            grid[it.key()] = it.value();
    }
    return grid;
}

QVariantMap
ComponentSchema::componentGrid(const QJsonArray& items,
                               const QVariantMap& composition,
                               const QJsonArray& categories,
                               const QString& parentId,
                               const QString& categoryId)
{
    if (parentId.isEmpty())
        return sectionGrid(composition, categories, categoryId);
    for (const auto& entry : items)
    {
        const auto item = entry.toObject();
        if (item["id"] != parentId)
            continue;
        if (item["componentType"] != "container")
            return {};
        const auto layout = item["presentation"].toObject()["layout"].toObject();
        return {{"columns", layout["columns"].toInt()},
                {"rows", layout["rows"].toInt()},
                {"gap", layout["gap"].toDouble()},
                {"margin", layout["padding"].toDouble()},
                {"overlap", false},
                {"snap", true},
                {"mode", "screen"}};
    }
    return {};
}
QString
ComponentSchema::effectiveCategory(const QJsonArray& items, const QString& id)
{
    auto current = id;
    QSet<QString> visited;
    for (int depth = 0; depth <= MaxComponentNestingDepth && !current.isEmpty(); ++depth)
    {
        if (visited.contains(current))
            return {};
        visited.insert(current);
        bool found = false;
        for (const auto& entry : items)
        {
            const auto item = entry.toObject();
            if (item["id"] != current)
                continue;
            found = true;
            const auto parent = item.value("parentId").toString();
            if (parent.isEmpty())
                return item.value("categoryId").toString();
            current = parent;
            break;
        }
        if (!found)
            return {};
    }
    return {};
}
bool
ComponentSchema::positionLocked(const QJsonArray& items, const QString& id)
{
    return positionLockedWalk(items, id, QSet<QString>(), 0);
}

bool
ComponentSchema::positionLockedWalk(const QJsonArray& items, QString current, QSet<QString> visited,
                                    int depth)
{
    if (positionLockedShouldStop(current, depth))
        return positionLockedStopResult(current, depth);
    if (positionLockedCycleCheck(visited, current))
        return true;
    visited.insert(current);
    QJsonObject item;
    if (!positionLockedFindItem(items, current, item))
        return true;
    return positionLockedContinue(items, item, visited, depth);
}

bool
ComponentSchema::positionLockedShouldStop(const QString& current, int depth)
{
    return current.isEmpty() || depth > MaxComponentNestingDepth;
}

bool
ComponentSchema::positionLockedStopResult(const QString& current, int depth)
{
    Q_UNUSED(current);
    return depth > MaxComponentNestingDepth;
}

bool
ComponentSchema::positionLockedContinue(const QJsonArray& items, const QJsonObject& item,
                                        QSet<QString> visited, int depth)
{
    if (positionLockedIsLocked(item))
        return true;
    return positionLockedWalk(items, item.value("parentId").toString(), visited, depth + 1);
}

bool
ComponentSchema::positionLockedCycleCheck(const QSet<QString>& visited, const QString& current)
{
    return visited.contains(current);
}

bool
ComponentSchema::positionLockedFindItem(const QJsonArray& items, const QString& current, QJsonObject& item)
{
    for (const auto& entry : items)
    {
        const auto candidate = entry.toObject();
        if (candidate["id"] != current)
            continue;
        item = candidate;
        return true;
    }
    return false;
}

bool
ComponentSchema::positionLockedIsLocked(const QJsonObject& item)
{
    return item.value("locked").toBool();
}

bool
ComponentSchema::hierarchyFits(const QJsonArray& items,
                               const QVariantMap& composition,
                               const QJsonArray& categories,
                               QString* error)
{
    QHash<QString, QJsonObject> byId;
    QHash<QString, QJsonArray> children;
    QJsonArray roots;
    bool containers = false;
    if (error)
        error->clear();
    if (!hierarchyFitsValidateIds(byId, children, roots, containers, items, error) ||
        !hierarchyFitsValidateParents(byId, children, error))
        return false;
    if (containers && (!composition.value("enabled").toBool() || !composition.contains("grid")))
    {
        if (error)
            *error = "Groups require an enabled page grid. Preview grid conversion first.";
        return false;
    }
    return
           hierarchyFitsValidateContainers(byId, children, composition, categories, error) &&
           hierarchyFitsValidateRoots(byId, roots, composition, categories, error);
}

bool
ComponentSchema::hierarchyFitsValidateIds(QHash<QString, QJsonObject>& byId, QHash<QString, QJsonArray>& children,
                                          QJsonArray& roots, bool& containers, const QJsonArray& items,
                                          QString* error)
{
    const auto fail = [&](const QString& message)
    {
        if (error)
            *error = message;
        return false;
    };
    if (error)
        error->clear();
    for (const auto& entry : items)
    {
        const auto item = entry.toObject();
        const auto id = item["id"].toString();
        if (!safeId(id) || byId.contains(id))
            return fail("A component identity is invalid or duplicated.");
        byId[id] = item;
        if (item["componentType"] == "container")
            containers = true;
    }
    for (const auto& entry : items)
    {
        const auto item = entry.toObject();
        const auto id = item["id"].toString(), parent = item.value("parentId").toString();
        if (parent.isEmpty())
            roots.append(item);
        else
        {
            if (!item.value("categoryId").toString().isEmpty())
                return fail("A nested component inherits its section from its parent.");
            if (!item.contains("placement"))
                return fail("Every nested component needs a local placement.");
            children[parent].append(item);
        }
    }
    return true;
}

bool
ComponentSchema::hierarchyFitsValidateParents(const QHash<QString, QJsonObject>& byId,
                                              const QHash<QString, QJsonArray>& children, QString* error)
{
    Q_UNUSED(children);
    const auto fail = [&](const QString& message)
    {
        if (error)
            *error = message;
        return false;
    };
    for (const auto& entry : byId)
    {
        const auto item = entry;
        const auto parent = item.value("parentId").toString();
        QSet<QString> visited{item["id"].toString()};
        auto ancestor = parent;
        int depth = item["componentType"] == "container" ? 1 : 0;
        while (!ancestor.isEmpty())
        {
            if (visited.contains(ancestor))
                return fail("A group cannot contain itself or one of its parents.");
            if (!byId.contains(ancestor) || byId[ancestor]["componentType"] != "container")
                return fail("Choose an existing group as the parent.");
            visited.insert(ancestor);
            if (++depth > MaxComponentNestingDepth)
                return fail("Groups support at most four nested container levels.");
            ancestor = byId[ancestor].value("parentId").toString();
        }
    }
    return true;
}

bool
ComponentSchema::hierarchyFitsValidateContainers(const QHash<QString, QJsonObject>& byId,
                                                 const QHash<QString, QJsonArray>& children,
                                                 const QVariantMap& composition, const QJsonArray& categories,
                                                 QString* error)
{
    const auto fail = [&](const QString& message)
    {
        if (error)
            *error = message;
        return false;
    };
    Q_UNUSED(byId);
    for (auto it = byId.cbegin(); it != byId.cend(); ++it)
    {
        if (!hierarchyFitsValidateContainer(it.value(), children, composition, categories, it.key(), fail))
            return false;
    }
    return true;
}

bool
ComponentSchema::hierarchyFitsValidateContainer(const QJsonObject& item,
                                                const QHash<QString, QJsonArray>& children,
                                                const QVariantMap& composition, const QJsonArray& categories,
                                                const QString& id,
                                                const std::function<bool(const QString&)>& fail)
{
    if (item["componentType"] != "container")
        return true;
    const auto kids = children.value(id);
    if (item.contains("appCollection") && !kids.isEmpty())
        return fail("An app collection uses registry membership, not placed children.");
    if (QStringList{"page-menu","page-panel"}.contains(item.value("family").toString()) && !kids.isEmpty())
        return fail("A page menu uses page membership, not placed children.");
    if (QStringList{"detected-sources", "source-notice"}.contains(item.value("family").toString()) &&
        !kids.isEmpty())
        return fail("A detected-source component projects device facts, not placed children.");
    if (item.value("family") == "carousel")
    {
        if (kids.size() > 128)
            return fail("A button carousel supports up to 128 buttons.");
        for (const auto& child : kids)
            if (child.toObject()["componentType"] != "button")
                return fail("A button carousel contains buttons. Use a group for other components.");
    }
    if (!item.contains("placement") ||
        !validChildLayout(item["presentation"].toObject()["layout"].toObject().toVariantMap()))
        return fail("A group needs a bounded placement and internal layout.");
    if (!placementsFit(kids, componentGrid(QJsonArray{item}, composition, categories, id, {})))
        return fail("Group contents overlap or do not fit their local grid.");
    return true;
}

bool
ComponentSchema::hierarchyFitsValidateRoots(const QHash<QString, QJsonObject>& byId, const QJsonArray& items,
                                            const QVariantMap& composition, const QJsonArray& categories,
                                            QString* error)
{
    Q_UNUSED(byId);
    Q_UNUSED(items);
    const auto fail = [&](const QString& message)
    {
        if (error)
            *error = message;
        return false;
    };
    const auto grid = sectionGrid(composition, categories, {});
    if (grid.value("mode") != "sections")
        return placementsFit(items, grid) ? true : fail("Page components overlap or do not fit the grid.");
    QHash<QString, QJsonArray> groups;
    for (const auto& entry : items)
    {
        const auto obj = entry.toObject();
        if (obj.value("parentId").toString().isEmpty())
        {
            if (!obj.contains("placement"))
                return fail("Every section component needs a placement.");
            groups[obj["categoryId"].toString()].append(entry);
        }
    }
    for (const auto& entry : categories)
        if (entry.toObject()["presentation"].toObject()["variant"] == "library" &&
            !groups.value(entry.toObject()["id"].toString()).isEmpty())
            return fail("A media shelf manages its own contents.");
    for (auto it = groups.cbegin(); it != groups.cend(); ++it)
        if (!placementsFit(it.value(), sectionGrid(composition, categories, it.key())))
            return fail("Section components overlap or do not fit their grid.");
    return true;
}

bool
ComponentSchema::layoutPlacementsFit(const QJsonArray& items,
                                     const QVariantMap& composition,
                                     const QJsonArray& categories)
{
    return hierarchyFits(items, composition, categories);
}
