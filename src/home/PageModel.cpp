#include "home/PageModel.h"

#include "home/HomeLayout.h"
#include "home/ComponentSchema.h"

#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>
#include <QUrl>
#include <QtMath>
#include <functional>

namespace {

QVariantMap normalizedActionForItem(const QJsonObject &object)
{
    const auto kind = object.value("kind").toString();
    if (kind == QStringLiteral("app")) {
        return QJsonObject{{"type", "launch-app"}, {"appId", object.value("appId")}}.toVariantMap();
    }
    if (kind == QStringLiteral("action") && object.contains("action")) {
        return object.value("action").toObject().toVariantMap();
    }
    return QVariantMap{};
}

} // namespace

PageModel::PageModel(HomeLayout *owner, const QString &id, QObject *parent)
    : QObject(parent)
    , m_owner(owner)
    , m_pageId(id)
{}

bool PageModel::loadDocument(const QByteArray &data, bool /*userDocument*/)
{
    if (!m_owner) return false;
    const auto doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        m_owner->setError(tr("Invalid page document."));
        return false;
    }
    const auto object = doc.object();
    if (!object.contains("composition") || !object.value("composition").isObject()) {
        m_owner->setError(tr("Page document is missing a composition."));
        return false;
    }
    m_pageDocument = object;
    m_documentLoaded = true;
    m_owner->setError({});
    emit documentLoaded();
    emit compositionChanged();
    emit gridChanged();
    emit placementsChanged();
    emit presentationChanged();
    return true;
}

bool PageModel::storePage()
{
    if (!m_owner) return false;
    auto document = m_pageDocument;
    QJsonArray components;
    for (const auto &entry : document.value("components").toArray()) {
        QString error;
        const auto normalized = ComponentSchema::component(entry.toObject().toVariantMap(), true, &error);
        if (normalized.isEmpty()) { m_owner->setError(error); return false; }
        components.append(QJsonObject::fromVariantMap(normalized));
    }
    document["components"] = components;
    document["schemaVersion"] = 2;
    const bool ok = m_owner->storePage(m_pageId, document);
    if (!ok) m_owner->setError(m_owner->errorString());
    return ok;
}

bool PageModel::setSectionPresentation(const QString &id, const QVariantMap &value)
{
    if (!ComponentSchema::validPresentation(value, true)) return false;
    const auto previous = m_pageDocument;
    auto categories = m_pageDocument.value("categories").toArray();
    for (qsizetype i = 0; i < categories.size(); ++i) {
        auto category = categories[i].toObject();
        if (category.value("id").toString() != id) continue;
        category["presentation"] = QJsonObject::fromVariantMap(value);
        categories[i] = category;
        m_pageDocument["categories"] = categories;
        if (!storePage()) { m_pageDocument = previous; return false; }
        emit placementsChanged();
        return true;
    }
    return false;
}

QVariantMap PageModel::document() const
{
    return m_pageDocument.toVariantMap();
}

QVariantMap PageModel::themeDefaults() const
{
    return {{"focusColor", "#c994a5"}, {"surfaceColor", "#202126"}, {"selectedColor", "#353039"},
            {"textColor", "#f5f1f3"}, {"mutedColor", "#aaa5af"}, {"typographyScale", 1.0},
            {"homeColumns", 3}, {"spacing", 20}, {"motionEnabled", true}, {"fontFamily", "DejaVu Sans"}};
}

