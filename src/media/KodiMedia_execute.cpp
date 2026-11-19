#include "KodiMedia.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

QVariantList sortedHistory(QVariantList items, int limit);

// Accept source edits only after validating the bounded bridge response and revision.
bool
KodiMedia::handleSourceResponse(const QJsonObject& result)
{
    const int stage = m_stage;
    m_busy = false;
    releaseSocket();
    const auto revision = result.value("revision").toString();
    bool valid = QRegularExpression("^[0-9a-f]{64}$").match(revision).hasMatch();
    if (stage == 23)
    {
        valid = valid && result.value("preview").toBool() &&
                result.value("source").toObject().toVariantMap() == m_sourceDraft;
        if (valid)
        {
            m_sourceRevision = revision;
            m_sourcePreviewReady = true;
            m_sourceStatus = "Preview ready · source/content/scanner settings apply only after confirmation. "
                             "Removing a source preserves media and library titles.";
        }
    }
    else
    {
        const auto sources = result.value("sources");
        const auto scrapers = result.value("scrapers");
        const auto roots = result.value("roots");
        valid = valid && sources.isArray() && sources.toArray().size() <= 128 &&
                scrapers.isArray() && scrapers.toArray().size() <= 384 &&
                roots.isArray() && roots.toArray().size() <= 128;
        for (const auto& value : sources.toArray())
        {
            const auto row = value.toObject();
            valid = valid && row["name"].isString() && row["name"].toString().size() <= 160 &&
                    row["path"].isString() && row["path"].toString().size() <= 4096 && row["editable"].isBool();
        }
        for (const auto& value : scrapers.toArray())
        {
            const auto row = value.toObject();
            valid = valid && row["id"].isString() && row["id"].toString().size() <= 160 &&
                    row["name"].isString() && row["name"].toString().size() <= 160 &&
                    QStringList{"movies", "tvshows", "musicvideos"}.contains(row["content"].toString());
        }
        for (const auto& value : roots.toArray())
            valid = valid && value.isString() && value.toString().startsWith('/') && value.toString().size() <= 4096;
        if (stage == 24)
            valid = valid && result.value("applied").toBool();
        if (valid)
        {
            m_librarySources = sources.toArray().toVariantList();
            m_sourceScrapers = scrapers.toArray().toVariantList();
            m_sourceRoots.clear();
            for (const auto& value : roots.toArray())
                m_sourceRoots.append(value.toString());
            m_sourceStatus = stage == 24
                ? "Kodi source settings saved. Scan the library when ready; files and watched/resume state were not changed."
                : "Choose a source or add an allowed local media folder. Existing remote/multipath sources are preserved.";
        }
    }
    if (!valid)
    {
        m_sourcePreviewReady = false;
        m_sourceStatus = "Invalid source response. Refresh before retrying.";
    }
    if (valid && stage == 24)
        emit sourceApplied();
    emit changed();
    return valid;
}

void
KodiMedia::consume()
{
    if (!m_busy)
        return;
    m_received += m_socket.readAll();
    if (m_received.size() > 4 * 1024 * 1024)
    {
        fail("Kodi returned too much data.");
        return;
    }
    if (!m_received.endsWith('\n'))
        return;
    const auto response = QJsonDocument::fromJson(m_received).object();
    if (response.value("jsonrpc") != "2.0" || response.value("id").toInt(-1) != m_id)
    {
        qWarning() << "Kodi bridge reply rejected: stage" << m_stage << "expected id" << m_id << "received id"
                   << response.value("id").toInt(-1) << "bytes" << m_received.size();
        fail(m_mediaKind == "channels" ? "TV guide unavailable. Configure a PVR TV source in Kodi, then refresh."
                                       : "Kodi returned an incompatible response.");
        return;
    }
    if (response.contains("error"))
    {
        if (!handleErrorResponse(response))
            fail(m_mediaKind == "channels" ? "TV guide unavailable. Configure a PVR TV source in Kodi, then refresh."
                                           : "Kodi returned an incompatible response.");
        return;
    }
    if (m_stage != 2 && m_stage != 4 && !response.value("result").isObject())
    {
        fail("Kodi returned an incompatible response.");
        return;
    }
    const auto result = response.value("result").toObject();
    m_timeout.stop();
    using StageHandler = bool (KodiMedia::*)(const QJsonObject&);
    static const QHash<int, StageHandler> stageHandlers{
        {22, &KodiMedia::handleSourceResponse},
        {23, &KodiMedia::handleSourceResponse},
        {24, &KodiMedia::handleSourceResponse},
        {20, &KodiMedia::handlePlaybackOptionsResponse},
        {21, &KodiMedia::handlePlaybackOptionsResponse},
        {16, &KodiMedia::handlePlaylistResponse},
        {17, &KodiMedia::handlePlaylistResponse},
        {18, &KodiMedia::handlePlaylistResponse},
        {19, &KodiMedia::handlePlaylistResponse},
        {9, &KodiMedia::handlePlaylistSaveResponse},
        {10, &KodiMedia::handlePlaylistRenameResponse},
        {11, &KodiMedia::handlePlaylistDeleteResponse},
        {12, &KodiMedia::handlePlaylistReorderResponse},
        {13, &KodiMedia::handleSubtitleLoadResponse},
        {14, &KodiMedia::handleSubtitleTracksResponse},
        {15, &KodiMedia::handleSubtitleTracksResponse},
        {5, &KodiMedia::handlePlaybackStatusResponse},
        {6, &KodiMedia::handlePlaybackStatusResponse},
        {7, &KodiMedia::handlePlaybackStatusResponse},
        {4, &KodiMedia::handleQuitResponse},
        {8, &KodiMedia::handleManualMovieResponse},
        {3, &KodiMedia::handleScanResponse},
        {2, &KodiMedia::handleScanResponse},
        {0, &KodiMedia::handleVersionResponse},
    };
    auto it = stageHandlers.constFind(m_stage);
    if (it != stageHandlers.cend())
    {
        (this->*it.value())(result);
        return;
    }
    handleLibraryListResponse(result);
}

