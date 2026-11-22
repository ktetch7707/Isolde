#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "home/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <functional>

QVariantList
StudioSession::actionChoices() const
{
    QVariantList choices;
    if (!currentProfile())
        return choices;
    for (const auto& entry : m_live->studioOptions().value("actions").toList())
        choices.append(entry);
    const auto document = QJsonDocument::fromJson(m_draft).object();
    const auto root = document["rootPage"].toObject();
    if (!root.contains("visible") || root["visible"].toBool())
        choices.append(QVariantMap{{"name",
                                    tr("Open page · ") + root.value("name").toString(
                                                             m_live->studioOptions().value("rootPageName").toString())},
                                   {"action", QVariantMap{{"type", "root-page"}}}});
    for (const auto& entry : document["pages"].toArray())
    {
        const auto page = entry.toObject();
        choices.append(QVariantMap{{"name", tr("Open page · ") + page["name"].toString()},
                                   {"action", QVariantMap{{"type", "open-page"}, {"pageId", page["id"].toString()}}}});
    }
    for (const auto& entry : m_live->registry()->apps())
    {
        const auto app = entry.toMap();
        const auto id = app.value("id").toString();
        choices.append(QVariantMap{{"name", tr("Open app · ") + app.value("name").toString()},
                                   {"logoSource", app.value("logoSource")},
                                   {"appType", app.value("type")},
                                   {"appSection", app.value("section")},
                                   {"action", QVariantMap{{"type", "launch-app"}, {"appId", id}}}});
        const auto* definition = m_live->registry()->find(id);
        if (definition)
            for (const auto& type : definition->handledActions)
                if (QStringList{"open-file", "open-folder", "open-url", "open-disc"}.contains(type))
                    choices.append(QVariantMap{{"name", type + " · " + app.value("name").toString()},
                                               {"needsTarget", true},
                                               {"action", QVariantMap{{"type", type}, {"openWith", id}}}});
    }
    for (const auto& entry : m_live->m_collections)
    {
        const auto collection = entry.toMap();
        choices.append(QVariantMap{
            {"name", tr("Open collection · ") + collection.value("name").toString()},
            {"mediaKind", collection.value("media", "movies")},
            {"action", QVariantMap{{"type", "open-collection"}, {"collectionId", collection.value("id")}}}});
    }
    return choices;
}

QVariantList
StudioSession::sourceChoices(const QString& id, const QString& pageId) const
{
    if (!currentProfile())
        return {};
    HomeLayout worker(m_live->registry());
    configureWorker(worker);
    auto document = QJsonDocument::fromJson(m_draft).object();
    if (!pageId.isEmpty())
    {
        bool found = false;
        for (const auto& entry : document["pages"].toArray())
            if (entry.toObject()["id"] == pageId)
            {
                document = entry.toObject()["layout"].toObject();
                found = true;
                break;
            }
        if (!found)
            return {};
    }
    if (!pageId.isEmpty())
    {
        auto composition = document["composition"].toObject();
        const QJsonValue library =
            QJsonDocument::fromJson(m_draft).object()["composition"].toObject().value("designLibrary");
        if (!library.isUndefined())
            composition["designLibrary"] = library;
        document["composition"] = composition;
    }
    if (!worker.loadDocument(QJsonDocument(document).toJson(), false))
        return {};
    return worker.sourceChoices(id);
}

QObject*
StudioSession::proposalModel() const
{
    return m_proposalModel;
}

void
StudioSession::configureWorker(HomeLayout& worker, bool assetPaths) const
{
    worker.m_draftOnly = true;
    if (!m_live)
        return;
    worker.m_defaultPath = m_live->m_defaultPath;
    worker.m_defaultTheme = m_live->m_defaultTheme;
    worker.m_collections = m_live->m_collections;
    worker.m_captureInputsReady = m_live->m_captureInputsReady;
    worker.m_assetPaths = m_live->m_assetPaths;
    for (auto it = m_stagedAssets.cbegin(); it != m_stagedAssets.cend(); ++it)
        worker.m_assetPaths[it.key()] = it.value();
    if (assetPaths)
        worker.m_userPath = m_live->m_userPath;
}

bool
StudioSession::validNewAction(const QVariantMap& value)
{
    if (value.isEmpty())
        return true;
    QString error;
    const auto action = ComponentSchema::action(value, &error);
    if (action.isEmpty())
    {
        setError(error);
        return false;
    }
    const auto type = action.value("type").toString();
    if (type == "open-page" || type == "show-page-in-panel")
    {
        bool exists = false;
        for (const auto& entry : m_model->studioPages())
            if (entry.toMap().value("id") == action.value("pageId"))
                exists = true;
        if (!exists)
        {
            setError(tr("Choose an existing destination page."));
            return false;
        }
    }
    if (type == "root-page" && !m_model->rootPageVisible())
    {
        setError(tr("This page was removed. Choose an existing destination."));
        return false;
    }
    return true;
}

// Check every destination before a sequence becomes part of a Studio draft.
bool
StudioSession::validNewActions(const QVariant& value)
{
    if (value.metaType().id() != QMetaType::QVariantList)
    {
        setError(tr("Choose a valid action sequence."));
        return false;
    }
    for (const auto& entry : value.toList())
        if (entry.metaType().id() != QMetaType::QVariantMap || entry.toMap().isEmpty() ||
            !validNewAction(entry.toMap()))
        {
            if (m_error.isEmpty())
                setError(tr("Choose a valid action sequence."));
            return false;
        }
    return true;
}

QVariantMap
StudioSession::componentPrototype(const QString& type, const QVariantMap& configuration)
{
    const auto options = m_live->studioOptions();
    QJsonArray nodes;
    if (!validatePrototypePrerequisites(type, options, nodes))
        return {};
    auto configured = nodes.first().toObject();
    if (configuration.contains("variant") && !applyVariantSelection(configuration, options, type, configured))
        return {};
    if (!validateComponentConfigKeys(configuration))
        return {};
    if (!applyComponentConfiguration(configuration, configured))
        return {};
    nodes[0] = configured;
    if (configuration.contains("buttons") && !applyCarouselButtons(configuration, options, type, configured, nodes))
        return {};
    if (configuration.contains("apps") && !applyAppCollection(type, configuration, configured, nodes))
        return {};
    return finalizePrototype(type, nodes);
}

