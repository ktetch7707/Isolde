#include "home/ComponentSchema.h"
#include "home/DesignSystem.h"
#include "home/HomeLayout.h"
#include "home/ManagedGif.h"
#include "home/ThemeColors.h"
#include "settings/ProfileManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QtMath>
#include <algorithm>

namespace
{

QVariantMap
appearanceDefaults()
{
    return {{"focusColor", "#c994a5"},
            {"surfaceColor", "#202126"},
            {"selectedColor", "#353039"},
            {"textColor", "#f5f1f3"},
            {"mutedColor", "#aaa5af"},
            {"typographyScale", 1.0},
            {"homeColumns", 3},
            {"spacing", 20},
            {"motionEnabled", true},
            {"highContrast", false},
            {"fontFamily", "DejaVu Sans"}};
}

bool
normalizeAppearance(QVariantMap& values)
{
    const auto defaults = appearanceDefaults();
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        const auto key = it.key();
        if (!validateAppearanceKey(values, key, defaults))
            return false;
    }
    return true;
}

bool
validateAppearanceKey(QVariantMap& values, const QString& key, const QVariantMap& defaults)
{
    if (validateColorThemeKey(values, key))
        return true;
    if (validateSourcePolicyKey(values, key))
        return true;
    if (validateSurfaceThemesKey(values, key))
        return true;
    if (validateColorKey(values, key))
        return true;
    if (validateFontFamilyKey(values, key))
        return true;
    if (validateBooleanKey(values, key))
        return true;
    if (validateNumericKey(values, key, defaults))
        return true;
    return validateUnknownKey(key);
}

bool
validateColorThemeKey(QVariantMap& values, const QString& key)
{
    if (key != "colorTheme")
        return false;
    if (!ThemeColors::valid(values.value(key)))
        return false;
    return true;
}

bool
validateSourcePolicyKey(QVariantMap& values, const QString& key)
{
    if (key != "sourcePolicy")
        return false;
    if (values.value(key).metaType().id() != QMetaType::QVariantMap ||
        !ComponentSchema::validSourcePolicy(values.value(key).toMap()))
        return false;
    return true;
}

bool
validateSurfaceThemesKey(QVariantMap& values, const QString& key)
{
    if (key != "surfaceThemes")
        return false;
    if (!ComponentSchema::validSurfaceThemes(values.value(key)))
        return false;
    return true;
}

bool
validateColorKey(QVariantMap& values, const QString& key)
{
    if (!key.endsWith("Color") && key != "backgroundStart" && key != "backgroundEnd")
        return false;
    if (!validateColorValue(values.value(key)))
        return false;
    values[key] = QColor(values.value(key).toString()).name();
    return true;
}

bool
validateFontFamilyKey(QVariantMap& values, const QString& key)
{
    if (key != "fontFamily")
        return false;
    if (!validateFontFamilyValue(values.value(key)))
        return false;
    return true;
}

bool
validateBooleanKey(QVariantMap& values, const QString& key)
{
    if (key != "motionEnabled" && key != "highContrast")
        return false;
    if (!validateBooleanValue(values.value(key)))
        return false;
    return true;
}

bool
validateNumericKey(QVariantMap& values, const QString& key, const QVariantMap& defaults)
{
    if (!validateNumericValue(values.value(key), key, defaults))
        return false;
    return true;
}

bool
validateUnknownKey(const QString& key, const QVariantMap& defaults)
{
    return defaults.contains(key) || ThemeColors::role(key) || key == "tileRoundness";
}

bool
validateColorValue(const QVariant& value)
{
    const QColor color(value.toString());
    return color.isValid() && color.alpha() == 255;
}

bool
validateFontFamilyValue(const QVariant& value)
{
    return value.metaType().id() == QMetaType::QString && !value.toString().trimmed().isEmpty() &&
           value.toString().size() <= HomeLayout::MaxPageNameLength && !value.toString().contains(QChar::Null);
}

bool
validateBooleanValue(const QVariant& value)
{
    return value.metaType().id() == QMetaType::Bool;
}

bool
validateNumericValue(const QVariant& value, const QString& key, const QVariantMap& defaults)
{
    if (!validateNumericKeyDefaults(key, defaults))
        return false;
    if (!validateNumericValueType(value))
        return false;
    bool ok = false;
    const double number = value.toDouble(&ok);
    const double low = numericRangeLow(key);
    const double high = numericRangeHigh(key);
    if (!isValidNumber(number, ok, low, high, key))
        return false;
    return true;
}

bool
validateNumericKeyDefaults(const QString& key, const QVariantMap& defaults)
{
    return defaults.contains(key) || key == "tileRoundness";
}

bool
validateNumericValueType(const QVariant& value)
{
    return value.metaType().id() != QMetaType::QString && value.metaType().id() != QMetaType::Bool;
}

double
numericRangeLow(const QString& key)
{
    if (key == "typographyScale")
        return HomeLayout::MinTypographyScale;
    if (key == "homeColumns")
        return HomeLayout::MinHomeColumns;
    if (key == "spacing")
        return HomeLayout::MinSpacing;
    return 0;
}

double
numericRangeHigh(const QString& key)
{
    if (key == "typographyScale")
        return HomeLayout::MaxTypographyScale;
    if (key == "homeColumns")
        return HomeLayout::MaxHomeColumns;
    if (key == "spacing")
        return HomeLayout::MaxSpacing;
    return 1;
}

bool
isValidNumber(double number, bool ok, double low, double high, const QString& key)
{
    if (!ok || !qIsFinite(number) || number < low || number > high)
        return false;
    if ((key == "homeColumns" || key == "spacing") && number != qFloor(number))
        return false;
    return true;
}

