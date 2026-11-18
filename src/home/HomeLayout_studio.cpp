#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "home/StudioSession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QVariantMap
HomeLayout::studioOptions() const
{
    QFile file(QDir(QFileInfo(m_defaultPath).absolutePath()).filePath("studio-defaults.json"));
    if (!file.open(QIODevice::ReadOnly))
    {
        file.setFileName(":/config/studio-defaults.json");
        if (!file.open(QIODevice::ReadOnly))
            return {};
    }
    if (file.size() > 128 * 1024)
        return {};
    const auto bytes = file.readAll();
    if (m_optionsCached && m_optionsPath == file.fileName() && m_optionsBytes == bytes)
        return m_optionsCache;
    m_optionsCached = true;
    m_optionsPath = file.fileName();
    m_optionsBytes = bytes;
    m_optionsCache.clear();
    const auto object = QJsonDocument::fromJson(bytes).object();
    QVariantMap guide;
    if (!validateStudioDefaults(object, guide))
        return {};
    QVariantMap result = object.toVariantMap();
    if (!finalizeStudioOptions(result, guide))
        return {};
    result["maxScrollableRows"] = ComponentSchema::MaxScrollableRows;
    m_optionsCache = result;
    return m_optionsCache;
}


bool
HomeLayout::validateStudioPanelPreset(const QJsonObject& object) const
{
    const auto group = ComponentSchema::component(object["groupPreset"].toObject().toVariantMap(), true);
    if (group.isEmpty() || group.value("componentType") != "container")
        return false;
    if (!panel["children"].isArray() || panel["children"].toArray().size() > 32)
        return false;
    QJsonArray panelItems;
    const auto panelRoot = ComponentSchema::component(panel["root"].toObject().toVariantMap(), true);
    if (panelRoot.isEmpty() || panelRoot.value("componentType") != "container")
        return false;
    panelItems.append(QJsonObject::fromVariantMap(panelRoot));
    for (const auto& entry : panel["children"].toArray())
    {
        const auto child = ComponentSchema::component(entry.toObject().toVariantMap(), true);
        if (child.isEmpty())
            return false;
        panelItems.append(QJsonObject::fromVariantMap(child));
    }
    if (!ComponentSchema::hierarchyFits(
            panelItems,
            {{"schemaVersion", 1}, {"enabled", true}, {"grid", object["defaultGrid"].toObject().toVariantMap()}},
            {}))
        return false;
    return true;
}


bool
HomeLayout::validateStudioActionsList(const QJsonArray& actions) const
{
    for (const auto& entry : actions)
    {
        const auto choice = entry.toObject();
        if (!choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
            choice["name"].toString().size() > 160)
            return false;
        const auto actionList = choice["actions"].toArray();
        if (actionList.isEmpty() || ComponentSchema::action(actionList.first().toObject().toVariantMap()).isEmpty())
            return false;
        if (choice.contains("aliases"))
        {
            if (!choice["aliases"].isArray() || choice["aliases"].toArray().size() > 8)
                return false;
            for (const auto& alias : choice["aliases"].toArray())
                if (!alias.isString() || alias.toString().size() > 80 || alias.toString().contains(QChar::Null))
                    return false;
        }
    }
    return true;
}