QVariantMap
StudioSession::finalizePrototype(const QString& type, QJsonArray& nodes)
{
    QHash<QString, QString> remap;
    if (!allocateNodeIdentities(nodes, type, remap))
        return {};
    if (!remapNodeParentIds(nodes, remap))
        return {};
    const auto root = nodes.first().toObject();
    if (!root.value("parentId").toString().isEmpty())
    {
        setError(tr("The preset root cannot have an external parent."));
        return {};
    }
    HomeLayout view(m_live->registry());
    configureWorker(view, true);
    view.m_items = nodes;
    return {{"type", type},
            {"rootId", root["id"].toString()},
            {"root", root.toVariantMap()},
            {"components", nodes.toVariantList()},
            {"viewItems", view.items()}};
}

bool
StudioSession::validatePrototypePrerequisites(const QString& type, const QVariantMap& options, QJsonArray& nodes)
{
    if (!currentProfile())
    {
        setError(tr("Reopen Skin Studio before adding components."));
        return false;
    }
    if (!selectComponentNodes(type, options, nodes))
        return false;
    if (nodes.isEmpty() || nodes.size() > 129)
    {
        setError(tr("The packaged component preset is invalid."));
        return false;
    }
    return true;
}

bool
StudioSession::applyVariantSelection(const QVariantMap& configuration, const QVariantMap& options,
                                     const QString& type, QJsonObject& configured)
{
    if (configuration["variant"].metaType().id() != QMetaType::QString)
    {
        setError(tr("Choose a registered starting style."));
        return false;
    }
    QVariantMap selected;
    for (const auto& entry : options.value("componentVariants").toMap().value(type).toList())
        if (entry.toMap().value("id") == configuration["variant"])
            selected = entry.toMap();
    if (!applyVariantConfiguration(configuration, options, type, configured))
        return false;
    return true;
}

bool
StudioSession::validateComponentConfigKeys(const QVariantMap& configuration)
{
    for (auto it = configuration.cbegin(); it != configuration.cend(); ++it)
    {
        if (!QStringList{"name", "action", "actions", "customLogo", "presentation", "appCollection", "apps", "buttons", "variant"}
                 .contains(it.key()))
        {
            setError(tr("Unsupported component configuration."));
            return false;
        }
        if (it.key() == "apps" || it.key() == "buttons" || it.key() == "variant")
            continue;
    }
    return true;
}

bool
StudioSession::applyCarouselButtons(const QVariantMap& configuration, const QVariantMap& options,
                                    const QString& type, QJsonObject& configured, QJsonArray& nodes)
{
    const auto buttons = configuration.value("buttons").toList();
    if (type != "carousel" || configuration.value("buttons").metaType().id() != QMetaType::QVariantList ||
        buttons.size() > 128)
    {
        setError(tr("Choose up to 128 carousel buttons."));
        return false;
    }
    auto style = configured["presentation"].toObject();
    auto layout = style["layout"].toObject();
    layout["columns"] = qMax(layout["columns"].toInt(), int(buttons.size()));
    style["layout"] = layout;
    configured["presentation"] = style;
    nodes[0] = configured;
    int column = 0;
    for (const auto& value : buttons)
    {
        const auto changes = value.toMap();
        if (value.metaType().id() != QMetaType::QVariantMap)
        {
            setError(tr("Choose a named button."));
            return false;
        }
        auto child = QJsonObject::fromVariantMap(options.value("buttonChildPreset").toMap());
        for (auto it = changes.cbegin(); it != changes.cend(); ++it)
        {
            if (!QStringList{"name", "action", "actions"}.contains(it.key()))
            {
                setError(tr("Unsupported carousel button property."));
                return false;
            }
            child[it.key()] = QJsonValue::fromVariant(it.value());
        }
        child["id"] = "carousel-button-" + QString::number(column + 1);
        child["parentId"] = configured["id"];
        auto p = child["placement"].toObject();
        p["x"] = column++;
        child["placement"] = p;
        nodes.append(child);
    }
    return true;
}

bool
StudioSession::applyAppCollection(const QString& type, const QVariantMap& configuration, QJsonObject& configured, QJsonArray& nodes)
{
    const auto apps = configuration.value("apps").toList();
    QSet<QString> selected;
    if (type != "app-bar" || configuration.value("apps").metaType().id() != QMetaType::QVariantList ||
        apps.size() > 128)
    {
        setError(tr("Choose up to 128 installed apps."));
        return false;
    }
    for (const auto& value : apps)
    {
        const auto id = value.toString();
        const auto* app = m_live->registry()->find(id);
        if (value.metaType().id() != QMetaType::QString || selected.contains(id) || !app ||
            !QStringList{"desktop", "web", "android"}.contains(app->type))
        {
            setError(tr("Choose distinct installed applications."));
            return false;
        }
        selected.insert(id);
    }
    configured["appCollection"] = QJsonObject{{"mode", "selected"}, {"ids", QJsonArray::fromVariantList(apps)}};
    nodes[0] = configured;
    return true;
}

