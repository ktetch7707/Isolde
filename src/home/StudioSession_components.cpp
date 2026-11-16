#include "home/StudioSession.h"

QVariantList StudioSession::actionChoices() const {
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
    for (const auto& entry : document["pages"].toArray()) {
        const auto page = entry.toObject();
        choices.append(QVariantMap{{"name", tr("Open page · ") + page["name"].toString()},
                                   {"action", QVariantMap{{"type", "open-page"}, {"pageId", page["id"].toString()}}}});
    }
    for (const auto& entry : m_live->registry()->apps()) {
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
    for (const auto& entry : m_live->m_collections) {
        const auto collection = entry.toMap();
        choices.append(QVariantMap{
            {"name", tr("Open collection · ") + collection.value("name").toString()},
            {"mediaKind", collection.value("media", "movies")},
            {"action", QVariantMap{{"type", "open-collection"}, {"collectionId", collection.value("id")}}}});
    }
    return choices;
}

QVariantList StudioSession::sourceChoices(const QString& id, const QString& pageId) const {
    if (!currentProfile())
        return {};
    HomeLayout worker(m_live->registry());
    configureWorker(worker);
    auto document = QJsonDocument::fromJson(m_draft).object();
    if (!pageId.isEmpty()) {
        bool found = false;
        for (const auto& entry : document["pages"].toArray())
            if (entry.toObject()["id"] == pageId) {
                document = entry.toObject()["layout"].toObject();
                found = true;
                break;
            }
        if (!found)
            return {};
    }
    if (!pageId.isEmpty()) {
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

QObject* StudioSession::proposalModel() const {
    return m_proposalModel;
}

void StudioSession::configureWorker(HomeLayout& worker, bool assetPaths) const {
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

bool StudioSession::validNewAction(const QVariantMap& value) {
    if (value.isEmpty())
        return true;
    QString error;
    const auto action = ComponentSchema::action(value, &error);
    if (action.isEmpty()) {
        setError(error);
        return false;
    }
    const auto type = action.value("type").toString();
    if (type == "open-page") {
        bool exists = false;
        for (const auto& entry : m_model->studioPages())
            if (entry.toMap().value("id") == action.value("pageId"))
                exists = true;
        if (!exists) {
            setError(tr("Choose an existing destination page."));
            return false;
        }
    }
    if (type == "root-page" && !m_model->rootPageVisible()) {
        setError(tr("This page was removed. Choose an existing destination."));
        return false;
    }
    return true;
}

QVariantMap StudioSession::componentPrototype(const QString& type, const QVariantMap& configuration) {
    if (!currentProfile()) {
        setError(tr("Reopen Skin Studio before adding components."));
        return {};
    }
    const auto options = m_live->studioOptions();
    QJsonArray nodes;
    if (options.value("componentPresets").toMap().contains(type))
        nodes.append(QJsonObject::fromVariantMap(options.value("componentPresets").toMap().value(type).toMap()));
    else if (type == "container")
        nodes.append(QJsonObject::fromVariantMap(options.value("groupPreset").toMap()));
    else if (type == "input-panel") {
        const auto preset = options.value("inputPanelPreset").toMap();
        nodes.append(QJsonObject::fromVariantMap(preset.value("root").toMap()));
        for (const auto& entry : preset.value("children").toList())
            nodes.append(QJsonObject::fromVariantMap(entry.toMap()));
    } else if (type == "clock" || type == "text") {
        HomeLayout factory(m_live->registry());
        configureWorker(factory);
        if (!factory.addWidget(type)) {
            setError(factory.errorString());
            return {};
        }
        auto item = factory.m_items.last().toObject();
        const auto span = options.value("widgetSpans").toMap().value(type).toMap();
        item["placement"] = QJsonObject::fromVariantMap(
            {{"x", 0}, {"y", 0}, {"width", span.value("width")}, {"height", span.value("height")}});
        nodes.append(item);
    } else {
        setError(tr("Choose a registered component type."));
        return {};
    }
    if (nodes.isEmpty() || nodes.size() > 129) {
        setError(tr("The packaged component preset is invalid."));
        return {};
    }
    auto configured = nodes.first().toObject();
    if (configuration.contains("variant")) {
        if (configuration["variant"].metaType().id() != QMetaType::QString) {
            setError(tr("Choose a registered starting style."));
            return {};
        }
        QVariantMap selected;
        for (const auto& entry : options.value("componentVariants").toMap().value(type).toList())
            if (entry.toMap().value("id") == configuration["variant"])
                selected = entry.toMap();
        if (selected.isEmpty()) {
            setError(tr("This starting style is not available for that component."));
            return {};
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
    for (auto it = configuration.cbegin(); it != configuration.cend(); ++it) {
        if (!QStringList{"name", "action", "customLogo", "presentation", "appCollection", "apps", "buttons", "variant"}
                 .contains(it.key())) {
            setError(tr("Unsupported component configuration."));
            return {};
        }
        if (it.key() == "apps" || it.key() == "buttons" || it.key() == "variant")
            continue;
        if (it.key() == "presentation") {
            if (it.value().metaType().id() != QMetaType::QVariantMap) {
                setError(tr("Choose valid presentation values."));
                return {};
            }
            auto style = configured["presentation"].toObject();
            const auto changes = it.value().toMap();
            for (auto change = changes.cbegin(); change != changes.cend(); ++change)
                style[change.key()] = QJsonValue::fromVariant(change.value());
            configured["presentation"] = style;
        } else if (it.key() == "action" && it.value().toMap().isEmpty())
            configured.remove("action");
        else
            configured[it.key()] = QJsonValue::fromVariant(it.value());
    }
    nodes[0] = configured;
    if (configuration.contains("buttons")) {
        const auto buttons = configuration.value("buttons").toList();
        if (type != "carousel" || configuration.value("buttons").metaType().id() != QMetaType::QVariantList ||
            buttons.size() > 128) {
            setError(tr("Choose up to 128 carousel buttons."));
            return {};
        }
        auto style = configured["presentation"].toObject();
        auto layout = style["layout"].toObject();
        layout["columns"] = qMax(layout["columns"].toInt(), int(buttons.size()));
        style["layout"] = layout;
        configured["presentation"] = style;
        nodes[0] = configured;
        int column = 0;
        for (const auto& value : buttons) {
            const auto changes = value.toMap();
            if (value.metaType().id() != QMetaType::QVariantMap) {
                setError(tr("Choose a named button."));
                return {};
            }
            auto child = QJsonObject::fromVariantMap(options.value("buttonChildPreset").toMap());
            for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
                if (!QStringList{"name", "action"}.contains(it.key())) {
                    setError(tr("Unsupported carousel button property."));
                    return {};
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
    }
    if (configuration.contains("apps")) {
        const auto apps = configuration.value("apps").toList();
        QSet<QString> selected;
        if (type != "app-bar" || configuration.value("apps").metaType().id() != QMetaType::QVariantList ||
            apps.size() > 128) {
            setError(tr("Choose up to 128 installed apps."));
            return {};
        }
        for (const auto& value : apps) {
            const auto id = value.toString();
            const auto* app = m_live->registry()->find(id);
            if (value.metaType().id() != QMetaType::QString || selected.contains(id) || !app ||
                !QStringList{"desktop", "web", "android"}.contains(app->type)) {
                setError(tr("Choose distinct installed applications."));
                return {};
            }
            selected.insert(id);
        }
        configured["appCollection"] = QJsonObject{{"mode", "selected"}, {"ids", QJsonArray::fromVariantList(apps)}};
        nodes[0] = configured;
    }
    QHash<QString, QString> remap;
    QSet<QString> allocated;
    for (int i = 0; i < nodes.size(); ++i) {
        if (!validNewAction(nodes[i].toObject()["action"].toObject().toVariantMap()))
            return {};
        QString error;
        const auto canonical = ComponentSchema::component(nodes[i].toObject().toVariantMap(), true, &error);
        if (canonical.isEmpty()) {
            setError(error);
            return {};
        }
        auto item = QJsonObject::fromVariantMap(canonical);
        const auto oldId = item["id"].toString();
        if (remap.contains(oldId)) {
            setError(tr("The preset contains duplicate identities."));
            return {};
        }
        auto identity = allocateId((type == "clock" || type == "text") ? type : oldId);
        if (identity.isEmpty())
            return {};
        const auto base = identity;
        int suffix = 2;
        while (allocated.contains(identity))
            identity = base + QString::number(suffix++);
        if (!ComponentSchema::safeId(identity)) {
            setError(tr("The component identity exceeds its limit."));
            return {};
        }
        allocated.insert(identity);
        remap[oldId] = identity;
        item["id"] = identity;
        nodes[i] = item;
    }
    for (int i = 0; i < nodes.size(); ++i) {
        auto item = nodes[i].toObject();
        const auto parent = item.value("parentId").toString();
        if (!parent.isEmpty()) {
            if (!remap.contains(parent)) {
                setError(tr("The preset refers to an external parent."));
                return {};
            }
            item["parentId"] = remap[parent];
            nodes[i] = item;
        }
    }
    const auto root = nodes.first().toObject();
    if (!root.value("parentId").toString().isEmpty()) {
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

QVariantMap StudioSession::replacementProposal(const QString& id, const QString& type, const QString& pageId) {
    const auto fail = [](const QString& reason) {
        return QVariantMap{{"valid", false}, {"reason", reason}};
    };
    if (!currentProfile())
        return fail(tr("Reopen Studio before replacing a component."));
    if (!QStringList{"button", "image", "text", "shape", "container"}.contains(type))
        return fail(tr("Choose a compatible component type."));
    auto document = QJsonDocument::fromJson(m_draft).object(), page = document;
    int pageIndex = -1;
    auto pages = document["pages"].toArray();
    if (!pageId.isEmpty()) {
        for (int i = 0; i < pages.size(); ++i)
            if (pages[i].toObject()["id"] == pageId) {
                pageIndex = i;
                page = pages[i].toObject()["layout"].toObject();
                break;
            }
        if (pageIndex < 0)
            return fail(tr("The page no longer exists."));
    }
    auto entries = page["components"].toArray();
    int index = -1;
    for (int i = 0; i < entries.size(); ++i) {
        const auto entry = entries[i].toObject();
        if (entry["id"] == id)
            index = i;
        if (entry["parentId"] == id)
            return fail(tr("This container has children. Edit its contents or make an independent copy first."));
    }
    if (index < 0 || ComponentSchema::positionLocked(entries, id))
        return fail(tr("Choose an unlocked component to replace."));
    const auto original = entries[index].toObject();
    if (!QStringList{"button", "image", "text", "shape", "container"}.contains(original["componentType"].toString()) ||
        original.contains("family") || original.contains("appCollection"))
        return fail(
            tr("This source owns its contents. Replace a single button, image, text, shape or empty container."));
    const auto prototype = componentPrototype(type);
    if (prototype.isEmpty())
        return fail(errorString());
    auto replacement = QJsonObject::fromVariantMap(prototype["root"].toMap());
    QStringList losses;
    for (const auto& key : QStringList{"id",
                                       "name",
                                       "placement",
                                       "parentId",
                                       "categoryId",
                                       "hidden",
                                       "locked",
                                       "opacity",
                                       "accessibility",
                                       "showTitle",
                                       "showLogo"})
        if (original.contains(key))
            replacement[key] = original[key];
    auto presentation = replacement["presentation"].toObject();
    const auto oldStyle = original["presentation"].toObject();
    for (auto it = oldStyle.begin(); it != oldStyle.end(); ++it) {
        const auto key = it.key();
        const bool semantic = key.startsWith("library") || key.startsWith("guide") ||
                              (key.startsWith("body") && type != "text") ||
                              (key == "animateArtwork" && type != "image") ||
                              (key == "shape" && type != "shape" &&
                               (type != "container" || it.value() == "ellipse" || it.value() == "divider"));
        if (semantic ||
            !ComponentSchema::validPresentation({{key, it.value().toVariant()}}, false, type == "container")) {
            losses.append(key == "layout"     ? tr("Container layout")
                          : key == "shape"    ? tr("Shape outline")
                          : key == "bodyText" ? tr("Text body")
                                              : tr("Presentation: %1").arg(key));
            continue;
        }
        presentation[key] = it.value();
    }
    replacement["presentation"] = presentation;
    if (original["componentType"] == "text" && type != "text" && !losses.contains(tr("Text body")))
        losses.append(tr("Text body"));
    if (original.contains("action")) {
        if (type == "button" || type == "image")
            replacement["action"] = original["action"];
        else
            losses.append(tr("Action link"));
    }
    if (original.contains("customLogo")) {
        if (type != "shape" && type != "text")
            replacement["customLogo"] = original["customLogo"];
        else
            losses.append(tr("Image artwork"));
    }
    const auto normalized = ComponentSchema::component(replacement.toVariantMap(), true);
    if (normalized.isEmpty())
        return fail(tr("These properties do not fit the replacement. Keep the original or choose another type."));
    entries[index] = QJsonObject::fromVariantMap(normalized);
    page["components"] = entries;
    if (pageIndex < 0)
        document = page;
    else {
        auto entry = pages[pageIndex].toObject();
        entry["layout"] = page;
        pages[pageIndex] = entry;
        document["pages"] = pages;
    }
    HomeLayout validator(m_live->registry());
    configureWorker(validator);
    if (!validator.loadDocument(QJsonDocument(document).toJson(), false))
        return fail(validator.errorString());
    return {{"valid", true}, {"document", document.toVariantMap()}, {"losses", losses}};
}

QVariantMap StudioSession::placementProposal(const QString& id,
                                             const QVariantMap& placement,
                                             const QString& strategy,
                                             const QString& pageId,
                                             const QVariantMap& options) {
    const auto fail = [](const QString& reason) {
        return QVariantMap{{"valid", false}, {"reason", reason}};
    };
    if (!m_active)
        return fail(QStringLiteral("The edit session has closed."));
    if (!QStringList{"cancel", "shift", "swap"}.contains(strategy))
        return fail(tr("Choose Shift, Swap or Cancel."));
    for (auto it = options.cbegin(); it != options.cend(); ++it)
        if ((it.key() != "categoryId" && it.key() != "parentId") || it.value().metaType().id() != QMetaType::QString)
            return fail(tr("Choose a valid destination section or group."));
    auto doc = QJsonDocument::fromJson(m_draft).object();
    if (!pageId.isEmpty()) {
        bool found = false;
        for (const auto& entry : doc["pages"].toArray())
            if (entry.toObject()["id"] == pageId) {
                doc = entry.toObject()["layout"].toObject();
                found = true;
                break;
            }
        if (!found)
            return fail(QStringLiteral("The page no longer exists."));
    }
    auto entries = doc["components"].toArray();
    QString category, parent;
    bool selectedFound = false;
    for (const auto& entry : entries)
        if (entry.toObject()["id"] == id) {
            if (ComponentSchema::positionLocked(entries, id))
                return fail(tr("Unlock the component and its containing groups before moving or resizing it."));
            category = entry.toObject()["categoryId"].toString();
            parent = entry.toObject()["parentId"].toString();
            selectedFound = true;
        }
    if (!selectedFound)
        return fail(QStringLiteral("The component no longer exists."));
    const auto composition = doc["composition"].toObject().toVariantMap();
    const bool sections = composition.value("grid").toMap().value("mode") == "sections";
    const auto sourceCategory = category;
    const auto sourceParent = parent;
    if (options.contains("parentId"))
        parent = options.value("parentId").toString();
    if (!parent.isEmpty()) {
        bool found = false;
        QString ancestor = parent;
        for (const auto& entry : entries)
            if (entry.toObject()["id"] == parent)
                found = entry.toObject()["componentType"] == "container";
        if (!found || ComponentSchema::positionLocked(entries, parent))
            return fail(tr("Choose an unlocked destination group."));
        for (int depth = 0; !ancestor.isEmpty() && depth < 6; ++depth) {
            if (ancestor == id)
                return fail(tr("A group cannot contain itself or its ancestors."));
            QString next;
            for (const auto& entry : entries)
                if (entry.toObject()["id"] == ancestor)
                    next = entry.toObject()["parentId"].toString();
            ancestor = next;
        }
        category.clear();
    } else if (!sourceParent.isEmpty())
        category = ComponentSchema::effectiveCategory(entries, id);
    if (options.contains("categoryId")) {
        if (!parent.isEmpty() && !options.value("categoryId").toString().isEmpty())
            return fail(tr("Group contents inherit their section."));
        if (!sections && !options.value("categoryId").toString().isEmpty())
            return fail(tr("Moving between sections requires a scrolling section layout."));
        category = options.value("categoryId").toString();
        bool found = category.isEmpty();
        for (const auto& entry : doc["categories"].toArray())
            if (entry.toObject()["id"] == category) {
                found = true;
                if (entry.toObject()["presentation"].toObject().value("variant") == "library")
                    return fail(tr("A media shelf manages its own contents. Choose a component section."));
            }
        if (!found)
            return fail(tr("The destination section no longer exists."));
    }
    const auto grid =
        ComponentSchema::componentGrid(entries, composition, doc["categories"].toArray(), parent, category);
    const int cols = grid["columns"].toInt(), rows = grid["rows"].toInt();
    if (!ComponentSchema::validPlacement(placement, cols, rows))
        return fail(QStringLiteral("The candidate is outside the grid."));
    if (placement.value("height").toInt() > 10)
        return fail(tr("A component can be at most ten rows tall."));
    int selected = -1;
    QVariantMap origin;
    QList<int> colliders;
    for (int i = 0; i < entries.size(); ++i) {
        const auto item = entries[i].toObject();
        if (item["id"] == id) {
            selected = i;
            origin = item["placement"].toObject().toVariantMap();
        } else if ((!grid.value("overlap").toBool() || strategy == "swap") && item["parentId"].toString() == parent &&
                   (!sections || !parent.isEmpty() || item["categoryId"].toString() == category) &&
                   item.contains("placement") &&
                   ComponentSchema::placementsOverlap(item["placement"].toObject().toVariantMap(), placement))
            colliders.append(i);
    }
    if (selected < 0)
        return fail(QStringLiteral("The component no longer exists."));
    for (const int index : colliders)
        if (entries[index].toObject().value("locked").toBool())
            return fail(tr("A locked component occupies this region. Unlock it or choose another position."));
    QVariantList changes;
    auto moving = entries[selected].toObject();
    moving["placement"] = QJsonObject::fromVariantMap(placement);
    if (category.isEmpty())
        moving.remove("categoryId");
    else
        moving["categoryId"] = category;
    if (parent.isEmpty())
        moving.remove("parentId");
    else
        moving["parentId"] = parent;
    entries[selected] = moving;
    if (!colliders.isEmpty() && strategy == "cancel")
        return fail(QStringLiteral("Occupied cells: choose Shift, Swap or Cancel."));
    if (!colliders.isEmpty() && strategy == "swap") {
        if (colliders.size() != 1 || origin.isEmpty())
            return fail(QStringLiteral("Swap needs one occupant and an existing origin."));
        auto other = entries[colliders.first()].toObject();
        auto replacement = other["placement"].toObject().toVariantMap();
        replacement["x"] = origin["x"];
        replacement["y"] = origin["y"];
        if (sourceCategory != category) {
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
        entries[colliders.first()] = other;
    } else if (!colliders.isEmpty() && strategy == "shift") {
        const auto originalEntries = doc["components"].toArray();
        const auto stableOrder = [&originalEntries](int a, int b) {
            const auto x = originalEntries[a].toObject(), y = originalEntries[b].toObject();
            const auto p = x["placement"].toObject(), q = y["placement"].toObject();
            if (p["y"] != q["y"])
                return p["y"].toInt() < q["y"].toInt();
            if (p["x"] != q["x"])
                return p["x"].toInt() < q["x"].toInt();
            return x["id"].toString() < y["id"].toString();
        };
        std::sort(colliders.begin(), colliders.end(), stableOrder);
        if (colliders.size() >= 64)
            return fail(QStringLiteral("The shift exceeds 64 affected components."));
        QSet<int> fixed{selected};
        int attempts = 0;
        bool affectedLimit = false;
        std::function<bool(QList<int>)> solve = [&](QList<int> pending) -> bool {
            pending.erase(std::remove_if(pending.begin(),
                                         pending.end(),
                                         [&](int i) { return fixed.contains(i); }),
                          pending.end());
            std::sort(pending.begin(), pending.end(), stableOrder);
            pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
            if (pending.isEmpty())
                return true;
            const int index = pending.takeFirst();
            if (fixed.size() >= 64) {
                affectedLimit = true;
                return false;
            }
            if (originalEntries[index].toObject().value("locked").toBool())
                return false;
            const auto baseline = entries;
            const auto fixedBefore = fixed;
            auto item = entries[index].toObject();
            auto target = originalEntries[index].toObject()["placement"].toObject().toVariantMap();
            item.remove("placement");
            entries[index] = item;
            const auto vacant = entries;
            QList<QPair<int, int>> cells;
            int searchRows = rows;
            if (rows == ComponentSchema::MaxScrollableRows) {
                int bottom = placement.value("y").toInt() + placement.value("height").toInt();
                for (const auto& value : originalEntries) {
                    const auto entry = value.toObject();
                    if (entry.value("parentId").toString() != parent)
                        continue;
                    const auto p = entry["placement"].toObject();
                    bottom = qMax(bottom, p["y"].toInt() + p["height"].toInt());
                }
                searchRows = qMin(rows, bottom + 10);
            }
            for (int row = 0; row < searchRows; ++row)
                for (int col = 0; col < cols; ++col)
                    cells.append({col, row});
            const int originalX = target["x"].toInt(), originalY = target["y"].toInt();
            std::stable_sort(cells.begin(), cells.end(), [=](const auto& a, const auto& b) {
                return std::abs(a.first - originalX) + std::abs(a.second - originalY) <
                       std::abs(b.first - originalX) + std::abs(b.second - originalY);
            });
            for (int pass = 0; pass < 2; ++pass)
                for (const auto& cell : cells) {
                    if (++attempts > 4096) {
                        entries = baseline;
                        fixed = fixedBefore;
                        return false;
                    }
                    target["x"] = cell.first;
                    target["y"] = cell.second;
                    if (!ComponentSchema::validPlacement(target, cols, rows))
                        continue;
                    QList<int> displaced;
                    bool blocked = false;
                    for (int j = 0; j < entries.size(); ++j) {
                        const auto other = entries[j].toObject();
                        if (j == index || other.value("parentId").toString() != parent ||
                            (sections && parent.isEmpty() && other.value("categoryId").toString() != category) ||
                            !other.contains("placement") ||
                            !ComponentSchema::placementsOverlap(other["placement"].toObject().toVariantMap(), target))
                            continue;
                        if (fixed.contains(j) || other.value("locked").toBool()) {
                            blocked = true;
                            break;
                        }
                        displaced.append(j);
                    }
                    if (blocked || (pass == 0 && !displaced.isEmpty()) || (pass == 1 && displaced.isEmpty()))
                        continue;
                    auto placed = item;
                    placed["placement"] = QJsonObject::fromVariantMap(target);
                    entries[index] = placed;
                    fixed.insert(index);
                    auto next = pending;
                    next.append(displaced);
                    if (solve(next))
                        return true;
                    entries = vacant;
                    fixed = fixedBefore;
                    if (attempts > 4096) {
                        entries = baseline;
                        return false;
                    }
                }
            entries = baseline;
            fixed = fixedBefore;
            return false;
        };
        if (!solve(colliders))
            return fail(attempts > 4096
                            ? tr("Shift reached its 4096-proposal search limit. Choose another cell or Swap.")
                        : affectedLimit ? tr("The shift exceeds 64 affected components.")
                                        : tr("No complete shift fits. Cancel or choose another cell."));
    } else if (!colliders.isEmpty())
        return fail(QStringLiteral("Choose a collision strategy."));
    const auto originalEntries = doc["components"].toArray();
    const auto appendChange = [&](int index) {
        const auto item = entries[index].toObject();
        changes.append(QVariantMap{{"id", item["id"].toString()},
                                   {"placement", item["placement"].toObject().toVariantMap()},
                                   {"categoryId", item.value("categoryId").toString()},
                                   {"parentId", item.value("parentId").toString()}});
    };
    appendChange(selected);
    for (int i = 0; i < entries.size(); ++i)
        if (i != selected &&
            (entries[i].toObject().value("placement") != originalEntries[i].toObject().value("placement") ||
             entries[i].toObject().value("categoryId") != originalEntries[i].toObject().value("categoryId") ||
             entries[i].toObject().value("parentId") != originalEntries[i].toObject().value("parentId")))
            appendChange(i);
    doc["components"] = entries;
    QString error;
    if (!ComponentSchema::hierarchyFits(entries, composition, doc["categories"].toArray(), &error))
        return fail(error);
    return {{"valid", true}, {"placements", changes}, {"reason", QString()}};
}

QVariantMap StudioSession::additionProposal(const QString& type,
                                            const QVariantMap& placement,
                                            const QString& strategy,
                                            const QString& pageId,
                                            const QString& categoryId,
                                            const QString& parentId,
                                            const QVariantMap& configuration,
                                            bool enableScrolling) {
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
    if (!pageId.isEmpty()) {
        for (int i = 0; i < pages.size(); ++i)
            if (pages[i].toObject()["id"] == pageId)
                pageIndex = i;
        if (pageIndex < 0)
            return {{"valid", false}, {"reason", QStringLiteral("The page no longer exists.")}};
        document = pages[pageIndex].toObject()["layout"].toObject();
    }
    if (enableScrolling) {
        if (!parentId.isEmpty() || !categoryId.isEmpty())
            return {{"valid", false}, {"reason", tr("Choose the page before enabling vertical scrolling.")}};
        auto composition = document["composition"].toObject();
        composition["verticalScrolling"] = true;
        document["composition"] = composition;
    }
    if (type == "page-menu" && !pageId.isEmpty()) {
        auto composition = document["composition"].toObject();
        composition["pageNavigation"] = "none";
        document["composition"] = composition;
    }
    auto entries = document["components"].toArray();
    for (const auto& value : prototype.value("components").toList()) {
        auto item = QJsonObject::fromVariantMap(value.toMap());
        if (item["id"] == identity) {
            item.remove("placement");
            if (!parentId.isEmpty())
                item["parentId"] = parentId;
            else if (!categoryId.isEmpty())
                item["categoryId"] = categoryId;
        }
        entries.append(item);
    }
    document["components"] = entries;
    const auto store = [&] {
        if (pageIndex >= 0) {
            auto entry = pages[pageIndex].toObject();
            entry["layout"] = document;
            pages[pageIndex] = entry;
            root["pages"] = pages;
        } else
            root = document;
    };
    store();
    transaction.m_draft = QJsonDocument(root).toJson();
    auto proposal = transaction.placementProposal(
        identity, placement, strategy, pageId, QVariantMap{{"parentId", parentId}, {"categoryId", categoryId}});
    if (!proposal.value("valid").toBool())
        return proposal;
    proposal["id"] = identity;
    for (const auto& change : proposal["placements"].toList()) {
        const auto value = change.toMap();
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].toObject()["id"] == value["id"].toString()) {
                auto entry = entries[i].toObject();
                entry["placement"] = QJsonObject::fromVariantMap(value["placement"].toMap());
                for (const auto& key : QStringList{"parentId", "categoryId"}) {
                    const auto destination = value.value(key).toString();
                    if (destination.isEmpty())
                        entry.remove(key);
                    else
                        entry[key] = destination;
                }
                entries[i] = entry;
            }
    }
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