bool
HomeLayout::validateStudioComponentVariants(const QJsonObject& object, const QJsonObject& presets) const
{
    const auto variants = object["componentVariants"].toObject();
    if (!variants.isObject() || variants.size() > 3)
        return false;
    const auto families = variants.toObject();
    for (auto family = families.begin(); family != families.end(); ++family)
    {
        if (!validateStudioVariantFamily(family.key(), family.value().toArray(), presets))
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioVariantFamily(const QString& familyKey, const QJsonArray& variants, const QJsonObject& presets) const
{
    if (!QStringList{"button", "image", "text"}.contains(familyKey) || !variants.isArray() ||
        variants.isEmpty() || variants.size() > 8)
        return false;
    QSet<QString> variantsSeen;
    for (const auto& value : variants)
    {
        if (!validateStudioVariant(value.toObject(), familyKey, presets, variantsSeen))
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioVariant(const QJsonObject& variant, const QString& familyKey, const QJsonObject& presets,
                                  QSet<QString>& variantsSeen) const
{
    const auto id = variant["id"].toString();
    if (variant.size() != 5 || !variant["id"].isString() || !ComponentSchema::safeId(id) ||
        variantsSeen.contains(id) || !variant["name"].isString() ||
        variant["name"].toString().trimmed().isEmpty() || variant["name"].toString().size() > 80 ||
        !variant["description"].isString() || variant["description"].toString().size() > 160 ||
        !variant["presentation"].isObject() || !variant["span"].isObject() ||
        variant["span"].toObject().size() != 2)
        return false;
    auto candidate = presets[familyKey].toObject(), style = candidate["presentation"].toObject();
    const auto changes = variant["presentation"].toObject();
    for (auto it = changes.begin(); it != changes.end(); ++it)
        style[it.key()] = it.value();
    candidate["presentation"] = style;
    const auto span = variant["span"].toObject();
    QJsonObject placement{{"x", 0}, {"y", 0}, {"width", span["width"]}, {"height", span["height"]}};
    candidate["placement"] = placement;
    if (ComponentSchema::component(candidate.toVariantMap(), true).isEmpty())
        return false;
    variantsSeen.insert(id);
    return true;
}

bool
HomeLayout::validateStudioPresets(const QJsonObject& presets) const
{
    for (auto it = presets.constBegin(); it != presets.constEnd(); ++it)
        if (!QStringList{"button",
                         "image",
                         "shape",
                         "text",
                         "library",
                         "recently-viewed",
                         "music-library",
                         "tv-guide",
                         "carousel",
                         "app-bar",
                         "page-menu",
                         "detected-sources",
                         "source-notice"}
                 .contains(it.key()) ||
             ComponentSchema::component(it.value().toObject().toVariantMap(), true).isEmpty())
            return false;
    return true;
}

bool
HomeLayout::validateStudioShapeChoices(const QJsonObject& object, const QJsonObject& presets) const
{
    const auto shapeChoices = object["shapeChoices"].toArray();
    if (!object["shapeChoices"].isArray() || shapeChoices.isEmpty() || shapeChoices.size() > 8)
        return false;
    for (const auto& value : shapeChoices)
    {
        const auto choice = value.toObject();
        const auto type = choice["type"].toString();
        if (choice.size() != 3 || !choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
            choice["name"].toString().size() > 80 || !QStringList{"shape", "container"}.contains(type) ||
            !choice["presentation"].isObject())
            return false;
        auto candidate = type == "shape" ? presets["shape"].toObject() : object["groupPreset"].toObject();
        auto style = candidate["presentation"].toObject();
        const auto changes = choice["presentation"].toObject();
        for (auto it = changes.begin(); it != changes.end(); ++it)
            style[it.key()] = it.value();
        candidate["presentation"] = style;
        if (ComponentSchema::component(candidate.toVariantMap(), true).isEmpty())
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioReplacementChoices(const QJsonObject& object) const
{
    const auto choices = object["replacementChoices"].toArray();
    if (!object["replacementChoices"].isArray() || choices.isEmpty() || choices.size() > 5)
        return false;
    QSet<QString> types;
    for (const auto& value : choices)
    {
        const auto choice = value.toObject();
        const auto type = choice["type"].toString();
        if (choice.size() != 2 || !choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
            choice["name"].toString().size() > 80 ||
            !QStringList{"button", "image", "text", "shape", "container"}.contains(type) || types.contains(type))
            return false;
        types.insert(type);
    }
    return true;
}

bool
HomeLayout::validateStudioCatalogIds(const QJsonObject& object, const QJsonObject& presets) const
{
    const auto catalog = object["catalog"].toArray();
    QSet<QString> catalogIds;
    if (presets.isEmpty() || presets.size() > 16 || catalog.isEmpty() || catalog.size() > 16 ||
        object["inspector"].toObject()["fields"].toArray().size() > 160)
        return false;
    for (const auto& entry : catalog)
    {
        const auto item = entry.toObject();
        const auto id = item["id"].toString(), name = item["name"].toString();
        if ((id != "widgets" && !presets.contains(id)) || catalogIds.contains(id) || name.trimmed().isEmpty() ||
            name.size() > 80 || item["description"].toString().size() > 160)
            return false;
        catalogIds.insert(id);
    }
    return true;
}

bool
HomeLayout::validateStudioActionCategoryTypes(const QJsonArray& categories) const
{
    const QStringList pickerTypes{"none",
                                  "media-picker",
                                  "root-page",
                                  "open-page",
                                  "launch-app",
                                  "open-apps",
                                  "open-search",
                                  "app-drawer",
                                  "open-collection",
                                  "open-guide",
                                  "play-movie",
                                  "open-show",
                                  "switch-input",
                                  "settings",
                                  "desktop-mode",
                                  "open-file",
                                  "open-folder",
                                  "open-url",
                                  "open-disc"};
    for (const auto& value : categories)
    {
        if (!validateStudioActionCategory(value.toObject(), pickerTypes))
            return false;
    }
    if (!categories.first().toObject()["types"].toArray().isEmpty())
        return false;
    return true;
}

bool
HomeLayout::validateStudioActionCategory(const QJsonObject& category, const QStringList& pickerTypes) const
{
    if (category.size() != 2 || !category["name"].isString() || category["name"].toString().trimmed().isEmpty() ||
        category["name"].toString().size() > 80 || !category["types"].isArray() ||
        category["types"].toArray().size() > pickerTypes.size())
        return false;
    QSet<QString> types;
    for (const auto& type : category["types"].toArray())
    {
        if (!type.isString() || !pickerTypes.contains(type.toString()) || types.contains(type.toString()))
            return false;
        types.insert(type.toString());
    }
    return true;
}

bool
HomeLayout::validateStudioGridModeIds(const QJsonArray& modes) const
{
    QSet<QString> ids;
    for (const auto& entry : modes)
    {
        const auto mode = entry.toObject();
        const auto id = mode["id"].toString(), name = mode["name"].toString();
        if (!QStringList{"screen", "sections"}.contains(id) || ids.contains(id) || name.trimmed().isEmpty() ||
            name.size() > 80)
            return false;
        ids.insert(id);
    }
    return ids.size() == 2;
}

bool
HomeLayout::applyStudioDefaults(QVariantMap& result, const QVariantMap& guide) const
{
    if (!ComponentSchema::validSettingsNavigation(result.value("settingsNavigation").toMap()))
    {
        QFile packaged(":/config/studio-defaults.json");
        if (!packaged.open(QIODevice::ReadOnly) || packaged.size() > 128 * 1024)
            return false;
        const auto navigation =
            QJsonDocument::fromJson(packaged.readAll()).object()["settingsNavigation"].toObject().toVariantMap();
        if (!ComponentSchema::validSettingsNavigation(navigation))
            return false;
        result["settingsNavigation"] = navigation;
    }
    if (!result.contains("referenceViewport"))
    {
        QFile packaged(":/config/studio-defaults.json");
        if (!packaged.open(QIODevice::ReadOnly) || packaged.size() > 128 * 1024)
            return false;
        result["referenceViewport"] =
            QJsonDocument::fromJson(packaged.readAll()).object()["referenceViewport"].toVariant();
    }
    return true;
}

QVariantMap
HomeLayout::studioAppearance() const
{
    auto result = appearance();
    const auto defaults = studioOptions().value("appearance").toMap();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        result[it.key()] = it.value();
    for (auto it = m_studioAppearance.cbegin(); it != m_studioAppearance.cend(); ++it)
        if (it.key() != "recentColors")
            result[it.key()] = it.value();
    return result;
}
bool
HomeLayout::applyTheme(const QVariantMap& theme, bool includeSuggestedArtwork)
{
    const auto presentation = prepareThemePresentation(theme);
    QString startColor, endColor;
    double roundness = -1.0, tileOpacity = 1.0;
    if (!validateThemeColors(theme, startColor, endColor, roundness, tileOpacity))
        return false;
    applyThemeArtwork(theme, tileOpacity, includeSuggestedArtwork);
    return commitTheme(theme, startColor, endColor, roundness, presentation);
}

QVariantMap
HomeLayout::prepareThemePresentation(const QVariantMap& theme)
{
    auto presentation = appearanceDefaults();
    if (m_appearance.contains("surfaceThemes"))
        presentation["surfaceThemes"] = m_appearance["surfaceThemes"];
    if (m_appearance.contains("sourcePolicy"))
        presentation["sourcePolicy"] = m_appearance["sourcePolicy"];
    for (auto it = presentation.begin(); it != presentation.end(); ++it)
        if (theme.contains(it.key()))
            it.value() = theme.value(it.key());
    if (theme.contains("surfaceThemes"))
        presentation["surfaceThemes"] = theme["surfaceThemes"];
    if (theme.contains("sourcePolicy"))
        presentation["sourcePolicy"] = theme["sourcePolicy"];
    return presentation;
}

bool
HomeLayout::validateThemeColors(const QVariantMap& theme, QString& startColor, QString& endColor,
                                double& roundness, double& tileOpacity)
{
    startColor = theme.value(QStringLiteral("backgroundStart")).toString();
    endColor = theme.value(QStringLiteral("backgroundEnd")).toString();
    roundness = theme.contains(QStringLiteral("tileRoundness"))
                    ? theme.value(QStringLiteral("tileRoundness")).toDouble()
                    : -1.0;
    tileOpacity = theme.contains(QStringLiteral("tileOpacity"))
                      ? theme.value(QStringLiteral("tileOpacity")).toDouble()
                      : 1.0;
    QColor start(startColor), endColorObj(endColor);
    if (!start.isValid() || !endColorObj.isValid() || !qIsFinite(roundness) || !qIsFinite(tileOpacity) ||
        roundness < 0.0 || roundness > 1.0 || tileOpacity < 0.2 || tileOpacity > 1.0)
        return false;
    return true;
}

void
HomeLayout::applyThemeArtwork(const QVariantMap& theme, double tileOpacity, bool includeSuggestedArtwork)
{
    const auto previousItems = m_items;
    const auto suggestions = theme.value(QStringLiteral("suggestedArtwork")).toMap();
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value(QStringLiteral("kind")).toString();
        if (kind != QStringLiteral("app") && kind != QStringLiteral("action"))
            continue;
        item.insert(QStringLiteral("opacity"), tileOpacity);
        if (includeSuggestedArtwork)
        {
            const QStringList keys{item.value(QStringLiteral("appId")).toString(),
                                   item.value(QStringLiteral("id")).toString(),
                                   item.value(QStringLiteral("sourceType")).toString(),
                                   item.value(QStringLiteral("name")).toString().toLower()};
            for (const auto& key : keys)
            {
                const auto suggestion = suggestions.value(key).toMap();
                const QColor accent(suggestion.value(QStringLiteral("accent")).toString());
                const auto symbol = suggestion.value(QStringLiteral("symbol")).toString().trimmed().left(3);
                if (symbol.isEmpty() || !accent.isValid())
                    continue;
                item.insert(QStringLiteral("symbol"), symbol);
                item.insert(QStringLiteral("accent"), accent.name());
                item.remove(QStringLiteral("customLogo"));
                break;
            }
        }
        m_items.replace(i, item);
    }
    Q_UNUSED(previousItems);
}

bool
HomeLayout::commitTheme(const QVariantMap& theme, const QString& startColor, const QString& endColor,
                        double roundness, const QVariantMap& presentation)
{
    const auto previousStart = m_backgroundStart;
    const auto previousEnd = m_backgroundEnd;
    const auto previousRoundness = m_tileRoundness;
    const auto previousItems = m_items;
    const auto previousAppearance = m_appearance;
    const auto previousKeys = m_themeKeys;
    if (!normalizeAppearance(presentation))
        return false;
    for (auto it = presentation.cbegin(); it != presentation.cend(); ++it)
        m_themeKeys.insert(it.key());
    m_themeKeys.insert("backgroundStart");
    m_themeKeys.insert("backgroundEnd");
    m_themeKeys.insert("tileRoundness");
    m_appearance = presentation;
    m_backgroundStart = startColor;
    m_backgroundEnd = endColor;
    m_tileRoundness = roundness;
    QString validationError;
    if (!validateStudioDocument(documentData(), &validationError, m_defaultTheme) || !save())
    {
        if (!validationError.isEmpty())
            setError(validationError);
        m_themeKeys = previousKeys;
        m_backgroundStart = previousStart;
        m_backgroundEnd = previousEnd;
        m_tileRoundness = previousRoundness;
        m_items = previousItems;
        m_appearance = previousAppearance;
        return false;
    }
    emit layoutChanged();
    return true;
}
bool
HomeLayout::resetToDefaults()
{
    if (!ProfileManager::allows("edit"))
    {
        setError(tr("Editing is restricted in Settings > Profiles."));
        return false;
    }
    if (m_owner || !m_pages.isEmpty())
    {
        QFile defaults(m_defaultPath);
        if (!defaults.open(QIODevice::ReadOnly))
        {
            setError(tr("Unable to read defaults."));
            return false;
        }
        return applySkinDocument(QJsonDocument::fromJson(defaults.readAll()).object(), {});
    }
    QFile::remove(m_userPath);
    return load(m_defaultPath, m_userPath);
}
QObject*
HomeLayout::beginStudio()
{
    if (m_studio)
    {
        m_studio->invalidate();
        m_studio->deleteLater();
    }
    m_studio = new StudioSession(this);
    m_studio->bind(this);
    return m_studio;
}
bool
HomeLayout::applyStudioDraft(const QByteArray& draft, const QString& focusComponentId)
{
    if (!ProfileManager::allows("edit"))
    {
        setError(tr("Editing is restricted in Settings > Profiles."));
        return false;
    }
    const QByteArray previousBytes = documentData();
    const QJsonArray previousItems = m_items;
    const QJsonArray previousCategories = m_categories;
    const QJsonArray previousPages = m_pages;
    const QString previousHomePage = m_homePageId;
    const auto previousRootPage = m_rootPage;
    const auto previousStudioAppearance = m_studioAppearance;
    const QVariantMap previousAppearance = m_appearance;
    const auto previousThemeKeys = m_themeKeys;
    const QVariantMap previousComposition = m_composition;
    const QString previousStart = m_backgroundStart, previousEnd = m_backgroundEnd;
    const double previousRoundness = m_tileRoundness;
    const bool previousBrand = m_brandVisible;
    const QString previousFocus = m_focusOrigin;
    HomeLayout probe(m_registry);
    probe.m_defaultTheme = m_defaultTheme;
    probe.m_owner = m_owner;
    if (!probe.loadDocument(draft, false))
    {
        setError(probe.errorString().isEmpty() ? tr("The draft could not be applied — nothing changed.")
                                               : probe.errorString());
        return false;
    }
    m_items = probe.m_items;
    m_categories = probe.m_categories;
    if (!m_owner)
    {
        m_pages = probe.m_pages;
        m_homePageId = probe.m_homePageId;
        m_rootPage = probe.m_rootPage;
        m_studioAppearance = probe.m_studioAppearance;
    }
    m_appearance = probe.m_appearance;
    m_themeKeys = probe.m_themeKeys;
    m_composition = probe.m_composition;
    m_backgroundStart = probe.m_backgroundStart;
    m_backgroundEnd = probe.m_backgroundEnd;
    m_tileRoundness = probe.m_tileRoundness;
    m_brandVisible = probe.m_brandVisible;
    if (!focusComponentId.isEmpty())
        m_focusOrigin = focusComponentId;
    if (!save())
    {
        m_items = previousItems;
        m_categories = previousCategories;
        m_pages = previousPages;
        m_homePageId = previousHomePage;
        m_rootPage = previousRootPage;
        m_studioAppearance = previousStudioAppearance;
        m_appearance = previousAppearance;
        m_themeKeys = previousThemeKeys;
        m_composition = previousComposition;
        m_backgroundStart = previousStart;
        m_backgroundEnd = previousEnd;
        m_tileRoundness = previousRoundness;
        m_brandVisible = previousBrand;
        m_focusOrigin = previousFocus;
        syncPages();
        emit layoutChanged();
        if (documentData() != previousBytes)
        {
            load(m_defaultPath, m_userPath);
            m_focusOrigin = previousFocus;
            emit layoutChanged();
        }
        return false;
    }
    emit layoutChanged();
    setError({});
    return true;
}

bool
HomeLayout::validateStudioAppIconStyles(const QJsonObject& styles)
{
    if (styles.size() != 3)
        return false;
    for (const auto& name : QStringList{"signature", "tiles", "rail"})
        if (!styles[name].isObject() || !ComponentSchema::validPresentation(styles[name].toObject().toVariantMap()))
            return false;
    return true;
}

bool
HomeLayout::validateStudioArtworkCleanup(const QJsonObject& cleanup)
{
    const auto surfaces = cleanup.value("previewSurfaces").toArray();
    if (surfaces.size() != 3)
        return false;
    for (const auto& value : surfaces)
    {
        const auto item = value.toObject();
        const QColor colour(item["colour"].toString());
        if (item.size() != 2 || item["name"].toString().isEmpty() || item["name"].toString().size() > 40 ||
            !colour.isValid() || colour.alpha() != 255)
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioReferenceViewport(const QJsonObject& viewport)
{
    if (viewport.size() != 2)
        return false;
    const QMap<QString, QPair<int, int>> bounds{{"width", {640, 3840}}, {"height", {360, 2160}}};
    for (auto it = bounds.cbegin(); it != bounds.cend(); ++it)
    {
        const auto value = viewport[it.key()];
        const double number = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number ||
            number < it.value().first || number > it.value().second)
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioColorPicker(const QJsonObject& picker)
{
    if (picker.size() != 1 || !picker["presets"].isArray() || picker["presets"].toArray().isEmpty() ||
        picker["presets"].toArray().size() > 8)
        return false;
    for (const auto& entry : picker["presets"].toArray())
    {
        const auto swatch = entry.toObject();
        const QColor color(swatch["value"].toString());
        if (swatch.size() != 2 || !swatch["name"].isString() || swatch["name"].toString().isEmpty() ||
            swatch["name"].toString().size() > 40 || !swatch["value"].isString() || !color.isValid() ||
            color.alpha() != 255)
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioDesktop(const QJsonObject& desktop)
{
    if (desktop.size() != 8)
        return false;
    if (!validateDesktopStringKeys(desktop, QStringList{"title", "returnLabel", "returnShortcut"}))
        return false;
    if (!validateDesktopNumericKeys(desktop, QStringList{"width", "height", "margin", "radius", "labelSize"}))
        return false;
    return true;
}
bool
HomeLayout::validateDesktopStringKeys(const QJsonObject& obj, const QStringList& keys)
{
    for (const auto& key : keys)
        if (!obj[key].isString() || obj[key].toString().trimmed().isEmpty() ||
            obj[key].toString().size() > 80 || obj[key].toString().contains(QChar::Null))
            return false;
    return true;
}
bool
HomeLayout::validateDesktopNumericKeys(const QJsonObject& obj, const QStringList& keys)
{
    for (const auto& key : keys)
    {
        const auto value = obj[key];
        const auto n = value.toDouble(-1);
        const int low = key == "width"       ? 240
                        : key == "height"    ? 80
                        : key == "labelSize" ? 14
                                             : 0,
                  high = key == "width"       ? 600
                         : key == "height"    ? 240
                         : key == "labelSize" ? 40
                                              : 100;
        if (!value.isDouble() || !std::isfinite(n) || n < low || n > high || std::floor(n) != n)
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioThemeGroups(const QJsonArray& groups)
{
    if (groups.size() != 5)
        return false;
    QSet<QString> seenGroups;
    for (const auto& value : groups)
    {
        const auto group = value.toObject();
        const auto id = group["id"].toString();
        if (!ThemeColors::roles().contains(id) || seenGroups.contains(id) || group["label"].toString().isEmpty())
            return false;
        seenGroups.insert(id);
        QStringList keys;
        for (const auto& role : group["roles"].toArray())
        {
            const auto row = role.toObject();
            if (row["label"].toString().isEmpty())
                return false;
            keys.append(row["key"].toString());
        }
        auto expected = ThemeColors::roles().value(id);
        keys.sort();
        expected.sort();
        if (keys != expected)
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioSchemes(const QJsonArray& schemes)
{
    if (schemes.isEmpty() || schemes.size() > 32)
        return false;
    for (const auto& value : schemes)
    {
        const auto scheme = value.toObject();
        if (scheme["name"].toString().trimmed().isEmpty() || scheme["name"].toString().size() > 80 ||
            !ThemeColors::valid(
                QVariantMap{{"schemaVersion", 1}, {"colors", scheme["colors"].toObject().toVariantMap()}}))
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioStylePresets(const QJsonArray& presets)
{
    for (const auto& value : presets)
    {
        const auto preset = value.toObject();
        if (preset["name"].toString().isEmpty() ||
            !ComponentSchema::validPresentation(preset["presentation"].toObject().toVariantMap()))
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioTrialDurations(const QJsonArray& trials)
{
    if (trials.size() != 3)
        return false;
    for (int i = 0; i < trials.size(); ++i)
    {
        const auto trial = trials[i].toObject();
        const int seconds = QList<int>{30, 60, 300}[i];
        if (trial.size() != 2 || !trial["label"].isString() || trial["label"].toString().trimmed().isEmpty() ||
            trial["label"].toString().size() > 40 || !trial["seconds"].isDouble() ||
            trial["seconds"].toDouble() != seconds)
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioHeaderDefaults(const QJsonObject& defaults)
{
    if (defaults.size() != 2 || !defaults.contains("navigation") || !defaults.contains("identity"))
        return false;
    for (const auto& mode : QStringList{"navigation", "identity"})
        if (!defaults[mode].isObject() ||
            !ComponentSchema::validComposition({{"schemaVersion", 1},
                                                {"headerMode", mode},
                                                {"showClock", true},
                                                {"header", defaults[mode].toObject().toVariantMap()}}))
            return false;
    return true;
}

bool
HomeLayout::validateStudioSettings(const QJsonObject& object)
{
    if (!validateStudioAppIconStyles(object.value("appIconStyles").toObject()))
        return false;
    if (!validateStudioArtworkCleanup(object.value("artworkCleanup").toObject()))
        return false;
    if (!validateStudioReferenceViewport(object.value("referenceViewport").toObject()))
        return false;
    if (object.contains("responsiveDefaults") &&
        !DesignSystem::validResponsive(object["responsiveDefaults"].toObject().toVariantMap()))
        return false;
    if (object["schemaVersion"].toInt() != 1 || !object["defaultGrid"].isObject() ||
        !ComponentSchema::validGrid(object["defaultGrid"].toObject().toVariantMap()) || !object["gridModes"].isArray())
        return false;
    if (!object["appearance"].isObject() || !readableStudioAppearance(object["appearance"].toObject().toVariantMap()))
        return false;
    return true;
}

bool
HomeLayout::validateStudioAppPicker(const QJsonObject& picker)
{
    const QMap<QString, QPair<int, int>> bounds{{"columns", {2, 6}},
                                                {"cardHeight", {96, 240}},
                                                {"gap", {0, 32}},
                                                {"radius", {0, 40}},
                                                {"margin", {8, 48}},
                                                {"labelSize", {14, 28}},
                                                {"logoSize", {24, 96}}};
    if (picker.size() != bounds.size())
        return false;
    for (auto it = bounds.cbegin(); it != bounds.cend(); ++it)
    {
        const auto value = picker[it.key()];
        const auto n = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < it.value().first ||
            n > it.value().second)
            return false;
    }
    if (picker["cardHeight"].toInt() <
        picker["logoSize"].toInt() + 2 * picker["labelSize"].toInt() + 2 * picker["gap"].toInt())
        return false;
    return true;
}

bool
HomeLayout::validateStudioWidgetSpans(const QJsonObject& spans)
{
    for (const auto& type : QStringList{"clock", "text"})
    {
        const auto span = spans[type].toObject().toVariantMap();
        if (span.size() != 2 ||
            !ComponentSchema::validPlacement(
                {{"x", 0}, {"y", 0}, {"width", span.value("width")}, {"height", span.value("height")}}, 16, 10))
            return false;
    }
    return true;
}

bool
HomeLayout::validateStudioMenuWidth(const QJsonValue& value)
{
    return value.isDouble() && value.toDouble() >= 200 && value.toDouble() <= 480;
}

bool
HomeLayout::validateStudioSectionHeight(const QJsonValue& value)
{
    return value.isDouble() && value.toDouble() >= 60 && value.toDouble() <= 800;
}

QJsonObject
HomeLayout::validateStudioCatalog(const QVariantArray& catalog, const QVariantMap& presets)
{
    QSet<QString> catalogIds;
    if (presets.isEmpty() || presets.size() > 16 || catalog.isEmpty() || catalog.size() > 16)
        return {};
    for (const auto& entry : catalog)
    {
        const auto item = entry.toObject();
        const auto id = item["id"].toString(), name = item["name"].toString();
        if ((id != "widgets" && !presets.contains(id)) || catalogIds.contains(id) || name.trimmed().isEmpty() ||
            name.size() > 80 || item["description"].toString().size() > 160)
            return {};
        catalogIds.insert(id);
    }
    return QVariantMap{};
}

bool
HomeLayout::validateStudioActions(const QVariantArray& actions)
{
    for (const auto& entry : actions)
    {
        const auto choice = entry.toObject();
        if (!choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
            choice["name"].toString().size() > 160)
            return false;
        const auto actionList = choice["actions"].toArray();
        if (actionList.isEmpty() || ComponentSchema::action(actionList.first().toObject().toVariantMap()).isEmpty())
            return false;
        if (choice.contains("aliases"))
        {
            if (!choice["aliases"].isArray() || choice["aliases"].toArray().size() > 8)
                return false;
            for (const auto& alias : choice["aliases"].toArray())
                if (!alias.isString() || alias.toString().size() > 80 || alias.toString().contains(QChar::Null))
                    return false;
        }
    }
    return true;
}

bool
HomeLayout::validateStudioActionCategories(const QVariantArray& categories)
{
    const QStringList pickerTypes{"none",
                                  "media-picker",
                                  "root-page",
                                  "open-page",
                                  "launch-app",
                                  "open-apps",
                                  "open-search",
                                  "app-drawer",
                                  "open-collection",
                                  "open-guide",
                                  "play-movie",
                                  "open-show",
                                  "switch-input",
                                  "settings",
                                  "desktop-mode",
                                  "open-file",
                                  "open-folder",
                                  "open-url",
                                  "open-disc"};
    for (const auto& value : categories)
    {
        const auto category = value.toObject();
        if (category.size() != 2 || !category["name"].isString() || category["name"].toString().trimmed().isEmpty() ||
            category["name"].toString().size() > 80 || !category["types"].isArray() ||
            category["types"].toArray().size() > pickerTypes.size())
            return false;
        QSet<QString> types;
        for (const auto& type : category["types"].toArray())
        {
            if (!type.isString() || !pickerTypes.contains(type.toString()) || types.contains(type.toString()))
                return false;
            types.insert(type.toString());
        }
    }
    return categories.first().toObject()["types"].toArray().isEmpty();
}

bool
HomeLayout::validateStudioGridModes(const QVariantArray& modes, const QVariantMap& defaultGrid)
{
    QSet<QString> ids;
    for (const auto& entry : modes)
    {
        const auto mode = entry.toObject();
        const auto id = mode["id"].toString(), name = mode["name"].toString();
        if (!QStringList{"screen", "sections"}.contains(id) || ids.contains(id) || name.trimmed().isEmpty() ||
            name.size() > 80)
            return false;
        ids.insert(id);
    }
    return ids.size() == 2 && ids.contains(defaultGrid["mode"].toString());
}

bool
HomeLayout::validateStudioOverlayKeys(const QVariantArray& overlayKeys, const QVariantMap& guide)
{
    Q_UNUSED(overlayKeys);
    Q_UNUSED(guide);
    return true;
}

bool
HomeLayout::validateStudioDefaults(QJsonObject& object, QVariantMap& guide) const
{
    guide = object.contains("guide") ? ComponentSchema::guideOptions(object["guide"].toObject().toVariantMap())
                                     : QVariantMap();
    if (object.contains("guide") && guide.isEmpty())
        return false;
    if (!validateStudioHeaderDefaults(object.value("headerDefaults").toObject()))
        return false;
    if (!validateStudioAppPicker(object.value("appPicker").toObject()))
        return false;
    if (!validateStudioDesktop(object.value("desktop").toObject()))
        return false;
    if (!validateStudioThemeGroups(simple["themeGroups"].toArray()))
        return false;
    if (!validateStudioSchemes(simple["schemes"].toArray()))
        return false;
    if (!validateStudioStylePresets(simple["stylePresets"].toArray()))
        return false;
    if (!validateStudioTrialDurations(object["trialDurations"].toArray()))
        return false;
    if (!validateStudioMenuWidth(object["menuWidth"]))
        return false;
    if (!validateStudioSectionHeight(object["sectionHeight"]))
        return false;
    if (!validateStudioWidgetSpans(object.value("widgetSpans").toObject()))
        return false;
    const auto presets = object["componentPresets"].toObject();
    if (!validateStudioCatalog(object["catalog"].toArray(), presets))
        return false;
    if (!validateStudioActions(object["actions"].toArray()))
        return false;
    if (!validateStudioActionCategories(object["actionCategories"].toArray()))
        return false;
    if (!validateStudioGridModes(object["gridModes"].toArray(), object["defaultGrid"].toObject()))
        return false;
    if (!validateStudioPanelPreset(object))
        return false;
    if (!validateStudioCatalogIds(object, presets))
        return false;
    if (!validateStudioActionsList(object["actions"].toArray()))
        return false;
    const auto actionCategories = object["actionCategories"].toArray();
    if (!object["actionCategories"].isArray() || actionCategories.isEmpty() || actionCategories.size() > 12)
        return false;
    if (!validateStudioActionCategoryTypes(actionCategories))
        return false;
    if (!validateStudioGridModeIds(object["gridModes"].toArray()))
        return false;
    const QStringList overlayKeys{"surface", "overlayWidth", "overlayHeight", "overlayRows", "overlayPosition"};
    if (!validateStudioOverlayKeys(overlayKeys, {}))
        return false;
    return true;
}

bool
HomeLayout::finalizeStudioOptions(QVariantMap& result, const QVariantMap& guide) const
{
    result["guide"] = guide;
    const QStringList overlayKeys{"surface", "overlayWidth", "overlayHeight", "overlayRows", "overlayPosition"};
    if (guide.isEmpty() || std::any_of(overlayKeys.cbegin(),
                                       overlayKeys.cend(),
                                       [&guide](const QString& key)
                                       {
                                           return !guide.contains(key);
                                       }))
    {
        QFile packaged(":/config/studio-defaults.json");
        if (!packaged.open(QIODevice::ReadOnly) || packaged.size() > 128 * 1024)
            return false;
        const auto defaults = ComponentSchema::guideOptions(
            QJsonDocument::fromJson(packaged.readAll()).object()["guide"].toObject().toVariantMap());
        if (defaults.isEmpty())
            return false;
        for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
            if (!guide.contains(it.key()))
                guide[it.key()] = it.value();
    }
    return true;
}

