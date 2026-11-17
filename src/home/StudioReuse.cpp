#include <QJsonArray>
#include <QSet>
#include "ComponentSchema.h"
#include "DesignSystem.h"
#include "HomeLayout.h"
#include "StudioSession.h"

namespace
{
QJsonObject
page(const QJsonObject& document, const QString& id)
{
    if (id.isEmpty())
        return document;
    for (const auto& entry : document["pages"].toArray())
        if (entry.toObject()["id"] == id)
            return entry.toObject()["layout"].toObject();
    return {};
}
void
store(QJsonObject& document, const QString& id, const QJsonObject& layout)
{
    if (id.isEmpty())
    {
        document = layout;
        return;
    }
    auto pages = document["pages"].toArray();
    for (qsizetype i = 0; i < pages.size(); ++i)
    {
        auto entry = pages[i].toObject();
        if (entry["id"] != id)
            continue;
        entry["layout"] = layout;
        pages[i] = entry;
        break;
    }
    document["pages"] = pages;
}
QJsonArray
subtree(const QJsonArray& nodes, const QString& id)
{
    QJsonArray result;
    QSet<QString> selected;
    for (const auto& node : nodes)
        if (node.toObject()["id"] == id)
        {
            result.append(node);
            selected.insert(id);
            break;
        }
    for (int depth = 0; depth < ComponentSchema::MaxComponentNestingDepth; ++depth)
        for (const auto& value : nodes)
        {
            const auto node = value.toObject();
            const auto nodeId = node["id"].toString();
            if (!selected.contains(nodeId) && selected.contains(node["parentId"].toString()))
            {
                result.append(node);
                selected.insert(nodeId);
            }
        }
    return result;
}
QJsonArray
prototype(QJsonArray nodes)
{
    for (qsizetype i = 0; i < nodes.size(); ++i)
    {
        auto node = nodes[i].toObject();
        node.remove("linkedDesign");
        node.remove("designNode");
        if (i == 0)
        {
            node.remove("parentId");
            node.remove("categoryId");
        }
        nodes[i] = node;
    }
    return nodes;
}
QString
fresh(const QString& base, const QJsonArray& nodes)
{
    QSet<QString> ids;
    for (const auto& value : nodes)
        ids.insert(value.toObject()["id"].toString());
    for (int i = 1; i <= 65536; ++i)
    {
        const auto id = QString("%1-%2").arg(base.left(50)).arg(i);
        if (isUniqueId(id, ids))
            return id;
    }
    return {};
}

bool
isUniqueId(const QString& id, const QSet<QString>& ids)
{
    return !ids.contains(id);
}
QJsonArray
instantiate(const QJsonArray& definition, QJsonArray existing, const QJsonObject& destination, const QString& link)
{
    QHash<QString, QString> ids;
    for (qsizetype i = 0; i < definition.size(); ++i)
    {
        const auto node = definition[i].toObject();
        const auto id = i == 0 && destination.contains("id") ? destination["id"].toString() : fresh("saved", existing);
        ids[node["id"].toString()] = id;
        existing.append(QJsonObject{{"id", id}});
    }
    QJsonArray result;
    for (qsizetype i = 0; i < definition.size(); ++i)
    {
        auto node = definition[i].toObject();
        const auto original = node["id"].toString();
        node["id"] = ids[original];
        if (i == 0)
        {
            for (const auto& key : {"placement", "categoryId", "parentId"})
            {
                if (destination.contains(key))
                    node[key] = destination[key];
                else if (QString(key) != "placement")
                    node.remove(key);
            }
        }
        else
            node["parentId"] = ids[node["parentId"].toString()];
        // Only parent links are part of the current component schema. Page/app actions retain their owners.
        if (!link.isEmpty())
        {
            node["linkedDesign"] = link;
            node["designNode"] = original;
        }
        result.append(node);
    }
    return result;
}
} // namespace

QVariantList
StudioSession::reusableChoices(const QString& kind) const
{
    if (!currentProfile() || (kind != "styles" && kind != "components"))
        return {};
    return QJsonDocument::fromJson(m_draft)
        .object()["composition"]
        .toObject()["designLibrary"]
        .toObject()[kind]
        .toArray()
        .toVariantList();
}