bool
StudioSession::selectComponentNodes(const QString& type, const QVariantMap& options, QJsonArray& nodes)
{
    if (options.value("componentPresets").toMap().contains(type))
        nodes.append(QJsonObject::fromVariantMap(options.value("componentPresets").toMap().value(type).toMap()));
    else if (type == "container")
        nodes.append(QJsonObject::fromVariantMap(options.value("groupPreset").toMap()));
    else if (type == "input-panel")
    {
        const auto preset = options.value("inputPanelPreset").toMap();
        nodes.append(QJsonObject::fromVariantMap(preset.value("root").toMap()));
        for (const auto& entry : preset.value("children").toList())
            nodes.append(QJsonObject::fromVariantMap(entry.toMap()));
    }
    else if (type == "clock" || type == "text")
    {
        HomeLayout factory(m_live->registry());
        configureWorker(factory);
        if (!factory.addWidget(type))
        {
            setError(factory.errorString());
            return false;
        }
        auto item = factory.m_items.last().toObject();
        const auto span = options.value("widgetSpans").toMap().value(type).toMap();
        item["placement"] = QJsonObject::fromVariantMap(
            {{"x", 0}, {"y", 0}, {"width", span.value("width")}, {"height", span.value("height")}});
        nodes.append(item);
    }
    else
    {
        setError(tr("Choose a registered component type."));
        return false;
    }
    return true;
}

bool
StudioSession::applyVariantConfiguration(const QVariantMap& configuration, const QVariantMap& options,
                                         const QString& type, QJsonObject& configured)
{
    if (configuration.contains("variant"))
    {
        if (configuration["variant"].metaType().id() != QMetaType::QString)
        {
            setError(tr("Choose a registered starting style."));
            return false;
        }
        QVariantMap selected;
        for (const auto& entry : options.value("componentVariants").toMap().value(type).toList())
            if (entry.toMap().value("id") == configuration["variant"])
                selected = entry.toMap();
        if (selected.isEmpty())
        {
            setError(tr("This starting style is not available for that component."));
            return false;
        }
        auto style = configured["presentation"].toObject();
        const auto changes = selected["presentation"].toMap();
        for (auto it = changes.cbegin(); it != changes.cend(); ++it)
            style[it.key()] = QJsonValue::fromVariant(it.value());
        configured["presentation"] = style;
        auto placement = configured["placement"].toObject();
        const auto span = selected["span"].toMap();
        placement["width"] = QJsonValue::fromVariant(span["width"]);
        placement["height"] = QJsonValue::fromVariant(span["height"]);
        configured["placement"] = placement;
    }
    return true;
}

bool
StudioSession::applyComponentConfiguration(const QVariantMap& configuration, QJsonObject& configured)
{
    for (auto it = configuration.cbegin(); it != configuration.cend(); ++it)
    {
        if (it.key() == "apps" || it.key() == "buttons" || it.key() == "variant")
            continue;
        if (it.key() == "presentation")
        {
            if (it.value().metaType().id() != QMetaType::QVariantMap)
            {
                setError(tr("Choose valid presentation values."));
                return false;
            }
            auto style = configured["presentation"].toObject();
            const auto changes = it.value().toMap();
            for (auto change = changes.cbegin(); change != changes.cend(); ++change)
                style[change.key()] = QJsonValue::fromVariant(change.value());
            configured["presentation"] = style;
        }
        else if (it.key() == "action" && it.value().toMap().isEmpty())
            configured.remove("action");
        else
            configured[it.key()] = QJsonValue::fromVariant(it.value());
    }
    return true;
}

bool
StudioSession::allocateNodeIdentities(QJsonArray& nodes, const QString& type, QHash<QString, QString>& remap)
{
    QSet<QString> allocated;
    for (int i = 0; i < nodes.size(); ++i)
    {
        if (!validNewAction(nodes[i].toObject()["action"].toObject().toVariantMap()))
            return false;
        if (nodes[i].toObject().contains("actions") &&
            !validNewActions(nodes[i].toObject()["actions"].toVariant()))
            return false;
        QString error;
        const auto canonical = ComponentSchema::component(nodes[i].toObject().toVariantMap(), true, &error);
        if (canonical.isEmpty())
        {
            setError(error);
            return false;
        }
        auto item = QJsonObject::fromVariantMap(canonical);
        const auto oldId = item["id"].toString();
        if (remap.contains(oldId))
        {
            setError(tr("The preset contains duplicate identities."));
            return false;
        }
        auto identity = allocateId((type == "clock" || type == "text") ? type : oldId);
        if (identity.isEmpty())
            return false;
        const auto base = identity;
        int suffix = 2;
        while (allocated.contains(identity))
            identity = base + QString::number(suffix++);
        if (!ComponentSchema::safeId(identity))
        {
            setError(tr("The component identity exceeds its limit."));
            return false;
        }
        allocated.insert(identity);
        remap[oldId] = identity;
        item["id"] = identity;
        nodes[i] = item;
    }
    return true;
}

bool
StudioSession::remapNodeParentIds(QJsonArray& nodes, const QHash<QString, QString>& remap)
{
    for (int i = 0; i < nodes.size(); ++i)
    {
        auto item = nodes[i].toObject();
        const auto parent = item.value("parentId").toString();
        if (!parent.isEmpty())
        {
            if (!remap.contains(parent))
            {
                setError(tr("The preset refers to an external parent."));
                return false;
            }
            item["parentId"] = remap[parent];
            nodes[i] = item;
        }
    }
    return true;
}

QVariantMap
StudioSession::replacementProposal(const QString& id, const QString& type, const QString& pageId)
{
    const auto fail = [](const QString& reason)
    {
        return QVariantMap{{"valid", false}, {"reason", reason}};
    };
    if (!currentProfile())
        return fail(tr("Reopen Studio before replacing a component."));
    if (!QStringList{"button", "image", "text", "shape", "container"}.contains(type))
        return fail(tr("Choose a compatible component type."));
    QJsonObject document;
    QJsonArray entries, pages;
    int pageIndex = -1, index = -1;
    bool hasChildren = false;
    if (!findReplacementTarget(id, pageId, document, entries, pageIndex, index, hasChildren))
    {
        if (hasChildren)
            return fail(tr("This container has children. Edit its contents or make an independent copy first."));
        return fail(tr("The page or component no longer exists."));
    }
    const auto original = entries[index].toObject();
    if (!validateComponentReplaceable(original, entries, id, type))
        return fail(tr("Choose an unlocked component to replace."));
    QJsonObject replacement;
    QStringList losses;
    if (!buildReplacementPrototype(type, original, replacement, losses))
        return fail(errorString());
    if (!validateReplacementResult(replacement))
        return fail(tr("These properties do not fit the replacement. Keep the original or choose another type."));
    entries[index] = QJsonObject::fromVariantMap(ComponentSchema::component(replacement.toVariantMap(), true));
    pages = document["pages"].toArray();
    QJsonObject page = pageIndex < 0 ? document : pages.at(pageIndex).toObject()["layout"].toObject();
    page["components"] = entries;
    if (!updateReplacementDocument(document, entries, pages, pageIndex, page))
        return fail(tr("Replacement document failed validation."));
    return {{"valid", true}, {"document", document.toVariantMap()}, {"losses", losses}};
}