bool
KodiMedia::handleErrorResponse(const QJsonObject& /*response*/)
{
    static const QSet<int> playbackControlStages{7};
    static const QSet<int> manualMovieStages{8};
    static const QSet<int> subtitleLoadStages{13};
    static const QSet<int> playlistOperationStages{9, 10, 11, 12, 16, 17, 18, 19};
    static const QSet<int> subtitleTracksStages{14, 15};
    static const QSet<int> playbackOptionsStages{20, 21};
    static const QSet<int> sourceOperationStages{22, 23, 24};
    if (playbackControlStages.contains(m_stage))
        return handlePlaybackControlError();
    if (manualMovieStages.contains(m_stage))
        return handleManualMovieError();
    if (subtitleLoadStages.contains(m_stage))
        return handleSubtitleLoadError();
    if (playlistOperationStages.contains(m_stage))
        return handlePlaylistOperationError();
    if (subtitleTracksStages.contains(m_stage))
        return handleSubtitleTracksError();
    if (playbackOptionsStages.contains(m_stage))
        return handlePlaybackOptionsError();
    if (sourceOperationStages.contains(m_stage))
        return handleSourceOperationError();
    if (m_mediaKind == "playlists" || m_viewKind == "playlist")
        return handlePlaylistKindError();
    return false;
}

bool
KodiMedia::handlePlaybackControlError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_playbackStatus =
        "Kodi could not apply that playback control. Playback and the library connection are "
        "retained; retry or stop when ready.";
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaylistOperationError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_playlistStatus =
        "Playlist operation failed. Reload and check the name, supported rules/order and "
        "storage before retrying. No change was confirmed.";
    emit changed();
    return true;
}

bool
KodiMedia::handleSubtitleLoadError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_playbackStatus =
        QStringLiteral("Kodi could not load this local subtitle. Keep your video playing and retry.");
    emit changed();
    return true;
}

bool
KodiMedia::handleSourceOperationError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_sourcePreviewReady = false;
    m_sourceStatus =
        "Source operation unavailable. Stop playback/scanning, check the managed Kodi "
        "installation, refresh and retry. Previous unfinished edits must recover first.";
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaybackOptionsError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_playbackOptionsStatus = "Kodi could not read or change this video's options. Refresh and retry.";
    emit changed();
    return true;
}