QVariantMap
StudioSession::previewReuse(const QString& operation, const QVariantMap& a, const QString& pageId)
{
    cancelPreview();
    if (!editingPermitted() || !currentProfile())
        return {};
    auto document = QJsonDocument::fromJson(m_draft).object();
    auto layout = page(document, pageId);
    auto nodes = layout["components"].toArray();
    const auto selected = subtree(nodes, a.value("id").toString());
    QJsonObject library = document["composition"].toObject()["designLibrary"].toObject();
    if (library.isEmpty())
        library = {{"schemaVersion", 1}, {"styles", QJsonArray{}}, {"components", QJsonArray{}}};
    const auto allNodes = document["components"].toArray();
    for (const auto& entry : document["pages"].toArray())
        for (const auto& node : entry.toObject()["layout"].toObject()["components"].toArray())
            allNodes.append(node);
    QVariantList impact;
    const auto name = a.value("name").toString().trimmed();
    const auto libraryId = a.value("libraryId").toString();
    using ReuseOp = bool (StudioSession::*)(const QVariantMap&, QJsonObject, QJsonObject,
                                            const QJsonArray&, QVariantList&, const QString&);
    static const QHash<QString, ReuseOp> reuseOps{
        {"responsive", &StudioSession::handleReuseResponsive},
        {"saveStyle", &StudioSession::handleReuseSave},
        {"saveComponent", &StudioSession::handleReuseSave},
        {"editStyle", &StudioSession::handleReuseEditStyle},
        {"insert", &StudioSession::handleReuseInsert},
        {"editAll", &StudioSession::handleReuseEditAll},
    };
    auto it = reuseOps.constFind(operation);
    if (it != reuseOps.cend())
    {
        if (!(this->*it)(a, document, layout, selected, impact, pageId))
            return {};
    }
    else if (!handleReuseAppearanceEdit(a, document, layout, selected, library, impact, pageId))
        return {};
    composition = document["composition"].toObject();
    composition["designLibrary"] = library;
    document["composition"] = composition;
    if (!DesignSystem::valid(library.toVariantMap()))
    {
        setError(tr("The saved definition is invalid or cyclic. Nested linked symbols are not supported."));
        return {};
    }
    if (!proposeDocument(QJsonDocument(document).toJson()))
        return {};
    return {{"valid", true}, {"changes", impact}};
}

bool
StudioSession::handleReuseResponsive(const QVariantMap& a, QJsonObject document, QJsonObject layout,
                                     const QJsonArray& selected, QVariantList& impact, const QString& pageId)
{
    Q_UNUSED(selected);
    Q_UNUSED(impact);
    Q_UNUSED(pageId);
    auto categories = layout["categories"].toArray();
    bool found = false;
    for (qsizetype i = 0; i < categories.size(); ++i)
    {
        auto category = categories[i].toObject();
        if (category["id"].toString() != a.value("categoryId").toString())
            continue;
        auto style = category["presentation"].toObject();
        if (style.value("variant") == "library" ||
            layout["composition"].toObject()["grid"].toObject()["mode"] == "sections")
        {
            setError(tr("Responsive rules require a flowing component section, not a Kodi library or authored "
                        "placement grid."));
            return false;
        }
        for (const auto& value : layout["components"].toArray())
            if (value.toObject()["categoryId"] == category["id"] && value.toObject().contains("placement"))
            {
                setError(tr("Convert this section to flow before choosing responsive rules. Authored placement "
                            "is never rewritten."));
                return false;
            }
        const auto rule = a.value("rule").toMap();
        if (!DesignSystem::validResponsive(rule))
        {
            setError(tr("Choose bounded minimum sizes and aspect ratio."));
            return false;
        }
        style["responsive"] = QJsonObject::fromVariantMap(rule);
        category["presentation"] = style;
        categories[i] = category;
        found = true;
    }
    if (!found)
    {
        setError(tr("Select a named flow section first."));
        return false;
    }
    layout["categories"] = categories;
    store(document, pageId, layout);
    impact.append(QVariantMap{{"page", pageId},
                              {"name", a.value("categoryId")},
                              {"change", tr("Responsive flow; authored placement unchanged")}});
    return true;
}

