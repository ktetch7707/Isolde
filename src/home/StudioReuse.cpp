#include "StudioSession.h"
#include "HomeLayout.h"
#include "ComponentSchema.h"
#include "DesignSystem.h"
#include <QJsonArray>
#include <QSet>

namespace {
QJsonObject page(const QJsonObject &document, const QString &id)
{
    if (id.isEmpty()) return document;
    for (const auto &entry : document["pages"].toArray()) if (entry.toObject()["id"] == id) return entry.toObject()["layout"].toObject();
    return {};
}
void store(QJsonObject &document, const QString &id, const QJsonObject &layout)
{
    if (id.isEmpty()) { document = layout; return; }
    auto pages = document["pages"].toArray();
    for (qsizetype i = 0; i < pages.size(); ++i) {
        auto entry = pages[i].toObject(); if (entry["id"] != id) continue;
        entry["layout"] = layout; pages[i] = entry; break;
    }
    document["pages"] = pages;
}
QJsonArray subtree(const QJsonArray &nodes, const QString &id)
{
    QJsonArray result; QSet<QString> selected;
    for (const auto &node : nodes) if (node.toObject()["id"] == id) { result.append(node); selected.insert(id); break; }
    for (int depth = 0; depth < ComponentSchema::MaxComponentNestingDepth; ++depth)
        for (const auto &value : nodes) {
            const auto node = value.toObject(); const auto nodeId = node["id"].toString();
            if (!selected.contains(nodeId) && selected.contains(node["parentId"].toString())) { result.append(node); selected.insert(nodeId); }
        }
    return result;
}
QJsonArray prototype(QJsonArray nodes)
{
    for (qsizetype i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i].toObject(); node.remove("linkedDesign"); node.remove("designNode");
        if (i == 0) { node.remove("parentId"); node.remove("categoryId"); }
        nodes[i] = node;
    }
    return nodes;
}
QString fresh(const QString &base, const QJsonArray &nodes)
{
    QSet<QString> ids; for (const auto &value : nodes) ids.insert(value.toObject()["id"].toString());
    for (int i = 1; i <= 65536; ++i) { const auto id = QString("%1-%2").arg(base.left(50)).arg(i); if (!ids.contains(id)) return id; }
    return {};
}
QJsonArray instantiate(const QJsonArray &definition, QJsonArray existing, const QJsonObject &destination, const QString &link)
{
    QHash<QString, QString> ids;
    for (qsizetype i = 0; i < definition.size(); ++i) {
        const auto node = definition[i].toObject();
        const auto id = i == 0 && destination.contains("id") ? destination["id"].toString() : fresh("saved", existing);
        ids[node["id"].toString()] = id; existing.append(QJsonObject{{"id", id}});
    }
    QJsonArray result;
    for (qsizetype i = 0; i < definition.size(); ++i) {
        auto node = definition[i].toObject(); const auto original = node["id"].toString(); node["id"] = ids[original];
        if (i == 0) {
            for (const auto &key : {"placement", "categoryId", "parentId"}) {
                if (destination.contains(key)) node[key] = destination[key]; else if (QString(key) != "placement") node.remove(key);
            }
        } else node["parentId"] = ids[node["parentId"].toString()];
        // Only parent links are part of the current component schema. Page/app actions retain their owners.
        if (!link.isEmpty()) { node["linkedDesign"] = link; node["designNode"] = original; }
        result.append(node);
    }
    return result;
}
}

QVariantList StudioSession::reusableChoices(const QString &kind) const
{
    if (!currentProfile() || (kind != "styles" && kind != "components")) return {};
    return QJsonDocument::fromJson(m_draft).object()["composition"].toObject()["designLibrary"].toObject()[kind].toArray().toVariantList();
}