bool
KodiMedia::handleSubtitleTracksError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_subtitleStatus = QStringLiteral("Kodi could not read or change the managed video's subtitle tracks.");
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaylistSaveError()
{
    m_busy = false;
    m_timeout.stop();
    releaseSocket();
    m_playlistStatus =
        "Could not save this Kodi playlist. Previous playlists are unchanged. Check storage and retry.";
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaylistKindError()
{
    fail(
        "Kodi video playlist unavailable. Refresh to retry; choose a library playlist of the selected kind. "
        "Catalog limit: 128 saved playlists.");
    return true;
}

bool
KodiMedia::handleManualMovieError()
{
    m_timeout.stop();
    releaseSocket();
    ++m_manualMissingCount;
    const auto generation = m_generation;
    QTimer::singleShot(0,
                       this,
                       [this, generation]
                       {
                           if (generation == m_generation && m_busy && m_stage == 8)
                               loadNextManualMovie();
                       });
    return true;
}

bool
KodiMedia::validateSourceRevision(const QJsonObject& result)
{
    const auto revision = result["revision"].toString();
    return QRegularExpression("^[0-9a-f]{64}$").match(revision).hasMatch();
}

bool
KodiMedia::validatePreviewSource(const QJsonObject& result)
{
    return result["preview"].toBool() && result["source"].toObject().toVariantMap() == m_sourceDraft;
}

bool
KodiMedia::validateSourceList(const QJsonObject& result)
{
    if (!result["sources"].isArray() || !result["scrapers"].isArray() || !result["roots"].isArray())
        return false;
    const auto sources = result["sources"].toArray();
    const auto scrapers = result["scrapers"].toArray();
    const auto roots = result["roots"].toArray();
    if (sources.size() > 128 || scrapers.size() > 384 || roots.size() > 128)
        return false;
    return validateSourceRows(sources) && validateScraperRows(scrapers) && validateRootRows(roots);
}

bool
KodiMedia::validateSourceRows(const QJsonArray& sources)
{
    for (const auto& value : sources)
    {
        const auto row = value.toObject();
        if (!row["name"].isString() || row["name"].toString().size() > 160 || !row["path"].isString() ||
            row["path"].toString().size() > 4096 || !row["editable"].isBool())
            return false;
    }
    return true;
}

bool
KodiMedia::validateScraperRows(const QJsonArray& scrapers)
{
    for (const auto& value : scrapers)
    {
        const auto row = value.toObject();
        if (!row["id"].isString() || row["id"].toString().size() > 160 || !row["name"].isString() ||
            row["name"].toString().size() > 160 ||
            !QStringList{"movies", "tvshows", "musicvideos"}.contains(row["content"].toString()))
            return false;
    }
    return true;
}

bool
KodiMedia::validateRootRows(const QJsonArray& roots)
{
    for (const auto& value : roots)
    {
        if (!value.isString() || !value.toString().startsWith('/') || value.toString().size() > 4096)
            return false;
    }
    return true;
}

void
KodiMedia::applySourceResult(const QJsonObject& result, bool valid, int stage)
{
    const auto revision = result["revision"].toString();
    if (stage == 23 && valid)
    {
        m_sourceRevision = revision;
        m_sourcePreviewReady = true;
        m_sourceStatus =
            "Preview ready · source/content/scanner settings apply only after confirmation. "
            "Removing a source preserves media and library titles.";
    }
    else if (stage != 23 && valid)
    {
        m_librarySources = result["sources"].toArray().toVariantList();
        m_sourceScrapers = result["scrapers"].toArray().toVariantList();
        m_sourceRoots.clear();
        for (const auto& value : result["roots"].toArray())
            m_sourceRoots.append(value.toString());
        m_sourceStatus = stage == 24 ? "Kodi source settings saved. Scan the library when ready; files and "
                                       "watched/resume state were not changed."
                                      : "Choose a source or add an allowed local media folder. Existing "
                                        "remote/multipath sources are preserved.";
    }
    if (!valid)
    {
        m_sourcePreviewReady = false;
        m_sourceStatus = "Invalid source response. Refresh before retrying.";
    }
    if (valid && stage == 24)
        emit sourceApplied();
}

bool
KodiMedia::handlePlaybackOptionsResponse(const QJsonObject& result)
{
    const bool valid = validatePlaybackOptions(result);
    m_busy = false;
    releaseSocket();
    applyPlaybackOptionsResult(result, valid);
    emit changed();
    return true;
}

bool
KodiMedia::validatePlaybackOptions(const QJsonObject& result)
{
    if (!result["audio"].isArray() || !result["subtitles"].isArray() || !result["views"].isArray() ||
        !result["subtitleEnabled"].isBool() || !result["view"].isString())
        return false;
    for (const auto& key : QStringList{"audio", "subtitles"})
    {
        const auto rows = result[key].toArray();
        QSet<int> seen;
        if (rows.size() > 64 || !validatePlaybackOptionRows(rows, seen))
            return false;
    }
    const QStringList allowed{"normal", "zoom", "stretch4:3", "widezoom", "stretch16:9", "original"};
    const auto views = result["views"].toArray();
    if (views.size() > allowed.size())
        return false;
    for (const auto& view : views)
        if (!view.isString() || !allowed.contains(view.toString()))
            return false;
    return true;
}

bool
KodiMedia::validatePlaybackOptionRows(const QJsonArray& rows, QSet<int>& seen)
{
    for (const auto& value : rows)
    {
        const auto row = value.toObject();
        const auto id = row["id"].toInt(-1);
        if (!row["id"].isDouble() || row["id"].toDouble() != id || id < 0 || seen.contains(id) ||
            !row["name"].isString() || row["name"].toString().size() > 160 || !row["language"].isString() ||
            row["language"].toString().size() > 32 || !row["selected"].isBool())
            return false;
        seen.insert(id);
    }
    return true;
}

void
KodiMedia::applyPlaybackOptionsResult(const QJsonObject& result, bool valid)
{
    if (valid)
    {
        m_playbackOptions = result.toVariantMap();
        m_playbackOptionsStatus = "Choose an option. Confirm applies it to the managed Kodi player.";
    }
    else
    {
        m_playbackOptions.clear();
        m_playbackOptionsStatus = "Kodi returned invalid playback options. Refresh to retry.";
    }
}

bool
KodiMedia::handlePlaylistResponse(const QJsonObject& result)
{
    const int stage = m_stage;
    m_busy = false;
    releaseSocket();
    if (stage == 16 || stage == 19)
    {
        handlePlaylistSavedResponse(result, stage);
        emit changed();
        return true;
    }
    const auto rows = result[stage == 17 ? "items" : "members"];
    const auto array = rows.isArray() ? rows.toArray() : QJsonArray{};
    const bool valid = validatePlaylistRows(array, stage) &&
                       (stage != 18 || !array.isEmpty()) &&
                       (stage != 17 ||
                        (result["total"].isDouble() &&
                         result["total"].toDouble() >= double(array.size()) &&
                         result["total"].toDouble() <= 10000000));
    applyPlaylistResult(result, stage, valid);
    emit changed();
    return true;
}

void
KodiMedia::handlePlaylistSavedResponse(const QJsonObject& result, int stage)
{
    const bool ok = result[stage == 16 ? "edited" : "reordered"].toBool();
    m_playlistStatus =
        ok ? "Playlist saved in Kodi." : "Kodi did not confirm the change. Reload before retrying.";
}

bool
KodiMedia::validatePlaylistRows(const QJsonArray& rows, int stage)
{
    const int maxSize = stage == 17 ? 50 : 256;
    if (rows.size() > maxSize)
        return false;
    for (int i = 0; i < rows.size(); ++i)
    {
        const auto row = rows[i].toObject();
        const auto title = row["title"].toString();
        if (title.isEmpty() || title.size() > 160)
            return false;
        if (stage == 17)
        {
            if (!QStringList{"movie", "tvshow", "episode"}.contains(row["mediaType"].toString()) ||
                row["mediaId"].toInt() <= 0)
                return false;
        }
        else
        {
            if (!row["index"].isDouble() || row["index"].toDouble() != i)
                return false;
        }
    }
    return true;
}

void
KodiMedia::applyPlaylistResult(const QJsonObject& result, int stage, bool valid)
{
    const auto array = result[stage == 17 ? "items" : "members"].toArray();
    if (stage == 18)
        valid = valid && QRegularExpression("^[0-9a-f]{64}$").match(result["revision"].toString()).hasMatch();
    if (!valid)
    {
        m_playlistStatus = "Kodi returned invalid playlist data. Reload before retrying.";
        return;
    }
    if (stage == 17)
    {
        m_playlistRuleRevision = result["revision"].toString();
        m_playlistPreviewReady = true;
        m_playlistPreview = array.toVariantList();
        m_playlistStatus =
            QString("Preview · %1 of %2 items · not saved").arg(array.size()).arg(result["total"].toInt());
    }
    else
    {
        m_playlistMembers = array.toVariantList();
        m_playlistRevision = result["revision"].toString();
        m_playlistStatus =
            "Choose an item, move it with Up/Down, then Confirm. Save order applies the complete draft.";
    }
}

bool
KodiMedia::handlePlaylistSaveResponse(const QJsonObject& result)
{
    m_busy = false;
    releaseSocket();
    const auto filename = result["playlistName"].toString();
    if (!result["saved"].toBool() || !validPlaylistName(filename) || !filename.startsWith("Isolde-"))
    {
        m_playlistStatus =
            "Kodi returned an invalid playlist-save acknowledgement. Refresh the playlist list before retrying.";
        emit changed();
        return true;
    }
    m_playlistStatus = "Playlist saved in Kodi · " + result["name"].toString();
    emit changed();
    emit playlistSaved(filename);
    return true;
}

bool
KodiMedia::handlePlaylistRenameResponse(const QJsonObject& result)
{
    m_busy = false;
    releaseSocket();
    if (!result["renamed"].toBool())
    {
        m_playlistStatus = "Kodi did not confirm the playlist rename. Previous playlists are unchanged.";
        emit changed();
        return true;
    }
    m_playlistStatus = "Playlist renamed in Kodi · " + result["newName"].toString();
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaylistDeleteResponse(const QJsonObject& result)
{
    m_busy = false;
    releaseSocket();
    if (!result["deleted"].toBool())
    {
        m_playlistStatus = "Kodi did not confirm the playlist deletion. Previous playlists are unchanged.";
        emit changed();
        return true;
    }
    m_playlistStatus = "Playlist deleted from Kodi · " + result["name"].toString();
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaylistReorderResponse(const QJsonObject& result)
{
    m_busy = false;
    releaseSocket();
    if (!result["reordered"].toBool())
    {
        m_playlistStatus = "Kodi did not confirm the playlist reorder. Previous playlists are unchanged.";
        emit changed();
        return true;
    }
    m_playlistStatus = "Playlist reordered in Kodi · " + result["name"].toString();
    emit changed();
    return true;
}

bool
KodiMedia::handleSubtitleLoadResponse(const QJsonObject& result)
{
    m_busy = false;
    releaseSocket();
    m_playbackStatus = result.value("loaded").toBool()
                           ? QStringLiteral("Subtitle loaded in Kodi.")
                           : QStringLiteral("Kodi did not confirm loading the subtitle.");
    emit changed();
    return true;
}

bool
KodiMedia::handleSubtitleTracksResponse(const QJsonObject& result)
{
    const auto tracks = result.value(QStringLiteral("tracks")).toArray();
    if (tracks.size() > 64 || !result.value(QStringLiteral("enabled")).isBool())
    {
        fail(QStringLiteral("Kodi returned an invalid subtitle track response."));
        return true;
    }
    QVariantList normalized;
    for (const auto& value : tracks)
    {
        if (!value.isObject())
            continue;
        const auto track = value.toObject();
        if (!track.value(QStringLiteral("id")).isDouble() || track.value(QStringLiteral("id")).toInt(-1) < 0)
            continue;
        normalized.append(
            QVariantMap{{QStringLiteral("id"), track.value(QStringLiteral("id")).toInt()},
                        {QStringLiteral("name"), track.value(QStringLiteral("name")).toString()},
                        {QStringLiteral("language"), track.value(QStringLiteral("language")).toString()},
                        {QStringLiteral("external"), track.value(QStringLiteral("external")).toBool()},
                        {QStringLiteral("selected"), track.value(QStringLiteral("selected")).toBool()}});
    }
    m_subtitleTracks = normalized;
    m_busy = false;
    releaseSocket();
    m_subtitleStatus = m_stage == 14
                           ? QStringLiteral("%1 subtitle track(s) discovered from Kodi.").arg(normalized.size())
                           : QStringLiteral("Kodi subtitle selection updated.");
    emit changed();
    return true;
}

bool
KodiMedia::handlePlaybackStatusResponse(const QJsonObject& result)
{
    if (!validatePlaybackState(result))
        return true;
    const auto state = result.value("state").toString();
    m_busy = false;
    releaseSocket();
    applyPlaybackCommonFields(result);
    if (state == "starting" || state == "playing" || state == "paused")
    {
        handlePlaybackPlayingState(result, state);
    }
    else
    {
        if (handlePlaybackStoppedState(result, state))
            return true;
    }
    emit changed();
    handleChannelSurfRequest(result);
    flushPendingControl();
    return true;
}

bool
KodiMedia::validatePlaybackState(const QJsonObject& result)
{
    const auto state = result.value("state").toString();
    if (!QStringList{"starting", "playing", "paused", "ended", "failed", "replaced", "idle"}.contains(state) ||
        !result.value("home").isBool())
    {
        fail("Invalid playback status.");
        return false;
    }
    return true;
}

void
KodiMedia::applyPlaybackCommonFields(const QJsonObject& result)
{
    m_playbackStatus = m_presentationError.isEmpty() ? result.value("state").toString() : m_presentationError;
    m_playbackCanPause = result.value("canPause").isBool() && result.value("canPause").toBool();
    m_playbackCanSeek = result.value("canSeek").isBool() && result.value("canSeek").toBool();
    if (result.value("controlError").isString())
        m_playbackStatus = result.value("controlError").toString().left(512);
    if (result.value("state").toString() == "failed" && result.value("reason").isString())
        m_playbackStatus = result.value("reason").toString().left(512);
}

void
KodiMedia::handlePlaybackPlayingState(const QJsonObject& result, const QString& state)
{
    m_playbackPoll.start();
    if (m_stage == 5 && state == "starting")
        emit playbackPresentationRequested();
    const auto optionRequest = result["optionsRequest"].toInt();
    if (optionRequest > 0 && optionRequest != m_optionsRequest)
    {
        m_optionsRequest = optionRequest;
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, generation]
                           {
                               if (generation == m_generation && m_playbackActive)
                                   emit playbackOptionsRequested();
                           });
    }
    if (state != "starting" && !m_playbackVisible)
    {
        m_adjacentTransition = false;
        m_playbackVisible = true;
        emit playbackStarted();
    }
}

bool
KodiMedia::handlePlaybackStoppedState(const QJsonObject& result, const QString& state)
{
    m_playbackPoll.stop();
    m_playbackActive = m_playbackVisible = false;
    m_subtitleTracks.clear();
    m_lastPlaybackNaturalEnd =
        state == "ended" && result.value("naturalEnd").isBool() && result.value("naturalEnd").toBool();
    if (m_stage == 5 && m_scheduledPlayback && state == "failed" && !result.value("home").toBool() &&
        result.value("availability") == "unavailable")
        emit scheduledMediaUnavailable(m_scheduledKind, m_scheduledId);
    if (state == "failed")
        emit playbackFailed(m_playbackStatus);
    if (handleAdjacentTitle(result, state))
        return true;
    m_adjacentGlobal = m_adjacentAfterRead = -1;
    m_adjacentTransition = false;
    const bool home = result.value("home").toBool() || m_returnHomeRequested;
    m_returnHomeRequested = false;
    emit playbackReturned(home);
    handlePlaybackEnded(result, state);
    return false;
}

bool
KodiMedia::handleAdjacentTitle(const QJsonObject& result, const QString& state)
{
    if (state == "ended" && m_adjacentGlobal >= 0 && !m_returnHomeRequested && !result.value("home").toBool())
    {
        m_adjacentAfterRead = m_adjacentGlobal;
        m_adjacentGlobal = -1;
        m_playbackStatus = "Loading the adjacent title from the same library view…";
        const auto generation = m_generation;
        const int target = m_adjacentAfterRead;
        QTimer::singleShot(0,
                           this,
                           [this, generation, target]
                           {
                               if (generation == m_generation && m_adjacentTransition && !m_busy &&
                                   !m_playbackActive)
                               {
                                   m_selectedIndex = target % 50;
                                   loadPage((target / 50) * 50);
                               }
                           });
        emit changed();
        return true;
    }
    return false;
}

void
KodiMedia::handlePlaybackEnded(const QJsonObject& result, const QString& state)
{
    Q_UNUSED(result);
    if (state == "ended" && !m_scheduledPlayback)
    {
        const auto generation = m_generation;
        QTimer::singleShot(1200,
                           this,
                           [this, generation]
                           {
                               if (generation == m_generation && m_connected && !m_busy && !m_playbackActive)
                                   loadPage(m_pageStart);
                           });
    }
}

void
KodiMedia::handleChannelSurfRequest(const QJsonObject& result)
{
    const auto surfRequest = result.value("channelRequest");
    if (m_playbackActive && surfRequest.isDouble() && surfRequest.toDouble() == surfRequest.toInt() &&
        surfRequest.toInt() > 0 && surfRequest.toInt() != m_lastGuideSurfRequest &&
        result.value("channelDelta").isDouble() &&
        (result.value("channelDelta").toDouble() == 1 || result.value("channelDelta").toDouble() == -1))
    {
        m_lastGuideSurfRequest = surfRequest.toInt();
        const int direction = result.value("channelDelta").toInt();
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, direction, generation]
                           {
                               if (generation == m_generation && m_playbackActive)
                                   emit guideSurfRequested(direction);
                           });
    }
}

