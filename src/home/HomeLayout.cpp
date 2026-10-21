#include "home/HomeLayout.h"
#include "home/ComponentSchema.h"
#include "home/StudioSession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QRegularExpression>
#include <QtMath>
#include <QCryptographicHash>

namespace {

QVariantMap appearanceDefaults()
{
    return {{"focusColor", "#c994a5"}, {"surfaceColor", "#202126"}, {"selectedColor", "#353039"},
            {"textColor", "#f5f1f3"}, {"mutedColor", "#aaa5af"}, {"typographyScale", 1.0},
            {"homeColumns", 3}, {"spacing", 20}, {"motionEnabled", true}, {"fontFamily", "DejaVu Sans"}};
}

bool normalizeAppearance(QVariantMap &values)
{
    const auto defaults = appearanceDefaults();
    for (auto it = values.begin(); it != values.end(); ++it) {
        const auto key = it.key();
        if (key.endsWith("Color") || key == "backgroundStart" || key == "backgroundEnd") {
            const QColor color(it.value().toString());
            if (!color.isValid() || color.alpha() != 255) return false;
            it.value() = color.name();
        } else if (key == "fontFamily") {
            if (it.value().metaType().id() != QMetaType::QString || it.value().toString().trimmed().isEmpty()
                || it.value().toString().size() > HomeLayout::MaxPageNameLength || it.value().toString().contains(QChar::Null)) return false;
        } else if (key == "motionEnabled") {
            if (it.value().metaType().id() != QMetaType::Bool) return false;
        } else {
            if (!defaults.contains(key) && key != "tileRoundness") return false;
            if (it.value().metaType().id() == QMetaType::QString || it.value().metaType().id() == QMetaType::Bool) return false;
            bool ok = false; const double number = it.value().toDouble(&ok);
            const double low = key == "typographyScale" ? HomeLayout::MinTypographyScale
                            : key == "homeColumns" ? HomeLayout::MinHomeColumns
                            : key == "spacing" ? HomeLayout::MinSpacing
                            : 0;
            const double high = key == "typographyScale" ? HomeLayout::MaxTypographyScale
                             : key == "homeColumns" ? HomeLayout::MaxHomeColumns
                             : key == "spacing" ? HomeLayout::MaxSpacing
                             : 1;
            if (!ok || !qIsFinite(number) || number < low || number > high
                || ((key == "homeColumns" || key == "spacing") && number != qFloor(number))) return false;
        }
        if (!defaults.contains(key) && key != "backgroundStart" && key != "backgroundEnd" && key != "tileRoundness") return false;
    }
    return true;
}

double contrast(const QColor &a,const QColor &b) {
    const auto luminance=[](const QColor &color) {
        const auto linear=[](double channel){return channel<=0.04045?channel/12.92:qPow((channel+0.055)/1.055,2.4);};
        return 0.2126*linear(color.redF())+0.7152*linear(color.greenF())+0.0722*linear(color.blueF());
    };
    const double x=luminance(a),y=luminance(b);return (qMax(x,y)+0.05)/(qMin(x,y)+0.05);
}
bool readableStudioAppearance(QVariantMap values) {
    if(!normalizeAppearance(values))return false;
    const auto surface=QColor(values.value("surfaceColor").toString()),selected=QColor(values.value("selectedColor").toString());
    for(const auto &key:QStringList{"textColor","mutedColor"}) {
        const auto ink=QColor(values.value(key).toString());
        if(!ink.isValid() || !surface.isValid() || !selected.isValid() || contrast(ink,surface)<HomeLayout::MinTextContrast || contrast(ink,selected)<HomeLayout::MinTextContrast)return false;
    }
    const auto focus=QColor(values.value("focusColor").toString());
    return focus.isValid() && contrast(focus,surface)>=HomeLayout::MinFocusContrast && contrast(focus,selected)>=HomeLayout::MinFocusContrast;
}

bool isSafeActionId(const QString &id)
{
    return id.isEmpty() || ComponentSchema::safeId(id);
}
bool isSupportedActionType(const QString &type)
{
    static const QSet<QString> actionTypes{QStringLiteral("launch-app"), QStringLiteral("open-folder"),
                                           QStringLiteral("open-file"), QStringLiteral("open-url"),
                                           QStringLiteral("open-disc"), QStringLiteral("desktop-mode"),
                                           QStringLiteral("switch-input")};
    return actionTypes.contains(type);
}

QString defaultActionTypeForKind(const QJsonObject &item)
{
    if (item.value(QStringLiteral("kind")).toString() == QStringLiteral("app"))
        return QStringLiteral("launch-app");
    return {};
}

QJsonObject normalizedActionForItem(const QJsonObject &item)
{
    auto action = item.value(QStringLiteral("action")).toObject();
    if (action.isEmpty() && item.value("kind") == "app")
        action = {{"type", "launch-app"}, {"appId", item.value("appId")}};
    return QJsonObject::fromVariantMap(ComponentSchema::action(action.toVariantMap()));
}

} // namespace

HomeLayout::HomeLayout(AppRegistry *registry, QObject *parent) : QObject(parent), m_registry(registry)
{
    connect(this,&HomeLayout::layoutChanged,this,&HomeLayout::syncPages);
    connect(m_registry, &AppRegistry::appsChanged, this, &HomeLayout::layoutChanged);
}

QVariantList HomeLayout::items() const
{
    QVariantList result;
    QSet<QString> hidden;
    if(!m_draftOnly){
        for(const auto &entry:m_items)if(entry.toObject().value("hidden").toBool())hidden.insert(entry.toObject()["id"].toString());
        // Parent-chain walks are bounded at MaxComponentNestingDepth levels so
        // hidden state cannot cascade forever through nested items.
        for(int depth=0;depth<ComponentSchema::MaxComponentNestingDepth;++depth)for(const auto &entry:m_items){const auto item=entry.toObject();if(hidden.contains(item.value("parentId").toString()))hidden.insert(item["id"].toString());}
    }
    const auto appendItem = [this, &result,&hidden](const QJsonValue &value) {
        const auto object = value.toObject();
        if(hidden.contains(object["id"].toString()))return;
        auto item = object.toVariantMap();
        const auto action = normalizedActionForItem(object);
        if (!action.isEmpty()) {
            item.insert("actionType", action.value(QStringLiteral("type")).toString());
            item.insert("action", action.toVariantMap());
        }
        if (item.value("kind") == QStringLiteral("app")) {
            const auto appId = item.value("appId").toString();
            const auto *app = m_registry->find(appId);
            item.insert("name", !item.value("name").toString().isEmpty()?item.value("name"):app ? app->name : appId);
            item.insert("section", app ? app->section : QStringLiteral("Missing app"));
            const auto tileSymbol = item.value("symbol").toString();
            const auto tileAccent = item.value("accent").toString();
            item.insert("symbol", tileSymbol.isEmpty() ? (app ? app->symbol : "?") : tileSymbol);
            item.insert("accent", tileAccent.isEmpty() ? (app ? app->accent.name() : "#64748B") : tileAccent);
            item.insert("presentationMode", app ? app->presentationMode : "native-tv");
            item.insert("networkRequirement", app ? app->networkRequirement : "none");
        } else if (item.value("kind") == QStringLiteral("action")) {
            const auto section = item.value("section").toString();
            const auto symbol = item.value("symbol").toString();
            const auto accent = item.value("accent").toString();
            const auto networkRequirement = item.value("networkRequirement").toString();
            item.insert("section", section.isEmpty() ? tr("Custom") : section);
            item.insert("symbol", symbol.isEmpty() ? QStringLiteral("•") : symbol);
            item.insert("accent", accent.isEmpty() ? QStringLiteral("#64748B") : accent);
            item.insert("networkRequirement", networkRequirement.isEmpty() ? QStringLiteral("none") : networkRequirement);
        }
        const auto logoFile = item.value("customLogo").toString();
        item.insert("logoSource", resolveArtwork(logoFile));
        auto unavailable = ComponentSchema::unavailable(action.toVariantMap(), m_registry, m_collections);
        if (unavailable.isEmpty() && action.value("type").toString() == "switch-input" && !m_captureInputsReady)
            unavailable = "Configure Settings → System → HDMI Inputs";
        if(object.value("requiredCapability")=="capture-inputs" && !m_captureInputsReady)unavailable=tr("HDMI capture unavailable · configure inputs in Settings");
        item.insert("unavailableReason", unavailable);
        result.append(ComponentSchema::component(item, true));
    };
    for (const auto &categoryValue : m_categories) {
        const auto categoryId = categoryValue.toObject().value("id").toString();
        for (const auto &value : m_items)
            if (value.toObject().value("categoryId").toString() == categoryId) appendItem(value);
    }
    for (const auto &value : m_items)
        if (value.toObject().value("categoryId").toString().isEmpty()) appendItem(value);
    return result;
}