bool
StudioSession::findReplacementTarget(const QString& id, const QString& pageId, QJsonObject& document,
                                     QJsonArray& entries, int& pageIndex, int& index, bool& hasChildren)
{
    document = QJsonDocument::fromJson(m_draft).object();
    auto pages = document["pages"].toArray();
    pageIndex = -1;
    if (!pageId.isEmpty())
    {
        for (int i = 0; i < pages.size(); ++i)
            if (pages[i].toObject()["id"] == pageId)
            {
                pageIndex = i;
                break;
            }
        if (pageIndex < 0)
            return false;
    }
    auto page = pageIndex < 0 ? document : pages.at(pageIndex).toObject()["layout"].toObject();
    entries = page["components"].toArray();
    index = -1;
    hasChildren = false;
    for (int i = 0; i < entries.size(); ++i)
    {
        const auto entry = entries[i].toObject();
        if (entry["id"] == id)
            index = i;
        if (entry["parentId"] == id)
            hasChildren = true;
    }
    return index >= 0;
}

bool
StudioSession::validateComponentReplaceable(const QJsonObject& original, const QJsonArray& entries, const QString& id,
                                            const QString& type)
{
    Q_UNUSED(type);
    if (ComponentSchema::positionLocked(entries, id))
        return false;
    const auto compType = original["componentType"].toString();
    if (!QStringList{"button", "image", "text", "shape", "container"}.contains(compType))
        return false;
    return !original.contains("family") && !original.contains("appCollection");
}

bool
StudioSession::buildReplacementPrototype(const QString& type, const QJsonObject& original,
                                         QJsonObject& replacement, QStringList& losses)
{
    const auto prototype = componentPrototype(type);
    if (prototype.isEmpty())
        return false;
    replacement = QJsonObject::fromVariantMap(prototype["root"].toMap());
    for (const auto& key : QStringList{"id", "name", "placement", "parentId", "categoryId", "hidden", "locked",
                                       "opacity", "accessibility", "showTitle", "showLogo"})
        if (original.contains(key))
            replacement[key] = original[key];
    auto presentation = replacement["presentation"].toObject();
    const auto oldStyle = original["presentation"].toObject();
    for (auto it = oldStyle.begin(); it != oldStyle.end(); ++it)
    {
        const auto key = it.key();
        if (isSemanticStyleKey(key, it.value().toVariant(), type) ||
            !ComponentSchema::validPresentation({{key, it.value().toVariant()}}, false, type == "container"))
        {
            losses.append(lossMessageForKey(key));
            continue;
        }
        presentation[key] = it.value();
    }
    replacement["presentation"] = presentation;
    if (original["componentType"] == "text" && type != "text" && !losses.contains(tr("Text body")))
        losses.append(tr("Text body"));
    if (original.contains("action") || original.contains("actions"))
    {
        if (type == "button" || type == "image")
        {
            if (original.contains("action"))
                replacement["action"] = original["action"];
            if (original.contains("actions"))
                replacement["actions"] = original["actions"];
        }
        else
            losses.append(tr("Action link"));
    }
    if (original.contains("customLogo"))
    {
        if (type != "shape" && type != "text")
            replacement["customLogo"] = original["customLogo"];
        else
            losses.append(tr("Image artwork"));
    }
    return true;
}

bool
StudioSession::isSemanticStyleKey(const QString& key, const QVariant& value, const QString& type)
{
    return key.startsWith("library") || key.startsWith("guide") ||
           (key.startsWith("body") && type != "text") ||
           (key == "animateArtwork" && type != "image") ||
           (key == "shape" && type != "shape" &&
            (type != "container" || value.toString() == "ellipse" || value.toString() == "divider"));
}

QString
StudioSession::lossMessageForKey(const QString& key)
{
    if (key == "layout")
        return tr("Container layout");
    if (key == "shape")
        return tr("Shape outline");
    if (key == "bodyText")
        return tr("Text body");
    return tr("Presentation: %1").arg(key);
}

bool
StudioSession::validateReplacementResult(const QJsonObject& replacement)
{
    return !ComponentSchema::component(replacement.toVariantMap(), true).isEmpty();
}

bool
StudioSession::updateReplacementDocument(QJsonObject& document, const QJsonArray& entries,
                                         QJsonArray pages, int pageIndex, const QJsonObject& page)
{
    QJsonObject updatedPage = page;
    updatedPage["components"] = entries;
    if (pageIndex < 0)
        document = updatedPage;
    else
    {
        auto entry = pages[pageIndex].toObject();
        entry["layout"] = updatedPage;
        pages[pageIndex] = entry;
        document["pages"] = pages;
    }
    HomeLayout validator(m_live->registry());
    configureWorker(validator);
    return validator.loadDocument(QJsonDocument(document).toJson(), false);
}

// Include the selected item first and every displaced neighbor in one atomic proposal.
QVariantList
StudioSession::collectPlacementChanges(const QJsonArray& entries, const QJsonArray& originalEntries, int selected)
{
    QVariantList changes;
    const auto append = [&](int index)
    {
        const auto item = entries[index].toObject();
        changes.append(QVariantMap{{"id", item.value("id").toString()},
                                   {"placement", item.value("placement").toObject().toVariantMap()},
                                   {"categoryId", item.value("categoryId").toString()},
                                   {"parentId", item.value("parentId").toString()}});
    };
    append(selected);
    for (int i = 0; i < entries.size(); ++i)
    {
        const auto item = entries[i].toObject();
        const auto original = originalEntries[i].toObject();
        if (i != selected && (item.value("placement") != original.value("placement") ||
                              item.value("categoryId") != original.value("categoryId") ||
                              item.value("parentId") != original.value("parentId")))
            append(i);
    }
    return changes;
}

