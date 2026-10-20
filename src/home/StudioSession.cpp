#include "home/StudioSession.h"
#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QFile>
#include <algorithm>
#include <functional>

StudioSession::StudioSession(QObject *parent)
    : QObject(parent)
{
    connect(this, &StudioSession::changed, this, [this] {
        if (m_model && !m_draft.isEmpty() && m_model->documentData() != m_draft) {
            m_model->loadDocument(m_draft, false);
            emit m_model->layoutChanged();
        }
    });
}

QObject *StudioSession::model() const { return m_model; }
QObject *StudioSession::proposalModel() const { return m_proposalModel; }

bool StudioSession::previewCommand(const QString &operation, const QVariantMap &arguments, const QString &pageId)
{
    if (!m_active || !m_live) return false;
    HomeLayout temporary(m_live->registry()); temporary.m_draftOnly = true;
    if (!temporary.loadDocument(m_draft, false)) return false;
    StudioSession transaction;
    if (!transaction.bind(&temporary) || !transaction.execute(operation, arguments, pageId)) {
        setError(transaction.errorString()); return false;
    }
    m_proposed = transaction.draft();
    m_proposalBaseline = m_draft;
    if (!m_proposalModel) { m_proposalModel = new HomeLayout(m_live->registry(),this); m_proposalModel->m_draftOnly = true; }
    m_proposalModel->m_userPath=m_live->m_userPath;
    m_proposalModel->loadDocument(m_proposed,false);
    emit m_proposalModel->layoutChanged(); emit changed();
    return true;
}

bool StudioSession::acceptPreview()
{
    if (m_proposed.isEmpty()) return false;
    if (m_draft != m_proposalBaseline) { setError(QStringLiteral("The draft changed. Preview the arrangement again.")); return false; }
    const auto candidate = m_proposed;
    if (!pushEdit(candidate)) return false;
    cancelPreview(); return true;
}

void StudioSession::cancelPreview()
{
    m_proposed.clear();
    m_proposalBaseline.clear();
    if (m_proposalModel) { m_proposalModel->deleteLater(); m_proposalModel = nullptr; }
    emit changed();
}

bool StudioSession::bind(HomeLayout *layout)
{
    if (!layout) return false;
    m_live = layout;
    m_profilePath = layout->m_userPath;
    QFile disk(m_profilePath);
    m_diskExisted = disk.exists();
    m_diskReadable = !m_diskExisted || (disk.size() <= kMaxDocumentBytes && disk.open(QIODevice::ReadOnly));
    m_diskBaseline = m_diskReadable && m_diskExisted ? disk.readAll() : QByteArray();
    m_model = new HomeLayout(layout->registry(), this);
    m_model->m_draftOnly = true;
    m_model->m_userPath=layout->m_userPath;
    m_model->m_defaultPath=layout->m_defaultPath;
    m_model->m_collections = layout->m_collections;
    m_model->m_captureInputsReady = layout->m_captureInputsReady;
    m_active = open(layout->committedDocument());
    connect(layout, &QObject::destroyed, this, &StudioSession::invalidate);
    return m_active;
}

void StudioSession::invalidate()
{
    m_active = false;
    m_live.clear();
    m_diskBaseline.clear();
    m_profilePath.clear();
    cancelPreview();
    m_draft.clear();
    m_committed.clear();
    m_undo.clear();
    m_redo.clear();
    if (m_model) {
        m_model->m_items = {};
        m_model->m_categories = {};
        m_model->m_pages = {};
        m_model->m_collections = {};
        m_model->m_composition.clear();
        m_model->m_homePageId.clear();
        m_model->m_rootPage={};m_model->m_studioAppearance.clear();
        m_model->m_userPath.clear();
        m_model->m_defaultPath.clear();
        m_model->syncPages();
        emit m_model->layoutChanged();
    }
    setError(QStringLiteral("The profile edit session has closed."));
    emit changed();
}

bool StudioSession::apply(const QString &focusId)
{
    if (!m_active || !m_live || m_live->m_userPath != m_profilePath) {
        setError(QStringLiteral("The profile changed. Reopen Page Studio."));
        return false;
    }
    if (isStaleAgainst(m_live->committedDocument())) {
        setError(QStringLiteral("The applied design changed. Reload or copy your draft before applying."));
        return false;
    }
    QFile disk(m_profilePath);
    if (!m_diskReadable || disk.exists() != m_diskExisted ||
        (m_diskExisted && (disk.size() > kMaxDocumentBytes || !disk.open(QIODevice::ReadOnly) || disk.readAll() != m_diskBaseline))) {
        setError(QStringLiteral("The saved design changed or cannot be read. Reopen Studio before applying; your draft is retained."));
        return false;
    }
    disk.close();
    if (!m_live->applyStudioDraft(m_draft, focusId)) {
        setError(m_live->errorString());
        return false;
    }
    m_draft = m_live->committedDocument();
    m_diskBaseline = m_draft; m_diskExisted = true; m_diskReadable = true;
    markApplied();
    return true;
}