QVariantList HomeLayout::components() const { return items(); }
void HomeLayout::setCollections(const QVariantList &collections)
{
    if (m_collections == collections) return;
    m_collections = collections; emit layoutChanged();
}
void HomeLayout::setCaptureInputsReady(bool ready)
{
    if (m_captureInputsReady == ready) return;
    m_captureInputsReady = ready;
    emit layoutChanged();
}
QVariantList HomeLayout::sourceChoices(const QString &itemId) const
{
    QVariantList choices;
    for (const auto &value : m_items) {
        const auto item = value.toObject();
        if (item.value("id") != itemId) continue;
        const auto action = normalizedActionForItem(item);
        const auto type = action.value("type").toString();
        if (type == "switch-input") {
            // HDMI inputs are numbered 1..ComponentSchema::MaxHdmiInput.
            for (int input = ComponentSchema::MinHdmiInput; input <= ComponentSchema::MaxHdmiInput; ++input)
                choices.append(QVariantMap{{"id", QString::number(input)}, {"name", QString("HDMI %1").arg(input)}});
        } else if (type == "open-collection" || type == "play-movie" || type == "open-show") {
            for (const auto &entry : m_collections) choices.append(QVariantMap{{"id", entry.toMap().value("id")}, {"name", entry.toMap().value("name")}});
        } else for (const auto &entry : m_registry->apps()) {
            const auto app = entry.toMap();
            if (type == "launch-app" || app.value("handledActions").toStringList().contains(type))
                choices.append(QVariantMap{{"id", app.value("id")}, {"name", app.value("name")}});
        }
        break;
    }
    return choices;
}
bool HomeLayout::rebindSource(const QString &itemId, const QString &sourceId)
{
    bool compatible = false;
    for (const auto &choice : sourceChoices(itemId)) if (choice.toMap().value("id") == sourceId) compatible = true;
    if (!compatible) { setError(tr("Choose a compatible installed source.")); return false; }
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items[i].toObject();
        if (item.value("id") != itemId) continue;
        const auto previous = item;
        auto action = normalizedActionForItem(item);
        const auto type = action.value("type").toString();
        if (type == "launch-app") { action["appId"] = sourceId; if (item.value("kind") == "app") item["appId"] = sourceId; }
        else if (type == "open-collection" || type == "play-movie" || type == "open-show") action["collectionId"] = sourceId;
        else if (type == "switch-input") action["input"] = sourceId.toInt();
        else action["openWith"] = sourceId;
        item["action"] = action;
        item = QJsonObject::fromVariantMap(ComponentSchema::component(item.toVariantMap(), true));
        m_items[i] = item;
        if (!save()) { m_items[i] = previous; return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

QVariantList HomeLayout::categories() const
{
    return m_categories.toVariantList();
}

QVariantList HomeLayout::sections() const
{
    QVariantList result;
    const auto resolved = items();
    for (const auto &categoryValue : m_categories) {
        const auto category = categoryValue.toObject();
        QVariantList categoryItems;
        for (const auto &itemValue : resolved)
            if (itemValue.toMap().value("parentId").toString().isEmpty() && itemValue.toMap().value("categoryId").toString() == category.value("id").toString()) categoryItems.append(itemValue);
        if (!categoryItems.isEmpty() || category["presentation"].toObject()["variant"] == "library" || grid().value("mode")=="sections") { auto section = category.toVariantMap(); section["items"] = categoryItems; result.append(section); }
    }
    QVariantList uncategorized;
    for (const auto &itemValue : resolved)
        if (itemValue.toMap().value("parentId").toString().isEmpty() && itemValue.toMap().value("categoryId").toString().isEmpty()) uncategorized.append(itemValue);
    if (!uncategorized.isEmpty() || grid().value("mode")=="sections") result.append(QVariantMap{{"id", ""}, {"name", tr("Uncategorized")}, {"items", uncategorized}});
    return result;
}

bool HomeLayout::brandVisible() const { return m_brandVisible; }
QString HomeLayout::backgroundStart() const { return m_backgroundStart; }
QString HomeLayout::backgroundEnd() const { return m_backgroundEnd; }
double HomeLayout::tileRoundness() const { return m_tileRoundness; }
QVariantMap HomeLayout::appearance() const
{
    auto result = appearanceDefaults();
    for (auto it = m_appearance.cbegin(); it != m_appearance.cend(); ++it) result[it.key()] = it.value();
    result["backgroundStart"] = m_backgroundStart; result["backgroundEnd"] = m_backgroundEnd;
    result["tileRoundness"] = m_tileRoundness;
    return result;
}
bool HomeLayout::setAppearance(const QVariantMap &changes)
{
    auto normalized = changes;
    if (!normalizeAppearance(normalized)) { setError(tr("Choose valid opaque colors and appearance values within the displayed ranges.")); return false; }
    const auto previous = m_appearance;
    const auto previousKeys=m_themeKeys;for(auto it=changes.cbegin();it!=changes.cend();++it)m_themeKeys.insert(it.key());
    const auto previousStart = m_backgroundStart, previousEnd = m_backgroundEnd;
    const auto previousRoundness = m_tileRoundness;
    if (normalized.contains("backgroundStart")) m_backgroundStart = normalized.take("backgroundStart").toString();
    if (normalized.contains("backgroundEnd")) m_backgroundEnd = normalized.take("backgroundEnd").toString();
    if (normalized.contains("tileRoundness")) m_tileRoundness = normalized.take("tileRoundness").toDouble();
    for (auto it = normalized.cbegin(); it != normalized.cend(); ++it) m_appearance[it.key()] = it.value();
    if (!save()) {
        m_themeKeys=previousKeys;m_appearance = previous; m_backgroundStart = previousStart; m_backgroundEnd = previousEnd;
        m_tileRoundness = previousRoundness; return false;
    }
    emit layoutChanged(); return true;
}
bool HomeLayout::setComposition(const QVariantMap &value)
{
    if (!ComponentSchema::validComposition(value)) { setError(tr("Invalid Home composition.")); return false; }
    if (!ComponentSchema::layoutPlacementsFit(m_items,value,m_categories)) {
        setError(tr("Placed widgets do not fit this composition's grid.")); return false;
    }
    const auto previous = m_composition; m_composition = value;
    if (!save()) { m_composition = previous; return false; }
    emit layoutChanged(); return true;
}
QVariantMap HomeLayout::grid() const
{
    return ComponentSchema::normalizedGrid(m_composition.value("grid").toMap());
}
QVariantMap HomeLayout::studioOptions() const
{
    QFile file(QDir(QFileInfo(m_defaultPath).absolutePath()).filePath("studio-defaults.json"));
    if(!file.open(QIODevice::ReadOnly)) {file.setFileName(":/config/studio-defaults.json");if(!file.open(QIODevice::ReadOnly))return {};}
    if(file.size()>64*1024)return {};
    const auto object=QJsonDocument::fromJson(file.readAll()).object();
    if(object["schemaVersion"].toInt()!=1 || !object["defaultGrid"].isObject()
        || !ComponentSchema::validGrid(object["defaultGrid"].toObject().toVariantMap()) || !object["gridModes"].isArray())return {};
    if(!object["appearance"].isObject() || !readableStudioAppearance(object["appearance"].toObject().toVariantMap()))return {};
    if(object.contains("colorPicker")) {
        const auto picker=object["colorPicker"].toObject();
        if(picker.size()!=1 || !picker["presets"].isArray() || picker["presets"].toArray().isEmpty() || picker["presets"].toArray().size()>8)return {};
        for(const auto &entry:picker["presets"].toArray()) {
            const auto swatch=entry.toObject();const QColor color(swatch["value"].toString());
            if(swatch.size()!=2 || !swatch["name"].isString() || swatch["name"].toString().isEmpty() || swatch["name"].toString().size()>40
                || !swatch["value"].isString() || !color.isValid() || color.alpha()!=255)return {};
        }
    }
    if(!object["menuWidth"].isDouble() || object["menuWidth"].toDouble()<200 || object["menuWidth"].toDouble()>480)return {};
    if(!object["sectionHeight"].isDouble() || object["sectionHeight"].toDouble()<60 || object["sectionHeight"].toDouble()>800)return {};
    for(const auto &type:QStringList{"clock","text"}){const auto span=object["widgetSpans"].toObject()[type].toObject().toVariantMap();if(span.size()!=2 || !ComponentSchema::validPlacement({{"x",0},{"y",0},{"width",span.value("width")},{"height",span.value("height")}},16,10))return {};}
    const auto group=ComponentSchema::component(object["groupPreset"].toObject().toVariantMap(),true);
    if(group.isEmpty() || group.value("componentType")!="container")return {};
    const auto panel=object["inputPanelPreset"].toObject();
    if(!panel["children"].isArray() || panel["children"].toArray().size()>32)return {};
    QJsonArray panelItems;
    const auto panelRoot=ComponentSchema::component(panel["root"].toObject().toVariantMap(),true);if(panelRoot.isEmpty() || panelRoot.value("componentType")!="container")return {};
    panelItems.append(QJsonObject::fromVariantMap(panelRoot));
    for(const auto &entry:panel["children"].toArray()){const auto child=ComponentSchema::component(entry.toObject().toVariantMap(),true);if(child.isEmpty())return {};panelItems.append(QJsonObject::fromVariantMap(child));}
    if(!ComponentSchema::hierarchyFits(panelItems,{{"schemaVersion",1},{"enabled",true},{"grid",object["defaultGrid"].toObject().toVariantMap()}},{}))return {};
    const auto presets=object["componentPresets"].toObject();
    for(auto it=presets.constBegin();it!=presets.constEnd();++it)if(!QStringList{"button","image","library","carousel","app-bar"}.contains(it.key()) || ComponentSchema::component(it.value().toObject().toVariantMap(),true).isEmpty())return {};
    if(presets.size()!=5 || object["catalog"].toArray().size()!=6 || object["inspector"].toObject()["fields"].toArray().size()>160)return {};
    for(const auto &entry:object["actions"].toArray())if(ComponentSchema::action(entry.toObject()["action"].toObject().toVariantMap()).isEmpty())return {};
    QSet<QString> ids;
    for(const auto &entry:object["gridModes"].toArray()) {
        const auto mode=entry.toObject();const auto id=mode["id"].toString(),name=mode["name"].toString();
        if(!QStringList{"screen","sections"}.contains(id) || ids.contains(id) || name.trimmed().isEmpty() || name.size()>80)return {};
        ids.insert(id);
    }
    if(ids.size()!=2 || !ids.contains(object["defaultGrid"].toObject()["mode"].toString()))return {};
    auto result=object.toVariantMap();result["maxScrollableRows"]=ComponentSchema::MaxScrollableRows;
    return result;
}

QVariantMap HomeLayout::studioAppearance() const {
    auto result=appearance();
    const auto defaults=studioOptions().value("appearance").toMap();
    for(auto it=defaults.cbegin();it!=defaults.cend();++it)result[it.key()]=it.value();
    for(auto it=m_studioAppearance.cbegin();it!=m_studioAppearance.cend();++it)result[it.key()]=it.value();
    return result;
}
bool HomeLayout::setGrid(const QVariantMap &value)
{
    if (!ComponentSchema::validGrid(value)) { setError(tr("Invalid Home grid.")); return false; }
    auto updated = grid();
    for (auto it = value.cbegin(); it != value.cend(); ++it) updated.insert(it.key(), it.value());
    auto config = m_composition;
    if (config.isEmpty()) config = {{"schemaVersion", 1}, {"enabled", true}};
    config["grid"] = updated;
    return setComposition(config);
}
bool HomeLayout::setItemPlacement(const QString &id, const QVariantMap &placement)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items[i].toObject(); if (item["id"] != id) continue;
        const auto previous = item;
        item["placement"] = QJsonObject::fromVariantMap(placement);
        auto proposed=m_items;proposed[i]=item;
        if(!ComponentSchema::layoutPlacementsFit(proposed,m_composition,m_categories)) {
            setError(tr("The placement exceeds its grid or overlaps another component in that section."));return false;
        }
        m_items[i] = item;
        if (!save()) { m_items[i] = previous; return false; }
        emit layoutChanged(); setError({}); return true;
    }
    return false;
}
bool HomeLayout::clearItemPlacement(const QString &id)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items[i].toObject();
        if (item["id"] != id || !item.contains("placement")) continue;
        const auto previous = item;
        item.remove("placement"); m_items[i] = item;
        if (!save()) { m_items[i] = previous; return false; }
        emit layoutChanged(); setError({}); return true;
    }
    return false;
}
bool HomeLayout::duplicateItem(const QString &id)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        const auto original = m_items.at(i).toObject();
        if (original.value("id").toString() != id) continue;
        auto copy = original;
        QString baseId = id + QStringLiteral("-copy");
        QString newId = baseId;
        int suffix = 2;
        const auto idExists = [this](const QString &candidate) {
            for (const auto &value : m_items)
                if (value.toObject().value("id").toString() == candidate) return true;
            return false;
        };
        while (idExists(newId)) newId = baseId + QString::number(suffix++);
        copy.insert("id", newId);
        const auto configured = grid();
        const auto columns = configured.value("columns").toInt();
        const auto rows = configured.value("rows").toInt();
        if (copy.contains("placement")) {
            const auto placement = copy.value("placement").toObject().toVariantMap();
            // Keep the placement only if it still fits and does not collide; otherwise drop it
            // and let the user place the duplicate via the grid editor.
            if (!ComponentSchema::validPlacement(placement, columns, rows)) {
                copy.remove("placement");
            } else {
                bool collides = false;
                for (const auto &entry : m_items) {
                    const auto other = entry.toObject();
                    if (!other.contains("placement")) continue;
                    if (ComponentSchema::placementsOverlap(placement,
                            other.value("placement").toObject().toVariantMap())) {
                        collides = true;
                        break;
                    }
                }
                if (collides) copy.remove("placement");
            }
        }
        m_items.append(copy);
        if (!save()) { m_items.removeLast(); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

QVariantList HomeLayout::placements() const
{
    QVariantList result;
    for (const auto &entry : m_items) {
        const auto item = entry.toObject();
        if (!item.contains("placement")) continue;
        result.append(QVariantMap{{QStringLiteral("id"), item.value("id").toString()},
                                 {QStringLiteral("placement"), item.value("placement").toObject().toVariantMap()}});
    }
    return result;
}
QVariantList HomeLayout::gridCollisions(const QString &itemId, const QVariantMap &placement) const{
    QVariantList result;
    QString category,parent;
    for(const auto &entry:m_items)if(entry.toObject()["id"]==itemId){category=entry.toObject()["categoryId"].toString();parent=entry.toObject().value("parentId").toString();}
    const auto configured = ComponentSchema::componentGrid(m_items,m_composition,m_categories,parent,category);
    if (!ComponentSchema::validPlacement(placement, configured.value("columns").toInt(), configured.value("rows").toInt()))
        return result;
    const auto placed = m_items;
    for (const auto &entry : placed) {
        const auto item = entry.toObject();
        if (item.value("id").toString() == itemId || !item.contains("placement")) continue;
        if(item.value("parentId").toString()!=parent || (parent.isEmpty() && grid().value("mode")=="sections" && item["categoryId"].toString()!=category))continue;
        if (!ComponentSchema::placementsOverlap(placement, item.value("placement").toObject().toVariantMap())) continue;
        QString name = item.value("name").toString();
        if (item.value("kind").toString() == QStringLiteral("app")) {
            const auto *app = m_registry->find(item.value("appId").toString());
            if (app) name = app->name;
        }
        result.append(QVariantMap{{"id", item.value("id").toString()}, {"name", name}});
    }
    return result;
}
QString HomeLayout::wallpaperSource() const
{
    return resolveArtwork(m_composition.value("wallpaper").toString());
}
QString HomeLayout::resolveArtwork(const QString &relative) const
{
    if(relative.isEmpty())return {};
    if(!QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$").match(relative).hasMatch())return {};
    if(m_assetPaths.contains(relative))return QUrl::fromLocalFile(m_assetPaths.value(relative)).toString();
    if(m_userPath.isEmpty())return {};
    const auto profile=QFileInfo(QFileInfo(m_userPath).absolutePath()).canonicalFilePath();
    const auto path=QDir(QFileInfo(m_userPath).absolutePath()).filePath(relative);
    const auto canonical=QFileInfo(path).canonicalFilePath();
    if(!canonical.isEmpty() && (profile.isEmpty() || QDir(profile).relativeFilePath(canonical).startsWith("../")))return {};
    return QUrl::fromLocalFile(path).toString();
}
bool HomeLayout::setWallpaper(const QString &sourceUrl)
{
    const QUrl url(sourceUrl);
    if (!url.isLocalFile()) { setError(tr("Choose a local PNG or JPEG wallpaper.")); return false; }
    QFile source(url.toLocalFile());
    if (!source.open(QIODevice::ReadOnly) || source.size() > HomeLayout::MaxArtworkSize) return false;
    const auto data = source.readAll(); const bool png = data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"));
    if (!png && !data.startsWith(QByteArray::fromHex("ffd8ff"))) return false;
    const auto relative = "artwork/wallpaper-" + QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()) + (png ? ".png" : ".jpg");
    const auto directory = QFileInfo(m_userPath).absolutePath();
    if (!QDir().mkpath(QDir(directory).filePath("artwork"))) return false;
    QSaveFile output(QDir(directory).filePath(relative));
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) return false;
    auto config = m_composition;
    if (config.isEmpty()) config = {{"schemaVersion",1},{"enabled",true}};
    config["wallpaper"] = relative;
    return setComposition(config);
}
bool HomeLayout::setItemPresentation(const QString &id, const QVariantMap &value)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items[i].toObject(); if (item["id"] != id) continue;
        if(!ComponentSchema::validPresentation(value,item["componentType"]=="library",item["componentType"]=="container")){setError(tr("Choose supported presentation choices for this component."));return false;}
        const auto previous = item; item["presentation"] = QJsonObject::fromVariantMap(value); m_items[i] = item;
        if (!save()) { m_items[i] = previous; return false; }
        emit layoutChanged(); return true;
    }
    return false;
}
bool HomeLayout::setSectionPresentation(const QString &id, const QVariantMap &value)
{
    if (!ComponentSchema::validPresentation(value, true)) return false;
    for (qsizetype i = 0; i < m_categories.size(); ++i) {
        auto section = m_categories[i].toObject(); if (section["id"] != id) continue;
        const auto previous = section; section["presentation"] = QJsonObject::fromVariantMap(value); m_categories[i] = section;
        if (!save()) { m_categories[i] = previous; return false; }
        emit layoutChanged(); return true;
    }
    return false;
}
QString HomeLayout::errorString() const { return m_errorString; }

