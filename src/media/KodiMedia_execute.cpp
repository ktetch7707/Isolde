#include "KodiMedia.h"

void KodiMedia::consume() {
    if (!m_busy)
        return;
    m_received += m_socket.readAll();
    if (m_received.size() > 4 * 1024 * 1024) {
        fail("Kodi returned too much data.");
        return;
    }
    if (!m_received.endsWith('\n'))
        return;
    const auto response = QJsonDocument::fromJson(m_received).object();
    if (response.value("jsonrpc") != "2.0" || response.value("id").toInt(-1) != m_id) {
        qWarning() << "Kodi bridge reply rejected: stage" << m_stage << "expected id" << m_id << "received id"
                   << response.value("id").toInt(-1) << "bytes" << m_received.size();
        fail(m_mediaKind == "channels" ? "TV guide unavailable. Configure a PVR TV source in Kodi, then refresh."
                                       : "Kodi returned an incompatible response.");
        return;
    }
    if (response.contains("error")) {
        if (m_stage == 7) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_playbackStatus = "Kodi could not apply that playback control. Playback and the library connection are "
                               "retained; retry or stop when ready.";
            emit changed();
            return;
        }
        if ((m_stage >= 9 && m_stage <= 12) || (m_stage >= 16 && m_stage <= 19)) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_playlistStatus = "Playlist operation failed. Reload and check the name, supported rules/order and "
                               "storage before retrying. No change was confirmed.";
            emit changed();
            return;
        }
        if (m_stage == 13) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_playbackStatus =
                QStringLiteral("Kodi could not load this local subtitle. Keep your video playing and retry.");
            emit changed();
            return;
        }
        if (m_stage >= 22 && m_stage <= 24) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_sourcePreviewReady = false;
            m_sourceStatus = "Source operation unavailable. Stop playback/scanning, check the managed Kodi "
                             "installation, refresh and retry. Previous unfinished edits must recover first.";
            emit changed();
            return;
        }
        if (m_stage == 20 || m_stage == 21) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_playbackOptionsStatus = "Kodi could not read or change this video's options. Refresh and retry.";
            emit changed();
            return;
        }
        if (m_stage == 14 || m_stage == 15) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_subtitleStatus = QStringLiteral("Kodi could not read or change the managed video's subtitle tracks.");
            emit changed();
            return;
        }
        if (m_stage == 9) {
            m_busy = false;
            m_timeout.stop();
            releaseSocket();
            m_playlistStatus =
                "Could not save this Kodi playlist. Previous playlists are unchanged. Check storage and retry.";
            emit changed();
            return;
        }
        if (m_mediaKind == "playlists" || m_viewKind == "playlist") {
            fail("Kodi video playlist unavailable. Refresh to retry; choose a library playlist of the selected kind. "
                 "Catalog limit: 128 saved playlists.");
            return;
        }
        if (m_stage == 8) {
            m_timeout.stop();
            releaseSocket();
            ++m_manualMissingCount;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_generation && m_busy && m_stage == 8)
                    loadNextManualMovie();
            });
            return;
        }
        fail(m_mediaKind == "channels" ? "TV guide unavailable. Configure a PVR TV source in Kodi, then refresh."
                                       : "Kodi returned an incompatible response.");
        return;
    }
    if (m_stage != 2 && m_stage != 4 && !response.value("result").isObject()) {
        fail("Kodi returned an incompatible response.");
        return;
    }
    const auto result = response.value("result").toObject();
    m_timeout.stop();
    if (m_stage >= 22 && m_stage <= 24) {
        const auto stage = m_stage;
        m_busy = false;
        releaseSocket();
        const auto revision = result["revision"].toString();
        bool valid = QRegularExpression("^[0-9a-f]{64}$").match(revision).hasMatch();
        if (stage == 23) {
            valid = valid && result["preview"].toBool() && result["source"].toObject().toVariantMap() == m_sourceDraft;
            if (valid) {
                m_sourceRevision = revision;
                m_sourcePreviewReady = true;
                m_sourceStatus = "Preview ready · source/content/scanner settings apply only after confirmation. "
                                 "Removing a source preserves media and library titles.";
            }
        } else {
            valid = valid && result["sources"].isArray() && result["sources"].toArray().size() <= 128 &&
                    result["scrapers"].isArray() && result["scrapers"].toArray().size() <= 384 &&
                    result["roots"].isArray() && result["roots"].toArray().size() <= 128;
            for (const auto& value : result["sources"].toArray()) {
                const auto row = value.toObject();
                valid = valid && row["name"].isString() && row["name"].toString().size() <= 160 &&
                        row["path"].isString() && row["path"].toString().size() <= 4096 && row["editable"].isBool();
            }
            for (const auto& value : result["scrapers"].toArray()) {
                const auto row = value.toObject();
                valid = valid && row["id"].isString() && row["id"].toString().size() <= 160 && row["name"].isString() &&
                        row["name"].toString().size() <= 160 &&
                        QStringList{"movies", "tvshows", "musicvideos"}.contains(row["content"].toString());
            }
            for (const auto& value : result["roots"].toArray())
                valid =
                    valid && value.isString() && value.toString().startsWith('/') && value.toString().size() <= 4096;
            if (stage == 24)
                valid = valid && result["applied"].toBool();
            if (valid) {
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
        }
        if (!valid) {
            m_sourcePreviewReady = false;
            m_sourceStatus = "Invalid source response. Refresh before retrying.";
        }
        if (valid && stage == 24)
            emit sourceApplied();
        emit changed();
        return;
    }
    if (m_stage == 20 || m_stage == 21) {
        bool valid = result["audio"].isArray() && result["subtitles"].isArray() && result["views"].isArray() &&
                     result["subtitleEnabled"].isBool() && result["view"].isString();
        for (const auto& key : QStringList{"audio", "subtitles"}) {
            const auto rows = result[key].toArray();
            QSet<int> seen;
            valid = valid && rows.size() <= 64;
            for (const auto& value : rows) {
                const auto row = value.toObject();
                const auto id = row["id"].toInt(-1);
                valid = valid && row["id"].isDouble() && row["id"].toDouble() == id && id >= 0 && !seen.contains(id) &&
                        row["name"].isString() && row["name"].toString().size() <= 160 && row["language"].isString() &&
                        row["language"].toString().size() <= 32 && row["selected"].isBool();
                seen.insert(id);
            }
        }
        const QStringList allowed{"normal", "zoom", "stretch4:3", "widezoom", "stretch16:9", "original"};
        valid = valid && result["views"].toArray().size() <= allowed.size();
        for (const auto& view : result["views"].toArray())
            valid = valid && view.isString() && allowed.contains(view.toString());
        m_busy = false;
        releaseSocket();
        if (valid) {
            m_playbackOptions = result.toVariantMap();
            m_playbackOptionsStatus = "Choose an option. Confirm applies it to the managed Kodi player.";
        } else {
            m_playbackOptions.clear();
            m_playbackOptionsStatus = "Kodi returned invalid playback options. Refresh to retry.";
        }
        emit changed();
        return;
    }
    if (m_stage >= 16 && m_stage <= 19) {
        const int stage = m_stage;
        m_busy = false;
        releaseSocket();
        if (stage == 16 || stage == 19) {
            const bool ok = result[stage == 16 ? "edited" : "reordered"].toBool();
            m_playlistStatus =
                ok ? "Playlist saved in Kodi." : "Kodi did not confirm the change. Reload before retrying.";
        } else {
            const auto rows = result[stage == 17 ? "items" : "members"];
            bool valid = rows.isArray() && rows.toArray().size() <= (stage == 17 ? 50 : 256);
            const auto array = rows.toArray();
            for (int i = 0; i < array.size(); ++i) {
                const auto row = array[i].toObject();
                const auto title = row["title"].toString();
                valid =
                    valid && !title.isEmpty() && title.size() <= 160 &&
                    (stage == 17 ? QStringList{"movie", "tvshow", "episode"}.contains(row["mediaType"].toString()) &&
                                       row["mediaId"].toInt() > 0
                                 : row["index"].isDouble() && row["index"].toDouble() == i);
            }
            if (stage == 18)
                valid = valid && !array.isEmpty() &&
                        QRegularExpression("^[0-9a-f]{64}$").match(result["revision"].toString()).hasMatch();
            if (stage == 17)
                valid = valid && result["total"].isDouble() && result["total"].toDouble() >= array.size() &&
                        result["total"].toDouble() <= 10000000 &&
                        QRegularExpression("^[0-9a-f]{64}$").match(result["revision"].toString()).hasMatch();
            if (valid) {
                if (stage == 17) {
                    m_playlistRuleRevision = result["revision"].toString();
                    m_playlistPreviewReady = true;
                    m_playlistPreview = array.toVariantList();
                    m_playlistStatus =
                        QString("Preview · %1 of %2 items · not saved").arg(array.size()).arg(result["total"].toInt());
                } else {
                    m_playlistMembers = array.toVariantList();
                    m_playlistRevision = result["revision"].toString();
                    m_playlistStatus =
                        "Choose an item, move it with Up/Down, then Confirm. Save order applies the complete draft.";
                }
            } else
                m_playlistStatus = "Kodi returned invalid playlist data. Reload before retrying.";
        }
        emit changed();
        return;
    }
    if (m_stage == 9) {
        m_busy = false;
        releaseSocket();
        const auto filename = result["playlistName"].toString();
        if (!result["saved"].toBool() || !validPlaylistName(filename) || !filename.startsWith("Isolde-")) {
            m_playlistStatus =
                "Kodi returned an invalid playlist-save acknowledgement. Refresh the playlist list before retrying.";
            emit changed();
            return;
        }
        m_playlistStatus = "Playlist saved in Kodi · " + result["name"].toString();
        emit changed();
        emit playlistSaved(filename);
        return;
    }
    if (m_stage == 10) {
        m_busy = false;
        releaseSocket();
        if (!result["renamed"].toBool()) {
            m_playlistStatus = "Kodi did not confirm the playlist rename. Previous playlists are unchanged.";
            emit changed();
            return;
        }
        m_playlistStatus = "Playlist renamed in Kodi · " + result["newName"].toString();
        emit changed();
        return;
    }
    if (m_stage == 11) {
        m_busy = false;
        releaseSocket();
        if (!result["deleted"].toBool()) {
            m_playlistStatus = "Kodi did not confirm the playlist deletion. Previous playlists are unchanged.";
            emit changed();
            return;
        }
        m_playlistStatus = "Playlist deleted from Kodi · " + result["name"].toString();
        emit changed();
        return;
    }
    if (m_stage == 12) {
        m_busy = false;
        releaseSocket();
        if (!result["reordered"].toBool()) {
            m_playlistStatus = "Kodi did not confirm the playlist reorder. Previous playlists are unchanged.";
            emit changed();
            return;
        }
        m_playlistStatus = "Playlist reordered in Kodi · " + result["name"].toString();
        emit changed();
        return;
    }
    if (m_stage == 13) {
        m_busy = false;
        releaseSocket();
        m_playbackStatus = result.value("loaded").toBool()
                               ? QStringLiteral("Subtitle loaded in Kodi.")
                               : QStringLiteral("Kodi did not confirm loading the subtitle.");
        emit changed();
        return;
    }
    if (m_stage == 14 || m_stage == 15) {
        const auto tracks = result.value(QStringLiteral("tracks")).toArray();
        if (tracks.size() > 64 || !result.value(QStringLiteral("enabled")).isBool()) {
            fail(QStringLiteral("Kodi returned an invalid subtitle track response."));
            return;
        }
        QVariantList normalized;
        for (const auto& value : tracks) {
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
        return;
    }
    if (m_stage == 5 || m_stage == 6 || m_stage == 7) {
        const auto state = result.value("state").toString();
        if (!QStringList{"starting", "playing", "paused", "ended", "failed", "replaced", "idle"}.contains(state) ||
            !result.value("home").isBool()) {
            fail("Invalid playback status.");
            return;
        }
        m_busy = false;
        releaseSocket();
        m_playbackStatus = m_presentationError.isEmpty() ? state : m_presentationError;
        m_playbackCanPause = result.value("canPause").isBool() && result.value("canPause").toBool();
        m_playbackCanSeek = result.value("canSeek").isBool() && result.value("canSeek").toBool();
        if (result.value("controlError").isString())
            m_playbackStatus = result.value("controlError").toString().left(512);
        if (state == "failed" && result.value("reason").isString())
            m_playbackStatus = result.value("reason").toString().left(512);
        if (state == "starting" || state == "playing" || state == "paused") {
            m_playbackPoll.start();
            if (m_stage == 5 && state == "starting")
                emit playbackPresentationRequested();
            const auto optionRequest = result["optionsRequest"].toInt();
            if (optionRequest > 0 && optionRequest != m_optionsRequest) {
                m_optionsRequest = optionRequest;
                const auto generation = m_generation;
                QTimer::singleShot(0, this, [this, generation] {
                    if (generation == m_generation && m_playbackActive)
                        emit playbackOptionsRequested();
                });
            }
            if (state != "starting" && !m_playbackVisible) {
                m_adjacentTransition = false;
                m_playbackVisible = true;
                emit playbackStarted();
            }
        } else {
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
            if (state == "ended" && m_adjacentGlobal >= 0 && !m_returnHomeRequested && !result.value("home").toBool()) {
                m_adjacentAfterRead = m_adjacentGlobal;
                m_adjacentGlobal = -1;
                m_playbackStatus = "Loading the adjacent title from the same library view…";
                const auto generation = m_generation;
                const int target = m_adjacentAfterRead;
                QTimer::singleShot(0, this, [this, generation, target] {
                    if (generation == m_generation && m_adjacentTransition && !m_busy && !m_playbackActive) {
                        m_selectedIndex = target % 50;
                        loadPage((target / 50) * 50);
                    }
                });
                emit changed();
                return;
            }
            m_adjacentGlobal = m_adjacentAfterRead = -1;
            m_adjacentTransition = false;
            const bool home = result.value("home").toBool() || m_returnHomeRequested;
            m_returnHomeRequested = false;
            emit playbackReturned(home);
            if (state == "ended" && !m_scheduledPlayback) {
                const auto generation = m_generation;
                QTimer::singleShot(1200, this, [this, generation] {
                    if (generation == m_generation && m_connected && !m_busy && !m_playbackActive)
                        loadPage(m_pageStart);
                });
            }
        }
        emit changed();
        const auto surfRequest = result.value("channelRequest");
        if (m_playbackActive && surfRequest.isDouble() && surfRequest.toDouble() == surfRequest.toInt() &&
            surfRequest.toInt() > 0 && surfRequest.toInt() != m_lastGuideSurfRequest &&
            result.value("channelDelta").isDouble() &&
            (result.value("channelDelta").toDouble() == 1 || result.value("channelDelta").toDouble() == -1)) {
            m_lastGuideSurfRequest = surfRequest.toInt();
            const int direction = result.value("channelDelta").toInt();
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, direction, generation] {
                if (generation == m_generation && m_playbackActive)
                    emit guideSurfRequested(direction);
            });
        }
        if (!m_pendingControl.isEmpty()) {
            const auto action = m_pendingControl;
            m_pendingControl.clear();
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, action, generation] {
                if (generation == m_generation)
                    playbackControl(action);
            });
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
            m_timeout.stop();
            releaseSocket();
            ++m_manualMissingCount;
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == m_generation && m_busy && m_stage == 8)
                    loadNextManualMovie();
            });
            return;
        }
        auto entry = item.toVariantMap();
        entry["mediaType"] = shows ? "tvshow" : "movie";
        entry["mediaId"] = m_manualCurrentId;
        m_manualResults.append(entry);
        releaseSocket();
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (generation == m_generation && m_busy && m_stage == 8)
                loadNextManualMovie();
        });
        return;
    }
    if (m_stage >= 2) {
        if (m_stage == 2) {
            if (response.value("result") != "OK") {
                fail("Kodi did not accept the scan.");
                return;
            }
            m_stage = 3;
            m_scanStatus = "Scan requested. Waiting for Kodi's scan status…";
        } else {
            const auto scanning = result.value("Library.IsScanning" + m_scanKind);
            if (!scanning.isBool()) {
                fail("Kodi returned an invalid scan status.");
                return;
            }
            ++m_scanPolls;
            m_observedScan = m_observedScan || scanning.toBool();
            if ((!scanning.toBool() && (m_observedScan || m_scanPolls >= 5)) || m_scanPolls >= 1800) {
                m_scanStatus = scanning.toBool() ? "Kodi is still scanning. Monitoring stopped after 30 minutes."
                                   : m_observedScan  ? "Kodi finished scanning. Your files were not moved or renamed."
                                                     : "Kodi accepted the scan and is idle. A short scan may have finished "
                                                       "between checks; new library items are not guaranteed.";
                m_busy = false;
                releaseSocket();
                refresh();
                return;
            }
            if (scanning.toBool())
                m_scanStatus = m_scanKind == "Music"
                                   ? "Kodi is scanning configured music folders and tags. You can keep using Isolde."
                                   : "Kodi is scanning configured video folders. You can keep using Isolde.";
        }
        releaseSocket();
        emit changed();
        const auto generation = m_generation;
        QTimer::singleShot(1000, this, [this, generation] {
            if (m_busy && generation == m_generation)
                request("XBMC.GetInfoBooleans", {{"booleans", QJsonArray{"Library.IsScanning" + m_scanKind}}});
        });
        return;
    }
    if (m_stage == 0) {
        if (result.value("version").toObject().value("major").toInt() != 13) {
            fail("This Kodi API version has not been validated. Open Kodi directly for now.");
            return;
        }
        m_stage = 1;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (!m_busy || generation != m_generation)
                return;
            loadPage(m_requestedStart);
        });
        return;
    }
    const QString listKey = m_mediaKind == "history" ? (m_recentEpisodes ? "episodes" : "movies")
                            : m_mediaKind == "shows" ? "tvshows"
                                                     : m_mediaKind;
    const auto limits = result.value("limits").toObject();
    if (!result.value(listKey).isArray() && !(result.value(listKey).isUndefined() && limits.value("total").isDouble() &&
                                              limits.value("total").toInt(-1) == 0)) {
        fail("Kodi returned an invalid library list.");
        return;
    }
    const auto entries = result.value(listKey).toArray();
    const int total = limits.value("total").toInt(-1);
    if (entries.isEmpty() && total >= 0 && total < m_requestedStart) {
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (m_busy && generation == m_generation)
                loadPage(0);
        });
        return;
    }
    if (entries.size() > (m_mediaKind == "playlists" ? 128
                          : m_mediaKind == "history" ? m_historyLimit
                                                     : 50) ||
        total < 0 || limits.value("start").toInt(-1) != m_requestedStart ||
        limits.value("end").toInt(-1) != m_requestedStart + entries.size() ||
        total < m_requestedStart + entries.size() || (entries.isEmpty() && total > m_requestedStart)) {
        fail("Kodi returned invalid library paging information. Refresh to retry.");
        return;
    }
    m_pageStart = m_requestedStart;
    m_pageCount = entries.size();
    m_totalMovies = total;
    const auto defaultType = m_mediaKind == "playlists"  ? "playlist"
                             : m_mediaKind == "channels" ? "channel"
                             : m_mediaKind == "albums"   ? "album"
                             : m_mediaKind == "songs"    ? "song"
                             : m_mediaKind == "history"  ? (m_recentEpisodes ? "episode" : "movie")
                             : m_mediaKind == "movies"   ? "movie"
                             : m_mediaKind == "shows"    ? "tvshow"
                             : m_mediaKind == "seasons"  ? "season"
                                                         : "episode";
    QVariantList normalized;
    QSet<QString> ids;
    for (const auto& value : entries) {
        auto entry = value.toObject().toVariantMap();
        const QString type =
            m_viewKind == "playlist" ? entry.value("playlistMediaType").toString() : QString(defaultType);
        if (m_viewKind == "playlist" &&
            !(m_mediaKind == "movies" ? QStringList{"movie"} : QStringList{"tvshow", "episode"}).contains(type)) {
            fail("Kodi returned an invalid playlist media kind.");
            return;
        }
        if (m_mediaKind == "playlists" && !validPlaylistName(entry.value("playlistName").toString())) {
            fail("Kodi returned an invalid playlist name.");
            return;
        }
        const auto identityKey = m_mediaKind == "seasons" ? "season" : QString(type) + "id";
        const int identity = entry.value(identityKey, -1).toInt();
        const auto identityToken = type + ":" + QString::number(identity);
        if (identity < (m_mediaKind == "seasons" ? 0 : 1) || ids.contains(identityToken)) {
            fail("Kodi returned invalid or duplicate library identities.");
            return;
        }
        ids.insert(identityToken);
        entry["mediaType"] = type;
        entry["mediaId"] = entry.value(QString(type) + "id", -1);
        if (m_mediaKind == "history") {
            const auto date = entry.value("lastplayed").toString();
            const auto parsed = QDateTime::fromString(date, "yyyy-MM-dd HH:mm:ss");
            if (!parsed.isValid() || parsed.toString("yyyy-MM-dd HH:mm:ss") != date || date < "1970-01-02 00:00:00")
                continue;
            entry["viewedAt"] = date;
            entry["providerId"] = "kodi";
        }
        if (entry.value("title").toString().isEmpty())
            entry["title"] = m_mediaKind == "seasons"
                                 ? (identity == 0 ? QString("Specials") : "Season " + QString::number(identity))
                                 : entry.value("label");
        if (m_mediaKind == "channels") {
            if (entry.value("hidden").toBool())
                continue;
            const auto now = entry.value("broadcastnow").toMap(), next = entry.value("broadcastnext").toMap();
            entry["title"] =
                QString::number(entry.value("channelnumber").toInt()) + " · " + entry.value("title").toString();
            entry["subtitle"] =
                now.value("title").toString() +
                (next.value("title").toString().isEmpty() ? QString() : " · Next: " + next.value("title").toString());
            entry["plot"] = now.value("plot");
        }
        normalized.append(entry);
    }
    if (m_mediaKind == "history") {
        m_recentResults.append(normalized);
        if (!m_recentEpisodes) {
            m_recentEpisodes = true;
            releaseSocket();
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation] {
                if (m_busy && generation == m_generation)
                    loadPage(0);
            });
            return;
        }
        m_recentResults.append(m_appOpenings);
        normalized = sortedHistory(m_recentResults, m_historyLimit);
        m_recentResults.clear();
        m_pageStart = 0;
        m_pageCount = normalized.size();
        m_totalMovies = normalized.size();
    }
    m_movies = normalized;
    restoreHistorySelection();
    m_selectedIndex = qBound(-1, m_selectedIndex, qMax(-1, int(m_movies.size()) - 1));
    m_status =
        m_movies.isEmpty()
            ? (m_mediaKind == "channels"
                   ? "No TV channels in this profile. Open TV Guide to set up a source, then refresh."
               : m_mediaKind == "albums"
                   ? "No albums in this profile. Open Settings, then Media & Playback to add a music source and scan."
               : m_mediaKind == "history" ? "Nothing viewed in this profile yet. Kodi viewing history appears here."
               : m_viewKind == "all"
                   ? "No items in this library view. Open Settings, then Media & Playback to add or scan sources."
                   : m_viewKind == "continue" ? "No items with saved progress in this profile."
                                              : "Not found in your Kodi library.")
            : QString("%1 · %2–%3 of %4")
                  .arg(browseTitle())
                  .arg(m_pageStart + 1)
                  .arg(m_pageStart + m_movies.size())
                  .arg(m_totalMovies);
    m_busy = false;
    m_connected = true;
    releaseSocket();
    finishAdjacent();
    emit changed();
    if (m_pendingMovie > 0) {
        const int movie = m_pendingMovie;
        m_pendingMovie = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation, movie] {
            if (generation != m_generation)
                return;
            bool listed = false;
            for (const auto& entry : m_movies)
                listed |= entry.toMap().value("movieid").toInt() == movie;
            if (listed)
                playMovie(movie, true);
            else {
                m_status =
                    "This movie is no longer in the current collection page. Choose a movie from the refreshed list.";
                emit changed();
            }
        });
    }
    if (m_pendingShow > 0) {
        const int tvShowId = m_pendingShow;
        m_pendingShow = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation, tvShowId] {
            if (generation != m_generation)
                return;
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
        m_pendingDirectKind.clear();
        m_pendingDirectId = 0;
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation, kind, id, resume] {
            if (generation == m_generation && !m_busy && !m_playbackActive)
                playMedia(kind, id, resume);
        });
    }
}