// Scrolling and page-menu conversion belong to the same draft transaction as the addition.
bool
StudioSession::applyAdditionProposalOptions(QJsonObject& document, const QString& type, const QString& pageId,
                                             bool enableScrolling, const QString& parentId, const QString& categoryId)
{
    if (enableScrolling)
    {
        if (!parentId.isEmpty() || !categoryId.isEmpty())
        {
            setError(tr("Choose the page before enabling vertical scrolling."));
            return false;
        }
        auto composition = document["composition"].toObject();
        composition["verticalScrolling"] = true;
        document["composition"] = composition;
    }
    if (type == "page-menu" && !pageId.isEmpty())
    {
        auto composition = document["composition"].toObject();
        composition["pageNavigation"] = "none";
        document["composition"] = composition;
    }
    return true;
}

QVariantMap
StudioSession::placementProposal(const QString& id,
                                 const QVariantMap& placement,
                                 const QString& strategy,
                                 const QString& pageId,
                                 const QVariantMap& options)
{
    if (!m_active)
        return {{"valid", false}, {"reason", QStringLiteral("The edit session has closed.")}};
    if (!validatePlacementProposalInputs(strategy, options))
        return {{"valid", false}, {"reason", m_error}};
    QJsonObject doc = QJsonDocument::fromJson(m_draft).object();
    QString category, parent, sourceCategory, sourceParent;
    bool sections = false;
    QJsonObject composition;
    QJsonArray entries;
    if (!resolvePlacementDestination(doc, entries, id, options, pageId, category, parent,
                                     sourceCategory, sourceParent, sections, composition))
        return {{"valid", false}, {"reason", m_error}};
    if (!pageId.isEmpty() && !findPageLayout(doc, pageId, doc))
        return {{"valid", false}, {"reason", tr("The page no longer exists.")}};
    const auto grid =
        ComponentSchema::componentGrid(entries, composition.toVariantMap(), doc["categories"].toArray(), parent, category);
    const int cols = grid["columns"].toInt(), rows = grid["rows"].toInt();
    if (!ComponentSchema::validPlacement(placement, cols, rows))
        return {{"valid", false}, {"reason", QStringLiteral("The candidate is outside the grid.")}};
    if (placement.value("height").toInt() > 10)
        return {{"valid", false}, {"reason", tr("A component can be at most ten rows tall.")}};
    const QList<int> colliders =
        findPlacementColliders(entries, id, placement, parent, category, sections,
                               grid.value("overlap").toBool() && strategy != "swap");
    int selected = -1;
    QVariantMap origin;
    if (!findPlacementTarget(entries, id, selected, origin))
        return {{"valid", false}, {"reason", QStringLiteral("The component no longer exists.")}};
    if (!validateCollidersNotLocked(entries, colliders))
        return {{"valid", false},
                {"reason", tr("A locked component occupies this region. Unlock it or choose another position.")}};
    QJsonArray workingEntries = entries;
    auto moving = workingEntries[selected].toObject();
    moving["placement"] = QJsonObject::fromVariantMap(placement);
    if (category.isEmpty())
        moving.remove("categoryId");
    else
        moving["categoryId"] = category;
    if (parent.isEmpty())
        moving.remove("parentId");
    else
        moving["parentId"] = parent;
    workingEntries[selected] = moving;
    int attempts = 0;
    bool affectedLimit = false;
    if (!executePlacementStrategy(entries, workingEntries, id, strategy, colliders, placement, origin,
                                  parent, category, sourceParent, sourceCategory, cols, rows, attempts, affectedLimit, sections))
        return {{"valid", false}, {"reason", m_error}};
    QVariantList changes = collectPlacementChanges(workingEntries, entries, selected);
    QJsonObject resultDoc = doc;
    resultDoc["components"] = workingEntries;
    QString error;
    if (!ComponentSchema::hierarchyFits(workingEntries, composition.toVariantMap(), doc["categories"].toArray(), &error))
        return {{"valid", false}, {"reason", error}};
    return {{"valid", true}, {"placements", changes}, {"reason", QString()}};
}

