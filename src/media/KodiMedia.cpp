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

QVariantList
sortedHistory(QVariantList items, int limit)
{
    std::sort(items.begin(),
              items.end(),
              [](const QVariant& a, const QVariant& b)
              {
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
static QString
historyIdentity(const QVariantMap& item)
{
    return item.value("providerId").toString() + "/" + item.value("mediaType").toString() + "/" +
           item.value("mediaId").toString();
}
void
KodiMedia::restoreHistorySelection()
{
    if (m_mediaKind != "history" || m_historySelection.isEmpty())
        return;
    for (int index = 0; index < m_movies.size(); ++index)
        if (historyIdentity(m_movies[index].toMap()) == m_historySelection)
        {
            m_selectedIndex = index;
            return;
        }
}
KodiMedia::KodiMedia(const QString& profile, QObject* parent, const QString& testDirectory) : QObject(parent)
{
    m_profile = profile;
    m_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/native-profiles/" + profile +
                  "/kodi/home/.kodi/userdata/isolde";
    if (!testDirectory.isEmpty())
        m_directory = testDirectory;
    QSettings settings;
    m_nextEpisodeOptIn = settings.value(QString("profiles/%1/nextEpisodeOptIn").arg(profile), false).toBool();
    m_playbackPoll.setInterval(500);
    connect(&m_playbackPoll,
            &QTimer::timeout,
            this,
            [this]
            {
                if (!m_busy && m_playbackActive)
                {
                    m_busy = true;
                    m_stage = 6;
                    request("Isolde.PlaybackStatus");
                }
            });
    m_socket.setReadBufferSize(4 * 1024 * 1024 + 1);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(3000);
    connect(&m_timeout,
            &QTimer::timeout,
            this,
            [this]
            {
                fail("Kodi did not respond. You can retry without leaving Isolde.");
            });
    connect(&m_socket,
            &QLocalSocket::connected,
            this,
            [this]
            {
                m_socket.write(m_request);
            });
    connect(&m_socket, &QLocalSocket::readyRead, this, &KodiMedia::consume);
    connect(&m_socket,
            &QLocalSocket::errorOccurred,
            this,
            [this](QLocalSocket::LocalSocketError error)
            {
                if (error == QLocalSocket::PeerClosedError && m_received.endsWith('\n'))
                    return;
                if (m_busy)
                    fail(
                        "Kodi's local bridge is unavailable for this profile. Start Kodi and enable the bridge, then "
                        "retry.");
            });
}
KodiMedia::~KodiMedia()
{
    m_busy = false;
    releaseSocket();
}
/** Releases the Kodi JSON-RPC socket connection.
 * @brief Releases the Kodi JSON-RPC socket connection.
 * @details Closes the active socket, resets the descriptor, and stops the timeout timer.
 * @why Frees the network socket and prevents resource leaks when the guide is no longer needed.
 * @return none
 */
void
KodiMedia::releaseSocket()
{
    m_timeout.stop();
    m_socket.abort();
    #ifdef Q_OS_LINUX
    if (m_descriptor >= 0)
        ::close(m_descriptor);
    #endif
    m_descriptor = -1;
}
/** Handles a failed operation on the Kodi media bridge.
 * @brief Handles a failed operation on the Kodi media bridge.
 * @details Rolls back any in-progress state, clears pending media, aborts the socket, and emits error/playback signals.
 * @why Ensures a failed Kodi operation leaves the catalog in a consistent state without leaving partial data.
 * @param message Human-readable failure message to surface to the user.
 * @param forceDisconnect If true, force-disconnects the socket even during active maintenance windows.
 * @return none
 */
/** Handles a failed operation on the Kodi media bridge.
 * @brief Handles a failed operation on the Kodi media bridge.
 * @details Rolls back any in-progress state, clears pending media, aborts the socket, and emits error/playback signals.
 * @why Ensures a failed Kodi operation leaves the catalog in a consistent state without leaving partial data behind.
 * @param message Human-readable failure message to surface to the user.
 * @param forceDisconnect If true, force-disconnects the socket even during active maintenance windows.
 * @return none
 */
void
KodiMedia::fail(const QString& message, bool forceDisconnect)
{
    if (failSourcePreview(message, forceDisconnect))
        return;
    if (failPlaybackOptions(message, forceDisconnect))
        return;
    if (failSubtitle(message, forceDisconnect))
        return;
    failPlaylist(message);
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
    failScan(message);
    ++m_generation;
    m_connected = false;
    m_busy = false;
    releaseSocket();
    m_movies.clear();
    m_recentResults.clear();
    m_pageStart = m_totalMovies = 0;
    failRestoreHistory();
    m_status = message;
    emit changed();
    if (wasPlaying)
        emit playbackReturned(returnHome);
    if (quitting)
        emit quitReply(false);
}

/** Handles a source preview failure during channel import.
 * @brief Handles a failed source preview during Kodi media bridge import.
 * @details Clears source preview state, releases the socket, and emits changed signal when not force-disconnecting.
 * @why Prevents stale source preview state from persisting after a failed preview operation.
 * @param message Human-readable failure message describing why the source preview failed.
 * @param forceDisconnect If true, forces socket disconnection even during active maintenance stages.
 * @return true if the failure was handled (preview state cleared), false otherwise.
 */
bool
KodiMedia::failSourcePreview(const QString& message, bool forceDisconnect)
{
    if (m_busy && m_stage >= 22 && m_stage <= 24)
    {
        m_sourcePreviewReady = false;
        m_sourceStatus = "Source operation could not be confirmed. Refresh to recover before another edit. " + message;
        if (!forceDisconnect) {
            m_busy = false;
            releaseSocket();
            emit changed();
            return true;
        }
    }
    return false;
}

/** Handles playback option failures during Kodi media import.
 * @brief Handles a failed playback options operation during Kodi media bridge import.
 * @details Clears playback options state, releases the socket, and emits changed signal when not force-disconnecting.
 * @why Prevents stale playback option state from persisting after a failed playback options operation.
 * @param message Human-readable failure message describing why playback options failed.
 * @param forceDisconnect If true, forces socket disconnection even during active maintenance stages.
 * @return true if the failure was handled (playback options state cleared), false otherwise.
 */
bool
KodiMedia::failPlaybackOptions(const QString& message, bool forceDisconnect)
{
    if (!forceDisconnect && m_busy && (m_stage == 20 || m_stage == 21))
    {
        m_busy = false;
        releaseSocket();
        m_playbackOptionsStatus = "Playback options unavailable. " + message;
        emit changed();
        return true;
    }
    return false;
}

/** Handles a subtitle playback option failure during Kodi media import.
 * @brief Handles a failed subtitle playback option operation during Kodi media bridge import.
 * @details Clears subtitle status, releases the socket, and emits changed signal when not force-disconnecting.
 * @why Prevents stale subtitle state from persisting after a failed subtitle operation.
 * @param message Human-readable failure message describing why the subtitle operation failed.
 * @param forceDisconnect If true, forces socket disconnection even during active maintenance stages.
 * @return true if the failure was handled (subtitle state cleared), false otherwise.
 */
bool
KodiMedia::failSubtitle(const QString& message, bool forceDisconnect)
{
    if (!forceDisconnect && m_busy && (m_stage == 14 || m_stage == 15))
    {
        m_busy = false;
        m_subtitleStatus = QStringLiteral("Kodi subtitle operation failed: %1").arg(message);
        releaseSocket();
        emit changed();
        return true;
    }
    return false;
}

/** Handles a playlist operation failure during Kodi media import.
 * @brief Handles a failed playlist operation during Kodi media bridge import.
 * @details Updates the playlist status message to indicate the operation could not be confirmed.
 * @why Prevents stale playlist state from persisting after a failed playlist operation.
 * @param message Human-readable failure message describing why the playlist operation failed.
 * @return none
 */
void
KodiMedia::failPlaylist(const QString& message)
{
    if (((m_stage >= 9 && m_stage <= 12) || (m_stage >= 16 && m_stage <= 19) || m_stage == 25 || m_stage == 26) && m_busy)
        m_playlistStatus =
            "Playlist operation could not be confirmed. Refresh the saved-playlist list before retrying. " + message;
}

/** Handles a scan failure during Kodi media import.
 * @brief Handles a failed scan operation during Kodi media bridge import.
 * @details Updates the scan status message to indicate the scan is unavailable.
 * @why Prevents stale scan state from persisting after a failed scan operation.
 * @param message Human-readable failure message describing why the scan failed.
 * @return none
 */
void
KodiMedia::failScan(const QString& message)
{
    if (m_busy && (m_stage == 2 || m_stage == 3))
        m_scanStatus = "Scan status unavailable. " + message;
}

/** Restores history state for a Kodi media guide channel.
 * @brief Restores history state for a Kodi media guide channel.
 * @details Restores the movies list from app openings history, sets total movies and page count, restores history selection, and clamps selected index.
 * @why Ensures the history channel reflects previously opened apps and maintains the correct selected index within bounds.
 * @return none
 */
void
KodiMedia::failRestoreHistory()
{
    if (!m_retired && m_mediaKind == "history")
    {
        m_movies = sortedHistory(m_appOpenings, m_historyLimit);
        m_totalMovies = m_pageCount = static_cast<int>(m_movies.size());
        restoreHistorySelection();
        m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
    }
}
/** Quits the Kodi media bridge if idle.
 * @brief Quits the Kodi media bridge if idle.
 * @details If not busy, increments generation, marks busy, sets stage to quit (4), and requests quit via Kodi bridge.
 * @why Allows the media bridge to exit cleanly when no operation is in progress.
 * @return none
 */
void
KodiMedia::quitIfIdle()
{
    if (m_busy)
        return;
    ++m_generation;
    m_busy = true;
    m_stage = 4;
    request("Isolde.QuitIfIdle");
}
void
KodiMedia::invalidate(const QString& reason)
{
    m_optionsRequest = 0;
    m_playbackOptions.clear();
    m_sourcePreviewReady = false;
    fail(reason, true);
}

void
KodiMedia::setNextEpisodeOptIn(bool enabled)
{
    // Persists the next-episode auto-play preference scoped to the active
    // Isolde profile. Stored as a QSettings INI value keyed by profile ID so
    // each profile independently controls whether the next episode plays
    // automatically after the current one ends.
    QSettings settings;
    settings.setValue(QString("profiles/%1/nextEpisodeOptIn").arg(qApp->property("isoldeProfile").toString()), enabled);
    m_nextEpisodeOptIn = enabled;
    emit changed();
}
void
KodiMedia::retire(const QString& reason)
{
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
void
KodiMedia::refresh()
{
    if (m_busy || m_playbackActive)
        return;
    beginRead(0);
}
void
KodiMedia::beginRead(int start)
{
    m_returnSelectionId = -1;
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
/** Checks if navigation is allowed.
 * @brief Checks if navigation is allowed.
 * @details Returns true if not retired, not playback active, and not busy (or in stage 0 or 1).
 * @why Prevents navigation actions during active operations or retired states.
 * @return true if navigation is allowed, false otherwise.
 */
bool
KodiMedia::canNavigate() const
{
    return !m_retired && !m_playbackActive && (!m_busy || m_stage == 0 || m_stage == 1);
}
/** Returns the browse title for the current media kind.
 * @brief Returns the browse title for the current media kind.
 * @details Dispatches to the appropriate browse title helper based on media kind: playlists, playlist view, history, music, channels, manual, movies/shows, seasons, episodes, or default.
 * @why Provides a human-readable title for the current media browsing context.
 * @return Human-readable browse title string.
 */
QString
KodiMedia::browseTitle() const
{
    if (m_mediaKind == "playlists")
        return browseTitlePlaylists();
    if (m_viewKind == "playlist")
        return browseTitlePlaylistView();
    if (m_mediaKind == "history")
        return browseTitleHistory();
    if (m_mediaKind == "albums")
        return browseTitleMusic();
    if (m_mediaKind == "songs")
        return browseTitleMusic();
    if (m_mediaKind == "channels")
        return browseTitleChannels();
    if (m_viewKind == "manual")
        return browseTitleManual();
    if (m_mediaKind == "movies")
        return browseTitleMoviesShows();
    if (m_mediaKind == "shows")
        return browseTitleMoviesShows();
    if (m_mediaKind == "seasons")
        return browseTitleSeasons();
    if (m_showId < 1)
        return browseTitleEpisodes();
    return browseTitleDefault();
}

/** Returns the browse title for playlists media kind.
 * @brief Returns the browse title for playlists media kind.
 * @details Returns the literal string "Kodi video playlists".
 * @why Provides a title for the playlists media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitlePlaylists() const
{
    return "Kodi video playlists";
}

/** Returns the browse title for playlist view.
 * @brief Returns the browse title for playlist view.
 * @details Returns a formatted string combining "Kodi playlist ·" with the current view value.
 * @why Provides a title for the playlist view context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitlePlaylistView() const
{
    return "Kodi playlist · " + m_viewValue;
}

/** Returns the browse title for history media kind.
 * @brief Returns the browse title for history media kind.
 * @details Returns the literal string "Recently viewed".
 * @why Provides a title for the history media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleHistory() const
{
    return "Recently viewed";
}

/** Returns the browse title for music media kind.
 * @brief Returns the browse title for music media kind.
 * @details Returns "Music · Albums" for albums media kind, or the show title if available, otherwise "Music · Tracks".
 * @why Provides a title for the music media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleMusic() const
{
    if (m_mediaKind == "albums")
        return "Music · Albums";
    return m_showTitle.isEmpty() ? "Music · Tracks" : m_showTitle;
}

/** Returns the browse title for channels media kind.
 * @brief Returns the browse title for channels media kind.
 * @details Returns the literal string "TV Guide".
 * @why Provides a title for the channels media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleChannels() const
{
    return "TV Guide";
}

/** Returns the browse title for manual media kind.
 * @brief Returns the browse title for manual media kind.
 * @details Returns the current view value string.
 * @why Provides a title for the manual media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleManual() const
{
    return m_viewValue;
}

/** Returns the browse title for movies/shows media kind.
 * @brief Returns the browse title for movies/shows media kind.
 * @details Returns "Movies" for movies media kind, otherwise "Shows".
 * @why Provides a title for the movies/shows media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleMoviesShows() const
{
    if (m_mediaKind == "movies")
        return "Movies";
    return "Shows";
}

/** Returns the browse title for seasons media kind.
 * @brief Returns the browse title for seasons media kind.
 * @details Returns the show title followed by " · Seasons".
 * @why Provides a title for the seasons media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleSeasons() const
{
    return m_showTitle + " · Seasons";
}

/** Returns the browse title for episodes media kind.
 * @brief Returns the browse title for episodes media kind.
 * @details Returns "Search Results · Episodes" for search view kind, "Continue Watching · Episodes" for continue view kind, "Recently Watched · Episodes" for watched view kind, or "Recently Added · Episodes" otherwise.
 * @why Provides a title for the episodes media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleEpisodes() const
{
    if (m_viewKind == "search")
        return "Search Results · Episodes";
    return m_viewKind == "continue" ? "Continue Watching · Episodes"
           : m_viewKind == "watched" ? "Recently Watched · Episodes"
                                     : "Recently Added · Episodes";
}

/** Returns the browse title for default media kind.
 * @brief Returns the browse title for default media kind.
 * @details Returns the show title followed by " · Specials" for season 0, otherwise " · Season N".
 * @why Provides a title for the default media kind context.
 * @return Human-readable title string.
 */
QString
KodiMedia::browseTitleDefault() const
{
    return m_showTitle + (m_season == 0 ? " · Specials" : " · Season " + QString::number(m_season));
}
/** Returns the current browse state.
 * @brief Returns the current browse state.
 * @details Returns a BrowseState struct containing mediaKind, viewKind, viewValue, showTitle, showId, season, pageStart, and selectedIndex.
 * @why Captures the current browsing state for save/restore operations.
 * @return BrowseState struct with current browse state.
 */
KodiMedia::BrowseState
KodiMedia::browseState() const
{
    const auto selected = m_movies.value(m_selectedIndex).toMap();
    return {m_mediaKind, m_viewKind, m_viewValue, m_showTitle, m_showId, m_season, m_pageStart, m_selectedIndex,
            selected.value("mediaType").toString(), selected.value("mediaId", -1).toInt()};
}
/** Restores browse state from a saved BrowseState.
 * @brief Restores browse state from a saved BrowseState.
 * @details Resets pending fields, sets mediaKind/viewKind/viewValue/showTitle/showId/season/selectedIndex from the state, then begins reading at the saved page start.
 * @why Restores the browsing state after navigation or restoration from cache.
 * @param state The BrowseState to restore.
 * @return none
 */
void
KodiMedia::restoreBrowse(const BrowseState& state)
{
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
/** Selects an index in the current browse list.
 * @brief Selects an index in the current browse list.
 * @details Clamps the index to valid range and updates m_selectedIndex if changed, emitting changed signal.
 * @why Allows selection of a specific media item in the current browse context.
 * @param index The index to select.
 * @return none
 */
void
KodiMedia::selectIndex(int index)
{
    index = qBound(-1, index, qMax(-1, int(m_movies.size()) - 1));
    if (index == m_selectedIndex)
        return;
    m_selectedIndex = index;
    emit changed();
}
/** Opens a library view for the given kind.
 * @brief Opens a library view for the given kind.
 * @details If navigation is allowed and kind is movies/shows, clears pending state and restores browse to the library kind with view "all".
 * @why Opens a library view for browsing movies or shows content.
 * @param kind The library kind to open ("movies" or "shows").
 * @return none
 */
void
KodiMedia::openLibrary(const QString& kind)
{
    if (!canNavigate() || !QStringList{"movies", "shows"}.contains(kind))
        return;
    m_pendingShow = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    if (m_mediaKind == kind && m_history.isEmpty() && m_viewKind == "all")
    {
        if (!m_busy)
            beginRead(m_pageStart);
        return;
    }
    m_history.clear();
    m_pendingMovie = 0;
    restoreBrowse({kind, "all", {}, {}, -1, -1, 0, 0});
}
/** Opens playlists media kind.
 * @brief Opens playlists media kind.
 * @details If navigation is allowed, clears history and pending state, then restores browse to playlists view.
 * @why Opens the playlists media kind for browsing available playlists.
 * @return none
 */
// Restrict playlist identities to bounded filenames handled by Kodi.
bool
KodiMedia::validPlaylistName(const QString& name)
{
    if (name.isEmpty() || name.size() > 128 || name.contains(QRegularExpression("[/\\\\:%\\x00-\\x1f]")))
        return false;
    return name.endsWith(".xsp", Qt::CaseInsensitive) || name.endsWith(".m3u", Qt::CaseInsensitive) ||
           name.endsWith(".m3u8", Qt::CaseInsensitive);
}

void
KodiMedia::openPlaylists()
{
    if (!canNavigate())
        return;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    restoreBrowse({"playlists", "all", {}, {}, -1, -1, 0, 0});
}
/** Opens history media kind.
 * @brief Opens history media kind.
 * @details If history count is valid and navigation is allowed, clears history and pending state, then restores browse to history/watched view.
 * @why Opens the history media kind for browsing recently viewed items.
 * @param count The history count limit.
 * @return none
 */
void
KodiMedia::openHistory(int count)
{
    if (!validateHistoryCount(count) || !canNavigate())
        return;
    const int selected = m_mediaKind == "history" ? m_selectedIndex : 0;
    m_historyLimit = count;
    m_history.clear();
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    restoreBrowse({"history", "watched", {}, {}, -1, -1, 0, selected});
}

/** Validates history count.
 * @brief Validates history count.
 * @details Checks that count is between 1 and 50 inclusive.
 * @why Ensures history count is within acceptable bounds.
 * @param count The history count to validate.
 * @return true if count is valid, false otherwise.
 */
bool
KodiMedia::validateHistoryCount(int count) const
{
    return count >= 1 && count <= 50;
}
/** Sets app openings from a list of items.
 * @brief Sets app openings from a list of items.
 * @details Validates each item and builds accepted list of app openings with valid identities.
 * @why Processes app opening events from Kodi and stores them for history tracking.
 * @param items The list of app opening items.
 * @return none
 */
void
KodiMedia::setAppOpenings(const QVariantList& items)
{
    if (m_retired || items.size() > 50)
        return;
    QVariantList accepted;
    QSet<QString> identities;
    for (const auto& value : items)
    {
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
/** Opens a view for the given kind and view.
 * @brief Opens a view for the given kind and view.
 * @details Validates the view, clears history and pending state, then restores browse to the given kind/view/value.
 * @why Opens a media browsing view for the given kind and view.
 * @param kind The media kind to open.
 * @param view The view type.
 * @param value The view value.
 * @return none
 */
void
KodiMedia::openView(const QString& kind, const QString& view, const QString& value)
{
    if (!openViewValid(kind, view, value))
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

/** Validates an open view operation.
 * @brief Validates an open view operation.
 * @details Checks navigation, kind, view, playlist, albums/channels, history, episodes, value length, and tag/genre validity.
 * @why Ensures view opening requests are valid before processing.
 * @param kind The media kind.
 * @param view The view type.
 * @param value The view value.
 * @return true if the view operation is valid, false otherwise.
 */
bool
KodiMedia::openViewValid(const QString& kind, const QString& view, const QString& value)
{
    return openViewCanNavigate() &&
           openViewValidKind(kind) &&
           openViewValidView(view) &&
           openViewValidPlaylist(kind, view, value) &&
           openViewValidAlbumsChannels(kind, view, value) &&
           openViewValidHistory(kind, view, value) &&
           openViewValidEpisodes(kind, view) &&
           openViewValidValueLength(value) &&
           openViewValidTagGenre(value, view);
}

/** Checks if navigation is allowed for open view operations.
 * @brief Checks if navigation is allowed for open view operations.
 * @details Delegates to canNavigate().
 * @why Delegates navigation check to shared canNavigate function.
 * @return true if navigation is allowed, false otherwise.
 */
bool
KodiMedia::openViewCanNavigate() const
{
    return canNavigate();
}

/** Validates open view kind.
 * @brief Validates open view kind.
 * @details Checks that kind is one of movies, shows, episodes, history, albums, channels.
 * @why Ensures valid media kind for open view operations.
 * @param kind The media kind to validate.
 * @return true if kind is valid, false otherwise.
 */
bool
KodiMedia::openViewValidKind(const QString& kind)
{
    return QStringList{"movies", "shows", "episodes", "history", "albums", "channels", "musicvideos", "pictures"}.contains(kind);
}

/** Validates open view view type.
 * @brief Validates open view view type.
 * @details Checks that view is one of all, recent, watched, continue, search, tag, genre, playlist.
 * @why Ensures valid view type for open view operations.
 * @param view The view type to validate.
 * @return true if view is valid, false otherwise.
 */
bool
KodiMedia::openViewValidView(const QString& view)
{
    return QStringList{"all", "recent", "watched", "continue", "search", "tag", "genre", "playlist"}.contains(view);
}

/** Validates open view playlist.
 * @brief Validates open view playlist.
 * @details If view is playlist, checks that kind is movies/shows and value is valid playlist name.
 * @why Ensures playlist view kind is valid for movies/shows media kinds.
 * @param kind The media kind.
 * @param view The view type.
 * @param value The view value.
 * @return true if playlist view is valid, false otherwise.
 */
bool
KodiMedia::openViewValidPlaylist(const QString& kind, const QString& view, const QString& value)
{
    if (view != "playlist")
        return true;
    return QStringList{"movies", "shows"}.contains(kind) && validPlaylistName(value);
}

/** Validates open view albums/channels.
 * @brief Validates open view albums/channels.
 * @details If kind is albums/channels, checks that view is all and value is empty.
 * @why Ensures albums/channels media kinds use all view with empty value.
 * @param kind The media kind.
 * @param view The view type.
 * @param value The view value.
 * @return true if albums/channels view is valid, false otherwise.
 */
bool
KodiMedia::openViewValidAlbumsChannels(const QString& kind, const QString& view, const QString& value)
{
    if (kind != "albums" && kind != "channels")
        return true;
    return view == "all" && value.isEmpty();
}

/** Validates open view history.
 * @brief Validates open view history.
 * @details If kind is history, checks that view is watched and value is empty.
 * @why Ensures history media kind uses watched view with empty value.
 * @param kind The media kind.
 * @param view The view type.
 * @param value The view value.
 * @return true if history view is valid, false otherwise.
 */
bool
KodiMedia::openViewValidHistory(const QString& kind, const QString& view, const QString& value)
{
    if (kind != "history")
        return true;
    return view == "watched" && value.isEmpty();
}

/** Validates open view episodes.
 * @brief Validates open view episodes.
 * @details If kind is episodes, checks that view is search.
 * @why Ensures episodes media kind uses search view.
 * @param kind The media kind.
 * @param view The view type.
 * @return true if episodes view is valid, false otherwise.
 */
bool
KodiMedia::openViewValidEpisodes(const QString& kind, const QString& view)
{
    if (kind != "episodes")
        return true;
    return view == "search";
}

/** Validates open view value length.
 * @brief Validates open view value length.
 * @details Checks that value trimmed size is at most 160 characters.
 * @why Ensures view value is within length limits.
 * @param value The view value to validate.
 * @return true if value length is valid, false otherwise.
 */
bool
KodiMedia::openViewValidValueLength(const QString& value)
{
    return value.trimmed().size() <= 160;
}

/** Validates open view tag/genre.
 * @brief Validates open view tag/genre.
 * @details If view is tag/genre, checks that value trimmed is not empty.
 * @why Ensures tag/genre views have a non-empty value.
 * @param value The view value.
 * @param view The view type.
 * @return true if tag/genre view is valid, false otherwise.
 */
bool
KodiMedia::openViewValidTagGenre(const QString& value, const QString& view)
{
    if (view != "tag" && view != "genre")
        return true;
    return !value.trimmed().isEmpty();
}
/** Opens show search result.
 * @brief Opens show search result.
 * @details Validates query, TV show ID, and page start, then restores browse to shows/search view.
 * @why Opens show search results for browsing show episodes.
 * @param query The search query.
 * @param tvShowId The TV show ID.
 * @param pageStart The page start index.
 * @return none
 */
void
KodiMedia::openShowResult(const QString& query, int tvShowId, int pageStart)
{
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
/** Plays media by kind and ID.
 * @brief Plays media by kind and ID.
 * @details Validates kind (movie/episode/song/channel) and ID, then initiates playback via Kodi bridge.
 * @why Initiates playback of a specific media item in Kodi.
 * @param kind The media kind (movie, episode, song, channel).
 * @param id The media ID.
 * @param resume Whether to resume playback.
 * @return none
 */
/** Plays media item by kind, ID, and resume flag.
 * @brief Plays media item by kind, ID, and resume flag.
 * @details Validates kind (movie/episode/song/channel) and ID, then initiates playback via Kodi bridge. If busy, queues the play for after library read finishes.
 * @why Initiates playback of a specific media item in Kodi with resume support.
 * @param kind The media kind (movie, episode, song, channel).
 * @param id The media ID.
 * @param resume Whether to resume playback.
 * @return none
 */
void
KodiMedia::playMedia(const QString& kind, int id, bool resume)
{
    if (m_retired || !QStringList{"movie", "episode", "song", "channel"}.contains(kind) || id < 1 || m_playbackActive)
        return;
    if (m_busy)
    {
        if (m_stage != 0 && m_stage != 1)
        {
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
/** Activates media item at index.
 * @brief Activates media item at index.
 * @details Validates index, then plays selected item, activates album/show season, or plays selected depending on view kind.
 * @why Activates a media item for playback from the current browse context.
 * @param index The index of the item to activate.
 * @return none
 */
void
KodiMedia::activate(int index)
{
    if (!activateCanProceed(index))
        return;
    selectIndex(index);
    const auto item = m_movies[index].toMap();
    if (m_viewKind == "playlist" && item.value("mediaType").toString() == "episode")
    {
        playSelected(true);
    }
    else if (m_mediaKind == "albums")
    {
        activateAlbum(item);
    }
    else if (m_mediaKind == "shows" || m_mediaKind == "seasons")
    {
        activateShowSeason(item);
    }
    else
        playSelected(true);
}

/** Checks if activation can proceed at index.
 * @brief Checks if activation can proceed at index.
 * @details Returns true if not busy, connected, not playback active, and index is valid.
 * @why Ensures activation only proceeds when bridge is ready and index is valid.
 * @param index The index to check.
 * @return true if activation can proceed, false otherwise.
 */
bool
KodiMedia::activateCanProceed(int index)
{
    return !m_busy && m_connected && !m_playbackActive && index >= 0 && index < m_movies.size();
}

/** Activates album media item.
 * @brief Activates album media item.
 * @details Restores browse to songs/album view with album ID and title.
 * @why Activates album for browsing songs in the album.
 * @param item The album item map.
 * @return none
 */
void
KodiMedia::activateAlbum(const QVariantMap& item)
{
    const int album = item.value("albumid").toInt();
    if (album < 1)
        return;
    m_history.append(browseState());
    restoreBrowse({"songs", "album", QString::number(album), item.value("title").toString(), -1, -1, 0, 0});
}

/** Activates show season media item.
 * @brief Activates show season media item.
 * @details Restores browse to seasons/episodes view with show ID and season.
 * @why Activates show season for browsing episodes in the season.
 * @param item The item map with show/season info.
 * @return none
 */
void
KodiMedia::activateShowSeason(const QVariantMap& item)
{
    auto next = browseState();
    if (m_mediaKind == "shows")
    {
        if (item.value("tvshowid").toInt() < 1)
            return;
        next.kind = "seasons";
        next.show = item.value("tvshowid").toInt();
        next.showTitle = item.value("title", item.value("label")).toString();
        next.season = -1;
    }
    else
    {
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
}
/** Goes back in browse history.
 * @brief Goes back in browse history.
 * @details Restores last browsed state from history stack.
 * @why Allows user to navigate back to previous browse state.
 * @return true if navigation went back, false otherwise.
 */
bool
KodiMedia::goBack()
{
    if (!canNavigate() || m_history.isEmpty())
        return false;
    m_pendingMovie = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    const auto parent = m_history.takeLast();
    restoreBrowse(parent);
    m_returnSelectionType = parent.selectedType;
    m_returnSelectionId = parent.selectedId;
    return true;
}
/** Plays selected item.
 * @brief Plays selected item.
 * @details Plays the media item at selected index based on media kind and view kind.
 * @why Plays the currently selected media item in Kodi.
 * @param resume Whether to resume playback.
 * @return none
 */
/** Plays selected item.
 * @brief Plays selected item.
 * @details Plays the media item at selected index based on media kind and view kind.
 * @why Plays the currently selected media item in Kodi.
 * @param resume Whether to resume playback.
 * @return none
 */
void
KodiMedia::playSelected(bool resume)
{
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
/** Goes to next page.
 * @brief Goes to next page.
 * @details If not busy, not playback active, and has next page, resets selected index and loads next page.
 * @why Allows user to navigate to next page of browse results.
 * @return none
 */
void
KodiMedia::nextPage()
{
    if (!m_busy && !m_playbackActive && hasNextPage())
    {
        m_selectedIndex = 0;
        loadPage(m_pageStart + 50);
    }
}
/** Goes to previous page.
 * @brief Goes to previous page.
 * @details If not busy, not playback active, connected, and page start > 0, resets selected index and loads previous page.
 * @why Allows user to navigate to previous page of browse results.
 * @return none
 */
void
KodiMedia::previousPage()
{
    if (!m_busy && !m_playbackActive && m_connected && m_pageStart > 0)
    {
        m_selectedIndex = 0;
        loadPage(qMax(0, m_pageStart - 50));
    }
}
/** Loads page at given start index.
 * @brief Loads page at given start index.
 * @details Loads page data for current view kind into browse results.
 * @why Loads a page of media items for browsing.
 * @param start The page start index.
 * @return none
 */
/** Loads page at given start index.
 * @brief Loads page at given start index.
 * @details Loads page data for current view kind into browse results.
 * @why Loads a page of media items for browsing.
 * @param start The page start index.
 * @return none
 */
void
KodiMedia::loadPage(int start)
{
    if (m_viewKind == "manual")
    {
        loadManualPage(start);
        return;
    }
    m_requestedStart = start;
    m_busy = true;
    m_stage = 1;
    m_status = "Loading " + browseTitle() + "…";
    emit changed();
    QJsonArray fields{"title", "year", "plot", "runtime", "resume", "playcount", "art", "uniqueid"};
    if (m_mediaKind == "playlists")
    {
        request("Isolde.GetPlaylists");
        return;
    }
    if (m_viewKind == "playlist")
    {
        request("Isolde.GetPlaylistItems",
                {{"name", m_viewValue}, {"kind", m_mediaKind}, {"start", start}, {"end", start + 50}});
        return;
    }
    QString method;
    loadPageMethodFields(m_mediaKind, method, fields);
    loadPageHistoryFields(m_mediaKind, fields);
    const auto sort = m_sortField.isEmpty() ? loadPageSort(m_mediaKind, m_viewKind) : m_sortField;
    const auto order = m_sortOrder.isEmpty() ? loadPageSortOrder(m_viewKind) : m_sortOrder;
    QJsonObject params{
        {"properties", fields},
        {"sort",
         QJsonObject{{"method", sort},
                     {"order", order},
                     {"ignorearticle", true}}},
        {"limits", QJsonObject{{"start", start}, {"end", start + loadPageLimit(m_mediaKind)}}}};
    loadPageViewFilter(m_viewKind, m_viewValue, params);
    if (!m_watchFilter.isEmpty())
    {
        params.insert("filter", QJsonObject{{"field", "playcount"}, {"operator", "greaterthan"}, {"value", 0}});
    }
    if (!m_facets.isEmpty())
    {
        QJsonObject facetFilters;
        for (const auto& key : m_facets.keys())
        {
            const auto value = m_facets.value(key);
            if (value.canConvert<int>())
                facetFilters.insert(key, value.toInt());
            else
                facetFilters.insert(key, value.toString());
        }
        params.insert("facetfilters", facetFilters);
    }
    loadPageTvShowId(m_mediaKind, m_showId, params);
    loadPageSeason(m_mediaKind, m_season, params);
    loadPageAlbumId(m_mediaKind, m_viewValue, params);
    loadPageChannels(m_mediaKind, params);
    request(method, params);
}

/** Loads page method fields.
 * @brief Loads page method fields.
 * @details Sets method and fields based on media kind for Kodi API request.
 * @param mediaKind The media kind.
 * @param method Output method string.
 * @param fields Output fields array.
 * @return Method string.
 */
QString
KodiMedia::loadPageMethodFields(const QString& mediaKind, QString& method, QJsonArray& fields)
{
    method = "VideoLibrary.GetMovies";
    if (mediaKind == "shows")
    {
        method = "VideoLibrary.GetTVShows";
        fields = {"title", "year", "plot", "episode", "watchedepisodes", "art", "uniqueid"};
    }
    else if (mediaKind == "seasons")
    {
        method = "VideoLibrary.GetSeasons";
        fields = {"season", "showtitle", "episode", "watchedepisodes", "tvshowid", "art"};
    }
    else if (mediaKind == "episodes" || (mediaKind == "history" && m_recentEpisodes))
    {
        method = "VideoLibrary.GetEpisodes";
        fields = {
            "title", "plot", "runtime", "resume", "playcount", "season", "episode", "showtitle", "tvshowid", "art",
            "firstaired"};
    }
    else if (mediaKind == "albums")
    {
        method = "AudioLibrary.GetAlbums";
        fields = {"title", "artist", "year", "art"};
    }
    else if (mediaKind == "songs")
    {
        method = "AudioLibrary.GetSongs";
        fields = {"title", "artist", "album", "albumid", "track", "duration", "art"};
    }
    else if (mediaKind == "channels")
    {
        method = "PVR.GetChannels";
        fields = {"channelnumber", "broadcastnow", "broadcastnext", "locked", "hidden"};
    }
    else if (mediaKind == "musicvideos")
    {
        method = "VideoLibrary.GetMusicVideos";
        fields = {"title", "year", "plot", "runtime", "playcount", "art", "uniqueid"};
    }
    else if (mediaKind == "pictures")
    {
        method = "Files.GetDirectory";
        fields = {"label", "file", "filetype", "size", "date", "time", "iconimage", "plot"};
    }
    return method;
}

/** Loads page history fields.
 * @brief Loads page history fields.
 * @details Appends lastplayed field if media kind is history.
 * @param mediaKind The media kind.
 * @param fields Output fields array.
 * @return none
 */
void
KodiMedia::loadPageHistoryFields(const QString& mediaKind, QJsonArray& fields)
{
    if (mediaKind == "history")
        fields.append("lastplayed");
}

/** Loads page sort method.
 * @brief Loads page sort method.
 * @details Returns sort method based on media kind and view kind.
 * @param mediaKind The media kind.
 * @param viewKind The view kind.
 * @return Sort method string.
 */
QString
KodiMedia::loadPageSort(const QString& mediaKind, const QString& viewKind)
{
    if (mediaKind == "songs")
        return "track";
    if (viewKind == "continue" || viewKind == "watched")
        return "lastplayed";
    if (viewKind == "recent")
        return "dateadded";
    if (mediaKind == "episodes" || mediaKind == "seasons")
        return "episode";
    return "title";
}

/** Loads page sort order.
 * @brief Loads page sort order.
 * @details Returns sort order based on view kind.
 * @param viewKind The view kind.
 * @return Sort order string.
 */
QString
KodiMedia::loadPageSortOrder(const QString& viewKind)
{
    if (viewKind == "continue" || viewKind == "recent" || viewKind == "watched")
        return "descending";
    return "ascending";
}

/** Loads page limit.
 * @brief Loads page limit.
 * @details Returns page limit based on media kind (history limit or 50).
 * @param mediaKind The media kind.
 * @return Page limit integer.
 */
int
KodiMedia::loadPageLimit(const QString& mediaKind)
{
    return mediaKind == "history" ? m_historyLimit : 50;
}

/** Loads page view filter.
 * @brief Loads page view filter.
 * @details Inserts filter parameters based on view kind into params object.
 * @param viewKind The view kind.
 * @param viewValue The view value.
 * @param params Output parameters object.
 * @return none
 */
void
KodiMedia::loadPageViewFilter(const QString& viewKind, const QString& viewValue, QJsonObject& params)
{
    if (viewKind == "continue")
        params.insert("filter", QJsonObject{{"field", "inprogress"}, {"operator", "true"}, {"value", ""}});
    else if (viewKind == "watched")
        params.insert("filter", QJsonObject{{"field", "lastplayed"}, {"operator", "after"}, {"value", "1970-01-01"}});
    else if (viewKind != "all" && viewKind != "recent")
        params.insert("filter",
                      QJsonObject{{"field", viewKind == "search" ? "title" : viewKind},
                                  {"operator", viewKind == "search" ? "contains" : "is"},
                                  {"value", viewValue}});
}

/** Loads page TV show ID parameter.
 * @brief Loads page TV show ID parameter.
 * @details Adds tvshowid parameter for seasons/episodes media kinds.
 * @param mediaKind The media kind.
 * @param showId The TV show ID.
 * @param params Output parameters object.
 * @return none
 */
void
KodiMedia::loadPageTvShowId(const QString& mediaKind, int showId, QJsonObject& params)
{
    if ((mediaKind == "seasons" || mediaKind == "episodes") && showId > 0)
        params["tvshowid"] = showId;
}

/** Loads page season parameter.
 * @brief Loads page season parameter.
 * @details Adds season parameter for episodes media kind.
 * @param mediaKind The media kind.
 * @param season The season number.
 * @param params Output parameters object.
 * @return none
 */
void
KodiMedia::loadPageSeason(const QString& mediaKind, int season, QJsonObject& params)
{
    if (mediaKind == "episodes" && season >= 0)
        params["season"] = season;
}

/** Loads page album ID parameter.
 * @brief Loads page album ID parameter.
 * @details Adds albumid parameter for songs media kind.
 * @param mediaKind The media kind.
 * @param viewValue The view value (album ID).
 * @param params Output parameters object.
 * @return none
 */
void
KodiMedia::loadPageAlbumId(const QString& mediaKind, const QString& viewValue, QJsonObject& params)
{
    if (mediaKind == "songs")
        params.insert("filter", QJsonObject{{"albumid", viewValue.toInt()}});
}

/** Loads page channels parameter.
 * @brief Loads page channels parameter.
 * @details Adds channelgroupid parameter for channels media kind.
 * @param mediaKind The media kind.
 * @param params Output parameters object.
 * @return none
 */
void
KodiMedia::loadPageChannels(const QString& mediaKind, QJsonObject& params)
{
    if (mediaKind == "channels")
    {
        params.remove("sort");
        params.insert("channelgroupid", "alltv");
    }
}
/** Loads manual page.
 * @brief Loads manual page.
 * @details Loads manual page data at given start index.
 * @param start The page start index.
 * @return none
 */
/** Loads manual page.
 * @brief Loads manual page.
 * @details Loads manual page data at given start index.
 * @param start The page start index.
 * @return none
 */
void
KodiMedia::loadManualPage(int start)
{
    if (start < 0 || start % 50 != 0 || start > m_manualMembers.size())
        return;
    m_manualType = m_mediaKind == "shows" ? "tvshow" : "movie";
    m_requestedStart = start;
    m_pageStart = start;
    m_pageCount = static_cast<int>(qMin<qsizetype>(50, m_manualMembers.size() - start));
    m_totalMovies = static_cast<int>(m_manualMembers.size());
    m_manualNextIndex = start;
    m_manualMissingCount = 0;
    m_manualResults.clear();
    m_busy = true;
    m_stage = 8;
    m_status = "Loading manual collection from Kodi…";
    emit changed();
    loadNextManualMovie();
}
/** Loads next manual movie.
 * @brief Loads next manual movie.
 * @details Loads next manual movie data from member list.
 * @return none
 */
void
KodiMedia::loadNextManualMovie()
{
    if (m_manualNextIndex >= m_requestedStart + m_pageCount)
    {
        finishManualPage();
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

/** Finishes manual page loading.
 * @brief Finishes manual page loading.
 * @details Finalizes manual page load, sets movies list, emits changed signal.
 * @return none
 */
void
KodiMedia::finishManualPage()
{
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
    finishManualPendingMovie();
    finishManualPendingShow();
}

/** Finishes manual pending movie.
 * @brief Finishes manual pending movie.
 * @details Plays pending movie if it is in the movies list, otherwise shows error message.
 * @return none
 */
void
KodiMedia::finishManualPendingMovie()
{
    if (m_pendingMovie <= 0)
        return;
    const int movieId = m_pendingMovie;
    m_pendingMovie = 0;
    const auto generation = m_generation;
    QTimer::singleShot(0,
                       this,
                       [this, generation, movieId]
                       {
                           if (generation != m_generation)
                               return;
                           bool listed = false;
                           for (const auto& entry : m_movies)
                               listed |= entry.toMap().value("movieid").toInt() == movieId;
                           if (listed)
                               playMovie(movieId, true);
                           else
                           {
                               m_status = "This movie is no longer in the manual collection page.";
                               emit changed();
                           }
                       });
}

/** Finishes manual pending show.
 * @brief Finishes manual pending show.
 * @details Activates pending show if it is in the movies list, otherwise shows error message.
 * @return none
 */
void
KodiMedia::finishManualPendingShow()
{
    if (m_pendingShow <= 0)
        return;
    const int showId = m_pendingShow;
    m_pendingShow = 0;
    const auto generation = m_generation;
    QTimer::singleShot(0,
                       this,
                       [this, generation, showId]
                       {
                           if (generation != m_generation)
                               return;
                           for (int index = 0; index < m_movies.size(); ++index)
                           {
                               if (m_movies[index].toMap().value("tvshowid").toInt() == showId)
                               {
                                   m_selectedIndex = index;
                                   activate(index);
                                   return;
                               }
                           }
                           m_status = "This show is no longer in the manual collection.";
                           emit changed();
                       });
}
/** Sets view kind and value.
 * @brief Sets view kind and value.
 * @details Validates view, clears pending state, updates view kind/value, restores browse.
 * @param kind The view kind.
 * @param value The view value.
 * @return none
 */
void
KodiMedia::setView(const QString& kind, const QString& value)
{
    if (!setViewCanProceed(kind, value))
        return;
    const auto text = value.trimmed();
    const auto previous = browseState();
    m_pendingMovie = m_pendingShow = 0;
    m_pendingDirectKind.clear();
    m_pendingDirectId = 0;
    ++m_generation;
    m_busy = false;
    releaseSocket();
    m_viewKind = kind == "search" && text.isEmpty() ? "all" : kind;
    m_viewValue = setViewViewValue(kind, text);
    setViewShowsToEpisodes(kind);
    m_selectedIndex = 0;
    refresh();
}

bool
KodiMedia::setViewCanProceed(const QString& kind, const QString& value)
{
    if (m_playbackActive || (m_busy && m_stage != 0 && m_stage != 1))
        return false;
    if (!QStringList{"all", "continue", "recent", "watched", "search", "tag", "genre"}.contains(kind) ||
        value.trimmed().size() > 160)
        return false;
    const auto text = value.trimmed();
    if ((kind == "tag" || kind == "genre") && text.isEmpty())
        return false;
    if ((kind == "tag" || kind == "genre") && !QStringList{"movies", "shows"}.contains(m_mediaKind))
        return false;
    if (m_mediaKind == "seasons")
        return false;
    return true;
}

QString
KodiMedia::setViewViewValue(const QString& kind, const QString& text)
{
    if (kind == "all" || kind == "continue" || kind == "recent" || kind == "watched")
        return QString();
    return text;
}

/** Sets view shows to episodes.
 * @brief Sets view shows to episodes.
 * @details Converts shows media kind to episodes media kind for continue/recent/watched view kinds.
 * @param kind The view kind.
 * @return none
 */
void
KodiMedia::setViewShowsToEpisodes(const QString& kind)
{
    if (m_mediaKind == "movies")
        return;
    if (kind == "continue" || kind == "recent" || kind == "watched")
    {
        if (m_mediaKind == "shows")
        {
            m_history.append(browseState());
            m_mediaKind = "episodes";
            m_showId = m_season = -1;
            m_showTitle.clear();
        }
    }
}

/** Sets the sort field and direction for the current browse view.
 * @brief Sets sort field and direction for browsing.
 * @details Validates the field against the known sort keys and the direction
 * against ascending/descending, then stores them so they are applied to the
 * next Kodi browse request. An empty field clears the sort override and falls
 * back to the default sort for the current view kind.
 * @param field The sort field key (e.g. "title", "year", "dateadded").
 * @param direction "ascending" or "descending".
 */
void
KodiMedia::setSort(const QString& field, const QString& direction)
{
    static const QStringList validFields{"title", "year", "dateadded", "lastplayed", "episode", "track"};
    static const QStringList validDirections{"ascending", "descending"};
    if (field.isEmpty())
    {
        m_sortField.clear();
        m_sortOrder.clear();
        return;
    }
    if (!validFields.contains(field) || !validDirections.contains(direction))
        return;
    m_sortField = field;
    m_sortOrder = direction;
}

/** Enables or disables the watched filter for the current browse view.
 * @brief Sets watched filter state.
 * @details Stores the watched filter flag so it is applied to the next
 * Kodi browse request as a playcount/inprogress filter.
 * @param enabled Whether to filter to watched items only.
 */
void
KodiMedia::setWatchFilter(bool enabled)
{
    m_watchFilter = enabled ? "watched" : QString();
}

/** Sets a facet value for filtering results.
 * @brief Sets a facet filter value.
 * @details Stores a facet key/value pair that is applied to browse requests.
 * An empty value clears the facet.
 * @param facetId The facet identifier.
 * @param value The facet value to filter by.
 */
void
KodiMedia::setFacet(const QString& facetId, const QVariant& value)
{
    if (facetId.isEmpty())
        return;
    if (value.isNull() || (value.toString().isEmpty() && !value.canConvert<int>()))
    {
        m_facets.remove(facetId);
        return;
    }
    m_facets[facetId] = value;
}

QStringList
KodiMedia::sortKeys() const
{
    return QStringList{"title", "year", "dateadded", "lastplayed", "episode", "track"};
}

QStringList
KodiMedia::sortLabels() const
{
    return QStringList{"Title", "Year", "Date added", "Last played", "Episode", "Track"};
}

QString
KodiMedia::sortField() const
{
    return m_sortField;
}

QString
KodiMedia::sortOrder() const
{
    return m_sortOrder;
}

bool
KodiMedia::watchFilter() const
{
    return !m_watchFilter.isEmpty();
}

QVariantMap
KodiMedia::facets() const
{
    return m_facets;
}
/** Plays movie by ID.
 * @brief Plays movie by ID.
 * @details Plays movie with given ID if media kind is movies.
 * @param movieId The movie ID.
 * @param resume Whether to resume playback.
 * @return none
 */
/** Plays movie by ID.
 * @brief Plays movie by ID.
 * @details Plays movie with given ID if media kind is movies.
 * @param movieId The movie ID.
 * @param resume Whether to resume playback.
 * @return none
 */
void
KodiMedia::playMovie(int movieId, bool resume)
{
    if (m_mediaKind != "movies")
        return;
    playLibraryItem("movie", movieId, resume);
}
/** Plays library item by kind, ID, resume flag.
 * @brief Plays library item by kind, ID, resume flag.
 * @details Plays library item via Kodi bridge with optional resume parameter.
 * @param kind The media kind.
 * @param id The media ID.
 * @param resume Whether to resume playback.
 * @param requireListed Whether item must be listed.
 * @return none
 */
/** Plays library item by kind, ID, resume flag.
 * @brief Plays library item by kind, ID, resume flag.
 * @details Plays library item via Kodi bridge with optional resume parameter.
 * @param kind The media kind.
 * @param id The media ID.
 * @param resume Whether to resume playback.
 * @param requireListed Whether item must be listed.
 * @return none
 */
void
KodiMedia::playLibraryItem(const QString& kind, int id, bool resume, bool requireListed)
{
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
/** Plays scheduled item by kind, ID, offset seconds.
 * @brief Plays scheduled item by kind, ID, offset seconds.
 * @details Plays scheduled item via Kodi bridge with offset seconds.
 * @param kind The media kind.
 * @param id The media ID.
 * @param offsetSeconds The offset seconds.
 * @return true if play scheduled successfully, false otherwise.
 */
bool
KodiMedia::playScheduled(const QString& kind, int id, int offsetSeconds)
{
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

/** Marks a movie or episode watched or unwatched via Kodi's library detail write.
 * @brief Marks watched state on a Kodi library item.
 * @details Validates the media kind and ID, then issues VideoLibrary.SetMovieDetails
 * or VideoLibrary.SetEpisodeDetails with the requested playcount through the local
 * bridge. Returns false when the bridge is busy or the identity is invalid; the RPC
 * result is delivered asynchronously through the normal response handler.
 * @why Writes must be validated at the boundary and use Kodi as the authoritative
 * watched-state source, never derived locally from a resume point.
 * @param kind The media kind (movie or episode).
 * @param id The stable Kodi library identity.
 * @param watched Whether to mark the item watched (true) or unwatched (false).
 * @return true when the write request was dispatched, false when it cannot proceed.
 */
bool
KodiMedia::markWatched(const QString& kind, int id, bool watched)
{
    if (m_busy || m_playbackActive || id < 1 || !QStringList{"movie", "episode"}.contains(kind))
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 27;
    m_presentationError.clear();
    m_playbackStatus = watched ? "Marking watched…" : "Marking unwatched…";
    emit changed();
    const QString method = kind == "movie" ? "VideoLibrary.SetMovieDetails" : "VideoLibrary.SetEpisodeDetails";
    QJsonObject params;
    params[kind + "id"] = id;
    QJsonObject details;
    details["playcount"] = watched ? 1 : 0;
    params["details"] = details;
    request(method, params);
    return true;
}

/** Finds the next unwatched media item via an ordered Kodi query.
 * @brief Finds the next unwatched media item.
 * @details Queries Kodi for the next unwatched item of the given kind,
 * ordered by title ascending, filtered by playcount=0 (unwatched).
 * Stores the result so it can be activated via activateNextUnwatched().
 * @param kind The media kind (movie, episode, show).
 * @return true when the next unwatched query was dispatched, false when busy or invalid kind.
 */
bool
KodiMedia::nextUnwatched(const QString& kind)
{
    static const QStringList validKinds{"movie", "episode", "show"};
    if (m_busy || m_playbackActive || !validKinds.contains(kind))
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 44;
    m_presentationError.clear();
    m_playbackStatus = "Finding next unwatched…";
    emit changed();
    m_nextUnwatchedKind = kind;
    m_nextUnwatchedId = 0;
    const QString method = kind == "movie" ? "VideoLibrary.GetMovies"
                                         : (kind == "show" ? "VideoLibrary.GetTVShows"
                                                           : "VideoLibrary.GetEpisodes");
    QJsonObject params;
    params["properties"] = QJsonArray{"title", "year", "plot", "runtime", "resume", "playcount", "art", "uniqueid"};
    params["sort"] = QJsonObject{{"method", "title"}, {"order", "ascending"}, {"ignorearticle", true}};
    params["filter"] = QJsonObject{{"field", "playcount"}, {"operator", "equals"}, {"value", 0}};
    params["limits"] = QJsonObject{{"start", 0}, {"end", 1}};
    request(method, params);
    return true;
}

/** Activates the next unwatched item found by the last nextUnwatched() call.
 * @brief Activates next unwatched item.
 * @details Plays the media item returned by the previous nextUnwatched()
 * query. Returns false if no unwatched item was found or navigation is not possible.
 * @return true when activation was dispatched, false otherwise.
 */
bool
KodiMedia::activateNextUnwatched()
{
    if (m_busy || m_playbackActive || m_nextUnwatchedId < 1 || m_nextUnwatchedKind.isEmpty())
        return false;
    return playMedia(m_nextUnwatchedKind, m_nextUnwatchedId, true);
}

/** Returns the Kodi library identity of the next unwatched item.
 * @brief Returns next unwatched media ID.
 * @return The media ID, or 0 if none found.
 */
int
KodiMedia::nextUnwatchedId() const
{
    return m_nextUnwatchedId;
}

/** Returns the media kind of the next unwatched item.
 * @brief Returns next unwatched media kind.
 * @return The media kind string, or empty if none found.
 */
QString
KodiMedia::nextUnwatchedKind() const
{
    return m_nextUnwatchedKind;
}

/** Returns available versions/editions/extras for a media item.
 * @brief Returns media versions/editions/extras.
 * @details Queries Kodi for version/edition/extras information for the
 * given media item. Returns an empty list when no versions are available.
 * @param kind The media kind (movie, episode, show).
 * @param id The Kodi library identity.
 * @return List of version objects with id, name, type, selected flags.
 */
QVariantList
KodiMedia::mediaVersions(const QString& kind, int id)
{
    static const QStringList validKinds{"movie", "episode", "show"};
    if (m_busy || m_playbackActive || !validKinds.contains(kind) || id < 1)
        return QVariantList();
    ++m_generation;
    m_busy = true;
    m_stage = 45;
    m_presentationError.clear();
    m_playbackStatus = "Loading versions…";
    emit changed();
    m_mediaVersionsKind = kind;
    m_mediaVersionsId = id;
    const QString method = kind == "movie" ? "VideoLibrary.GetMovieDetails"
                                         : (kind == "show" ? "VideoLibrary.GetTVShowDetails"
                                                           : "VideoLibrary.GetEpisodeDetails");
    QJsonObject params;
    params[kind + "id"] = id;
    params["properties"] = QJsonArray{"title", "year", "plot", "runtime", "resume", "playcount", "art",
                                      "uniqueid", "set", "setid", "cast", "art", "fanart"};
    request(method, params);
    return QVariantList();
}

/** Selects a specific version/edition for a media item.
 * @brief Selects media version/edition.
 * @details Selects a version/edition/extras variant for playback.
 * @param kind The media kind (movie, episode, show).
 * @param id The Kodi library identity.
 * @param version The version identifier to select.
 * @return true when the selection was dispatched, false otherwise.
 */
bool
KodiMedia::selectMediaVersion(const QString& kind, int id, const QString& version)
{
    static const QStringList validKinds{"movie", "episode", "show"};
    if (m_busy || m_playbackActive || !validKinds.contains(kind) || id < 1 || version.isEmpty())
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 46;
    m_presentationError.clear();
    m_playbackStatus = "Selecting version…";
    emit changed();
    m_mediaVersionsKind = kind;
    m_mediaVersionsId = id;
    const QString method = kind == "movie" ? "VideoLibrary.SetMovieDetails"
                                         : (kind == "show" ? "VideoLibrary.SetTVShowDetails"
                                                           : "VideoLibrary.SetEpisodeDetails");
    QJsonObject params;
    params[kind + "id"] = id;
    QJsonObject details;
    details["set"] = version;
    params["details"] = details;
    request(method, params);
    return true;
}

/** Edits a single metadata field on a Kodi library item.
 * @brief Edits a metadata field via Kodi's VideoLibrary API.
 * @details Submits an allowlisted typed patch against the selected media
 * identity. Only safe, non-destructive fields may be edited. Retains the
 * prior confirmed view until Kodi acknowledges. Reports errors through
 * errorString().
 * @param kind The media kind (movie, episode, show).
 * @param id The Kodi library identity.
 * @param field The metadata field key (title, plot, etc.).
 * @param value The new field value.
 * @return true when the edit request was dispatched, false when it cannot proceed.
 */
bool
KodiMedia::editMetadata(const QString& kind, int id, const QString& field, const QString& value)
{
    static const QStringList validKinds{"movie", "episode", "show"};
    static const QStringList validFields{"title", "plot", "genre", "year", "rating"};
    if (m_busy || m_playbackActive || id < 1 || !validKinds.contains(kind) ||
        !validFields.contains(field) || value.isEmpty() || value.size() > 1024)
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 28;
    m_pendingEditKind = kind;
    m_pendingEditId = id;
    m_pendingEditField = field;
    m_pendingEditValue = value;
    m_presentationError.clear();
    m_playbackStatus = "Editing " + field + "…";
    emit changed();
    const QString method = kind == "show" ? "VideoLibrary.SetTVShowDetails"
                                          : (kind == "movie" ? "VideoLibrary.SetMovieDetails"
                                                              : "VideoLibrary.SetEpisodeDetails");
    QJsonObject params;
    params[kind + "id"] = id;
    QJsonObject details;
    details[field] = value;
    params["details"] = details;
    request(method, params);
    return true;
}

/** Refreshes metadata for a library item from Kodi.
 * @brief Refreshes metadata from Kodi.
 * @details Re-reads the item's details from Kodi, replacing the local
 * cached view with Kodi's confirmed values. Used after an edit succeeds
 * or when the user explicitly requests a refresh.
 * @param kind The media kind.
 * @param id The Kodi library identity.
 * @return true when the refresh request was dispatched, false otherwise.
 */
bool
KodiMedia::refreshMetadata(const QString& kind, int id)
{
    static const QStringList validKinds{"movie", "episode", "show"};
    if (m_busy || m_playbackActive || id < 1 || !validKinds.contains(kind))
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 29;
    m_pendingEditKind = kind;
    m_pendingEditId = id;
    m_presentationError.clear();
    m_playbackStatus = "Refreshing metadata…";
    emit changed();
    const QString method = kind == "show" ? "VideoLibrary.GetTVShowDetails"
                                          : (kind == "movie" ? "VideoLibrary.GetMovieDetails"
                                                              : "VideoLibrary.GetEpisodeDetails");
    QJsonObject params;
    params[kind + "id"] = id;
    QJsonArray fields{"title", "plot", "genre", "year", "rating", "art", "uniqueid"};
    params["properties"] = fields;
    request(method, params);
    return true;
}

/** Resets metadata for a library item to Kodi's confirmed values.
 * @brief Resets metadata to Kodi's confirmed values.
 * @details Discards any local draft and reloads the item's details from
 * Kodi. Used to abandon an in-progress edit.
 * @param kind The media kind.
 * @param id The Kodi library identity.
 * @return true when the reset request was dispatched, false otherwise.
 */
bool
KodiMedia::resetMetadata(const QString& kind, int id)
{
    return refreshMetadata(kind, id);
}

/** Plays guide stream by URL.
 * @brief Plays guide stream by URL.
 * @details Plays guide stream via Kodi bridge.
 * @param url The stream URL.
 * @return true if play scheduled successfully, false otherwise.
 */
/** Plays guide stream by URL.
 * @brief Plays guide stream by URL.
 * @details Plays guide stream via Kodi bridge.
 * @param url The stream URL.
 * @return true if play scheduled successfully, false otherwise.
 */
bool
KodiMedia::playGuideStream(const QString& url)
{
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
/** Plays PVR item by ID, client ID, unique ID.
 * @brief Plays PVR item by ID, client ID, unique ID.
 * @details Plays PVR item via Kodi bridge.
 * @param id The item ID.
 * @param clientId The client ID.
 * @param uniqueId The unique ID.
 * @return true if play scheduled successfully, false otherwise.
 */
bool
KodiMedia::playPvr(int id, int clientId, int uniqueId)
{
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
/** Handles presentation failure.
 * @brief Handles presentation failure.
 * @details Sets presentation error and playback status, stops playback control.
 * @return none
 */
void
KodiMedia::presentationFailed()
{
    if (!m_playbackActive)
        return;
    m_presentationError =
        "Could not show the video window. Playback was asked to stop. This setup requires "
        "X11/XWayland and one Kodi window.";
    m_playbackStatus = m_presentationError;
    playbackControl("stop");
    emit changed();
}
/** Controls playback.
 * @brief Controls playback.
 * @details Controls playback based on action.
 * @param action The playback control action.
 * @return none
 */
/** Controls playback.
 * @brief Controls playback.
 * @details Controls playback based on action.
 * @param action The playback control action.
 * @return none
 */
void
KodiMedia::playbackControl(const QString& action)
{
    if (action == "home" || action == "stop")
    {
        if (action == "home")
            m_returnHomeRequested = true;
        if (m_adjacentTransition)
        {
            m_adjacentGlobal = m_adjacentAfterRead = -1;
            m_adjacentTransition = false;
            if (!m_playbackActive)
            {
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
    if (m_busy)
    {
        if (action == "stop" || action == "home")
            m_pendingControl = action;
        return;
    }
    m_busy = true;
    m_stage = 7;
    request("Isolde.PlaybackControl", {{"action", action}});
}
// Send one absolute seek after confirmation through the existing control lifecycle.
void
KodiMedia::seekPlayback(int position)
{
    if (!playbackCanSeek() || m_busy || position < 0 || position >= m_playbackTimeline.value("duration").toInt())
        return;
    m_busy = true;
    m_stage = 7;
    request("Isolde.PlaybackControl", {{"action", "seek"}, {"position", position}});
}

/** Checks if adjacent scope is active.
 * @brief Checks if adjacent scope is active.
 * @details Checks if adjacent scope conditions are met for playback control.
 * @return true if adjacent scope is active, false otherwise.
 */
/** Checks if adjacent scope is active.
 * @brief Checks if adjacent scope is active.
 * @details Returns true if playback is active, not scheduled playback, connected, not a manual/history view kind, playing kind is movie/episode/song, and selected index is valid and matches playing item.
 * @why Determines if adjacent navigation is possible within current playback context.
 * @return true if adjacent scope is active, false otherwise.
 */
bool
KodiMedia::adjacentScope() const
{
    if (!m_playbackActive || m_scheduledPlayback || !m_connected ||
        QStringList{"manual", "history"}.contains(m_viewKind) ||
        !QStringList{"movie", "episode", "song"}.contains(m_playingKind) || m_selectedIndex < 0 ||
        m_selectedIndex >= m_movies.size())
        return false;
    const auto item = m_movies[m_selectedIndex].toMap();
    return item.value("mediaType").toString() == m_playingKind && item.value("mediaId").toInt() == m_playingId;
}
/** Checks if playback can go previous.
 * @brief Checks if playback can go previous.
 * @details Returns true if adjacent scope and page start + selected index > 0.
 * @return true if playback can go previous, false otherwise.
 */
bool
KodiMedia::playbackCanPrevious() const
{
    return adjacentScope() && m_pageStart + m_selectedIndex > 0;
}
/** Checks if playback can go next.
 * @brief Checks if playback can go next.
 * @details Returns true if adjacent scope and page start + selected index + 1 < total movies.
 * @return true if playback can go next, false otherwise.
 */
/** Checks if playback can go next.
 * @brief Checks if playback can go next.
 * @details Returns true if adjacent scope and page start + selected index + 1 < total movies.
 * @why Determines if playback can navigate to next item.
 * @return true if playback can go next, false otherwise.
 */
/** Checks if playback can go next.
 * @brief Checks if playback can go next.
 * @details Returns true if adjacent scope and page start + selected index + 1 < total movies.
 * @why Determines if playback can navigate to next item.
 * @return true if playback can go next, false otherwise.
 */
bool
KodiMedia::playbackCanNext() const
{
    return adjacentScope() && m_pageStart + m_selectedIndex + 1 < m_totalMovies;
}
bool
KodiMedia::canPlayAdjacent(int direction) const
{
    return !m_busy && (direction == -1 || direction == 1) &&
           (direction < 0 ? playbackCanPrevious() : playbackCanNext());
}
bool
KodiMedia::playAdjacent(int direction)
{
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
void
KodiMedia::finishAdjacent()
{
    if (m_adjacentAfterRead < 0)
        return;
    const int index = m_adjacentAfterRead - m_pageStart;
    m_adjacentAfterRead = -1;
    if (index < 0 || index >= m_movies.size())
    {
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
    if (!QStringList{"movie", "episode", "song"}.contains(kind) || id < 1)
    {
        m_adjacentTransition = false;
        m_playbackStatus = "Choose a specific playable title from this library view.";
        emit changed();
        emit playbackReturned(false);
        return;
    }
    const auto generation = m_generation;
    QTimer::singleShot(0,
                       this,
                       [this, generation, kind, id]
                       {
                           if (generation == m_generation && m_adjacentTransition && !m_busy && !m_playbackActive)
                               playLibraryItem(kind, id, true);
                       });
}
void
KodiMedia::loadSubtitle(const QString& path)
{
    const QFileInfo subtitle(path);
    if (!m_playbackActive || !subtitle.isFile() ||
        subtitle.suffix().compare(QStringLiteral("srt"), Qt::CaseInsensitive) != 0 || subtitle.size() > 8 * 1024 * 1024)
    {
        m_playbackStatus = QStringLiteral("Load a completed local SRT while an Isolde-managed video is playing.");
        emit changed();
        return;
    }
    if (m_busy)
    {
        m_playbackStatus = QStringLiteral("Kodi is busy. Wait for the current request and retry loading the subtitle.");
        emit changed();
        return;
    }
    m_busy = true;
    m_stage = 13;
    request(QStringLiteral("Isolde.LoadSubtitle"), {{QStringLiteral("path"), subtitle.absoluteFilePath()}});
}
void
KodiMedia::refreshSubtitleTracks()
{
    if (!m_playbackActive || m_busy)
    {
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
void
KodiMedia::refreshPlaybackOptions()
{
    if (!m_playbackActive || m_busy)
        return;
    m_busy = true;
    m_stage = 20;
    m_playbackOptionsStatus = "Reading this video's audio, subtitle and viewing options…";
    emit changed();
    request("Isolde.GetPlaybackOptions");
}
void
KodiMedia::setPlaybackOption(const QString& kind, const QVariant& value)
{
    if (!m_playbackActive || m_busy || !QStringList{"audio", "subtitle", "view"}.contains(kind))
        return;
    m_busy = true;
    m_stage = 21;
    m_playbackOptionsStatus = "Applying the selected option in Kodi…";
    emit changed();
    request("Isolde.SetPlaybackOption", {{"kind", kind}, {"value", QJsonValue::fromVariant(value)}});
}

/** Sets the audio offset (delay) in milliseconds for the current playback.
 * @brief Sets audio offset for playback sync.
 * @details Adjusts audio delay relative to video via Player.SetAudioDelay.
 * Bounded to -2000..2000 ms. Rejects when no video is playing.
 * @param delayMs Audio delay in milliseconds (negative = audio earlier, positive = later).
 */
void
KodiMedia::setAudioOffset(int delayMs)
{
    if (!m_playbackActive || m_busy || delayMs < -2000 || delayMs > 2000)
        return;
    m_busy = true;
    m_stage = 37;
    m_playbackOptionsStatus = "Applying audio offset in Kodi…";
    emit changed();
    request("Isolde.SetPlaybackOption", {{"kind", "audiooffset"}, {"value", delayMs}});
}

/** Returns the current audio offset in milliseconds.
 * @brief Returns audio offset.
 * @return Audio offset in milliseconds, or 0 if not set.
 */
int
KodiMedia::audioOffset() const
{
    return m_audioOffset;
}

/** Sets the playback speed multiplier for the current video playback.
 * @brief Sets playback speed via Player.SetSpeed.
 * @details Adjusts playback speed (0.1x to 8.0x). 1.0 is normal speed.
 * Rejects when no video is playing.
 * @param speed Playback speed multiplier (0.1 to 8.0).
 */
void
KodiMedia::setPlaybackSpeed(double speed)
{
    if (!m_playbackActive || m_busy || speed < 0.1 || speed > 8.0)
        return;
    m_busy = true;
    m_stage = 38;
    m_playbackOptionsStatus = "Applying playback speed in Kodi…";
    emit changed();
    request("Isolde.SetPlaybackOption", {{"kind", "speed"}, {"value", speed}});
}

/** Returns the current playback speed multiplier.
 * @brief Returns playback speed.
 * @return Speed multiplier, or 1.0 if not set.
 */
double
KodiMedia::playbackSpeed() const
{
    return m_playbackSpeed;
}

/** Sets the subtitle delay in milliseconds for the current video playback.
 * @brief Sets subtitle delay via Player.SetSubtitleDelay.
 * @details Adjusts subtitle delay relative to video (-2000..2000 ms). Negative
 * means subtitles appear earlier, positive means later. Rejects when no video
 * is playing.
 * @param delayMs Subtitle delay in milliseconds.
 */
void
KodiMedia::setSubtitleDelay(int delayMs)
{
    if (!m_playbackActive || m_busy || delayMs < -2000 || delayMs > 2000)
        return;
    m_busy = true;
    m_stage = 39;
    m_playbackOptionsStatus = "Applying subtitle delay in Kodi…";
    emit changed();
    request("Isolde.SetPlaybackOption", {{"kind", "subtitleoffset"}, {"value", delayMs}});
}

/** Returns the current subtitle delay in milliseconds.
 * @brief Returns subtitle delay.
 * @return Subtitle delay in milliseconds, or 0 if not set.
 */
int
KodiMedia::subtitleDelay() const
{
    return m_subtitleDelay;
}

/** Sets crossfade for music playback.
 * @brief Sets music crossfade.
 * @details Enables or disables crossfade between music tracks via Kodi settings.
 * @param enabled Whether crossfade is enabled.
 */
void
KodiMedia::setCrossfade(bool enabled)
{
    m_crossfadeEnabled = enabled;
    emit changed();
    request("Isolde.SetMusicOption", {{"kind", "crossfade"}, {"value", enabled}});
}

/** Returns whether crossfade is enabled for music.
 * @brief Returns crossfade state.
 * @return true when crossfade is enabled.
 */
bool
KodiMedia::crossfadeEnabled() const
{
    return m_crossfadeEnabled;
}

/** Sets gapless playback for music.
 * @brief Sets gapless playback.
 * @details Enables or disables gapless playback between music tracks via Kodi settings.
 * @param enabled Whether gapless playback is enabled.
 */
void
KodiMedia::setGapless(bool enabled)
{
    m_gaplessEnabled = enabled;
    emit changed();
    request("Isolde.SetMusicOption", {{"kind", "gapless"}, {"value", enabled}});
}

/** Returns whether gapless playback is enabled for music.
 * @brief Returns gapless state.
 * @return true when gapless playback is enabled.
 */
bool
KodiMedia::gaplessEnabled() const
{
    return m_gaplessEnabled;
}

/** Sets replaygain mode for music playback.
 * @brief Sets replaygain mode.
 * @details Sets the replaygain mode via Kodi settings: off, track, album, or auto.
 * @param mode The replaygain mode string.
 */
void
KodiMedia::setReplayGain(const QString& mode)
{
    if (!QStringList{"off", "track", "album", "auto"}.contains(mode))
        return;
    m_replayGainMode = mode;
    emit changed();
    request("Isolde.SetMusicOption", {{"kind", "replaygain"}, {"value", mode}});
}

/** Returns the current replaygain mode for music.
 * @brief Returns replaygain mode.
 * @return Replaygain mode string: off, track, album, or auto.
 */
QString
KodiMedia::replayGainMode() const
{
    return m_replayGainMode;
}

/** Returns the list of PVR groups/favorites.
 * @brief Returns PVR groups list.
 * @details Lists PVR groups from Kodi's PVR backend via PVR.GetPVRGroups.
 * Returns an empty list when no PVR backend is configured or no groups exist.
 * @return List of group objects with id, name, type, memberCount.
 */
QVariantList
KodiMedia::pvrGroups() const
{
    return m_pvrGroups;
}

/** Returns the members of a PVR group/favorite by ID.
 * @brief Returns PVR group members.
 * @details Lists members of a specific PVR group via PVR.GetGroupMembers.
 * @param id The PVR group ID.
 * @return List of member objects with id, title, type.
 */
QVariantList
KodiMedia::pvrGroupMembers(int id)
{
    return m_pvrGroupMembers;
}

/** Adds an item to a PVR group/favorite.
 * @brief Adds item to PVR group.
 * @details Adds a media item to a PVR group via PVR.AddToGroup.
 * @param groupId The PVR group ID.
 * @param memberId The media item ID to add.
 * @param type The media type (movie, tvshow, episode, song, musicvideo, pvr).
 * @return true when the add request was dispatched, false otherwise.
 */
bool
KodiMedia::addToPVRGroup(int groupId, int memberId, const QString& type)
{
    static const QStringList validTypes{"movie", "tvshow", "episode", "song", "musicvideo", "pvr"};
    if (m_busy || groupId < 1 || memberId < 1 || !validTypes.contains(type))
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 51;
    m_presentationError.clear();
    m_pvrGroupId = groupId;
    m_playbackStatus = "Adding to group…";
    emit changed();
    request("Isolde.AddToGroup", {{"id", groupId}, {"type", type}, {"memberid", memberId}});
    return true;
}

/** Removes an item from a PVR group/favorite.
 * @brief Removes item from PVR group.
 * @details Removes a media item from a PVR group via PVR.RemoveFromGroup.
 * @param groupId The PVR group ID.
 * @param memberId The media item ID to remove.
 * @param type The media type (movie, tvshow, episode, song, musicvideo, pvr).
 * @return true when the remove request was dispatched, false otherwise.
 */
bool
KodiMedia::removeFromPVRGroup(int groupId, int memberId, const QString& type)
{
    static const QStringList validTypes{"movie", "tvshow", "episode", "song", "musicvideo", "pvr"};
    if (m_busy || groupId < 1 || memberId < 1 || !validTypes.contains(type))
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 52;
    m_presentationError.clear();
    m_pvrGroupId = groupId;
    m_playbackStatus = "Removing from group…";
    emit changed();
    request("Isolde.RemoveFromGroup", {{"id", groupId}, {"type", type}, {"memberid", memberId}});
    return true;
}

/** Refreshes the PVR groups list from Kodi.
 * @brief Refreshes PVR groups.
 * @details Fetches PVR groups via Isolde.GetPVRGroups -> PVR.GetPVRGroups.
 * @return true when the refresh request was dispatched, false otherwise.
 */
bool
KodiMedia::refreshPVRGroups()
{
    if (m_busy)
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 53;
    m_pvrGroupsStatus = "Refreshing PVR groups…";
    emit changed();
    request("Isolde.GetPVRGroups", {});
    return true;
}

/** Refreshes the members of a PVR group/favorite by ID.
 * @brief Refreshes PVR group members.
 * @details Fetches group members via Isolde.GetGroupMembers -> PVR.GetGroupMembers.
 * @param id The PVR group ID.
 * @return true when the refresh request was dispatched, false otherwise.
 */
bool
KodiMedia::refreshPVRGroupMembers(int id)
{
    if (m_busy || id < 1)
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 54;
    m_pvrGroupId = id;
    m_pvrGroupMembersStatus = "Refreshing group members…";
    emit changed();
    request("Isolde.GetGroupMembers", {{"id", id}});
    return true;
}

/** Returns the list of playback chapters.
 * @brief Returns playback chapters.
 * @details Lists chapters from Kodi's player via Player.GetProperties.
 * Returns an empty list when no video is playing or no chapters exist.
 * @return List of chapter objects with index, time, name, selected flags.
 */
QVariantList
KodiMedia::chapters() const
{
    return m_chapters;
}

/** Seeks to a playback chapter by index.
 * @brief Seeks to chapter.
 * @details Seeks to the chapter's time position via Player.Seek.
 * Rejects when no video is playing or the index is invalid.
 * @param index The chapter index to seek to.
 * @return true when the chapter seek was dispatched, false otherwise.
 */
bool
KodiMedia::seekChapter(int index)
{
    if (m_busy || m_playbackActive == false || index < 0 || m_chapters.isEmpty() || index >= m_chapters.size())
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 47;
    m_chaptersStatus = "Seeking to chapter…";
    emit changed();
    request("Isolde.SeekChapter", {{"index", index}});
    return true;
}

/** Returns the list of PVR recordings.
 * @brief Returns PVR recordings.
 * @details Lists recordings from Kodi's PVR backend via PVR.GetRecordings.
 * Returns an empty list when no PVR backend is configured or no recordings exist.
 * @return List of recording objects with id, title, channel, startTime, endTime, duration, plot.
 */
QVariantList
KodiMedia::recordings() const
{
    return m_recordings;
}

/** Deletes a PVR recording by ID.
 * @brief Deletes a recording.
 * @details Removes a recording via PVR.DeleteRecorded. Rejects when no
 * recordings exist or the ID is invalid.
 * @param id The recording ID to delete.
 * @return true when the deletion request was dispatched, false otherwise.
 */
bool
KodiMedia::deleteRecording(int id)
{
    if (m_busy || id < 1)
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 40;
    m_recordingsStatus = "Deleting recording…";
    emit changed();
    request("Isolde.DeleteRecording", {{"id", id}});
    return true;
}

/** Returns the list of PVR timers.
 * @brief Returns PVR timers.
 * @details Lists timers from Kodi's PVR backend via PVR.GetTimers.
 * Returns an empty list when no PVR backend is configured or no timers exist.
 * @return List of timer objects with id, title, channel, startTime, endTime, type, state, plot.
 */
QVariantList
KodiMedia::timers() const
{
    return m_timers;
}

/** Adds a PVR timer.
 * @brief Adds a recording timer.
 * @details Creates a timer via PVR.AddTimer.
 * @param title The timer title.
 * @param channel The channel ID to record.
 * @param startTime The start time string.
 * @param endTime The end time string.
 * @return true when the timer creation request was dispatched, false otherwise.
 */
bool
KodiMedia::addTimer(const QString& title, int channel, const QString& startTime, const QString& endTime)
{
    if (m_busy || title.trimmed().isEmpty() || channel < 1 || startTime.trimmed().isEmpty() || endTime.trimmed().isEmpty())
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 41;
    m_timersStatus = "Adding timer…";
    emit changed();
    request("Isolde.AddTimer",
            {{"title", title.trimmed()}, {"channel", channel}, {"startTime", startTime.trimmed()}, {"endTime", endTime.trimmed()}});
    return true;
}

/** Deletes a PVR timer by ID.
 * @brief Deletes a timer.
 * @details Removes a timer via PVR.DeleteTimer. Rejects when the ID is invalid.
 * @param id The timer ID to delete.
 * @return true when the deletion request was dispatched, false otherwise.
 */
bool
KodiMedia::deleteTimer(int id)
{
    if (m_busy || id < 1)
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 42;
    m_timersStatus = "Deleting timer…";
    emit changed();
    request("Isolde.DeleteTimer", {{"id", id}});
    return true;
}

/** Refreshes the PVR timer list from Kodi.
 * @brief Refreshes PVR timers.
 * @details Fetches timers via Isolde.GetTimers → PVR.GetTimers.
 * @return true when the refresh request was dispatched, false otherwise.
 */
bool
KodiMedia::refreshTimers()
{
    if (m_busy)
        return false;
    ++m_generation;
    m_busy = true;
    m_stage = 43;
    m_timersStatus = "Refreshing timers…";
    emit changed();
    request("Isolde.GetTimers", {});
    return true;
}

void
KodiMedia::setSubtitleTrack(int trackId)
{
    if (!m_playbackActive || m_busy || trackId < -1)
    {
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
void
KodiMedia::request(const QString& method, const QJsonObject& params)
{
    if (m_retired)
    {
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
    if (m_descriptor < 0)
    {
        fail("Kodi's local bridge is not set up for this profile yet.");
        return;
    }
    m_timeout.start();
    m_socket.connectToServer(QString("/proc/self/fd/%1/rpc.sock").arg(m_descriptor));
#else
    fail("The local Kodi bridge currently requires Linux.");
#endif
}