QVariantList PageModel::resolveItems(const QJsonArray &components) const
{
    QVariantList result;
    const auto *registry = m_owner ? m_owner->registry() : nullptr;
    for (const auto &value : components) {
        const auto object = value.toObject();
        auto item = object.toVariantMap();
        const auto action = normalizedActionForItem(object);
        if (!action.isEmpty()) {
            item.insert("actionType", action.value(QStringLiteral("type")).toString());
            item.insert("action", action);
        }
        if (item.value("kind") == QStringLiteral("app")) {
            const auto appId = item.value("appId").toString();
            const auto *app = registry ? registry->find(appId) : nullptr;
            item.insert("name", app ? app->name : item.value("name", appId));
            item.insert("section", app ? app->section : QStringLiteral("Missing app"));
            const auto tileSymbol = item.value("symbol").toString();
            const auto tileAccent = item.value("accent").toString();
            item.insert("symbol", tileSymbol.isEmpty() ? (app ? app->symbol : QStringLiteral("?")) : tileSymbol);
            item.insert("accent", tileAccent.isEmpty() ? (app ? app->accent.name() : QStringLiteral("#64748B")) : tileAccent);
            item.insert("presentationMode", app ? app->presentationMode : QStringLiteral("native-tv"));
            item.insert("networkRequirement", app ? app->networkRequirement : QStringLiteral("none"));
        } else if (item.value("kind") == QStringLiteral("action")) {
            const auto section = item.value("section").toString();
            const auto symbol = item.value("symbol").toString();
            const auto accent = item.value("accent").toString();
            const auto networkRequirement = item.value("networkRequirement").toString();
            item.insert("section", section.isEmpty() ? QCoreApplication::translate("PageModel", "Custom") : section);
            item.insert("symbol", symbol.isEmpty() ? QStringLiteral("•") : symbol);
            item.insert("accent", accent.isEmpty() ? QStringLiteral("#64748B") : accent);
            item.insert("networkRequirement", networkRequirement.isEmpty() ? QStringLiteral("none") : networkRequirement);
        }
        const auto logoFile = item.value("customLogo").toString();
        item.insert("logoSource", logoFile.isEmpty() ? QString()
            : QUrl::fromLocalFile(QDir(QFileInfo(QString()).absolutePath()).filePath(logoFile)).toString());
        auto unavailable = ComponentSchema::unavailable(action, registry, QVariantList{});
        if (unavailable.isEmpty() && action.value("type").toString() == QStringLiteral("switch-input")) {
            unavailable = QCoreApplication::translate("PageModel", "Configure Settings → System → HDMI Inputs");
        }
        item.insert("unavailableReason", unavailable);
        result.append(ComponentSchema::component(item, true));
    }
    return result;
}

QVariantMap PageModel::composition() const
{
    if (m_documentLoaded && m_pageDocument.contains("composition"))
        return m_pageDocument.value("composition").toObject().toVariantMap();
    if (!m_owner) return QVariantMap{};
    return m_owner->composition();
}

bool PageModel::setComposition(const QVariantMap &value)
{
    if (!m_owner) return false;
    if (!ComponentSchema::validComposition(value)) {
        m_owner->setError(tr("Invalid page composition."));
        return false;
    }
    const auto grid = ComponentSchema::normalizedGrid(value.value("grid").toMap());
    const auto components = m_pageDocument.value("components").toArray();
    QJsonArray itemsForFit;
    for (const auto &v : components) itemsForFit.append(v);
    if (!ComponentSchema::layoutPlacementsFit(itemsForFit,value,m_pageDocument["categories"].toArray())) {
        m_owner->setError(tr("Placed widgets do not fit this page composition's grid."));
        return false;
    }
    const auto previous = m_pageDocument;
    m_pageDocument["composition"] = QJsonObject::fromVariantMap(value);
    if (!storePage()) { m_pageDocument = previous; return false; }
    m_documentLoaded = true;
    emit compositionChanged();
    emit gridChanged();
    emit placementsChanged();
    return true;
}

QVariantMap PageModel::grid() const
{
    if (m_documentLoaded && m_pageDocument.contains("composition")) {
        const auto comp = m_pageDocument.value("composition").toObject();
        if (comp.contains("grid"))
            return ComponentSchema::normalizedGrid(comp.value("grid").toObject().toVariantMap());
    }
    if (!m_owner) return QVariantMap{};
    return m_owner->grid();
}

