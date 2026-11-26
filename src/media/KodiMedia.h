#pragma once
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <functional>

class KodiMedia final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList movies READ movies NOTIFY changed)
    Q_PROPERTY(QVariantList items READ movies NOTIFY changed)
    Q_PROPERTY(QString mediaKind READ mediaKind NOTIFY changed)
    Q_PROPERTY(QString hierarchyLevel READ hierarchyLevel NOTIFY changed)
    Q_PROPERTY(QString browseTitle READ browseTitle NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE selectIndex NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY changed)
    Q_PROPERTY(QString cleanStatus READ cleanStatus NOTIFY changed)
    Q_PROPERTY(int pageStart READ pageStart NOTIFY changed)
    Q_PROPERTY(int totalMovies READ totalMovies NOTIFY changed)
    Q_PROPERTY(bool hasNextPage READ hasNextPage NOTIFY changed)
    Q_PROPERTY(bool playbackActive READ playbackActive NOTIFY changed)
    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY changed)
    Q_PROPERTY(QString playbackMediaType READ playbackMediaType NOTIFY changed)
    Q_PROPERTY(bool playbackCanPrevious READ playbackCanPrevious NOTIFY changed)
    Q_PROPERTY(bool playbackCanNext READ playbackCanNext NOTIFY changed)
    Q_PROPERTY(QString subtitleStatus READ subtitleStatus NOTIFY changed)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY changed)
    Q_PROPERTY(QVariantMap playbackOptions READ playbackOptions NOTIFY changed)
    Q_PROPERTY(QString playbackOptionsStatus READ playbackOptionsStatus NOTIFY changed)
    Q_PROPERTY(QVariantList librarySources READ librarySources NOTIFY changed)
    Q_PROPERTY(QVariantList sourceScrapers READ sourceScrapers NOTIFY changed)
    Q_PROPERTY(QStringList sourceRoots READ sourceRoots NOTIFY changed)
    Q_PROPERTY(QString sourceStatus READ sourceStatus NOTIFY changed)
    Q_PROPERTY(bool sourcePreviewReady READ sourcePreviewReady NOTIFY changed)
    Q_PROPERTY(bool playbackCanPause READ playbackCanPause NOTIFY changed)
    Q_PROPERTY(bool playbackCanSeek READ playbackCanSeek NOTIFY changed)
    Q_PROPERTY(QVariantMap playbackTimeline READ playbackTimeline NOTIFY changed)
    Q_PROPERTY(bool nextEpisodeOptIn READ nextEpisodeOptIn NOTIFY changed)
    Q_INVOKABLE void
    setNextEpisodeOptIn(bool enabled);
    Q_PROPERTY(QString viewKind READ viewKind NOTIFY changed)
    Q_PROPERTY(QString viewValue READ viewValue NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QString collectionStatus READ collectionStatus NOTIFY changed)
    Q_PROPERTY(QString playlistStatus READ playlistStatus NOTIFY changed)
    Q_PROPERTY(QVariantList playlistPreview READ playlistPreview NOTIFY changed)
    Q_PROPERTY(bool playlistPreviewReady READ playlistPreviewReady NOTIFY changed)
    Q_PROPERTY(QVariantMap smartPlaylist READ smartPlaylist NOTIFY changed)
    Q_PROPERTY(QVariantList playlistMembers READ playlistMembers NOTIFY changed)
  public:
    explicit KodiMedia(const QString& profile, QObject* parent = nullptr, const QString& testDirectory = {});
    ~KodiMedia() override;
    // Identify scoped TV drilldown separately from global episode search and playlists.
    QString hierarchyLevel() const
    {
        if (m_mediaKind == "seasons" && m_showId > 0)
            return QStringLiteral("seasons");
        if (m_mediaKind == "episodes" && m_showId > 0 && m_season >= 0)
            return QStringLiteral("episodes");
        return {};
    }
    QString
    status() const
    {
        return m_status + (m_mediaKind == "history" ? m_historyNotice : QString());
    }
    QVariantList
    movies() const
    {
        return m_movies;
    }
    QVariantList
    items() const
    {
        return m_movies;
    }
    QString
    mediaKind() const
    {
        return m_mediaKind;
    }
    QString
    browseTitle() const;
    QString
    browseTitlePlaylists() const;
    QString
    browseTitlePlaylistView() const;
    QString
    browseTitleHistory() const;
    QString
    browseTitleMusic() const;
    QString
    browseTitleChannels() const;
    QString
    browseTitleManual() const;
    QString
    browseTitleMoviesShows() const;
    QString
    browseTitleSeasons() const;
    QString
    browseTitleEpisodes() const;
    QString
    browseTitleDefault() const;
    int
    selectedIndex() const
    {
        return m_selectedIndex;
    }
    bool
    canGoBack() const
    {
        return !m_history.isEmpty();
    }
    Q_INVOKABLE void
    selectIndex(int index);
    Q_INVOKABLE void
    openLibrary(const QString& kind);
    Q_INVOKABLE void
    savePlaylist(const QString& name);
    /** Saves the current playback queue as a named playlist.
     * @brief Saves the current playback queue as a named playlist.
     * @details Takes the current ordered list of played/queued media items
     * and persists it as a playlist that can later be restored. Rejects if a
     * save is already in progress or the name is invalid.
     * @param name Playlist name (1-80 chars).
     */
    Q_INVOKABLE void
    saveQueue(const QString& name);
    /** Restores a saved playlist as the current playback queue.
     * @brief Restores a saved playlist as the current playback queue.
     * @details Loads playlist members and queues them for playback. Rejects if
     * a restore is already in progress or the playlist cannot be found.
     * @param name Playlist name.
     */
    Q_INVOKABLE void
    restoreQueue(const QString& name);
    /** Adds an entry to a playlist.
     * @brief Adds an entry to a playlist.
     * @details Appends a media item to the playlist's member list. Uses
     * revision-based stale-write protection.
     * @param name Playlist name.
     * @param mediaType Media type ("movie", "episode", "song").
     * @param mediaId Media item ID.
     */
    Q_INVOKABLE void
    addPlaylistEntry(const QString& name, const QString& mediaType, int mediaId);
    /** Removes an entry from a playlist by index.
     * @brief Removes an entry from a playlist by index.
     * @details Removes the member at the given index. Uses revision-based
     * stale-write protection.
     * @param name Playlist name.
     * @param index Zero-based member index.
     */
    Q_INVOKABLE void
    removePlaylistEntry(const QString& name, int index);
    /** Sets shuffle mode for music playback.
     * @brief Sets shuffle mode for music playback.
     * @details Toggles shuffle on/off for the current music queue through
     * Kodi's player. Rejects when no music is playing.
     * @param enabled Whether shuffle is enabled.
     */
    Q_INVOKABLE void
    setShuffle(bool enabled);
    /** Sets repeat mode for music playback.
     * @brief Sets repeat mode for music playback.
     * @details Cycles repeat mode (off / one / all) for the current music
     * queue through Kodi's player. Rejects when no music is playing.
     * @param mode Repeat mode string ("off", "one", "all").
     */
    Q_INVOKABLE void
    setRepeatMode(const QString& mode);
    /** Returns whether shuffle is enabled.
     * @brief Returns shuffle state.
     * @return true when shuffle is enabled.
     */
    Q_INVOKABLE bool
    shuffleEnabled() const;
    /** Returns the current repeat mode.
     * @brief Returns repeat mode.
     * @return Repeat mode string ("off", "one", "all").
     */
    Q_INVOKABLE QString
    repeatMode() const;
    /** Loads lyrics for the current song.
     * @brief Loads lyrics for the current song.
     * @details Requests lyrics from Kodi's lyrics add-on or embedded tags.
     * Returns empty when no lyrics provider is available. Skinnable slot.
     * @return Lyrics text or empty string when unavailable.
     */
    Q_INVOKABLE QString
    loadLyrics();
    /** Returns whether a lyrics provider is available.
     * @brief Returns lyrics availability.
     * @return true when lyrics can be loaded.
     */
    Q_INVOKABLE bool
    lyricsAvailable() const;
    /** Loads visualization for the current song.
     * @brief Loads visualization for the current song.
     * @details Requests the current visualization add-on and state from Kodi.
     * Returns empty when no visualization provider is available. Skinnable slot.
     * @return Visualization name or empty string when unavailable.
     */
    Q_INVOKABLE QString
    loadVisualization();
    /** Returns whether a visualization provider is available.
     * @brief Returns visualization availability.
     * @return true when visualization can be loaded.
     */
    Q_INVOKABLE bool
    visualizationAvailable() const;
    /** Starts a picture slideshow from a source path.
     * @brief Starts a picture slideshow.
     * @details Launches Kodi's slideshow for a picture source path. Rejects
     * when no pictures source is configured or playback is active.
     * @param path Absolute picture source path.
     * @return true when slideshow was requested, false otherwise.
     */
    Q_INVOKABLE bool
    startSlideshow(const QString& path);
    /** Returns whether a slideshow is active.
     * @brief Returns slideshow state.
     * @return true when a slideshow is active.
     */
    Q_INVOKABLE bool
    slideshowActive() const;
    Q_INVOKABLE void loadSmartPlaylist(const QString& name);
    Q_INVOKABLE void previewSmartPlaylist(const QVariantMap& draft);
    Q_INVOKABLE void saveSmartPlaylist(const QString& name, const QVariantMap& draft);
    // Expose loaded rules separately from the preview's matching media rows.
    QVariantMap smartPlaylist() const { return m_smartPlaylist; }
    Q_INVOKABLE void
    renamePlaylist(const QString& name, const QString& newName);
    Q_INVOKABLE void
    deletePlaylist(const QString& name);
    Q_INVOKABLE void
    reorderPlaylist(const QString& name, const QVariantList& order);
    Q_INVOKABLE void
    sortPlaylist(const QString& name, const QString& sort, const QString& direction);
    Q_INVOKABLE void
    editPlaylist(const QString& name, const QString& view, const QString& value);
    Q_INVOKABLE void
    previewPlaylist(const QString& name, const QString& view, const QString& value);
    Q_INVOKABLE void
    loadPlaylistMembers(const QString& name);
    QVariantList
    playlistPreview() const
    {
        return m_playlistPreview;
    }
    bool
    playlistPreviewReady() const
    {
        return m_playlistPreviewReady;
    }
    QVariantList
    playlistMembers() const
    {
        return m_playlistMembers;
    }
    QString
    playlistStatus() const
    {
        return m_playlistStatus;
    }
    void
    openPlaylists();
    static bool
    validPlaylistName(const QString& name);
    bool
    activateCanProceed(int index);
    void
    activateAlbum(const QVariantMap& item);
    void
    activateShowSeason(const QVariantMap& item);
    Q_INVOKABLE void
    openView(const QString& kind, const QString& view, const QString& value = {});
    bool
    openViewValid(const QString& kind, const QString& view, const QString& value);
    bool
    openViewCanNavigate() const;
    bool
    openViewValidKind(const QString& kind);
    bool
    openViewValidView(const QString& view);
    bool
    openViewValidPlaylist(const QString& kind, const QString& view, const QString& value);
    bool
    openViewValidAlbumsChannels(const QString& kind, const QString& view, const QString& value);
    bool
    openViewValidHistory(const QString& kind, const QString& view, const QString& value);
    bool
    openViewValidEpisodes(const QString& kind, const QString& view);
    bool
    openViewValidValueLength(const QString& value);
    bool
    openViewValidTagGenre(const QString& value, const QString& view);
    void
    openHistory(int count);
    void
    setAppOpenings(const QVariantList& items);
    void
    setHistoryNotice(const QString& notice)
    {
        if (m_retired || m_historyNotice == notice)
            return;
        m_historyNotice = notice;
        emit changed();
    }
    // Permanent profile teardown; unlike an unavailable bridge, this cannot retry.
    void
    retire(const QString& reason);
    Q_INVOKABLE void
    openShowResult(const QString& query, int tvShowId, int pageStart = 0);
    Q_INVOKABLE void
    playMedia(const QString& kind, int id, bool resume = true);
    Q_INVOKABLE void
    loadSubtitle(const QString& path);
    Q_INVOKABLE void
    refreshSubtitleTracks();
    Q_INVOKABLE void
    setSubtitleTrack(int trackId);
    // Guide coordinator only: explicit broadcast offset, never saved resume.
    bool
    playScheduled(const QString& kind, int id, int offsetSeconds);
    bool
    markWatched(const QString& kind, int id, bool watched);
    /** Finds the next unwatched media item via an ordered Kodi query.
     * @brief Finds the next unwatched media item.
     * @details Queries Kodi for the next unwatched item of the given kind,
     * ordered by title ascending, filtered by playcount=0 (unwatched).
     * Stores the result so it can be activated via activateNextUnwatched().
     * @param kind The media kind (movie, episode, show).
     * @return true when the next unwatched query was dispatched, false when busy or invalid kind.
     */
    Q_INVOKABLE bool
    nextUnwatched(const QString& kind);
    /** Activates the next unwatched item found by the last nextUnwatched() call.
     * @brief Activates next unwatched item.
     * @details Plays the media item returned by the previous nextUnwatched()
     * query. Returns false if no unwatched item was found or navigation is not possible.
     * @return true when activation was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    activateNextUnwatched();
    /** Returns the Kodi library identity of the next unwatched item.
     * @brief Returns next unwatched media ID.
     * @return The media ID, or 0 if none found.
     */
    Q_INVOKABLE int
    nextUnwatchedId() const;
    /** Returns the media kind of the next unwatched item.
     * @brief Returns next unwatched media kind.
     * @return The media kind string, or empty if none found.
     */
    Q_INVOKABLE QString
    nextUnwatchedKind() const;
    /** Returns available versions/editions/extras for a media item.
     * @brief Returns media versions/editions/extras.
     * @details Queries Kodi for version/edition/extras information for the
     * given media item. Returns an empty list when no versions are available.
     * @param kind The media kind (movie, episode, show).
     * @param id The Kodi library identity.
     * @return List of version objects with id, name, type, selected flags.
     */
    Q_INVOKABLE QVariantList
    mediaVersions(const QString& kind, int id);
    /** Selects a specific version/edition for a media item.
     * @brief Selects media version/edition.
     * @details Selects a version/edition/extras variant for playback.
     * @param kind The media kind (movie, episode, show).
     * @param id The Kodi library identity.
     * @param version The version identifier to select.
     * @return true when the selection was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    selectMediaVersion(const QString& kind, int id, const QString& version);
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
    Q_INVOKABLE bool
    editMetadata(const QString& kind, int id, const QString& field, const QString& value);
    /** Refreshes metadata for a library item from Kodi.
     * @brief Refreshes metadata from Kodi.
     * @details Re-reads the item's details from Kodi, replacing the local
     * cached view with Kodi's confirmed values. Used after an edit succeeds
     * or when the user explicitly requests a refresh.
     * @param kind The media kind.
     * @param id The Kodi library identity.
     * @return true when the refresh request was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    refreshMetadata(const QString& kind, int id);
    /** Resets metadata for a library item to Kodi's confirmed values.
     * @brief Resets metadata to Kodi's confirmed values.
     * @details Discards any local draft and reloads the item's details from
     * Kodi. Used to abandon an in-progress edit.
     * @param kind The media kind.
     * @param id The Kodi library identity.
     * @return true when the reset request was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    resetMetadata(const QString& kind, int id);
    bool
    playPvr(int id, int clientId, int uniqueId);
    bool
    playGuideStream(const QString& url);
    Q_INVOKABLE void
    activate(int index);
    Q_INVOKABLE bool
    goBack();
    Q_INVOKABLE void
    playSelected(bool resume = true);
    bool
    busy() const
    {
        return m_busy;
    }
    bool
    connected() const
    {
        return m_connected;
    }
    QString
    scanStatus() const
    {
        return m_scanStatus;
    }
    QString
    cleanStatus() const
    {
        return m_cleanStatus;
    }
    int
    pageStart() const
    {
        return m_pageStart;
    }
    int
    totalMovies() const
    {
        return m_totalMovies;
    }
    bool
    hasNextPage() const
    {
        return m_connected && m_pageStart + m_pageCount < m_totalMovies;
    }
    Q_INVOKABLE void
    nextPage();
    Q_INVOKABLE void
    previousPage();
    bool
    playbackActive() const
    {
        return m_playbackActive;
    }
    QString
    playbackStatus() const
    {
        return m_playbackStatus;
    }
    QString
    playbackMediaType() const
    {
        return m_playingKind;
    }
    bool
    playbackCanPrevious() const;
    bool
    playbackCanNext() const;
    bool
    canPlayAdjacent(int direction) const;
    Q_INVOKABLE bool
    playAdjacent(int direction);
    QString
    subtitleStatus() const
    {
        return m_subtitleStatus;
    }
    QVariantList
    subtitleTracks() const
    {
        return m_subtitleTracks;
    }
    QVariantMap
    playbackOptions() const
    {
        return m_playbackOptions;
    }
    QString
    playbackOptionsStatus() const
    {
        return m_playbackOptionsStatus;
    }
    Q_INVOKABLE void
    refreshPlaybackOptions();
    Q_INVOKABLE void
    setPlaybackOption(const QString& kind, const QVariant& value);
    /** Sets the audio offset (delay) in milliseconds for the current playback.
     * @brief Sets audio offset for playback sync.
     * @details Adjusts audio delay relative to video via Player.SetAudioDelay.
     * Bounded to -2000..2000 ms. Rejects when no video is playing.
     * @param delayMs Audio delay in milliseconds (negative = audio earlier, positive = later).
     */
    Q_INVOKABLE void
    setAudioOffset(int delayMs);
    /** Returns the current audio offset in milliseconds.
     * @brief Returns audio offset.
     * @return Audio offset in milliseconds, or 0 if not set.
     */
    Q_INVOKABLE int
    audioOffset() const;
    /** Sets the playback speed multiplier for the current video playback.
     * @brief Sets playback speed via Player.SetSpeed.
     * @details Adjusts playback speed (0.1x to 8.0x). 1.0 is normal speed.
     * Rejects when no video is playing.
     * @param speed Playback speed multiplier (0.1 to 8.0).
     */
    Q_INVOKABLE void
    setPlaybackSpeed(double speed);
    /** Returns the current playback speed multiplier.
     * @brief Returns playback speed.
     * @return Speed multiplier, or 1.0 if not set.
     */
    Q_INVOKABLE double
    playbackSpeed() const;
    /** Sets the subtitle delay in milliseconds for the current video playback.
     * @brief Sets subtitle delay via Player.SetSubtitleDelay.
     * @details Adjusts subtitle delay relative to video (-2000..2000 ms). Negative
     * means subtitles appear earlier, positive means later. Rejects when no video
     * is playing.
     * @param delayMs Subtitle delay in milliseconds.
     */
    Q_INVOKABLE void
    setSubtitleDelay(int delayMs);
    /** Returns the current subtitle delay in milliseconds.
     * @brief Returns subtitle delay.
     * @return Subtitle delay in milliseconds, or 0 if not set.
     */
     Q_INVOKABLE int
     subtitleDelay() const;
     /** Sets crossfade for music playback.
      * @brief Sets music crossfade.
      * @details Enables or disables crossfade between music tracks via Kodi settings.
      * @param enabled Whether crossfade is enabled.
      */
     Q_INVOKABLE void
     setCrossfade(bool enabled);
     /** Returns whether crossfade is enabled for music.
      * @brief Returns crossfade state.
      * @return true when crossfade is enabled.
      */
     Q_INVOKABLE bool
     crossfadeEnabled() const;
     /** Sets gapless playback for music.
      * @brief Sets gapless playback.
      * @details Enables or disables gapless playback between music tracks via Kodi settings.
      * @param enabled Whether gapless playback is enabled.
      */
     Q_INVOKABLE void
     setGapless(bool enabled);
     /** Returns whether gapless playback is enabled for music.
      * @brief Returns gapless state.
      * @return true when gapless playback is enabled.
      */
     Q_INVOKABLE bool
     gaplessEnabled() const;
     /** Sets replaygain mode for music playback.
      * @brief Sets replaygain mode.
      * @details Sets the replaygain mode via Kodi settings: off, track, album, or auto.
      * @param mode The replaygain mode string.
      */
     Q_INVOKABLE void
     setReplayGain(const QString& mode);
     /** Returns the current replaygain mode for music.
      * @brief Returns replaygain mode.
      * @return Replaygain mode string: off, track, album, or auto.
      */
     Q_INVOKABLE QString
     replayGainMode() const;
     /** Returns the list of PVR groups/favorites.
      * @brief Returns PVR groups list.
      * @details Lists PVR groups from Kodi's PVR backend via PVR.GetPVRGroups.
      * Returns an empty list when no PVR backend is configured or no groups exist.
      * @return List of group objects with id, name, type, memberCount.
      */
     Q_INVOKABLE QVariantList
     pvrGroups() const;
     /** Returns the members of a PVR group/favorite by ID.
      * @brief Returns PVR group members.
      * @details Lists members of a specific PVR group via PVR.GetGroupMembers.
      * @param id The PVR group ID.
      * @return List of member objects with id, title, type.
      */
     Q_INVOKABLE QVariantList
     pvrGroupMembers(int id);
     /** Adds an item to a PVR group/favorite.
      * @brief Adds item to PVR group.
      * @details Adds a media item to a PVR group via PVR.AddToGroup.
      * @param groupId The PVR group ID.
      * @param memberId The media item ID to add.
      * @param type The media type (movie, tvshow, episode, song, musicvideo, pvr).
      * @return true when the add request was dispatched, false otherwise.
      */
     Q_INVOKABLE bool
     addToPVRGroup(int groupId, int memberId, const QString& type);
     /** Removes an item from a PVR group/favorite.
      * @brief Removes item from PVR group.
      * @details Removes a media item from a PVR group via PVR.RemoveFromGroup.
      * @param groupId The PVR group ID.
      * @param memberId The media item ID to remove.
      * @param type The media type (movie, tvshow, episode, song, musicvideo, pvr).
      * @return true when the remove request was dispatched, false otherwise.
      */
     Q_INVOKABLE bool
     removeFromPVRGroup(int groupId, int memberId, const QString& type);
     /** Refreshes the PVR groups list from Kodi.
      * @brief Refreshes PVR groups.
      * @details Fetches PVR groups via Isolde.GetPVRGroups -> PVR.GetPVRGroups.
      * @return true when the refresh request was dispatched, false otherwise.
      */
     Q_INVOKABLE bool
     refreshPVRGroups();
     /** Refreshes the members of a PVR group/favorite by ID.
      * @brief Refreshes PVR group members.
      * @details Fetches group members via Isolde.GetGroupMembers -> PVR.GetGroupMembers.
      * @param id The PVR group ID.
      * @return true when the refresh request was dispatched, false otherwise.
      */
     Q_INVOKABLE bool
     refreshPVRGroupMembers(int id);
     /** Returns the list of playback chapters.
      * @brief Returns playback chapters.
      * @details Lists chapters from Kodi's player via Player.GetProperties.
      * Returns an empty list when no video is playing or no chapters exist.
      * @return List of chapter objects with index, time, name, selected flags.
      */
     Q_INVOKABLE QVariantList
     chapters() const;
     /** Seeks to a playback chapter by index.
      * @brief Seeks to chapter.
      * @details Seeks to the chapter's time position via Player.Seek.
      * Rejects when no video is playing or the index is invalid.
      * @param index The chapter index to seek to.
      * @return true when the chapter seek was dispatched, false otherwise.
      */
     Q_INVOKABLE bool
     seekChapter(int index);
     /** Returns the list of PVR recordings.
     * @brief Returns PVR recordings.
     * @details Lists recordings from Kodi's PVR backend via PVR.GetRecordings.
     * Returns an empty list when no PVR backend is configured or no recordings exist.
     * @return List of recording objects with id, title, channel, startTime, endTime, duration, plot.
     */
    Q_INVOKABLE QVariantList
    recordings() const;
    /** Deletes a PVR recording by ID.
     * @brief Deletes a recording.
     * @details Removes a recording via PVR.DeleteRecorded. Rejects when no
     * recordings exist or the ID is invalid.
     * @param id The recording ID to delete.
     * @return true when the deletion request was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    deleteRecording(int id);
    /** Returns the list of PVR timers.
     * @brief Returns PVR timers.
     * @details Lists timers from Kodi's PVR backend via PVR.GetTimers.
     * Returns an empty list when no PVR backend is configured or no timers exist.
     * @return List of timer objects with id, title, channel, startTime, endTime, type, state, plot.
     */
    Q_INVOKABLE QVariantList
    timers() const;
    /** Refreshes the PVR timer list from Kodi.
     * @brief Refreshes PVR timers.
     * @details Fetches timers via Isolde.GetTimers → PVR.GetTimers.
     * @return true when the refresh request was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    refreshTimers();
    /** Adds a PVR timer.
     * @brief Adds a recording timer.
     * @details Creates a timer via PVR.AddTimer.
     * @param title The timer title.
     * @param channel The channel ID to record.
     * @param startTime The start time string.
     * @param endTime The end time string.
     * @return true when the timer creation request was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    addTimer(const QString& title, int channel, const QString& startTime, const QString& endTime);
    /** Deletes a PVR timer by ID.
     * @brief Deletes a timer.
     * @details Removes a timer via PVR.DeleteTimer. Rejects when the ID is invalid.
     * @param id The timer ID to delete.
     * @return true when the deletion request was dispatched, false otherwise.
     */
    Q_INVOKABLE bool
    deleteTimer(int id);
    Q_INVOKABLE void
    requestVideoPresentation()
    {
        if (m_playbackActive && m_playbackVisible)
            emit playbackPresentationRequested();
    }
    QVariantList
    librarySources() const
    {
        return m_librarySources;
    }
    QVariantList
    sourceScrapers() const
    {
        return m_sourceScrapers;
    }
    QStringList
    sourceRoots() const
    {
        return m_sourceRoots;
    }
    QString
    sourceStatus() const
    {
        return m_sourceStatus;
    }
    bool
    sourcePreviewReady() const
    {
        return m_sourcePreviewReady;
    }
    void
    setSourceEditGuard(std::function<bool()> guard)
    {
        m_sourceGuard = std::move(guard);
    }
    Q_INVOKABLE void
    refreshSources();
    Q_INVOKABLE void
    previewSource(const QVariantMap& source);
    Q_INVOKABLE void
    applySource(const QVariantMap& source);
    Q_INVOKABLE void
    cancelSourcePreview();
    bool
    playbackCanPause() const
    {
        return m_playbackActive && m_playbackCanPause;
    }
    bool
    playbackCanSeek() const
    {
        return m_playbackActive && m_playbackCanSeek;
    }
    bool
    nextEpisodeOptIn() const
    {
        return m_nextEpisodeOptIn;
    }
    Q_INVOKABLE void
    setNextEpisodeOptIn(bool enabled);
    bool
    lastPlaybackNaturalEnd() const
    {
        return m_lastPlaybackNaturalEnd;
    }
    QString
    viewKind() const
    {
        return m_viewKind;
    }
    QString
    viewValue() const
    {
        return m_viewValue;
    }
    QVariantList
    collections() const;
    QString
    collectionStatus() const
    {
        return m_collectionStatus;
    }
    Q_INVOKABLE void
    setView(const QString& kind, const QString& value = {});
    /** Sets the sort field and direction for the current browse view.
     * @brief Sets sort field and direction for browsing.
     * @details Validates the field against the known sort keys and the direction
     * against ascending/descending, then stores them so they are applied to the
     * next Kodi browse request. An empty field clears the sort override and falls
     * back to the default sort for the current view kind.
     * @why B02 spec item 1: typed query options for sort field/direction must be
     * exposed to Studio and the remote UI so users can reorder results.
     * @param field The sort field key (e.g. "title", "year", "dateadded").
     * @param direction "ascending" or "descending".
     */
    Q_INVOKABLE void
    setSort(const QString& field, const QString& direction);
    /** Enables or disables the watched filter for the current browse view.
     * @brief Sets watched filter state.
     * @details When enabled, Kodi returns only watched items; when disabled, all
     * items are returned. Stored and applied to the next browse request.
     * @why B02 spec item 1: watch filter is a typed query option exposed to the UI.
     * @param enabled Whether to filter to watched items only.
     */
    Q_INVOKABLE void
    setWatchFilter(bool enabled);
    /** Sets a facet value for filtering results.
     * @brief Sets a facet filter value.
     * @details Stores a facet key/value pair that is applied to browse requests.
     * Facets let Studio and the remote UI filter by genre, tag, year, etc.
     * @why B02 spec item 1: supported facets must be exposed as typed query options.
     * @param facetId The facet identifier.
     * @param value The facet value to filter by (empty clears the facet).
     */
    Q_INVOKABLE void
    setFacet(const QString& facetId, const QVariant& value);
    Q_INVOKABLE QStringList
    sortKeys() const;
    Q_INVOKABLE QStringList
    sortLabels() const;
    Q_INVOKABLE QString
    sortField() const;
    Q_INVOKABLE QString
    sortOrder() const;
    Q_INVOKABLE bool
    watchFilter() const;
    Q_INVOKABLE QVariantMap
    facets() const;
    Q_INVOKABLE void
    saveCollection(const QString& name);
    Q_INVOKABLE void
    createManualMovieCollection(const QString& name, int movieId);
    Q_INVOKABLE void
    addMovieToCollection(const QString& collectionId, int movieId);
    Q_INVOKABLE void
    removeMovieFromCollection(const QString& collectionId, int movieId);
    Q_INVOKABLE void
    createManualCollection(const QString& name, const QString& mediaType, int itemId);
    Q_INVOKABLE void
    addManualCollectionMember(const QString& collectionId, const QString& mediaType, int itemId);
    Q_INVOKABLE void
    removeManualCollectionMember(const QString& collectionId, int itemId);
    Q_INVOKABLE void
    renameCollection(const QString& id, const QString& name);
    Q_INVOKABLE void
    removeCollection(const QString& id);
    Q_INVOKABLE void
    openCollection(const QString& id);
    Q_INVOKABLE void
    configureShelf(const QString& id, bool pinned, int count = 6);
    Q_INVOKABLE void
    moveCollection(const QString& id, int offset);
    Q_INVOKABLE void
    playCollectionMovie(const QString& id, int movieId);
    Q_INVOKABLE void
    openCollectionShow(const QString& id, int tvShowId);
    Q_INVOKABLE void
    playMovie(int movieId, bool resume = true);
    Q_INVOKABLE void
    playbackControl(const QString& action);
    // Commit the chosen timestamp through the owned Kodi session.
    Q_INVOKABLE void seekPlayback(int position);
    // Expose the latest Kodi sample without a second polling loop.
    QVariantMap playbackTimeline() const { return m_playbackActive ? m_playbackTimeline : QVariantMap{}; }
    void
    presentationFailed();
    Q_INVOKABLE void
    invalidate(const QString& reason);
    Q_INVOKABLE void
    refresh();
    Q_INVOKABLE void
    scanMovies();
    Q_INVOKABLE void
    scanMusic();
    Q_INVOKABLE void
    clean();
    void
    quitIfIdle();
  signals:
    void
    sourceApplied();
    void
    playbackOptionsRequested();
    void
    playbackPresentationRequested();
    void
    changed();
    void
    collectionsChanged();
    void
    quitReply(bool accepted);
    void
    playbackStarted();
    void
    playbackReturned(bool home);
    void
    scheduledMediaUnavailable(const QString& kind, int id);
    void
    guideSurfRequested(int direction);
    void
    playbackFailed(const QString& reason);
    void
    playlistSaved(const QString& filename);

  private:
    bool
    adjacentScope() const;
    void
    finishAdjacent();
    void
    startScan(const QString& kind);
    void
    request(const QString& method, const QJsonObject& params = {});
    void
    consume();
    bool
    handleErrorResponse(const QJsonObject& response);
    bool
    handleSourceResponse(const QJsonObject& result);
    bool
    validateSourceRevision(const QJsonObject& result);
    bool
    validatePreviewSource(const QJsonObject& result);
    bool
    validateSourceList(const QJsonObject& result);
    bool
    validateSourceRows(const QJsonArray& sources);
    bool
    validateScraperRows(const QJsonArray& scrapers);
    bool
    validateRootRows(const QJsonArray& roots);
    void
    applySourceResult(const QJsonObject& result, bool valid, int stage);
    bool
    handlePlaybackControlError();
    bool
    handlePlaylistOperationError();
    bool
    handleSubtitleLoadError();
    bool
    handleSourceOperationError();
    bool
    handlePlaybackOptionsError();
    bool
    handleSubtitleTracksError();
    bool
    handlePlaylistSaveError();
    bool
    handlePlaylistKindError();
    bool
    handleManualMovieError();
    bool
    handleMarkWatchedResponse(const QJsonObject& result);
    bool
    handleMetadataEditResponse(const QJsonObject& result);
    bool
    handleMetadataRefreshResponse(const QJsonObject& result);
    bool
    handlePlaybackOptionsResponse(const QJsonObject& result);
    bool
    handleRecordingsResponse(const QJsonObject& result);
    bool
    handleDeleteRecordingResponse(const QJsonObject& result);
    bool
    handleTimersResponse(const QJsonObject& result);
    bool
    handleAddTimerResponse(const QJsonObject& result);
    bool
    handleDeleteTimerResponse(const QJsonObject& result);
    bool
    handleNextUnwatchedResponse(const QJsonObject& result);
    bool
    handleMediaVersionsResponse(const QJsonObject& result);
    bool
    handleMediaVersionSelectResponse(const QJsonObject& result);
    bool
    handleGetChaptersResponse(const QJsonObject& result);
    bool
    handleSeekChapterResponse(const QJsonObject& result);
    bool
    handleGetMusicOptionsResponse(const QJsonObject& result);
    bool
    handleSetMusicOptionResponse(const QJsonObject& result);
    bool
    handlePVRGroupsResponse(const QJsonObject& result);
    bool
    handlePVRGroupMembersResponse(const QJsonObject& result);
    bool
    validatePlaybackOptions(const QJsonObject& result);
    bool
    validatePlaybackOptionRows(const QJsonArray& rows, QSet<int>& seen);
    void
    applyPlaybackOptionsResult(const QJsonObject& result, bool valid);
    bool
    handlePlaylistResponse(const QJsonObject& result);
    void
    handlePlaylistSavedResponse(const QJsonObject& result, int stage);
    bool
    validatePlaylistRows(const QJsonArray& rows, int stage);
    void
    applyPlaylistResult(const QJsonObject& result, int stage, bool valid);
    bool
    handlePlaylistSaveResponse(const QJsonObject& result);
    bool
    handlePlaylistRenameResponse(const QJsonObject& result);
    bool
    handlePlaylistDeleteResponse(const QJsonObject& result);
    bool
    handlePlaylistReorderResponse(const QJsonObject& result);
    bool
    handleSubtitleLoadResponse(const QJsonObject& result);
    bool
    handleSubtitleTracksResponse(const QJsonObject& result);
    bool
    handlePlaybackStatusResponse(const QJsonObject& result);
    bool
    validatePlaybackState(const QJsonObject& result);
    void
    applyPlaybackCommonFields(const QJsonObject& result);
    void
    handlePlaybackPlayingState(const QJsonObject& result, const QString& state);
    bool
    handlePlaybackStoppedState(const QJsonObject& result, const QString& state);
    bool
    handleAdjacentTitle(const QJsonObject& result, const QString& state);
    void
    handlePlaybackEnded(const QJsonObject& result, const QString& state);
    void
    handleChannelSurfRequest(const QJsonObject& result);
    void
    flushPendingControl();
    bool
    handleQuitResponse(const QJsonObject& result);
    bool
    handleManualMovieResponse(const QJsonObject& result);
    bool
    handleScanResponse(const QJsonObject& result);
    bool
    handleCleanResponse(const QJsonObject& result);
    bool
    handleVersionResponse(const QJsonObject& result);
    bool
    handleLibraryListResponse(const QJsonObject& result);
    bool
    validateLibraryPaging(const QJsonArray& entries, int total, const QJsonObject& limits);
    bool
    validateAndNormalizeEntries(const QJsonArray& entries, const QString& defaultType,
                                QVariantList& normalized, QSet<QString>& ids);
    QString
    resolveLibraryListKey() const;
    QString
    resolveDefaultType() const;
    QVariantMap
    normalizeLibraryEntry(const QJsonValue& value, const QString& type, const QString& defaultType);
    bool
    processHistoryLibraryResults(QVariantList& normalized);
    QString
    computeLibraryStatus() const;
    void
    handlePendingLibraryActions();
    void
    fail(const QString& message, bool forceDisconnect = false);
    bool
    failSourcePreview(const QString& message, bool forceDisconnect);
    bool
    failPlaybackOptions(const QString& message, bool forceDisconnect);
    bool
    failSubtitle(const QString& message, bool forceDisconnect);
    void
    failPlaylist(const QString& message);
    bool
    canPlayCollectionMovie(const QString& id, int movieId);
    void
    failScan(const QString& message);
    void
    failRestoreHistory();
    bool
    canDeletePlaylist(const QString& name) const;
    bool
    canLoadPlaylistMembers(const QString& name) const;
    bool
    canRenamePlaylist(const QString& name, const QString& newName) const;
    bool
    canPreviewSource() const;
    bool
    validateHistoryCount(int count) const;
    void
    releaseSocket();
    void
    loadPage(int start);
    QString
    loadPageMethodFields(const QString& mediaKind, QString& method, QJsonArray& fields);
    void
    loadPageHistoryFields(const QString& mediaKind, QJsonArray& fields);
    QString
    loadPageSort(const QString& mediaKind, const QString& viewKind);
    QString
    loadPageSortOrder(const QString& viewKind);
    int
    loadPageLimit(const QString& mediaKind);
    void
    loadPageViewFilter(const QString& viewKind, const QString& viewValue, QJsonObject& params);
    void
    loadPageTvShowId(const QString& mediaKind, int showId, QJsonObject& params);
    void
    loadPageSeason(const QString& mediaKind, int season, QJsonObject& params);
    void
    loadPageAlbumId(const QString& mediaKind, const QString& viewValue, QJsonObject& params);
    void
    loadPageChannels(const QString& mediaKind, QJsonObject& params);
    void
    finishManualPage();
    void
    finishManualPendingMovie();
    void
    finishManualPendingShow();
    bool
    setViewCanProceed(const QString& kind, const QString& value);
    QString
    setViewViewValue(const QString& kind, const QString& text);
    void
    setViewShowsToEpisodes(const QString& kind);
    void
    loadManualPage(int start);
    void
    loadNextManualMovie();
    void
    beginRead(int start);
    void
    playLibraryItem(const QString& kind, int id, bool resume, bool requireListed = true);
    bool
    canNavigate() const;
    struct BrowseState
    {
        QString kind, view, value, showTitle;
        int show, season, start, selected;
        QString selectedType{};
        int selectedId = -1;
    };
    BrowseState
    browseState() const;
    void
    restoreBrowse(const BrowseState& state);
    bool
    storeCollections(const QVariantList& entries);
    QString m_profile, m_viewKind = "all", m_viewValue, m_collectionStatus;
    QString m_returnSelectionType;
    int m_returnSelectionId = -1;
    QString m_playlistStatus;
    QVariantList m_playlistPreview, m_playlistMembers;
    QVariantMap m_smartPlaylist;
    QString m_smartPlaylistName, m_smartPlaylistRevision;
    QString m_playlistOrderName, m_playlistRevision;
    QString m_playlistRuleName, m_playlistRuleView, m_playlistRuleValue, m_playlistRuleRevision;
    bool m_playlistPreviewReady = false;
    int m_lastGuideSurfRequest = 0;
    QString m_directory, m_status = "Open Kodi with the Isolde bridge enabled to browse this profile's library.";
    QLocalSocket m_socket;
    QTimer m_timeout;
    QByteArray m_received, m_request;
    QVariantList m_movies;
    QVariantList m_recentResults;
    QVariantList m_appOpenings;
    QString m_historyNotice;
    QString m_historySelection;
    void
    restoreHistorySelection();
    int m_historyLimit = 50;
    bool m_retired = false;
    bool m_recentEpisodes = false;
    int m_descriptor = -1, m_id = 0, m_stage = 0;
    quint64 m_generation = 0;
    bool m_busy = false, m_connected = false;
    QString m_scanStatus;
    QString m_cleanStatus;
    bool m_observedScan = false;
    int m_scanPolls = 0;
    QString m_scanKind = "Video";
    int m_pageStart = 0, m_requestedStart = 0, m_totalMovies = 0, m_pageCount = 0;
    bool m_playbackActive = false, m_playbackVisible = false;
    bool m_playbackCanPause = false, m_playbackCanSeek = false;
    bool m_nextEpisodeOptIn = false;
    bool m_nextLaunchGuard = false;
    QVariantMap m_playbackTimeline;
    bool m_lastPlaybackNaturalEnd = false;
    bool m_scheduledPlayback = false;
    QString m_scheduledKind;
    int m_scheduledId = 0;
    QString m_playbackStatus;
    QString m_playingKind;
    int m_playingId = 0, m_adjacentGlobal = -1, m_adjacentAfterRead = -1;
    bool m_adjacentTransition = false, m_returnHomeRequested = false;
    QString m_subtitleStatus = QStringLiteral("Subtitle track discovery is not active.");
    QVariantList m_subtitleTracks;
    QVariantMap m_playbackOptions;
    QString m_sortField, m_sortOrder, m_watchFilter;
    QVariantMap m_facets;
    QString m_playbackOptionsStatus;
    int m_optionsRequest = 0;
    int m_audioOffset = 0;
    double m_playbackSpeed = 1.0;
    int m_subtitleDelay = 0;
    bool m_crossfadeEnabled = false;
    bool m_gaplessEnabled = false;
    QString m_replayGainMode = "off";
    QVariantList m_pvrGroups;
    QString m_pvrGroupsStatus;
    QVariantList m_pvrGroupMembers;
    QString m_pvrGroupMembersStatus;
    int m_pvrGroupId = 0;
    QVariantList m_recordings;
    QString m_recordingsStatus;
    QVariantList m_timers;
    QString m_timersStatus;
    QVariantList m_chapters;
    QString m_chaptersStatus;
    QVariantList m_librarySources, m_sourceScrapers;
    QStringList m_sourceRoots;
    QString m_sourceStatus, m_sourceRevision;
    QVariantMap m_sourceDraft;
    bool m_sourcePreviewReady = false;
    std::function<bool()> m_sourceGuard;
    QString m_pendingControl;
    QString m_presentationError;
    QTimer m_playbackPoll;
    int m_pendingMovie = 0, m_pendingShow = 0;
    QString m_pendingDirectKind;
    int m_pendingDirectId = 0;
    bool m_pendingDirectResume = true;
    QVariantList m_manualMembers;
    QVariantList m_manualResults;
    int m_manualNextIndex = 0;
    int m_manualCurrentId = 0, m_manualMissingCount = 0;
    QString m_manualType = "movie";
    QString m_mediaKind = "movies", m_showTitle;
    int m_showId = -1, m_season = -1, m_selectedIndex = 0;
    QList<BrowseState> m_history;
    QString m_pendingEditKind, m_pendingEditField, m_pendingEditValue;
    int m_pendingEditId = 0;
    QString m_queueSaveName, m_queueRestoreName, m_queueRevision;
    QVariantList m_queueMembers;
    bool m_queueOperationInProgress = false;
    QString m_addEntryMediaType;
    int m_addEntryMediaId = 0;
    int m_removeEntryIndex = 0;
    bool m_shuffleEnabled = false;
    QString m_repeatMode = "off";
    bool m_lyricsAvailable = false;
    bool m_visualizationAvailable = false;
    bool m_slideshowActive = false;
    QString m_lyricsText;
    QString m_visualizationName;
    int m_nextUnwatchedId = 0;
    QString m_nextUnwatchedKind;
    QVariantList m_mediaVersions;
    QString m_mediaVersionsKind;
    int m_mediaVersionsId = 0;
};
