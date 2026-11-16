#include "home/StudioSession.h"

bool StudioSession::previewCommand(const QString& operation, const QVariantMap& arguments, const QString& pageId) {
    if (!editingPermitted() || !m_active || !m_live)
        return false;
    HomeLayout temporary(m_live->registry());
    configureWorker(temporary);
    if (!temporary.loadDocument(m_draft, false))
        return false;
    StudioSession transaction;
    if (!transaction.bind(&temporary) || !transaction.execute(operation, arguments, pageId)) {
        setError(transaction.errorString());
        return false;
    }
    m_proposed = transaction.draft();
    m_proposalBaseline = m_draft;
    if (!m_proposalModel)
        m_proposalModel = new HomeLayout(m_live->registry(), this);
    configureWorker(*m_proposalModel, true);
    m_proposalModel->loadDocument(m_proposed, false);
    emit m_proposalModel->layoutChanged();
    emit changed();
    return true;
}

bool StudioSession::acceptPreview() {
    if (m_proposed.isEmpty())
        return false;
    if (m_draft != m_proposalBaseline) {
        setError(QStringLiteral("The draft changed. Preview the arrangement again."));
        return false;
    }
    const auto candidate = m_proposed;
    if (!pushEdit(candidate))
        return false;
    cancelPreview();
    return true;
}

bool StudioSession::acceptColorPreview(const QString& color) {
    const QColor parsed(color);
    if (!m_active || !m_live || !parsed.isValid() || parsed.alpha() != 255) {
        setError(tr("Choose an opaque color in the active Studio session."));
        return false;
    }
    if (m_proposed.isEmpty())
        return true;
    if (m_draft != m_proposalBaseline) {
        setError(tr("The draft changed. Preview the color again."));
        return false;
    }
    auto candidate = QJsonDocument::fromJson(m_proposed).object();
    if (candidate != QJsonDocument::fromJson(m_draft).object()) {
        auto appearance = candidate["studioAppearance"].toObject();
        const auto recent = appearance["recentColors"].toArray();
        QJsonArray next{parsed.name()};
        for (const auto& entry : recent)
            if (entry.toString() != parsed.name() && next.size() < ComponentSchema::MaxRecentStudioColors)
                next.append(entry);
        appearance["recentColors"] = next;
        candidate["studioAppearance"] = appearance;
    }
    if (!pushEdit(QJsonDocument(candidate).toJson()))
        return false;
    cancelPreview();
    return true;
}

void StudioSession::cancelPreview() {
    m_proposed.clear();
    m_proposalBaseline.clear();
    if (m_proposalModel) {
        m_proposalModel->deleteLater();
        m_proposalModel = nullptr;
    }
    emit changed();
}

bool StudioSession::apply(const QString& focusId) {
    if (!editingPermitted())
        return false;
    if (!m_active || !m_live || m_live->m_userPath != m_profilePath) {
        setError(QStringLiteral("The profile changed. Reopen Skin Studio."));
        return false;
    }
    if (isStaleAgainst(m_live->committedDocument())) {
        setError(QStringLiteral("The applied design changed. Reload or copy your draft before applying."));
        return false;
    }
    if (!profileStorageSafe()) {
        setError(tr("The layout and version history must stay at their actual profile locations."));
        return false;
    }
    if (!diskRevisionUnchanged()) {
        setError(QStringLiteral(
            "The saved design changed or cannot be read. Reopen Studio before applying; your draft is retained."));
        return false;
    }
    QStringList created;
    const auto artDirectory = QDir(QFileInfo(m_profilePath).absolutePath()).filePath("artwork");
    const bool hadArtDirectory = QFileInfo::exists(artDirectory);
    if (QFileInfo(artDirectory).isSymLink()) {
        setError(tr("The managed artwork directory must stay inside this profile."));
        return false;
    }
    const auto cleanup = [&] {
        for (const auto& path : created)
            QFile::remove(path);
        if (!hadArtDirectory)
            QDir().rmdir(artDirectory);
    };
    const auto serialized = QString::fromUtf8(m_draft);
    for (auto it = m_stagedAssets.cbegin(); it != m_stagedAssets.cend(); ++it) {
        bool keepOriginal = false;
        for (auto origin = m_iconOriginals.cbegin(); origin != m_iconOriginals.end(); ++origin)
            if (origin.value() == it.key() && serialized.contains(origin.key()))
                keepOriginal = true;
        if (!serialized.contains(it.key()) && !keepOriginal)
            continue;
        QFile source(it.value());
        if (!source.open(QIODevice::ReadOnly)) {
            cleanup();
            setError(tr("Staged artwork is missing. Choose it again; the draft is retained."));
            return false;
        }
        const auto data = source.readAll(),
                   target = QDir(QFileInfo(m_profilePath).absolutePath()).filePath(it.key()).toUtf8();
        const auto expected = "artwork/studio-" +
                              QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) +
                              (ManagedGif::matches(data)   ? ".gif"
                               : it.key().endsWith(".png") ? ".png"
                                                           : ".jpg");
        if (data.size() > 20 * 1024 * 1024 || expected != it.key()) {
            cleanup();
            setError(tr("Staged artwork changed. Import it again before applying."));
            return false;
        }
        const auto targetPath = QString::fromUtf8(target);
        if (QFile::exists(targetPath)) {
            QFile existing(targetPath);
            if (!existing.open(QIODevice::ReadOnly) || existing.readAll() != data) {
                cleanup();
                setError(tr("Managed artwork conflicts with an existing file."));
                return false;
            }
            continue;
        }
        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
            cleanup();
            setError(tr("Could not create the managed artwork directory."));
            return false;
        }
        QSaveFile output(targetPath);
        if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) {
            cleanup();
            setError(tr("Could not save artwork. Nothing applied."));
            return false;
        }
        created.append(targetPath);
    }
    if (!diskRevisionUnchanged()) {
        cleanup();
        setError(tr("The saved design changed while preparing artwork. Your draft is retained."));
        return false;
    }
    if (isDirty() && !archiveVersion(tr("Before Apply"), m_committed)) {
        cleanup();
        return false;
    }
    if (!m_live->applyStudioDraft(m_draft, focusId)) {
        cleanup();
        setError(m_live->errorString());
        return false;
    }
    m_draft = m_live->committedDocument();
    m_diskBaseline = m_draft;
    m_diskExisted = true;
    m_diskReadable = true;
    markApplied();
    return true;
}