bool PageModel::setGrid(const QVariantMap &value)
{
    if (!m_owner) return false;
    if (!ComponentSchema::validGrid(value)) {
        m_owner->setError(tr("Invalid page grid."));
        return false;
    }
    auto updated = grid();
    for (auto it = value.cbegin(); it != value.cend(); ++it) updated.insert(it.key(), it.value());
    const auto components = m_pageDocument.value("components").toArray();
    QJsonArray itemsForFit;
    for (const auto &v : components) itemsForFit.append(v);
    auto proposed=composition();proposed["grid"]=updated;
    if (!ComponentSchema::layoutPlacementsFit(itemsForFit, proposed,m_pageDocument["categories"].toArray())) {
        m_owner->setError(tr("This grid would move placed widgets outside its bounds."));
        return false;
    }
    auto config = composition();
    if (config.isEmpty()) config = {{"schemaVersion", 1}, {"enabled", true}};
    config["grid"] = updated;
    return setComposition(config);
}

QVariantList PageModel::placements() const
{
    QVariantList result;
    if (m_pageDocument.contains("components")) {
        for (const auto &value : m_pageDocument.value("components").toArray()) {
            const auto object = value.toObject();
            if (object.contains("placement")) {
                result.append(QVariantMap{{"id", object.value("id").toString()},
                                          {"placement", object.value("placement").toObject().toVariantMap()}});
            }
        }
    }
    return result;
}

QVariantMap PageModel::presentation() const
{
    if (m_documentLoaded && m_pageDocument.contains("presentation"))
        return m_pageDocument.value("presentation").toObject().toVariantMap();
    if (!m_owner) return QVariantMap{};
    return m_owner->composition().value("presentation").toMap();
}

bool PageModel::setPresentation(const QVariantMap &value)
{
    if (!m_owner) return false;
    const auto previous = m_pageDocument;
    m_pageDocument["presentation"] = QJsonObject::fromVariantMap(value);
    if (!storePage()) { m_pageDocument = previous; return false; }
    m_documentLoaded = true;
    emit presentationChanged();
    return true;
}

QVariantList PageModel::items() const
{
    QJsonArray components;
    if (m_pageDocument.contains("components"))
        components = m_pageDocument.value("components").toArray();
    if(m_owner && !m_owner->m_draftOnly){QJsonArray visible;for(const auto &entry:components)if(!entry.toObject().value("hidden").toBool())visible.append(entry);components=visible;}
    return resolveItems(components);
}

QVariantList PageModel::sections() const
{
    QVariantList result;
    if (!m_pageDocument.contains("categories")) {
        if (!m_owner) return {};
        return m_owner->sections();
    }
    const auto resolved = items();
    for (const auto &categoryValue : m_pageDocument.value("categories").toArray()) {
        const auto category = categoryValue.toObject();
        QVariantList categoryItems;
        for (const auto &itemValue : resolved)
            if (itemValue.toMap().value("categoryId").toString() == category.value("id").toString())
                categoryItems.append(itemValue);
        if (!categoryItems.isEmpty() || category.value("presentation").toObject().value("variant").toString() == QStringLiteral("library") || grid().value("mode")=="sections") {
            auto section = category.toVariantMap();
            section["items"] = categoryItems;
            result.append(section);
        }
    }
    QVariantList uncategorized;
    for (const auto &itemValue : resolved)
        if (itemValue.toMap().value("categoryId").toString().isEmpty()) uncategorized.append(itemValue);
    if (!uncategorized.isEmpty() || grid().value("mode")=="sections")
        result.append(QVariantMap{{"id", ""}, {"name", QCoreApplication::translate("PageModel", "Uncategorized")}, {"items", uncategorized}});
    return result;
}

QString PageModel::backgroundStart() const
{
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        if (theme.contains("backgroundStart")) return theme.value("backgroundStart").toString();
    }
    if (!m_owner) return QString();
    return m_owner->backgroundStart();
}

QString PageModel::backgroundEnd() const
{
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        if (theme.contains("backgroundEnd")) return theme.value("backgroundEnd").toString();
    }
    if (!m_owner) return QString();
    return m_owner->backgroundEnd();
}

QVariantMap PageModel::appearance() const
{
    QVariantMap result = themeDefaults();
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        for (const auto &key : theme.keys())
            result.insert(key, theme.value(key).toVariant());
    }
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        if (theme.contains("backgroundStart")) result["backgroundStart"] = theme.value("backgroundStart").toString();
        if (theme.contains("backgroundEnd")) result["backgroundEnd"] = theme.value("backgroundEnd").toString();
        if (theme.contains("tileRoundness")) result["tileRoundness"] = theme.value("tileRoundness").toDouble();
    }
    if (!m_owner) return result;
    const auto ownerAppearance = m_owner->appearance();
    for (auto it = ownerAppearance.cbegin(); it != ownerAppearance.cend(); ++it) {
        if (!result.contains(it.key())) result.insert(it.key(), it.value());
    }
    return result;
}

