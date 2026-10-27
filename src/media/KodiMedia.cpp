#include "KodiMedia.h"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QUuid>
#include <QSet>
#include <QRegularExpression>
#include <QDateTime>
#include <algorithm>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif

KodiMedia::KodiMedia(const QString &profile, QObject *parent, const QString &testDirectory) : QObject(parent)
{
    m_profile = profile;
    m_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/native-profiles/" + profile + "/kodi/home/.kodi/userdata/isolde";
    if (!testDirectory.isEmpty()) m_directory = testDirectory;
    m_playbackPoll.setInterval(500);
    connect(&m_playbackPoll, &QTimer::timeout, this, [this] {
        if (!m_busy && m_playbackActive) { m_busy = true; m_stage = 6; request("Isolde.PlaybackStatus"); }
    });
    m_socket.setReadBufferSize(4 * 1024 * 1024 + 1);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(3000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail("Kodi did not respond. You can retry without leaving Isolde."); });
    connect(&m_socket, &QLocalSocket::connected, this, [this] { m_socket.write(m_request); });
    connect(&m_socket, &QLocalSocket::readyRead, this, &KodiMedia::consume);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        if (error == QLocalSocket::PeerClosedError && m_received.endsWith('\n')) return;
        if (m_busy) fail("Kodi's local bridge is unavailable for this profile. Start Kodi and enable the bridge, then retry.");
    });
}
KodiMedia::~KodiMedia() { m_busy = false; releaseSocket(); }
void KodiMedia::releaseSocket()
{
    m_timeout.stop();
    m_socket.abort();
#ifdef Q_OS_LINUX
    if (m_descriptor >= 0) ::close(m_descriptor);
#endif
    m_descriptor = -1;
}
void KodiMedia::fail(const QString &message)
{
    if(m_stage==9 && m_busy)m_playlistStatus="Playlist save could not be confirmed. Refresh the saved-playlist list before retrying. "+message;
    m_pendingMovie = m_pendingShow = 0;
    m_pendingDirectKind.clear(); m_pendingDirectId = 0;
    const bool wasPlaying = m_playbackActive;
    m_lastPlaybackNaturalEnd=false;
    m_pendingControl.clear();
    m_playbackPoll.stop();
    m_playbackActive = m_playbackVisible = false;
    if (wasPlaying) m_playbackStatus = "Playback status unavailable. Kodi may still be playing. " + message;
    const bool quitting = m_busy && m_stage == 4;
    if (m_busy && (m_stage == 2 || m_stage == 3)) m_scanStatus = "Scan status unavailable. " + message;
    ++m_generation;
    m_connected = false;
    m_busy = false;
    releaseSocket();
    m_movies.clear();
    m_recentResults.clear();
    m_pageStart = m_totalMovies = 0;
    m_status = message;
    emit changed();
    if (wasPlaying) emit playbackReturned(false);
    if (quitting) emit quitReply(false);
}
void KodiMedia::quitIfIdle()
{
    if (m_busy) return;
    ++m_generation;
    m_busy = true;
    m_stage = 4;
    request("Isolde.QuitIfIdle");
}
void KodiMedia::invalidate(const QString &reason)
{
    fail(reason);
}
void KodiMedia::refresh()
{
    if (m_busy || m_playbackActive) return;
    beginRead(0);
}
void KodiMedia::beginRead(int start)
{
    m_recentResults.clear(); m_recentEpisodes = false;
    ++m_generation;
    m_busy = false; releaseSocket();
    m_connected = false;
    m_movies.clear();
    m_totalMovies = 0; m_pageCount = 0; m_pageStart = m_requestedStart = start;
    m_busy = true;
    m_stage = 0;
    m_status = "Connecting to this profile's local Kodi…";
    emit changed();
    request("JSONRPC.Version");
}
bool KodiMedia::canNavigate() const { return !m_playbackActive && (!m_busy || m_stage == 0 || m_stage == 1); }
QString KodiMedia::browseTitle() const
{
    if (m_mediaKind == "playlists") return "Kodi video playlists";
    if (m_viewKind == "playlist") return "Kodi playlist · " + m_viewValue;
    if (m_mediaKind == "history") return "Recently viewed";
    if (m_mediaKind == "albums") return "Music · Albums";
    if (m_mediaKind == "songs") return m_showTitle.isEmpty()?"Music · Tracks":m_showTitle;
    if (m_mediaKind == "channels") return "TV Guide";
    if (m_viewKind == "manual") return m_viewValue;
    if (m_mediaKind == "movies") return "Movies";
    if (m_mediaKind == "shows") return "Shows";
    if (m_mediaKind == "seasons") return m_showTitle + " · Seasons";
    if (m_showId < 1) {
        if (m_viewKind == "search") return "Search Results · Episodes";
        return m_viewKind == "continue" ? "Continue Watching · Episodes" : m_viewKind == "watched" ? "Recently Watched · Episodes" : "Recently Added · Episodes";
    }
    return m_showTitle + (m_season == 0 ? " · Specials" : " · Season " + QString::number(m_season));
}
KodiMedia::BrowseState KodiMedia::browseState() const
{
    return {m_mediaKind, m_viewKind, m_viewValue, m_showTitle, m_showId, m_season, m_pageStart, m_selectedIndex};
}
void KodiMedia::restoreBrowse(const BrowseState &state)
{
    m_pendingShow = 0;
    m_pendingDirectKind.clear(); m_pendingDirectId = 0;
    m_mediaKind = state.kind; m_viewKind = state.view; m_viewValue = state.value; m_showTitle = state.showTitle;
    m_showId = state.show; m_season = state.season; m_selectedIndex = state.selected;
    beginRead(state.start);
}
void KodiMedia::selectIndex(int index)
{
    index = qBound(-1, index, qMax(-1, int(m_movies.size()) - 1));
    if (index == m_selectedIndex) return;
    m_selectedIndex = index; emit changed();
}
void KodiMedia::openLibrary(const QString &kind)
{
    if (!canNavigate() || !QStringList{"movies", "shows"}.contains(kind)) return;
    m_pendingShow = 0;
    m_pendingDirectKind.clear(); m_pendingDirectId = 0;
    if (m_mediaKind == kind && m_history.isEmpty() && m_viewKind == "all") { if (!m_busy) beginRead(m_pageStart); return; }
    m_history.clear(); m_pendingMovie = 0;
    restoreBrowse({kind, "all", {}, {}, -1, -1, 0, 0});
}
bool KodiMedia::validPlaylistName(const QString &name) {
    if(name.isEmpty() || name.size()>128 || name.contains(QRegularExpression("[/\\\\:%\\x00-\\x1f]")))return false;
    return name.endsWith(".xsp",Qt::CaseInsensitive) || name.endsWith(".m3u",Qt::CaseInsensitive) || name.endsWith(".m3u8",Qt::CaseInsensitive);
}
void KodiMedia::openPlaylists() {
    if(!canNavigate())return;
    m_history.clear();m_pendingMovie=0;m_pendingDirectKind.clear();m_pendingDirectId=0;
    restoreBrowse({"playlists","all",{},{},-1,-1,0,0});
}
void KodiMedia::savePlaylist(const QString &name) {
    if(m_busy || m_playbackActive || !QStringList{"movies","shows"}.contains(m_mediaKind)
       || !QStringList{"all","recent","continue","watched","search","tag","genre"}.contains(m_viewKind)
       || name.trimmed().isEmpty() || name.trimmed().size()>80){m_playlistStatus="Choose a movie/show library view and a playlist name of 1–80 characters, then retry.";emit changed();return;}
    ++m_generation;m_busy=true;m_stage=9;m_playlistStatus="Saving playlist in this Kodi profile…";emit changed();
    request("Isolde.CreatePlaylist",{{"name",name.trimmed()},{"kind",m_mediaKind},{"view",m_viewKind},{"value",m_viewValue}});
}
void KodiMedia::openView(const QString &kind,const QString &view,const QString &value) {
    if(!canNavigate() || !QStringList{"movies","shows","episodes","history","albums","channels"}.contains(kind) || !QStringList{"all","recent","watched","continue","search","tag","genre","playlist"}.contains(view)
        || (view=="playlist" && (!QStringList{"movies","shows"}.contains(kind) || !validPlaylistName(value)))
        || ((kind == "albums" || kind == "channels") && (view != "all" || !value.isEmpty()))
        || (kind == "history" && (view != "watched" || !value.isEmpty()))
        || (kind == "episodes" && view != "search")
        || value.trimmed().size()>160 || ((view=="tag"||view=="genre") && (value.trimmed().isEmpty())))return;
    m_history.clear();m_pendingMovie=0; m_pendingDirectKind.clear(); m_pendingDirectId=0;
    restoreBrowse({kind=="shows"&&(view=="recent"||view=="continue"||view=="watched")?"episodes":kind,view,value.trimmed(),{},-1,-1,0,0});
}
void KodiMedia::openShowResult(const QString &query, int tvShowId, int pageStart)
{
    const auto text = query.trimmed();
    if (!canNavigate() || tvShowId < 1 || text.isEmpty() || text.size() > 160
        || pageStart < 0 || pageStart % 50 != 0 || pageStart > 10000000) return;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear(); m_pendingDirectId = 0;
    m_mediaKind = "shows"; m_viewKind = "search"; m_viewValue = text;
    m_showId = m_season = -1; m_showTitle.clear(); m_selectedIndex = 0;
    m_pendingShow = tvShowId;
    beginRead(pageStart);
}
void KodiMedia::playMedia(const QString &kind,int id,bool resume) {
    if(!QStringList{"movie","episode","song","channel"}.contains(kind)||id<1||m_playbackActive)return;
    if (m_busy) {
        if (m_stage != 0 && m_stage != 1) {
            m_playbackStatus = "Kodi is busy. Wait for the current library operation to finish.";
            emit changed();
            return;
        }
        m_pendingDirectKind = kind; m_pendingDirectId = id; m_pendingDirectResume = resume;
        m_status = "Opening the selected local result after Kodi finishes its library read…";
        emit changed();
        return;
    }
    playLibraryItem(kind,id,resume,false);
}
void KodiMedia::activate(int index)
{
    if (m_busy || !m_connected || m_playbackActive || index < 0 || index >= m_movies.size()) return;
    selectIndex(index);
    const auto item = m_movies[index].toMap();
    if (m_mediaKind == "albums") {
        const int album=item.value("albumid").toInt();if(album<1)return;
        m_history.append(browseState());
        restoreBrowse({"songs","album",QString::number(album),item.value("title").toString(),-1,-1,0,0});
    } else if (m_mediaKind == "shows" || m_mediaKind == "seasons") {
        auto next = browseState();
        if (m_mediaKind == "shows") {
            if (item.value("tvshowid").toInt() < 1) return;
            next.kind = "seasons"; next.show = item.value("tvshowid").toInt();
            next.showTitle = item.value("title", item.value("label")).toString(); next.season = -1;
        } else {
            if (!item.contains("season") || item.value("season").toInt() < 0) return;
            next.kind = "episodes"; next.season = item.value("season").toInt();
        }
        m_history.append(browseState()); next.view = "all"; next.value.clear(); next.start = next.selected = 0;
        restoreBrowse(next);
    } else playSelected(true);
}
bool KodiMedia::goBack()
{
    if (!canNavigate() || m_history.isEmpty()) return false;
    m_pendingMovie = 0; m_pendingDirectKind.clear(); m_pendingDirectId = 0;
    restoreBrowse(m_history.takeLast()); return true;
}
void KodiMedia::playSelected(bool resume)
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_movies.size()) return;
    const auto item = m_movies[m_selectedIndex].toMap();
    if (m_mediaKind == "movies") playLibraryItem("movie", item.value("movieid").toInt(), resume);
    else if (m_mediaKind == "episodes") playLibraryItem("episode", item.value("episodeid").toInt(), resume);
    else if (m_mediaKind == "songs") playLibraryItem("song", item.value("songid").toInt(), false);
    else if (m_mediaKind == "channels") playLibraryItem("channel", item.value("channelid").toInt(), false);
}
void KodiMedia::nextPage()
{
    if (!m_busy && !m_playbackActive && hasNextPage()) { m_selectedIndex = 0; loadPage(m_pageStart + 50); }
}
void KodiMedia::previousPage()
{
    if (!m_busy && !m_playbackActive && m_connected && m_pageStart > 0) { m_selectedIndex = 0; loadPage(qMax(0, m_pageStart - 50)); }
}
void KodiMedia::loadPage(int start)
{
    if (m_viewKind == "manual") { loadManualPage(start); return; }
    m_requestedStart = start;
    m_busy = true;
    m_stage = 1;
    m_status = "Loading " + browseTitle() + "…";
    emit changed();
    QJsonArray fields{"title", "year", "plot", "runtime", "resume", "playcount", "art"};
    if(m_mediaKind=="playlists"){request("Isolde.GetPlaylists");return;}
    if(m_viewKind=="playlist"){
        request("Isolde.GetPlaylistItems",{{"name",m_viewValue},{"kind",m_mediaKind},{"start",start},{"end",start+50}});return;
    }
    QString method = "VideoLibrary.GetMovies";
    if (m_mediaKind == "shows") { method = "VideoLibrary.GetTVShows"; fields = {"title", "year", "plot", "episode", "watchedepisodes", "art"}; }
    else if (m_mediaKind == "seasons") { method = "VideoLibrary.GetSeasons"; fields = {"season", "showtitle", "episode", "watchedepisodes", "tvshowid", "art"}; }
    else if (m_mediaKind == "episodes" || (m_mediaKind == "history" && m_recentEpisodes)) { method = "VideoLibrary.GetEpisodes"; fields = {"title", "plot", "runtime", "resume", "playcount", "season", "episode", "showtitle", "tvshowid", "art"}; }
    if (m_mediaKind == "history") fields.append("lastplayed");
    if (m_mediaKind == "albums") {method="AudioLibrary.GetAlbums";fields={"title","artist","year","art"};}
    else if (m_mediaKind == "songs") {method="AudioLibrary.GetSongs";fields={"title","artist","album","albumid","track","duration","art"};}
    else if (m_mediaKind == "channels") {method="PVR.GetChannels";fields={"channelnumber","broadcastnow","broadcastnext","locked","hidden"};}
    const auto sort = m_mediaKind=="songs"?"track":(m_viewKind == "continue" || m_viewKind == "watched") ? "lastplayed" : m_viewKind == "recent" ? "dateadded"
        : (m_mediaKind == "episodes" || m_mediaKind == "seasons") ? "episode" : "title";
    QJsonObject params{{"properties", fields},
        {"sort", QJsonObject{{"method", sort},
            {"order", (m_viewKind == "continue" || m_viewKind == "recent" || m_viewKind == "watched") ? "descending" : "ascending"}, {"ignorearticle", true}}},
        {"limits", QJsonObject{{"start", start}, {"end", start + 50}}}};
    if (m_viewKind == "continue") params.insert("filter", QJsonObject{{"field", "inprogress"}, {"operator", "true"}, {"value", ""}});
    else if (m_viewKind == "watched") params.insert("filter", QJsonObject{{"field", "lastplayed"}, {"operator", "after"}, {"value", "1970-01-01"}});
    else if (m_viewKind != "all" && m_viewKind != "recent") params.insert("filter", QJsonObject{{"field", m_viewKind == "search" ? "title" : m_viewKind},
        {"operator", m_viewKind == "search" ? "contains" : "is"}, {"value", m_viewValue}});
    if ((m_mediaKind == "seasons" || m_mediaKind == "episodes") && m_showId > 0) params["tvshowid"] = m_showId;
    if (m_mediaKind == "episodes" && m_season >= 0) params["season"] = m_season;
    if(m_mediaKind=="songs")params.insert("filter",QJsonObject{{"albumid",m_viewValue.toInt()}});
    if(m_mediaKind=="channels"){params.remove("sort");params.insert("channelgroupid","alltv");}
    request(method, params);
}
void KodiMedia::loadManualPage(int start)
{
    if (start < 0 || start % 50 != 0 || start > m_manualMembers.size()) return;
    m_manualType = m_mediaKind == "shows" ? "tvshow" : "movie";
    m_requestedStart = start;
    m_pageStart = start;
    m_pageCount = static_cast<int>(qMin<qsizetype>(50, m_manualMembers.size() - start));
    m_totalMovies = m_manualMembers.size();
    m_manualNextIndex = start;
    m_manualMissingCount = 0;
    m_manualResults.clear();
    m_busy = true;
    m_stage = 8;
    m_status = "Loading manual collection from Kodi…";
    emit changed();
    loadNextManualMovie();
}
void KodiMedia::loadNextManualMovie()
{
    if (m_manualNextIndex >= m_requestedStart + m_pageCount) {
        m_movies = m_manualResults;
        m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
        m_busy = false; m_connected = true; m_stage = 1;
        if (m_movies.isEmpty()) m_status = m_pageCount == 0 ? "This manual collection is empty. Open a library item and add it with Menu."
            : "No saved items on this page are available in Kodi.";
        else m_status = m_manualMissingCount > 0
            ? QString("%1 saved item(s) are no longer available in Kodi.").arg(m_manualMissingCount)
            : QString("%1 items in this manual collection.").arg(m_totalMovies);
        releaseSocket();
        emit changed();
        if (m_pendingMovie > 0) {
            const int movieId = m_pendingMovie;
            m_pendingMovie = 0;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation, movieId] {
                if (generation != m_generation) return;
                bool listed = false;
                for (const auto &entry : m_movies) listed |= entry.toMap().value("movieid").toInt() == movieId;
                if (listed) playMovie(movieId, true);
                else { m_status = "This movie is no longer in the manual collection page."; emit changed(); }
            });
        }
        if (m_pendingShow > 0) {
            const int showId = m_pendingShow;
            m_pendingShow = 0;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation, showId] {
                if (generation != m_generation) return;
                for (int index = 0; index < m_movies.size(); ++index) {
                    if (m_movies[index].toMap().value("tvshowid").toInt() == showId) {
                        m_selectedIndex = index; activate(index); return;
                    }
                }
                m_status = "This show is no longer in the manual collection."; emit changed();
            });
        }
        return;
    }
    m_manualCurrentId = m_manualMembers[m_manualNextIndex++].toInt();
    const bool shows = m_manualType == "tvshow";
    request(shows ? "VideoLibrary.GetTVShowDetails" : "VideoLibrary.GetMovieDetails",
        {{shows ? "tvshowid" : "movieid", m_manualCurrentId},
         {"properties", shows ? QJsonArray{"title", "year", "plot", "episode", "watchedepisodes", "art"}
                               : QJsonArray{"title", "year", "plot", "runtime", "resume", "playcount", "art"}}});
}
void KodiMedia::setView(const QString &kind, const QString &value)
{
    if (m_playbackActive || (m_busy && m_stage != 0 && m_stage != 1)) return;
    if (!QStringList{"all", "continue", "recent", "watched", "search", "tag", "genre"}.contains(kind) || value.trimmed().size() > 160) return;
    const auto text = value.trimmed();
    if ((kind == "tag" || kind == "genre") && text.isEmpty()) return;
    if ((kind == "tag" || kind == "genre") && !QStringList{"movies", "shows"}.contains(m_mediaKind)) return;
    if (m_mediaKind == "seasons") return;
    const auto previous = browseState();
    // Cancel a superseded read so old results cannot populate the new view.
    m_pendingMovie = m_pendingShow = 0; m_pendingDirectKind.clear(); m_pendingDirectId = 0;
    ++m_generation; m_busy = false; releaseSocket();
    m_viewKind = kind == "search" && text.isEmpty() ? "all" : kind;
    m_viewValue = (m_viewKind == "all" || m_viewKind == "continue" || m_viewKind == "recent" || m_viewKind == "watched") ? QString() : text;
    if (m_mediaKind != "movies") {
        if (kind == "continue" || kind == "recent" || kind == "watched") {
            if (m_mediaKind == "shows") {
                m_history.append(previous);
                m_mediaKind = "episodes"; m_showId = m_season = -1; m_showTitle.clear();
            }
        }
    }
    m_selectedIndex = 0;
    refresh();
}
QVariantList KodiMedia::collections() const
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Collections");
    const auto doc = QJsonDocument::fromJson(settings.value("profiles/" + m_profile + "/v1").toByteArray());
    // Treat persisted definitions as input, not executable query instructions.
    // Keep the v1 storage format compatible and leave malformed originals untouched.
    QVariantList result;
    QSet<QString> ids, names;
    static const QRegularExpression safeId("^[A-Za-z0-9_-]{1,80}$");
    for (const auto &entry : doc.array()) {
        if (result.size() >= 64) break;
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
        if (manual && !object.value("members").isArray()) validMembers = false;
        for (const auto &member : object.value("members").toArray()) {
            const double memberId = member.toDouble(-1);
            if (!member.isDouble() || !std::isfinite(memberId) || memberId < 1 || memberId > 2147483647
                || std::floor(memberId) != memberId || memberIds.contains(static_cast<int>(memberId)) || members.size() >= 256) {
                validMembers = false;
                break;
            }
            memberIds.insert(static_cast<int>(memberId));
            members.append(static_cast<int>(memberId));
        }
        if (!safeId.match(id).hasMatch() || ids.contains(id) || name.isEmpty() || name.size() > 80
            || names.contains(name.toCaseFolded()) || (!manual && !QStringList{"search", "tag", "genre"}.contains(kind))
            || !QStringList{"movies", "shows"}.contains(media)
            || (!manual && (value.isEmpty() || value.size() > 160)) || (manual && !value.isEmpty()) || !validMembers) continue;
        ids.insert(id); names.insert(name.toCaseFolded());
        result.append(QVariantMap{{"id", id}, {"name", name}, {"kind", kind}, {"media", media}, {"value", value},
            {"members", members},
            {"home", object.value("home").toBool(false)},
            {"count", qBound(1, object.value("count").toInt(6), 12)}});
    }
    return result;
}
bool KodiMedia::storeCollections(const QVariantList &entries)
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Collections");
    settings.setValue("profiles/" + m_profile + "/v1", QJsonDocument(QJsonArray::fromVariantList(entries)).toJson(QJsonDocument::Compact));
    settings.sync();
    m_collectionStatus = settings.status() == QSettings::NoError ? "Collection saved. Kodi and media files are unchanged." : "Could not save collections.";
    emit changed();
    if (settings.status() == QSettings::NoError) emit collectionsChanged();
    return settings.status() == QSettings::NoError;
}
void KodiMedia::configureShelf(const QString &id, bool pinned, int count)
{
    if (count < 1 || count > 12) return;
    auto entries = collections();
    int pins = 0;
    for (const auto &entry : entries) if (entry.toMap().value("home").toBool() && entry.toMap().value("id") != id) ++pins;
    if (pinned && pins >= 8) { m_collectionStatus = "Up to eight Home shelves are supported."; emit changed(); return; }
    for (auto &entry : entries) {
        auto map = entry.toMap();
        if (map.value("id") == id) { map["home"] = pinned; map["count"] = count; entry = map; storeCollections(entries); return; }
    }
}
void KodiMedia::moveCollection(const QString &id, int offset)
{
    if (offset != -1 && offset != 1) return;
    auto entries = collections();
    for (qsizetype i = 0; i < entries.size(); ++i) if (entries[i].toMap().value("id") == id) {
        const auto target = i + offset;
        if (target < 0 || target >= entries.size()) return;
        entries.move(i, target); storeCollections(entries); return;
    }
}
void KodiMedia::playCollectionMovie(const QString &id, int movieId)
{
    if (movieId < 1 || m_playbackActive || (m_busy && m_stage != 0 && m_stage != 1)) return;
    for (const auto &entry : collections()) {
        if (entry.toMap().value("id") == id && entry.toMap().value("media", "movies").toString() == "movies") {
            openCollection(id);
            if (m_busy) m_pendingMovie = movieId;
            return;
        }
    }
}
void KodiMedia::openCollectionShow(const QString &id, int tvShowId)
{
    if (tvShowId < 1 || !canNavigate()) return;
    bool isShowCollection = false;
    for (const auto &entry : collections()) {
        const auto definition = entry.toMap();
        if (definition.value("id") == id && definition.value("media", "movies").toString() == "shows") {
            isShowCollection = true;
            break;
        }
    }
    if (!isShowCollection) return;
    openCollection(id);
    if (m_busy) m_pendingShow = tvShowId;
}
void KodiMedia::saveCollection(const QString &name)
{
    auto entries = collections();
    const auto clean = name.trimmed();
    if (!QStringList{"movies", "shows"}.contains(m_mediaKind) || clean.isEmpty() || clean.size() > 80 || entries.size() >= 64
        || !QStringList{"search", "tag", "genre"}.contains(m_viewKind)) {
        m_collectionStatus = "Choose a Movies or Shows title, tag or genre view and a name of 1–80 characters. Maximum 64 collections."; emit changed(); return;
    }
    for (const auto &entry : entries) if (entry.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0) {
        m_collectionStatus = "A collection already has that name."; emit changed(); return;
    }
    entries.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"name", clean},
        {"kind", m_viewKind}, {"media", m_mediaKind}, {"value", m_viewValue}});
    storeCollections(entries);
}
void KodiMedia::createManualMovieCollection(const QString &name, int movieId)
{
    createManualCollection(name, "movie", movieId);
}
void KodiMedia::createManualCollection(const QString &name, const QString &mediaType, int itemId)
{
    const auto clean = name.trimmed();
    auto entries = collections();
    bool listed = false;
    const QString expectedKind = mediaType == "movie" ? "movies" : mediaType == "tvshow" ? "shows" : "";
    const auto identity = mediaType == "movie" ? "movieid" : "tvshowid";
    for (const auto &item : m_movies) listed |= item.toMap().value(identity).toInt() == itemId;
    if (itemId < 1 || expectedKind.isEmpty() || m_mediaKind != expectedKind || !listed
        || clean.isEmpty() || clean.size() > 80 || entries.size() >= 64) {
        m_collectionStatus = "Choose a listed movie or TV show and a collection name of 1–80 characters. Maximum 64 collections.";
        emit changed(); return;
    }
    for (const auto &entry : entries) if (entry.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0) {
        m_collectionStatus = "A collection already has that name."; emit changed(); return;
    }
    entries.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"name", clean},
        {"kind", "manual"}, {"media", expectedKind}, {"value", QString()}, {"members", QVariantList{itemId}}});
    storeCollections(entries);
}
void KodiMedia::addMovieToCollection(const QString &collectionId, int movieId)
{
    addManualCollectionMember(collectionId, "movie", movieId);
}
void KodiMedia::addManualCollectionMember(const QString &collectionId, const QString &mediaType, int itemId)
{
    const QString expectedKind = mediaType == "movie" ? "movies" : mediaType == "tvshow" ? "shows" : "";
    const auto identity = mediaType == "movie" ? "movieid" : "tvshowid";
    if (itemId < 1 || expectedKind.isEmpty() || m_mediaKind != expectedKind) return;
    bool listed = false;
    for (const auto &item : m_movies) listed |= item.toMap().value(identity).toInt() == itemId;
    if (!listed) { m_collectionStatus = "Choose an item from the current Kodi results first."; emit changed(); return; }
    auto entries = collections();
    for (auto &entry : entries) {
        auto map = entry.toMap();
        if (map.value("id") != collectionId || map.value("kind") != "manual" || map.value("media") != expectedKind) continue;
        auto members = map.value("members").toList();
        for (const auto &member : members) if (member.toInt() == itemId) {
            m_collectionStatus = "This item is already in the collection."; emit changed(); return;
        }
        if (members.size() >= 256) { m_collectionStatus = "Manual collections can contain up to 256 items."; emit changed(); return; }
        members.append(itemId); map["members"] = members; entry = map;
        if (storeCollections(entries) && m_viewKind == "manual" && m_viewValue == map.value("name")) openCollection(collectionId);
        return;
    }
}
void KodiMedia::removeMovieFromCollection(const QString &collectionId, int movieId)
{
    removeManualCollectionMember(collectionId, movieId);
}
void KodiMedia::removeManualCollectionMember(const QString &collectionId, int itemId)
{
    auto entries = collections();
    for (auto &entry : entries) {
        auto map = entry.toMap();
        if (map.value("id") != collectionId || map.value("kind") != "manual") continue;
        auto members = map.value("members").toList();
        for (qsizetype index = 0; index < members.size(); ++index) if (members[index].toInt() == itemId) {
            members.removeAt(index); map["members"] = members; entry = map;
            if (storeCollections(entries) && m_viewKind == "manual" && m_viewValue == map.value("name")) openCollection(collectionId);
            return;
        }
    }
}
void KodiMedia::renameCollection(const QString &id, const QString &name)
{
    auto entries = collections();
    const auto clean = name.trimmed();
    if (clean.isEmpty() || clean.size() > 80) return;
    for (const auto &entry : entries) if (entry.toMap().value("id") != id && entry.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0) {
        m_collectionStatus = "A collection already has that name."; emit changed(); return;
    }
    for (auto &entry : entries) {
        auto map = entry.toMap();
        if (map.value("id") == id) { map["name"] = clean; entry = map; storeCollections(entries); return; }
    }
}
void KodiMedia::removeCollection(const QString &id)
{
    auto entries = collections();
    for (qsizetype i = 0; i < entries.size(); ++i) if (entries[i].toMap().value("id") == id) {
        entries.removeAt(i); storeCollections(entries); return;
    }
}
void KodiMedia::openCollection(const QString &id)
{
    if (!canNavigate()) return;
    m_pendingShow = 0;
    for (const auto &entry : collections()) {
        const auto map = entry.toMap();
        if (map.value("id") == id) {
            m_mediaKind = map.value("media", "movies").toString();
            if (!QStringList{"movies", "shows"}.contains(m_mediaKind)) return;
            m_history.clear(); m_showId = m_season = -1; m_showTitle.clear();
            if (map.value("kind") == "manual") {
                m_viewKind = "manual"; m_viewValue = map.value("name").toString();
                m_manualMembers = map.value("members").toList(); m_selectedIndex = 0;
                beginRead(0);
                return;
            }
            setView(map.value("kind").toString(), map.value("value").toString()); return;
        }
    }
}
void KodiMedia::scanMovies()
{
    if (m_busy || m_playbackActive || !m_connected) return;
    ++m_generation;
    m_busy = true;
    m_stage = 2;
    m_observedScan = false;
    m_scanPolls = 0;
    m_scanStatus = "Requesting a scan of this profile's configured video folders…";
    emit changed();
    request("VideoLibrary.Scan");
}
void KodiMedia::playMovie(int movieId, bool resume)
{
    if (m_mediaKind != "movies") return;
    playLibraryItem("movie", movieId, resume);
}
void KodiMedia::playLibraryItem(const QString &kind, int id, bool resume, bool requireListed)
{
    if (m_busy || m_playbackActive || (requireListed && !m_connected)) return;
    bool listed = false;
    for (const auto &item : m_movies) listed |= item.toMap().value(kind + "id").toInt() == id;
    if ((requireListed && !listed) || id < 1) return;
    ++m_generation;
    m_scheduledPlayback=false;
    m_playbackActive = true; m_playbackVisible = false; m_busy = true; m_stage = 5;
    m_presentationError.clear();
    m_playbackStatus = "Starting " + kind + "…";
    emit changed();
    request(kind == "movie" ? "Isolde.PlayMovie" : kind == "song" ? "Isolde.PlaySong" : kind == "channel" ? "Isolde.PlayChannel" : "Isolde.PlayEpisode", {{kind + "id", id}, {"resume", (kind=="song" || kind=="channel")?false:resume}});
}
bool KodiMedia::playScheduled(const QString &kind, int id, int offsetSeconds)
{
    if (m_busy || m_playbackActive || id<1 || offsetSeconds<0 || offsetSeconds>=86400
        || !QStringList{"movie","episode","musicvideo","song"}.contains(kind)) return false;
    ++m_generation;
    m_scheduledPlayback=true;
    m_playbackActive=true; m_playbackVisible=false; m_busy=true; m_stage=5;
    m_presentationError.clear(); m_playbackStatus="Tuning to the channel's broadcast position…";
    emit changed();
    request("Isolde.PlayScheduled",{{"mediaType",kind},{"mediaId",id},{"offsetSeconds",offsetSeconds}});
    return m_playbackActive;
}
bool KodiMedia::playPvr(int id,int clientId,int uniqueId)
{
    if(m_busy || m_playbackActive || id<1)return false;
    ++m_generation;m_scheduledPlayback=false;m_playbackActive=true;m_playbackVisible=false;m_busy=true;m_stage=5;
    m_presentationError.clear();m_playbackStatus="Tuning to the configured Kodi PVR channel…";emit changed();
    request("Isolde.PlayChannel",{{"channelid",id},{"resume",false},{"clientid",clientId},{"uniqueid",uniqueId}});
    return m_playbackActive;
}
void KodiMedia::presentationFailed()
{
    if (!m_playbackActive) return;
    m_presentationError = "Could not show the video window. Playback was asked to stop. This setup requires X11/XWayland and one Kodi window.";
    m_playbackStatus = m_presentationError;
    playbackControl("stop");
    emit changed();
}
void KodiMedia::playbackControl(const QString &action)
{
    if (!m_playbackActive || !QStringList{"pause", "stop", "home", "forward", "backward"}.contains(action)) return;
    if (m_busy) { if (action == "stop" || action == "home") m_pendingControl = action; return; }
    m_busy = true; m_stage = 7;
    request("Isolde.PlaybackControl", {{"action", action}});
}
void KodiMedia::request(const QString &method, const QJsonObject &params)
{
    releaseSocket();
    m_received.clear();
    m_request = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", ++m_id}, {"method", method}, {"params", params}}).toJson(QJsonDocument::Compact) + '\n';
#ifdef Q_OS_LINUX
    m_descriptor = ::open(m_directory.toLocal8Bit().constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (m_descriptor < 0) { fail("Kodi's local bridge is not set up for this profile yet."); return; }
    m_timeout.start();
    m_socket.connectToServer(QString("/proc/self/fd/%1/rpc.sock").arg(m_descriptor));
#else
    fail("The local Kodi bridge currently requires Linux.");
#endif
}
void KodiMedia::consume()
{
    if (!m_busy) return;
    m_received += m_socket.readAll();
    if (m_received.size() > 4 * 1024 * 1024) { fail("Kodi returned too much data."); return; }
    if (!m_received.endsWith('\n')) return;
    const auto response = QJsonDocument::fromJson(m_received).object();
    if (response.value("jsonrpc") != "2.0" || response.value("id").toInt(-1) != m_id) {
        fail(m_mediaKind=="channels"?"TV guide unavailable. Configure a PVR TV source in Kodi, then refresh.":"Kodi returned an incompatible response."); return;
    }
    if (response.contains("error")) {
        if(m_stage==9){m_busy=false;m_timeout.stop();releaseSocket();m_playlistStatus="Could not save this Kodi playlist. Previous playlists are unchanged. Check storage and retry.";emit changed();return;}
        if(m_mediaKind=="playlists" || m_viewKind=="playlist"){
            fail("Kodi video playlist unavailable. Refresh to retry; choose a library playlist of the selected kind. Catalog limit: 128 saved playlists.");return;
        }
        if (m_stage == 8) {
            m_timeout.stop(); releaseSocket(); ++m_manualMissingCount;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_generation && m_busy && m_stage == 8) loadNextManualMovie();
            });
            return;
        }
        fail(m_mediaKind=="channels"?"TV guide unavailable. Configure a PVR TV source in Kodi, then refresh.":"Kodi returned an incompatible response."); return;
    }
    if (m_stage != 2 && m_stage != 4 && !response.value("result").isObject()) {
        fail("Kodi returned an incompatible response."); return;
    }
    const auto result = response.value("result").toObject();
    // Finish this reply before initiating a new connection (no reentrant socket reads).
    m_timeout.stop();
    if(m_stage==9){
        m_busy=false;releaseSocket();const auto filename=result["playlistName"].toString();
        if(!result["saved"].toBool() || !validPlaylistName(filename) || !filename.startsWith("Isolde-")){
            m_playlistStatus="Kodi returned an invalid playlist-save acknowledgement. Refresh the playlist list before retrying.";emit changed();return;
        }
        m_playlistStatus="Playlist saved in Kodi · "+result["name"].toString();emit changed();emit playlistSaved(filename);return;
    }
    if (m_stage == 5 || m_stage == 6 || m_stage == 7) {
        const auto state = result.value("state").toString();
        if (!QStringList{"starting", "playing", "paused", "ended", "failed", "replaced", "idle"}.contains(state)
            || !result.value("home").isBool()) { fail("Invalid playback status."); return; }
        m_busy = false; releaseSocket();
        m_playbackStatus = m_presentationError.isEmpty() ? state : m_presentationError;
        m_playbackCanPause=result.value("canPause").isBool() && result.value("canPause").toBool();
        m_playbackCanSeek=result.value("canSeek").isBool() && result.value("canSeek").toBool();
        if(result.value("controlError").isString())m_playbackStatus=result.value("controlError").toString().left(512);
        if (state=="failed" && result.value("reason").isString()) m_playbackStatus=result.value("reason").toString().left(512);
        if (state == "starting" || state == "playing" || state == "paused") {
            m_playbackPoll.start();
            if (state != "starting" && !m_playbackVisible) { m_playbackVisible = true; emit playbackStarted(); }
        } else {
            m_playbackPoll.stop(); m_playbackActive = m_playbackVisible = false;
            m_lastPlaybackNaturalEnd=state=="ended" && result.value("naturalEnd").isBool() && result.value("naturalEnd").toBool();
            emit playbackReturned(result.value("home").toBool());
            if (state == "ended" && !m_scheduledPlayback) {
                // Kodi persists progress on stop; update the current shelf after
                // that write without replacing the user's filter or selection.
                const auto generation = m_generation;
                QTimer::singleShot(1200, this, [this, generation] {
                    if (generation == m_generation && m_connected && !m_busy && !m_playbackActive) loadPage(m_pageStart);
                });
            }
        }
        emit changed();
        if (!m_pendingControl.isEmpty()) {
            const auto action = m_pendingControl;
            m_pendingControl.clear();
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, action, generation] { if (generation == m_generation) playbackControl(action); });
        }
        return;
    }
    if (m_stage == 4) {
        const bool accepted = response.value("result") == "OK";
        m_busy = false;
        releaseSocket();
        emit quitReply(accepted);
        return;
    }
    if (m_stage == 8) {
        const bool shows = m_manualType == "tvshow";
        const auto item = result.value(shows ? "tvshowdetails" : "moviedetails").toObject();
        const auto idKey = shows ? QString("tvshowid") : QString("movieid");
        if (item.value(idKey).toInt(-1) != m_manualCurrentId || item.value("title").toString().isEmpty()) {
            m_timeout.stop(); releaseSocket(); ++m_manualMissingCount;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_generation && m_busy && m_stage == 8) loadNextManualMovie();
            });
            return;
        }
        auto entry = item.toVariantMap();
        entry["mediaType"] = shows ? "tvshow" : "movie"; entry["mediaId"] = m_manualCurrentId;
        m_manualResults.append(entry);
        releaseSocket();
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (generation == m_generation && m_busy && m_stage == 8) loadNextManualMovie();
        });
        return;
    }
    if (m_stage >= 2) {
        if (m_stage == 2) {
            if (response.value("result") != "OK") { fail("Kodi did not accept the scan."); return; }
            m_stage = 3;
            m_scanStatus = "Scan requested. Waiting for Kodi's scan status…";
        } else {
            const auto scanning = result.value("Library.IsScanningVideo");
            if (!scanning.isBool()) { fail("Kodi returned an invalid scan status."); return; }
            ++m_scanPolls;
            m_observedScan = m_observedScan || scanning.toBool();
            if ((!scanning.toBool() && (m_observedScan || m_scanPolls >= 5)) || m_scanPolls >= 1800) {
                m_scanStatus = scanning.toBool() ? "Kodi is still scanning. Monitoring stopped after 30 minutes."
                    : m_observedScan ? "Kodi finished scanning. Your files were not moved or renamed."
                    : "Kodi accepted the scan and is idle. A short scan may have finished between checks; no new movies are guaranteed.";
                m_busy = false;
                releaseSocket();
                refresh();
                return;
            }
            if (scanning.toBool()) m_scanStatus = "Kodi is scanning configured video folders. You can keep using Isolde.";
        }
        releaseSocket();
        emit changed();
        const auto generation = m_generation;
        QTimer::singleShot(1000, this, [this, generation] {
            if (m_busy && generation == m_generation)
                request("XBMC.GetInfoBooleans", {{"booleans", QJsonArray{"Library.IsScanningVideo"}}});
        });
        return;
    }
    if (m_stage == 0) {
        if (result.value("version").toObject().value("major").toInt() != 13) {
            fail("This Kodi API version has not been validated. Open Kodi directly for now."); return;
        }
        m_stage = 1;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (!m_busy || generation != m_generation) return;
            loadPage(m_requestedStart);
        });
        return;
    }
    const QString listKey = m_mediaKind == "history" ? (m_recentEpisodes ? "episodes" : "movies") : m_mediaKind == "shows" ? "tvshows" : m_mediaKind;
    if (!result.value(listKey).isArray()) { fail("Kodi returned an invalid library list."); return; }
    const auto entries = result.value(listKey).toArray();
    const auto limits = result.value("limits").toObject();
    const int total = limits.value("total").toInt(-1);
    if (entries.isEmpty() && total >= 0 && total < m_requestedStart) {
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] { if (m_busy && generation == m_generation) loadPage(0); });
        return;
    }
    if (entries.size() > (m_mediaKind=="playlists"?128:50) || total < 0 || limits.value("start").toInt(-1) != m_requestedStart
        || limits.value("end").toInt(-1) != m_requestedStart + entries.size()
        || total < m_requestedStart + entries.size() || (entries.isEmpty() && total > m_requestedStart)) {
        fail("Kodi returned invalid library paging information. Refresh to retry."); return;
    }
    m_pageStart = m_requestedStart;
    m_pageCount = entries.size();
    m_totalMovies = total;
    const auto defaultType = m_mediaKind == "playlists" ? "playlist" : m_mediaKind == "channels" ? "channel" : m_mediaKind == "albums" ? "album" : m_mediaKind == "songs" ? "song" : m_mediaKind == "history" ? (m_recentEpisodes ? "episode" : "movie") : m_mediaKind == "movies" ? "movie" : m_mediaKind == "shows" ? "tvshow"
        : m_mediaKind == "seasons" ? "season" : "episode";
    QVariantList normalized;
    QSet<QString> ids;
    for (const auto &value : entries) {
        auto entry = value.toObject().toVariantMap();
        const QString type=m_viewKind=="playlist"?entry.value("playlistMediaType").toString():QString(defaultType);
        if(m_viewKind=="playlist" && !(m_mediaKind=="movies"?QStringList{"movie"}:QStringList{"tvshow","episode"}).contains(type)){fail("Kodi returned an invalid playlist media kind.");return;}
        if(m_mediaKind=="playlists" && !validPlaylistName(entry.value("playlistName").toString())){fail("Kodi returned an invalid playlist name.");return;}
        const auto identityKey = m_mediaKind == "seasons" ? "season" : QString(type) + "id";
        const int identity = entry.value(identityKey, -1).toInt();
        const auto identityToken=type+":"+QString::number(identity);
        if (identity < (m_mediaKind == "seasons" ? 0 : 1) || ids.contains(identityToken)) { fail("Kodi returned invalid or duplicate library identities."); return; }
        ids.insert(identityToken);
        entry["mediaType"] = type; entry["mediaId"] = entry.value(QString(type) + "id", -1);
        if (m_mediaKind == "history") {
            const auto date = entry.value("lastplayed").toString();
            const auto parsed = QDateTime::fromString(date, "yyyy-MM-dd HH:mm:ss");
            if (!parsed.isValid() || parsed.toString("yyyy-MM-dd HH:mm:ss") != date || date < "1970-01-02 00:00:00") continue;
            entry["viewedAt"] = date; entry["providerId"] = "kodi";
        }
        if (entry.value("title").toString().isEmpty()) entry["title"] = m_mediaKind == "seasons"
            ? (identity == 0 ? QString("Specials") : "Season " + QString::number(identity)) : entry.value("label");
        if(m_mediaKind=="channels"){
            if(entry.value("hidden").toBool())continue;
            const auto now=entry.value("broadcastnow").toMap(),next=entry.value("broadcastnext").toMap();
            entry["title"]=QString::number(entry.value("channelnumber").toInt())+" · "+entry.value("title").toString();
            entry["subtitle"]=now.value("title").toString()+(next.value("title").toString().isEmpty()?QString():" · Next: "+next.value("title").toString());
            entry["plot"]=now.value("plot");
        }
        normalized.append(entry);
    }
    if (m_mediaKind == "history") {
        m_recentResults.append(normalized);
        if (!m_recentEpisodes) {
            m_recentEpisodes = true; releaseSocket();
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation] { if (m_busy && generation == m_generation) loadPage(0); });
            return;
        }
        std::sort(m_recentResults.begin(), m_recentResults.end(), [](const QVariant &a, const QVariant &b) {
            const auto left=a.toMap(),right=b.toMap();
            const auto date=QString::compare(left["viewedAt"].toString(),right["viewedAt"].toString());
            if(date) return date>0;
            if(left["mediaType"]!=right["mediaType"])return left["mediaType"].toString()<right["mediaType"].toString();
            return left["mediaId"].toInt()<right["mediaId"].toInt();
        });
        while(m_recentResults.size()>50)m_recentResults.removeLast();
        normalized=m_recentResults; m_recentResults.clear();
        m_pageStart=0; m_pageCount=normalized.size(); m_totalMovies=normalized.size();
    }
    m_movies = normalized;
    m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
    m_status = m_movies.isEmpty() ? (m_mediaKind == "channels" ? "No TV channels in this profile. Configure a PVR TV source in Kodi, then refresh." : m_mediaKind == "albums" ? "No albums in this profile's Kodi music library. Add music sources in Kodi, then refresh." : m_mediaKind == "history" ? "Nothing viewed in this profile yet. Kodi viewing history appears here." : m_viewKind == "all" ? "No items in this Kodi library view. Configure sources in Kodi or refresh to retry."
        : m_viewKind == "continue" ? "No items with saved progress in this profile." : "Not found in your Kodi library.")
        : QString("%1 · %2–%3 of %4").arg(browseTitle()).arg(m_pageStart + 1).arg(m_pageStart + m_movies.size()).arg(m_totalMovies);
    m_busy = false;
    m_connected = true;
    releaseSocket();
    emit changed();
    if (m_pendingMovie > 0) {
        const int movie = m_pendingMovie;
        m_pendingMovie = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation, movie] {
            if (generation != m_generation) return;
            bool listed = false;
            for (const auto &entry : m_movies) listed |= entry.toMap().value("movieid").toInt() == movie;
            if (listed) playMovie(movie, true);
            else { m_status = "This movie is no longer in the current collection page. Choose a movie from the refreshed list."; emit changed(); }
        });
    }
    if (m_pendingShow > 0) {
        const int tvShowId = m_pendingShow;
        m_pendingShow = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation, tvShowId] {
            if (generation != m_generation) return;
            for (int index = 0; index < m_movies.size(); ++index) {
                if (m_movies[index].toMap().value("tvshowid").toInt() == tvShowId) {
                    m_selectedIndex = index;
                    activate(index);
                    return;
                }
            }
            m_status = "This show is no longer in the selected collection. Choose a show from the refreshed list.";
            emit changed();
        });
    }
    if (!m_pendingDirectKind.isEmpty()) {
        const auto kind = m_pendingDirectKind;
        const auto id = m_pendingDirectId;
        const auto resume = m_pendingDirectResume;
        m_pendingDirectKind.clear(); m_pendingDirectId = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation, kind, id, resume] {
            if (generation == m_generation && !m_busy && !m_playbackActive) playMedia(kind, id, resume);
        });
    }
}