QVariantMap
StudioSession::additionProposal(const QString& type,
                                const QVariantMap& placement,
                                const QString& strategy,
                                const QString& pageId,
                                const QString& categoryId,
                                const QString& parentId,
                                const QVariantMap& configuration,
                                bool enableScrolling)
{
    if (!currentProfile())
        return {{"valid", false}, {"reason", QStringLiteral("The edit session has closed.")}};
    HomeLayout temporary(m_live->registry());
    configureWorker(temporary);
    if (!temporary.loadDocument(m_draft, false))
        return {{"valid", false}, {"reason", temporary.errorString()}};
    StudioSession transaction;
    if (!transaction.bind(&temporary))
        return {{"valid", false}, {"reason", transaction.errorString()}};
    const auto prototype = componentPrototype(type, configuration);
    if (prototype.isEmpty())
        return {{"valid", false}, {"reason", errorString()}};
    const auto identity = prototype.value("rootId").toString();
    auto root = QJsonDocument::fromJson(m_draft).object();
    auto pages = root["pages"].toArray();
    auto document = root;
    int pageIndex = -1;
    if (!pageId.isEmpty())
    {
        for (int i = 0; i < pages.size(); ++i)
            if (pages[i].toObject()["id"] == pageId)
                pageIndex = i;
        if (pageIndex < 0)
            return {{"valid", false}, {"reason", QStringLiteral("The page no longer exists.")}};
        document = pages[pageIndex].toObject()["layout"].toObject();
    }
    if (!applyAdditionProposalOptions(document, type, pageId, enableScrolling, parentId, categoryId))
        return {{"valid", false}, {"reason", errorString()}};
    auto entries = document["components"].toArray();
    for (const auto& value : prototype.value("components").toList())
    {
        auto item = QJsonObject::fromVariantMap(value.toMap());
        if (item["id"] == identity)
        {
            item.remove("placement");
            if (!parentId.isEmpty())
                item["parentId"] = parentId;
            else if (!categoryId.isEmpty())
                item["categoryId"] = categoryId;
        }
        entries.append(item);
    }
    document["components"] = entries;
    const auto store = [&]
    {
        if (pageIndex >= 0)
        {
            auto entry = pages[pageIndex].toObject();
            entry["layout"] = document;
            pages[pageIndex] = entry;
            root["pages"] = pages;
        }
        else
            root = document;
    };
    store();
    transaction.m_draft = QJsonDocument(root).toJson();
    auto proposal = transaction.placementProposal(
        identity, placement, strategy, pageId, QVariantMap{{"parentId", parentId}, {"categoryId", categoryId}});
    if (!proposal.value("valid").toBool())
        return proposal;
    proposal["id"] = identity;
    if (!applyPlacementProposals(entries, proposal["placements"].toList()))
        return proposal;
    document["components"] = entries;
    store();
    QString error;
    if (!HomeLayout::validateStudioDocument(
            QJsonDocument(root).toJson(), &error, m_live ? m_live->m_defaultTheme : QVariantMap{}))
        return {{"valid", false}, {"reason", error}};
    proposal["document"] = root.toVariantMap();
    proposal["commands"] = QVariantList{QVariantMap{{"operation", "addResolved"},
                                                    {"arguments",
                                                     QVariantMap{{"type", type},
                                                                 {"placement", placement},
                                                                 {"strategy", strategy},
                                                                 {"categoryId", categoryId},
                                                                 {"parentId", parentId},
                                                                 {"configuration", configuration},
                                                                 {"enableScrolling", enableScrolling}}},
                                                    {"pageId", pageId}}};
    return proposal;
}

bool
StudioSession::validatePlacementProposalInputs(const QString& strategy, const QVariantMap& options)
{
    if (!QStringList{"cancel", "shift", "swap"}.contains(strategy))
    {
        setError(tr("Choose Shift, Swap or Cancel."));
        return false;
    }
    for (auto it = options.cbegin(); it != options.cend(); ++it)
        if ((it.key() != "categoryId" && it.key() != "parentId") || it.value().metaType().id() != QMetaType::QString)
        {
            setError(tr("Choose a valid destination section or group."));
            return false;
        }
    return true;
}

bool
StudioSession::applyPlacementProposals(QJsonArray& entries, const QVariantList& placements)
{
    for (const auto& change : placements)
    {
        const auto value = change.toMap();
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["id"] == value["id"].toString())
            {
                auto entry = entries[i].toObject();
                entry["placement"] = QJsonObject::fromVariantMap(value["placement"].toMap());
                for (const auto& key : QStringList{"parentId", "categoryId"})
                {
                    const auto destination = value.value(key).toString();
                    if (destination.isEmpty())
                        entry.remove(key);
                    else
                        entry[key] = destination;
                }
                entries[i] = entry;
            }
    }
    return true;
}

bool
StudioSession::resolvePlacementDestination(const QJsonObject& document, QJsonArray& entries, const QString& id,
                                           const QVariantMap& options, const QString& pageId, QString& category,
                                           QString& parent, QString& sourceCategory, QString& sourceParent,
                                           bool& sections, QJsonObject& composition)
{
    QJsonObject doc = document;
    if (!pageId.isEmpty())
    {
        if (!findPageLayout(doc, pageId, doc))
        {
            setError(QStringLiteral("The page no longer exists."));
            return false;
        }
    }
    entries = doc["components"].toArray();
    bool selectedFound = false;
    for (const auto& entry : entries)
        if (entry.toObject()["id"] == id)
        {
            if (ComponentSchema::positionLocked(entries, id))
            {
                setError(tr("Unlock the component and its containing groups before moving or resizing it."));
                return false;
            }
            category = entry.toObject()["categoryId"].toString();
            parent = entry.toObject()["parentId"].toString();
            sourceCategory = category;
            sourceParent = parent;
            selectedFound = true;
        }
    if (!selectedFound)
    {
        setError(QStringLiteral("The component no longer exists."));
        return false;
    }
    composition = doc["composition"].toObject();
    sections = composition.toVariantMap().value("grid").toMap().value("mode") == "sections";
    if (options.contains("parentId"))
        parent = options.value("parentId").toString();
    if (!parent.isEmpty())
    {
        bool categoryCleared = false;
        if (!validatePlacementParent(entries, parent, id, categoryCleared))
            return false;
        if (categoryCleared)
            category.clear();
    }
    else if (!sourceParent.isEmpty())
        category = ComponentSchema::effectiveCategory(entries, id);
    if (options.contains("categoryId"))
    {
        category = options.value("categoryId").toString();
        bool categoryValid = false;
        if (!validatePlacementCategory(doc, category, sections, parent, categoryValid))
            return false;
        if (!categoryValid)
        {
            setError(tr("The destination section no longer exists."));
            return false;
        }
    }
    return true;
}

bool
StudioSession::validatePlacementParent(const QJsonArray& entries, const QString& parent, const QString& id,
                                       bool& categoryCleared)
{
    categoryCleared = false;
    bool found = false;
    QString ancestor = parent;
    for (const auto& entry : entries)
        if (entry.toObject()["id"] == parent)
            found = entry.toObject()["componentType"] == "container";
    if (!found || ComponentSchema::positionLocked(entries, parent))
    {
        setError(tr("Choose an unlocked destination group."));
        return false;
    }
    for (int depth = 0; !ancestor.isEmpty() && depth < 6; ++depth)
    {
        if (ancestor == id)
        {
            setError(tr("A group cannot contain itself or its ancestors."));
            return false;
        }
        QString next;
        for (const auto& entry : entries)
            if (entry.toObject()["id"] == ancestor)
                next = entry.toObject()["parentId"].toString();
        ancestor = next;
    }
    categoryCleared = true;
    return true;
}

