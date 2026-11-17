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
    if (object.contains("appIconStyles"))
    {
        const auto styles = object["appIconStyles"].toObject();
        if (styles.size() != 3)
            return {};
        for (const auto& name : QStringList{"signature", "tiles", "rail"})
            if (!styles[name].isObject() || !ComponentSchema::validPresentation(styles[name].toObject().toVariantMap()))
                return {};
    }
    if (object.contains("artworkCleanup"))
    {
        const auto surfaces = object["artworkCleanup"].toObject()["previewSurfaces"].toArray();
        if (surfaces.size() != 3)
            return {};
        for (const auto& value : surfaces)
        {
            const auto item = value.toObject();
            const QColor colour(item["colour"].toString());
            if (item.size() != 2 || item["name"].toString().isEmpty() || item["name"].toString().size() > 40 ||
                !colour.isValid() || colour.alpha() != 255)
                return {};
        }
    }
    if (object.contains("referenceViewport"))
    {
        const auto viewport = object["referenceViewport"].toObject();
        if (viewport.size() != 2)
            return {};
        const QMap<QString, QPair<int, int>> bounds{{"width", {640, 3840}}, {"height", {360, 2160}}};
        for (auto it = bounds.cbegin(); it != bounds.cend(); ++it)
        {
            const auto value = viewport[it.key()];
            const double number = value.toDouble(-1);
            if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number ||
                number < it.value().first || number > it.value().second)
                return {};
        }
    }
    if (object.contains("responsiveDefaults") &&
        !DesignSystem::validResponsive(object["responsiveDefaults"].toObject().toVariantMap()))
        return {};
    if (object["schemaVersion"].toInt() != 1 || !object["defaultGrid"].isObject() ||
        !ComponentSchema::validGrid(object["defaultGrid"].toObject().toVariantMap()) || !object["gridModes"].isArray())
        return {};
    if (!object["appearance"].isObject() || !readableStudioAppearance(object["appearance"].toObject().toVariantMap()))
        return {};
    auto guide = object.contains("guide") ? ComponentSchema::guideOptions(object["guide"].toObject().toVariantMap())
                                          : QVariantMap();
    if (object.contains("guide") && guide.isEmpty())
        return {};
    if (object.contains("headerDefaults"))
    {
        const auto defaults = object["headerDefaults"].toObject();
        if (defaults.size() != 2 || !defaults.contains("navigation") || !defaults.contains("identity"))
            return {};
        for (const auto& mode : QStringList{"navigation", "identity"})
            if (!defaults[mode].isObject() ||
                !ComponentSchema::validComposition({{"schemaVersion", 1},
                                                    {"headerMode", mode},
                                                    {"showClock", true},
                                                    {"header", defaults[mode].toObject().toVariantMap()}}))
                return {};
    }
    if (object.contains("appPicker"))
    {
        const auto picker = object["appPicker"].toObject();
        const QMap<QString, QPair<int, int>> bounds{{"columns", {2, 6}},
                                                    {"cardHeight", {96, 240}},
                                                    {"gap", {0, 32}},
                                                    {"radius", {0, 40}},
                                                    {"margin", {8, 48}},
                                                    {"labelSize", {14, 28}},
                                                    {"logoSize", {24, 96}}};
        if (picker.size() != bounds.size())
            return {};
        for (auto it = bounds.cbegin(); it != bounds.cend(); ++it)
        {
            const auto value = picker[it.key()];
            const auto n = value.toDouble(-1);
            if (!value.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < it.value().first ||
                n > it.value().second)
                return {};
        }
        if (picker["cardHeight"].toInt() <
            picker["logoSize"].toInt() + 2 * picker["labelSize"].toInt() + 2 * picker["gap"].toInt())
            return {};
    }
    const auto desktop = object["desktop"].toObject();
    if (desktop.size() != 8)
        return {};
    for (const auto& key : QStringList{"title", "returnLabel", "returnShortcut"})
        if (!desktop[key].isString() || desktop[key].toString().trimmed().isEmpty() ||
            desktop[key].toString().size() > 80 || desktop[key].toString().contains(QChar::Null))
            return {};
    for (const auto& key : QStringList{"width", "height", "margin", "radius", "labelSize"})
    {
        const auto value = desktop[key];
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
            return {};
    }
    if (object.contains("colorPicker"))
    {
        const auto picker = object["colorPicker"].toObject();
        if (picker.size() != 1 || !picker["presets"].isArray() || picker["presets"].toArray().isEmpty() ||
            picker["presets"].toArray().size() > 8)
            return {};
        for (const auto& entry : picker["presets"].toArray())
        {
            const auto swatch = entry.toObject();
            const QColor color(swatch["value"].toString());
            if (swatch.size() != 2 || !swatch["name"].isString() || swatch["name"].toString().isEmpty() ||
                swatch["name"].toString().size() > 40 || !swatch["value"].isString() || !color.isValid() ||
                color.alpha() != 255)
                return {};
        }
    }
    const auto simple = object["simpleStudio"].toObject();
    const auto groups = simple["themeGroups"].toArray();
    if (groups.size() != 5 || simple["schemes"].toArray().isEmpty() || simple["schemes"].toArray().size() > 32)
        return {};
    QSet<QString> seenGroups;
    for (const auto& value : groups)
    {
        const auto group = value.toObject();
        const auto id = group["id"].toString();
        if (!ThemeColors::roles().contains(id) || seenGroups.contains(id) || group["label"].toString().isEmpty())
            return {};
        seenGroups.insert(id);
        QStringList keys;
        for (const auto& role : group["roles"].toArray())
        {
            const auto row = role.toObject();
            if (row["label"].toString().isEmpty())
                return {};
            keys.append(row["key"].toString());
        }
        auto expected = ThemeColors::roles().value(id);
        keys.sort();
        expected.sort();
        if (keys != expected)
            return {};
    }
    for (const auto& value : simple["schemes"].toArray())
    {
        const auto scheme = value.toObject();
        if (scheme["name"].toString().trimmed().isEmpty() || scheme["name"].toString().size() > 80 ||
            !ThemeColors::valid(
                QVariantMap{{"schemaVersion", 1}, {"colors", scheme["colors"].toObject().toVariantMap()}}))
            return {};
    }
    for (const auto& value : simple["stylePresets"].toArray())
    {
        const auto preset = value.toObject();
        if (preset["name"].toString().isEmpty() ||
            !ComponentSchema::validPresentation(preset["presentation"].toObject().toVariantMap()))
            return {};
    }
    const auto trials = object["trialDurations"].toArray();
    if (trials.size() != 3)
        return {};
    for (int i = 0; i < trials.size(); ++i)
    {
        const auto trial = trials[i].toObject();
        const int seconds = QList<int>{30, 60, 300}[i];
        if (trial.size() != 2 || !trial["label"].isString() || trial["label"].toString().trimmed().isEmpty() ||
            trial["label"].toString().size() > 40 || !trial["seconds"].isDouble() ||
            trial["seconds"].toDouble() != seconds)
            return {};
    }
    if (!object["menuWidth"].isDouble() || object["menuWidth"].toDouble() < 200 || object["menuWidth"].toDouble() > 480)
        return {};
    if (!object["sectionHeight"].isDouble() || object["sectionHeight"].toDouble() < 60 ||
        object["sectionHeight"].toDouble() > 800)
        return {};
    for (const auto& type : QStringList{"clock", "text"})
    {
        const auto span = object["widgetSpans"].toObject()[type].toObject().toVariantMap();
        if (span.size() != 2 ||
            !ComponentSchema::validPlacement(
                {{"x", 0}, {"y", 0}, {"width", span.value("width")}, {"height", span.value("height")}}, 16, 10))
            return {};
    }
    const auto group = ComponentSchema::component(object["groupPreset"].toObject().toVariantMap(), true);
    if (group.isEmpty() || group.value("componentType") != "container")
        return {};
    const auto panel = object["inputPanelPreset"].toObject();
    if (!panel["children"].isArray() || panel["children"].toArray().size() > 32)
        return {};
    QJsonArray panelItems;
    const auto panelRoot = ComponentSchema::component(panel["root"].toObject().toVariantMap(), true);
    if (panelRoot.isEmpty() || panelRoot.value("componentType") != "container")
        return {};
    panelItems.append(QJsonObject::fromVariantMap(panelRoot));
    for (const auto& entry : panel["children"].toArray())
    {
        const auto child = ComponentSchema::component(entry.toObject().toVariantMap(), true);
        if (child.isEmpty())
            return {};
        panelItems.append(QJsonObject::fromVariantMap(child));
    }
    if (!ComponentSchema::hierarchyFits(
            panelItems,
            {{"schemaVersion", 1}, {"enabled", true}, {"grid", object["defaultGrid"].toObject().toVariantMap()}},
            {}))
        return {};
    const auto presets = object["componentPresets"].toObject();
    if (object.contains("componentVariants"))
    {
        if (!object["componentVariants"].isObject() || object["componentVariants"].toObject().size() > 3)
            return {};
        const auto families = object["componentVariants"].toObject();
        for (auto family = families.begin(); family != families.end(); ++family)
        {
            if (!QStringList{"button", "image", "text"}.contains(family.key()) || !family.value().isArray() ||
                family.value().toArray().isEmpty() || family.value().toArray().size() > 8)
                return {};
            QSet<QString> variants;
            for (const auto& value : family.value().toArray())
            {
                const auto variant = value.toObject();
                const auto id = variant["id"].toString();
                if (variant.size() != 5 || !variant["id"].isString() || !ComponentSchema::safeId(id) ||
                    variants.contains(id) || !variant["name"].isString() ||
                    variant["name"].toString().trimmed().isEmpty() || variant["name"].toString().size() > 80 ||
                    !variant["description"].isString() || variant["description"].toString().size() > 160 ||
                    !variant["presentation"].isObject() || !variant["span"].isObject() ||
                    variant["span"].toObject().size() != 2)
                    return {};
                auto candidate = presets[family.key()].toObject(), style = candidate["presentation"].toObject();
                const auto changes = variant["presentation"].toObject();
                for (auto it = changes.begin(); it != changes.end(); ++it)
                    style[it.key()] = it.value();
                candidate["presentation"] = style;
                const auto span = variant["span"].toObject();
                QJsonObject placement{{"x", 0}, {"y", 0}, {"width", span["width"]}, {"height", span["height"]}};
                candidate["placement"] = placement;
                if (ComponentSchema::component(candidate.toVariantMap(), true).isEmpty())
                    return {};
                variants.insert(id);
            }
        }
    }
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
            return {};
    if (object.contains("shapeChoices"))
    {
        if (!object["shapeChoices"].isArray() || object["shapeChoices"].toArray().isEmpty() ||
            object["shapeChoices"].toArray().size() > 8)
            return {};
        for (const auto& value : object["shapeChoices"].toArray())
        {
            const auto choice = value.toObject();
            const auto type = choice["type"].toString();
            if (choice.size() != 3 || !choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
                choice["name"].toString().size() > 80 || !QStringList{"shape", "container"}.contains(type) ||
                !choice["presentation"].isObject())
                return {};
            auto candidate = type == "shape" ? presets["shape"].toObject() : object["groupPreset"].toObject();
            auto style = candidate["presentation"].toObject();
            const auto changes = choice["presentation"].toObject();
            for (auto it = changes.begin(); it != changes.end(); ++it)
                style[it.key()] = it.value();
            candidate["presentation"] = style;
            if (ComponentSchema::component(candidate.toVariantMap(), true).isEmpty())
                return {};
        }
    }
    if (object.contains("replacementChoices"))
    {
        const auto choices = object["replacementChoices"].toArray();
        if (!object["replacementChoices"].isArray() || choices.isEmpty() || choices.size() > 5)
            return {};
        QSet<QString> types;
        for (const auto& value : choices)
        {
            const auto choice = value.toObject();
            const auto type = choice["type"].toString();
            if (choice.size() != 2 || !choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
                choice["name"].toString().size() > 80 ||
                !QStringList{"button", "image", "text", "shape", "container"}.contains(type) || types.contains(type))
                return {};
            types.insert(type);
        }
    }
    const auto catalog = object["catalog"].toArray();
    QSet<QString> catalogIds;
    if (presets.isEmpty() || presets.size() > 16 || catalog.isEmpty() || catalog.size() > 16 ||
        object["inspector"].toObject()["fields"].toArray().size() > 160)
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
    for (const auto& entry : object["actions"].toArray())
    {
        const auto choice = entry.toObject();
        if (!choice["name"].isString() || choice["name"].toString().trimmed().isEmpty() ||
            choice["name"].toString().size() > 160)
            return {};
        const auto actionList = choice["actions"].toArray();
        if (actionList.isEmpty() || ComponentSchema::action(actionList.first().toObject().toVariantMap()).isEmpty())
            return {};
        if (choice.contains("aliases"))
        {
            if (!choice["aliases"].isArray() || choice["aliases"].toArray().size() > 8)
                return {};
            for (const auto& alias : choice["aliases"].toArray())
                if (!alias.isString() || alias.toString().size() > 80 || alias.toString().contains(QChar::Null))
                    return {};
        }
    }
    const auto actionCategories = object["actionCategories"].toArray();
    if (!object["actionCategories"].isArray() || actionCategories.isEmpty() || actionCategories.size() > 12)
        return {};
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
    for (const auto& value : actionCategories)
    {
        const auto category = value.toObject();
        if (category.size() != 2 || !category["name"].isString() || category["name"].toString().trimmed().isEmpty() ||
            category["name"].toString().size() > 80 || !category["types"].isArray() ||
            category["types"].toArray().size() > pickerTypes.size())
            return {};
        QSet<QString> types;
        for (const auto& type : category["types"].toArray())
        {
            if (!type.isString() || !pickerTypes.contains(type.toString()) || types.contains(type.toString()))
                return {};
            types.insert(type.toString());
        }
    }
    if (!actionCategories.first().toObject()["types"].toArray().isEmpty())
        return {};
    QSet<QString> ids;
    for (const auto& entry : object["gridModes"].toArray())
    {
        const auto mode = entry.toObject();
        const auto id = mode["id"].toString(), name = mode["name"].toString();
        if (!QStringList{"screen", "sections"}.contains(id) || ids.contains(id) || name.trimmed().isEmpty() ||
            name.size() > 80)
            return {};
        ids.insert(id);
    }
    if (ids.size() != 2 || !ids.contains(object["defaultGrid"].toObject()["mode"].toString()))
        return {};
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
            return {};
        const auto defaults = ComponentSchema::guideOptions(
            QJsonDocument::fromJson(packaged.readAll()).object()["guide"].toObject().toVariantMap());
        if (defaults.isEmpty())
            return {};
        for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
            if (!guide.contains(it.key()))
                guide[it.key()] = it.value();
    }
    auto result = object.toVariantMap();
    if (!ComponentSchema::validSettingsNavigation(result.value("settingsNavigation").toMap()))
    {
        QFile packaged(":/config/studio-defaults.json");
        if (!packaged.open(QIODevice::ReadOnly) || packaged.size() > 128 * 1024)
            return {};
        const auto navigation =
            QJsonDocument::fromJson(packaged.readAll()).object()["settingsNavigation"].toObject().toVariantMap();
        if (!ComponentSchema::validSettingsNavigation(navigation))
            return {};
        result["settingsNavigation"] = navigation;
    }
    if (!result.contains("referenceViewport"))
    {
        QFile packaged(":/config/studio-defaults.json");
        if (!packaged.open(QIODevice::ReadOnly) || packaged.size() > 128 * 1024)
            return {};
        result["referenceViewport"] =
            QJsonDocument::fromJson(packaged.readAll()).object()["referenceViewport"].toVariant();
    }
    result["guide"] = guide;
    result["maxScrollableRows"] = ComponentSchema::MaxScrollableRows;
    m_optionsCache = result;
    return m_optionsCache;
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
    if (!normalizeAppearance(presentation))
        return false;
    const QColor startColor(theme.value(QStringLiteral("backgroundStart")).toString());
    const QColor endColor(theme.value(QStringLiteral("backgroundEnd")).toString());
    const auto roundness = theme.contains(QStringLiteral("tileRoundness"))
                               ? theme.value(QStringLiteral("tileRoundness")).toDouble()
                               : -1.0;
    const auto tileOpacity =
        theme.contains(QStringLiteral("tileOpacity")) ? theme.value(QStringLiteral("tileOpacity")).toDouble() : 1.0;
    if (!startColor.isValid() || !endColor.isValid() || !qIsFinite(roundness) || !qIsFinite(tileOpacity) ||
        roundness < 0.0 || roundness > 1.0 || tileOpacity < 0.2 || tileOpacity > 1.0)
        return false;
    const auto previousStart = m_backgroundStart;
    const auto previousEnd = m_backgroundEnd;
    const auto previousRoundness = m_tileRoundness;
    const auto previousItems = m_items;
    const auto previousAppearance = m_appearance;
    const auto previousKeys = m_themeKeys;
    for (auto it = presentation.cbegin(); it != presentation.cend(); ++it)
        m_themeKeys.insert(it.key());
    m_themeKeys.insert("backgroundStart");
    m_themeKeys.insert("backgroundEnd");
    m_themeKeys.insert("tileRoundness");
    m_appearance = presentation;
    m_backgroundStart = startColor.name();
    m_backgroundEnd = endColor.name();
    m_tileRoundness = roundness;
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