bool
StudioSession::handleReuseSave(const QVariantMap& a, QJsonObject document, QJsonObject layout,
                               const QJsonArray& selected, QVariantList& impact, const QString& pageId)
{
    Q_UNUSED(document);
    Q_UNUSED(layout);
    Q_UNUSED(pageId);
    const auto name = a.value("name").toString().trimmed();
    const QString kind = a.value("operation").toString() == "saveStyle" ? "styles" : "components";
    if (selected.isEmpty() || name.isEmpty() || name.size() > 80)
    {
        setError(tr("Select a component and enter a name of up to 80 characters."));
        return false;
    }
    QJsonObject library = document["composition"].toObject()["designLibrary"].toObject();
    auto entries = library[kind].toArray();
    if (entries.size() >= 64)
    {
        setError(tr("This profile already has 64 saved items of this kind."));
        return false;
    }
    const auto id = fresh(kind, entries);
    QJsonObject entry{{"id", id}, {"name", name}};
    if (kind == "styles")
    {
        auto style = QJsonObject::fromVariantMap(DesignSystem::styleProperties(DesignSystem::presentation(
            selected[0].toObject()["presentation"].toObject().toVariantMap(), library.toVariantMap())));
        if (!ComponentSchema::validPresentation(style.toVariantMap()))
        {
            setError(tr(
                "This section-specific appearance cannot be used as a component style. Copy appearance instead."));
            return false;
        }
        entry["presentation"] = style;
    }
    else
        entry["nodes"] = prototype(selected);
    entries.append(entry);
    library[kind] = entries;
    document["composition"].toObject()["designLibrary"] = library;
    impact.append(QVariantMap{{"page", pageId}, {"name", name},
                              {"change", tr("Save independent definition")}});
    return true;
}

bool
StudioSession::handleReuseEditStyle(const QVariantMap& a, QJsonObject document, QJsonObject layout,
                                    const QJsonArray& selected, QVariantList& impact, const QString& pageId)
{
    Q_UNUSED(selected);
    Q_UNUSED(layout);
    Q_UNUSED(pageId);
    QJsonObject library = document["composition"].toObject()["designLibrary"].toObject();
    auto entries = library["styles"].toArray();
    bool found = false;
    const auto presentation = DesignSystem::styleProperties(a.value("presentation").toMap());
    if (presentation.contains("styleId") || !ComponentSchema::validPresentation(presentation))
    {
        setError(tr("Choose compatible shared appearance values."));
        return false;
    }
    for (qsizetype i = 0; i < entries.size(); ++i)
    {
        auto entry = entries[i].toObject();
        if (entry["id"] != a.value("libraryId").toString())
            continue;
        entry["presentation"] = QJsonObject::fromVariantMap(presentation);
        entries[i] = entry;
        found = true;
    }
    if (!found)
    {
        setError(tr("The shared style no longer exists."));
        return false;
    }
    library["styles"] = entries;
    document["composition"].toObject()["designLibrary"] = library;
    QStringList pages{QString()};
    for (const auto& entry : document["pages"].toArray())
        pages.append(entry.toObject()["id"].toString());
    for (const auto& id : pages)
        for (const auto& value : page(document, id)["components"].toArray())
        {
            const auto node = value.toObject();
            if (node["presentation"].toObject()["styleId"] == a.value("libraryId").toString())
                impact.append(QVariantMap{{"page", id},
                                          {"id", node["id"].toString()},
                                          {"name", node["name"].toString()},
                                          {"change", tr("Shared style; explicit overrides retained")}});
        }
    if (impact.isEmpty())
        impact.append(QVariantMap{{"name", a.value("libraryId").toString()}, {"change", tr("Update unused style")}});
    return true;
}

bool
StudioSession::handleReuseInsert(const QVariantMap& a, QJsonObject document, QJsonObject layout,
                                 const QJsonArray& selected, QVariantList& impact, const QString& pageId)
{
    Q_UNUSED(selected);
    Q_UNUSED(pageId);
    QJsonObject library = document["composition"].toObject()["designLibrary"].toObject();
    QJsonObject definition;
    for (const auto& value : library["components"].toArray())
        if (value.toObject()["id"] == a.value("libraryId").toString())
            definition = value.toObject();
    if (definition.isEmpty())
    {
        setError(tr("Choose an existing saved component."));
        return false;
    }
    QJsonObject destination;
    for (const auto& key : {"placement", "parentId", "categoryId"})
        if (a.contains(key))
            destination[key] = QJsonValue::fromVariant(a[key]);
    const auto allNodes = document["components"].toArray();
    for (const auto& entry : document["pages"].toArray())
        for (const auto& node : entry.toObject()["layout"].toObject()["components"].toArray())
            allNodes.append(node);
    if (!destination.contains("placement") && (layout["composition"].toObject()["grid"].isObject() ||
                                               definition["nodes"].toArray()[0].toObject().contains("placement")))
    {
        const auto allNodes = document["components"].toArray();
        for (const auto& entry : document["pages"].toArray())
            for (const auto& node : entry.toObject()["layout"].toObject()["components"].toArray())
                allNodes.append(node);
        if (!findFreePlacement(destination, layout, allNodes, definition))
        {
            setError(tr("No free placement fits this saved component. Move items or choose another page."));
            return false;
        }
    }
    const auto additions = instantiate(
        definition["nodes"].toArray(), allNodes, destination,
        a.value("linked").toBool() ? a.value("libraryId").toString() : QString());
    for (const auto& node : additions)
        layout["components"].toArray().append(node);
    store(document, pageId, layout);
    impact.append(QVariantMap{
        {"page", pageId},
        {"name", definition["name"].toString()},
        {"change", a.value("linked").toBool() ? tr("Insert linked instance") : tr("Insert independent copy")}});
    return true;
}