bool HomeLayout::load(const QString &defaultPath, const QString &userPath)
{
    if (m_studio) m_studio->invalidate();
    m_legacyDocument.clear();
    m_defaultPath = defaultPath;
    if (m_defaultPath.isEmpty()) {
        m_defaultPath = QDir(QCoreApplication::applicationDirPath()).filePath("config/default-home-layout.json");
        if (!QFile::exists(m_defaultPath)) m_defaultPath = QStringLiteral(":/config/default-home-layout.json");
    }
    m_userPath = userPath.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("home-layout.json")
        : userPath;

    QFile defaults(m_defaultPath);
    m_defaultTheme.clear();
    if (!defaults.open(QIODevice::ReadOnly) || !loadDocument(defaults.read(HomeLayout::MaxLayoutSize + 1), false)) return false;
    m_defaultTheme=appearance();
    QFile user(m_userPath);
    if (user.exists() && user.open(QIODevice::ReadOnly)) {
        const auto defaultItems = m_items;
        const auto defaultCategories = m_categories;
        const auto defaultBrand = m_brandVisible;
        const auto defaultStart = m_backgroundStart;
        const auto defaultEnd = m_backgroundEnd;
        const auto defaultRoundness = m_tileRoundness;
        if (!loadDocument(user.read(HomeLayout::MaxLayoutSize + 1), true)) {
            m_items = defaultItems; m_brandVisible = defaultBrand;
            m_categories = defaultCategories;
            m_backgroundStart = defaultStart; m_backgroundEnd = defaultEnd;
            m_tileRoundness = defaultRoundness;
        }
    }
    emit layoutChanged();
    emit skinsChanged();
    return true;
}

