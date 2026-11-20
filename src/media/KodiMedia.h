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
    Q_PROPERTY(QString browseTitle READ browseTitle NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE selectIndex NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY changed)
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
    Q_PROPERTY(QString viewKind READ viewKind NOTIFY changed)
    Q_PROPERTY(QString viewValue READ viewValue NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QString collectionStatus READ collectionStatus NOTIFY changed)
    Q_PROPERTY(QString playlistStatus READ playlistStatus NOTIFY changed)
    Q_PROPERTY(QVariantList playlistPreview READ playlistPreview NOTIFY changed)
    Q_PROPERTY(bool playlistPreviewReady READ playlistPreviewReady NOTIFY changed)
    Q_PROPERTY(QVariantList playlistMembers READ playlistMembers NOTIFY changed)
  public:
    explicit KodiMedia(const QString& profile, QObject* parent = nullptr, const QString& testDirectory = {});
    ~KodiMedia() override;
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
    handlePlaybackOptionsResponse(const QJsonObject& result);
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
    };
    BrowseState
    browseState() const;
    void
    restoreBrowse(const BrowseState& state);
    bool
    storeCollections(const QVariantList& entries);
    QString m_profile, m_viewKind = "all", m_viewValue, m_collectionStatus;
    QString m_playlistStatus;
    QVariantList m_playlistPreview, m_playlistMembers;
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
    bool m_observedScan = false;
    int m_scanPolls = 0;
    QString m_scanKind = "Video";
    int m_pageStart = 0, m_requestedStart = 0, m_totalMovies = 0, m_pageCount = 0;
    bool m_playbackActive = false, m_playbackVisible = false;
    bool m_playbackCanPause = false, m_playbackCanSeek = false;
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
    QString m_playbackOptionsStatus;
    int m_optionsRequest = 0;
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
};