void
KodiMedia::flushPendingControl()
{
    if (!m_pendingControl.isEmpty())
    {
        const auto action = m_pendingControl;
        m_pendingControl.clear();
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, action, generation]
                           {
                               if (generation == m_generation)
                                   playbackControl(action);
                           });
    }
}

bool
KodiMedia::handleQuitResponse(const QJsonObject& result)
{
    const bool accepted = result.value("result") == "OK";
    m_busy = false;
    releaseSocket();
    emit quitReply(accepted);
    return true;
}

bool
KodiMedia::handleManualMovieResponse(const QJsonObject& result)
{
    const bool shows = m_manualType == "tvshow";
    const auto item = result.value(shows ? "tvshowdetails" : "moviedetails").toObject();
    const auto idKey = shows ? QString("tvshowid") : QString("movieid");
    if (item.value(idKey).toInt(-1) != m_manualCurrentId || item.value("title").toString().isEmpty())
    {
        m_timeout.stop();
        releaseSocket();
        ++m_manualMissingCount;
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, generation]
                           {
                               if (generation == m_generation && m_busy && m_stage == 8)
                                   loadNextManualMovie();
                           });
        return true;
    }
    auto entry = item.toVariantMap();
    entry["mediaType"] = shows ? "tvshow" : "movie";
    entry["mediaId"] = m_manualCurrentId;
    m_manualResults.append(entry);
    releaseSocket();
    const auto generation = m_generation;
    QTimer::singleShot(0,
                       this,
                       [this, generation]
                       {
                           if (generation == m_generation && m_busy && m_stage == 8)
                               loadNextManualMovie();
                       });
    return true;
}

