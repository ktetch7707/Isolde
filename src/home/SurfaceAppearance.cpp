#include "home/ComponentSchema.h"

#include <QColor>
#include <QSet>
#include <QtMath>

namespace {
double luminance(const QColor &color)
{
    const auto linear = [](double v) { return v <= 0.04045 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

double contrast(const QColor &a, const QColor &b)
{
    const double x = luminance(a), y = luminance(b);
    return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}

QString readable(const QVariant &candidate, const QColor &surface, double minimum)
{
    const QColor color(candidate.toString());
    if (color.isValid() && color.alpha() == 255 && contrast(color, surface) >= minimum)
        return color.name();
    return contrast(Qt::white, surface) > contrast(Qt::black, surface) ? "#ffffff" : "#000000";
}

const QSet<QString> colorRoles{
    "backgroundStart", "backgroundEnd", "surfaceColor", "selectedColor", "textColor", "mutedColor",
    "focusColor", "elevatedColor", "outlineColor", "accentColor", "inputColor", "inputTextColor",
    "scrimColor", "successColor", "warningColor", "errorColor", "unavailableColor"};

bool validTokens(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QVariantMap) return false;
    const auto tokens = value.toMap();
    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it) {
        if (colorRoles.contains(it.key())) {
            if (it.value().metaType().id() != QMetaType::QString) return false;
            const QColor color(it.value().toString());
            if (!color.isValid() || color.alpha() != 255) return false;
        } else if (it.key() == "motionEnabled" || it.key() == "highContrast") {
            if (it.value().metaType().id() != QMetaType::Bool) return false;
        } else if (it.key() == "fontFamily") {
            if (it.value().metaType().id() != QMetaType::QString || it.value().toString().trimmed().isEmpty()
                || it.value().toString().size() > 80 || it.value().toString().contains(QChar::Null)) return false;
        } else {
            double low = 0, high = 1;
            if (it.key() == "typographyScale") { low = 0.8; high = 1.3; }
            else if (it.key() == "spacing") { low = 8; high = 40; }
            else if (it.key() == "cornerRadius") { low = 0; high = 48; }
            else if (it.key() != "scrimOpacity" && it.key() != "panelOpacity" && it.key() != "tileRoundness") return false;
            if (it.value().metaType().id() == QMetaType::Bool || it.value().metaType().id() == QMetaType::QString) return false;
            bool ok = false;
            const double number = it.value().toDouble(&ok);
            if (!ok || !qIsFinite(number) || number < low || number > high) return false;
        }
    }
    return true;
}
}

bool ComponentSchema::validSurfaceThemes(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QVariantMap) return false;
    const auto policies = value.toMap();
    for (auto it = policies.cbegin(); it != policies.cend(); ++it) {
        if (!QStringList{"search", "apps", "settings", "kodi"}.contains(it.key())
            || it.value().metaType().id() != QMetaType::QVariantMap) return false;
        const auto policy = it.value().toMap();
        for (auto key = policy.cbegin(); key != policy.cend(); ++key)
            if (!QStringList{"schemaVersion", "mode", "tokens"}.contains(key.key())) return false;
        if (policy.value("schemaVersion").metaType().id() == QMetaType::Bool
            || policy.value("schemaVersion").metaType().id() == QMetaType::QString
            || policy.value("schemaVersion").toDouble() != 1.0) return false;
        const auto mode = policy.value("mode").toString();
        if (!QStringList{"home", "origin", "custom"}.contains(mode) || (it.key() == "settings" && mode == "origin")) return false;
        if (policy.contains("tokens") && !validTokens(policy.value("tokens"))) return false;
        if (mode != "custom" && !policy.value("tokens").toMap().isEmpty()) return false;
    }
    return true;
}

bool ComponentSchema::readableSurfaceThemes(const QVariantMap &home, const QVariantMap &origin)
{
    auto policies = home.value("surfaceThemes").toMap();
    const auto local = origin.value("surfaceThemes").toMap();
    for (auto it = local.cbegin(); it != local.cend(); ++it)
        if (it.key() != "settings") policies[it.key()] = it.value();
    for (auto it = policies.cbegin(); it != policies.cend(); ++it) {
        const auto policy = it.value().toMap();
        if (policy.value("mode") != "custom") continue;
        auto effective = it.key() == "settings" ? home : origin;
        const auto tokens = policy.value("tokens").toMap();
        for (auto token = tokens.cbegin(); token != tokens.cend(); ++token) effective[token.key()] = token.value();
        const auto resolved = surfacePalette(effective);
        for (const auto &role : QStringList{"textColor", "mutedColor", "focusColor", "inputTextColor",
                                          "outlineColor", "accentColor", "successColor", "warningColor", "errorColor", "unavailableColor"})
            if (tokens.contains(role) && QColor(tokens[role].toString()) != QColor(resolved[role].toString())) return false;
    }
    return true;
}

