#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include "GuideFeeds.h"
#include "KodiMedia.h"
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif

static QVariantList sortedHistory(QVariantList items, int limit) {
    std::sort(items.begin(), items.end(), [](const QVariant& a, const QVariant& b) {
        const auto left = a.toMap(), right = b.toMap();
        const int date = QString::compare(left["viewedAt"].toString(), right["viewedAt"].toString());
        if (date)
            return date > 0;
        if (left["providerId"] != right["providerId"])
            return left["providerId"].toString() < right["providerId"].toString();
        if (left["mediaType"] != right["mediaType"])
            return left["mediaType"].toString() < right["mediaType"].toString();
        if (left["mediaType"] == "app")
            return left["appId"].toString() < right["appId"].toString();
        return left["mediaId"].toInt() < right["mediaId"].toInt();
    });
    while (items.size() > limit)
        items.removeLast();
    return items;
}
static QString historyIdentity(const QVariantMap& item) {
    return item.value("providerId").toString() + "/" + item.value("mediaType").toString() + "/" +
           item.value("mediaId").toString();
}
void KodiMedia::restoreHistorySelection() {
    if (m_mediaKind != "history" || m_historySelection.isEmpty())
        return;
    for (int index = 0; index < m_movies.size(); ++index)
        if (historyIdentity(m_movies[index].toMap()) == m_historySelection) {
            m_selectedIndex = index;
            return;
        }
}
KodiMedia::KodiMedia(const QString& profile, QObject* parent, const QString& testDirectory) : QObject(parent) {
    m_profile = profile;
    m_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles/" + profile +
                  "/kodi/home/.kodi/userdata/isolde";
    if (!testDirectory.isEmpty())
        m_directory = testDirectory;
    m_playbackPoll.setInterval(500);
    connect(&m_playbackPoll, &QTimer::timeout, this, [this] {
        if (!m_busy && m_playbackActive) {
            m_busy = true;
            m_stage = 6;
            request("Isolde.PlaybackStatus");
        }
    });
    m_socket.setReadBufferSize(4 * 1024 * 1024 + 1);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(3000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        fail("Kodi did not respond. You can retry without leaving Isolde.");
    });
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        m_socket.write(m_request);
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, &KodiMedia::consume);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        if (error == QLocalSocket::PeerClosedError && m_received.endsWith('\n'))
            return;
        if (m_busy)
            fail("Kodi's local bridge is unavailable for this profile. Start Kodi and enable the bridge, then retry.");
    });
}
KodiMedia::~KodiMedia() {
    m_busy = false;
    releaseSocket();
}
void KodiMedia::releaseSocket() {
    m_timeout.stop();
    m_socket.abort();
#ifdef Q_OS_LINUX
    if (m_descriptor >= 0)
        ::close(m_descriptor);
#endif
    m_descriptor = -1;
}
void KodiMedia::fail(const QString& message, bool forceDisconnect) {
    if (m_busy && m_stage >= 22 && m_stage <= 24) {
        m_sourcePreviewReady = false;
        m_sourceStatus = "Source operation could not be confirmed. Refresh to recover before another edit. " + message;
        if (!forceDisconnect) {
            m_busy = false;
            releaseSocket();
            emit changed();
            return;
        }
    }
    if (!forceDisconnect && m_busy && (m_stage == 20 || m_stage == 21)) {
        m_busy = false;
        releaseSocket();
        m_playbackOptionsStatus = "Playback options unavailable. " + message;
        emit changed();
        return;
    }
    if (!forceDisconnect && m_busy && (m_stage == 14 || m_stage == 15)) {
        m_busy = false;
        m_subtitleStatus = QStringLiteral("Kodi subtitle operation failed: %1").arg(message);
        releaseSocket();
        emit changed();
        return;
    }
    if (((m_stage >= 9 && m_stage <= 12) || (m_stage >= 16 && m_stage <= 19)) && m_busy)
        m_playlistStatus =
            "Playlist operation could not be confirmed. Refresh the saved-playlist list before retrying. " + message;
    m_pendingMovie = m_pendingShow = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    const bool wasPlaying = m_playbackActive || m_adjacentTransition;
    const bool returnHome = m_returnHomeRequested;
    m_adjacentTransition = false;
    m_adjacentGlobal = m_adjacentAfterRead = -1;
    m_returnHomeRequested = false;
    m_lastPlaybackNaturalEnd = false;
    m_pendingControl.clear();
    m_playbackPoll.stop();
    m_playbackActive = m_playbackVisible = false;
    if (wasPlaying)
        m_playbackStatus = "Playback status unavailable. Kodi may still be playing. " + message;
    const bool quitting = m_busy && m_stage == 4;
    if (m_busy && (m_stage == 2 || m_stage == 3))
        m_scanStatus = "Scan status unavailable. " + message;
    ++m_generation;
    m_connected = false;
    m_busy = false;
    releaseSocket();
    m_movies.clear();
    m_recentResults.clear();
    m_pageStart = m_totalMovies = 0;
    if (!m_retired && m_mediaKind == "history") {
        m_movies = sortedHistory(m_appOpenings, m_historyLimit);
        m_totalMovies = m_pageCount = m_movies.size();
        restoreHistorySelection();
        m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
    }
    m_status = message;
    emit changed();
    if (wasPlaying)
        emit playbackReturned(returnHome);
    if (quitting)
        emit quitReply(false);
}
void KodiMedia::quitIfIdle() {
    if (m_busy)
        return;
    ++m_generation;
    m_busy = true;
    m_stage = 4;
    request("Isolde.QuitIfIdle");
}
void KodiMedia::invalidate(const QString& reason) {
    m_optionsRequest = 0;
    m_playbackOptions.clear();
    m_sourcePreviewReady = false;
    fail(reason, true);
}
void KodiMedia::retire(const QString& reason) {
    m_retired = true;
    m_optionsRequest = 0;
    m_sourcePreviewReady = false;
    m_playbackOptions.clear();
    m_subtitleTracks.clear();
    m_librarySources.clear();
    m_sourceScrapers.clear();
    m_sourceRoots.clear();
    m_sourceDraft.clear();
    m_sourceRevision.clear();
    m_appOpenings.clear();
    m_historySelection.clear();
    fail(reason, true);
}
void KodiMedia::refresh() {
    if (m_busy || m_playbackActive)
        return;
    beginRead(0);
}
void KodiMedia::beginRead(int start) {
    if (m_retired)
        return;
    if (m_mediaKind != "history")
        m_historySelection.clear();
    else if (m_selectedIndex >= 0 && m_selectedIndex < m_movies.size())
        m_historySelection = historyIdentity(m_movies[m_selectedIndex].toMap());
    m_recentResults.clear();
    m_recentEpisodes = false;
    ++m_generation;
    m_busy = false;
    releaseSocket();
    m_connected = false;
    m_movies.clear();
    m_totalMovies = 0;
    m_pageCount = 0;
    m_pageStart = m_requestedStart = start;
    m_busy = true;
    m_stage = 0;
    m_status = "Connecting to this profile's local Kodi…";
    emit changed();
    request("JSONRPC.Version");
}
bool KodiMedia::canNavigate() const {
    return !m_retired && !m_playbackActive && (!m_busy || m_stage == 0 || m_stage == 1);
}
QString KodiMedia::browseTitle() const {
    if (m_mediaKind == "playlists")
        return "Kodi video playlists";
    if (m_viewKind == "playlist")
        return "Kodi playlist · " + m_viewValue;
    if (m_mediaKind == "history")
        return "Recently viewed";
    if (m_mediaKind == "albums")
        return "Music · Albums";
    if (m_mediaKind == "songs")
        return m_showTitle.isEmpty() ? "Music · Tracks" : m_showTitle;
    if (m_mediaKind == "channels")
        return "TV Guide";
    if (m_viewKind == "manual")
        return m_viewValue;
    if (m_mediaKind == "movies")
        return "Movies";
    if (m_mediaKind == "shows")
        return "Shows";
    if (m_mediaKind == "seasons")
        return m_showTitle + " · Seasons";
    if (m_showId < 1) {
        if (m_viewKind == "search")
            return "Search Results · Episodes";
        return m_viewKind == "continue"  ? "Continue Watching · Episodes"
               : m_viewKind == "watched" ? "Recently Watched · Episodes"
                                          : "Recently Added · Episodes";
    }
    return m_showTitle + (m_season == 0 ? " · Specials" : " · Season " + QString::number(m_season));
}
KodiMedia::BrowseState KodiMedia::browseState() const {
    return {m_mediaKind, m_viewKind, m_viewValue, m_showTitle, m_showId, m_season, m_pageStart, m_selectedIndex};
}
void KodiMedia::restoreBrowse(const BrowseState& state) {
    m_pendingShow = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    m_mediaKind = state.kind;
    m_viewKind = state.view;
    m_viewValue = state.value;
    m_showTitle = state.showTitle;
    m_showId = state.show;
    m_season = state.season;
    m_selectedIndex = state.selected;
    beginRead(state.start);
}
void KodiMedia::selectIndex(int index) {
    index = qBound(-1, index, qMax(-1, int(m_movies.size()) - 1));
    if (index == m_selectedIndex)
        return;
    m_selectedIndex = index;
    emit changed();
}
void KodiMedia::openLibrary(const QString& kind) {
    if (!canNavigate() || !QStringList{"movies", "shows"}.contains(kind))
        return;
    m_pendingShow = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    if (m_mediaKind == kind && m_history.isEmpty() && m_viewKind == "all") {
        if (!m_busy)
            beginRead(m_pageStart);
        return;
    }
    m_history.clear();
    m_pendingMovie = 0;
    restoreBrowse({kind, "all", {}, {}, -1, -1, 0, 0});
}
bool KodiMedia::validPlaylistName(const QString& name) {
    if (name.isEmpty() || name.size() > 128 || name.contains(QRegularExpression("[/\\\\:%\\x00-\\x1f]")))
        return false;
    return name.endsWith(".xsp", Qt::CaseInsensitive) || name.endsWith(".m3u", Qt::CaseInsensitive) ||
           name.endsWith(".m3u8", Qt::CaseInsensitive);
}
void KodiMedia::openPlaylists() {
    if (!canNavigate())
        return;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    restoreBrowse({"playlists", "all", {}, {}, -1, -1, 0, 0});
}
void KodiMedia::openHistory(int count) {
    if (count < 1 || count > 50 || !canNavigate())
        return;
    const int selected = m_mediaKind == "history" ? m_selectedIndex : 0;
    m_historyLimit = count;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    restoreBrowse({"history", "watched", {}, {}, -1, -1, 0, selected});
}
void KodiMedia::setAppOpenings(const QVariantList& items) {
    if (m_retired || items.size() > 50)
        return;
    QVariantList accepted;
    QSet<QString> identities;
    for (const auto& value : items) {
        const auto item = value.toMap();
        const auto id = item.value("appId").toString();
        const auto date = item.value("viewedAt").toString();
        const auto parsed = QDateTime::fromString(date, "yyyy-MM-dd HH:mm:ss");
        if (!QRegularExpression("^[a-z0-9][a-z0-9.-]{0,79}$").match(id).hasMatch() || identities.contains(id) ||
            item.value("providerId") != "isolde-app-openings" || item.value("mediaType") != "app" ||
            !parsed.isValid() || parsed.toString("yyyy-MM-dd HH:mm:ss") != date ||
            item.value("title").toString().size() > 160)
            return;
        identities.insert(id);
        accepted.append(item);
    }
    m_appOpenings = accepted;
}
void KodiMedia::openView(const QString& kind, const QString& view, const QString& value) {
    if (!canNavigate() || !QStringList{"movies", "shows", "episodes", "history", "albums", "channels"}.contains(kind) ||
        !QStringList{"all", "recent", "watched", "continue", "search", "tag", "genre", "playlist"}.contains(view) ||
        (view == "playlist" && (!QStringList{"movies", "shows"}.contains(kind) || !validPlaylistName(value))) ||
        ((kind == "albums" || kind == "channels") && (view != "all" || !value.isEmpty())) ||
        (kind == "history" && (view != "watched" || !value.isEmpty())) || (kind == "episodes" && view != "search") ||
        value.trimmed().size() > 160 || ((view == "tag" || view == "genre") && (value.trimmed().isEmpty())))
        return;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    restoreBrowse({kind == "shows" && (view == "recent" || view == "continue" || view == "watched") ? "episodes" : kind,
                   view,
                   value.trimmed(),
                   {},
                   -1,
                   -1,
                   0,
                   0});
}
void KodiMedia::openShowResult(const QString& query, int tvShowId, int pageStart) {
    const auto text = query.trimmed();
    if (!canNavigate() || tvShowId < 1 || text.isEmpty() || text.size() > 160 || pageStart < 0 || pageStart % 50 != 0 ||
        pageStart > 10000000)
        return;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    m_mediaKind = "shows";
    m_viewKind = "search";
    m_viewValue = text;
    m_showId = m_season = -1;
    m_showTitle.clear();
    m_selectedIndex = 0;
    m_pendingShow = tvShowId;
    beginRead(pageStart);
}
void KodiMedia::playMedia(const QString& kind, int id, bool resume) {
    if (m_retired || !QStringList{"movie", "episode", "song", "channel"}.contains(kind) || id < 1 || m_playbackActive)
        return;
    if (m_busy) {
        if (m_stage != 0 && m_stage != 1) {
            m_playbackStatus = "Kodi is busy. Wait for the current library operation to finish.";
            emit changed();
            return;
        }
        m_pendingDirectKind = kind;
        m_pendingDirectId = id;
        m_pendingDirectResume = resume;
        m_status = "Opening the selected local result after Kodi finishes its library read…";
        emit changed();
        return;
    }
    playLibraryItem(kind, id, resume, false);
}
void KodiMedia::activate(int index) {
    if (m_busy || !m_connected || m_playbackActive || index < 0 || index >= m_movies.size())
        return;
    selectIndex(index);
    const auto item = m_movies[index].toMap();
    if (m_viewKind == "playlist" && item.value("mediaType").toString() == "episode") {
        playSelected(true);
    } else if (m_mediaKind == "albums") {
        const int album = item.value("albumid").toInt();
        if (album < 1)
            return;
        m_history.append(browseState());
        restoreBrowse({"songs", "album", QString::number(album), item.value("title").toString(), -1, -1, 0, 0});
    } else if (m_mediaKind == "shows" || m_mediaKind == "seasons") {
        auto next = browseState();
        if (m_mediaKind == "shows") {
            if (item.value("tvshowid").toInt() < 1)
                return;
            next.kind = "seasons";
            next.show = item.value("tvshowid").toInt();
            next.showTitle = item.value("title", item.value("label")).toString();
            next.season = -1;
        } else {
            if (!item.contains("season") || item.value("season").toInt() < 0)
                return;
            next.kind = "episodes";
            next.season = item.value("season").toInt();
        }
        m_history.append(browseState());
        next.view = "all";
        next.value.clear();
        next.start = next.selected = 0;
        restoreBrowse(next);
    } else
        playSelected(true);
}
bool KodiMedia::goBack() {
    if (!canNavigate() || m_history.isEmpty())
        return false;
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    restoreBrowse(m_history.takeLast());
    return true;
}
void KodiMedia::playSelected(bool resume) {
    if (m_selectedIndex < 0 || m_selectedIndex >= m_movies.size())
        return;
    const auto item = m_movies[m_selectedIndex].toMap();
    if (m_viewKind == "playlist" && item.value("mediaType").toString() == "episode")
        playLibraryItem("episode", item.value("episodeid").toInt(), resume);
    else if (m_mediaKind == "movies")
        playLibraryItem("movie", item.value("movieid").toInt(), resume);
    else if (m_mediaKind == "episodes")
        playLibraryItem("episode", item.value("episodeid").toInt(), resume);
    else if (m_mediaKind == "songs")
        playLibraryItem("song", item.value("songid").toInt(), false);
    else if (m_mediaKind == "channels")
        playLibraryItem("channel", item.value("channelid").toInt(), false);
}
void KodiMedia::nextPage() {
    if (!m_busy && !m_playbackActive && hasNextPage()) {
        m_selectedIndex = 0;
        loadPage(m_pageStart + 50);
    }
}
void KodiMedia::previousPage() {
    if (!m_busy && !m_playbackActive && m_connected && m_pageStart > 0) {
        m_selectedIndex = 0;
        loadPage(qMax(0, m_pageStart - 50));
    }
}
void KodiMedia::loadPage(int start) {
    if (m_viewKind == "manual") {
        loadManualPage(start);
        return;
    }
    m_requestedStart = start;
    m_busy = true;
    m_stage = 1;
    m_status = "Loading " + browseTitle() + "…";
    emit changed();
    QJsonArray fields{"title", "year", "plot", "runtime", "resume", "playcount", "art", "uniqueid"};
    if (m_mediaKind == "playlists") {
        request("Isolde.GetPlaylists");
        return;
    }
    if (m_viewKind == "playlist") {
        request("Isolde.GetPlaylistItems",
                {{"name", m_viewValue}, {"kind", m_mediaKind}, {"start", start}, {"end", start + 50}});
        return;
    }
    QString method = "VideoLibrary.GetMovies";
    if (m_mediaKind == "shows") {
        method = "VideoLibrary.GetTVShows";
        fields = {"title", "year", "plot", "episode", "watchedepisodes", "art", "uniqueid"};
    } else if (m_mediaKind == "seasons") {
        method = "VideoLibrary.GetSeasons";
        fields = {"season", "showtitle", "episode", "watchedepisodes", "tvshowid", "art"};
    } else if (m_mediaKind == "episodes" || (m_mediaKind == "history" && m_recentEpisodes)) {
        method = "VideoLibrary.GetEpisodes";
        fields = {
            "title", "plot", "runtime", "resume", "playcount", "season", "episode", "showtitle", "tvshowid", "art"};
    }
    if (m_mediaKind == "history")
        fields.append("lastplayed");
    if (m_mediaKind == "albums") {
        method = "AudioLibrary.GetAlbums";
        fields = {"title", "artist", "year", "art"};
    } else if (m_mediaKind == "songs") {
        method = "AudioLibrary.GetSongs";
        fields = {"title", "artist", "album", "albumid", "track", "duration", "art"};
    } else if (m_mediaKind == "channels") {
        method = "PVR.GetChannels";
        fields = {"channelnumber", "broadcastnow", "broadcastnext", "locked", "hidden"};
    }
    const auto sort = m_mediaKind == "songs"                                    ? "track"
                      : (m_viewKind == "continue" || m_viewKind == "watched")   ? "lastplayed"
                      : m_viewKind == "recent"                                  ? "dateadded"
                      : (m_mediaKind == "episodes" || m_mediaKind == "seasons") ? "episode"
                                                                                 : "title";
    QJsonObject params{
        {"properties", fields},
        {"sort",
         QJsonObject{{"method", sort},
                     {"order",
                      (m_viewKind == "continue" || m_viewKind == "recent" || m_viewKind == "watched") ? "descending"
                                                                                                     : "ascending"},
                     {"ignorearticle", true}}},
        {"limits", QJsonObject{{"start", start}, {"end", start + (m_mediaKind == "history" ? m_historyLimit : 50)}}}};
    if (m_viewKind == "continue")
        params.insert("filter", QJsonObject{{"field", "inprogress"}, {"operator", "true"}, {"value", ""}});
    else if (m_viewKind == "watched")
        params.insert("filter", QJsonObject{{"field", "lastplayed"}, {"operator", "after"}, {"value", "1970-01-01"}});
    else if (m_viewKind != "all" && m_viewKind != "recent")
        params.insert("filter",
                      QJsonObject{{"field", m_viewKind == "search" ? "title" : m_viewKind},
                                  {"operator", m_viewKind == "search" ? "contains" : "is"},
                                  {"value", m_viewValue}});
    if ((m_mediaKind == "seasons" || m_mediaKind == "episodes") && m_showId > 0)
        params["tvshowid"] = m_showId;
    if (m_mediaKind == "episodes" && m_season >= 0)
        params["season"] = m_season;
    if (m_mediaKind == "songs")
        params.insert("filter", QJsonObject{{"albumid", m_viewValue.toInt()}});
    if (m_mediaKind == "channels") {
        params.remove("sort");
        params.insert("channelgroupid", "alltv");
    }
    request(method, params);
}
void KodiMedia::loadManualPage(int start) {
    if (start < 0 || start % 50 != 0 || start > m_manualMembers.size())
        return;
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
void KodiMedia::loadNextManualMovie() {
    if (m_manualNextIndex >= m_requestedStart + m_pageCount) {
        m_movies = m_manualResults;
        m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
        m_busy = false;
        m_connected = true;
        m_stage = 1;
        if (m_movies.isEmpty())
            m_status = m_pageCount == 0 ? "This manual collection is empty. Open a library item and add it with Menu."
                                        : "No saved items on this page are available in Kodi.";
        else
            m_status = m_manualMissingCount > 0
                           ? QString("%1 saved item(s) are no longer available in Kodi.").arg(m_manualMissingCount)
                           : QString("%1 items in this manual collection.").arg(m_totalMovies);
        releaseSocket();
        emit changed();
        if (m_pendingMovie > 0) {
            const int movieId = m_pendingMovie;
            m_pendingMovie = 0;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation, movieId] {
                if (generation != m_generation)
                    return;
                bool listed = false;
                for (const auto& entry : m_movies)
                    listed |= entry.toMap().value("movieid").toInt() == movieId;
                if (listed)
                    playMovie(movieId, true);
                else {
                    m_status = "This movie is no longer in the manual collection page.";
                    emit changed();
                }
            });
        }
        if (m_pendingShow > 0) {
            const int showId = m_pendingShow;
            m_pendingShow = 0;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation, showId] {
                if (generation != m_generation)
                    return;
                for (int index = 0; index < m_movies.size(); ++index) {
                    if (m_movies[index].toMap().value("tvshowid").toInt() == showId) {
                        m_selectedIndex = index;
                        activate(index);
                        return;
                    }
                }
                m_status = "This show is no longer in the manual collection.";
                emit changed();
            });
        }
        return;
    }
    m_manualCurrentId = m_manualMembers[m_manualNextIndex++].toInt();
    const bool shows = m_manualType == "tvshow";
    request(shows ? "VideoLibrary.GetTVShowDetails" : "VideoLibrary.GetMovieDetails",
            {{shows ? "tvshowid" : "movieid", m_manualCurrentId},
             {"properties",
              shows ? QJsonArray{"title", "year", "plot", "episode", "watchedepisodes", "art"}
                    : QJsonArray{"title", "year", "plot", "runtime", "resume", "playcount", "art"}}});
}
void KodiMedia::setView(const QString& kind, const QString& value) {
    if (m_playbackActive || (m_busy && m_stage != 0 && m_stage != 1))
        return;
    if (!QStringList{"all", "continue", "recent", "watched", "search", "tag", "genre"}.contains(kind) ||
        value.trimmed().size() > 160)
        return;
    const auto text = value.trimmed();
    if ((kind == "tag" || kind == "genre") && text.isEmpty())
        return;
    if ((kind == "tag" || kind == "genre") && !QStringList{"movies", "shows"}.contains(m_mediaKind))
        return;
    if (m_mediaKind == "seasons")
        return;
    const auto previous = browseState();
    m_pendingMovie = m_pendingShow = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    ++m_generation;
    m_busy = false;
    releaseSocket();
    m_viewKind = kind == "search" && text.isEmpty() ? "all" : kind;
    m_viewValue = (m_viewKind == "all" || m_viewKind == "continue" || m_viewKind == "recent" || m_viewKind == "watched")
                      ? QString()
                      : text;
    if (m_mediaKind != "movies") {
        if (kind == "continue" || kind == "recent" || kind == "watched") {
            if (m_mediaKind == "shows") {
                m_history.append(previous);
                m_mediaKind = "episodes";
                m_showId = m_season = -1;
                m_showTitle.clear();
            }
        }
    }
    m_selectedIndex = 0;
    refresh();
}
void KodiMedia::playMovie(int movieId, bool resume) {
    if (m_mediaKind != "movies")
        return;
    playLibraryItem("movie", movieId, resume);
}
void KodiMedia::playLibraryItem(const QString& kind, int id, bool resume, bool requireListed) {
    if (m_busy || m_playbackActive || (requireListed && !m_connected))
        return;
    bool listed = false;
    for (const auto& item : m_movies)
        listed |= item.toMap().value(kind + "id").toInt() == id;
    if ((requireListed && !listed) || id < 1)
        return;
    ++m_generation;
    m_playingKind = kind;
    m_playingId = id;
    m_returnHomeRequested = false;
    m_playbackOptions.clear();
    m_scheduledPlayback = false;
    m_optionsRequest = 0;
    m_playbackActive = true;
    m_playbackVisible = false;
    m_busy = true;
    m_stage = 5;
    m_presentationError.clear();
    m_playbackStatus = "Starting " + kind + "…";
    emit changed();
    request(kind == "movie"     ? "Isolde.PlayMovie"
            : kind == "song"    ? "Isolde.PlaySong"
            : kind == "channel" ? "Isolde.PlayChannel"
                                : "Isolde.PlayEpisode",
            {{kind + "id", id}, {"resume", (kind == "song" || kind == "channel") ? false : resume}});
}
bool KodiMedia::playScheduled(const QString& kind, int id, int offsetSeconds) {
    if (m_busy || m_playbackActive || id < 1 || offsetSeconds < 0 || offsetSeconds >= 86400 ||
        !QStringList{"movie", "episode", "musicvideo", "song"}.contains(kind))
        return false;
    ++m_generation;
    m_playingKind = kind;
    m_playingId = id;
    m_returnHomeRequested = false;
    m_playbackOptions.clear();
    m_scheduledPlayback = true;
    m_scheduledKind = kind;
    m_scheduledId = id;
    m_optionsRequest = 0;
    m_playbackActive = true;
    m_playbackVisible = false;
    m_busy = true;
    m_stage = 5;
    m_presentationError.clear();
    m_playbackStatus = "Tuning to the channel's broadcast position…";
    emit changed();
    request("Isolde.PlayScheduled", {{"mediaType", kind}, {"mediaId", id}, {"offsetSeconds", offsetSeconds}});
    return m_playbackActive;
}
bool KodiMedia::playGuideStream(const QString& url) {
    if (m_busy || m_playbackActive || !GuideFeeds::streamUrl(url))
        return false;
    ++m_generation;
    m_playingKind = "stream";
    m_playingId = 0;
    m_returnHomeRequested = false;
    m_playbackOptions.clear();
    m_scheduledPlayback = false;
    m_optionsRequest = 0;
    m_playbackActive = true;
    m_playbackVisible = false;
    m_busy = true;
    m_stage = 5;
    m_presentationError.clear();
    m_playbackStatus = "Connecting to the reviewed live stream…";
    emit changed();
    request("Isolde.PlayGuideStream", {{"url", url}});
    return m_playbackActive;
}
bool KodiMedia::playPvr(int id, int clientId, int uniqueId) {
    if (m_busy || m_playbackActive || id < 1)
        return false;
    ++m_generation;
    m_playingKind = "channel";
    m_playingId = id;
    m_returnHomeRequested = false;
    m_playbackOptions.clear();
    m_scheduledPlayback = false;
    m_optionsRequest = 0;
    m_playbackActive = true;
    m_playbackVisible = false;
    m_busy = true;
    m_stage = 5;
    m_presentationError.clear();
    m_playbackStatus = "Tuning to the configured Kodi PVR channel…";
    emit changed();
    request("Isolde.PlayChannel",
            {{"channelid", id}, {"resume", false}, {"clientid", clientId}, {"uniqueid", uniqueId}});
    return m_playbackActive;
}
void KodiMedia::presentationFailed() {
    if (!m_playbackActive)
        return;
    m_presentationError = "Could not show the video window. Playback was asked to stop. This setup requires "
                          "X11/XWayland and one Kodi window.";
    m_playbackStatus = m_presentationError;
    playbackControl("stop");
    emit changed();
}
void KodiMedia::playbackControl(const QString& action) {
    if (action == "home" || action == "stop") {
        if (action == "home")
            m_returnHomeRequested = true;
        if (m_adjacentTransition) {
            m_adjacentGlobal = m_adjacentAfterRead = -1;
            m_adjacentTransition = false;
            if (!m_playbackActive) {
                ++m_generation;
                m_busy = false;
                releaseSocket();
                emit changed();
                emit playbackReturned(action == "home");
                m_returnHomeRequested = false;
                return;
            }
        }
    }
    if (!m_playbackActive || !QStringList{"pause", "stop", "home", "forward", "backward"}.contains(action))
        return;
    if (m_busy) {
        if (action == "stop" || action == "home")
            m_pendingControl = action;
        return;
    }
    m_busy = true;
    m_stage = 7;
    request("Isolde.PlaybackControl", {{"action", action}});
}
bool KodiMedia::adjacentScope() const {
    if (!m_playbackActive || m_scheduledPlayback || !m_connected ||
        QStringList{"manual", "history"}.contains(m_viewKind) ||
        !QStringList{"movie", "episode", "song"}.contains(m_playingKind) || m_selectedIndex < 0 ||
        m_selectedIndex >= m_movies.size())
        return false;
    const auto item = m_movies[m_selectedIndex].toMap();
    return item.value("mediaType").toString() == m_playingKind && item.value("mediaId").toInt() == m_playingId;
}
bool KodiMedia::playbackCanPrevious() const {
    return adjacentScope() && m_pageStart + m_selectedIndex > 0;
}
bool KodiMedia::playbackCanNext() const {
    return adjacentScope() && m_pageStart + m_selectedIndex + 1 < m_totalMovies;
}
bool KodiMedia::canPlayAdjacent(int direction) const {
    return !m_busy && (direction == -1 || direction == 1) &&
           (direction < 0 ? playbackCanPrevious() : playbackCanNext());
}
bool KodiMedia::playAdjacent(int direction) {
    if (!canPlayAdjacent(direction))
        return false;
    const int target = m_pageStart + m_selectedIndex + direction;
    playbackControl("stop");
    m_adjacentGlobal = target;
    m_adjacentTransition = true;
    m_playbackStatus = direction < 0 ? "Opening the previous title…" : "Opening the next title…";
    emit changed();
    return true;
}
void KodiMedia::finishAdjacent() {
    if (m_adjacentAfterRead < 0)
        return;
    const int index = m_adjacentAfterRead - m_pageStart;
    m_adjacentAfterRead = -1;
    if (index < 0 || index >= m_movies.size()) {
        m_adjacentTransition = false;
        m_playbackStatus = "The adjacent item is no longer available. Choose a title from the refreshed list.";
        emit changed();
        emit playbackReturned(false);
        return;
    }
    const auto item = m_movies[index].toMap();
    const auto kind = item.value("mediaType").toString();
    const int id = item.value("mediaId").toInt();
    m_selectedIndex = index;
    if (!QStringList{"movie", "episode", "song"}.contains(kind) || id < 1) {
        m_adjacentTransition = false;
        m_playbackStatus = "Choose a specific playable title from this library view.";
        emit changed();
        emit playbackReturned(false);
        return;
    }
    const auto generation = m_generation;
    QTimer::singleShot(0, this, [this, generation, kind, id] {
        if (generation == m_generation && m_adjacentTransition && !m_busy && !m_playbackActive)
            playLibraryItem(kind, id, true);
    });
}
void KodiMedia::loadSubtitle(const QString& path) {
    const QFileInfo subtitle(path);
    if (!m_playbackActive || !subtitle.isFile() ||
        subtitle.suffix().compare(QStringLiteral("srt"), Qt::CaseInsensitive) != 0 ||
        subtitle.size() > 8 * 1024 * 1024) {
        m_playbackStatus = QStringLiteral("Load a completed local SRT while an Isolde-managed video is playing.");
        emit changed();
        return;
    }
    if (m_busy) {
        m_playbackStatus = QStringLiteral("Kodi is busy. Wait for the current request and retry loading the subtitle.");
        emit changed();
        return;
    }
    m_busy = true;
    m_stage = 13;
    request(QStringLiteral("Isolde.LoadSubtitle"), {{QStringLiteral("path"), subtitle.absoluteFilePath()}});
}
void KodiMedia::refreshSubtitleTracks() {
    if (!m_playbackActive || m_busy) {
        m_subtitleStatus =
            m_playbackActive
                ? QStringLiteral("Kodi is busy. Wait and retry subtitle track discovery.")
                : QStringLiteral("Subtitle tracks are available only during Isolde-managed video playback.");
        emit changed();
        return;
    }
    m_busy = true;
    m_stage = 14;
    m_subtitleStatus = QStringLiteral("Reading subtitle tracks from the managed Kodi player…");
    request(QStringLiteral("Isolde.GetSubtitleTracks"));
}
void KodiMedia::refreshPlaybackOptions() {
    if (!m_playbackActive || m_busy)
        return;
    m_busy = true;
    m_stage = 20;
    m_playbackOptionsStatus = "Reading this video's audio, subtitle and viewing options…";
    emit changed();
    request("Isolde.GetPlaybackOptions");
}
void KodiMedia::setPlaybackOption(const QString& kind, const QVariant& value) {
    if (!m_playbackActive || m_busy || !QStringList{"audio", "subtitle", "view"}.contains(kind))
        return;
    m_busy = true;
    m_stage = 21;
    m_playbackOptionsStatus = "Applying the selected option in Kodi…";
    emit changed();
    request("Isolde.SetPlaybackOption", {{"kind", kind}, {"value", QJsonValue::fromVariant(value)}});
}
void KodiMedia::setSubtitleTrack(int trackId) {
    if (!m_playbackActive || m_busy || trackId < -1) {
        m_subtitleStatus = QStringLiteral("Choose an available subtitle track during Isolde-managed video playback.");
        emit changed();
        return;
    }
    m_busy = true;
    m_stage = 15;
    m_subtitleStatus = QStringLiteral("Applying subtitle track selection in Kodi…");
    request(QStringLiteral("Isolde.SetSubtitleTrack"),
            {{QStringLiteral("subtitleId"), trackId < 0 ? QJsonValue(QStringLiteral("off")) : QJsonValue(trackId)}});
}
void KodiMedia::request(const QString& method, const QJsonObject& params) {
    if (m_retired) {
        fail("This profile is no longer active.");
        return;
    }
    releaseSocket();
    m_timeout.setInterval(m_stage >= 22 && m_stage <= 24 ? 25000 : 3000);
    m_received.clear();
    m_request = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", ++m_id}, {"method", method}, {"params", params}})
                    .toJson(QJsonDocument::Compact) +
                '\n';
#ifdef Q_OS_LINUX
    m_descriptor = ::open(m_directory.toLocal8Bit().constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (m_descriptor < 0) {
        fail("Kodi's local bridge is not set up for this profile yet.");
        return;
    }
    m_timeout.start();
    m_socket.connectToServer(QString("/proc/self/fd/%1/rpc.sock").arg(m_descriptor));
#else
    fail("The local Kodi bridge currently requires Linux.");
#endif
}