bool
KodiMedia::handleScanResponse(const QJsonObject& result)
{
    if (m_stage == 2)
    {
        if (result.value("result") != "OK")
        {
            fail("Kodi did not accept the scan.");
            return true;
        }
        m_stage = 3;
        m_scanStatus = "Scan requested. Waiting for Kodi's scan status…";
    }
    else
    {
        const auto scanning = result.value("Library.IsScanning" + m_scanKind);
        if (!scanning.isBool())
        {
            fail("Kodi returned an invalid scan status.");
            return true;
        }
        ++m_scanPolls;
        m_observedScan = m_observedScan || scanning.toBool();
        if ((!scanning.toBool() && (m_observedScan || m_scanPolls >= 5)) || m_scanPolls >= 1800)
        {
            m_scanStatus = scanning.toBool() ? "Kodi is still scanning. Monitoring stopped after 30 minutes."
                               : m_observedScan  ? "Kodi finished scanning. Your files were not moved or renamed."
                                                 : "Kodi accepted the scan and is idle. A short scan may have finished "
                                                   "between checks; new library items are not guaranteed.";
            m_busy = false;
            releaseSocket();
            refresh();
            return true;
        }
        if (scanning.toBool())
            m_scanStatus = m_scanKind == "Music"
                               ? "Kodi is scanning configured music folders and tags. You can keep using Isolde."
                               : "Kodi is scanning configured video folders. You can keep using Isolde.";
    }
    releaseSocket();
    emit changed();
    const auto generation = m_generation;
    QTimer::singleShot(1000,
                       this,
                       [this, generation]
                       {
                           if (m_busy && generation == m_generation)
                               request("XBMC.GetInfoBooleans",
                                       {{"booleans", QJsonArray{"Library.IsScanning" + m_scanKind}}});
                       });
    return true;
}

