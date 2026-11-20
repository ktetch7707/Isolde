#include "KodiMedia.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

void
KodiMedia::savePlaylist(const QString& name)
{
    if (m_busy || m_playbackActive || !QStringList{"movies", "shows"}.contains(m_mediaKind) ||
        !QStringList{"all", "recent", "continue", "watched", "search", "tag", "genre"}.contains(m_viewKind) ||
        name.trimmed().isEmpty() || name.trimmed().size() > 80)
    {
        m_playlistStatus = "Choose a movie/show library view and a playlist name of 1–80 characters, then retry.";
        emit changed();
        return;
    }
    ++m_generation;
    m_busy = true;
    m_stage = 9;
    m_playlistStatus = "Saving playlist in this Kodi profile…";
    emit changed();
    request("Isolde.CreatePlaylist",
            {{"name", name.trimmed()}, {"kind", m_mediaKind}, {"view", m_viewKind}, {"value", m_viewValue}});
}
void
KodiMedia::renamePlaylist(const QString& name, const QString& newName)
{
    if (!canRenamePlaylist(name, newName))
    {
        m_playlistStatus = "Cannot rename this playlist. Choose a valid name and retry.";
        emit changed();
        return;
    }
    ++m_generation;
    m_busy = true;
    m_stage = 10;
    m_playlistStatus = "Renaming Kodi playlist…";
    emit changed();
    request("Isolde.RenamePlaylist", {{"name", name.trimmed()}, {"newName", newName.trimmed()}});
}

bool
KodiMedia::canRenamePlaylist(const QString& name, const QString& newName) const
{
    return !m_busy && !m_playbackActive && validPlaylistName(name) && validPlaylistName(newName) && name != newName;
}
void
KodiMedia::deletePlaylist(const QString& name)
{
    if (!canDeletePlaylist(name))
    {
        m_playlistStatus = "Cannot delete this playlist. Choose a valid playlist and retry.";
        emit changed();
        return;
    }
    ++m_generation;
    m_busy = true;
    m_stage = 11;
    m_playlistStatus = "Deleting Kodi playlist…";
    emit changed();
    request("Isolde.DeletePlaylist", {{"name", name.trimmed()}});
}

bool
KodiMedia::canDeletePlaylist(const QString& name) const
{
    return !m_busy && !m_playbackActive && validPlaylistName(name);
}
void
KodiMedia::reorderPlaylist(const QString& name, const QVariantList& order)
{
    if (m_busy || m_playbackActive || !validPlaylistName(name) || order.isEmpty() || order.size() > 256 ||
        name != m_playlistOrderName || m_playlistRevision.size() != 64)
    {
        m_playlistStatus = "Cannot reorder this playlist. Choose a valid playlist and order, then retry.";
        emit changed();
        return;
    }
    QJsonArray ints;
    for (const auto& v : order)
    {
        if (!v.canConvert<int>())
        {
            m_playlistStatus = "Playlist order must be whole-number indices.";
            emit changed();
            return;
        }
        ints.append(v.toInt());
    }
    QSet<int> seen;
    for (const auto& value : ints)
    {
        const int i = value.toInt();
        if (seen.contains(i))
        {
            m_playlistStatus = "Playlist order contains duplicate indices.";
            emit changed();
            return;
        }
        seen.insert(i);
    }
    ++m_generation;
    m_busy = true;
    m_stage = 12;
    m_playlistStatus = "Reordering Kodi playlist…";
    emit changed();
    request("Isolde.ReorderPlaylist", {{"name", name.trimmed()}, {"order", ints}, {"revision", m_playlistRevision}});
}
void
KodiMedia::sortPlaylist(const QString& name, const QString& sort, const QString& direction)
{
    if (m_busy || m_playbackActive || !validPlaylistName(name))
        return;
    ++m_generation;
    m_busy = true;
    m_stage = 19;
    m_playlistStatus = "Saving smart-playlist order…";
    emit changed();
    request("Isolde.SortPlaylist", {{"name", name}, {"sort", sort}, {"direction", direction}});
}
void
KodiMedia::editPlaylist(const QString& name, const QString& view, const QString& value)
{
    if (m_busy || m_playbackActive || !validPlaylistName(name))
        return;
    if (!m_playlistPreviewReady || name != m_playlistRuleName || view != m_playlistRuleView ||
        value != m_playlistRuleValue || m_playlistRuleRevision.size() != 64)
    {
        m_playlistStatus = "Preview this rule before saving.";
        emit changed();
        return;
    }
    ++m_generation;
    m_busy = true;
    m_stage = 16;
    m_playlistStatus = "Saving playlist rule…";
    emit changed();
    request("Isolde.EditPlaylist",
            {{"name", name}, {"view", view}, {"value", value}, {"revision", m_playlistRuleRevision}});
}
void
KodiMedia::previewPlaylist(const QString& name, const QString& view, const QString& value)
{
    if (m_busy || m_playbackActive || !validPlaylistName(name))
        return;
    m_playlistRuleName = name;
    m_playlistRuleView = view;
    m_playlistRuleValue = value;
    m_playlistRuleRevision.clear();
    ++m_generation;
    m_busy = true;
    m_stage = 17;
    m_playlistPreview.clear();
    m_playlistPreviewReady = false;
    m_playlistStatus = "Previewing the first 50 matching Kodi items…";
    emit changed();
    request("Isolde.PreviewPlaylistRule", {{"name", name}, {"view", view}, {"value", value}});
}
void
KodiMedia::loadPlaylistMembers(const QString& name)
{
    if (!canLoadPlaylistMembers(name))
        return;
    ++m_generation;
    m_busy = true;
    m_stage = 18;
    m_playlistOrderName = name;
    m_playlistRevision.clear();
    m_playlistMembers.clear();
    m_playlistStatus = "Loading playlist members…";
    emit changed();
    request("Isolde.GetPlaylistOrder", {{"name", name}});
}