bool PageModel::setAppearance(const QVariantMap &changes)
{
    if (!m_owner) return false;
    auto normalized = changes;
    const auto defaults = themeDefaults();
    for (auto it = normalized.begin(); it != normalized.end(); ++it) {
        const auto key = it.key();
        if (key.endsWith("Color") || key == "backgroundStart" || key == "backgroundEnd") {
            const QColor color(it.value().toString());
            if (!color.isValid() || color.alpha() != 255) {
                m_owner->setError(tr("Choose valid opaque colors and appearance values within the displayed ranges."));
                return false;
            }
            it.value() = color.name();
        } else if (key == "fontFamily") {
            if (it.value().metaType().id() != QMetaType::QString || it.value().toString().trimmed().isEmpty()
                || it.value().toString().size() > 80 || it.value().toString().contains(QChar::Null)) {
                m_owner->setError(tr("Invalid font family."));
                return false;
            }
        } else if (key == "motionEnabled") {
            if (it.value().metaType().id() != QMetaType::Bool) {
                m_owner->setError(tr("motionEnabled must be a boolean."));
                return false;
            }
        } else {
            if (!defaults.contains(key) && key != "tileRoundness") {
                m_owner->setError(tr("Unknown appearance key."));
                return false;
            }
            if (it.value().metaType().id() == QMetaType::QString || it.value().metaType().id() == QMetaType::Bool) {
                m_owner->setError(tr("Appearance value must be a number."));
                return false;
            }
            bool ok = false; const double number = it.value().toDouble(&ok);
            const double low = key == "typographyScale" ? 0.8 : key == "homeColumns" ? 2 : key == "spacing" ? 8 : 0;
            const double high = key == "typographyScale" ? 1.3 : key == "homeColumns" ? 6 : key == "spacing" ? 40 : 1;
            if (!ok || !qIsFinite(number) || number < low || number > high
                || ((key == "homeColumns" || key == "spacing") && number != qFloor(number))) {
                m_owner->setError(tr("Appearance value out of range."));
                return false;
            }
        }
        if (!defaults.contains(key) && key != "backgroundStart" && key != "backgroundEnd" && key != "tileRoundness") {
            m_owner->setError(tr("Unknown appearance key."));
            return false;
        }
    }
    const auto previous = m_pageDocument;
    QJsonObject theme;
    if (m_pageDocument.contains("theme")) theme = m_pageDocument.value("theme").toObject();
    if (normalized.contains("backgroundStart")) theme.insert("backgroundStart", normalized.take("backgroundStart").toString());
    if (normalized.contains("backgroundEnd")) theme.insert("backgroundEnd", normalized.take("backgroundEnd").toString());
    if (normalized.contains("tileRoundness")) theme.insert("tileRoundness", normalized.take("tileRoundness").toDouble());
    for (auto it = normalized.cbegin(); it != normalized.cend(); ++it) theme.insert(it.key(), QJsonValue::fromVariant(it.value()));
    m_pageDocument["theme"] = theme;
    if (!storePage()) { m_pageDocument = previous; return false; }
    m_documentLoaded = true;
    emit presentationChanged();
    return true;
}

QString PageModel::wallpaperSource() const
{
    if (m_documentLoaded && m_pageDocument.contains("composition")) {
        const auto comp = m_pageDocument.value("composition").toObject();
        if (comp.contains("wallpaperSource")) return comp.value("wallpaperSource").toString();
    }
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        if (theme.contains("wallpaperSource")) return theme.value("wallpaperSource").toString();
    }
    if (!m_owner) return QString();
    return m_owner->wallpaperSource();
}

double PageModel::tileRoundness() const
{
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        if (theme.contains("tileRoundness"))
            return qBound(0.0, theme.value("tileRoundness").toDouble(0.18), 1.0);
    }
    if (!m_owner) return 0.18;
    return m_owner->tileRoundness();
}