bool
KodiMedia::handleVersionResponse(const QJsonObject& result)
{
    if (result.value("version").toObject().value("major").toInt() != 13)
    {
        fail("This Kodi API version has not been validated. Open Kodi directly for now.");
        return true;
    }
    m_stage = 1;
    const auto generation = m_generation;
    QTimer::singleShot(0,
                       this,
                       [this, generation]
                       {
                           if (!m_busy || generation != m_generation)
                               return;
                           loadPage(m_requestedStart);
                       });
    return true;
}

bool
KodiMedia::validateLibraryPaging(const QJsonArray& entries, int total, const QJsonObject& limits)
{
    const int maxSize = m_mediaKind == "playlists" ? 128 : m_mediaKind == "history" ? m_historyLimit : 50;
    if (entries.size() > maxSize || total < 0)
        return false;
    if (limits.value("start").toInt(-1) != m_requestedStart)
        return false;
    if (limits.value("end").toInt(-1) != m_requestedStart + entries.size())
        return false;
    if (total < m_requestedStart + entries.size())
        return false;
    if (entries.isEmpty() && total > m_requestedStart)
        return false;
    return true;
}

bool
KodiMedia::validateAndNormalizeEntries(const QJsonArray& entries, const QString& defaultType,
                                       QVariantList& normalized, QSet<QString>& ids)
{
    for (const auto& value : entries)
    {
        auto entry = normalizeLibraryEntry(value, QString(), defaultType);
        if (entry.isEmpty())
            continue;
        const QString type = entry.value("mediaType").toString();
        if (m_viewKind == "playlist" &&
            !(m_mediaKind == "movies" ? QStringList{"movie"} : QStringList{"tvshow", "episode"}).contains(type))
        {
            fail("Kodi returned an invalid playlist media kind.");
            return false;
        }
        if (m_mediaKind == "playlists" && !validPlaylistName(entry.value("playlistName").toString()))
        {
            fail("Kodi returned an invalid playlist name.");
            return false;
        }
        const auto identityKey = m_mediaKind == "seasons" ? "season" : QString(type) + "id";
        const int identity = entry.value(identityKey, -1).toInt();
        const auto identityToken = type + ":" + QString::number(identity);
        if (identity < (m_mediaKind == "seasons" ? 0 : 1) || ids.contains(identityToken))
        {
            fail("Kodi returned invalid or duplicate library identities.");
            return false;
        }
        ids.insert(identityToken);
        normalized.append(entry);
    }
    return true;
}