bool
KodiMedia::canLoadPlaylistMembers(const QString& name) const
{
    return !m_busy && !m_playbackActive && validPlaylistName(name);
}
QVariantList
KodiMedia::collections() const
{
    if (m_retired)
        return {};
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Collections");
    const auto doc = QJsonDocument::fromJson(settings.value("profiles/" + m_profile + "/v1").toByteArray());
    QVariantList result;
    QSet<QString> ids, names;
    static const QRegularExpression safeId("^[A-Za-z0-9_-]{1,80}$");
    for (const auto& entry : doc.array())
    {
        if (result.size() >= 64)
            break;
        const auto object = entry.toObject();
        const auto id = object.value("id").toString();
        const auto name = object.value("name").toString().trimmed();
        const auto kind = object.value("kind").toString();
        const auto media = object.value("media").toString("movies");
        const auto value = object.value("value").toString().trimmed();
        const bool manual = kind == "manual";
        QVariantList members;
        QSet<int> memberIds;
        bool validMembers = true;
        if (manual && !object.value("members").isArray())
            validMembers = false;
        for (const auto& member : object.value("members").toArray())
        {
            const double memberId = member.toDouble(-1);
            if (!member.isDouble() || !std::isfinite(memberId) || memberId < 1 || memberId > 2147483647 ||
                std::floor(memberId) != memberId || memberIds.contains(static_cast<int>(memberId)) ||
                members.size() >= 256)
            {
                validMembers = false;
                break;
            }
            memberIds.insert(static_cast<int>(memberId));
            members.append(static_cast<int>(memberId));
        }
        if (!safeId.match(id).hasMatch() || ids.contains(id) || name.isEmpty() || name.size() > 80 ||
            names.contains(name.toCaseFolded()) || (!manual && !QStringList{"search", "tag", "genre"}.contains(kind)) ||
            !QStringList{"movies", "shows"}.contains(media) || (!manual && (value.isEmpty() || value.size() > 160)) ||
            (manual && !value.isEmpty()) || !validMembers)
            continue;
        ids.insert(id);
        names.insert(name.toCaseFolded());
        result.append(QVariantMap{{"id", id},
                                  {"name", name},
                                  {"kind", kind},
                                  {"media", media},
                                  {"value", value},
                                  {"members", members},
                                  {"home", object.value("home").toBool(false)},
                                  {"count", qBound(1, object.value("count").toInt(6), 12)}});
    }
    return result;
}
bool
KodiMedia::storeCollections(const QVariantList& entries)
{
    if (m_retired)
        return false;
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Collections");
    settings.setValue("profiles/" + m_profile + "/v1",
                      QJsonDocument(QJsonArray::fromVariantList(entries)).toJson(QJsonDocument::Compact));
    settings.sync();
    m_collectionStatus = settings.status() == QSettings::NoError
                             ? "Collection saved. Kodi and media files are unchanged."
                             : "Could not save collections.";
    emit changed();
    if (settings.status() == QSettings::NoError)
        emit collectionsChanged();
    return settings.status() == QSettings::NoError;
}
void
KodiMedia::configureShelf(const QString& id, bool pinned, int count)
{
    if (count < 1 || count > 12)
        return;
    auto entries = collections();
    int pins = 0;
    for (const auto& entry : entries)
        if (entry.toMap().value("home").toBool() && entry.toMap().value("id") != id)
            ++pins;
    if (pinned && pins >= 8)
    {
        m_collectionStatus = "Up to eight Home shelves are supported.";
        emit changed();
        return;
    }
    for (auto& entry : entries)
    {
        auto map = entry.toMap();
        if (map.value("id") == id)
        {
            map["home"] = pinned;
            map["count"] = count;
            entry = map;
            storeCollections(entries);
            return;
        }
    }
}
void
KodiMedia::moveCollection(const QString& id, int offset)
{
    if (offset != -1 && offset != 1)
        return;
    auto entries = collections();
    for (qsizetype i = 0; i < entries.size(); ++i)
        if (entries[i].toMap().value("id") == id)
        {
            const auto target = i + offset;
            if (target < 0 || target >= entries.size())
                return;
            entries.move(i, target);
            storeCollections(entries);
            return;
        }
}
void
KodiMedia::playCollectionMovie(const QString& id, int movieId)
{
    if (!canPlayCollectionMovie(id, movieId))
        return;
    for (const auto& entry : collections())
    {
        if (entry.toMap().value("id") == id && entry.toMap().value("media", "movies").toString() == "movies")
        {
            openCollection(id);
            if (m_busy)
                m_pendingMovie = movieId;
            return;
        }
    }
}