bool
StudioSession::handleReuseEditAll(const QVariantMap& a, QJsonObject document, QJsonObject layout,
                                  const QJsonArray& selected, QVariantList& impact, const QString& pageId)
{
    Q_UNUSED(layout);
    Q_UNUSED(pageId);
    QJsonObject library = document["composition"].toObject()["designLibrary"].toObject();
    if (selected.isEmpty())
    {
        setError(tr("Select the edited linked instance."));
        return false;
    }
    const auto link = selected[0].toObject()["linkedDesign"].toString();
    if (link.isEmpty())
    {
        setError(tr("This is an independent component. Insert a linked instance first."));
        return false;
    }
    const auto definition = prototype(selected);
    auto entries = library["components"].toArray();
    bool found = false;
    for (qsizetype i = 0; i < entries.size(); ++i)
    {
        auto entry = entries[i].toObject();
        if (entry["id"] != link)
            continue;
        entry["nodes"] = definition;
        entries[i] = entry;
        found = true;
        break;
    }
    if (!found)
    {
        setError(tr("The linked definition is missing. Detach this instance to keep editing it."));
        return false;
    }
    library["components"] = entries;
    document["composition"].toObject()["designLibrary"] = library;
    QStringList pages{QString()};
    for (const auto& entry : document["pages"].toArray())
        pages.append(entry.toObject()["id"].toString());
    const auto allNodes = document["components"].toArray();
    for (const auto& entry : document["pages"].toArray())
        for (const auto& node : entry.toObject()["layout"].toObject()["components"].toArray())
            allNodes.append(node);
    for (const auto& id : pages)
    {
        auto target = page(document, id);
        auto values = target["components"].toArray();
        QJsonArray replaced;
        QSet<QString> descendants;
        for (const auto& value : values)
        {
            const auto node = value.toObject();
            if (node["linkedDesign"] != link)
                continue;
            const auto parent = node["parentId"].toString();
            bool child = false;
            for (const auto& possible : values)
                if (possible.toObject()["id"] == parent && possible.toObject()["linkedDesign"] == link)
                    child = true;
            if (child)
                continue;
            const auto old = subtree(values, node["id"].toString());
            for (const auto& item : old)
                descendants.insert(item.toObject()["id"].toString());
            const auto replacement = instantiate(definition, allNodes, node, link);
            for (const auto& item : replacement)
            {
                replaced.append(item);
                allNodes.append(item);
            }
            impact.append(QVariantMap{{"page", id},
                                      {"id", node["id"].toString()},
                                      {"name", node["name"].toString()},
                                      {"change", tr("Edit linked instance")}});
        }
        QJsonArray result;
        for (const auto& value : values)
            if (!descendants.contains(value.toObject()["id"].toString()))
                result.append(value);
        for (const auto& value : replaced)
            result.append(value);
        target["components"] = result;
        store(document, id, target);
    }
    return true;
}