bool PageModel::brandVisible() const
{
    if (m_documentLoaded && m_pageDocument.contains("theme")) {
        const auto theme = m_pageDocument.value("theme").toObject();
        if (theme.contains("brandVisible")) return theme.value("brandVisible").toBool(false);
    }
    if (!m_owner) return false;
    return m_owner->brandVisible();
}

QString PageModel::errorString() const
{
    if (!m_owner) return QString();
    return m_owner->errorString();
}

bool PageModel::addWidget(const QString &widgetType)
{
    if (!m_owner) return false;
    if (widgetType != QStringLiteral("clock") && widgetType != "text") return false;
    QString id = widgetType;
    int suffix = 2;
    const auto idExists = [this](const QString &candidate) {
        if (!m_pageDocument.contains("components")) return false;
        for (const auto &value : m_pageDocument.value("components").toArray())
            if (value.toObject().value("id").toString() == candidate) return true;
        return false;
    };
    while (idExists(id)) id = widgetType + QString::number(suffix++);
    QJsonObject item{{"kind", "widget"}, {"id", id}, {"widgetType", widgetType}, {"size", "standard"}};
    if (widgetType == "text") {
        item.insert("name", QStringLiteral("Text"));
        item.insert("presentation", QJsonObject{{"variant", "text"}, {"subtitle", QCoreApplication::translate("PageModel", "Your message")}});
    }
    const auto previous = m_pageDocument;
    QJsonArray components;
    if (m_pageDocument.contains("components")) components = m_pageDocument.value("components").toArray();
    components.append(item);
    m_pageDocument["components"] = components;
    if (!storePage()) { m_pageDocument = previous; return false; }
    m_documentLoaded = true;
    emit placementsChanged();
    return true;
}

bool PageModel::addButton()
{
    if (!m_owner) return false;
    QString id = QStringLiteral("button-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject item{{"kind", "action"}, {"id", id}, {"name", QCoreApplication::translate("PageModel", "Button")},
                     {"action", QJsonObject{{"type", "app-drawer"}}}};
    const auto previous = m_pageDocument;
    QJsonArray components;
    if (m_pageDocument.contains("components")) components = m_pageDocument.value("components").toArray();
    components.append(item);
    m_pageDocument["components"] = components;
    if (!storePage()) { m_pageDocument = previous; return false; }
    m_documentLoaded = true;
    emit placementsChanged();
    return true;
}

bool PageModel::setItemName(const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty() || name.trimmed().size() > 80) {
        m_owner->setError(tr("Use a card name of 1-80 characters."));
        return false;
    }
    return setItemField(id, [name](QJsonObject &item) { item.insert("name", name.trimmed()); });
}

bool PageModel::setItemAction(const QString &id, const QVariantMap &action)
{
    const auto normalized = ComponentSchema::action(action);
    if (normalized.isEmpty()) {
        m_owner->setError(tr("Choose a valid action and destination."));
        return false;
    }
    return setItemField(id, [normalized](QJsonObject &item) {
        item.insert("kind", QStringLiteral("action"));
        item.insert("action", QJsonObject::fromVariantMap(normalized));
        item.remove("appId");
        if (item.value("name").toString().isEmpty()) item.insert("name", QStringLiteral("Link"));
    });
}

bool PageModel::setItemPresentation(const QString &id, const QVariantMap &value)
{
    return setItemField(id, [value](QJsonObject &item) {
        item.insert("presentation", QJsonObject::fromVariantMap(value));
    });
}