bool HomeLayout::loadDocument(const QByteArray &data, bool userDocument)
{
    if (data.size() > 4 * 1024 * 1024) { setError(tr("Home layout exceeds the supported size.")); return false; }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    const int version = document.object().value("schemaVersion").toInt();
    const auto entries = document.object().value(version == 1 ? "items" : "components");
    if (error.error != QJsonParseError::NoError || !document.isObject()
        || (version != 1 && version != 2) || !entries.isArray() || data.size() > 4 * 1024 * 1024
        || entries.toArray().size() > 1024) {
        setError(userDocument ? tr("User Home layout is invalid; defaults were restored.")
                              : tr("Default Home layout is invalid."));
        return false;
    }
    const auto object = document.object();
    QJsonArray pages;
    if (object.contains("pages") && (!object["pages"].isArray() || object["pages"].toArray().size() > 64 || m_owner)) {
        setError(tr("Invalid page collection.")); return false;
    }
    QSet<QString> pageIds;
    for (const auto &entry : object["pages"].toArray()) {
        const auto page = entry.toObject(); const auto id = page["id"].toString();
        HomeLayout validator(m_registry); validator.m_owner = this;
        if (!ComponentSchema::safeId(id) || pageIds.contains(id) || page["name"].toString().trimmed().isEmpty()
            || page["name"].toString().size() > 80 || !page["layout"].isObject()
            || !validator.loadDocument(QJsonDocument(page["layout"].toObject()).toJson(), false)) {
            setError(tr("A page has an invalid identity, name or layout.")); return false;
        }
        pageIds.insert(id); pages.append(page);
    }
    const auto homePage=object.value("homePageId");
    const auto rootPage=object.value("rootPage").toObject();
    const auto editorAppearance=object.value("studioAppearance").toObject().toVariantMap();
    if(object.contains("studioAppearance")) {
        auto effective=appearanceDefaults();const auto packaged=studioOptions().value("appearance").toMap();
        for(auto it=packaged.cbegin();it!=packaged.cend();++it)effective[it.key()]=it.value();
        for(auto it=editorAppearance.cbegin();it!=editorAppearance.cend();++it)effective[it.key()]=it.value();
        if(m_owner || !object["studioAppearance"].isObject() || !readableStudioAppearance(effective)) {setError(tr("Studio colors must keep text and focus readable on normal and selected surfaces."));return false;}
    }
    if(object.contains("rootPage")) {
        bool valid=!m_owner && object["rootPage"].isObject();
        for(auto it=rootPage.begin();it!=rootPage.end();++it)if(!QStringList{"name","visible","position"}.contains(it.key()))valid=false;
        const auto position=rootPage.value("position");
        if(!rootPage.value("name").isString() || rootPage.value("name").toString().trimmed().isEmpty() || rootPage.value("name").toString().size()>80 || rootPage.value("name").toString().contains(QChar::Null)
            || !rootPage.value("visible").isBool() || !position.isDouble() || position.toDouble()!=position.toInt() || position.toInt()<0 || position.toInt()>pages.size()
            || (!rootPage.value("visible").toBool() && homePage.toString().isEmpty()) || pages.size()+(rootPage.value("visible").toBool()?1:0)>64)valid=false;
        if(!valid){setError(tr("Root page metadata is invalid. Nothing changed."));return false;}
    }
    else if(!m_owner && pages.size()>=64){setError(tr("The interface exceeds the 64-page limit including Home."));return false;}
    if(object.contains("homePageId") && (m_owner || !homePage.isString() || (!homePage.toString().isEmpty() && !pageIds.contains(homePage.toString())))) {
        setError(tr("The designated Home page does not exist."));return false;
    }
    const auto theme = object.value("theme").toObject();
    const auto composition = object.value("composition").toObject().toVariantMap();
    if ((object.contains("composition") && !object["composition"].isObject()) || !ComponentSchema::validComposition(composition)) {
        setError(tr("Home composition is invalid.")); return false;
    }
    const QColor start(theme.contains("backgroundStart")?theme.value("backgroundStart").toString():m_defaultTheme.value("backgroundStart",m_initialBackgroundStart).toString());
    const QColor end(theme.contains("backgroundEnd")?theme.value("backgroundEnd").toString():m_defaultTheme.value("backgroundEnd",m_initialBackgroundEnd).toString());
    if (!start.isValid() || !end.isValid()) { setError(tr("Home theme contains invalid colors.")); return false; }
    auto presentation = appearanceDefaults();
    for(auto it=presentation.begin();it!=presentation.end();++it)if(m_defaultTheme.contains(it.key()))it.value()=m_defaultTheme.value(it.key());
    for (auto it = presentation.begin(); it != presentation.end(); ++it)
        if (theme.contains(it.key())) it.value() = theme.value(it.key()).toVariant();
    if (!normalizeAppearance(presentation)) { setError(tr("Home appearance contains invalid values.")); return false; }
    QSet<QString> ids;
    QSet<QString> categoryIds;
    const auto categoryArray = object.value("categories").toArray();
    if (categoryArray.size() > HomeLayout::MaxLayoutCategories) { setError(tr("Too many Home categories.")); return false; }
    for (const auto &value : categoryArray) {
        const auto category = value.toObject();
        const auto id = category.value("id").toString();
        if (id.isEmpty() || category.value("name").toString().trimmed().isEmpty() || categoryIds.contains(id)
            || (category.contains("presentation") && (!category["presentation"].isObject()
                || !ComponentSchema::validPresentation(category["presentation"].toObject().toVariantMap(), true)))) {
            setError(tr("Home layout contains an invalid or duplicate category.")); return false;
        }
        categoryIds.insert(id);
    }
    QJsonArray components;
    for (const auto &value : entries.toArray()) {
        QString componentError;
        const auto normalized = ComponentSchema::component(value.toObject().toVariantMap(), version == 1, &componentError);
        const auto item = QJsonObject::fromVariantMap(normalized);
        const auto kind = item.value("kind").toString();
        const auto id = item.value("id").toString();
        const auto actionType = item.value("action").toObject().value("type").toString(defaultActionTypeForKind(item)).trimmed();
        if (normalized.isEmpty() || item.value("componentType") == "media-shelf" || ids.contains(id) || (kind != "app" && kind != "widget" && kind != "action")
            || (kind == "app" && actionType == QStringLiteral("launch-app") && item.value("appId").toString().isEmpty())
            || (kind == "action" && item.value("name").toString().trimmed().isEmpty())
            || (kind == "widget" && item.value("widgetType").toString().isEmpty())
            || (!item.value("categoryId").toString().isEmpty() && !categoryIds.contains(item.value("categoryId").toString()))
            || (!actionType.isEmpty() && normalizedActionForItem(item).isEmpty())
            || (item.contains("opacity") && (!item.value("opacity").isDouble() || item.value("opacity").toDouble() < 0.0 || item.value("opacity").toDouble() > 1.0))) {
            setError(tr("Home layout contains an invalid or duplicate item.")); return false;
        }
        ids.insert(id);
        components.append(item);
    }
    QString hierarchyError;
    if (!ComponentSchema::hierarchyFits(components, composition, categoryArray,&hierarchyError)) {
        setError(hierarchyError); return false;
    }
    m_items = components;
    m_pages = pages;
    m_homePageId = homePage.toString();
    m_rootPage=rootPage;
    m_studioAppearance=editorAppearance;
    m_composition = composition;
    m_appearance = presentation;
    m_themeKeys.clear();for(const auto &key:theme.keys())m_themeKeys.insert(key);
    if (userDocument && version == 1) m_legacyDocument = data;
    m_categories = categoryArray;
    m_brandVisible = theme.value("brandVisible").toBool(false);
    m_backgroundStart = start.name();
    m_backgroundEnd = end.name();
    m_tileRoundness = qBound(0.0, theme.value("tileRoundness").toDouble(0.18), 1.0);
    setError({});
    return true;
}