QVariantMap ComponentSchema::surfacePalette(const QVariantMap &appearance)
{
    auto result = appearance;
    result.remove("surfaceThemes");
    const auto fallback = [&](const QString &key, const QVariant &value) {
        if (!result.contains(key)) result[key] = value;
    };
    fallback("surfaceColor", "#202126");
    fallback("selectedColor", "#353039");
    fallback("backgroundStart", result["surfaceColor"]);
    fallback("backgroundEnd", result["surfaceColor"]);
    fallback("elevatedColor", result["surfaceColor"]);
    fallback("inputColor", result["selectedColor"]);
    // Resolve translucent panels against their declared opaque background.
    // Consumers receive an opaque, contrast-tested color, never child opacity.
    const double panelOpacity = result.value("panelOpacity", 1.0).toDouble();
    const QColor backdrop(result["backgroundEnd"].toString());
    for (const auto &role : QStringList{"surfaceColor", "selectedColor", "elevatedColor", "inputColor"}) {
        const QColor panel(result[role].toString());
        result[role] = QColor::fromRgbF(panel.redF() * panelOpacity + backdrop.redF() * (1 - panelOpacity),
                                      panel.greenF() * panelOpacity + backdrop.greenF() * (1 - panelOpacity),
                                      panel.blueF() * panelOpacity + backdrop.blueF() * (1 - panelOpacity)).name();
    }
    const QColor surface(result["surfaceColor"].toString());
    const QColor selected(result["selectedColor"].toString());
    const QColor elevated(result["elevatedColor"].toString());
    // Old palettes may contain low-contrast text. Resolve readable shell roles
    // without changing or persisting any of the user's stored values.
    const bool highContrast = result.value("highContrast").toBool();
    const double textMinimum = highContrast ? 7.0 : 4.5;
    for (const auto &key : QStringList{"textColor", "mutedColor"})
        result[key] = readable(result.value(key), surface, textMinimum);
    result["selectedTextColor"] = readable(result["textColor"], selected, textMinimum);
    result["elevatedTextColor"] = readable(result["textColor"], elevated, textMinimum);
    result["elevatedMutedColor"] = readable(result["mutedColor"], elevated, textMinimum);
    result["elevatedFocusColor"] = readable(result["focusColor"], elevated, 3.0);
    result["backgroundTextColor"] = readable(result["textColor"], backdrop, textMinimum);
    result["backgroundMutedColor"] = readable(result["mutedColor"], backdrop, textMinimum);
    result["inputTextColor"] = readable(result.value("inputTextColor", result["textColor"]), QColor(result["inputColor"].toString()), textMinimum);
    result["focusColor"] = readable(result.value("focusColor", "#c994a5"), surface, 3.0);
    result["selectedFocusColor"] = readable(result["focusColor"], selected, 3.0);
    for (const auto &key : QStringList{"outlineColor", "accentColor", "unavailableColor"})
        result[key] = readable(result.value(key, result["mutedColor"]), surface, 3.0);
    const QVariantMap statuses{{"successColor", "#27834a"}, {"warningColor", "#9a6700"}, {"errorColor", "#cb3247"}};
    for (auto it = statuses.cbegin(); it != statuses.cend(); ++it)
        result[it.key()] = readable(result.value(it.key(), it.value()), surface, textMinimum);
    fallback("scrimColor", "#000000");
    fallback("scrimOpacity", 0.85);
    fallback("panelOpacity", 1.0);
    fallback("tileRoundness", DefaultTileRoundness);
    fallback("cornerRadius", 4.0 + 12.0 * result["tileRoundness"].toDouble());
    fallback("typographyScale", 1.0);
    fallback("spacing", 20);
    fallback("fontFamily", "DejaVu Sans");
    fallback("motionEnabled", true);
    fallback("highContrast", false);
    return result;
}