QString PageModel::addLibraryWidget(const QString &name, const QString &kind)
{
    if (!m_owner) return QString();
    if (name.trimmed().isEmpty() || !QStringList{"movies", "shows"}.contains(kind)) return QString();
    auto comp = m_pageDocument.value("composition").toObject();
    if (!comp.value("enabled").toBool()) {
        comp.insert("schemaVersion", 1); comp.insert("enabled", true); comp.insert("fit", false);
        m_pageDocument["composition"] = comp;
    }
    const auto id = QStringLiteral("library-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto previous = m_pageDocument;
    auto categories = m_pageDocument.value("categories").toArray();
    categories.append(QJsonObject{{"id", id}, {"name", name.trimmed().left(60)},
        {"presentation", QJsonObject{{"variant", "library"}, {"height", 500}, {"libraryKind", kind},
                                    {"libraryView", "all"}, {"libraryLayout", "grid"}, {"libraryColumns", 4}}}});
    m_pageDocument["categories"] = categories;
    if (!storePage()) { m_pageDocument = previous; return QString(); }
    m_documentLoaded = true;
    emit placementsChanged();
    return id;
}

bool PageModel::setItemPlacement(const QString &id, const QVariantMap &placement)
{
    if (!m_owner) return false;
    auto proposed=m_pageDocument["components"].toArray();bool found=false;
    for(int i=0;i<proposed.size();++i){auto item=proposed[i].toObject();if(item["id"]!=id)continue;item["placement"]=QJsonObject::fromVariantMap(placement);proposed[i]=item;found=true;}
    if(!found)return false;
    if(!ComponentSchema::layoutPlacementsFit(proposed,composition(),m_pageDocument["categories"].toArray())) {
        m_owner->setError(tr("The placement exceeds its grid or overlaps another component in that section."));return false;
    }
    return setItemField(id, [placement](QJsonObject &item) {
        item.insert("placement", QJsonObject::fromVariantMap(placement));
    });
}

bool PageModel::clearItemPlacement(const QString &id)
{
    if (!m_owner) return false;
    const auto previous = m_pageDocument;
    bool found = false;
    if (m_pageDocument.contains("components")) {
        QJsonArray components;
        for (const auto &value : m_pageDocument.value("components").toArray()) {
            auto item = value.toObject();
            if (item.value("id").toString() == id && item.contains("placement")) {
                item.remove("placement");
                found = true;
            }
            components.append(item);
        }
        m_pageDocument["components"] = components;
    }
    if (!found) return false;
    if (!storePage()) { m_pageDocument = previous; return false; }
    emit placementsChanged();
    return true;
}

QVariantList PageModel::pageSearchSources() const
{
    QVariantList sources;
    const auto collect = [&sources](const QJsonArray &entries) {
        for (const auto &value : entries) {
            const auto entry = value.toObject();
            const auto presentation = entry.value("presentation").toObject();
            if (presentation.value("variant").toString() != QStringLiteral("library")) continue;
            const auto kind = presentation.value("libraryKind").toString(QStringLiteral("movies"));
            if (!QStringList{QStringLiteral("movies"), QStringLiteral("shows")}.contains(kind)) continue;
            const auto id = entry.value("id").toString();
            if (id.isEmpty()) continue;
            sources.append(QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), entry.value("name").toString()},
                {QStringLiteral("kind"), kind},
                {QStringLiteral("view"), presentation.value("libraryView").toString(QStringLiteral("all"))},
                {QStringLiteral("value"), presentation.value("libraryValue").toString()}});
        }
    };
    collect(m_pageDocument.value("categories").toArray());
    collect(m_pageDocument.value("components").toArray());
    return sources;
}

bool PageModel::setItemField(const QString &id, std::function<void(QJsonObject &)> mutate)
{
    if (!m_owner) return false;
    const auto previousDoc = m_pageDocument;
    bool found = false;
    if (m_pageDocument.contains("components")) {
        QJsonArray components;
        for (const auto &value : m_pageDocument.value("components").toArray()) {
            auto item = value.toObject();
            if (item.value("id").toString() != id) { components.append(item); continue; }
            const auto previous = item;
            mutate(item);
            if (item.value("kind") == QStringLiteral("app") || item.value("kind") == QStringLiteral("action")) {
                item = QJsonObject::fromVariantMap(ComponentSchema::component(item.toVariantMap(), true));
                if (item.isEmpty()) item = previous;
            }
            components.append(item);
            found = true;
        }
        if (!found) return false;
        m_pageDocument["components"] = components;
    } else {
        return false;
    }
    if (!storePage()) { m_pageDocument = previousDoc; return false; }
    emit placementsChanged();
    return true;
}