bool
StudioSession::handleReuseAppearanceEdit(const QVariantMap& a, QJsonObject document, QJsonObject layout,
                                         const QJsonArray& selected, QVariantList& impact, const QString& pageId)
{
    Q_UNUSED(document);
    Q_UNUSED(impact);
    Q_UNUSED(pageId);
    QJsonObject library = document["composition"].toObject()["designLibrary"].toObject();
    const auto id = selected[0].toObject()["id"].toString();
    bool changed = false;
    QJsonObject styleDefinition;
    for (const auto& value : library["styles"].toArray())
        if (value.toObject()["id"] == a.value("libraryId").toString())
            styleDefinition = value.toObject();
    auto sourceLayout = page(document, a.value("sourcePage").toString());
    const auto source = subtree(sourceLayout["components"].toArray(), a.value("sourceId").toString());
    const auto operation = a.value("operation").toString();
    for (qsizetype i = 0; i < layout["components"].toArray().size(); ++i)
    {
        auto node = layout["components"].toArray()[i].toObject();
        if (operation == "detach")
        {
            bool member = false;
            for (const auto& value : selected)
                if (value.toObject()["id"] == node["id"])
                    member = true;
            if (!member)
                continue;
            node.remove("linkedDesign");
            node.remove("designNode");
        }
        else
        {
            if (node["id"] != id)
                continue;
            auto appearance = node["presentation"].toObject();
            if (operation == "useStyle")
            {
                if (styleDefinition.isEmpty())
                {
                    setError(tr("Choose an existing shared style."));
                    return false;
                }
                const auto visual = DesignSystem::styleProperties(appearance.toVariantMap());
                for (auto it = visual.cbegin(); it != visual.cend(); ++it)
                    appearance.remove(it.key());
                appearance["styleId"] = a.value("libraryId").toString();
            }
            else if (operation == "resetOverrides")
            {
                if (!appearance.contains("styleId"))
                {
                    setError(tr("This component has no shared style."));
                    return false;
                }
                const auto visual = DesignSystem::styleProperties(appearance.toVariantMap());
                for (auto it = visual.cbegin(); it != visual.cend(); ++it)
                    appearance.remove(it.key());
            }
            else if (operation == "detachStyle")
            {
                appearance = QJsonObject::fromVariantMap(
                    DesignSystem::presentation(appearance.toVariantMap(), library.toVariantMap()));
            }
            else if (operation == "pasteAppearance")
            {
                if (source.isEmpty())
                {
                    setError(tr("The copied appearance is no longer available."));
                    return false;
                }
                const auto visual = DesignSystem::styleProperties(DesignSystem::presentation(
                    source[0].toObject()["presentation"].toObject().toVariantMap(), library.toVariantMap()));
                const auto previous = DesignSystem::styleProperties(appearance.toVariantMap());
                for (auto it = previous.cbegin(); it != previous.cend(); ++it)
                    appearance.remove(it.key());
                appearance.remove("styleId");
                for (auto it = visual.cbegin(); it != visual.cend(); ++it)
                    appearance[it.key()] = QJsonValue::fromVariant(it.value());
            }
            else
            {
                setError(tr("Unknown reusable-design operation."));
                return false;
            }
            if (!ComponentSchema::validPresentation(appearance.toVariantMap(),
                                                    node["componentType"] == "library",
                                                    node["componentType"] == "container"))
            {
                setError(
                    tr("This appearance contains properties incompatible with the destination. No "
                       "properties were discarded."));
                return false;
            }
            node["presentation"] = appearance;
        }
        layout["components"].toArray()[i] = node;
        changed = true;
    }
    if (!changed)
    {
        setError(tr("The selected component no longer exists."));
        return false;
    }
    store(document, pageId, layout);
    impact.append(QVariantMap{{"page", pageId}, {"id", id},
                              {"name", selected[0].toObject()["name"].toString()}, {"change", operation}});
    return true;
}

bool
StudioSession::findFreePlacement(QJsonObject& destination, QJsonObject layout, const QJsonArray& allNodes,
                                 const QJsonObject& definition)
{
    const auto grid = ComponentSchema::componentGrid(layout["components"].toArray(),
                                                     layout["composition"].toObject().toVariantMap(),
                                                     layout["categories"].toArray(),
                                                     destination.value("parentId").toString(),
                                                     destination.value("categoryId").toString());
    auto placement = definition["nodes"].toArray()[0].toObject()["placement"].toObject();
    const int width = placement["width"].toInt(1), height = placement["height"].toInt(1);
    for (int y = 0; y < grid.value("rows", 5).toInt(); ++y)
    {
        for (int x = 0; x < grid.value("columns", 8).toInt(); ++x)
        {
            QVariantMap proposed{{"x", x}, {"y", y}, {"width", width}, {"height", height}};
            if (!ComponentSchema::validPlacement(
                    proposed, grid.value("columns", 8).toInt(), grid.value("rows", 5).toInt()))
                continue;
            bool collision = false;
            for (const auto& value : layout["components"].toArray())
            {
                const auto node = value.toObject();
                if (node["parentId"].toString() == destination.value("parentId").toString() &&
                    node["categoryId"].toString() == destination.value("categoryId").toString() &&
                    ComponentSchema::placementsOverlap(proposed, node["placement"].toObject().toVariantMap()))
                    collision = true;
            }
            if (!collision)
            {
                destination["placement"] = QJsonObject::fromVariantMap(proposed);
                return true;
            }
        }
    }
    return false;
}