double
contrast(const QColor& a, const QColor& b)
{
    const auto luminance = [](const QColor& color)
    {
        const auto linear = [](double channel)
        {
            return channel <= 0.04045 ? channel / 12.92 : qPow((channel + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    };
    const double x = luminance(a), y = luminance(b);
    return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}
bool
readableStudioAppearance(QVariantMap values)
{
    if (values.contains("recentColors") && !ComponentSchema::validRecentStudioColors(values.take("recentColors")))
        return false;
    if (!normalizeAppearance(values))
        return false;
    return validateSurfaceColors(values) && validateFocusColor(values);
}

bool
validateSurfaceColors(QVariantMap values)
{
    const auto surface = QColor(values.value("surfaceColor").toString()),
               selected = QColor(values.value("selectedColor").toString());
    for (const auto& key : QStringList{"textColor", "mutedColor"})
    {
        const auto ink = QColor(values.value(key).toString());
        if (!ink.isValid() || !surface.isValid() || !selected.isValid() ||
            contrast(ink, surface) < HomeLayout::MinTextContrast ||
            contrast(ink, selected) < HomeLayout::MinTextContrast)
            return false;
    }
    return true;
}

bool
validateFocusColor(QVariantMap values)
{
    const auto focus = QColor(values.value("focusColor").toString());
    const auto surface = QColor(values.value("surfaceColor").toString());
    const auto selected = QColor(values.value("selectedColor").toString());
    return focus.isValid() && contrast(focus, surface) >= HomeLayout::MinFocusContrast &&
           contrast(focus, selected) >= HomeLayout::MinFocusContrast;
}

bool
isSafeActionId(const QString& id)
{
    return id.isEmpty() || ComponentSchema::safeId(id);
}
bool
isSupportedActionType(const QString& type)
{
    static const QSet<QString> actionTypes{QStringLiteral("launch-app"),
                                           QStringLiteral("open-folder"),
                                           QStringLiteral("open-file"),
                                           QStringLiteral("open-url"),
                                           QStringLiteral("open-disc"),
                                           QStringLiteral("desktop-mode"),
                                           QStringLiteral("switch-input"),
                                           QStringLiteral("wait")};
    return actionTypes.contains(type);
}

QString
defaultActionTypeForKind(const QJsonObject& item)
{
    if (item.value(QStringLiteral("kind")).toString() == QStringLiteral("app"))
        return QStringLiteral("launch-app");
    return {};
}

QVariantList
normalizedActionForItem(const QJsonObject& item)
{
    auto actionList = item.value(QStringLiteral("actions")).toArray();
    if (actionList.isEmpty() && item.value("kind") == "app")
        actionList = QJsonArray{QJsonObject{{"type", "launch-app"}, {"appId", item.value("appId")}}};
    QVariantList result;
    for (const auto& entry : actionList)
    {
        auto normalized = ComponentSchema::action(entry.toObject().toVariantMap());
        if (!normalized.isEmpty())
            result.append(normalized);
    }
    return result;
}

} // namespace

HomeLayout::HomeLayout(AppRegistry* registry, QObject* parent) : QObject(parent), m_registry(registry)
{
    connect(this,
            &HomeLayout::layoutChanged,
            this,
            [this]
            {
                m_registryItems = items();
            });
    connect(this, &HomeLayout::layoutChanged, this, &HomeLayout::syncPages);
    connect(m_registry,
            &AppRegistry::appsChanged,
            this,
            [this]
            {
                if (items() != m_registryItems)
                    emit layoutChanged();
            });
}

QVariantList
HomeLayout::items() const
{
    QVariantList result;
    const auto hidden = computeHiddenIds();
    for (const auto& categoryValue : m_categories)
    {
        const auto categoryId = categoryValue.toObject().value("id").toString();
        for (const auto& value : m_items)
            if (value.toObject().value("categoryId").toString() == categoryId)
                appendItem(value, result, hidden);
    }
    for (const auto& value : m_items)
        if (value.toObject().value("categoryId").toString().isEmpty())
            appendItem(value, result, hidden);
    return result;
}

QSet<QString>
HomeLayout::computeHiddenIds() const
{
    QSet<QString> hidden;
    if (!m_draftOnly)
    {
        for (const auto& entry : m_items)
            if (entry.toObject().value("hidden").toBool())
                hidden.insert(entry.toObject()["id"].toString());
        for (int depth = 0; depth < ComponentSchema::MaxComponentNestingDepth; ++depth)
            for (const auto& entry : m_items)
            {
                const auto item = entry.toObject();
                if (hidden.contains(item.value("parentId").toString()))
                    hidden.insert(item["id"].toString());
            }
    }
    return hidden;
}

bool
HomeLayout::appendItem(const QJsonValue& value, QVariantList& result, const QSet<QString>& hidden)
{
    const auto object = value.toObject();
    if (hidden.contains(object["id"].toString()))
        return false;
    auto item = object.toVariantMap();
    item["presentation"] = componentPresentation(item.value("presentation").toMap());
    const auto actionList = normalizedActionForItem(object);
    if (!actionList.isEmpty())
    {
        item.insert("actionType", actionList.first().toMap().value(QStringLiteral("type")).toString());
        item.insert("actions", actionList);
    }
    setItemKindFields(item, object);
    setItemLogo(item, object, actionList);
    setItemUnavailableReason(item, object, actionList);
    result.append(ComponentSchema::component(item, true));
    return true;
}

void
HomeLayout::setItemKindFields(QVariantMap& item, const QJsonObject& object)
{
    const auto kind = item.value("kind").toString();
    if (kind == QStringLiteral("app"))
        setItemAppFields(item);
    else if (kind == QStringLiteral("action"))
        setItemActionFields(item);
}

void
HomeLayout::setItemAppFields(QVariantMap& item)
{
    const auto appId = item.value("appId").toString();
    const auto* app = m_registry->find(appId);
    item.insert("name", !item.value("name").toString().isEmpty() ? item.value("name") : app ? app->name : appId);
    item.insert("section", app ? app->section : QStringLiteral("Missing app"));
    const auto tileSymbol = item.value("symbol").toString();
    const auto tileAccent = item.value("accent").toString();
    item.insert("symbol", tileSymbol.isEmpty() ? (app ? app->symbol : "?") : tileSymbol);
    item.insert("accent", tileAccent.isEmpty() ? (app ? app->accent.name() : "#64748B") : tileAccent);
    item.insert("presentationMode", app ? app->presentationMode : "native-tv");
    item.insert("networkRequirement", app ? app->networkRequirement : "none");
}

void
HomeLayout::setItemActionFields(QVariantMap& item)
{
    const auto section = item.value("section").toString();
    const auto symbol = item.value("symbol").toString();
    const auto accent = item.value("accent").toString();
    const auto networkRequirement = item.value("networkRequirement").toString();
    item.insert("section", section.isEmpty() ? tr("Custom") : section);
    item.insert("symbol", symbol.isEmpty() ? QStringLiteral("•") : symbol);
    item.insert("accent", accent.isEmpty() ? QStringLiteral("#64748B") : accent);
    item.insert("networkRequirement", networkRequirement.isEmpty() ? QStringLiteral("none") : networkRequirement);
}

void
HomeLayout::setItemLogo(QVariantMap& item, const QJsonObject& object, const QVariantList& actionList)
{
    const auto logoFile = item.value("customLogo").toString();
    auto logo = resolveArtwork(logoFile);
    if (logoFile.isEmpty() && !actionList.isEmpty() && actionList.first().toMap().value("type") == "launch-app")
    {
        const auto actionMap = actionList.first().toMap();
        if (const auto* app = m_registry->find(actionMap.value("appId").toString()))
        {
            const auto mode = item.value("presentation").toMap().value("logoMode").toString();
            logo = mode == "monochrome" && !app->inkIconSource.isEmpty()  ? app->inkIconSource
                   : mode == "cutout" && !app->cutoutIconSource.isEmpty() ? app->cutoutIconSource
                                                                          : app->iconSource;
            item["cutoutLogoSource"] = app->cutoutIconSource;
            item["inkLogoSource"] = app->inkIconSource;
            item["inkAvailable"] = !app->inkIconSource.isEmpty();
            item["iconOpticalScale"] = app->iconOpticalScale;
            item["iconContrastBackdrop"] = app->iconContrastBackdrop;
        }
    }
    item.insert("logoSource", logo);
}

void
HomeLayout::setItemUnavailableReason(QVariantMap& item, const QJsonObject& object, const QVariantList& actionList)
{
    unavailableReasonFromSchema(item, actionList);
    unavailableReasonFromSwitchInput(item, actionList);
    unavailableReasonFromCapability(item, object);
    item.insert("unavailableReason", unavailableReasonValue(item));
}

void
HomeLayout::unavailableReasonFromSchema(QVariantMap& item, const QVariantList& actionList)
{
    item["unavailableReason"] = ComponentSchema::unavailable(
        actionList.isEmpty() ? QVariantMap() : actionList.first().toMap(), m_registry, m_collections);
}

void
HomeLayout::unavailableReasonFromSwitchInput(QVariantMap& item, const QVariantList& actionList)
{
    if (unavailableReasonValue(item).isEmpty() && !actionList.isEmpty() &&
        actionList.first().toMap().value("type").toString() == "switch-input" && !m_captureInputsReady)
        item["unavailableReason"] = "Configure Settings → System → HDMI Inputs";
}

void
HomeLayout::unavailableReasonFromCapability(QVariantMap& item, const QJsonObject& object)
{
    if (unavailableReasonValue(item).isEmpty() && object.value("requiredCapability") == "capture-inputs" &&
        !m_captureInputsReady)
        item["unavailableReason"] = tr("HDMI capture unavailable · configure inputs in Settings");
}

QString
HomeLayout::unavailableReasonValue(const QVariantMap& item)
{
    return item.value("unavailableReason").toString();
}

QVariantMap
HomeLayout::componentPresentation(const QVariantMap& authored) const
{
    const auto library = (m_owner ? m_owner->m_composition : m_composition).value("designLibrary").toMap();
    return DesignSystem::presentation(authored, library);
}
QVariantList
HomeLayout::components() const
{
    return items();
}
QVariantMap
HomeLayout::responsivePresentation(const QVariantMap& value, double width, double height, double unit) const
{
    return DesignSystem::responsive(value, width, height, unit);
}
void
HomeLayout::setCollections(const QVariantList& collections)
{
    if (m_collections == collections)
        return;
    m_collections = collections;
    emit layoutChanged();
}
void
HomeLayout::setCaptureInputsReady(bool ready)
{
    if (m_captureInputsReady == ready)
        return;
    m_captureInputsReady = ready;
    emit layoutChanged();
}
QVariantList
HomeLayout::sourceChoices(const QString& itemId) const
{
    QVariantList choices;
    for (const auto& value : m_items)
    {
        const auto item = value.toObject();
        if (item.value("id") != itemId)
            continue;
        const auto actionList = normalizedActionForItem(item);
        const auto type = actionList.isEmpty() ? QString() : actionList.first().toMap().value("type").toString();
        appendSourceChoicesForType(choices, type);
        break;
    }
    return choices;
}

void
HomeLayout::appendSourceChoicesForType(QVariantList& choices, const QString& type) const
{
    if (type == "switch-input")
    {
        appendHdmiInputChoices(choices);
    }
    else if (type == "open-collection" || type == "play-movie" || type == "open-show")
    {
        appendCollectionChoices(choices);
    }
    else
    {
        appendAppChoices(choices, type);
    }
}

void
HomeLayout::appendHdmiInputChoices(QVariantList& choices) const
{
    for (int input = ComponentSchema::MinHdmiInput; input <= ComponentSchema::MaxHdmiInput; ++input)
        choices.append(QVariantMap{{"id", QString::number(input)}, {"name", QString("HDMI %1").arg(input)}});
}

void
HomeLayout::appendCollectionChoices(QVariantList& choices) const
{
    for (const auto& entry : m_collections)
        choices.append(QVariantMap{{"id", entry.toMap().value("id")}, {"name", entry.toMap().value("name")}});
}

void
HomeLayout::appendAppChoices(QVariantList& choices, const QString& type) const
{
    for (const auto& entry : m_registry->apps())
    {
        const auto app = entry.toMap();
        if (type == "launch-app" || app.value("handledActions").toStringList().contains(type))
            choices.append(QVariantMap{{"id", app.value("id")}, {"name", app.value("name")}});
    }
}
bool
HomeLayout::rebindSource(const QString& itemId, const QString& sourceId)
{
    if (!sourceIsCompatible(itemId, sourceId))
    {
        setError(tr("Choose a compatible installed source."));
        return false;
    }
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items[i].toObject();
        if (item.value("id") != itemId)
            continue;
        return rebindItemAt(i, item, sourceId);
    }
    return false;
}

bool
HomeLayout::sourceIsCompatible(const QString& itemId, const QString& sourceId) const
{
    for (const auto& choice : sourceChoices(itemId))
        if (choice.toMap().value("id") == sourceId)
            return true;
    return false;
}

bool
HomeLayout::rebindItemAt(qsizetype i, const QJsonObject& item, const QString& sourceId)
{
    auto actionList = normalizedActionForItem(item);
    if (actionList.isEmpty())
        return false;
    auto first = actionList.first().toMap();
    const auto type = first.value("type").toString();
    if (type == "launch-app")
        setAppId(first, item, sourceId);
    else if (type == "open-collection" || type == "play-movie" || type == "open-show")
        first["collectionId"] = sourceId;
    else if (type == "switch-input")
        first["input"] = sourceId.toInt();
    else
        first["openWith"] = sourceId;
    actionList[0] = QJsonObject::fromVariantMap(first);
    item["actions"] = QJsonArray::fromVariantList(actionList);
    item.remove("action");
    item = QJsonObject::fromVariantMap(ComponentSchema::component(item.toVariantMap(), true));
    m_items[i] = item;
    if (!save())
    {
        m_items[i] = previousItem(i);
        return false;
    }
    emit layoutChanged();
    return true;
}

QJsonObject
HomeLayout::previousItem(qsizetype i) const
{
    return m_items[i].toObject();
}

void
HomeLayout::setAppId(QJsonObject& first, const QJsonObject& item, const QString& sourceId)
{
    first["appId"] = sourceId;
    if (item.value("kind") == "app")
        first["appId"] = sourceId;
}
QVariantList
HomeLayout::categories() const
{
    return m_categories.toVariantList();
}
QVariantList
HomeLayout::sections() const
{
    QVariantList result;
    const auto resolved = items();
    for (const auto& categoryValue : m_categories)
    {
        const auto category = categoryValue.toObject();
        auto categoryItems = resolveCategoryItems(resolved, category);
        if (!categoryItems.isEmpty() || category["presentation"].toObject()["variant"] == "library" ||
            grid().value("mode") == "sections")
        {
            auto section = category.toVariantMap();
            section["items"] = categoryItems;
            result.append(section);
        }
    }
    auto uncategorized = resolveUncategorized(resolved);
    if (!uncategorized.isEmpty() || grid().value("mode") == "sections")
        result.append(QVariantMap{{"id", ""}, {"name", tr("Uncategorized")}, {"items", uncategorized}});
    return result;
}

QVariantList
HomeLayout::resolveCategoryItems(const QVariantList& resolved, const QJsonObject& category) const
{
    QVariantList categoryItems;
    const auto categoryId = category.value("id").toString();
    for (const auto& itemValue : resolved)
        if (itemValue.toMap().value("parentId").toString().isEmpty() &&
            itemValue.toMap().value("categoryId").toString() == categoryId)
            categoryItems.append(itemValue);
    return categoryItems;
}

QVariantList
HomeLayout::resolveUncategorized(const QVariantList& resolved) const
{
    QVariantList uncategorized;
    for (const auto& itemValue : resolved)
        if (itemValue.toMap().value("parentId").toString().isEmpty() &&
            itemValue.toMap().value("categoryId").toString().isEmpty())
            uncategorized.append(itemValue);
    return uncategorized;
}
bool
HomeLayout::brandVisible() const
{
    return m_brandVisible;
}
QString
HomeLayout::backgroundStart() const
{
    return appearance().value("backgroundStart", m_backgroundStart).toString();
}
QString
HomeLayout::backgroundEnd() const
{
    return appearance().value("backgroundEnd", m_backgroundEnd).toString();
}
double
HomeLayout::tileRoundness() const
{
    return m_tileRoundness;
}
QVariantMap
HomeLayout::appearance() const
{
    QVariantMap authored;
    for (auto it = m_appearance.cbegin(); it != m_appearance.cend(); ++it)
        if (m_themeKeys.contains(it.key()))
            authored[it.key()] = it.value();
    if (m_themeKeys.contains("backgroundStart"))
        authored["backgroundStart"] = m_backgroundStart;
    if (m_themeKeys.contains("backgroundEnd"))
        authored["backgroundEnd"] = m_backgroundEnd;
    if (m_themeKeys.contains("tileRoundness"))
        authored["tileRoundness"] = m_tileRoundness;
    auto base = m_owner ? m_owner->homeAppearance() : m_appearance;
    if (!m_owner)
    {
        base["backgroundStart"] = m_backgroundStart;
        base["backgroundEnd"] = m_backgroundEnd;
        base["tileRoundness"] = m_tileRoundness;
    }
    return ThemeColors::resolve(base, authored);
}
bool
HomeLayout::setAppearance(const QVariantMap& changes)
{
    auto normalized = changes;
    if (!normalizeAppearance(normalized))
    {
        setError(tr("Choose valid opaque colors and appearance values within the displayed ranges."));
        return false;
    }
    const auto previous = m_appearance;
    const auto previousKeys = m_themeKeys;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it)
        m_themeKeys.insert(it.key());
    const auto previousStart = m_backgroundStart, previousEnd = m_backgroundEnd;
    const auto previousRoundness = m_tileRoundness;
    if (normalized.contains("backgroundStart"))
        m_backgroundStart = normalized.take("backgroundStart").toString();
    if (normalized.contains("backgroundEnd"))
        m_backgroundEnd = normalized.take("backgroundEnd").toString();
    if (normalized.contains("tileRoundness"))
        m_tileRoundness = normalized.take("tileRoundness").toDouble();
    for (auto it = normalized.cbegin(); it != normalized.cend(); ++it)
        m_appearance[it.key()] = it.value();
    QString validationError;
    if (!validateStudioDocument(documentData(), &validationError, m_defaultTheme) || !save())
    {
        if (!validationError.isEmpty())
            setError(validationError);
        m_themeKeys = previousKeys;
        m_appearance = previous;
        m_backgroundStart = previousStart;
        m_backgroundEnd = previousEnd;
        m_tileRoundness = previousRoundness;
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::setComposition(const QVariantMap& value)
{
    if (!ComponentSchema::validComposition(value))
    {
        setError(tr("Invalid Home composition."));
        return false;
    }
    if (!ComponentSchema::layoutPlacementsFit(m_items, value, m_categories))
    {
        setError(tr("Placed widgets do not fit this composition's grid."));
        return false;
    }
    const auto previous = m_composition;
    m_composition = value;
    if (!save())
    {
        m_composition = previous;
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::setGalleryFavorite(const QString& appId, bool favorite)
{
    const auto* app = m_registry ? m_registry->find(appId) : nullptr;
    if (!app || (app->type != "web" && app->type != "desktop" && app->type != "android"))
    {
        setError(tr("Choose an installed registered app to favorite."));
        return false;
    }
    auto composition = m_composition;
    auto gallery = composition.value("appGallery").toMap();
    QStringList favorites;
    for (const auto& value : gallery.value("favoriteIds").toList())
        favorites.append(value.toString());
    favorites.removeAll(appId);
    if (favorite)
        favorites.append(appId);
    std::sort(favorites.begin(),
              favorites.end(),
              [](const QString& a, const QString& b)
              {
                  return a.compare(b, Qt::CaseInsensitive) < 0;
              });
    QVariantList favoriteValues;
    for (const auto& id : favorites)
        favoriteValues.append(id);
    gallery.insert("favoriteIds", favoriteValues);
    composition.insert("appGallery", gallery);
    return setComposition(composition);
}
QVariantMap
HomeLayout::grid() const
{
    return ComponentSchema::normalizedGrid(m_composition.value("grid").toMap());
}
QString
HomeLayout::wallpaperSource() const
{
    return resolveArtwork(m_composition.value("wallpaper").toString());
}
QVariantList
HomeLayout::captureArtworkChoices() const
{
    QVariantList result;
    if (m_userPath.isEmpty())
        return result;
    const QDir directory(QDir(QFileInfo(m_userPath).absolutePath()).filePath("artwork"));
    const auto files = directory.entryInfoList(
        {"*.png", "*.jpg", "*.jpeg", "*.gif"}, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name);
    for (const auto& file : files)
    {
        if (result.size() >= 256)
            break;
        const auto relative = QStringLiteral("artwork/") + file.fileName();
        const auto resolved = managedArtworkSource(relative);
        if (resolved.isEmpty() || file.size() <= 0 || file.size() > MaxArtworkSize ||
            (relative.endsWith(".gif") && file.size() > 2 * 1024 * 1024))
            continue;
        QImageReader reader(QUrl(resolved).toLocalFile());
        const auto size = reader.size();
        if (!reader.canRead() || !size.isValid() || size.width() > 16384 || size.height() > 16384 ||
            qint64(size.width()) * size.height() > 32 * 1024 * 1024)
            continue;
        result.append(QVariantMap{{"id", relative}, {"name", file.completeBaseName()}});
    }
    return result;
}
QVariantList
HomeLayout::captureArtworkSource(int input) const
{
    if (input < 1 || input > 8)
        return {};
    const auto relative = m_composition.value("captureInputArtwork").toMap().value(QString::number(input)).toString();
    const auto managed = managedArtworkSource(relative);
    const QFileInfo file(QUrl(managed).toLocalFile());
    if (managed.isEmpty() || !file.isFile() || file.isSymLink())
        return {};
    return resolveArtwork(relative);
}
bool
HomeLayout::setCaptureInputArtwork(const QVariantMap& artwork)
{
    if (!ComponentSchema::validComposition({{"schemaVersion", 1}, {"captureInputArtwork", artwork}}))
    {
        setError(tr("Invalid HDMI input artwork mapping."));
        return false;
    }
    QSet<QString> available;
    for (const auto& choice : captureArtworkChoices())
        available.insert(choice.toMap().value("id").toString());
    for (const auto& value : artwork)
        if (!available.contains(value.toString()))
        {
            setError(tr("An HDMI image is no longer available. Refresh the artwork choices and try again."));
            return false;
        }
    auto composition = m_composition;
    if (artwork.isEmpty())
        composition.remove("captureInputArtwork");
    else
    {
        composition.insert("schemaVersion", 1);
        composition.insert("captureInputArtwork", artwork);
    }
    return setComposition(composition);
}
QString
HomeLayout::managedArtworkSource(const QString& relative) const
{
    if (relative.isEmpty())
        return {};
    if (!QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg|gif)$").match(relative).hasMatch())
        return {};
    if (m_assetPaths.contains(relative))
        return QUrl::fromLocalFile(m_assetPaths.value(relative)).toString();
    if (m_userPath.isEmpty())
        return {};
    const auto profile = QFileInfo(QFileInfo(m_userPath).absolutePath()).canonicalFilePath();
    const auto path = QDir(QFileInfo(m_userPath).absolutePath()).filePath(relative);
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.isEmpty() && (profile.isEmpty() || QDir(profile).relativeFilePath(canonical).startsWith("../")))
        return {};
    return QUrl::fromLocalFile(path).toString();
}
QString
HomeLayout::resolveArtwork(const QString& relative) const
{
    const auto source = managedArtworkSource(relative);
    if (!relative.endsWith(".gif"))
        return source;
    QFile input(QUrl(source).toLocalFile());
    if (source.isEmpty() || !input.open(QIODevice::ReadOnly) || input.size() > 2 * 1024 * 1024)
    {
        const_cast<HomeLayout*>(this)->setError(tr("The managed GIF is missing or exceeds 2 MiB."));
        return {};
    }
    const auto bytes = input.readAll();
    const auto key = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    if (const auto* cached = m_gifStills.object(key))
        return *cached;
    QString error;
    const auto decoded = ManagedGif::decode(bytes, true, &error);
    if (decoded.isEmpty())
    {
        const_cast<HomeLayout*>(this)->setError(error);
        return {};
    }
    const auto still = decoded["frames"].toList().first().toString();
    m_gifStills.insert(key, new QString(still), int(still.size() * 2));
    return still;
}
QVariantMap
HomeLayout::artworkAnimation(const QString& relative, const QString& owner)
{
    const auto fail = [&](const QString& reason)
    {
        setError(reason);
        return QVariantMap{{"error", reason}};
    };
    if (!ComponentSchema::safeId(owner) || !relative.endsWith(".gif"))
        return fail(tr("Choose managed GIF artwork."));
    if (!m_animationOwners.contains(owner) && m_animationOwners.size() >= 8)
        return fail(tr("Eight artwork animations are already active. Other images remain still."));
    const auto source = QUrl(managedArtworkSource(relative));
    QFile input(source.toLocalFile());
    if (!source.isLocalFile() || !input.open(QIODevice::ReadOnly) || input.size() > 2 * 1024 * 1024)
        return fail(tr("The managed GIF is missing or exceeds 2 MiB."));
    QString error;
    const auto result = ManagedGif::decode(input.readAll(), true, &error);
    if (result.isEmpty())
        return fail(error);
    m_animationOwners.insert(owner);
    return result;
}
void
HomeLayout::releaseArtworkAnimation(const QString& owner)
{
    m_animationOwners.remove(owner);
}
bool
HomeLayout::setWallpaper(const QString& sourceUrl)
{
    const QUrl url(sourceUrl);
    if (!url.isLocalFile())
    {
        setError(tr("Choose a local PNG or JPEG wallpaper."));
        return false;
    }
    QFile source(url.toLocalFile());
    if (!source.open(QIODevice::ReadOnly) || source.size() > HomeLayout::MaxArtworkSize)
        return false;
    const auto data = source.readAll();
    const bool png = data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"));
    if (!png && !data.startsWith(QByteArray::fromHex("ffd8ff")))
        return false;
    const auto relative = "artwork/wallpaper-" +
                          QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) +
                          (png ? ".png" : ".jpg");
    const auto directory = QFileInfo(m_userPath).absolutePath();
    if (!QDir().mkpath(QDir(directory).filePath("artwork")))
        return false;
    QSaveFile output(QDir(directory).filePath(relative));
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
        return false;
    auto config = m_composition;
    if (config.isEmpty())
        config = {{"schemaVersion", 1}, {"enabled", true}};
    config["wallpaper"] = relative;
    return setComposition(config);
}
bool
HomeLayout::setItemPresentation(const QString& id, const QVariantMap& value)
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items[i].toObject();
        if (item["id"] != id)
            continue;
        if (!ComponentSchema::validPresentation(
                value, item["componentType"] == "library", item["componentType"] == "container"))
        {
            setError(tr("Choose supported presentation choices for this component."));
            return false;
        }
        const auto previous = item;
        item["presentation"] = QJsonObject::fromVariantMap(value);
        m_items[i] = item;
        if (!save())
        {
            m_items[i] = previous;
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::setSectionPresentation(const QString& id, const QVariantMap& value)
{
    if (!ComponentSchema::validPresentation(value, true))
        return false;
    for (qsizetype i = 0; i < m_categories.size(); ++i)
    {
        auto section = m_categories[i].toObject();
        if (section["id"] != id)
            continue;
        const auto previous = section;
        section["presentation"] = QJsonObject::fromVariantMap(value);
        m_categories[i] = section;
        if (!save())
        {
            m_categories[i] = previous;
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
QString
HomeLayout::errorString() const
{
    return m_errorString;
}
bool
HomeLayout::load(const QString& defaultPath, const QString& userPath)
{
    m_animationOwners.clear();
    m_gifStills.clear();
    if (m_studio)
        m_studio->invalidate();
    m_legacyDocument.clear();
    m_defaultPath = defaultPath;
    if (m_defaultPath.isEmpty())
    {
        m_defaultPath = QDir(QCoreApplication::applicationDirPath()).filePath("config/default-home-layout.json");
        if (!QFile::exists(m_defaultPath))
            m_defaultPath = QStringLiteral(":/config/default-home-layout.json");
    }
    m_userPath =
        userPath.isEmpty()
            ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("home-layout.json")
            : userPath;
    QFile defaults(m_defaultPath);
    m_defaultTheme.clear();
    if (!defaults.open(QIODevice::ReadOnly) || !loadDocument(defaults.read(HomeLayout::MaxLayoutSize + 1), false))
        return false;
    m_defaultTheme = appearance();
    QFile user(m_userPath);
    if (user.exists() && user.open(QIODevice::ReadOnly))
    {
        const auto defaultItems = m_items;
        const auto defaultCategories = m_categories;
        const auto defaultBrand = m_brandVisible;
        const auto defaultStart = m_backgroundStart;
        const auto defaultEnd = m_backgroundEnd;
        const auto defaultRoundness = m_tileRoundness;
        if (!loadDocument(user.read(HomeLayout::MaxLayoutSize + 1), true))
        {
            m_items = defaultItems;
            m_brandVisible = defaultBrand;
            m_categories = defaultCategories;
            m_backgroundStart = defaultStart;
            m_backgroundEnd = defaultEnd;
            m_tileRoundness = defaultRoundness;
        }
    }
    emit layoutChanged();
    emit skinsChanged();
    return true;
}
bool
HomeLayout::loadDocument(const QByteArray& data, bool userDocument)
{
    int version = 0;
    QJsonArray entries;
    if (!validateDocumentStructure(data, userDocument, version, entries))
        return false;
    QJsonObject object = QJsonDocument::fromJson(data).object();
    QString designError;
    if (!m_owner && !DesignSystem::validDocument(object, &designError))
    {
        setError(designError);
        return false;
    }
    QJsonArray pages;
    QSet<QString> pageIds;
    if (!loadAndValidatePages(object, pageIds, pages))
        return false;
    const auto homePage = object.value("homePageId");
    QVariantMap editorAppearance;
    const QJsonObject rootPage = object.value("rootPage").toObject();
    if (object.contains("studioAppearance"))
        editorAppearance = object.value("studioAppearance").toObject().toVariantMap();
    if (!validateDocumentPrerequisites(object, pages, pageIds, homePage))
        return false;
    if (!validateHomePage(object, homePage, pageIds))
        return false;
    QVariantMap composition = object.value("composition").toObject().toVariantMap();
    if (!validateComposition(object, composition))
        return false;
    const QJsonObject theme = object.value("theme").toObject();
    QColor start, end;
    if (!extractThemeColors(theme, start, end))
        return false;
    QVariantMap presentation = buildAppearance(theme);
    if (!normalizeAppearance(presentation))
    {
        setError(tr("Home appearance contains invalid values."));
        return false;
    }
    if (!m_owner)
    {
        if (!validateShellReadability(pages, homePage, presentation, start, end))
            return false;
    }
    QSet<QString> categoryIds;
    const auto categoryArray = object.value("categories").toArray();
    QJsonArray components;
    if (!validateCategoriesAndComponents(data, version, categoryArray, entries, components))
        return false;
    QString hierarchyError;
    if (!ComponentSchema::hierarchyFits(components, composition, categoryArray, &hierarchyError))
    {
        setError(hierarchyError);
        return false;
    }
    m_items = components;
    m_pages = pages;
    m_homePageId = homePage.toString();
    m_rootPage = rootPage;
    m_studioAppearance = editorAppearance;
    m_composition = composition;
    m_appearance = presentation;
    m_themeKeys.clear();
    for (const auto& key : theme.keys())
        m_themeKeys.insert(key);
    if (userDocument && version == 1)
        m_legacyDocument = data;
    m_categories = categoryArray;
    m_brandVisible = theme.value("brandVisible").toBool(false);
    m_backgroundStart = start.name();
    m_backgroundEnd = end.name();
    m_tileRoundness = qBound(0.0, theme.value("tileRoundness").toDouble(0.18), 1.0);
    setError({});
    return true;
}

bool
HomeLayout::validateShellReadability(const QJsonArray& pages, const QJsonValue& homePage,
                                     const QVariantMap& presentation, const QColor& start, const QColor& end)
{
    auto shellBase = presentation;
    shellBase["backgroundStart"] = start.name();
    shellBase["backgroundEnd"] = end.name();
    auto home = shellBase;
    for (const auto& entry : pages)
        if (entry.toObject()["id"] == homePage)
        {
            const auto values = entry.toObject()["layout"].toObject()["theme"].toObject().toVariantMap();
            for (auto it = values.cbegin(); it != values.cend(); ++it)
            {
                if (it.key() == "surfaceThemes")
                {
                    auto policies = home.value(it.key()).toMap();
                    const auto local = it.value().toMap();
                    for (auto policy = local.cbegin(); policy != local.cend(); ++policy)
                        policies[policy.key()] = policy.value();
                    home[it.key()] = policies;
                }
                else
                    home[it.key()] = it.value();
            }
        }
    const auto readable = [&](const QVariantMap& theme)
    {
        auto origin = home;
        for (auto it = theme.cbegin(); it != theme.cend(); ++it)
            origin[it.key()] = it.value();
        return ComponentSchema::readableSurfaceThemes(home, origin);
    };
    if (!readable(shellBase))
    {
        setError(tr("Shell custom colors must keep text, status and focus readable."));
        return false;
    }
    for (const auto& entry : pages)
        if (!readable(entry.toObject()["layout"].toObject()["theme"].toObject().toVariantMap()))
        {
            setError(tr("A page has unreadable shell custom colors."));
            return false;
        }
    return true;
}

QVariantMap
HomeLayout::buildAppearance(const QJsonObject& theme)
{
    auto presentation = appearanceDefaults();
    for (auto it = presentation.begin(); it != presentation.end(); ++it)
        if (m_defaultTheme.contains(it.key()))
            it.value() = m_defaultTheme.value(it.key());
    for (auto it = presentation.begin(); it != presentation.end(); ++it)
        if (theme.contains(it.key()))
            it.value() = theme.value(it.key()).toVariant();
    if (m_defaultTheme.contains("surfaceThemes"))
        presentation["surfaceThemes"] = m_defaultTheme["surfaceThemes"];
    if (theme.contains("surfaceThemes"))
        presentation["surfaceThemes"] = theme["surfaceThemes"].toVariant();
    if (theme.contains("sourcePolicy"))
        presentation["sourcePolicy"] = theme["sourcePolicy"];
    if (theme.contains("colorTheme"))
        presentation["colorTheme"] = theme["colorTheme"].toVariant();
    for (const auto& keys : ThemeColors::roles())
        for (const auto& key : keys)
            if (theme.contains(key))
                presentation[key] = theme[key].toVariant();
    return presentation;
}

bool
HomeLayout::extractThemeColors(const QJsonObject& theme, QColor& start, QColor& end)
{
    start = QColor(theme.contains("backgroundStart")
                       ? theme.value("backgroundStart").toString()
                       : m_defaultTheme.value("backgroundStart", m_initialBackgroundStart).toString());
    end = QColor(theme.contains("backgroundEnd")
                     ? theme.value("backgroundEnd").toString()
                     : m_defaultTheme.value("backgroundEnd", m_initialBackgroundEnd).toString());
    if (!start.isValid() || !end.isValid())
    {
        setError(tr("Home theme contains invalid colors."));
        return false;
    }
    return true;
}

bool
HomeLayout::validateDocumentPrerequisites(const QJsonObject& object, const QJsonArray& pages,
                                          const QSet<QString>& pageIds, const QJsonValue& homePage)
{
    QString designError;
    if (!m_owner && !DesignSystem::validDocument(object, &designError))
    {
        setError(designError);
        return false;
    }
    QVariantMap editorAppearance;
    if (object.contains("studioAppearance"))
        editorAppearance = object.value("studioAppearance").toObject().toVariantMap();
    if (!validateStudioAppearance(object, editorAppearance))
        return false;
    const QJsonObject rootPage = object.value("rootPage").toObject();
    return validateRootPageCondition(object, rootPage, pages, pageIds, homePage);
}

bool
HomeLayout::validateRootPageCondition(const QJsonObject& object, const QJsonObject& rootPage,
                                      const QJsonArray& pages, const QSet<QString>& pageIds,
                                      const QJsonValue& homePage)
{
    if (object.contains("rootPage"))
        return validateRootPage(object, rootPage, pages, pageIds, homePage);
    if (!m_owner && pages.size() >= 64)
    {
        setError(tr("The interface exceeds the 64-page limit including Home."));
        return false;
    }
    return true;
}

bool
HomeLayout::validateDocumentStructure(const QByteArray& data, bool userDocument, int& version, QJsonArray& entries)
{
    if (data.size() > 4 * 1024 * 1024)
    {
        setError(tr("Home layout exceeds the supported size."));
        return false;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    version = document.object().value("schemaVersion").toInt();
    entries = document.object().value(version == 1 ? "items" : "components").toArray();
    if (error.error != QJsonParseError::NoError || !document.isObject() || (version != 1 && version != 2) ||
        !entries.isArray() || data.size() > 4 * 1024 * 1024 || entries.size() > 1024)
    {
        setError(userDocument ? tr("User Home layout is invalid; defaults were restored.")
                              : tr("Default Home layout is invalid."));
        return false;
    }
    return true;
}

bool
HomeLayout::loadAndValidatePages(const QJsonObject& object, QSet<QString>& pageIds, QJsonArray& pages)
{
    if (object.contains("pages") && (!object["pages"].isArray() || object["pages"].toArray().size() > 64 || m_owner))
    {
        setError(tr("Invalid page collection."));
        return false;
    }
    for (const auto& entry : object["pages"].toArray())
    {
        const auto page = entry.toObject();
        const auto id = page["id"].toString();
        HomeLayout validator(m_registry);
        validator.m_owner = this;
        if (!ComponentSchema::safeId(id) || pageIds.contains(id) || page["name"].toString().trimmed().isEmpty() ||
            page["name"].toString().size() > 80 || !page["layout"].isObject() ||
            !validator.loadDocument(QJsonDocument(page["layout"].toObject()).toJson(), false))
        {
            setError(tr("Page %1 has an invalid identity, name or layout: %2")
                         .arg(page["name"].toString(), validator.errorString()));
            return false;
        }
        pageIds.insert(id);
        pages.append(page);
    }
    return true;
}

bool
HomeLayout::validateStudioAppearance(const QJsonObject& object, QVariantMap& editorAppearance)
{
    if (object.contains("studioAppearance"))
    {
        auto effective = appearanceDefaults();
        const auto packaged = studioOptions().value("appearance").toMap();
        for (auto it = packaged.cbegin(); it != packaged.cend(); ++it)
            effective[it.key()] = it.value();
        for (auto it = editorAppearance.cbegin(); it != editorAppearance.cend(); ++it)
            effective[it.key()] = it.value();
        if (m_owner || !object["studioAppearance"].isObject() || !readableStudioAppearance(effective))
        {
            setError(tr("Studio colors must keep text and focus readable on normal and selected surfaces."));
            return false;
        }
    }
    return true;
}

bool
HomeLayout::validateRootPage(const QJsonObject& object, const QJsonObject& rootPage, const QJsonArray& pages,
                             const QSet<QString>& pageIds, const QJsonValue& homePage)
{
    bool valid = !m_owner && rootPage.isObject();
    for (auto it = rootPage.begin(); it != rootPage.end(); ++it)
        if (!QStringList{"name", "visible", "position"}.contains(it.key()))
            valid = false;
    const auto position = rootPage.value("position");
    if (!rootPage.value("name").isString() || rootPage.value("name").toString().trimmed().isEmpty() ||
        rootPage.value("name").toString().size() > 80 || rootPage.value("name").toString().contains(QChar::Null) ||
        !rootPage.value("visible").isBool() || !position.isDouble() || position.toDouble() != position.toInt() ||
        position.toInt() < 0 || position.toInt() > pages.size() ||
        (!rootPage.value("visible").toBool() && homePage.toString().isEmpty()) ||
        pages.size() + (rootPage.value("visible").toBool() ? 1 : 0) > 64)
        valid = false;
    if (!valid)
    {
        setError(tr("Root page metadata is invalid. Nothing changed."));
        return false;
    }
    return true;
}

bool
HomeLayout::validateHomePage(const QJsonObject& object, const QJsonValue& homePage, const QSet<QString>& pageIds)
{
    if (object.contains("homePageId") &&
        (m_owner || !homePage.isString() || (!homePage.toString().isEmpty() && !pageIds.contains(homePage.toString()))))
    {
        setError(tr("The designated Home page does not exist."));
        return false;
    }
    return true;
}

bool
HomeLayout::validateComposition(const QJsonObject& object, QVariantMap& composition)
{
    if ((object.contains("composition") && !object["composition"].isObject()) ||
        !ComponentSchema::validComposition(composition))
    {
        setError(tr("Home composition is invalid."));
        return false;
    }
    return true;
}

bool
HomeLayout::validateCategories(const QJsonArray& categoryArray, QSet<QString>& categoryIds)
{
    if (categoryArray.size() > HomeLayout::MaxLayoutCategories)
    {
        setError(tr("Too many Home categories."));
        return false;
    }
    for (const auto& value : categoryArray)
    {
        const auto category = value.toObject();
        const auto id = category.value("id").toString();
        if (id.isEmpty() || category.value("name").toString().trimmed().isEmpty() || categoryIds.contains(id) ||
            (category.contains("presentation") &&
             (!category["presentation"].isObject() ||
              !ComponentSchema::validPresentation(category["presentation"].toObject().toVariantMap(), true))))
        {
            setError(tr("Home layout contains an invalid or duplicate category."));
            return false;
        }
        categoryIds.insert(id);
    }
    return true;
}

bool
HomeLayout::validateCategoriesAndComponents(const QByteArray& data, int version,
                                            const QJsonArray& categoryArray, const QJsonArray& entries,
                                            QJsonArray& components)
{
    QSet<QString> categoryIds;
    if (categoryArray.size() > HomeLayout::MaxLayoutCategories)
    {
        setError(tr("Too many Home categories."));
        return false;
    }
    for (const auto& value : categoryArray)
    {
        if (!validateCategoryItem(value.toObject(), categoryIds))
        {
            setError(tr("Home layout contains an invalid or duplicate category."));
            return false;
        }
    }
    QSet<QString> ids;
    for (const auto& value : entries.toArray())
    {
        QJsonObject item;
        QString componentError;
        if (!validateComponentItem(value.toObject(), version, categoryIds, ids, item, componentError))
        {
            setError(componentError.isEmpty() ? tr("Home layout contains an invalid or duplicate item.")
                                              : componentError);
            return false;
        }
        ids.insert(item.value("id").toString());
        components.append(item);
    }
    return true;
}

bool
HomeLayout::validateCategoryItem(const QJsonObject& category, QSet<QString>& categoryIds) const
{
    const auto id = category.value("id").toString();
    if (id.isEmpty() || category.value("name").toString().trimmed().isEmpty() || categoryIds.contains(id) ||
        (category.contains("presentation") &&
         (!category["presentation"].isObject() ||
          !ComponentSchema::validPresentation(category["presentation"].toObject().toVariantMap(), true))))
        return false;
    categoryIds.insert(id);
    return true;
}

bool
HomeLayout::validateComponentItem(const QJsonObject& entry, int version, const QSet<QString>& categoryIds,
                                  QSet<QString>& ids, QJsonObject& item, QString& componentError) const
{
    QString componentError;
    const auto normalized =
        ComponentSchema::component(entry.toVariantMap(), version == 1, &componentError);
    item = QJsonObject::fromVariantMap(normalized);
    const auto kind = item.value("kind").toString();
    const auto id = item.value("id").toString();
    const auto actionType = [&item]()
    {
        if (item.contains("actions") && item["actions"].isArray() && !item["actions"].toArray().isEmpty())
            return item["actions"].toArray().first().toObject().value("type").toString();
        return QString();
    }();
    if (normalized.isEmpty() || item.value("componentType") == "media-shelf" || ids.contains(id) ||
        !isValidComponentKind(kind) || !isValidComponentAction(kind, actionType, item) ||
        !isValidComponentCategoryRef(item, categoryIds) || !isValidComponentOpacity(item))
    {
        componentError = tr("Home layout contains an invalid or duplicate item.");
        return false;
    }
    return true;
}

bool
HomeLayout::isValidComponentKind(const QString& kind) const
{
    return kind == "app" || kind == "widget" || kind == "action";
}

bool
HomeLayout::isValidComponentAction(const QString& kind, const QString& actionType, const QJsonObject& item) const
{
    if (kind == "app" && actionType == QStringLiteral("launch-app") && item.value("appId").toString().isEmpty())
        return false;
    if (kind == "action" && item.value("name").toString().trimmed().isEmpty())
        return false;
    if (kind == "widget" && item.value("widgetType").toString().isEmpty())
        return false;
    if (!actionType.isEmpty() && normalizedActionForItem(item).isEmpty())
        return false;
    return true;
}

bool
HomeLayout::isValidComponentCategoryRef(const QJsonObject& item, const QSet<QString>& categoryIds) const
{
    return item.value("categoryId").toString().isEmpty() ||
           categoryIds.contains(item.value("categoryId").toString());
}

bool
HomeLayout::isValidComponentOpacity(const QJsonObject& item) const
{
    return !item.contains("opacity") || (item.value("opacity").isDouble() &&
                                         item.value("opacity").toDouble() >= 0.0 &&
                                         item.value("opacity").toDouble() <= 1.0);
}

bool
HomeLayout::validateComponents(const QJsonArray& entries, int version, const QSet<QString>& categoryIds,
                               QSet<QString>& ids, QJsonArray& components)
{
    for (const auto& value : entries.toArray())
    {
        QString componentError;
        const auto normalized =
            ComponentSchema::component(value.toObject().toVariantMap(), version == 1, &componentError);
        const auto item = QJsonObject::fromVariantMap(normalized);
        const auto kind = item.value("kind").toString();
        const auto id = item.value("id").toString();
        const auto actionType = [&item]()
        {
            if (item.contains("actions") && item["actions"].isArray() && !item["actions"].toArray().isEmpty())
                return item["actions"].toArray().first().toObject().value("type").toString();
            return QString();
        }();
        if (normalized.isEmpty() || item.value("componentType") == "media-shelf" || ids.contains(id) ||
            (kind != "app" && kind != "widget" && kind != "action") ||
            (kind == "app" && actionType == QStringLiteral("launch-app") && item.value("appId").toString().isEmpty()) ||
            (kind == "action" && item.value("name").toString().trimmed().isEmpty()) ||
            (kind == "widget" && item.value("widgetType").toString().isEmpty()) ||
            (!item.value("categoryId").toString().isEmpty() &&
             !categoryIds.contains(item.value("categoryId").toString())) ||
            (!actionType.isEmpty() && normalizedActionForItem(item).isEmpty()) ||
            (item.contains("opacity") && (!item.value("opacity").isDouble() || item.value("opacity").toDouble() < 0.0 ||
                                          item.value("opacity").toDouble() > 1.0)))
        {
            setError(tr("Home layout contains an invalid or duplicate item."));
            return false;
        }
        ids.insert(id);
        components.append(item);
    }
    return true;
}

bool
HomeLayout::addAppTile(const QString& appId)
{
    const auto* definition = m_registry->find(appId);
    if (!definition)
        return false;
    for (const auto& value : m_items)
        if (value.toObject().contains("appCollection"))
            return true;
    if (!definition->launchConfiguration.value("emulatorRuntime").toString().isEmpty())
        return true;
    for (const auto& value : m_items)
        if (value.toObject().value("appId").toString() == appId)
            return true;
    QString id = appId;
    int suffix = 2;
    const auto idExists = [this](const QString& candidate)
    {
        for (const auto& value : m_items)
            if (value.toObject().value("id") == candidate)
                return true;
        return false;
    };
    while (idExists(id))
        id = appId + '-' + QString::number(suffix++);
    QJsonObject tile{{"kind", "app"},
                     {"id", id},
                     {"appId", appId},
                     {"actions", QJsonArray{QJsonObject{{"type", "launch-app"}, {"appId", appId}}}},
                     {"size", "standard"}};
    const auto section = m_registry->find(appId)->section;
    for (const auto& value : m_categories)
    {
        const auto category = value.toObject();
        if (category.value("name").toString().compare(section, Qt::CaseInsensitive) == 0)
        {
            tile.insert("categoryId", category.value("id"));
            break;
        }
    }
    m_items.append(tile);
    if (!save())
    {
        m_items.removeLast();
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::addActionTile(const QString& name,
                          const QString& actionType,
                          const QString& target,
                          const QString& categoryId,
                          const QString& openWith)
{
    const auto cleanName = name.trimmed().left(80);
    const auto cleanActionType = actionType.trimmed();
    const auto cleanTarget = target.trimmed();
    const auto cleanOpenWith = openWith.trimmed();
    const bool captureInput = cleanActionType == QStringLiteral("switch-input");
    QVariantMap requestedAction{{"type", cleanActionType}};
    if (captureInput)
        requestedAction["input"] = cleanTarget.toInt();
    else
    {
        requestedAction["target"] = cleanTarget;
        requestedAction["openWith"] = cleanOpenWith;
    }
    const auto normalizedAction = ComponentSchema::action(requestedAction);
    if (normalizedAction.isEmpty())
        return false;
    if (cleanName.isEmpty() || cleanTarget.isEmpty() || !isSupportedActionType(cleanActionType) ||
        cleanActionType == QStringLiteral("launch-app") || cleanActionType == QStringLiteral("desktop-mode") ||
        (captureInput && !cleanOpenWith.isEmpty()) ||
        (!captureInput &&
         (!isSafeActionId(cleanOpenWith) ||
          (!cleanOpenWith.isEmpty() && cleanOpenWith != QStringLiteral("system") && !m_registry->find(cleanOpenWith)))))
        return false;
    if (!captureInput && !cleanOpenWith.isEmpty() && cleanOpenWith != QStringLiteral("system") &&
        !m_registry->find(cleanOpenWith)->handledActions.contains(cleanActionType))
    {
        return false;
    }
    if (!categoryId.isEmpty())
    {
        bool found = false;
        for (const auto& value : m_categories)
            if (value.toObject().value("id").toString() == categoryId)
                found = true;
        if (!found)
            return false;
    }
    auto base = cleanName.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-|-$")));
    if (base.isEmpty())
        base = QStringLiteral("tile");
    QString id = base;
    int suffix = 2;
    const auto idExists = [this](const QString& candidate)
    {
        for (const auto& value : m_items)
            if (value.toObject().value("id").toString() == candidate)
                return true;
        return false;
    };
    while (idExists(id))
        id = base + QStringLiteral("-") + QString::number(suffix++);
    const QJsonObject action = QJsonObject::fromVariantMap(normalizedAction);
    QJsonObject tile{
        {"kind", "action"}, {"id", id}, {"name", cleanName}, {"size", "standard"}, {"actions", QJsonArray{action}}};
    if (!categoryId.isEmpty())
        tile.insert(QStringLiteral("categoryId"), categoryId);
    m_items.append(tile);
    if (!save())
    {
        m_items.removeLast();
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::removeAppTile(const QString& appId)
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        if (m_items.at(i).toObject().value("appId").toString() == appId)
        {
            const auto removed = m_items.takeAt(i);
            if (!save())
            {
                m_items.insert(i, removed);
                return false;
            }
            emit layoutChanged();
            return true;
        }
    }
    return true;
}
bool
HomeLayout::moveItem(const QString& id, int offset)
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        if (m_items.at(i).toObject().value("id").toString() == id)
        {
            const auto target = qBound<qsizetype>(0, i + offset, m_items.size() - 1);
            if (target == i)
                return true;
            const auto previousItems = m_items;
            const auto item = m_items.takeAt(i);
            m_items.insert(target, item);
            if (!save())
            {
                m_items = previousItems;
                return false;
            }
            emit layoutChanged();
            return true;
        }
    }
    return false;
}
bool
HomeLayout::removeItem(const QString& id)
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        if (m_items.at(i).toObject().value("id").toString() == id)
        {
            const auto removed = m_items.takeAt(i);
            if (!save())
            {
                m_items.insert(i, removed);
                return false;
            }
            emit layoutChanged();
            return true;
        }
    }
    return false;
}
bool
HomeLayout::addWidget(const QString& widgetType)
{
    if (widgetType != QStringLiteral("clock") && widgetType != "text")
        return false;
    QString id = widgetType;
    int suffix = 2;
    const auto idExists = [this](const QString& candidate)
    {
        for (const auto& value : m_items)
            if (value.toObject().value("id").toString() == candidate)
                return true;
        return false;
    };
    while (idExists(id))
        id = widgetType + QString::number(suffix++);
    m_items.append(QJsonObject{{"kind", "widget"}, {"id", id}, {"widgetType", widgetType}, {"size", "standard"}});
    if (widgetType == "text")
    {
        auto item = m_items.last().toObject();
        item["name"] = "Text";
        item["presentation"] = QJsonObject{{"variant", "text"}, {"subtitle", "Your message"}};
        m_items.replace(m_items.size() - 1, item);
    }
    if (!save())
    {
        m_items.removeLast();
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::setItemSize(const QString& id, const QString& size)
{
    if (size != QStringLiteral("compact") && size != QStringLiteral("standard") && size != QStringLiteral("wide"))
        return false;
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        if (item.value("id").toString() != id)
            continue;
        const auto previous = item.value("size");
        item.insert("size", size);
        m_items.replace(i, item);
        if (!save())
        {
            item.insert("size", previous);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::setItemOpacity(const QString& id, double opacity)
{
    if (!qIsFinite(opacity))
        return false;
    opacity = qBound(ComponentSchema::MinOpacity, opacity, ComponentSchema::MaxOpacity);
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        if (item.value("id").toString() != id)
            continue;
        const auto previous = item.value("opacity");
        item.insert("opacity", opacity);
        m_items.replace(i, item);
        if (!save())
        {
            if (previous.isUndefined())
                item.remove("opacity");
            else
                item.insert("opacity", previous);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::setTileOption(const QString& id, const QString& option, bool enabled)
{
    static const QSet<QString> options{QStringLiteral("showTitle"),
                                       QStringLiteral("showType"),
                                       QStringLiteral("showLogo"),
                                       QStringLiteral("logoFill")};
    if (!options.contains(option))
        return false;
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action")))
            continue;
        const auto previous = item.value(option);
        item.insert(option, enabled);
        m_items.replace(i, item);
        if (!save())
        {
            if (previous.isUndefined())
                item.remove(option);
            else
                item.insert(option, previous);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::setTileAppearance(const QString& id, const QString& symbol, const QString& accent)
{
    const auto cleanSymbol = symbol.trimmed().left(HomeLayout::MaxTileSymbolLength);
    const QColor accentColor(accent);
    if (cleanSymbol.isEmpty() || !accentColor.isValid())
        return false;
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action")))
            continue;
        const auto previousSymbol = item.value("symbol");
        const auto previousAccent = item.value("accent");
        const auto previousLogo = item.value("customLogo");
        item.insert(QStringLiteral("symbol"), cleanSymbol);
        item.insert(QStringLiteral("accent"), accentColor.name());
        item.remove(QStringLiteral("customLogo"));
        m_items.replace(i, item);
        if (!save())
        {
            if (previousSymbol.isUndefined())
                item.remove(QStringLiteral("symbol"));
            else
                item.insert(QStringLiteral("symbol"), previousSymbol);
            if (previousAccent.isUndefined())
                item.remove(QStringLiteral("accent"));
            else
                item.insert(QStringLiteral("accent"), previousAccent);
            if (!previousLogo.isUndefined())
                item.insert(QStringLiteral("customLogo"), previousLogo);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::setTileLogo(const QString& id, const QString& sourceUrl)
{
    const auto sourcePath = QUrl(sourceUrl).isLocalFile() ? QUrl(sourceUrl).toLocalFile() : sourceUrl;
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly) || source.size() > HomeLayout::MaxArtworkSize)
        return false;
    const auto data = source.readAll();
    const bool png = data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"));
    const bool jpeg = data.startsWith(QByteArray::fromHex("ffd8ff"));
    if (data.size() < 8 || (!png && !jpeg))
        return false;
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action")))
            continue;
        const auto artworkDir = QDir(QFileInfo(m_userPath).absolutePath()).filePath("artwork");
        if (!QDir().mkpath(artworkDir))
            return false;
        const auto relativePath =
            QStringLiteral("artwork/logo-") +
            QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) +
            (png ? QStringLiteral(".png") : QStringLiteral(".jpg"));
        QSaveFile output(QDir(QFileInfo(m_userPath).absolutePath()).filePath(relativePath));
        if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
            return false;
        const auto previous = item.value("customLogo");
        item.insert("customLogo", relativePath);
        item.insert("showLogo", true);
        m_items.replace(i, item);
        if (!save())
        {
            if (previous.isUndefined())
                item.remove("customLogo");
            else
                item.insert("customLogo", previous);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::clearTileLogo(const QString& id)
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action")))
            continue;
        const auto relativePath = item.value("customLogo").toString();
        if (relativePath.isEmpty())
            return true;
        item.remove("customLogo");
        m_items.replace(i, item);
        if (!save())
        {
            item.insert("customLogo", relativePath);
            m_items.replace(i, item);
            return false;
        }
        const auto* owner = m_owner ? m_owner : this;
        if (!owner->documentData().contains(
                QJsonDocument(QJsonArray{relativePath}).toJson(QJsonDocument::Compact).mid(1).chopped(1)))
            QFile::remove(QDir(QFileInfo(m_userPath).absolutePath()).filePath(relativePath));
        emit layoutChanged();
        return true;
    }
    return false;
}
QString
HomeLayout::addCategory(const QString& name)
{
    const auto cleanName = name.trimmed().left(60);
    if (cleanName.isEmpty())
        return {};
    auto base = cleanName.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-|-$")));
    if (base.isEmpty())
        base = QStringLiteral("category");
    QString id = base;
    int suffix = 2;
    const auto exists = [this](const QString& candidate)
    {
        for (const auto& v : m_categories)
            if (v.toObject().value("id").toString() == candidate)
                return true;
        return false;
    };
    while (exists(id))
        id = base + QStringLiteral("-") + QString::number(suffix++);
    m_categories.append(QJsonObject{{"id", id}, {"name", cleanName}});
    if (!save())
    {
        m_categories.removeLast();
        return {};
    }
    emit layoutChanged();
    return id;
}
bool
HomeLayout::renameCategory(const QString& categoryId, const QString& name)
{
    const auto cleanName = name.trimmed().left(60);
    if (cleanName.isEmpty())
        return false;
    for (qsizetype i = 0; i < m_categories.size(); ++i)
    {
        auto category = m_categories.at(i).toObject();
        if (category.value("id").toString() != categoryId)
            continue;
        const auto previous = category.value("name");
        category.insert("name", cleanName);
        m_categories.replace(i, category);
        if (!save())
        {
            category.insert("name", previous);
            m_categories.replace(i, category);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::removeCategory(const QString& categoryId)
{
    for (qsizetype i = 0; i < m_categories.size(); ++i)
    {
        if (m_categories.at(i).toObject().value("id").toString() != categoryId)
            continue;
        const auto oldCategories = m_categories, oldItems = m_items;
        m_categories.removeAt(i);
        for (qsizetype j = 0; j < m_items.size(); ++j)
        {
            auto item = m_items.at(j).toObject();
            if (item.value("categoryId").toString() == categoryId)
            {
                item.remove("categoryId");
                m_items.replace(j, item);
            }
        }
        if (!save())
        {
            m_categories = oldCategories;
            m_items = oldItems;
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::moveCategory(const QString& categoryId, int offset)
{
    for (qsizetype i = 0; i < m_categories.size(); ++i)
    {
        if (m_categories.at(i).toObject().value("id").toString() != categoryId)
            continue;
        const auto target = qBound<qsizetype>(0, i + offset, m_categories.size() - 1);
        if (target == i)
            return true;
        const auto category = m_categories.takeAt(i);
        m_categories.insert(target, category);
        if (!save())
        {
            m_categories.takeAt(target);
            m_categories.insert(i, category);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::assignItemCategory(const QString& itemId, const QString& categoryId)
{
    if (!categoryId.isEmpty())
    {
        bool found = false;
        for (const auto& v : m_categories)
            if (v.toObject().value("id").toString() == categoryId)
                found = true;
        if (!found)
            return false;
    }
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        if (item.value("id").toString() != itemId)
            continue;
        const auto previous = item.value("categoryId");
        if (categoryId.isEmpty())
            item.remove("categoryId");
        else
            item.insert("categoryId", categoryId);
        m_items.replace(i, item);
        if (!save())
        {
            if (previous.isUndefined())
                item.remove("categoryId");
            else
                item.insert("categoryId", previous);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}
bool
HomeLayout::setBrandVisible(bool visible)
{
    if (m_brandVisible == visible && m_themeKeys.contains("brandVisible"))
        return true;
    const auto previousKeys = m_themeKeys;
    m_themeKeys.insert("brandVisible");
    const auto previousVisible = m_brandVisible;
    m_brandVisible = visible;
    if (!save())
    {
        m_themeKeys = previousKeys;
        m_brandVisible = previousVisible;
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::setBackground(const QString& start, const QString& end)
{
    const QColor startColor(start), endColor(end);
    if (!startColor.isValid() || !endColor.isValid())
        return false;
    const auto previousStart = m_backgroundStart, previousEnd = m_backgroundEnd;
    const auto previousKeys = m_themeKeys;
    m_themeKeys.insert("backgroundStart");
    m_themeKeys.insert("backgroundEnd");
    m_backgroundStart = startColor.name();
    m_backgroundEnd = endColor.name();
    if (!save())
    {
        m_themeKeys = previousKeys;
        m_backgroundStart = previousStart;
        m_backgroundEnd = previousEnd;
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::setTileRoundness(double roundness)
{
    if (!qIsFinite(roundness))
        return false;
    roundness = qBound(0.0, roundness, 1.0);
    if (qFuzzyCompare(m_tileRoundness, roundness) && m_themeKeys.contains("tileRoundness"))
        return true;
    const auto previousKeys = m_themeKeys;
    m_themeKeys.insert("tileRoundness");
    const auto previous = m_tileRoundness;
    m_tileRoundness = roundness;
    if (!save())
    {
        m_themeKeys = previousKeys;
        m_tileRoundness = previous;
        return false;
    }
    emit layoutChanged();
    return true;
}
QByteArray
HomeLayout::documentData() const
{
    auto theme = QJsonObject::fromVariantMap(appearance());
    theme.insert("brandVisible", m_brandVisible);
    for (const auto& key : theme.keys())
        if (!m_themeKeys.contains(key))
            theme.remove(key);
    QJsonArray components;
    for (const auto& entry : m_items)
    {
        const auto normalized = ComponentSchema::component(entry.toObject().toVariantMap(), true);
        if (normalized.isEmpty())
            return {};
        components.append(QJsonObject::fromVariantMap(normalized));
    }
    QJsonObject object{{"schemaVersion", 2},
                       {"theme", theme},
                       {"composition", QJsonObject::fromVariantMap(m_composition)},
                       {"categories", m_categories},
                       {"components", components}};
    if (!m_owner)
    {
        object.insert("pages", m_pages);
        object.insert("homePageId", m_homePageId);
        if (!m_rootPage.isEmpty())
            object.insert("rootPage", m_rootPage);
        if (!m_studioAppearance.isEmpty())
            object.insert("studioAppearance", QJsonObject::fromVariantMap(m_studioAppearance));
    }
    return QJsonDocument(object).toJson();
}
bool
HomeLayout::save()
{
    if (m_draftOnly)
    {
        QString error;
        const bool ok = validateStudioDocument(documentData(), &error, m_defaultTheme);
        setError(error);
        return ok;
    }
    if (!ProfileManager::allows("edit"))
    {
        setError(tr("Editing is restricted in Settings > Profiles."));
        return false;
    }
    if (m_owner)
    {
        const bool ok = m_owner->storePage(m_pageId, QJsonDocument::fromJson(documentData()).object());
        setError(ok ? QString() : m_owner->errorString());
        return ok;
    }
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    const auto data = documentData();
    if (data.isEmpty() || data.size() > 4 * 1024 * 1024)
    {
        setError(tr("The design is invalid or exceeds 4 MiB."));
        return false;
    }
    if (!m_legacyDocument.isEmpty() && !QFile::exists(m_userPath + ".v1.backup.json"))
    {
        QSaveFile backup(m_userPath + ".v1.backup.json");
        if (!backup.open(QIODevice::WriteOnly) || backup.write(m_legacyDocument) != m_legacyDocument.size() ||
            !backup.commit())
        {
            setError(tr("Unable to back up the previous Home layout."));
            return false;
        }
    }
    QSaveFile file(m_userPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
    {
        setError(tr("Unable to save Home layout."));
        return false;
    }
    m_legacyDocument.clear();
    setError({});
    return true;
}
void
HomeLayout::setError(const QString& message)
{
    if (m_errorString == message)
        return;
    m_errorString = message;
    emit errorStringChanged();
}
QByteArray
HomeLayout::committedDocument() const
{
    return documentData();
}
bool
HomeLayout::validateStudioDocument(const QByteArray& data, QString* error, const QVariantMap& defaults)
{
    const auto count = [](const QJsonObject& document)
    {
        return document[document["schemaVersion"].toInt() == 1 ? "items" : "components"].toArray().size();
    };
    const auto document = QJsonDocument::fromJson(data).object();
    qsizetype nodes = count(document);
    for (const auto& page : document["pages"].toArray())
        nodes += count(page.toObject()["layout"].toObject());
    if (nodes > 1024)
    {
        if (error)
            *error = tr("The interface exceeds 1024 components across its pages.");
        return false;
    }
    AppRegistry registry;
    HomeLayout validator(&registry);
    validator.m_defaultTheme = defaults;
    const bool ok = validator.loadDocument(data, false);
    if (error)
        *error = validator.errorString();
    return ok;
}
bool
HomeLayout::pageAllowed(const QString& id) const
{
    return id.isEmpty() || id == m_homePageId || ProfileManager::allows("destination", id);
}