bool HomeLayout::addAppTile(const QString &appId)
{
    if (!m_registry->find(appId)) return false;
    for (const auto &value : m_items)
        if (value.toObject().value("appId").toString() == appId) return true;
    QString id = appId;
    int suffix = 2;
    const auto idExists = [this](const QString &candidate) {
        for (const auto &value : m_items) if (value.toObject().value("id") == candidate) return true;
        return false;
    };
    while (idExists(id)) id = appId + '-' + QString::number(suffix++);
    QJsonObject tile{{"kind", "app"}, {"id", id}, {"appId", appId},
                     {"action", QJsonObject{{"type", "launch-app"}, {"appId", appId}}},
                     {"size", "standard"}};
    const auto section = m_registry->find(appId)->section;
    for (const auto &value : m_categories) {
        const auto category = value.toObject();
        if (category.value("name").toString().compare(section, Qt::CaseInsensitive) == 0) { tile.insert("categoryId", category.value("id")); break; }
    }
    m_items.append(tile);
    if (!save()) { m_items.removeLast(); return false; }
    emit layoutChanged(); return true;
}

bool HomeLayout::addActionTile(const QString &name, const QString &actionType, const QString &target, const QString &categoryId, const QString &openWith)
{
    const auto cleanName = name.trimmed().left(80);
    const auto cleanActionType = actionType.trimmed();
    const auto cleanTarget = target.trimmed();
    const auto cleanOpenWith = openWith.trimmed();
    const bool captureInput = cleanActionType == QStringLiteral("switch-input");
    QVariantMap requestedAction{{"type", cleanActionType}};
    if (captureInput) requestedAction["input"] = cleanTarget.toInt();
    else { requestedAction["target"] = cleanTarget; requestedAction["openWith"] = cleanOpenWith; }
    const auto normalizedAction = ComponentSchema::action(requestedAction);
    if (normalizedAction.isEmpty()) return false;
    if (cleanName.isEmpty() || cleanTarget.isEmpty() || !isSupportedActionType(cleanActionType)
        || cleanActionType == QStringLiteral("launch-app") || cleanActionType == QStringLiteral("desktop-mode")
        || (captureInput && !cleanOpenWith.isEmpty())
        || (!captureInput && (!isSafeActionId(cleanOpenWith) || (!cleanOpenWith.isEmpty() && cleanOpenWith != QStringLiteral("system") && !m_registry->find(cleanOpenWith)))))
        return false;
    if (!captureInput && !cleanOpenWith.isEmpty() && cleanOpenWith != QStringLiteral("system")
        && !m_registry->find(cleanOpenWith)->handledActions.contains(cleanActionType)) {
        return false;
    }
    if (!categoryId.isEmpty()) {
        bool found = false;
        for (const auto &value : m_categories)
            if (value.toObject().value("id").toString() == categoryId) found = true;
        if (!found) return false;
    }

    auto base = cleanName.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-|-$")));
    if (base.isEmpty()) base = QStringLiteral("tile");
    QString id = base;
    int suffix = 2;
    const auto idExists = [this](const QString &candidate) {
        for (const auto &value : m_items)
            if (value.toObject().value("id").toString() == candidate) return true;
        return false;
    };
    while (idExists(id)) id = base + QStringLiteral("-") + QString::number(suffix++);

    const QJsonObject action = QJsonObject::fromVariantMap(normalizedAction);
    QJsonObject tile{{"kind", "action"}, {"id", id}, {"name", cleanName}, {"size", "standard"},
                     {"action", action}};
    if (!categoryId.isEmpty()) tile.insert(QStringLiteral("categoryId"), categoryId);
    m_items.append(tile);
    if (!save()) { m_items.removeLast(); return false; }
    emit layoutChanged();
    return true;
}