QVariantMap StudioSession::previewReuse(const QString &operation, const QVariantMap &a, const QString &pageId)
{
    cancelPreview();
    if (!editingPermitted() || !currentProfile()) return {};
    auto document = QJsonDocument::fromJson(m_draft).object(); auto layout = page(document, pageId);
    auto nodes = layout["components"].toArray(); const auto selected = subtree(nodes, a.value("id").toString());
    const auto fail = [&](const QString &message) -> QVariantMap { setError(message); return {}; };
    if (layout.isEmpty()) return fail(tr("Choose an existing page."));
    auto composition = document["composition"].toObject();
    auto library = composition["designLibrary"].toObject();
    if (library.isEmpty()) library = {{"schemaVersion", 1}, {"styles", QJsonArray{}}, {"components", QJsonArray{}}};
    QJsonArray allNodes=document["components"].toArray();
    for(const auto &entry:document["pages"].toArray())for(const auto &node:entry.toObject()["layout"].toObject()["components"].toArray())allNodes.append(node);
    QVariantList impact;
    const auto name = a.value("name").toString().trimmed();
    const auto libraryId = a.value("libraryId").toString();
    if (operation == "responsive") {
        auto categories=layout["categories"].toArray();bool found=false;
        for(qsizetype i=0;i<categories.size();++i) {
            auto category=categories[i].toObject();if(category["id"].toString()!=a.value("categoryId").toString())continue;
            auto style=category["presentation"].toObject();
            if(style.value("variant")=="library" || layout["composition"].toObject()["grid"].toObject()["mode"]=="sections")return fail(tr("Responsive rules require a flowing component section, not a Kodi library or authored placement grid."));
            for(const auto &value:nodes)if(value.toObject()["categoryId"]==category["id"] && value.toObject().contains("placement"))return fail(tr("Convert this section to flow before choosing responsive rules. Authored placement is never rewritten."));
            const auto rule=a.value("rule").toMap();
            if(!DesignSystem::validResponsive(rule))return fail(tr("Choose bounded minimum sizes and aspect ratio."));
            style["responsive"]=QJsonObject::fromVariantMap(rule);category["presentation"]=style;categories[i]=category;found=true;
        }
        if(!found)return fail(tr("Select a named flow section first."));
        layout["categories"]=categories;store(document,pageId,layout);
        impact.append(QVariantMap{{"page",pageId},{"name",a.value("categoryId")},{"change",tr("Responsive flow; authored placement unchanged")}});
    } else if (operation == "saveStyle" || operation == "saveComponent") {
        if (selected.isEmpty() || name.isEmpty() || name.size() > 80) return fail(tr("Select a component and enter a name of up to 80 characters."));
        const QString kind = operation == "saveStyle" ? "styles" : "components";
        auto entries = library[kind].toArray();
        if (entries.size() >= 64) return fail(tr("This profile already has 64 saved items of this kind."));
        const auto id = fresh(kind, entries); QJsonObject entry{{"id", id}, {"name", name}};
        if (kind == "styles") {
            auto style = QJsonObject::fromVariantMap(DesignSystem::styleProperties(DesignSystem::presentation(selected[0].toObject()["presentation"].toObject().toVariantMap(),library.toVariantMap())));
            if (!ComponentSchema::validPresentation(style.toVariantMap())) return fail(tr("This section-specific appearance cannot be used as a component style. Copy appearance instead."));
            entry["presentation"] = style;
        } else entry["nodes"] = prototype(selected);
        entries.append(entry); library[kind] = entries;
        impact.append(QVariantMap{{"page", pageId}, {"name", name}, {"change", tr("Save independent definition")}});
    } else if (operation == "editStyle") {
        auto entries = library["styles"].toArray(); bool found = false;
        const auto presentation = DesignSystem::styleProperties(a.value("presentation").toMap());
        if (presentation.contains("styleId") || !ComponentSchema::validPresentation(presentation)) return fail(tr("Choose compatible shared appearance values."));
        for (qsizetype i = 0; i < entries.size(); ++i) { auto entry = entries[i].toObject(); if (entry["id"] != libraryId) continue; entry["presentation"] = QJsonObject::fromVariantMap(presentation); entries[i] = entry; found = true; }
        if (!found) return fail(tr("The shared style no longer exists."));
        library["styles"] = entries;
        QStringList pages{QString()}; for (const auto &entry : document["pages"].toArray()) pages.append(entry.toObject()["id"].toString());
        for (const auto &id : pages) for (const auto &value : page(document,id)["components"].toArray()) {
            const auto node=value.toObject();
            if(node["presentation"].toObject()["styleId"]==libraryId) impact.append(QVariantMap{{"page",id},{"id",node["id"].toString()},{"name",node["name"].toString()},{"change",tr("Shared style; explicit overrides retained")}});
        }
        if(impact.isEmpty())impact.append(QVariantMap{{"name",libraryId},{"change",tr("Update unused style")}});
    } else if (operation == "insert") {
        QJsonObject definition;
        for (const auto &value : library["components"].toArray()) if (value.toObject()["id"] == libraryId) definition = value.toObject();
        if (definition.isEmpty()) return fail(tr("Choose an existing saved component."));
        QJsonObject destination;
        for (const auto &key : {"placement", "parentId", "categoryId"}) if (a.contains(key)) destination[key] = QJsonValue::fromVariant(a[key]);
        if(!destination.contains("placement") && (layout["composition"].toObject()["grid"].isObject() || definition["nodes"].toArray()[0].toObject().contains("placement"))) {
            const auto grid=ComponentSchema::componentGrid(nodes,layout["composition"].toObject().toVariantMap(),layout["categories"].toArray(),destination.value("parentId").toString(),destination.value("categoryId").toString());
            auto placement=definition["nodes"].toArray()[0].toObject()["placement"].toObject();
            const int width=placement["width"].toInt(1),height=placement["height"].toInt(1);bool found=false;
            for(int y=0;y<grid.value("rows",5).toInt() && !found;++y)for(int x=0;x<grid.value("columns",8).toInt();++x) {
                QVariantMap proposed{{"x",x},{"y",y},{"width",width},{"height",height}};
                if(!ComponentSchema::validPlacement(proposed,grid.value("columns",8).toInt(),grid.value("rows",5).toInt()))continue;
                bool collision=false;for(const auto &value:nodes){const auto node=value.toObject();if(node["parentId"].toString()==destination.value("parentId").toString() && node["categoryId"].toString()==destination.value("categoryId").toString() && ComponentSchema::placementsOverlap(proposed,node["placement"].toObject().toVariantMap()))collision=true;}
                if(!collision){destination["placement"]=QJsonObject::fromVariantMap(proposed);found=true;break;}
            }
            if(!found)return fail(tr("No free placement fits this saved component. Move items or choose another page."));
        }
        const auto additions = instantiate(definition["nodes"].toArray(), allNodes, destination, a.value("linked").toBool() ? libraryId : QString());
        for (const auto &node : additions) nodes.append(node);
        layout["components"] = nodes; store(document, pageId, layout);
        impact.append(QVariantMap{{"page", pageId}, {"name", definition["name"].toString()}, {"change", a.value("linked").toBool() ? tr("Insert linked instance") : tr("Insert independent copy")}});
    } else if (operation == "editAll") {
        if (selected.isEmpty()) return fail(tr("Select the edited linked instance."));
        const auto link = selected[0].toObject()["linkedDesign"].toString();
        if (link.isEmpty()) return fail(tr("This is an independent component. Insert a linked instance first."));
        const auto definition = prototype(selected); auto entries = library["components"].toArray(); bool found = false;
        for (qsizetype i = 0; i < entries.size(); ++i) { auto entry = entries[i].toObject(); if (entry["id"] != link) continue; entry["nodes"] = definition; entries[i] = entry; found = true; break; }
        if (!found) return fail(tr("The linked definition is missing. Detach this instance to keep editing it."));
        library["components"] = entries;
        QStringList pages{QString()}; for (const auto &entry : document["pages"].toArray()) pages.append(entry.toObject()["id"].toString());
        for (const auto &id : pages) {
            auto target = page(document, id); auto values = target["components"].toArray(); QJsonArray replaced;
            QSet<QString> descendants;
            for (const auto &value : values) {
                const auto node = value.toObject();
                if (node["linkedDesign"] != link) continue;
                const auto parent = node["parentId"].toString(); bool child = false;
                for (const auto &possible : values) if (possible.toObject()["id"] == parent && possible.toObject()["linkedDesign"] == link) child = true;
                if (child) continue;
                const auto old = subtree(values, node["id"].toString()); for (const auto &item : old) descendants.insert(item.toObject()["id"].toString());
                const auto replacement = instantiate(definition, allNodes, node, link); for (const auto &item : replacement) {replaced.append(item);allNodes.append(item);}
                impact.append(QVariantMap{{"page", id}, {"id", node["id"].toString()}, {"name", node["name"].toString()}, {"change", tr("Edit linked instance")}});
            }
            QJsonArray result; for (const auto &value : values) if (!descendants.contains(value.toObject()["id"].toString())) result.append(value);
            for (const auto &value : replaced) result.append(value);
            target["components"] = result; store(document, id, target);
        }
    } else {
        if (selected.isEmpty()) return fail(tr("Select a component first."));
        const auto id = selected[0].toObject()["id"].toString(); bool changed = false;
        QJsonObject styleDefinition;
        for (const auto &value : library["styles"].toArray()) if (value.toObject()["id"] == libraryId) styleDefinition = value.toObject();
        auto sourceLayout = page(document, a.value("sourcePage").toString());
        const auto source = subtree(sourceLayout["components"].toArray(), a.value("sourceId").toString());
        for (qsizetype i = 0; i < nodes.size(); ++i) {
            auto node = nodes[i].toObject();
            if (operation == "detach") {
                bool member = false; for (const auto &value : selected) if (value.toObject()["id"] == node["id"]) member = true;
                if (!member) continue; node.remove("linkedDesign"); node.remove("designNode");
            } else {
                if (node["id"] != id) continue;
                auto appearance = node["presentation"].toObject();
                if (operation == "useStyle") {
                    if (styleDefinition.isEmpty()) return fail(tr("Choose an existing shared style."));
                    const auto visual=DesignSystem::styleProperties(appearance.toVariantMap());for(auto it=visual.cbegin();it!=visual.cend();++it)appearance.remove(it.key());
                    appearance["styleId"]=libraryId;
                } else if (operation == "resetOverrides") {
                    if (!appearance.contains("styleId")) return fail(tr("This component has no shared style."));
                    const auto visual=DesignSystem::styleProperties(appearance.toVariantMap());for(auto it=visual.cbegin();it!=visual.cend();++it)appearance.remove(it.key());
                } else if (operation == "detachStyle") {
                    appearance = QJsonObject::fromVariantMap(DesignSystem::presentation(appearance.toVariantMap(), library.toVariantMap()));
                } else if (operation == "pasteAppearance") {
                    if (source.isEmpty()) return fail(tr("The copied appearance is no longer available."));
                    const auto visual=DesignSystem::styleProperties(DesignSystem::presentation(source[0].toObject()["presentation"].toObject().toVariantMap(),library.toVariantMap()));
                    const auto previous=DesignSystem::styleProperties(appearance.toVariantMap());for(auto it=previous.cbegin();it!=previous.cend();++it)appearance.remove(it.key());appearance.remove("styleId");
                    for(auto it=visual.cbegin();it!=visual.cend();++it)appearance[it.key()]=QJsonValue::fromVariant(it.value());
                } else return fail(tr("Unknown reusable-design operation."));
                if (!ComponentSchema::validPresentation(appearance.toVariantMap(), node["componentType"] == "library", node["componentType"] == "container")) return fail(tr("This appearance contains properties incompatible with the destination. No properties were discarded."));
                node["presentation"] = appearance;
            }
            nodes[i] = node; changed = true;
        }
        if (!changed) return fail(tr("The selected component no longer exists."));
        layout["components"] = nodes; store(document, pageId, layout);
        impact.append(QVariantMap{{"page", pageId}, {"id", id}, {"name", selected[0].toObject()["name"].toString()}, {"change", operation}});
    }
    composition = document["composition"].toObject(); composition["designLibrary"] = library; document["composition"] = composition;
    if (!DesignSystem::valid(library.toVariantMap())) return fail(tr("The saved definition is invalid or cyclic. Nested linked symbols are not supported."));
    if (!proposeDocument(QJsonDocument(document).toJson())) return {};
    return {{"valid", true}, {"changes", impact}};
}