bool
KodiMedia::canPlayCollectionMovie(const QString& id, int movieId)
{
    Q_UNUSED(id);
    return movieId >= 1 && !m_playbackActive && !(m_busy && m_stage != 0 && m_stage != 1);
}
void
KodiMedia::openCollectionShow(const QString& id, int tvShowId)
{
    if (tvShowId < 1 || !canNavigate())
        return;
    bool isShowCollection = false;
    for (const auto& entry : collections())
    {
        const auto definition = entry.toMap();
        if (definition.value("id") == id && definition.value("media", "movies").toString() == "shows")
        {
            isShowCollection = true;
            break;
        }
    }
    if (!isShowCollection)
        return;
    openCollection(id);
    if (m_busy)
        m_pendingShow = tvShowId;
}
void
KodiMedia::saveCollection(const QString& name)
{
    auto entries = collections();
    const auto clean = name.trimmed();
    if (!QStringList{"movies", "shows"}.contains(m_mediaKind) || clean.isEmpty() || clean.size() > 80 ||
        entries.size() >= 64 || !QStringList{"search", "tag", "genre"}.contains(m_viewKind))
    {
        m_collectionStatus =
            "Choose a Movies or Shows title, tag or genre view and a name of 1–80 characters. Maximum 64 collections.";
        emit changed();
        return;
    }
    for (const auto& entry : entries)
        if (entry.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0)
        {
            m_collectionStatus = "A collection already has that name.";
            emit changed();
            return;
        }
    entries.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                               {"name", clean},
                               {"kind", m_viewKind},
                               {"media", m_mediaKind},
                               {"value", m_viewValue}});
    storeCollections(entries);
}
void
KodiMedia::createManualMovieCollection(const QString& name, int movieId)
{
    createManualCollection(name, "movie", movieId);
}
void
KodiMedia::createManualCollection(const QString& name, const QString& mediaType, int itemId)
{
    const auto clean = name.trimmed();
    auto entries = collections();
    bool listed = false;
    const QString expectedKind = mediaType == "movie" ? "movies" : mediaType == "tvshow" ? "shows" : "";
    const auto identity = mediaType == "movie" ? "movieid" : "tvshowid";
    for (const auto& item : m_movies)
        listed |= item.toMap().value(identity).toInt() == itemId;
    if (itemId < 1 || expectedKind.isEmpty() || m_mediaKind != expectedKind || !listed || clean.isEmpty() ||
        clean.size() > 80 || entries.size() >= 64)
    {
        m_collectionStatus =
            "Choose a listed movie or TV show and a collection name of 1–80 characters. Maximum 64 collections.";
        emit changed();
        return;
    }
    for (const auto& entry : entries)
        if (entry.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0)
        {
            m_collectionStatus = "A collection already has that name.";
            emit changed();
            return;
        }
    entries.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                               {"name", clean},
                               {"kind", "manual"},
                               {"media", expectedKind},
                               {"value", QString()},
                               {"members", QVariantList{itemId}}});
    storeCollections(entries);
}
void
KodiMedia::addMovieToCollection(const QString& collectionId, int movieId)
{
    addManualCollectionMember(collectionId, "movie", movieId);
}
void
KodiMedia::addManualCollectionMember(const QString& collectionId, const QString& mediaType, int itemId)
{
    const QString expectedKind = mediaType == "movie" ? "movies" : mediaType == "tvshow" ? "shows" : "";
    const auto identity = mediaType == "movie" ? "movieid" : "tvshowid";
    if (itemId < 1 || expectedKind.isEmpty() || m_mediaKind != expectedKind)
        return;
    bool listed = false;
    for (const auto& item : m_movies)
        listed |= item.toMap().value(identity).toInt() == itemId;
    if (!listed)
    {
        m_collectionStatus = "Choose an item from the current Kodi results first.";
        emit changed();
        return;
    }
    auto entries = collections();
    for (auto& entry : entries)
    {
        auto map = entry.toMap();
        if (map.value("id") != collectionId || map.value("kind") != "manual" || map.value("media") != expectedKind)
            continue;
        auto members = map.value("members").toList();
        for (const auto& member : members)
            if (member.toInt() == itemId)
            {
                m_collectionStatus = "This item is already in the collection.";
                emit changed();
                return;
            }
        if (members.size() >= 256)
        {
            m_collectionStatus = "Manual collections can contain up to 256 items.";
            emit changed();
            return;
        }
        members.append(itemId);
        map["members"] = members;
        entry = map;
        if (storeCollections(entries) && m_viewKind == "manual" && m_viewValue == map.value("name"))
            openCollection(collectionId);
        return;
    }
}
void
KodiMedia::removeMovieFromCollection(const QString& collectionId, int movieId)
{
    removeManualCollectionMember(collectionId, movieId);
}
void
KodiMedia::removeManualCollectionMember(const QString& collectionId, int itemId)
{
    auto entries = collections();
    for (auto& entry : entries)
    {
        auto map = entry.toMap();
        if (map.value("id") != collectionId || map.value("kind") != "manual")
            continue;
        auto members = map.value("members").toList();
        for (qsizetype index = 0; index < members.size(); ++index)
            if (members[index].toInt() == itemId)
            {
                members.removeAt(index);
                map["members"] = members;
                entry = map;
                if (storeCollections(entries) && m_viewKind == "manual" && m_viewValue == map.value("name"))
                    openCollection(collectionId);
                return;
            }
    }
}
void
KodiMedia::renameCollection(const QString& id, const QString& name)
{
    auto entries = collections();
    const auto clean = name.trimmed();
    if (clean.isEmpty() || clean.size() > 80)
        return;
    for (const auto& entry : entries)
        if (entry.toMap().value("id") != id &&
            entry.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0)
        {
            m_collectionStatus = "A collection already has that name.";
            emit changed();
            return;
        }
    for (auto& entry : entries)
    {
        auto map = entry.toMap();
        if (map.value("id") == id)
        {
            map["name"] = clean;
            entry = map;
            storeCollections(entries);
            return;
        }
    }
}
void
KodiMedia::removeCollection(const QString& id)
{
    auto entries = collections();
    for (qsizetype i = 0; i < entries.size(); ++i)
        if (entries[i].toMap().value("id") == id)
        {
            entries.removeAt(i);
            storeCollections(entries);
            return;
        }
}
void
KodiMedia::openCollection(const QString& id)
{
    if (!canNavigate())
        return;
    m_pendingShow = 0;
    for (const auto& entry : collections())
    {
        const auto map = entry.toMap();
        if (map.value("id") == id)
        {
            m_mediaKind = map.value("media", "movies").toString();
            if (!QStringList{"movies", "shows"}.contains(m_mediaKind))
                return;
            m_history.clear();
            m_showId = m_season = -1;
            m_showTitle.clear();
            if (map.value("kind") == "manual")
            {
                m_viewKind = "manual";
                m_viewValue = map.value("name").toString();
                m_manualMembers = map.value("members").toList();
                m_selectedIndex = 0;
                beginRead(0);
                return;
            }
            setView(map.value("kind").toString(), map.value("value").toString());
            return;
        }
    }
}