bool HomeLayout::removeAppTile(const QString &appId)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).toObject().value("appId").toString() == appId) {
            const auto removed = m_items.takeAt(i);
            if (!save()) { m_items.insert(i, removed); return false; }
            emit layoutChanged(); return true;
        }
    }
    return true;
}

bool HomeLayout::moveItem(const QString &id, int offset)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).toObject().value("id").toString() == id) {
            const auto target = qBound<qsizetype>(0, i + offset, m_items.size() - 1);
            if (target == i) return true;
            const auto previousItems = m_items;
            const auto item = m_items.takeAt(i); m_items.insert(target, item);
            if (!save()) { m_items = previousItems; return false; }
            emit layoutChanged(); return true;
        }
    }
    return false;
}

bool HomeLayout::removeItem(const QString &id)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).toObject().value("id").toString() == id) {
            const auto removed = m_items.takeAt(i);
            if (!save()) { m_items.insert(i, removed); return false; }
            emit layoutChanged(); return true;
        }
    }
    return false;
}

bool HomeLayout::addWidget(const QString &widgetType)
{
    if (widgetType != QStringLiteral("clock") && widgetType != "text") return false;
    QString id = widgetType;
    int suffix = 2;
    const auto idExists = [this](const QString &candidate) {
        for (const auto &value : m_items)
            if (value.toObject().value("id").toString() == candidate) return true;
        return false;
    };
    while (idExists(id)) id = widgetType + QString::number(suffix++);
    m_items.append(QJsonObject{{"kind", "widget"}, {"id", id}, {"widgetType", widgetType}, {"size", "standard"}});
    if(widgetType=="text"){auto item=m_items.last().toObject();item["name"]="Text";item["presentation"]=QJsonObject{{"variant","text"},{"subtitle","Your message"}};m_items.replace(m_items.size()-1,item);}
    if (!save()) { m_items.removeLast(); return false; }
    emit layoutChanged(); return true;
}