bool StudioSession::execute(const QString &operation, const QVariantMap &a, const QString &pageId)
{
    if (!m_active || !m_live) { setError(QStringLiteral("Reopen Page Studio before editing.")); return false; }
    if(operation=="studioAppearance") {
        auto document=QJsonDocument::fromJson(m_draft).object();
        const auto value=a.value("value").toMap();
        if(value.isEmpty())document.remove("studioAppearance");else document["studioAppearance"]=QJsonObject::fromVariantMap(value);
        return pushEdit(QJsonDocument(document).toJson());
    }
    if(QStringList{"renamePage","duplicatePage","reorderPage","removePage"}.contains(operation)) {
        auto document=QJsonDocument::fromJson(m_draft).object();auto pages=document["pages"].toArray();
        const QString id=a.value("id").toString();int index=-1;
        for(int i=0;i<pages.size();++i)if(pages[i].toObject()["id"]==id)index=i;
        auto rootMeta=document["rootPage"].toObject();
        if(rootMeta.isEmpty())rootMeta={{"name",m_model->rootPageName()},{"visible",true},{"position",0}};
        if((id.isEmpty() && !rootMeta.value("visible").toBool()) || (!id.isEmpty() && index<0)){setError(tr("This page no longer exists."));return false;}
        QStringList order;for(const auto &entry:m_model->studioPages())order.append(entry.toMap()["id"].toString());
        const auto storeOrder=[&] {
            QJsonArray sorted;
            for(const auto &key:order)if(!key.isEmpty())for(const auto &entry:pages)if(entry.toObject()["id"]==key)sorted.append(entry);
            pages=sorted;rootMeta["position"]=qMax(0,int(order.indexOf(QString())));
        };
        if(operation=="renamePage" || operation=="duplicatePage") {
            const auto name=a.value("name").toString().trimmed();
            if(name.isEmpty() || name.size()>80 || name.contains(QChar::Null)){setError(tr("Use a page name of 1–80 characters."));return false;}
            if(operation=="renamePage") {
                if(id.isEmpty())rootMeta["name"]=name;
                else{auto entry=pages[index].toObject();entry["name"]=name;pages[index]=entry;}
            } else {
                if(order.size()>=64){setError(tr("Up to 64 pages are supported per profile."));return false;}
                auto layout=id.isEmpty()?document:pages[index].toObject()["layout"].toObject();
                layout.remove("pages");layout.remove("homePageId");layout.remove("rootPage");layout.remove("studioAppearance");
                QHash<QString,QString> copiedIds;QSet<QString> allocated;
                auto composition=layout["composition"].toObject();
                const auto remapIdentity=[&](const QString &old)->QString {
                    if(copiedIds.contains(old))return copiedIds[old];
                    auto fresh=allocateId(old);if(fresh.isEmpty())return {};
                    const auto base=fresh;int suffix=2;
                    while(allocated.contains(fresh))fresh=base+QString::number(suffix++);
                    if(!ComponentSchema::safeId(fresh))return {};
                    copiedIds[old]=fresh;allocated.insert(fresh);return fresh;
                };
                for(const auto &key:QStringList{"categories","components"}) {
                    auto values=layout[key].toArray();
                    for(int i=0;i<values.size();++i){auto entry=values[i].toObject();const auto fresh=remapIdentity(entry["id"].toString());if(fresh.isEmpty())return false;entry["id"]=fresh;values[i]=entry;}
                    layout[key]=values;
                }
                auto components=layout["components"].toArray();
                for(int i=0;i<components.size();++i){auto item=components[i].toObject();const auto category=item.value("categoryId").toString();if(copiedIds.contains(category))item["categoryId"]=copiedIds[category];components[i]=item;}
                layout["components"]=components;
                auto tabs=composition.value("navigation").toArray();
                for(int i=0;i<tabs.size();++i){auto tab=tabs[i].toObject();const auto old=tab["id"].toString(),fresh=remapIdentity(old);if(fresh.isEmpty())return false;tab["id"]=fresh;
                    auto sections=tab.value("sections").toArray();for(int j=0;j<sections.size();++j)if(copiedIds.contains(sections[j].toString()))sections[j]=copiedIds[sections[j].toString()];
                    if(tab.contains("sections"))tab["sections"]=sections;tabs[i]=tab;}
                if(composition.contains("navigation"))composition["navigation"]=tabs;
                const auto initial=composition.value("initialTab").toString();if(copiedIds.contains(initial))composition["initialTab"]=copiedIds[initial];
                layout["composition"]=composition;
                // Only local identities/references change; page/app/media links
                // retain their original destinations until explicitly edited.
                auto copyId=allocateId("page");if(copyId.isEmpty())return false;
                const auto copyBase=copyId;int copySuffix=2;
                while(allocated.contains(copyId))copyId=copyBase+QString::number(copySuffix++);
                if(!ComponentSchema::safeId(copyId)){setError(tr("The duplicated page identity exceeds its limit."));return false;}
                pages.append(QJsonObject{{"id",copyId},{"name",name},{"layout",layout}});
                order.insert(order.indexOf(id)+1,copyId);storeOrder();
            }
        } else if(operation=="reorderPage") {
            const int from=order.indexOf(id),offset=a.value("offset").toInt(),to=from+offset;
            if((offset!=1 && offset!=-1) || to<0 || to>=order.size()){setError(tr("This page is already at the edge of the page list."));return false;}
            order.move(from,to);storeOrder();
        } else {
            if(document["homePageId"].toString()==id){setError(tr("Set another page as Home before deleting this page."));return false;}
            const bool remap=a.contains("replacementPageId");const QString replacement=a.value("replacementPageId").toString();
            bool validReplacement=replacement.isEmpty()?rootMeta.value("visible").toBool():false;
            for(const auto &entry:pages)if(entry.toObject()["id"]==replacement)validReplacement=true;
            if(remap && (!validReplacement || replacement==id)){setError(tr("Choose another existing page for incoming links."));return false;}
            if(id.isEmpty()) {
                document["components"]=QJsonArray{};document["categories"]=QJsonArray{};document["composition"]=QJsonObject{};rootMeta["visible"]=false;
            } else pages.removeAt(index);
            document["pages"]=pages;
            int links=0;bool invalidOverride=false;
            std::function<QJsonValue(QJsonValue)> repair=[&](QJsonValue value)->QJsonValue {
                if(value.isArray()){auto values=value.toArray();for(int i=0;i<values.size();++i)values[i]=repair(values[i]);return values;}
                if(!value.isObject())return value;
                auto object=value.toObject();
                if((!id.isEmpty() && object.value("type")=="open-page" && object.value("pageId")==id) || (id.isEmpty() && object.value("type")=="root-page")) {
                    ++links;
                    if(remap){object["type"]=replacement.isEmpty()?"root-page":"open-page";if(replacement.isEmpty())object.remove("pageId");else object["pageId"]=replacement;}
                }
                for(auto it=object.begin();it!=object.end();++it) {
                    if(!id.isEmpty() && QStringList{"moviePage","showsPage"}.contains(it.key()) && it.value()==id) {
                        ++links;if(remap){if(replacement.isEmpty())invalidOverride=true;else it.value()=replacement;}
                    } else it.value()=repair(it.value());
                }
                return object;
            };
            document=repair(document).toObject();
            if((links && !remap) || invalidOverride){setError(tr("%1 incoming links require a named replacement page before deletion.").arg(links));return false;}
            pages=document["pages"].toArray();order.removeAll(id);storeOrder();
        }
        const auto normalizeComponents=[](QJsonObject layout) {
            const auto key=layout.value("schemaVersion").toInt()==1?QStringLiteral("items"):QStringLiteral("components");
            auto components=layout.value(key).toArray();
            for(int i=0;i<components.size();++i)components[i]=QJsonObject::fromVariantMap(ComponentSchema::component(components[i].toObject().toVariantMap(),true));
            layout[key]=components;return layout;
        };
        document=normalizeComponents(document);
        for(int i=0;i<pages.size();++i){auto entry=pages[i].toObject();entry["layout"]=normalizeComponents(entry["layout"].toObject());pages[i]=entry;}
        document["pages"]=pages;document["rootPage"]=rootMeta;
        return pushEdit(QJsonDocument(document).toJson());
    }
    if(operation=="setHomePage") {
        auto document=QJsonDocument::fromJson(m_draft).object();
        const auto id=a.value("id").toString();bool found=id.isEmpty() && m_model->rootPageVisible();
        for(const auto &entry:document["pages"].toArray())if(entry.toObject()["id"]==id)found=true;
        if(!found){setError(QStringLiteral("Choose an existing page as Home."));return false;}
        document["homePageId"]=id;return pushEdit(QJsonDocument(document).toJson());
    }
    if(operation=="addResolved" || operation=="addPlaced") {
        const auto proposal=additionProposal(a.value("type").toString(),a.value("placement").toMap(),
            a.value("strategy","cancel").toString(),pageId,a.value("categoryId").toString());
        if(!proposal.value("valid").toBool()){setError(proposal.value("reason").toString());return false;}
        return pushEdit(QJsonDocument(QJsonObject::fromVariantMap(proposal.value("document").toMap())).toJson());
    }
    if (operation == "batch") {
        const auto commands = a.value("commands").toList();
        if (commands.isEmpty() || commands.size() > 32) { setError(QStringLiteral("Choose between 1 and 32 grouped edits.")); return false; }
        HomeLayout temporary(m_live->registry()); temporary.m_draftOnly = true;
        if (!temporary.loadDocument(m_draft, false)) { setError(temporary.errorString()); return false; }
        StudioSession transaction;
        if (!transaction.bind(&temporary)) { setError(transaction.errorString()); return false; }
        for (const auto &entry : commands) {
            const auto command = entry.toMap();
            const auto op = command.value("operation").toString();
            if (op == "batch" || !transaction.execute(op, command.value("arguments").toMap(), command.value("pageId", pageId).toString())) {
                setError(op == "batch" ? QStringLiteral("Nested batches are not supported.") : transaction.errorString());
                return false;
            }
        }
        return pushEdit(transaction.draft());
    }
    auto root = QJsonDocument::fromJson(m_draft).object();
    auto pages = root["pages"].toArray();
    int pageIndex = -1;
    QJsonObject document = root;
    if (!pageId.isEmpty()) {
        for (int i = 0; i < pages.size(); ++i) if (pages[i].toObject()["id"] == pageId) pageIndex = i;
        if (pageIndex < 0) { setError(QStringLiteral("That page no longer exists.")); return false; }
        document = pages[pageIndex].toObject()["layout"].toObject();
    }
    HomeLayout worker(m_live->registry());
    worker.m_draftOnly = true;
    if (!worker.loadDocument(QJsonDocument(document).toJson(), false)) { setError(worker.errorString()); return false; }
    const auto id = a.value("id").toString();
    bool ok = false;
    if (operation == "place") {
        // Validate the complete result once: swaps/groups must not fail on an intermediate collision.
        auto entries = document["components"].toArray();
        const auto changes = a.value("placements").toList();
        QSet<QString> touched;
        if (changes.isEmpty() || changes.size() > 64) { setError(QStringLiteral("Choose between 1 and 64 placements.")); return false; }
        for (const auto &value : changes) {
            const auto change = value.toMap();
            const auto target = change.value("id").toString();
            bool found = false;
            if (touched.contains(target)) { setError(QStringLiteral("Duplicate placement identity.")); return false; }
            for (int i = 0; i < entries.size(); ++i) {
                auto entry = entries[i].toObject();
                if (entry["id"] != target) continue;
                if(entry.value("locked").toBool() && entry["placement"].toObject()!=QJsonObject::fromVariantMap(change.value("placement").toMap())) {setError(tr("Unlock the component before moving or resizing it."));return false;}
                entry["placement"] = QJsonObject::fromVariantMap(change.value("placement").toMap());
                entries[i] = entry; found = true;
            }
            if (!found) { setError(QStringLiteral("The component no longer exists.")); return false; }
            touched.insert(target);
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "convertGrid") {
        const auto grid = ComponentSchema::normalizedGrid(a.value("value").toMap());
        if (!ComponentSchema::validGrid(grid)) {
            setError(QStringLiteral("The chosen grid cannot contain this page. Choose a larger grid or cancel.")); return false;
        }
        auto composition = worker.m_composition;
        composition["schemaVersion"] = 1; composition["enabled"] = true;
        composition["grid"] = grid;
        if(grid.value("mode")=="sections") {
            composition["columns"]=1;composition["fit"]=false;
            composition["sectionHeight"]=a.value("sectionHeight",m_live->studioOptions().value("sectionHeight"));
            for(int i=0;i<worker.m_categories.size();++i) {
                auto category=worker.m_categories[i].toObject();auto style=category["presentation"].toObject();
                if(style.value("variant")!="library"){style["height"]=QJsonValue::fromVariant(composition["sectionHeight"]);category["presentation"]=style;worker.m_categories[i]=category;}
            }
        } else {
            for(const auto &entry:worker.m_categories) {
                const auto style=entry.toObject()["presentation"].toObject();
                if(style["variant"]=="library" || style.contains("trailingAction")) {
                    setError(QStringLiteral("Choose scrolling sections to preserve this page's library or section actions."));return false;
                }
            }
        }
        worker.m_composition = composition;
        QHash<QString,int> nextCell;
        for (int i = 0; i < worker.m_items.size(); ++i) {
            auto item = worker.m_items[i].toObject();
            const auto category=grid.value("mode")=="sections"?item["categoryId"].toString():QString();
            const auto local=ComponentSchema::sectionGrid(composition,worker.m_categories,category);
            const int columns=local["columns"].toInt(),rows=local["rows"].toInt(),cell=nextCell[category]++;
            if(cell>=columns*rows){setError(QStringLiteral("A section cannot contain its components in the chosen grid. Nothing changed."));return false;}
            item["placement"] = QJsonObject{{"x", cell%columns}, {"y", cell/columns}, {"width", 1}, {"height", 1}};
            worker.m_items[i] = item;
        }
        ok = worker.save();
    } else if(operation=="state") {
        const auto key=a.value("key").toString();
        if(!QStringList{"hidden","locked"}.contains(key) || a.value("value").metaType().id()!=QMetaType::Bool){setError(tr("Choose a valid visibility or lock state."));return false;}
        auto entries=document["components"].toArray();bool found=false;
        for(int i=0;i<entries.size();++i)if(entries[i].toObject()["id"]==id){auto item=entries[i].toObject();item[key]=QJsonValue::fromVariant(a.value("value"));entries[i]=item;found=true;}
        if(!found){setError(tr("Choose an existing component."));return false;}
        document["components"]=entries;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if (operation == "rename") ok = worker.setItemName(id, a.value("name").toString());
    else if (operation == "remove") ok = worker.removeItem(id);
    else if (operation == "duplicate") {
        ok = worker.duplicateItem(id);
        if (ok && worker.m_composition.contains("grid")) {
            auto copy = worker.m_items.last().toObject();
            QVariantMap shape;
            for (const auto &entry : worker.m_items) if (entry.toObject()["id"] == id) shape = entry.toObject()["placement"].toObject().toVariantMap();
            if (shape.isEmpty()) shape = {{"x",0},{"y",0},{"width",1},{"height",1}};
            const auto grid = worker.grid(); bool free = false;
            for (int row=0; row<grid["rows"].toInt() && !free; ++row) for (int col=0; col<grid["columns"].toInt() && !free; ++col) {
                shape["x"]=col; shape["y"]=row;
                if (ComponentSchema::validPlacement(shape,grid["columns"].toInt(),grid["rows"].toInt()) && worker.gridCollisions(copy["id"].toString(),shape).isEmpty()) free=true;
            }
            ok = free && worker.setItemPlacement(copy["id"].toString(),shape);
            if (!ok) worker.setError(QStringLiteral("There is no free region for the duplicate. Nothing changed."));
        }
    }
    else if (operation == "reorder") ok = worker.moveItem(id, a.value("offset").toInt());
    else if (operation == "presentation") ok = worker.setItemPresentation(id, a.value("value").toMap());
    else if (operation == "appearance") ok = worker.setAppearance(a.value("value").toMap());
    else if (operation == "composition") ok = worker.setComposition(a.value("value").toMap());
    else if (operation == "grid") ok = worker.setGrid(a.value("value").toMap());
    else if (operation == "action") ok = worker.setItemAction(id, a.value("value").toMap());
    else if (operation == "addWidget" || operation == "addPlaced") {
        ok = worker.addWidget(a.value("type").toString());
        if (ok && operation == "addPlaced") {
            const auto newId = worker.m_items.last().toObject()["id"].toString();
            ok = worker.setItemPlacement(newId, a.value("placement").toMap());
        }
    }
    else if (operation == "addButton") ok = worker.addButton();
    else if (operation == "newPage" && pageId.isEmpty()) ok = !worker.createPage(a.value("name").toString(), a.value("preset", "blank").toString()).isEmpty();
    else if (operation == "renamePage" && pageId.isEmpty()) ok = worker.renamePage(id, a.value("name").toString());
    else if (operation == "removePage" && pageId.isEmpty()) ok = worker.removePage(id);
    else { setError(QStringLiteral("Unknown or unavailable edit command.")); return false; }
    if (!ok) { setError(worker.errorString().isEmpty() ? QStringLiteral("The edit was refused; nothing changed.") : worker.errorString()); return false; }
    auto result = QJsonDocument::fromJson(worker.documentData()).object();
    if (operation == "newPage") {
        auto entries = result["pages"].toArray();
        auto entry = entries.last().toObject();
        entry["id"] = allocateId("page");
        if(a.value("preset","blank")=="blank") {
            auto layout=entry["layout"].toObject();auto composition=layout["composition"].toObject();
            composition["grid"]=QJsonObject::fromVariantMap(m_live->studioOptions().value("defaultGrid").toMap());
            layout["composition"]=composition;entry["layout"]=layout;
        }
        entries[entries.size() - 1] = entry;
        result["pages"] = entries;
    } else if (operation == "addButton" || operation == "addWidget" || operation == "addPlaced" || operation == "duplicate") {
        auto entries = result["components"].toArray();
        auto entry = entries.last().toObject();
        const auto base = operation == "addButton" ? QStringLiteral("button") : operation == "duplicate" ? id + QStringLiteral("-copy") : a.value("type").toString();
        const auto identity = allocateId(base);
        if (identity.isEmpty()) return false;
        entry["id"] = identity;
        entries[entries.size() - 1] = entry;
        result["components"] = entries;
    }
    if (pageIndex >= 0) {
        result.remove("pages"); // A page document cannot recursively contain the interface page collection.
        result.remove("homePageId");
        result.remove("rootPage");
        result.remove("studioAppearance");
        auto entry = pages[pageIndex].toObject(); entry["layout"] = result; pages[pageIndex] = entry; root["pages"] = pages;
    } else root = result;
    return pushEdit(QJsonDocument(root).toJson());
}

bool StudioSession::open(const QByteArray &committedDocument)
{
    if (!validDraft(committedDocument)) return false;
    m_committed = committedDocument;
    m_draft = committedDocument;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
    return true;
}

bool StudioSession::pushEdit(const QByteArray &candidateDraft)
{
    if (m_committed.isEmpty()) {
        setError(QStringLiteral("Open a page before editing."));
        return false;
    }
    if (candidateDraft == m_draft) return true;
    if (!validDraft(candidateDraft)) return false;
    qsizetype retained = m_draft.size();
    for (const auto &entry : m_undo) retained += entry.size();
    if (m_undo.size() >= kMaxUndoSteps || retained > kMaxHistoryBytes) {
        setError(QStringLiteral("History budget reached. Continue with a new history baseline or cancel this edit."));
        return false;
    }
    m_undo.push(m_draft);
    m_draft = candidateDraft;
    m_redo.clear();
    setError({});
    emit changed();
    return true;
}

bool StudioSession::undo()
{
    if (m_undo.isEmpty()) {
        setError(QStringLiteral("Nothing to undo."));
        return false;
    }
    m_redo.push(m_draft);
    m_draft = m_undo.pop();
    setError({});
    emit changed();
    return true;
}

bool StudioSession::redo()
{
    if (m_redo.isEmpty()) {
        setError(QStringLiteral("Nothing to redo."));
        return false;
    }
    m_undo.push(m_draft);
    m_draft = m_redo.pop();
    setError({});
    emit changed();
    return true;
}

void StudioSession::revert()
{
    m_draft = m_committed;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

void StudioSession::markApplied()
{
    m_committed = m_draft;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

bool StudioSession::isDirty() const
{
    return !m_committed.isEmpty() && m_draft != m_committed;
}

QString StudioSession::allocateId(const QString &baseId)
{
    // Deterministic "-copy"/"-2" suffix walk, mirroring duplicateItem so both
    // native and scripted paths converge on the same bytes from the same ids.
    QString base = baseId.trimmed();
    if (!ComponentSchema::safeId(base)) {
        setError(QStringLiteral("That name cannot be used as an identity — use letters, digits, dot, dash or underscore."));
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(m_draft);
    QSet<QString> taken;
    if (doc.isObject()) {
        const auto collect = [&taken](const QJsonObject &object) {
            for (const auto &key : {"components", "items", "pages", "categories"})
                for (const auto &entry : object.value(key).toArray()) taken.insert(entry.toObject().value("id").toString());
        };
        collect(doc.object());
        for (const auto &page : doc.object().value("pages").toArray()) collect(page.toObject().value("layout").toObject());
    }
    if (!taken.contains(base)) return base;
    const QString copyBase = base + QStringLiteral("-copy");
    if (!ComponentSchema::safeId(copyBase)) { setError(QStringLiteral("The generated identity exceeds its length limit. Rename the component before duplicating.")); return {}; }
    if (!taken.contains(copyBase)) return copyBase;
    int suffix = 2;
    while (taken.contains(copyBase + QString::number(suffix))) ++suffix;
    return copyBase + QString::number(suffix);
}

namespace {
QJsonValue canonicalValue(const QJsonValue &value)
{
    if (value.isObject()) {
        QJsonObject sorted;
        QStringList keys;
        const auto object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            keys.append(it.key());
        std::sort(keys.begin(), keys.end());
        for (const QString &key : keys)
            sorted.insert(key, canonicalValue(object.value(key)));
        return sorted;
    }
    if (value.isArray()) {
        QJsonArray out;
        for (const QJsonValue &entry : value.toArray())
            out.append(canonicalValue(entry));
        return out;
    }
    return value;
}
}

bool StudioSession::canonicalize(const QByteArray &first, const QByteArray &second,
                                 QByteArray &firstOut, QByteArray &secondOut)
{
    const QJsonDocument firstDoc = QJsonDocument::fromJson(first);
    const QJsonDocument secondDoc = QJsonDocument::fromJson(second);
    if (firstDoc.isNull() || secondDoc.isNull()) {
        setError(QStringLiteral("Cannot compare drafts — one is not valid JSON."));
        return false;
    }
    const auto emitDoc = [](const QJsonDocument &doc) {
        if (doc.isObject())
            return QJsonDocument(canonicalValue(doc.object()).toObject()).toJson(QJsonDocument::Compact);
        if (doc.isArray())
            return QJsonDocument(canonicalValue(doc.array()).toArray()).toJson(QJsonDocument::Compact);
        return doc.toJson(QJsonDocument::Compact);
    };
    firstOut = emitDoc(firstDoc);
    secondOut = emitDoc(secondDoc);
    return true;
}

bool StudioSession::isStaleAgainst(const QByteArray &currentApplied) const
{
    if (!isDirty()) return false;
    if (currentApplied.isEmpty()) return true; // applied doc vanished: conflict
    QByteArray mine, theirs;
    // Compare canonically so formatting-only rewrites are not conflicts.
    StudioSession *self = const_cast<StudioSession *>(this);
    if (!self->canonicalize(m_committed, currentApplied, mine, theirs)) return true;
    return mine != theirs;
}

bool StudioSession::validDraft(const QByteArray &data)
{
    if (data.isEmpty()) {
        setError(QStringLiteral("The draft document is empty — nothing was written."));
        return false;
    }
    if (data.size() > kMaxDocumentBytes) {
        setError(QStringLiteral("The draft exceeds 4 MiB and was refused."));
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        setError(QStringLiteral("The draft is not a valid layout document."));
        return false;
    }
    const QJsonObject object = doc.object();
    const int version = object.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version != 1 && version != 2) {
        setError(QStringLiteral("Unknown layout version — draft refused."));
        return false;
    }
    if (object.contains(version == 1 ? QStringLiteral("components") : QStringLiteral("items"))) {
        setError(QStringLiteral("The draft mixes incompatible layout versions."));
        return false;
    }
    const QJsonArray components = object.value(version == 1 ? QStringLiteral("items") : QStringLiteral("components")).toArray();
    if (components.size() > kMaxComponents) {
        setError(QStringLiteral("The draft exceeds 1024 components and was refused."));
        return false;
    }
    QString error;
    if (!HomeLayout::validateStudioDocument(data, &error)) {
        setError(error);
        return false;
    }
    return true;
}

void StudioSession::setError(const QString &message)
{
    if (m_error == message) return;
    m_error = message;
    emit changed();
}

QVariantMap StudioSession::placementProposal(const QString &id, const QVariantMap &placement,
                                            const QString &strategy, const QString &pageId)
{
    const auto fail = [](const QString &reason) { return QVariantMap{{"valid", false}, {"reason", reason}}; };
    if (!m_active) return fail(QStringLiteral("The edit session has closed."));
    auto doc = QJsonDocument::fromJson(m_draft).object();
    if (!pageId.isEmpty()) {
        bool found = false;
        for (const auto &entry : doc["pages"].toArray()) if (entry.toObject()["id"] == pageId) {
            doc = entry.toObject()["layout"].toObject(); found = true; break;
        }
        if (!found) return fail(QStringLiteral("The page no longer exists."));
    }
    auto entries = doc["components"].toArray();
    QString category;bool selectedFound=false;
    for(const auto &entry:entries)if(entry.toObject()["id"]==id){if(entry.toObject().value("locked").toBool())return fail(tr("Unlock the component before moving or resizing it."));category=entry.toObject()["categoryId"].toString();selectedFound=true;}
    if(!selectedFound)return fail(QStringLiteral("The component no longer exists."));
    const auto composition=doc["composition"].toObject().toVariantMap();
    const bool sections=composition.value("grid").toMap().value("mode")=="sections";
    const auto grid = ComponentSchema::sectionGrid(composition,doc["categories"].toArray(),category);
    const int cols = grid["columns"].toInt(), rows = grid["rows"].toInt();
    if (!ComponentSchema::validPlacement(placement, cols, rows)) return fail(QStringLiteral("The candidate is outside the grid."));
    int selected = -1;
    QVariantMap origin;
    QList<int> colliders;
    for (int i = 0; i < entries.size(); ++i) {
        const auto item = entries[i].toObject();
        if (item["id"] == id) { selected = i; origin = item["placement"].toObject().toVariantMap(); }
        else if ((!sections || item["categoryId"].toString()==category) && item.contains("placement") && ComponentSchema::placementsOverlap(item["placement"].toObject().toVariantMap(), placement)) colliders.append(i);
    }
    if (selected < 0) return fail(QStringLiteral("The component no longer exists."));
    for(const int index:colliders)if(entries[index].toObject().value("locked").toBool())return fail(tr("A locked component occupies this region. Unlock it or choose another position."));
    QVariantList changes{QVariantMap{{"id", id}, {"placement", placement}}};
    auto moving = entries[selected].toObject(); moving["placement"] = QJsonObject::fromVariantMap(placement); entries[selected] = moving;
    if (!colliders.isEmpty() && strategy == "cancel") return fail(QStringLiteral("Occupied cells: choose Shift, Swap or Cancel."));
    if (!colliders.isEmpty() && strategy == "swap") {
        if (colliders.size() != 1 || origin.isEmpty()) return fail(QStringLiteral("Swap needs one occupant and an existing origin."));
        auto other = entries[colliders.first()].toObject();
        auto replacement = other["placement"].toObject().toVariantMap();
        replacement["x"] = origin["x"]; replacement["y"] = origin["y"];
        other["placement"] = QJsonObject::fromVariantMap(replacement); entries[colliders.first()] = other;
        changes.append(QVariantMap{{"id", other["id"].toString()}, {"placement", replacement}});
    } else if (!colliders.isEmpty() && strategy == "shift") {
        std::sort(colliders.begin(), colliders.end(), [&entries](int a, int b) {
            const auto x = entries[a].toObject(), y = entries[b].toObject();
            const auto p = x["placement"].toObject(), q = y["placement"].toObject();
            if (p["y"] != q["y"]) return p["y"].toInt() < q["y"].toInt();
            if (p["x"] != q["x"]) return p["x"].toInt() < q["x"].toInt();
            return x["id"].toString() < y["id"].toString();
        });
        if (colliders.size() >= 64) return fail(QStringLiteral("The shift exceeds 64 affected components."));
        // Reserve unaffected cells and the candidate first; place affected nodes in deterministic order.
        for (int index : colliders) { auto item = entries[index].toObject(); item.remove("placement"); entries[index] = item; }
        for (int index : colliders) {
            const auto original = doc["components"].toArray()[index].toObject()["placement"].toObject().toVariantMap();
            auto target = original;
            bool fits = false;
            for (int row = 0; row < rows && !fits; ++row) for (int col = 0; col < cols && !fits; ++col) {
                target["x"] = col; target["y"] = row;
                if (!ComponentSchema::validPlacement(target, cols, rows)) continue;
                bool free = true;
                for (int j = 0; j < entries.size(); ++j) if (j != index && (!sections || entries[j].toObject()["categoryId"].toString()==category) && entries[j].toObject().contains("placement") &&
                    ComponentSchema::placementsOverlap(entries[j].toObject()["placement"].toObject().toVariantMap(), target)) { free = false; break; }
                if (free) fits = true;
            }
            if (!fits) return fail(QStringLiteral("No complete shift fits. Cancel or choose another cell."));
            auto item = entries[index].toObject(); item["placement"] = QJsonObject::fromVariantMap(target); entries[index] = item;
            changes.append(QVariantMap{{"id", item["id"].toString()}, {"placement", target}});
        }
    } else if (!colliders.isEmpty()) return fail(QStringLiteral("Choose a collision strategy."));
    doc["components"] = entries;
    QString error;
    if (!HomeLayout::validateStudioDocument(QJsonDocument(doc).toJson(), &error)) return fail(error);
    return {{"valid", true}, {"placements", changes}, {"reason", QString()}};
}

QVariantMap StudioSession::additionProposal(const QString &type, const QVariantMap &placement,
                                           const QString &strategy, const QString &pageId, const QString &categoryId)
{
    if (!m_active || !m_live) return {{"valid",false},{"reason",QStringLiteral("The edit session has closed.")}};
    HomeLayout temporary(m_live->registry()); temporary.m_draftOnly = true;
    if (!temporary.loadDocument(m_draft,false)) return {{"valid",false},{"reason",temporary.errorString()}};
    StudioSession transaction;
    if (!transaction.bind(&temporary)) return {{"valid",false},{"reason",transaction.errorString()}};
    const auto identity = transaction.allocateId(type);
    if(identity.isEmpty())return {{"valid",false},{"reason",transaction.errorString()}};
    auto root=QJsonDocument::fromJson(m_draft).object();auto pages=root["pages"].toArray();auto document=root;int pageIndex=-1;
    if(!pageId.isEmpty()) {
        for(int i=0;i<pages.size();++i)if(pages[i].toObject()["id"]==pageId)pageIndex=i;
        if(pageIndex<0)return {{"valid",false},{"reason",QStringLiteral("The page no longer exists.")}};
        document=pages[pageIndex].toObject()["layout"].toObject();
    }
    // Obtain registered defaults from HomeLayout, then validate the complete
    // addition/collision result together. No invalid intermediate is published.
    temporary.m_composition.clear();temporary.m_items={};temporary.m_categories={};temporary.m_pages={};temporary.m_homePageId.clear();
    if(!temporary.addWidget(type))return {{"valid",false},{"reason",QStringLiteral("Choose a supported widget.")}};
    QString componentError;
    auto item=QJsonObject::fromVariantMap(ComponentSchema::component(temporary.m_items.last().toObject().toVariantMap(),true,&componentError));
    if(item.isEmpty())return {{"valid",false},{"reason",componentError}};
    item["id"]=identity;
    if(!categoryId.isEmpty())item["categoryId"]=categoryId;
    auto entries=document["components"].toArray();entries.append(item);document["components"]=entries;
    const auto store=[&] {
        if(pageIndex>=0){auto entry=pages[pageIndex].toObject();entry["layout"]=document;pages[pageIndex]=entry;root["pages"]=pages;}
        else root=document;
    };
    store();transaction.m_draft=QJsonDocument(root).toJson();
    auto proposal = transaction.placementProposal(identity,placement,strategy,pageId);
    if (!proposal.value("valid").toBool()) return proposal;
    proposal["id"] = identity;
    for(const auto &change:proposal["placements"].toList()) {
        const auto value=change.toMap();
        for(int i=0;i<entries.size();++i)if(entries[i].toObject()["id"]==value["id"].toString()) {
            auto entry=entries[i].toObject();entry["placement"]=QJsonObject::fromVariantMap(value["placement"].toMap());entries[i]=entry;
        }
    }
    document["components"]=entries;store();QString error;
    if(!HomeLayout::validateStudioDocument(QJsonDocument(root).toJson(),&error))return {{"valid",false},{"reason",error}};
    proposal["document"]=root.toVariantMap();
    proposal["commands"] = QVariantList{
        QVariantMap{{"operation","addResolved"},{"arguments",QVariantMap{{"type",type},{"placement",placement},{"strategy",strategy},{"categoryId",categoryId}}},{"pageId",pageId}}
    };
    return proposal;
}
