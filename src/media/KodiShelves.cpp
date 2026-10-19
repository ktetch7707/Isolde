#include "KodiShelves.h"
#include "home/ComponentSchema.h"

KodiShelves::KodiShelves(const QString &profile, KodiMedia *library, QObject *parent, const QString &testDirectory)
    : QObject(parent), m_profile(profile), m_testDirectory(testDirectory), m_library(library)
{
    connect(library, &KodiMedia::collectionsChanged, this, &KodiShelves::rebuild);
    rebuild();
}
void KodiShelves::rebuild()
{
    // Presentation edits must not discard loaded results or restart in-flight reads.
    auto previous = m_shelves;
    m_shelves.clear();
    QVector<KodiMedia *> added;
    for (const auto &entry : m_library->collections()) {
        const auto definition = entry.toMap();
        if (!definition.value("home").toBool() || m_shelves.size() >= 8) continue;
        const auto media = definition.value("media", "movies").toString();
        if (!QStringList{"movies", "shows"}.contains(media)) continue;
        const auto kind = definition.value("kind").toString();
        if (!QStringList{"search", "tag", "genre", "manual"}.contains(kind)) continue;
        KodiMedia *client = nullptr;
        for (auto &old : previous) {
            if (old.client && old.definition.value("id") == definition.value("id")
                && old.definition.value("kind") == definition.value("kind")
                && old.definition.value("media", "movies") == media
                && old.definition.value("value") == definition.value("value")
                && old.definition.value("members") == definition.value("members")) {
                client = old.client;
                old.client = nullptr;
                break;
            }
        }
        if (!client) {
            client = new KodiMedia(m_profile, this, m_testDirectory);
            connect(client, &KodiMedia::changed, this, &KodiShelves::changed);
            added.append(client);
        }
        m_shelves.append({definition, client});
    }
    for (const auto &old : previous) delete old.client;
    // Publish only complete section lists, including during synchronous query signals.
    if (m_available) for (const auto &shelf : m_shelves)
        if (added.contains(shelf.client)) {
            if (shelf.definition.value("kind") == "manual") shelf.client->openCollection(shelf.definition.value("id").toString());
            else shelf.client->openView(shelf.definition.value("media", "movies").toString(),
                shelf.definition.value("kind").toString(), shelf.definition.value("value").toString());
        }
    emit changed();
}
void KodiShelves::setAvailable(bool available)
{
    if (m_available == available) return;
    m_available = available;
    if (available) refresh();
    else for (const auto &shelf : m_shelves) shelf.client->invalidate("Kodi is unavailable. Open the library to reconnect.");
    emit changed();
}
void KodiShelves::refresh()
{
    if (!m_available) return;
    for (const auto &shelf : m_shelves) {
        if (shelf.definition.value("kind") == "manual") shelf.client->openCollection(shelf.definition.value("id").toString());
        else shelf.client->openView(shelf.definition.value("media", "movies").toString(),
            shelf.definition.value("kind").toString(), shelf.definition.value("value").toString());
    }
}
QVariantList KodiShelves::sections() const
{
    QVariantList result;
    for (const auto &shelf : m_shelves) {
        const auto id = shelf.definition.value("id").toString();
        const auto prefix = "collection-" + id;
        const auto isShows = shelf.definition.value("media", "movies").toString() == "shows";
        const auto tile = [&](const QString &key, const QString &name, const QString &actionType, int itemId) -> QVariantMap {
            QVariantMap action{{"type", actionType}, {"collectionId", id}};
            if (actionType == "play-movie") action["movieId"] = itemId;
            if (actionType == "open-show") action["tvShowId"] = itemId;
            return ComponentSchema::component({{"id", prefix + '-' + key}, {"kind", "action"}, {"name", name}, {"section", "Collection"},
                {"symbol", actionType == "play-movie" ? "▶" : actionType == "open-show" ? "TV" : "≡"}, {"accent", "#5B8DEF"}, {"size", "standard"},
                {"networkRequirement", "none"}, {"action", action}}, true);
        };
        QVariantList items;
        QString label = "Browse collection";
        if (!m_available || (!shelf.client->connected() && !shelf.client->busy())) label = isShows ? "Unavailable · open Shows" : "Unavailable · open Movies";
        else if (shelf.client->busy()) label = "Loading collection…";
        else if (shelf.client->movies().isEmpty()) label = isShows ? "No matching shows · browse" : "No matching movies · browse";
        items.append(tile("browse", label, "open-collection", 0));
        const int count = qBound(1, shelf.definition.value("count", 6).toInt(), 12);
        for (const auto &entry : shelf.client->movies()) {
            if (items.size() > count) break;
            const auto item = entry.toMap();
            const auto itemId = item.value(isShows ? "tvshowid" : "movieid").toInt();
            if (itemId <= 0) continue;
            items.append(tile(QString::number(itemId), item.value("title").toString(), isShows ? "open-show" : "play-movie", itemId));
        }
        auto component = ComponentSchema::component({{"id", prefix}, {"componentType", "media-shelf"}, {"kind", "action"},
            {"name", shelf.definition.value("name")}, {"action", QVariantMap{{"type", "open-collection"}, {"collectionId", id}}}}, true);
        component["items"] = items;
        result.append(component);
    }
    return result;
}
QVariantList KodiShelves::items() const
{
    QVariantList result;
    for (const auto &section : sections()) result.append(section.toMap().value("items").toList());
    return result;
}