bool StudioSession::execute(const QString& operation, const QVariantMap& a, const QString& pageId) {
    if (!editingPermitted())
        return false;
    if (!currentProfile()) {
        setError(QStringLiteral("Reopen Skin Studio before editing."));
        return false;
    }
    if (operation == "replaceComponent") {
        if (a.size() != 2 || !a.contains("id") || !a.contains("type") ||
            a["id"].metaType().id() != QMetaType::QString || a["type"].metaType().id() != QMetaType::QString) {
            setError(tr("Choose a component and its replacement type."));
            return false;
        }
        const auto proposal = replacementProposal(a["id"].toString(), a["type"].toString(), pageId);
        if (!proposal["valid"].toBool()) {
            setError(proposal["reason"].toString());
            return false;
        }
        return pushEdit(QJsonDocument(QJsonObject::fromVariantMap(proposal["document"].toMap())).toJson());
    }
    if (operation == "restorePageDefaults") {
        if (!a.isEmpty()) {
            setError(tr("Choose the current page to restore."));
            return false;
        }
        QFile defaults(m_live->m_defaultPath);
        if (!defaults.open(QIODevice::ReadOnly) || defaults.size() > kMaxDocumentBytes) {
            setError(tr("Packaged page defaults are unavailable."));
            return false;
        }
        HomeLayout worker(m_live->registry());
        configureWorker(worker);
        if (!worker.loadDocument(defaults.readAll(), false)) {
            setError(worker.errorString());
            return false;
        }
        auto replacement = QJsonDocument::fromJson(worker.documentData()).object();
        for (const auto& key : QStringList{"pages", "homePageId", "rootPage", "studioAppearance"})
            replacement.remove(key);
        auto document = QJsonDocument::fromJson(m_draft).object();
        if (pageId.isEmpty()) {
            for (const auto& key : QStringList{"pages", "homePageId", "rootPage", "studioAppearance"})
                if (document.contains(key))
                    replacement[key] = document[key];
            document = replacement;
        } else {
            auto pages = document["pages"].toArray();
            bool found = false;
            for (int i = 0; i < pages.size(); ++i) {
                auto entry = pages[i].toObject();
                if (entry["id"] != pageId)
                    continue;
                entry["layout"] = replacement;
                pages[i] = entry;
                found = true;
                break;
            }
            if (!found) {
                setError(tr("The page no longer exists."));
                return false;
            }
            document["pages"] = pages;
        }
        return pushEdit(QJsonDocument(document).toJson());
    }
    if (operation == "studioAppearance") {
        auto document = QJsonDocument::fromJson(m_draft).object();
        auto value = a.value("value").toMap();
        if (!value.contains("recentColors") && document["studioAppearance"].toObject().contains("recentColors"))
            value["recentColors"] = document["studioAppearance"].toObject()["recentColors"].toVariant();
        if (value.isEmpty())
            document.remove("studioAppearance");
        else
            document["studioAppearance"] = QJsonObject::fromVariantMap(value);
        return pushEdit(QJsonDocument(document).toJson());
    }
    if (QStringList{"renamePage", "duplicatePage", "reorderPage", "removePage"}.contains(operation)) {
        auto document = QJsonDocument::fromJson(m_draft).object();
        auto pages = document["pages"].toArray();
        const QString id = a.value("id").toString();
        int index = -1;
        for (int i = 0; i < pages.size(); ++i)
            if (pages[i].toObject()["id"] == id)
                index = i;
        auto rootMeta = document["rootPage"].toObject();
        if (rootMeta.isEmpty())
            rootMeta = {{"name", m_model->rootPageName()}, {"visible", true}, {"position", 0}};
        if ((id.isEmpty() && !rootMeta.value("visible").toBool()) || (!id.isEmpty() && index < 0)) {
            setError(tr("This page no longer exists."));
            return false;
        }
        QStringList order;
        for (const auto& entry : m_model->studioPages())
            order.append(entry.toMap()["id"].toString());
        const auto storeOrder = [&] {
            QJsonArray sorted;
            for (const auto& key : order)
                if (!key.isEmpty())
                    for (const auto& entry : pages)
                        if (entry.toObject()["id"] == key)
                            sorted.append(entry);
            pages = sorted;
            rootMeta["position"] = qMax(0, int(order.indexOf(QString())));
        };
        if (operation == "renamePage" || operation == "duplicatePage") {
            const auto name = a.value("name").toString().trimmed();
            if (name.isEmpty() || name.size() > 80 || name.contains(QChar::Null)) {
                setError(tr("Use a page name of 1–80 characters."));
                return false;
            }
            if (operation == "renamePage") {
                if (id.isEmpty())
                    rootMeta["name"] = name;
                else {
                    auto entry = pages[index].toObject();
                    entry["name"] = name;
                    pages[index] = entry;
                }
            } else {
                if (order.size() >= 64) {
                    setError(tr("Up to 64 pages are supported per profile."));
                    return false;
                }
                auto layout = id.isEmpty() ? document : pages[index].toObject()["layout"].toObject();
                layout.remove("pages");
                layout.remove("homePageId");
                layout.remove("rootPage");
                layout.remove("studioAppearance");
                QHash<QString, QString> copiedIds;
                QSet<QString> allocated;
                auto composition = layout["composition"].toObject();
                const auto remapIdentity = [&](const QString& old) -> QString {
                    if (copiedIds.contains(old))
                        return copiedIds[old];
                    auto fresh = allocateId(old);
                    if (fresh.isEmpty())
                        return {};
                    const auto base = fresh;
                    int suffix = 2;
                    while (allocated.contains(fresh))
                        fresh = base + QString::number(suffix++);
                    if (!ComponentSchema::safeId(fresh))
                        return {};
                    copiedIds[old] = fresh;
                    allocated.insert(fresh);
                    return fresh;
                };
                for (const auto& key : QStringList{"categories", "components"}) {
                    auto values = layout[key].toArray();
                    for (int i = 0; i < values.size(); ++i) {
                        auto entry = values[i].toObject();
                        const auto fresh = remapIdentity(entry["id"].toString());
                        if (fresh.isEmpty())
                            return false;
                        entry["id"] = fresh;
                        values[i] = entry;
                    }
                    layout[key] = values;
                }
                auto components = layout["components"].toArray();
                for (int i = 0; i < components.size(); ++i) {
                    auto item = components[i].toObject();
                    for (const auto& key : QStringList{"categoryId", "parentId"}) {
                        const auto reference = item.value(key).toString();
                        if (copiedIds.contains(reference))
                            item[key] = copiedIds[reference];
                    }
                    components[i] = item;
                }
                layout["components"] = components;
                auto tabs = composition.value("navigation").toArray();
                for (int i = 0; i < tabs.size(); ++i) {
                    auto tab = tabs[i].toObject();
                    const auto old = tab["id"].toString(), fresh = remapIdentity(old);
                    if (fresh.isEmpty())
                        return false;
                    tab["id"] = fresh;
                    auto sections = tab.value("sections").toArray();
                    for (int j = 0; j < sections.size(); ++j)
                        if (copiedIds.contains(sections[j].toString()))
                            sections[j] = copiedIds[sections[j].toString()];
                    if (tab.contains("sections"))
                        tab["sections"] = sections;
                    tabs[i] = tab;
                }
                if (composition.contains("navigation"))
                    composition["navigation"] = tabs;
                const auto initial = composition.value("initialTab").toString();
                if (copiedIds.contains(initial))
                    composition["initialTab"] = copiedIds[initial];
                layout["composition"] = composition;
                auto copyId = allocateId("page");
                if (copyId.isEmpty())
                    return false;
                const auto copyBase = copyId;
                int copySuffix = 2;
                while (allocated.contains(copyId))
                    copyId = copyBase + QString::number(copySuffix++);
                if (!ComponentSchema::safeId(copyId)) {
                    setError(tr("The duplicated page identity exceeds its limit."));
                    return false;
                }
                pages.append(QJsonObject{{"id", copyId}, {"name", name}, {"layout", layout}});
                order.insert(order.indexOf(id) + 1, copyId);
                storeOrder();
            }
        } else if (operation == "reorderPage") {
            const int from = order.indexOf(id), offset = a.value("offset").toInt(), to = from + offset;
            if ((offset != 1 && offset != -1) || to < 0 || to >= order.size()) {
                setError(tr("This page is already at the edge of the page list."));
                return false;
            }
            order.move(from, to);
            storeOrder();
        } else {
            if (document["homePageId"].toString() == id) {
                setError(tr("Set another page as Home before deleting this page."));
                return false;
            }
            const bool remap = a.contains("replacementPageId");
            const QString replacement = a.value("replacementPageId").toString();
            bool validReplacement = replacement.isEmpty() ? rootMeta.value("visible").toBool() : false;
            for (const auto& entry : pages)
                if (entry.toObject()["id"] == replacement)
                    validReplacement = true;
            if (remap && (!validReplacement || replacement == id)) {
                setError(tr("Choose another existing page for incoming links."));
                return false;
            }
            if (id.isEmpty()) {
                document["components"] = QJsonArray{};
                document["categories"] = QJsonArray{};
                document["composition"] = QJsonObject{};
                rootMeta["visible"] = false;
            } else
                pages.removeAt(index);
            document["pages"] = pages;
            int links = 0;
            bool invalidOverride = false;
            std::function<QJsonValue(QJsonValue)> repair = [&](QJsonValue value) -> QJsonValue {
                if (value.isArray()) {
                    auto values = value.toArray();
                    for (int i = 0; i < values.size(); ++i)
                        values[i] = repair(values[i]);
                    return values;
                }
                if (!value.isObject())
                    return value;
                auto object = value.toObject();
                if ((!id.isEmpty() && object.value("type") == "open-page" && object.value("pageId") == id) ||
                    (id.isEmpty() && object.value("type") == "root-page")) {
                    ++links;
                    if (remap) {
                        object["type"] = replacement.isEmpty() ? "root-page" : "open-page";
                        if (replacement.isEmpty())
                            object.remove("pageId");
                        else
                            object["pageId"] = replacement;
                    }
                } else {
                    for (auto it = object.begin(); it != object.end(); ++it) {
                        if (!id.isEmpty() && QStringList{"moviePage", "showsPage"}.contains(it.key()) &&
                            it.value() == id) {
                            ++links;
                            if (remap) {
                                if (replacement.isEmpty())
                                    invalidOverride = true;
                                else
                                    it.value() = replacement;
                            }
                        } else
                            it.value() = repair(it.value());
                    }
                }
                return object;
            };
            document = repair(document).toObject();
            if ((links && !remap) || invalidOverride) {
                setError(tr("%1 incoming links require a named replacement page before deletion.").arg(links));
                return false;
            }
            pages = document["pages"].toArray();
            order.removeAll(id);
            storeOrder();
        }
        const auto normalizeComponents = [](QJsonObject layout) {
            const auto key =
                layout.value("schemaVersion").toInt() == 1 ? QStringLiteral("items") : QStringLiteral("components");
            auto components = layout.value(key).toArray();
            for (int i = 0; i < components.size(); ++i)
                components[i] = QJsonObject::fromVariantMap(
                    ComponentSchema::component(components[i].toObject().toVariantMap(), true));
            layout[key] = components;
            return layout;
        };
        document = normalizeComponents(document);
        for (int i = 0; i < pages.size(); ++i) {
            auto entry = pages[i].toObject();
            entry["layout"] = normalizeComponents(entry["layout"].toObject());
            pages[i] = entry;
        }
        document["pages"] = pages;
        document["rootPage"] = rootMeta;
        return pushEdit(QJsonDocument(document).toJson());
    }
    if (operation == "setHomePage") {
        auto document = QJsonDocument::fromJson(m_draft).object();
        const auto id = a.value("id").toString();
        bool found = id.isEmpty() && m_model->rootPageVisible();
        for (const auto& entry : document["pages"].toArray())
            if (entry.toObject()["id"] == id)
                found = true;
        if (!found) {
            setError(QStringLiteral("Choose an existing page as Home."));
            return false;
        }
        document["homePageId"] = id;
        return pushEdit(QJsonDocument(document).toJson());
    }
    if (operation == "addResolved" || operation == "addPlaced") {
        if (a.contains("enableScrolling") && a.value("enableScrolling").metaType().id() != QMetaType::Bool) {
            setError(tr("Choose whether this page scrolls."));
            return false;
        }
        const auto proposal = additionProposal(a.value("type").toString(),
                                               a.value("placement").toMap(),
                                               a.value("strategy", "cancel").toString(),
                                               pageId,
                                               a.value("categoryId").toString(),
                                               a.value("parentId").toString(),
                                               a.value("configuration").toMap(),
                                               a.value("enableScrolling", false).toBool());
        if (!proposal.value("valid").toBool()) {
            setError(proposal.value("reason").toString());
            return false;
        }
        return pushEdit(QJsonDocument(QJsonObject::fromVariantMap(proposal.value("document").toMap())).toJson());
    }
    if (operation == "batch") {
        const auto commands = a.value("commands").toList();
        if (commands.isEmpty() || commands.size() > 32) {
            setError(QStringLiteral("Choose between 1 and 32 grouped edits."));
            return false;
        }
        HomeLayout temporary(m_live->registry());
        configureWorker(temporary);
        if (!temporary.loadDocument(m_draft, false)) {
            setError(temporary.errorString());
            return false;
        }
        StudioSession transaction;
        if (!transaction.bind(&temporary)) {
            setError(transaction.errorString());
            return false;
        }
        for (const auto& entry : commands) {
            const auto command = entry.toMap();
            const auto op = command.value("operation").toString();
            if (op == "batch" || !transaction.execute(op,
                                                      command.value("arguments").toMap(),
                                                      command.value("pageId", pageId).toString())) {
                setError(op == "batch" ? QStringLiteral("Nested batches are not supported.")
                                       : transaction.errorString());
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
        for (int i = 0; i < pages.size(); ++i)
            if (pages[i].toObject()["id"] == pageId)
                pageIndex = i;
        if (pageIndex < 0) {
            setError(QStringLiteral("That page no longer exists."));
            return false;
        }
        document = pages[pageIndex].toObject()["layout"].toObject();
    }
    if (pageIndex >= 0 && root["composition"].toObject().contains("designLibrary")) {
        auto composition = document["composition"].toObject();
        composition["designLibrary"] = root["composition"].toObject()["designLibrary"];
        document["composition"] = composition;
    }
    HomeLayout worker(m_live->registry());
    configureWorker(worker);
    if (!worker.loadDocument(QJsonDocument(document).toJson(), false)) {
        setError(worker.errorString());
        return false;
    }
    const auto id = a.value("id").toString();
    bool ok = false;
    if (operation == "repairSource") {
        ok = worker.rebindSource(id, a.value("sourceId").toString());
    } else if (operation == "assignSection") {
        ok = worker.assignItemCategory(id, a.value("categoryId").toString());
    } else if (operation == "navigation") {
        auto composition = document["composition"].toObject();
        auto tabs = composition["navigation"].toArray();
        const auto kind = a.value("kind").toString();
        int index = -1;
        for (int i = 0; i < tabs.size(); ++i)
            if (tabs[i].toObject()["id"] == id)
                index = i;
        if (kind == "add") {
            if (tabs.size() >= 64) {
                setError(tr("Use at most 64 navigation tabs."));
                return false;
            }
            const auto identity = allocateId("tab");
            if (identity.isEmpty())
                return false;
            tabs.append(QJsonObject{{"id", identity}, {"name", a.value("name").toString()}});
        } else {
            if (index < 0) {
                setError(tr("Choose an existing navigation tab."));
                return false;
            }
            if (kind == "remove")
                tabs.removeAt(index);
            else if (kind == "move") {
                const int offset = a.value("offset").toInt(), target = index + offset;
                if ((offset != 1 && offset != -1) || target < 0 || target >= tabs.size()) {
                    setError(tr("The tab is already at that edge."));
                    return false;
                }
                const QJsonValue entry = tabs.at(index);
                tabs.removeAt(index);
                tabs.insert(target, entry);
            } else if (kind == "patch") {
                auto tab = tabs[index].toObject();
                const auto values = a.value("value").toMap();
                if (values.contains("action") && !validNewAction(values.value("action").toMap()))
                    return false;
                for (auto it = values.cbegin(); it != values.cend(); ++it) {
                    if (!QStringList{"name", "action", "sections"}.contains(it.key())) {
                        setError(tr("Unsupported navigation property."));
                        return false;
                    }
                    if (it.value().isNull() || (it.key() == "action" && it.value().toMap().isEmpty()))
                        tab.remove(it.key());
                    else
                        tab[it.key()] = QJsonValue::fromVariant(it.value());
                }
                tabs[index] = tab;
            } else {
                setError(tr("Choose a supported navigation operation."));
                return false;
            }
        }
        composition["navigation"] = tabs;
        if (composition.contains("initialTab")) {
            bool present = false;
            for (const auto& tab : tabs)
                if (tab.toObject()["id"] == composition["initialTab"])
                    present = true;
            if (!present) {
                if (tabs.isEmpty())
                    composition.remove("initialTab");
                else
                    composition["initialTab"] = tabs.first().toObject()["id"];
            }
        }
        document["composition"] = composition;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "pagePatch") {
        const auto scope = a.value("scope").toString();
        if (!QStringList{"theme", "composition"}.contains(scope)) {
            setError(tr("Choose a page property group."));
            return false;
        }
        const auto values = a.value("value").toMap();
        auto nested = document[scope].toObject();
        if (scope == "composition" && nested.isEmpty())
            nested = {{"schemaVersion", 1}, {"enabled", false}};
        if (scope == "composition")
            for (const auto& key : QStringList{"moviePage", "showsPage"})
                if (values.contains(key) && !values.value(key).isNull() &&
                    !validNewAction({{"type", "open-page"}, {"pageId", values.value(key)}}))
                    return false;
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            if (it.value().isNull())
                nested.remove(it.key());
            else
                nested[it.key()] = QJsonValue::fromVariant(it.value());
        }
        if (scope == "composition" && !ComponentSchema::validComposition(nested.toVariantMap())) {
            setError(tr("Page composition is invalid. Nothing changed."));
            return false;
        }
        document[scope] = nested;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "scrolling") {
        if (a.value("enabled").metaType().id() != QMetaType::Bool) {
            setError(tr("Choose whether this page scrolls."));
            return false;
        }
        auto composition = document["composition"].toObject();
        if (!composition.value("enabled").toBool() || !composition.contains("grid") ||
            ComponentSchema::normalizedGrid(composition["grid"].toObject().toVariantMap()).value("mode") != "screen") {
            setError(tr("This layout uses scrolling sections or automatic sizing. Preview a page grid before changing "
                        "this option."));
            return false;
        }
        composition["verticalScrolling"] = a.value("enabled").toBool();
        document["composition"] = composition;
        if (!a.value("enabled").toBool() && !ComponentSchema::layoutPlacementsFit(document["components"].toArray(),
                                                                                  composition.toVariantMap(),
                                                                                  document["categories"].toArray())) {
            setError(tr("Components extend below the first screen. Move or resize them before turning scrolling off."));
            return false;
        }
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "setApps") {
        if (a.contains("mode") && !QStringList{"all", "selected"}.contains(a.value("mode").toString())) {
            setError(tr("Choose All installed or Selected apps."));
            return false;
        }
        auto entries = document["components"].toArray();
        int rootIndex = -1;
        QSet<QString> removed;
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["id"] == id && entries[i].toObject()["family"] == "app-bar")
                rootIndex = i;
        if (rootIndex < 0 || ComponentSchema::positionLocked(entries, id)) {
            setError(tr("Choose an unlocked app bar."));
            return false;
        }
        removed.insert(id);
        for (int depth = 0; depth < 4; ++depth)
            for (const auto& entry : entries)
                if (removed.contains(entry.toObject()["parentId"].toString()))
                    removed.insert(entry.toObject()["id"].toString());
        removed.remove(id);
        for (const auto& entry : entries)
            if (removed.contains(entry.toObject()["id"].toString()) && entry.toObject().value("locked").toBool()) {
                setError(tr("Unlock the app bar's contents before replacing them."));
                return false;
            }
        const bool all = a.value("mode") == "all";
        const auto selected = a.value("apps").toList();
        if (!all && (a.value("apps").metaType().id() != QMetaType::QVariantList || selected.size() > 128)) {
            setError(tr("Choose up to 128 apps."));
            return false;
        }
        const auto previous = entries[rootIndex].toObject()["appCollection"].toObject()["ids"].toArray();
        QSet<QString> seen;
        for (const auto& value : selected) {
            const auto appId = value.toString();
            const auto* app = m_live->registry()->find(appId);
            if (value.metaType().id() != QMetaType::QString || !ComponentSchema::safeId(appId) ||
                seen.contains(appId) ||
                (app ? !QStringList{"desktop", "web", "android"}.contains(app->type)
                     : !previous.contains(QJsonValue(appId)))) {
                setError(tr("Choose distinct installed apps, or retain a missing app already in this collection."));
                return false;
            }
            seen.insert(appId);
        }
        QJsonArray result;
        QJsonObject membership{{"mode", all ? "all" : "selected"},
                               {"ids", all ? QJsonArray{} : QJsonArray::fromVariantList(selected)}};
        const auto group = entries[rootIndex].toObject()["appCollection"].toObject()["section"];
        if (group.isString())
            membership["section"] = group;
        const auto showStore = entries[rootIndex].toObject()["appCollection"].toObject()["showStore"];
        if (showStore.isBool())
            membership["showStore"] = showStore;
        for (const auto& entry : entries)
            if (!removed.contains(entry.toObject()["id"].toString())) {
                auto item = entry.toObject();
                if (item["id"] == id)
                    item["appCollection"] = membership;
                result.append(item);
            }
        document["components"] = result;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "patch") {
        auto ids = a.value("ids").toList();
        if (ids.isEmpty() && !id.isEmpty())
            ids.append(id);
        const auto values = a.value("value").toMap();
        QSet<QString> targets;
        if (ids.isEmpty() || ids.size() > 32 || values.isEmpty()) {
            setError(tr("Choose 1–32 components and a property to edit."));
            return false;
        }
        for (const auto& value : ids) {
            if (value.metaType().id() != QMetaType::QString || targets.contains(value.toString())) {
                setError(tr("Choose distinct component identities."));
                return false;
            }
            targets.insert(value.toString());
        }
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            if (!QStringList{"name",
                             "opacity",
                             "showTitle",
                             "showType",
                             "showLogo",
                             "logoFill",
                             "symbol",
                             "accent",
                             "action",
                             "customLogo",
                             "presentation",
                             "accessibility",
                             "size"}
                     .contains(it.key())) {
                setError(tr("Unsupported inspector property."));
                return false;
            }
        if (values.contains("action") && !validNewAction(values.value("action").toMap()))
            return false;
        auto entries = document["components"].toArray();
        int found = 0;
        for (int i = 0; i < entries.size(); ++i) {
            auto item = entries[i].toObject();
            if (!targets.contains(item["id"].toString()))
                continue;
            ++found;
            for (auto it = values.cbegin(); it != values.cend(); ++it) {
                const auto key = it.key();
                if (key == "presentation" || key == "accessibility") {
                    if (it.value().metaType().id() != QMetaType::QVariantMap) {
                        setError(tr("Choose valid property values."));
                        return false;
                    }
                    auto nested = item[key].toObject();
                    const auto changes = it.value().toMap();
                    for (auto v = changes.cbegin(); v != changes.cend(); ++v) {
                        if (v.value().isNull())
                            nested.remove(v.key());
                        else
                            nested[v.key()] = QJsonValue::fromVariant(v.value());
                    }
                    item[key] = nested;
                } else if (it.value().isNull() ||
                           (key == "action" && it.value().metaType().id() == QMetaType::QVariantMap &&
                            it.value().toMap().isEmpty()))
                    item.remove(key);
                else
                    item[key] = QJsonValue::fromVariant(it.value());
            }
            if (values.contains("action") &&
                (item["componentType"] == "launcher" || item["componentType"] == "media-shelf")) {
                item["kind"] = "widget";
                item["widgetType"] = "button";
                item["componentType"] = "button";
                item.remove("appId");
            }
            QString error;
            const auto canonical = ComponentSchema::component(item.toVariantMap(), true, &error);
            if (canonical.isEmpty()) {
                setError(error);
                return false;
            }
            entries[i] = QJsonObject::fromVariantMap(canonical);
        }
        if (found != targets.size()) {
            setError(tr("A selected component no longer exists."));
            return false;
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "section") {
        const auto kind = a.value("kind").toString();
        if (kind == "add")
            ok = !worker.addCategory(a.value("name").toString()).isEmpty();
        else if (kind == "rename")
            ok = worker.renameCategory(id, a.value("name").toString());
        else if (kind == "remove")
            ok = worker.removeCategory(id);
        else if (kind == "move")
            ok = worker.moveCategory(id, a.value("offset").toInt());
        else if (kind == "presentation") {
            const auto style = a.value("value").toMap();
            if (style.contains("trailingAction") && !validNewAction(style.value("trailingAction").toMap()))
                return false;
            ok = worker.setSectionPresentation(id, style);
        }
    } else if (operation == "brand")
        ok = worker.setBrandVisible(a.value("value").toBool());
    else if (operation == "arrange") {
        const auto selection = a.value("ids").toList();
        const auto mode = a.value("mode").toString();
        if (selection.size() < 2 || selection.size() > 32 ||
            !QStringList{"left", "center-x", "right", "top", "center-y", "bottom", "distribute-x", "distribute-y"}
                 .contains(mode)) {
            setError(tr("Choose an alignment and between 2 and 32 components."));
            return false;
        }
        auto entries = document["components"].toArray();
        QList<int> indices;
        QSet<QString> ids;
        QString parent, category;
        int low = 16, high = 0, total = 0;
        const bool horizontal = QStringList{"left", "center-x", "right", "distribute-x"}.contains(mode);
        const QString axis = horizontal ? "x" : "y", span = horizontal ? "width" : "height";
        for (const auto& value : selection) {
            const auto identity = value.toString();
            if (value.metaType().id() != QMetaType::QString || ids.contains(identity)) {
                setError(tr("Choose distinct component identities."));
                return false;
            }
            ids.insert(identity);
        }
        for (int i = 0; i < entries.size(); ++i) {
            const auto item = entries[i].toObject();
            if (!ids.contains(item["id"].toString()))
                continue;
            if (ComponentSchema::positionLocked(entries, item["id"].toString()) || !item.contains("placement")) {
                setError(tr("Unlock the selected components and choose a grid layout."));
                return false;
            }
            if (indices.isEmpty()) {
                parent = item.value("parentId").toString();
                category = item.value("categoryId").toString();
            }
            if (parent != item.value("parentId").toString() || category != item.value("categoryId").toString()) {
                setError(tr("Arrange components in the same group or section."));
                return false;
            }
            indices.append(i);
            const auto p = item["placement"].toObject();
            low = qMin(low, p[axis].toInt());
            high = qMax(high, p[axis].toInt() + p[span].toInt());
            total += p[span].toInt();
        }
        if (indices.size() != ids.size()) {
            setError(tr("A selected component no longer exists."));
            return false;
        }
        const bool distribute = mode.startsWith("distribute");
        int gap = 0;
        if (distribute) {
            const int space = high - low - total;
            if (space < 0 || space % (indices.size() - 1)) {
                setError(tr("Equal spacing requires whole cells. Adjust the outer positions first."));
                return false;
            }
            gap = space / (indices.size() - 1);
            std::stable_sort(indices.begin(), indices.end(), [&](int x, int y) {
                return entries[x].toObject()["placement"].toObject()[axis].toInt() <
                       entries[y].toObject()["placement"].toObject()[axis].toInt();
            });
        }
        int cursor = low;
        for (int index : indices) {
            auto item = entries[index].toObject();
            auto p = item["placement"].toObject();
            const int size = p[span].toInt();
            int position = low;
            if (distribute) {
                position = cursor;
                cursor += size + gap;
            } else if (mode.startsWith("center")) {
                if ((low + high - size) % 2) {
                    setError(
                        tr("Center alignment falls between grid cells. Adjust the sizes or outer positions first."));
                    return false;
                }
                position = (low + high - size) / 2;
            } else if (mode == "right" || mode == "bottom")
                position = high - size;
            p[axis] = position;
            item["placement"] = p;
            entries[index] = item;
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "group") {
        const auto selection = a.value("ids").toList();
        QSet<QString> ids;
        if (selection.size() < 2 || selection.size() > 32) {
            setError(tr("Select between 2 and 32 components to group."));
            return false;
        }
        for (const auto& value : selection) {
            if (value.metaType().id() != QMetaType::QString || ids.contains(value.toString())) {
                setError(tr("Choose distinct component identities."));
                return false;
            }
            ids.insert(value.toString());
        }
        auto entries = document["components"].toArray();
        QString parent, category;
        bool first = true;
        int left = 16, top = 10, right = 0, bottom = 0, found = 0;
        for (const auto& value : entries) {
            const auto item = value.toObject();
            if (!ids.contains(item["id"].toString()))
                continue;
            if (ComponentSchema::positionLocked(entries, item["id"].toString()) || !item.contains("placement")) {
                setError(tr("Unlock the selected components and use a page grid before grouping."));
                return false;
            }
            if (first) {
                parent = item.value("parentId").toString();
                category = item.value("categoryId").toString();
                first = false;
            }
            if (item.value("parentId").toString() != parent || item.value("categoryId").toString() != category) {
                setError(tr("Select components in the same group or section."));
                return false;
            }
            const auto p = item["placement"].toObject();
            left = qMin(left, p["x"].toInt());
            top = qMin(top, p["y"].toInt());
            right = qMax(right, p["x"].toInt() + p["width"].toInt());
            bottom = qMax(bottom, p["y"].toInt() + p["height"].toInt());
            ++found;
        }
        if (found != ids.size()) {
            setError(tr("A selected component no longer exists."));
            return false;
        }
        const QVariantMap bounds{{"x", left}, {"y", top}, {"width", right - left}, {"height", bottom - top}};
        for (const auto& value : entries) {
            const auto item = value.toObject();
            if (ids.contains(item["id"].toString()) || item.value("parentId").toString() != parent ||
                item.value("categoryId").toString() != category)
                continue;
            if (ComponentSchema::placementsOverlap(bounds, item["placement"].toObject().toVariantMap())) {
                setError(tr("The group bounds include another component. Select it too or move it first."));
                return false;
            }
        }
        const auto prototype = componentPrototype("container");
        if (prototype.isEmpty())
            return false;
        auto group = QJsonObject::fromVariantMap(prototype.value("root").toMap());
        const auto identity = group["id"].toString();
        group["placement"] = QJsonObject::fromVariantMap(bounds);
        if (!parent.isEmpty())
            group["parentId"] = parent;
        if (!category.isEmpty())
            group["categoryId"] = category;
        auto presentation = group["presentation"].toObject();
        auto layout = presentation["layout"].toObject();
        layout["columns"] = right - left;
        layout["rows"] = bottom - top;
        layout["padding"] = 0;
        layout["gap"] = ComponentSchema::componentGrid(entries,
                                                       document["composition"].toObject().toVariantMap(),
                                                       document["categories"].toArray(),
                                                       parent,
                                                       category)
                            .value("gap")
                            .toDouble();
        presentation["layout"] = layout;
        presentation["showHeading"] = false;
        group["presentation"] = presentation;
        QJsonArray result;
        bool inserted = false;
        for (const auto& value : entries) {
            auto item = value.toObject();
            if (ids.contains(item["id"].toString())) {
                if (!inserted) {
                    result.append(group);
                    inserted = true;
                }
                auto placement = item["placement"].toObject();
                placement["x"] = placement["x"].toInt() - left;
                placement["y"] = placement["y"].toInt() - top;
                item["placement"] = placement;
                item["parentId"] = identity;
                item.remove("categoryId");
            }
            result.append(item);
        }
        document["components"] = result;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "ungroup") {
        auto entries = document["components"].toArray();
        QJsonObject group;
        for (const auto& value : entries)
            if (value.toObject()["id"] == id)
                group = value.toObject();
        if (group["componentType"] != "container" || ComponentSchema::positionLocked(entries, id)) {
            setError(tr("Choose an unlocked group to ungroup."));
            return false;
        }
        const auto parent = group.value("parentId").toString(), category = group.value("categoryId").toString();
        const auto outer = group["placement"].toObject(), style = group["presentation"].toObject(),
                   inner = style["layout"].toObject();
        const auto parentGrid = ComponentSchema::componentGrid(entries,
                                                               document["composition"].toObject().toVariantMap(),
                                                               document["categories"].toArray(),
                                                               parent,
                                                               category);
        if (inner["columns"].toInt() != outer["width"].toInt() || inner["rows"].toInt() != outer["height"].toInt() ||
            inner["padding"].toDouble() != 0 || inner["gap"].toDouble() != parentGrid.value("gap").toDouble() ||
            style.value("showHeading").toBool()) {
            setError(tr("This group's internal grid differs from its parent. Move its children out explicitly to "
                        "preserve their sizes."));
            return false;
        }
        QJsonArray result;
        for (const auto& value : entries) {
            auto item = value.toObject();
            if (item["id"] == id)
                continue;
            if (item.value("parentId").toString() == id) {
                if (ComponentSchema::positionLocked(entries, item["id"].toString())) {
                    setError(tr("Unlock the group's contents before ungrouping."));
                    return false;
                }
                auto placement = item["placement"].toObject();
                placement["x"] = placement["x"].toInt() + outer["x"].toInt();
                placement["y"] = placement["y"].toInt() + outer["y"].toInt();
                item["placement"] = placement;
                if (parent.isEmpty())
                    item.remove("parentId");
                else
                    item["parentId"] = parent;
                if (category.isEmpty())
                    item.remove("categoryId");
                else
                    item["categoryId"] = category;
            }
            result.append(item);
        }
        document["components"] = result;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "childLayout") {
        const auto layout = a.value("value").toMap();
        if (!ComponentSchema::validChildLayout(layout)) {
            setError(tr("Choose a bounded group layout."));
            return false;
        }
        auto entries = document["components"].toArray();
        bool found = false;
        if (ComponentSchema::positionLocked(entries, id)) {
            setError(tr("Unlock this group before arranging it."));
            return false;
        }
        for (int i = 0; i < entries.size(); ++i) {
            auto item = entries[i].toObject();
            if (item["id"] == id && item["componentType"] == "container") {
                auto style = item["presentation"].toObject();
                style["layout"] = QJsonObject::fromVariantMap(layout);
                item["presentation"] = style;
                entries[i] = item;
                found = true;
            }
        }
        if (!found) {
            setError(tr("Choose an existing group."));
            return false;
        }
        if (layout.value("mode") != "grid") {
            int cursor = 0;
            const bool row = layout.value("mode") == "row";
            for (int i = 0; i < entries.size(); ++i) {
                auto item = entries[i].toObject();
                if (item.value("parentId").toString() != id)
                    continue;
                if (ComponentSchema::positionLocked(entries, item["id"].toString())) {
                    setError(tr("Unlock the group's contents before arranging them."));
                    return false;
                }
                auto p = item["placement"].toObject();
                p["x"] = row ? cursor : 0;
                p["y"] = row ? 0 : cursor;
                cursor += p[row ? "width" : "height"].toInt();
                item["placement"] = p;
                entries[i] = item;
            }
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "place") {
        auto entries = document["components"].toArray();
        const auto changes = a.value("placements").toList();
        QSet<QString> touched;
        if (changes.isEmpty() || changes.size() > 64) {
            setError(QStringLiteral("Choose between 1 and 64 placements."));
            return false;
        }
        for (const auto& value : changes) {
            const auto change = value.toMap();
            const auto target = change.value("id").toString();
            bool found = false;
            if (touched.contains(target)) {
                setError(QStringLiteral("Duplicate placement identity."));
                return false;
            }
            for (int i = 0; i < entries.size(); ++i) {
                auto entry = entries[i].toObject();
                if (entry["id"] != target)
                    continue;
                if (change.contains("parentId") && change.value("parentId").metaType().id() != QMetaType::QString) {
                    setError(tr("Choose a named destination group."));
                    return false;
                }
                const auto parent = change.value("parentId", entry.value("parentId").toString()).toString();
                if (!parent.isEmpty() && ComponentSchema::positionLocked(worker.m_items, parent)) {
                    setError(tr("Choose an unlocked destination group."));
                    return false;
                }
                if (change.contains("categoryId") && change.value("categoryId").metaType().id() != QMetaType::QString) {
                    setError(tr("Choose a named destination section."));
                    return false;
                }
                if (change.contains("categoryId") &&
                    entry.value("categoryId").toString() != change.value("categoryId").toString()) {
                    const auto destination = change.value("categoryId").toString();
                    bool eligible = destination.isEmpty();
                    for (const auto& value : worker.m_categories) {
                        const auto category = value.toObject();
                        if (category["id"] == destination)
                            eligible = category["presentation"].toObject().value("variant") != "library";
                    }
                    if ((!destination.isEmpty() && worker.grid().value("mode") != "sections") || !eligible) {
                        setError(tr("Choose an existing component section in a scrolling layout."));
                        return false;
                    }
                }
                if (ComponentSchema::positionLocked(worker.m_items, target) &&
                    (entry["placement"].toObject() != QJsonObject::fromVariantMap(change.value("placement").toMap()) ||
                     parent != entry.value("parentId").toString() ||
                     (change.contains("categoryId") &&
                      entry.value("categoryId").toString() != change.value("categoryId").toString()))) {
                    setError(tr("Unlock the component and its containing groups before moving or resizing it."));
                    return false;
                }
                entry["placement"] = QJsonObject::fromVariantMap(change.value("placement").toMap());
                if (change.contains("categoryId")) {
                    const auto category = change.value("categoryId").toString();
                    if (category.isEmpty())
                        entry.remove("categoryId");
                    else
                        entry["categoryId"] = category;
                }
                if (parent.isEmpty())
                    entry.remove("parentId");
                else
                    entry["parentId"] = parent;
                entries[i] = entry;
                found = true;
            }
            if (!found) {
                setError(QStringLiteral("The component no longer exists."));
                return false;
            }
            touched.insert(target);
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "insertRelative") {
        const auto relative = a.value("relativeId").toString();
        if (a.value("before").metaType().id() != QMetaType::Bool || relative == id) {
            setError(tr("Choose another component and an insertion direction."));
            return false;
        }
        auto entries = document["components"].toArray();
        QString parent;
        int selected = -1;
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["id"] == id) {
                selected = i;
                parent = entries[i].toObject()["parentId"].toString();
            }
        QJsonObject group;
        for (const auto& entry : entries)
            if (entry.toObject()["id"] == parent)
                group = entry.toObject();
        const auto layout = group["presentation"].toObject()["layout"].toObject();
        const auto mode = layout["mode"].toString();
        if (selected < 0 || parent.isEmpty() || group["componentType"] != "container" ||
            group.contains("appCollection") || (mode != "row" && mode != "column")) {
            setError(tr("Choose an item in an editable row or column."));
            return false;
        }
        const QString axis = mode == "row" ? "x" : "y", span = mode == "row" ? "width" : "height";
        QList<int> order;
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["parentId"] == parent)
                order.append(i);
        if (order.size() > 64) {
            setError(tr("An insertion can affect at most 64 components."));
            return false;
        }
        std::sort(order.begin(), order.end(), [&](int left, int right) {
            const auto l = entries[left].toObject(), r = entries[right].toObject();
            const auto lp = l["placement"].toObject()[axis].toInt(), rp = r["placement"].toObject()[axis].toInt();
            return lp != rp ? lp < rp : l["id"].toString() < r["id"].toString();
        });
        QList<int> gaps;
        int from = -1, to = -1;
        for (int i = 0; i < order.size(); ++i) {
            const auto node = entries[order[i]].toObject(), p = node["placement"].toObject();
            if (node["id"] == id)
                from = i;
            if (node["id"] == relative)
                to = i;
            if (i + 1 < order.size())
                gaps.append(entries[order[i + 1]].toObject()["placement"].toObject()[axis].toInt() - p[axis].toInt() -
                            p[span].toInt());
        }
        if (from < 0 || to < 0) {
            setError(tr("Choose an insertion target in the same group."));
            return false;
        }
        int cursor = entries[order.first()].toObject()["placement"].toObject()[axis].toInt();
        const int moving = order.takeAt(from);
        if (from < to)
            --to;
        order.insert(to + (a.value("before").toBool() ? 0 : 1), moving);
        for (int i = 0; i < order.size(); ++i) {
            auto node = entries[order[i]].toObject(), p = node["placement"].toObject();
            if (p[axis].toInt() != cursor && ComponentSchema::positionLocked(entries, node["id"].toString())) {
                setError(tr("A locked component blocks this insertion."));
                return false;
            }
            p[axis] = cursor;
            node["placement"] = p;
            entries[order[i]] = node;
            cursor += p[span].toInt() + (i < gaps.size() ? gaps[i] : 0);
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "orderRelative") {
        const auto relative = a.value("relativeId").toString();
        if (a.value("before").metaType().id() != QMetaType::Bool || relative == id) {
            setError(tr("Choose another component and an ordering direction."));
            return false;
        }
        auto entries = document["components"].toArray();
        int from = -1, to = -1;
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].toObject()["id"] == id)
                from = i;
            if (entries[i].toObject()["id"] == relative)
                to = i;
        }
        if (from < 0 || to < 0 ||
            entries[from].toObject().value("categoryId") != entries[to].toObject().value("categoryId") ||
            entries[from].toObject().value("parentId") != entries[to].toObject().value("parentId")) {
            setError(tr("Choose a component in the same section or group."));
            return false;
        }
        const QJsonValue moving = entries.at(from);
        for (int i = qMin(from, to); i <= qMax(from, to); ++i)
            if (entries[i].toObject().value("locked").toBool()) {
                setError(tr("Unlock the affected components before changing their order."));
                return false;
            }
        entries.removeAt(from);
        if (from < to)
            --to;
        entries.insert(to + (a.value("before").toBool() ? 0 : 1), moving);
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "convertGrid") {
        for (const auto& entry : worker.m_items)
            if (entry.toObject()["componentType"] == "container") {
                setError(tr("This page already contains groups. Edit its grid without converting its components."));
                return false;
            }
        const auto grid = ComponentSchema::normalizedGrid(a.value("value").toMap());
        if (!ComponentSchema::validGrid(grid)) {
            setError(QStringLiteral("The chosen grid cannot contain this page. Choose a larger grid or cancel."));
            return false;
        }
        auto composition = worker.m_composition;
        composition["schemaVersion"] = 1;
        composition["enabled"] = true;
        composition["grid"] = grid;
        if (grid.value("mode") == "sections") {
            composition["columns"] = 1;
            composition["fit"] = false;
            composition["sectionHeight"] = a.value("sectionHeight", m_live->studioOptions().value("sectionHeight"));
            for (int i = 0; i < worker.m_categories.size(); ++i) {
                auto category = worker.m_categories[i].toObject();
                auto style = category["presentation"].toObject();
                if (style.value("variant") != "library") {
                    style["height"] = QJsonValue::fromVariant(composition["sectionHeight"]);
                    category["presentation"] = style;
                    worker.m_categories[i] = category;
                }
            }
        } else {
            for (const auto& entry : worker.m_categories) {
                const auto style = entry.toObject()["presentation"].toObject();
                if (style["variant"] == "library" || style.contains("trailingAction")) {
                    setError(QStringLiteral(
                        "Choose scrolling sections to preserve this page's library or section actions."));
                    return false;
                }
            }
        }
        worker.m_composition = composition;
        QHash<QString, int> nextCell;
        for (int i = 0; i < worker.m_items.size(); ++i) {
            auto item = worker.m_items[i].toObject();
            const auto category = grid.value("mode") == "sections" ? item["categoryId"].toString() : QString();
            const auto local = ComponentSchema::sectionGrid(composition, worker.m_categories, category);
            const int columns = local["columns"].toInt(), rows = local["rows"].toInt(), cell = nextCell[category]++;
            if (cell >= columns * rows) {
                setError(
                    QStringLiteral("A section cannot contain its components in the chosen grid. Nothing changed."));
                return false;
            }
            item["placement"] = QJsonObject{{"x", cell % columns}, {"y", cell / columns}, {"width", 1}, {"height", 1}};
            worker.m_items[i] = item;
        }
        ok = worker.save();
    } else if (operation == "state") {
        const auto key = a.value("key").toString();
        if (!QStringList{"hidden", "locked"}.contains(key) || a.value("value").metaType().id() != QMetaType::Bool) {
            setError(tr("Choose a valid visibility or lock state."));
            return false;
        }
        auto entries = document["components"].toArray();
        bool found = false;
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["id"] == id) {
                auto item = entries[i].toObject();
                item[key] = QJsonValue::fromVariant(a.value("value"));
                entries[i] = item;
                found = true;
            }
        if (!found) {
            setError(tr("Choose an existing component."));
            return false;
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "rename")
        ok = worker.setItemName(id, a.value("name").toString());
    else if (operation == "remove") {
        auto entries = document["components"].toArray();
        QSet<QString> removed{id};
        bool found = false;
        for (const auto& entry : entries)
            if (entry.toObject()["id"] == id)
                found = true;
        if (!found || ComponentSchema::positionLocked(entries, id)) {
            setError(tr("Choose an unlocked component to remove."));
            return false;
        }
        for (int depth = 0; depth < 4; ++depth)
            for (const auto& entry : entries)
                if (removed.contains(entry.toObject()["parentId"].toString()))
                    removed.insert(entry.toObject()["id"].toString());
        for (const auto& entry : entries)
            if (removed.contains(entry.toObject()["id"].toString()) && entry.toObject().value("locked").toBool()) {
                setError(tr("Unlock the group's contents before removing it."));
                return false;
            }
        QJsonArray result;
        for (const auto& entry : entries)
            if (!removed.contains(entry.toObject()["id"].toString()))
                result.append(entry);
        document["components"] = result;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "duplicate") {
        auto entries = document["components"].toArray();
        QJsonObject copy;
        for (const auto& entry : entries)
            if (entry.toObject()["id"] == id)
                copy = entry.toObject();
        if (copy.isEmpty()) {
            setError(tr("Choose an existing component to duplicate."));
            return false;
        }
        const auto identity = allocateId(id + "-copy");
        if (identity.isEmpty())
            return false;
        copy["id"] = identity;
        if (worker.m_composition.contains("grid")) {
            auto shape = copy["placement"].toObject().toVariantMap();
            if (shape.isEmpty())
                shape = {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}};
            const auto category = copy.value("categoryId").toString(), parent = copy.value("parentId").toString();
            const bool sections = worker.grid().value("mode") == "sections";
            if (!parent.isEmpty() && ComponentSchema::positionLocked(entries, parent)) {
                setError(tr("Unlock the containing group before adding a duplicate."));
                return false;
            }
            const auto grid =
                ComponentSchema::componentGrid(entries, worker.m_composition, worker.m_categories, parent, category);
            bool free = false;
            for (int row = 0; row < grid["rows"].toInt() && !free; ++row)
                for (int col = 0; col < grid["columns"].toInt() && !free; ++col) {
                    shape["x"] = col;
                    shape["y"] = row;
                    if (!ComponentSchema::validPlacement(shape, grid["columns"].toInt(), grid["rows"].toInt()))
                        continue;
                    free = true;
                    for (const auto& entry : entries) {
                        const auto item = entry.toObject();
                        if (item.value("parentId").toString() == parent &&
                            (!sections || !parent.isEmpty() || item.value("categoryId").toString() == category) &&
                            item.contains("placement") &&
                            ComponentSchema::placementsOverlap(item["placement"].toObject().toVariantMap(), shape)) {
                            free = false;
                            break;
                        }
                    }
                }
            if (!free) {
                setError(tr("There is no free region for the duplicate. Nothing changed."));
                return false;
            }
            copy["placement"] = QJsonObject::fromVariantMap(shape);
        }
        QHash<QString, QString> remap{{id, identity}};
        QSet<QString> allocated{identity};
        for (int depth = 0; depth < 4; ++depth)
            for (const auto& entry : entries) {
                const auto item = entry.toObject();
                const auto child = item["id"].toString();
                if (remap.contains(child) || !remap.contains(item["parentId"].toString()))
                    continue;
                auto fresh = allocateId(child + "-copy");
                if (fresh.isEmpty())
                    return false;
                const auto base = fresh;
                int suffix = 2;
                while (allocated.contains(fresh))
                    fresh = base + QString::number(suffix++);
                if (!ComponentSchema::safeId(fresh)) {
                    setError(tr("The copied identity exceeds its limit."));
                    return false;
                }
                allocated.insert(fresh);
                remap[child] = fresh;
            }
        QJsonArray copies;
        copies.append(copy);
        for (const auto& entry : entries) {
            auto item = entry.toObject();
            const auto child = item["id"].toString();
            if (child == id || !remap.contains(child))
                continue;
            item["id"] = remap[child];
            item["parentId"] = remap[item["parentId"].toString()];
            copies.append(item);
        }
        for (const auto& entry : copies)
            entries.append(entry);
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if (operation == "reorder")
        ok = worker.moveItem(id, a.value("offset").toInt());
    else if (operation == "presentation")
        ok = worker.setItemPresentation(id, a.value("value").toMap());
    else if (operation == "appearance")
        ok = worker.setAppearance(a.value("value").toMap());
    else if (operation == "composition")
        ok = worker.setComposition(a.value("value").toMap());
    else if (operation == "grid")
        ok = worker.setGrid(a.value("value").toMap());
    else if (operation == "action")
        ok = worker.setItemAction(id, QVariantList{a.value("value").toMap()});
    else if (operation == "addWidget" || operation == "addPlaced") {
        ok = worker.addWidget(a.value("type").toString());
        if (ok && operation == "addPlaced") {
            const auto newId = worker.m_items.last().toObject()["id"].toString();
            ok = worker.setItemPlacement(newId, a.value("placement").toMap());
        }
    } else if (operation == "addButton")
        ok = worker.addButton();
    else if (operation == "newPage" && pageId.isEmpty())
        ok = !worker.createPage(a.value("name").toString(), a.value("preset", "blank").toString()).isEmpty();
    else if (operation == "renamePage" && pageId.isEmpty())
        ok = worker.renamePage(id, a.value("name").toString());
    else if (operation == "removePage" && pageId.isEmpty())
        ok = worker.removePage(id);
    else {
        setError(QStringLiteral("Unknown or unavailable edit command."));
        return false;
    }
    if (!ok) {
        setError(worker.errorString().isEmpty() ? QStringLiteral("The edit was refused; nothing changed.")
                                                : worker.errorString());
        return false;
    }
    auto result = QJsonDocument::fromJson(worker.documentData()).object();
    if (operation == "section" && a.value("kind") == "add") {
        auto sections = result["categories"].toArray();
        auto entry = sections.last().toObject();
        const auto identity = allocateId("section");
        if (identity.isEmpty())
            return false;
        entry["id"] = identity;
        sections[sections.size() - 1] = entry;
        result["categories"] = sections;
    }
    if (operation == "newPage") {
        auto entries = result["pages"].toArray();
        auto entry = entries.last().toObject();
        entry["id"] = allocateId("page");
        if (a.value("preset", "blank") == "blank") {
            auto layout = entry["layout"].toObject();
            auto composition = layout["composition"].toObject();
            composition["grid"] = QJsonObject::fromVariantMap(m_live->studioOptions().value("defaultGrid").toMap());
            layout["composition"] = composition;
            entry["layout"] = layout;
        }
        entries[entries.size() - 1] = entry;
        result["pages"] = entries;
    } else if (operation == "addButton" || operation == "addWidget" || operation == "addPlaced") {
        auto entries = result["components"].toArray();
        auto entry = entries.last().toObject();
        const auto base = operation == "addButton"   ? QStringLiteral("button")
                          : operation == "duplicate" ? id + QStringLiteral("-copy")
                                                     : a.value("type").toString();
        const auto identity = allocateId(base);
        if (identity.isEmpty())
            return false;
        entry["id"] = identity;
        entries[entries.size() - 1] = entry;
        result["components"] = entries;
    }
    if (pageIndex >= 0) {
        result.remove("pages");
        result.remove("homePageId");
        result.remove("rootPage");
        result.remove("studioAppearance");
        auto composition = result["composition"].toObject();
        composition.remove("designLibrary");
        result["composition"] = composition;
        auto entry = pages[pageIndex].toObject();
        entry["layout"] = result;
        pages[pageIndex] = entry;
        root["pages"] = pages;
    } else
        root = result;
    return pushEdit(QJsonDocument(root).toJson());
}