bool
KodiMedia::handleLibraryListResponse(const QJsonObject& result)
{
    const auto listKey = resolveLibraryListKey();
    const auto limits = result.value("limits").toObject();
    if (!result.value(listKey).isArray() && !(result.value(listKey).isUndefined() && limits.value("total").isDouble() &&
                                              limits.value("total").toInt(-1) == 0))
    {
        fail("Kodi returned an invalid library list.");
        return true;
    }
    const auto entries = result.value(listKey).toArray();
    const int total = limits.value("total").toInt(-1);
    if (entries.isEmpty() && total >= 0 && total < m_requestedStart)
    {
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, generation]
                           {
                               if (m_busy && generation == m_generation)
                                   loadPage(0);
                           });
        return true;
    }
    if (!validateLibraryPaging(entries, total, limits))
    {
        fail("Kodi returned invalid library paging information. Refresh to retry.");
        return true;
    }
    m_pageStart = m_requestedStart;
    m_pageCount = static_cast<int>(entries.size());
    m_totalMovies = total;
    const auto defaultType = resolveDefaultType();
    QVariantList normalized;
    QSet<QString> ids;
    if (!validateAndNormalizeEntries(entries, defaultType, normalized, ids))
        return true;
    if (processHistoryLibraryResults(normalized))
        return true;
    m_movies = normalized;
    restoreHistorySelection();
    m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
    m_status = computeLibraryStatus();
    m_busy = false;
    m_connected = true;
    releaseSocket();
    finishAdjacent();
    emit changed();
    handlePendingLibraryActions();
    return true;
}

QString
KodiMedia::resolveLibraryListKey() const
{
    return m_mediaKind == "history" ? (m_recentEpisodes ? "episodes" : "movies")
            : m_mediaKind == "shows" ? "tvshows"
                                     : m_mediaKind;
}

QString
KodiMedia::resolveDefaultType() const
{
    return m_mediaKind == "playlists"  ? "playlist"
            : m_mediaKind == "channels" ? "channel"
            : m_mediaKind == "albums"   ? "album"
            : m_mediaKind == "songs"    ? "song"
            : m_mediaKind == "history"  ? (m_recentEpisodes ? "episode" : "movie")
            : m_mediaKind == "movies"   ? "movie"
            : m_mediaKind == "shows"    ? "tvshow"
            : m_mediaKind == "seasons"  ? "season"
                                        : "episode";
}