bool
StudioSession::validatePlacementCategory(const QJsonObject& document, const QString& category, bool sections,
                                         const QString& parent, bool& categoryValid)
{
    categoryValid = false;
    if (!parent.isEmpty() && !category.isEmpty())
    {
        setError(tr("Group contents inherit their section."));
        return false;
    }
    if (!sections && !category.isEmpty())
    {
        setError(tr("Moving between sections requires a scrolling section layout."));
        return false;
    }
    bool found = category.isEmpty();
    for (const auto& entry : document["categories"].toArray())
        if (entry.toObject()["id"] == category)
        {
            found = true;
            if (entry.toObject()["presentation"].toObject().value("variant") == "library")
            {
                setError(tr("A media shelf manages its own contents. Choose a component section."));
                return false;
            }
        }
    categoryValid = found;
    return true;
}

QList<int>
StudioSession::findPlacementColliders(const QJsonArray& entries, const QString& id, const QVariantMap& placement,
                                      const QString& parent, const QString& category, bool sections,
                                      bool overlapEnabled)
{
    QList<int> colliders;
    for (int i = 0; i < entries.size(); ++i)
    {
        const auto item = entries[i].toObject();
        if (item["id"] == id)
            continue;
        if ((!overlapEnabled) && item["parentId"].toString() == parent &&
            (!sections || !parent.isEmpty() || item["categoryId"].toString() == category) &&
            item.contains("placement") &&
            ComponentSchema::placementsOverlap(item["placement"].toObject().toVariantMap(), placement))
            colliders.append(i);
    }
    return colliders;
}

bool
StudioSession::handlePlacementCancelStrategy(const QList<int>& colliders)
{
    Q_UNUSED(colliders);
    setError(QStringLiteral("Occupied cells: choose Shift, Swap or Cancel."));
    return false;
}

bool
StudioSession::handlePlacementSwapStrategy(const QJsonArray& entries, QJsonArray& entriesOut,
                                           const QList<int>& colliders, const QVariantMap& origin,
                                           const QString& sourceCategory, const QString& sourceParent,
                                           const QString& category)
{
    if (colliders.size() != 1 || origin.isEmpty())
    {
        setError(QStringLiteral("Swap needs one occupant and an existing origin."));
        return false;
    }
    auto other = entries[colliders.first()].toObject();
    auto replacement = other["placement"].toObject().toVariantMap();
    replacement["x"] = origin["x"];
    replacement["y"] = origin["y"];
    if (sourceCategory != category)
    {
        if (sourceCategory.isEmpty())
            other.remove("categoryId");
        else
            other["categoryId"] = sourceCategory;
    }
    if (sourceParent.isEmpty())
        other.remove("parentId");
    else
        other["parentId"] = sourceParent;
    other["placement"] = QJsonObject::fromVariantMap(replacement);
    entriesOut[colliders.first()] = other;
    return true;
}

bool
StudioSession::handlePlacementShiftStrategy(const QJsonArray& entries, QJsonArray& entriesOut, const QString& id,
                                            const QList<int>& colliders, const QVariantMap& placement,
                                            const QVariantMap& origin, const QString& parent,
                                            const QString& category, int cols, int rows, int& attempts,
                                            bool& affectedLimit, bool sections)
{
    Q_UNUSED(origin);
    const auto originalEntries = entries;
    QJsonArray workingEntries = entriesOut;
    const auto stableOrder = [&originalEntries](int a, int b)
    {
        const auto x = originalEntries[a].toObject(), y = originalEntries[b].toObject();
        const auto p = x["placement"].toObject(), q = y["placement"].toObject();
        if (p["y"] != q["y"])
            return p["y"].toInt() < q["y"].toInt();
        if (p["x"] != q["x"])
            return p["x"].toInt() < q["x"].toInt();
        return x["id"].toString() < y["id"].toString();
    };
    QList<int> sortedColliders = colliders;
    std::sort(sortedColliders.begin(), sortedColliders.end(), stableOrder);
    if (sortedColliders.size() >= 64)
    {
        affectedLimit = true;
        setError(QStringLiteral("The shift exceeds 64 affected components."));
        return false;
    }
    QSet<int> fixed;
    fixed.insert(selectedIndex(entries, id));
    int localAttempts = 0;
    std::function<bool(QList<int>)> solve = [&](QList<int> pending) -> bool
    {
        pending.erase(std::remove_if(pending.begin(),
                                     pending.end(),
                                     [&](int i)
                                     {
                                         return fixed.contains(i);
                                     }),
                      pending.end());
        std::sort(pending.begin(), pending.end(), stableOrder);
        pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
        if (pending.isEmpty())
            return true;
        const int index = pending.takeFirst();
        if (fixed.size() >= 64)
        {
            affectedLimit = true;
            return false;
        }
        if (originalEntries[index].toObject().value("locked").toBool())
            return false;
        const auto baseline = workingEntries;
        const auto fixedBefore = fixed;
        auto item = workingEntries[index].toObject();
        auto target = originalEntries[index].toObject()["placement"].toObject().toVariantMap();
        item.remove("placement");
        workingEntries[index] = item;
        const auto vacant = workingEntries;
        int searchRows = rows;
        if (rows == ComponentSchema::MaxScrollableRows)
        {
            int bottom = placement.value("y").toInt() + placement.value("height").toInt();
            for (const auto& value : originalEntries)
            {
                const auto entry = value.toObject();
                if (entry.value("parentId").toString() != parent)
                    continue;
                const auto p = entry["placement"].toObject();
                bottom = qMax(bottom, p["y"].toInt() + p["height"].toInt());
            }
            searchRows = qMin(rows, bottom + 10);
        }
        const auto cells = generateSortedCells(cols, searchRows, placement,
                                                            target["x"].toInt(), target["y"].toInt());
        for (int pass = 0; pass < 2; ++pass)
        {
            for (const auto& cell : cells)
            {
                if (++localAttempts > 4096)
                {
                    workingEntries = baseline;
                    fixed = fixedBefore;
                    return false;
                }
                target["x"] = cell.first;
                target["y"] = cell.second;
                if (!ComponentSchema::validPlacement(target, cols, rows))
                    continue;
                const auto displaced = findCollidersForTarget(workingEntries, index, target, parent, category,
                                                               sections, fixed);
                bool blocked = false;
                for (int j : displaced)
                    if (fixed.contains(j) || workingEntries[j].toObject().value("locked").toBool())
                    {
                        blocked = true;
                        break;
                    }
                if (shouldSkipShiftCell(blocked, pass, displaced))
                    continue;
                auto placed = item;
                placed["placement"] = QJsonObject::fromVariantMap(target);
                workingEntries[index] = placed;
                fixed.insert(index);
                auto next = pending;
                next.append(displaced);
                if (solve(next))
                    return true;
                workingEntries = vacant;
                fixed = fixedBefore;
                if (localAttempts > 4096)
                {
                    workingEntries = baseline;
                    return false;
                }
            }
        }
        workingEntries = baseline;
        fixed = fixedBefore;
        return false;
    };
    if (!solve(sortedColliders))
    {
        attempts = localAttempts;
        if (attempts > 4096)
            setError(tr("Shift reached its 4096-proposal search limit. Choose another cell or Swap."));
        else if (affectedLimit)
            setError(tr("The shift exceeds 64 affected components."));
        else
            setError(tr("No complete shift fits. Cancel or choose another cell."));
        return false;
    }
    attempts = localAttempts;
    entriesOut = workingEntries;
    return true;
}