bool HomeLayout::setItemSize(const QString &id, const QString &size)
{
    if (size != QStringLiteral("compact") && size != QStringLiteral("standard") && size != QStringLiteral("wide")) return false;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items.at(i).toObject();
        if (item.value("id").toString() != id) continue;
        const auto previous = item.value("size");
        item.insert("size", size); m_items.replace(i, item);
        if (!save()) { item.insert("size", previous); m_items.replace(i, item); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::setItemOpacity(const QString &id, double opacity)
{
    if (!qIsFinite(opacity)) return false;
    opacity = qBound(ComponentSchema::MinOpacity, opacity, ComponentSchema::MaxOpacity);
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items.at(i).toObject(); if (item.value("id").toString() != id) continue;
        const auto previous = item.value("opacity"); item.insert("opacity", opacity); m_items.replace(i, item);
        if (!save()) { if (previous.isUndefined()) item.remove("opacity"); else item.insert("opacity", previous); m_items.replace(i, item); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::setTileOption(const QString &id, const QString &option, bool enabled)
{
    static const QSet<QString> options{QStringLiteral("showTitle"), QStringLiteral("showType"),
                                       QStringLiteral("showLogo"), QStringLiteral("logoFill")};
    if (!options.contains(option)) return false;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action"))) continue;
        const auto previous = item.value(option);
        item.insert(option, enabled); m_items.replace(i, item);
        if (!save()) { if (previous.isUndefined()) item.remove(option); else item.insert(option, previous); m_items.replace(i, item); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::setTileAppearance(const QString &id, const QString &symbol, const QString &accent)
{
    const auto cleanSymbol = symbol.trimmed().left(HomeLayout::MaxTileSymbolLength);
    const QColor accentColor(accent);
    if (cleanSymbol.isEmpty() || !accentColor.isValid())
        return false;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
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
        if (!save()) {
            if (previousSymbol.isUndefined()) item.remove(QStringLiteral("symbol")); else item.insert(QStringLiteral("symbol"), previousSymbol);
            if (previousAccent.isUndefined()) item.remove(QStringLiteral("accent")); else item.insert(QStringLiteral("accent"), previousAccent);
            if (!previousLogo.isUndefined()) item.insert(QStringLiteral("customLogo"), previousLogo);
            m_items.replace(i, item);
            return false;
        }
        emit layoutChanged();
        return true;
    }
    return false;
}

bool HomeLayout::setTileLogo(const QString &id, const QString &sourceUrl)
{
    const auto sourcePath = QUrl(sourceUrl).isLocalFile() ? QUrl(sourceUrl).toLocalFile() : sourceUrl;
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly) || source.size() > HomeLayout::MaxArtworkSize) return false;
    const auto data = source.readAll();
    const bool png = data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"));
    const bool jpeg = data.startsWith(QByteArray::fromHex("ffd8ff"));
    if (data.size() < 8 || (!png && !jpeg)) return false;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action"))) continue;
        const auto artworkDir = QDir(QFileInfo(m_userPath).absolutePath()).filePath("artwork");
        if (!QDir().mkpath(artworkDir)) return false;
        const auto relativePath = QStringLiteral("artwork/logo-") + QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()) + (png ? QStringLiteral(".png") : QStringLiteral(".jpg"));
        QSaveFile output(QDir(QFileInfo(m_userPath).absolutePath()).filePath(relativePath));
        if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) return false;
        const auto previous = item.value("customLogo");
        item.insert("customLogo", relativePath); item.insert("showLogo", true); m_items.replace(i, item);
        if (!save()) { if (previous.isUndefined()) item.remove("customLogo"); else item.insert("customLogo", previous); m_items.replace(i, item); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::clearTileLogo(const QString &id)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value("kind").toString();
        if (item.value("id").toString() != id || (kind != QStringLiteral("app") && kind != QStringLiteral("action"))) continue;
        const auto relativePath = item.value("customLogo").toString();
        if (relativePath.isEmpty()) return true;
        item.remove("customLogo"); m_items.replace(i, item);
        if (!save()) { item.insert("customLogo", relativePath); m_items.replace(i, item); return false; }
        const auto *owner=m_owner?m_owner:this;
        if(!owner->documentData().contains(QJsonDocument(QJsonArray{relativePath}).toJson(QJsonDocument::Compact).mid(1).chopped(1)))
            QFile::remove(QDir(QFileInfo(m_userPath).absolutePath()).filePath(relativePath));
        emit layoutChanged(); return true;
    }
    return false;
}

QString HomeLayout::addCategory(const QString &name)
{
    const auto cleanName = name.trimmed().left(60);
    if (cleanName.isEmpty()) return {};
    auto base = cleanName.toLower(); base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-|-$"))); if (base.isEmpty()) base = QStringLiteral("category");
    QString id = base; int suffix = 2;
    const auto exists = [this](const QString &candidate) { for (const auto &v : m_categories) if (v.toObject().value("id").toString() == candidate) return true; return false; };
    while (exists(id)) id = base + QStringLiteral("-") + QString::number(suffix++);
    m_categories.append(QJsonObject{{"id", id}, {"name", cleanName}});
    if (!save()) { m_categories.removeLast(); return {}; }
    emit layoutChanged(); return id;
}

bool HomeLayout::renameCategory(const QString &categoryId, const QString &name)
{
    const auto cleanName = name.trimmed().left(60); if (cleanName.isEmpty()) return false;
    for (qsizetype i = 0; i < m_categories.size(); ++i) {
        auto category = m_categories.at(i).toObject(); if (category.value("id").toString() != categoryId) continue;
        const auto previous = category.value("name"); category.insert("name", cleanName); m_categories.replace(i, category);
        if (!save()) { category.insert("name", previous); m_categories.replace(i, category); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::removeCategory(const QString &categoryId)
{
    for (qsizetype i = 0; i < m_categories.size(); ++i) {
        if (m_categories.at(i).toObject().value("id").toString() != categoryId) continue;
        const auto oldCategories = m_categories, oldItems = m_items; m_categories.removeAt(i);
        for (qsizetype j = 0; j < m_items.size(); ++j) { auto item = m_items.at(j).toObject(); if (item.value("categoryId").toString() == categoryId) { item.remove("categoryId"); m_items.replace(j, item); } }
        if (!save()) { m_categories = oldCategories; m_items = oldItems; return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::moveCategory(const QString &categoryId, int offset)
{
    for (qsizetype i = 0; i < m_categories.size(); ++i) {
        if (m_categories.at(i).toObject().value("id").toString() != categoryId) continue;
        const auto target = qBound<qsizetype>(0, i + offset, m_categories.size() - 1);
        if (target == i) return true;
        const auto category = m_categories.takeAt(i); m_categories.insert(target, category);
        if (!save()) { m_categories.takeAt(target); m_categories.insert(i, category); return false; }
        emit layoutChanged(); return true;
    }
    return false;
}

bool HomeLayout::assignItemCategory(const QString &itemId, const QString &categoryId)
{
    if (!categoryId.isEmpty()) { bool found = false; for (const auto &v : m_categories) if (v.toObject().value("id").toString() == categoryId) found = true; if (!found) return false; }
    for (qsizetype i = 0; i < m_items.size(); ++i) { auto item = m_items.at(i).toObject(); if (item.value("id").toString() != itemId) continue; const auto previous = item.value("categoryId"); if (categoryId.isEmpty()) item.remove("categoryId"); else item.insert("categoryId", categoryId); m_items.replace(i, item); if (!save()) { if (previous.isUndefined()) item.remove("categoryId"); else item.insert("categoryId", previous); m_items.replace(i, item); return false; } emit layoutChanged(); return true; }
    return false;
}

bool HomeLayout::setBrandVisible(bool visible)
{
    if (m_brandVisible == visible && m_themeKeys.contains("brandVisible")) return true;
    const auto previousKeys=m_themeKeys;m_themeKeys.insert("brandVisible");
    const auto previousVisible=m_brandVisible;
    m_brandVisible = visible;
    if (!save()) {m_themeKeys=previousKeys; m_brandVisible = previousVisible; return false; }
    emit layoutChanged(); return true;
}

bool HomeLayout::setBackground(const QString &start, const QString &end)
{
    const QColor startColor(start), endColor(end);
    if (!startColor.isValid() || !endColor.isValid()) return false;
    const auto previousStart = m_backgroundStart, previousEnd = m_backgroundEnd;
    const auto previousKeys=m_themeKeys;m_themeKeys.insert("backgroundStart");m_themeKeys.insert("backgroundEnd");
    m_backgroundStart = startColor.name(); m_backgroundEnd = endColor.name();
    if (!save()) {m_themeKeys=previousKeys; m_backgroundStart = previousStart; m_backgroundEnd = previousEnd; return false; }
    emit layoutChanged(); return true;
}

bool HomeLayout::setTileRoundness(double roundness)
{
    if (!qIsFinite(roundness)) return false;
    roundness = qBound(0.0, roundness, 1.0);
    if (qFuzzyCompare(m_tileRoundness, roundness) && m_themeKeys.contains("tileRoundness")) return true;
    const auto previousKeys=m_themeKeys;m_themeKeys.insert("tileRoundness");
    const auto previous = m_tileRoundness; m_tileRoundness = roundness;
    if (!save()) {m_themeKeys=previousKeys; m_tileRoundness = previous; return false; }
    emit layoutChanged(); return true;
}

bool HomeLayout::applyTheme(const QVariantMap &theme, bool includeSuggestedArtwork)
{
    auto presentation = appearanceDefaults();
    for (auto it = presentation.begin(); it != presentation.end(); ++it)
        if (theme.contains(it.key())) it.value() = theme.value(it.key());
    if (!normalizeAppearance(presentation)) return false;
    const QColor startColor(theme.value(QStringLiteral("backgroundStart")).toString());
    const QColor endColor(theme.value(QStringLiteral("backgroundEnd")).toString());
    const auto roundness = theme.contains(QStringLiteral("tileRoundness"))
        ? theme.value(QStringLiteral("tileRoundness")).toDouble()
        : -1.0;
    const auto tileOpacity = theme.contains(QStringLiteral("tileOpacity"))
        ? theme.value(QStringLiteral("tileOpacity")).toDouble()
        : 1.0;
    if (!startColor.isValid() || !endColor.isValid() || !qIsFinite(roundness) || !qIsFinite(tileOpacity) || roundness < 0.0 || roundness > 1.0
        || tileOpacity < 0.2 || tileOpacity > 1.0)
        return false;
    const auto previousStart = m_backgroundStart;
    const auto previousEnd = m_backgroundEnd;
    const auto previousRoundness = m_tileRoundness;
    const auto previousItems = m_items;
    const auto previousAppearance = m_appearance;
    const auto previousKeys=m_themeKeys;for(auto it=presentation.cbegin();it!=presentation.cend();++it)m_themeKeys.insert(it.key());m_themeKeys.insert("backgroundStart");m_themeKeys.insert("backgroundEnd");m_themeKeys.insert("tileRoundness");
    m_appearance = presentation;
    m_backgroundStart = startColor.name();
    m_backgroundEnd = endColor.name();
    m_tileRoundness = roundness;
    const auto suggestions = theme.value(QStringLiteral("suggestedArtwork")).toMap();
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        auto item = m_items.at(i).toObject();
        const auto kind = item.value(QStringLiteral("kind")).toString();
        if (kind != QStringLiteral("app") && kind != QStringLiteral("action"))
            continue;
        item.insert(QStringLiteral("opacity"), tileOpacity);
        if (includeSuggestedArtwork) {
            const QStringList keys{item.value(QStringLiteral("appId")).toString(),
                                   item.value(QStringLiteral("id")).toString(),
                                   item.value(QStringLiteral("sourceType")).toString(),
                                   item.value(QStringLiteral("name")).toString().toLower()};
            for (const auto &key : keys) {
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
    if (!save()) {
        m_themeKeys=previousKeys;
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

bool HomeLayout::resetToDefaults()
{
    if (m_owner || !m_pages.isEmpty()) {
        QFile defaults(m_defaultPath);
        if(!defaults.open(QIODevice::ReadOnly)){setError(tr("Unable to read defaults."));return false;}
        return applySkinDocument(QJsonDocument::fromJson(defaults.readAll()).object(),{});
    }
    QFile::remove(m_userPath);
    return load(m_defaultPath, m_userPath);
}

QByteArray HomeLayout::documentData() const
{
    auto theme = QJsonObject::fromVariantMap(appearance());
    theme.insert("brandVisible", m_brandVisible);
    for(const auto &key:theme.keys())if(!m_themeKeys.contains(key))theme.remove(key);
    QJsonArray components;
    for (const auto &entry : m_items) {
        const auto normalized = ComponentSchema::component(entry.toObject().toVariantMap(), true);
        if (normalized.isEmpty()) return {};
        components.append(QJsonObject::fromVariantMap(normalized));
    }
    QJsonObject object{{"schemaVersion", 2}, {"theme", theme}, {"composition", QJsonObject::fromVariantMap(m_composition)}, {"categories", m_categories}, {"components", components}};
    if (!m_owner) {object.insert("pages",m_pages);object.insert("homePageId",m_homePageId);if(!m_rootPage.isEmpty())object.insert("rootPage",m_rootPage);if(!m_studioAppearance.isEmpty())object.insert("studioAppearance",QJsonObject::fromVariantMap(m_studioAppearance));}
    return QJsonDocument(object).toJson();
}

bool HomeLayout::save()
{
    if (m_draftOnly) {
        QString error;
        const bool ok = validateStudioDocument(documentData(), &error);
        setError(error);
        return ok;
    }
    if (m_owner) {
        const bool ok=m_owner->storePage(m_pageId,QJsonDocument::fromJson(documentData()).object());
        setError(ok?QString():m_owner->errorString());return ok;
    }
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    const auto data = documentData();
    if (data.isEmpty() || data.size()>4*1024*1024) { setError(tr("The design is invalid or exceeds 4 MiB.")); return false; }
    if (!m_legacyDocument.isEmpty() && !QFile::exists(m_userPath + ".v1.backup.json")) {
        QSaveFile backup(m_userPath + ".v1.backup.json");
        if (!backup.open(QIODevice::WriteOnly) || backup.write(m_legacyDocument) != m_legacyDocument.size() || !backup.commit()) {
            setError(tr("Unable to back up the previous Home layout.")); return false;
        }
    }
    QSaveFile file(m_userPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        setError(tr("Unable to save Home layout.")); return false;
    }
    m_legacyDocument.clear();
    setError({}); return true;
}

void HomeLayout::setError(const QString &message)
{
    if (m_errorString == message) return;
    m_errorString = message; emit errorStringChanged();
}

QByteArray HomeLayout::committedDocument() const
{
    return documentData();
}

bool HomeLayout::validateStudioDocument(const QByteArray &data, QString *error)
{
    const auto count = [](const QJsonObject &document) {
        return document[document["schemaVersion"].toInt() == 1 ? "items" : "components"].toArray().size();
    };
    const auto document = QJsonDocument::fromJson(data).object();
    qsizetype nodes = count(document);
    for (const auto &page : document["pages"].toArray()) nodes += count(page.toObject()["layout"].toObject());
    if (nodes > 1024) { if (error) *error = tr("The interface exceeds 1024 components across its pages."); return false; }
    AppRegistry registry;
    HomeLayout validator(&registry);
    const bool ok = validator.loadDocument(data, false);
    if (error) *error = validator.errorString();
    return ok;
}

QObject *HomeLayout::beginStudio()
{
    if (m_studio) { m_studio->invalidate(); m_studio->deleteLater(); }
    m_studio = new StudioSession(this);
    m_studio->bind(this);
    return m_studio;
}

bool HomeLayout::applyStudioDraft(const QByteArray &draft, const QString &focusComponentId)
{
    // T16.5 — validate the candidate without touching the live model first.
    // Snapshot live state; restore it byte-for-byte on any failure so the
    // applied model, original file bytes and focus origin survive intact.
    const QByteArray previousBytes = documentData();
    const QJsonArray previousItems = m_items;
    const QJsonArray previousCategories = m_categories;
    const QJsonArray previousPages = m_pages;
    const QString previousHomePage=m_homePageId;
    const auto previousRootPage=m_rootPage;
    const auto previousStudioAppearance=m_studioAppearance;
    const QVariantMap previousAppearance = m_appearance;
    const auto previousThemeKeys=m_themeKeys;
    const QVariantMap previousComposition = m_composition;
    const QString previousStart = m_backgroundStart, previousEnd = m_backgroundEnd;
    const double previousRoundness = m_tileRoundness;
    const bool previousBrand = m_brandVisible;
    const QString previousFocus = m_focusOrigin;

    HomeLayout probe(m_registry);
    probe.m_defaultTheme=m_defaultTheme;
    probe.m_owner = m_owner; // page-scoped drafts validate against the owner
    if (!probe.loadDocument(draft, false)) {
        setError(probe.errorString().isEmpty()
                     ? tr("The draft could not be applied — nothing changed.")
                     : probe.errorString());
        return false;
    }
    // Adopt the validated in-memory state, then persist atomically.
    m_items = probe.m_items;
    m_categories = probe.m_categories;
    if (!m_owner) {m_pages = probe.m_pages;m_homePageId=probe.m_homePageId;m_rootPage=probe.m_rootPage;m_studioAppearance=probe.m_studioAppearance;}
    m_appearance = probe.m_appearance;
    m_themeKeys=probe.m_themeKeys;
    m_composition = probe.m_composition;
    m_backgroundStart = probe.m_backgroundStart;
    m_backgroundEnd = probe.m_backgroundEnd;
    m_tileRoundness = probe.m_tileRoundness;
    m_brandVisible = probe.m_brandVisible;
    if (!focusComponentId.isEmpty()) m_focusOrigin = focusComponentId;
    if (!save()) {
        // Atomic-save failure: restore every field and keep the draft alive
        // in the caller (StudioSession still holds it) for retry.
        m_items = previousItems;
        m_categories = previousCategories;
        m_pages = previousPages;
        m_homePageId=previousHomePage;
        m_rootPage=previousRootPage;
        m_studioAppearance=previousStudioAppearance;
        m_appearance = previousAppearance;
        m_themeKeys=previousThemeKeys;
        m_composition = previousComposition;
        m_backgroundStart = previousStart;
        m_backgroundEnd = previousEnd;
        m_tileRoundness = previousRoundness;
        m_brandVisible = previousBrand;
        m_focusOrigin = previousFocus;
        syncPages();
        emit layoutChanged();
        if (documentData() != previousBytes) {
            // Last-resort guard: in-memory state failed to round-trip back.
            // Reload from disk (no rewrite) so the model matches the file.
            load(m_defaultPath, m_userPath);
            m_focusOrigin = previousFocus;
            emit layoutChanged();
        }
        return false; // save() already set the actionable error string
    }
    emit layoutChanged();
    setError({});
    return true;
}