QVariantMap
KodiMedia::normalizeLibraryEntry(const QJsonValue& value, const QString& /*type*/, const QString& defaultType)
{
    auto entry = value.toObject().toVariantMap();
    const QString type =
        m_viewKind == "playlist" ? entry.value("playlistMediaType").toString() : QString(defaultType);
    entry["mediaType"] = type;
    entry["mediaId"] = entry.value(QString(type) + "id", -1);
    if (m_mediaKind == "history")
    {
        const auto date = entry.value("lastplayed").toString();
        const auto parsed = QDateTime::fromString(date, "yyyy-MM-dd HH:mm:ss");
        if (!parsed.isValid() || parsed.toString("yyyy-MM-dd HH:mm:ss") != date || date < "1970-01-02 00:00:00")
            return {};
        entry["viewedAt"] = date;
        entry["providerId"] = "kodi";
    }
    if (entry.value("title").toString().isEmpty())
        entry["title"] = m_mediaKind == "seasons"
                             ? (entry.value("season").toInt() == 0 ? QString("Specials")
                                                                   : "Season " + QString::number(entry.value("season").toInt()))
                             : entry.value("label");
    if (m_mediaKind == "channels")
    {
        if (entry.value("hidden").toBool())
            return {};
        const auto now = entry.value("broadcastnow").toMap(), next = entry.value("broadcastnext").toMap();
        entry["title"] =
            QString::number(entry.value("channelnumber").toInt()) + " · " + entry.value("title").toString();
        entry["subtitle"] =
            now.value("title").toString() +
            (next.value("title").toString().isEmpty() ? QString() : " · Next: " + next.value("title").toString());
        entry["plot"] = now.value("plot");
    }
    return entry;
}

bool
KodiMedia::processHistoryLibraryResults(QVariantList& normalized)
{
    if (m_mediaKind != "history")
        return false;
    m_recentResults.append(normalized);
    if (!m_recentEpisodes)
    {
        m_recentEpisodes = true;
        releaseSocket();
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, generation]
                           {
                               if (m_busy && generation == m_generation)
                                   loadPage(0);
                           });
        return true;
    }
    m_recentResults.append(m_appOpenings);
    normalized = sortedHistory(m_recentResults, m_historyLimit);
    m_recentResults.clear();
    m_pageStart = 0;
    m_pageCount = static_cast<int>(normalized.size());
    m_totalMovies = static_cast<int>(normalized.size());
    return false;
}

QString
KodiMedia::computeLibraryStatus() const
{
    if (m_movies.isEmpty())
    {
        if (m_mediaKind == "channels")
            return "No TV channels in this profile. Open TV Guide to set up a source, then refresh.";
        if (m_mediaKind == "albums")
            return "No albums in this profile. Open Settings, then Media & Playback to add a music source and scan.";
        if (m_mediaKind == "history")
            return "Nothing viewed in this profile yet. Kodi viewing history appears here.";
        if (m_viewKind == "all")
            return "No items in this library view. Open Settings, then Media & Playback to add or scan sources.";
        if (m_viewKind == "continue")
            return "No items with saved progress in this profile.";
        return "Not found in your Kodi library.";
    }
    return QString("%1 · %2–%3 of %4")
        .arg(browseTitle())
        .arg(m_pageStart + 1)
        .arg(m_pageStart + m_movies.size())
        .arg(m_totalMovies);
}

void
KodiMedia::handlePendingLibraryActions()
{
    if (m_pendingMovie > 0)
    {
        const int movie = m_pendingMovie;
        m_pendingMovie = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, generation, movie]
                           {
                               if (generation != m_generation)
                                   return;
                               bool listed = false;
                               for (const auto& entry : m_movies)
                                   listed |= entry.toMap().value("movieid").toInt() == movie;
                               if (listed)
                                   playMovie(movie, true);
                               else
                               {
                                   m_status =
                                       "This movie is no longer in the current collection page. Choose a movie from "
                                       "the refreshed list.";
                                   emit changed();
                               }
                           });
    }
    if (m_pendingShow > 0)
    {
        const int tvShowId = m_pendingShow;
        m_pendingShow = 0;
        const auto generation = m_generation;
        QTimer::singleShot(
            0,
            this,
            [this, generation, tvShowId]
            {
                if (generation != m_generation)
                    return;
                for (int index = 0; index < m_movies.size(); ++index)
                {
                    if (m_movies[index].toMap().value("tvshowid").toInt() == tvShowId)
                    {
                        m_selectedIndex = index;
                        activate(index);
                        return;
                    }
                }
                m_status = "This show is no longer in the selected collection. Choose a show from the refreshed list.";
                emit changed();
            });
    }
    if (!m_pendingDirectKind.isEmpty())
    {
        const auto kind = m_pendingDirectKind;
        const auto id = m_pendingDirectId;
        const auto resume = m_pendingDirectResume;
        m_pendingDirectKind.clear();
        m_pendingDirectId = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0,
                           this,
                           [this, generation, kind, id, resume]
                           {
                               if (generation == m_generation && !m_busy && !m_playbackActive)
                                   playMedia(kind, id, resume);
                           });
    }
}