int
StudioSession::selectedIndex(const QJsonArray& entries, const QString& id)
{
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].toObject()["id"] == id)
            return i;
    return -1;
}

bool
StudioSession::findPlacementTarget(const QJsonArray& entries, const QString& id, int& selectedIndex, QVariantMap& origin)
{
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].toObject()["id"] == id)
        {
            selectedIndex = i;
            origin = entries[i].toObject()["placement"].toObject().toVariantMap();
            return true;
        }
    return false;
}

bool
StudioSession::findPageLayout(const QJsonObject& document, const QString& pageId, QJsonObject& layout)
{
    for (const auto& entry : document["pages"].toArray())
        if (entry.toObject()["id"] == pageId)
        {
            layout = entry.toObject()["layout"].toObject();
            return true;
        }
    return false;
}

bool
StudioSession::validateCollidersNotLocked(const QJsonArray& entries, const QList<int>& colliders)
{
    for (const int index : colliders)
        if (entries[index].toObject().value("locked").toBool())
            return false;
    return true;
}

QList<int>
StudioSession::findCollidersForTarget(const QJsonArray& entries, int index, const QVariantMap& target,
                                      const QString& parent, const QString& category, bool sections,
                                      const QSet<int>& fixed)
{
    Q_UNUSED(fixed);
    QList<int> colliders;
    for (int j = 0; j < entries.size(); ++j)
    {
        const auto other = entries[j].toObject();
        if (j == index || other.value("parentId").toString() != parent ||
            (sections && parent.isEmpty() && other.value("categoryId").toString() != category) ||
            !other.contains("placement") ||
            !ComponentSchema::placementsOverlap(other["placement"].toObject().toVariantMap(), target))
            continue;
        colliders.append(j);
    }
    return colliders;
}

QList<QPair<int, int>>
StudioSession::generateSortedCells(int cols, int rows, const QVariantMap& placement, int originalX, int originalY)
{
    QList<QPair<int, int>> cells;
    Q_UNUSED(placement);
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
            cells.append({col, row});
    std::stable_sort(cells.begin(),
                     cells.end(),
                     [=](const auto& a, const auto& b)
                     {
                         return std::abs(a.first - originalX) + std::abs(a.second - originalY) <
                                std::abs(b.first - originalX) + std::abs(b.second - originalY);
                     });
    return cells;
}

bool
StudioSession::shouldSkipShiftCell(bool blocked, int pass, const QList<int>& displaced)
{
    return blocked || (pass == 0 && !displaced.isEmpty()) || (pass == 1 && displaced.isEmpty());
}

bool
StudioSession::executePlacementStrategy(const QJsonArray& entries, QJsonArray& entriesOut, const QString& id,
                                        const QString& strategy, const QList<int>& colliders,
                                        const QVariantMap& placement, const QVariantMap& origin,
                                        const QString& parent, const QString& category,
                                        const QString& sourceParent, const QString& sourceCategory, int cols, int rows,
                                        int& attempts, bool& affectedLimit, bool sections)
{
    if (!colliders.isEmpty() && strategy == "cancel")
    {
        setError(QStringLiteral("Occupied cells: choose Shift, Swap or Cancel."));
        return false;
    }
    if (!colliders.isEmpty() && strategy == "swap")
    {
        if (!handlePlacementSwapStrategy(entries, entriesOut, colliders, origin,
                                         sourceCategory, sourceParent, category))
        {
            setError(QStringLiteral("Swap needs one occupant and an existing origin."));
            return false;
        }
        return true;
    }
    if (!colliders.isEmpty() && strategy == "shift")
    {
        if (!handlePlacementShiftStrategy(entries, entriesOut, id, colliders, placement, origin,
                                          parent, category, cols, rows, attempts, affectedLimit, sections))
        {
            if (attempts > 4096)
                setError(tr("Shift reached its 4096-proposal search limit. Choose another cell or Swap."));
            else if (affectedLimit)
                setError(tr("The shift exceeds 64 affected components."));
            else
                setError(tr("No complete shift fits. Cancel or choose another cell."));
            return false;
        }
        return true;
    }
    if (!colliders.isEmpty())
    {
        setError(QStringLiteral("Choose a collision strategy."));
        return false;
    }
    return true;
}
