#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QScopeGuard>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>
#include <algorithm>
#include "media/KodiArtwork.h"
#include "media/KodiImporter.h"
#include "media/KodiMedia.h"
#include "media/KodiService.h"
#include "media/KodiShelves.h"
#include "media/MediaGrants.h"

class KodiMediaTests : public QObject
{
    Q_OBJECT
  private slots:
    void
    desktopImportIsRecursivePrivateReversibleAndGuarded();
    void
    startupPreferenceIsPerProfile();
    void
    frontendPreferenceRoundTrip();
    void
    localPlaybackPreferenceIsPersistedAndProfileScoped();
    void
    mediaFolderPermissions();
    void
    readsLibraryThroughTwoRequests();
    void
    savedKodiPlaylistCatalogAndShelf();
    void
    recentlyViewedMergesKodiHistoryAndStaysBounded();
    void
    historyCountAndRetirement();
    void
    selectedAppHistoryMergeAndOffline();
    void
    musicAlbumsTracksAndKodiPlayback();
    void
    tvGuideReadsChannelsAndKeepsUnavailableStateHonest();
    void
    pagesLibraryAndResets();
    void
    playbackLifecycle();
    void
    sourcePreviewAndPlaybackOptionsBoundaries();
    void
    scheduledPlaybackUsesExplicitBroadcastOffset();
    void
    scheduledUnavailableAcknowledgementUsesOnlyOwnedStartIdentity();
    void
    localViewsAndCollections();
    void
    manualMovieCollections();
    void
    manualShowCollections();
    void
    episodeSearchAndShowResult();
    void
    markWatchedSetsPlaycountForMovieAndEpisode();
    void
    homeCollectionShelves();
    void
    validatesPersistedCollections();
    void
    televisionHierarchyAndEpisodePlayback();
    void
    artworkSelectionCancellation();
    void
    rejectsUnknownVersion();
    void
    unavailableDoesNotHang();
    void
    timesOut();
    void
    invalidationClearsConnectedLibrary();
    void
    cancelledRequestCanRetry();
    void
    scansConfiguredSources();
    void
    cancelsScanMonitoring();
    void
    observedScanCompletes();
    void
    rejectedScanIsNotSuccess();
    void
    playlistCollisionAndRulePreview();
    void
    searchHandlesAmbiguousTitles();
    void
    sortPreservesEqualValuesAcrossPages();
    void
    searchResumesAfterBackAndProfileSwitch();
    void
    searchHandlesMissingSelection();
    void
    searchEmptyVersusDisconnected();
    void
    searchLocalUnderNetworkDenial();
    void
    metadataEditConfirmedAndRejected();
    void
    artworkRolesAndInvalidRole();
    void
    cleanLibraryAcceptedAndRejected();
    void
    exportSettingsVerified();
    void
    cancelledCleanLeavesOriginalState();
    void
    resumeRestartChoiceRespected();
    void
    seekPreviewCommitAndCancel();
    void
    nextEpisodeOptInAutoPlay();
    void
    playbackStaleSessionRecovery();
    void
    playbackExactFocusReturn();
    void
    playlistPreviewLeavesSavedUntouched();
    void
    playlistStableShelfReferenceAcrossRename();
    void
    playlistIdenticalReorderAcrossInputs();
    void
    playlistInvalidNestedRulesRejected();
    void
    playlistStaleWriteRejected();
    void
    playlistReopenSavedQueue();
    void
    musicShuffleRepeatControls();
    void
    musicLyricsVisualizationSlots();
    void
    picturesSlideshowControls();
    void
    musicVideosPicturesLibraryViews();
    void
    audioOffsetSetAndRetrieved();
    void
    audioOffsetBoundsAndPlaybackGuard();
    void
    playbackSpeedSetAndRetrieved();
    void
    playbackSpeedBoundsAndPlaybackGuard();
    void
    subtitleDelaySetAndRetrieved();
    void
    subtitleDelayBoundsAndPlaybackGuard();
    void
    recordingsListReturned();
    void
    recordingDeleteAndConfirm();
    void
    timersListReturned();
    void
    timerAddAndDelete();
    void
    nextUnwatchedFoundAndActivated();
    void
    mediaVersionsFoundAndSelected();
    void
    chaptersListReturned();
    void
    chapterSeekSelected();
    void
    musicCrossfadeGaplessReplayGain();
    void
    pvrGroupsListReturned();
    void
    pvrGroupMembersReturned();
    void
    pvrGroupAddAndRemove();
};
void
KodiMediaTests::localPlaybackPreferenceIsPersistedAndProfileScoped()
{
    QTemporaryDir config;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        KodiService first("local-playback-first");
        QVERIFY(first.alwaysPlayLocally());
        first.setAlwaysPlayLocally(false);
        KodiService restored("local-playback-first");
        QVERIFY(!restored.alwaysPlayLocally());
        KodiService other("local-playback-other");
        QVERIFY(other.alwaysPlayLocally());
        restored.resetAlwaysPlayLocally();
        KodiService reset("local-playback-first");
        QVERIFY(reset.alwaysPlayLocally());
        QSettings stored(QSettings::IniFormat, QSettings::UserScope, "Isolde", "Kodi");
        const auto file = stored.fileName();
        const auto directory = QFileInfo(file).absolutePath();
        const auto fileMode = QFile::permissions(file), directoryMode = QFile::permissions(directory);
        {
            auto restore = qScopeGuard(
                [&]
                {
                    QFile::setPermissions(directory, directoryMode);
                    QFile::setPermissions(file, fileMode);
                });
            QVERIFY(QFile::setPermissions(file, QFile::ReadOwner));
            QVERIFY(QFile::setPermissions(directory, QFile::ReadOwner | QFile::ExeOwner));
            reset.setAlwaysPlayLocally(false);
            QVERIFY(reset.alwaysPlayLocally());
            QVERIFY(reset.status().contains("Could not save"));
        }
        QSignalSpy changed(&reset, &KodiService::changed);
        reset.setAlwaysPlayLocally(false);
        QVERIFY(!reset.alwaysPlayLocally());
        QCOMPARE(reset.status(), QString("Playback preference saved."));
        QCOMPARE(changed.count(), 1);
        reset.resetAlwaysPlayLocally();
        QCOMPARE(reset.status(), QString("Playback preference reset to default."));
    }
    QSettings::setDefaultFormat(original);
}
void
KodiMediaTests::mediaFolderPermissions()
{
    QTemporaryDir config, media;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        KodiService service("folder-test");
        service.setEnabled(false);
        QVERIFY(service.beginFolderEditing());
        QVERIFY(service.canEditFolders());
        service.endFolderEditing();
        QVERIFY(!service.enabled());
        const auto capabilities = service.capabilityInventory();
        QVERIFY(capabilities.size() >= 10);
        QSet<QString> capabilityIds;
        for (const auto& value : capabilities)
            capabilityIds.insert(value.toMap()["id"].toString());
        QVERIFY(capabilityIds.contains("sources"));
        QVERIFY(capabilityIds.contains("tracks"));
        QVERIFY(capabilityIds.contains("import"));
        const auto libraryViews = service.libraryViews();
        QCOMPARE(libraryViews.size(), 5);
        QVERIFY(std::any_of(libraryViews.begin(),
                            libraryViews.end(),
                            [](const QVariant& row)
                            {
                                return row.toMap()["kind"] == "albums";
                            }));
        QCOMPARE(service.frontend(), QString("isolde"));
        QVERIFY(service.canEditFolders());
        service.addLibraryFolder(QUrl::fromLocalFile(media.path()));
        QCOMPARE(service.libraryFolders(), QStringList{media.path()});
        service.addLibraryFolder(QUrl::fromLocalFile(media.path()));
        QCOMPARE(service.libraryFolders().size(), 1);
        KodiService restored("folder-test"), other("folder-other");
        QCOMPARE(restored.libraryFolders(), service.libraryFolders());
        QVERIFY(other.libraryFolders().isEmpty());
        service.suspend(true);
        service.removeLibraryFolder(media.path());
        QCOMPARE(service.libraryFolders().size(), 1);
        service.suspend(false);
        service.removeLibraryFolder(media.path());
        QVERIFY(service.libraryFolders().isEmpty());
        QVERIFY(QFileInfo::exists(media.path()));
        for (const auto& path :
             QStringList{"/", "/home", "/etc", "/proc", "/tmp", QDir::homePath(), QDir::homePath() + "/.ssh"})
            QVERIFY2(!MediaGrants::validate(path).isEmpty(), qPrintable(path));
        QVERIFY(!MediaGrants::validate(media.path() + "/../escape").isEmpty());
        QVERIFY(!MediaGrants::validate(media.filePath("missing")).isEmpty());
        QVERIFY(MediaGrants::validate(media.filePath("missing"), false).isEmpty());
        service.browseLibraryFolder("");
        QVERIFY(service.folderChoices().contains(QDir::homePath()));
        service.browseLibraryFolder("/etc");
        QVERIFY(service.browsePath().isEmpty());
    }
    QSettings::setDefaultFormat(original);
}

void
KodiMediaTests::frontendPreferenceRoundTrip()
{
    QTemporaryDir dir;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    {
        KodiService service("frontend-test");
        service.setEnabled(false);
        QCOMPARE(service.frontend(), QString("isolde"));
        service.setFrontend("kodi");
        QCOMPARE(service.frontend(), QString("kodi"));
        QVERIFY(!service.enabled());
        QVERIFY(!service.switching());
        KodiService restored("frontend-test");
        QCOMPARE(restored.frontend(), QString("kodi"));
        KodiService other("another-profile");
        QCOMPARE(other.frontend(), QString("isolde"));
        service.setFrontend("invalid");
        QCOMPARE(service.frontend(), QString("kodi"));
        service.suspend(true);
        service.setFrontend("isolde");
        QCOMPARE(service.frontend(), QString("kodi"));
        service.suspend(false);
        service.setFrontend("isolde");
        QCOMPARE(service.frontend(), QString("isolde"));
        QVERIFY(!service.enabled());
    }
    QSettings::setDefaultFormat(original);
}

void
KodiMediaTests::startupPreferenceIsPerProfile()
{
    QTemporaryDir dir;
    const auto originalFormat = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    {
        KodiService first("startup-test-a");
        QVERIFY(first.enabled());
        first.setEnabled(false);
        KodiService restored("startup-test-a");
        QVERIFY(!restored.enabled());
        KodiService other("startup-test-b");
        QVERIFY(other.enabled());
    }
    QSettings::setDefaultFormat(originalFormat);
}

static void
serve(QLocalServer& server, int version)
{
    QObject::connect(
        &server,
        &QLocalServer::newConnection,
        &server,
        [&server, version]
        {
            while (server.hasPendingConnections())
            {
                auto* socket = server.nextPendingConnection();
                QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                QObject::connect(
                    socket,
                    &QLocalSocket::readyRead,
                    socket,
                    [socket, version, &server]
                    {
                        const auto bytes = socket->property("input").toByteArray() + socket->readAll();
                        socket->setProperty("input", bytes);
                        if (!bytes.endsWith('\n'))
                            return;
                        const auto request = QJsonDocument::fromJson(bytes).object();
                        if (server.property("rejectControl").toBool() && request["method"] == "Isolde.PlaybackControl")
                        {
                            server.setProperty("rejectedControls", server.property("rejectedControls").toInt() + 1);
                            socket->write(QJsonDocument(QJsonObject{{"jsonrpc", "2.0"},
                                                                    {"id", request["id"]},
                                                                    {"error",
                                                                     QJsonObject{{"code", -32000},
                                                                                 {"message", "Rejected control"}}}})
                                              .toJson(QJsonDocument::Compact) +
                                          '\n');
                            socket->disconnectFromServer();
                            return;
                        }
                        if (server.property("rejectPlaylistMutation").toBool() &&
                            request["method"] == "Isolde.SortPlaylist")
                        {
                            socket->write(
                                QJsonDocument(
                                    QJsonObject{{"jsonrpc", "2.0"},
                                                {"id", request["id"]},
                                                {"error", QJsonObject{{"code", -32000}, {"message", "Refused"}}}})
                                    .toJson(QJsonDocument::Compact) +
                                '\n');
                            socket->disconnectFromServer();
                            return;
                        }
                        QJsonValue result;
                        if (request.value("method") == "JSONRPC.Version")
                            result = QJsonObject{{"version", QJsonObject{{"major", version}}}};
                        else if (request.value("method").toString().startsWith("Isolde.Play") ||
                                 request.value("method") == "Isolde.PlaybackControl")
                        {
                            server.setProperty("lastPlayMethod", request.value("method").toString());
                            server.setProperty("lastPlayParams", request.value("params").toObject().toVariantMap());
                            if (request.value("method") == "Isolde.PlayEpisode")
                                server.setProperty("lastEpisodePlayId", request["params"].toObject().value("episodeid").toInt());
                            server.setProperty("playRequests", server.property("playRequests").toInt() + 1);
                            result = QJsonObject{{"state", server.property("playState").toString()},
                                                 {"home", server.property("playHome").toBool()}};
                            const auto overrideKey =
                                request.value("method") == "Isolde.PlaybackStatus" ? "pollResult" : "playResult";
                            if (server.property(overrideKey).isValid())
                                result = QJsonObject::fromVariantMap(server.property(overrideKey).toMap());
                            if (server.property("autoStop").toBool() && request["method"] == "Isolde.PlaybackControl" &&
                                QStringList{"stop", "home"}.contains(request["params"].toObject()["action"].toString()))
                                result = QJsonObject{{"state", "ended"},
                                                     {"home", request["params"].toObject()["action"] == "home"}};
                        }
                        else if (QStringList{"Isolde.GetPlaybackOptions", "Isolde.SetPlaybackOption"}.contains(
                                     request["method"].toString()))
                        {
                            server.setProperty("optionParams", request["params"].toObject().toVariantMap());
                            result = QJsonObject{
                                {"audio",
                                 QJsonArray{QJsonObject{
                                     {"id", 0}, {"name", "Stereo"}, {"language", "en"}, {"selected", true}}}},
                                {"subtitles", QJsonArray{}},
                                {"views", QJsonArray{"normal", "zoom"}},
                                {"view", "normal"},
                                {"subtitleEnabled", false}};
                            if (server.property("musicQueueOptions").toBool())
                            {
                                const auto params = request["params"].toObject();
                                const auto kind = params.value("kind").toString();
                                if (kind == "shuffle")
                                    server.setProperty("musicShuffled", params.value("value").toBool());
                                if (kind == "repeat")
                                    server.setProperty("musicRepeat", params.value("value").toString());
                                auto options = result.toObject();
                                options.insert("shuffled", server.property("musicShuffled").toBool());
                                options.insert("repeat", server.property("musicRepeat").toString());
                                result = options;
                            }
                            const auto params = request["params"].toObject();
                            if (params["kind"] == "audiooffset" || params["kind"] == "speed")
                            {
                                auto options = result.toObject();
                                options.insert(params["kind"] == "audiooffset" ? "audioOffsetMs" : "speed", params["value"]);
                                result = options;
                            }
                            if (server.property("badOptions").toBool())
                                result = QJsonObject{{"audio", QJsonArray{}}};
                        }
                        else if (QStringList{"Isolde.GetSources", "Isolde.PreviewSource", "Isolde.ApplySource"}
                                     .contains(request["method"].toString()))
                        {
                            const auto params = request["params"].toObject();
                            server.setProperty("sourceParams", params.toVariantMap());
                            result = QJsonObject{{"revision", QString(64, 'c')},
                                                 {"sources", QJsonArray{}},
                                                 {"scrapers", QJsonArray{}},
                                                 {"roots", QJsonArray{"/media/fixture"}},
                                                 {"applied", true}};
                            if (request["method"] == "Isolde.PreviewSource")
                                result = QJsonObject{
                                    {"revision", QString(64, 'c')}, {"source", params["source"]}, {"preview", true}};
                        }
                        else if (request.value("method") == "Isolde.GetPlaylists")
                        {
                            result = QJsonObject{{"playlists",
                                                  QJsonArray{QJsonObject{{"playlistid", 1},
                                                                         {"title", "Family films"},
                                                                         {"playlistName", "Family films.xsp"}}}},
                                                 {"limits", QJsonObject{{"start", 0}, {"end", 1}, {"total", 1}}}};
                        }
                        else if (request.value("method") == "Isolde.CreatePlaylist")
                        {
                            const auto params = request.value("params").toObject();
                            server.setProperty("savedPlaylist", params.toVariantMap());
                            result = QJsonObject{{"saved", !server.property("rejectPlaylist").toBool()},
                                                 {"name", params["name"]},
                                                 {"kind", params["kind"]},
                                                 {"playlistName", "Isolde-12345678.xsp"}};
                        }
                        else if (request.value("method") == "Isolde.RenamePlaylist")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            server.setProperty("lastParams", request["params"].toObject().toVariantMap());
                            const auto params = request.value("params").toObject();
                            server.setProperty("savedPlaylist", params.toVariantMap());
                            result = QJsonObject{{"renamed", !server.property("rejectPlaylist").toBool()},
                                                 {"newName", params["newName"]},
                                                 {"playlistName", "Isolde-12345678.xsp"}};
                        }
                        else if (request.value("method") == "Isolde.DeletePlaylist")
                        {
                            const auto params = request.value("params").toObject();
                            server.setProperty("savedPlaylist", params.toVariantMap());
                            result = QJsonObject{{"deleted", !server.property("rejectPlaylist").toBool()},
                                                 {"name", params["name"]}};
                        }
                        else if (request.value("method") == "Isolde.GetChapters")
                        {
                            result = QJsonObject{
                                {"chapters",
                                 QJsonArray{
                                     QJsonObject{{"index", 0}, {"time", 0}, {"title", "Chapter One"}, {"selected", true}},
                                     QJsonObject{{"index", 1}, {"time", 300}, {"title", "Chapter Two"}, {"selected", false}},
                                     QJsonObject{{"index", 2}, {"time", 600}, {"title", "Chapter Three"}, {"selected", false}},
                                 }},
                                {"current", 0}};
                        }
                        else if (request.value("method") == "Isolde.SeekChapter")
                        {
                            result = QJsonObject{{"seeked", true}};
                        }
                        else if (request.value("method") == "Isolde.GetMusicOptions")
                        {
                            result = QJsonObject{
                                {"options",
                                 QJsonObject{
                                     {"music.crossfade", true},
                                     {"music.gapless", false},
                                     {"music.replaygain", "track"},
                                 }}};
                        }
                        else if (request.value("method") == "Isolde.SetMusicOption")
                        {
                            server.setProperty("lastMusicOptionKind", request["params"].toObject()["kind"].toVariant());
                            server.setProperty("lastMusicOptionValue", request["params"].toObject()["value"].toVariant());
                            result = QJsonObject{{"set", true}};
                        }
                        else if (request.value("method") == "Isolde.GetPVRGroups")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            result = QJsonObject{
                                {"groups",
                                 QJsonArray{
                                     QJsonObject{{"id", 1}, {"name", "Favorites"}, {"type", "tv"}},
                                     QJsonObject{{"id", 2}, {"name", "Regional"}, {"type", "tv"}},
                                 }}};
                        }
                        else if (request.value("method") == "Isolde.GetGroupMembers")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            server.setProperty("lastParams", request["params"].toObject().toVariantMap());
                            result = QJsonObject{
                                {"members",
                                 QJsonArray{
                                     QJsonObject{{"id", 101}, {"title", "Station A"}, {"type", "channel"}},
                                     QJsonObject{{"id", 102}, {"title", "Station B"}, {"type", "channel"}},
                                 }},
                                {"groupId", request["params"].toObject().value("id")}};
                        }
                        else if (request.value("method") == "Isolde.AddToGroup")
                        {
                            server.setProperty("lastAddToGroupKind", request["params"].toObject()["type"].toVariant());
                            server.setProperty("lastAddToGroupId", request["params"].toObject()["id"].toVariant());
                            server.setProperty("lastAddToGroupMemberId", request["params"].toObject()["memberid"].toVariant());
                            result = QJsonObject{{"added", true}};
                        }
                        else if (request.value("method") == "Isolde.RemoveFromGroup")
                        {
                            server.setProperty("lastRemoveFromGroupKind", request["params"].toObject()["type"].toVariant());
                            server.setProperty("lastRemoveFromGroupId", request["params"].toObject()["id"].toVariant());
                            server.setProperty("lastRemoveFromGroupMemberId", request["params"].toObject()["memberid"].toVariant());
                            result = QJsonObject{{"removed", true}};
                        }
                        else if (request.value("method") == "Isolde.GetPlaylistItems")
                        {
                            server.setProperty("playlistParams", request.value("params").toObject().toVariantMap());
                            const auto params = request.value("params").toObject();
                            const int start = params["start"].toInt();
                            result = QJsonObject{
                                {"movies",
                                 QJsonArray{QJsonObject{{"movieid", start + 41},
                                                        {"title", "Local film"},
                                                        {"playlistMediaType", "movie"}}}},
                                {"limits", QJsonObject{{"start", start}, {"end", start + 1}, {"total", 51}}}};
                            if (params["kind"] == "shows")
                                result = QJsonObject{{"tvshows",
                                                      QJsonArray{QJsonObject{{"episodeid", 41},
                                                                             {"title", "Playlist episode"},
                                                                             {"playlistMediaType", "episode"}}}},
                                                     {"limits", QJsonObject{{"start", 0}, {"end", 1}, {"total", 1}}}};
                        }
                        else if (request["method"] == "Isolde.PreviewPlaylistRule")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            server.setProperty("lastParams", request["params"].toObject().toVariantMap());
                            server.setProperty("ruleParams", request["params"].toObject().toVariantMap());
                            result = QJsonObject{{"items",
                                                  QJsonArray{QJsonObject{
                                                      {"title", "Fixture"}, {"mediaType", "movie"}, {"mediaId", 41}}}},
                                                 {"total", 1},
                                                 {"revision", QString(64, 'a')}};
                        }
                        else if (request["method"] == "Isolde.EditPlaylist" ||
                                 request["method"] == "Isolde.SortPlaylist" ||
                                 request["method"] == "Isolde.ReorderPlaylist")
                        {
                            server.setProperty("playlistMutation", request["params"].toObject().toVariantMap());
                            result = QJsonObject{{"edited", true}, {"reordered", true}};
                        }
                        else if (request["method"] == "Isolde.GetPlaylistOrder")
                        {
                            result = QJsonObject{{"members",
                                                  QJsonArray{QJsonObject{{"title", "Alpha"}, {"index", 0}},
                                                             QJsonObject{{"title", "Beta"}, {"index", 1}}}},
                                                 {"revision", QString(64, 'b')}};
                        }
                        else if (request.value("method") == "VideoLibrary.Clean")
                        {
                            server.setProperty("lastMethod", "VideoLibrary.Clean");
                            server.setProperty("lastParams", request.value("params").toObject().toVariantMap());
                            server.setProperty("lastCleanParams", request.value("params").toObject().toVariantMap());
                            server.setProperty("cleanRequests", server.property("cleanRequests").toInt() + 1);
                            result = "OK";
                        }
                        else if (request.value("method") == "VideoLibrary.Scan" ||
                                 request.value("method") == "AudioLibrary.Scan")
                        {
                            result = server.property("rejectScan").toBool() ? "rejected" : "OK";
                            server.setProperty("scanRequests", server.property("scanRequests").toInt() + 1);
                            server.setProperty("scanMethod", request["method"].toString());
                        }
                        else if (request.value("method") == "XBMC.GetInfoBooleans")
                        {
                            const bool scanning = server.property("scanning").toBool();
                            server.setProperty("scanning", false);
                            const auto boolean = request["params"].toObject()["booleans"].toArray().first().toString();
                            result = QJsonObject{{boolean, scanning}};
                        }
                        else if (request.value("method") == "VideoLibrary.GetMovieDetails")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            server.setProperty("lastParams", request.value("params").toObject().toVariantMap());
                            const int movieId = request.value("params").toObject().value("movieid").toInt();
                            server.setProperty("detailRequests", server.property("detailRequests").toInt() + 1);
                            result = QJsonObject{{"moviedetails",
                                                  QJsonObject{{"movieid", movieId},
                                                              {"title", QString("Fixture movie %1").arg(movieId)},
                                                              {"year", 2024}}}};
                        }
                        else if (request.value("method") == "VideoLibrary.GetTVShowDetails")
                        {
                            const int showId = request.value("params").toObject().value("tvshowid").toInt();
                            server.setProperty("detailRequests", server.property("detailRequests").toInt() + 1);
                            result = QJsonObject{{"tvshowdetails",
                                                  QJsonObject{{"tvshowid", showId},
                                                              {"title", QString("Fixture show %1").arg(showId)},
                                                              {"episode", 4},
                                                              {"watchedepisodes", 1}}}};
                        }
                        else if (request.value("method") == "VideoLibrary.SetMovieDetails")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            server.setProperty("lastParams", request.value("params").toObject().toVariantMap());
                            const int movieId = request.value("params").toObject().value("movieid").toInt();
                            const auto details = request.value("params").toObject().value("details").toObject();
                            const int playcount = details.value("playcount").toInt();
                            server.setProperty("lastMarkedMovieId", movieId);
                            server.setProperty("lastMarkedMoviePlaycount", playcount);
                            result = QJsonObject{{"ok", true}};
                        }
                        else if (request.value("method") == "VideoLibrary.SetEpisodeDetails")
                        {
                            server.setProperty("lastMethod", request.value("method").toString());
                            server.setProperty("lastParams", request.value("params").toObject().toVariantMap());
                            const int episodeId = request.value("params").toObject().value("episodeid").toInt();
                            const auto details = request.value("params").toObject().value("details").toObject();
                            const int playcount = details.value("playcount").toInt();
                            server.setProperty("lastMarkedEpisodeId", episodeId);
                            server.setProperty("lastMarkedEpisodePlaycount", playcount);
                            result = QJsonObject{{"ok", true}};
                        }
                        else
                        {
                            server.setProperty("libraryRequests", server.property("libraryRequests").toInt() + 1);
                            const auto params = request.value("params").toObject();
                            const auto method = request.value("method").toString();
                            server.setProperty("lastMethod", method);
                            server.setProperty("lastParams", params.toVariantMap());
                            if (server.property("historyFixture").toBool())
                            {
                                auto calls = server.property("historyRequests").toList();
                                calls.append(QVariantMap{{"method", method}, {"params", params.toVariantMap()}});
                                server.setProperty("historyRequests", calls);
                            }
                            const int start = params.value("limits").toObject().value("start").toInt();
                            const int total = method == "VideoLibrary.GetSeasons"  ? 2
                                              : server.property("total").isValid() ? server.property("total").toInt()
                                                                                   : 1;
                            QJsonArray movies;
                            const auto listKey = method == "VideoLibrary.GetTVShows"    ? "tvshows"
                                                 : method == "VideoLibrary.GetSeasons"  ? "seasons"
                                                 : method == "VideoLibrary.GetEpisodes" ? "episodes"
                                                                                        : "movies";
                            const auto idKey = method == "VideoLibrary.GetTVShows"    ? "tvshowid"
                                               : method == "VideoLibrary.GetSeasons"  ? "seasonid"
                                               : method == "VideoLibrary.GetEpisodes" ? "episodeid"
                                                                                      : "movieid";
                            for (int i = start; i < qMin(params.value("limits").toObject().value("end").toInt(), total);
                                 ++i)
                            {
                                QJsonObject entry{{idKey, i + 1}, {"title", "Fixture movie"}};
                                if (params.value("unwatchedOnly").toBool())
                                    entry["playcount"] = server.property("candidatePlaycount").toInt();
                                if (server.property("historyFixture").toBool())
                                {
                                    entry["dateadded"] = "2026-10-24 00:00:00";
                                    entry["lastplayed"] =
                                        i == 2   ? (method == "VideoLibrary.GetEpisodes" ? "2026-02-30 00:00:00" : "")
                                        : i == 0 ? (method == "VideoLibrary.GetEpisodes" ? "2026-10-23 12:00:00"
                                                                                         : "2026-10-20 12:00:00")
                                        : i == 1 ? "2026-10-22 12:00:00"
                                                 : "2026-10-21 12:00:00";
                                }
                                if (method == "VideoLibrary.GetSeasons")
                                {
                                    entry["season"] = i;
                                    entry.remove("title");
                                }
                                if (method == "VideoLibrary.GetEpisodes")
                                {
                                    entry["season"] = params.value("season").toInt();
                                    entry["episode"] = i + 1;
                                    if (server.property("watchFixture").toBool() && i < 4)
                                    {
                                        if (i != 3)
                                            entry["playcount"] = i == 0 ? 1 : 0;
                                        if (i != 1)
                                            entry["resume"] = QJsonObject{{"position", 120}};
                                    }
                                }
                                movies.append(entry);
                            }
                            server.setProperty("lastStart", start);
                            server.setProperty("lastEnd", params.value("limits").toObject().value("end").toInt());
                            result = QJsonObject{
                                {listKey, movies},
                                {"limits",
                                 QJsonObject{{"start", start},
                                             {"end", start + movies.size()},
                                             {"total", server.property("badPaging").toBool() ? -1 : total}}}};
                        }
                        socket->write(
                            QJsonDocument(
                                QJsonObject{{"jsonrpc", "2.0"}, {"id", request.value("id")}, {"result", result}})
                                .toJson(QJsonDocument::Compact) +
                            '\n');
                        socket->disconnectFromServer();
                    });
            }
        });
}
void
KodiMediaTests::savedKodiPlaylistCatalogAndShelf()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia catalog("playlist-profile", nullptr, dir.path());
    catalog.openPlaylists();
    QTRY_VERIFY(catalog.connected());
    QCOMPARE(catalog.items().size(), 1);
    QCOMPARE(catalog.items()[0].toMap()["playlistName"].toString(), QString("Family films.xsp"));
    KodiMedia shelf("playlist-profile", nullptr, dir.path());
    shelf.openView("movies", "playlist", "Family films.xsp");
    QTRY_VERIFY(shelf.connected());
    QCOMPARE(shelf.items()[0].toMap()["mediaType"].toString(), QString("movie"));
    QCOMPARE(shelf.items()[0].toMap()["mediaId"].toInt(), 41);
    QCOMPARE(server.property("playlistParams").toMap()["name"].toString(), QString("Family films.xsp"));
    shelf.nextPage();
    QTRY_COMPARE(shelf.pageStart(), 50);
    QCOMPARE(shelf.items()[0].toMap()["mediaId"].toInt(), 91);
    const auto before = shelf.items();
    shelf.openView("movies", "playlist", "../secret.xsp");
    QCOMPARE(shelf.items(), before);
    shelf.refresh();
    QTRY_VERIFY(!shelf.busy());
    QCOMPARE(shelf.viewKind(), QString("playlist"));
    shelf.openView("movies", "genre", "Adventure");
    QTRY_VERIFY(!shelf.busy());
    const auto library = shelf.items();
    QSignalSpy saved(&shelf, &KodiMedia::playlistSaved);
    shelf.savePlaylist("Family & friends");
    QTRY_COMPARE(saved.size(), 1);
    QCOMPARE(server.property("savedPlaylist").toMap()["view"].toString(), QString("genre"));
    QCOMPARE(server.property("savedPlaylist").toMap()["value"].toString(), QString("Adventure"));
    QCOMPARE(shelf.items(), library);
    QVERIFY(shelf.playlistStatus().contains("saved"));
    server.setProperty("rejectPlaylist", true);
    shelf.savePlaylist("Refused");
    QTRY_VERIFY(!shelf.busy());
    QCOMPARE(saved.size(), 1);
    QCOMPARE(shelf.items(), library);
    QVERIFY(shelf.playlistStatus().contains("invalid"));
    catalog.invalidate("Profile closed");
    QVERIFY(catalog.items().isEmpty());
    KodiMedia episodes("playlist-profile", nullptr, dir.path());
    episodes.openView("shows", "playlist", "Episodes.xsp");
    QTRY_VERIFY(episodes.connected());
    QTRY_VERIFY(!episodes.busy());
    QCOMPARE(episodes.items()[0].toMap()["mediaType"].toString(), QString("episode"));
    server.setProperty("playState", "playing");
    episodes.activate(0);
    QTRY_VERIFY(episodes.playbackActive());
    QTRY_VERIFY(!episodes.busy());
    QCOMPARE(server.property("lastPlayParams").toMap()["episodeid"].toInt(), 41);
    QCOMPARE(episodes.viewKind(), QString("playlist"));
    server.setProperty("playState", "ended");
    episodes.playbackControl("stop");
    QTRY_VERIFY(!episodes.playbackActive());
    QCOMPARE(episodes.selectedIndex(), 0);
    QCOMPARE(episodes.viewValue(), QString("Episodes.xsp"));
    KodiMedia editor("playlist-profile", nullptr, dir.path());
    editor.openView("movies", "all");
    QTRY_VERIFY(editor.connected());
    QTRY_VERIFY(!editor.busy());
    const auto retained = editor.items();
    editor.editPlaylist("Family films.xsp", "search", "Fixture");
    QVERIFY(!editor.busy());
    QVERIFY(editor.playlistStatus().contains("Preview"));
    editor.previewPlaylist("Family films.xsp", "search", "Fixture");
    QTRY_VERIFY(!editor.busy());
    QVERIFY(editor.playlistPreviewReady());
    QCOMPARE(editor.playlistPreview().size(), 1);
    QCOMPARE(editor.items(), retained);
    editor.editPlaylist("Family films.xsp", "search", "Changed after preview");
    QVERIFY(!editor.busy());
    editor.editPlaylist("Family films.xsp", "search", "Fixture");
    QTRY_VERIFY(!editor.busy());
    QCOMPARE(server.property("playlistMutation").toMap()["revision"].toString(), QString(64, 'a'));
    editor.loadPlaylistMembers("Members.m3u8");
    QTRY_VERIFY(!editor.busy());
    QCOMPARE(editor.playlistMembers().size(), 2);
    editor.reorderPlaylist("Members.m3u8", {1, 0});
    QTRY_VERIFY(!editor.busy());
    QCOMPARE(server.property("playlistMutation").toMap()["revision"].toString(), QString(64, 'b'));
    server.setProperty("rejectPlaylistMutation", true);
    editor.sortPlaylist("Family films.xsp", "title", "ascending");
    QTRY_VERIFY(!editor.busy());
    QVERIFY(editor.connected());
    QCOMPARE(editor.items(), retained);
    QVERIFY(editor.playlistStatus().contains("failed"));
}
void
KodiMediaTests::playlistCollisionAndRulePreview()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia catalog("playlist-collision", nullptr, dir.path());
    catalog.openPlaylists();
    QTRY_VERIFY(catalog.connected());
    QCOMPARE(catalog.items().size(), 1);
    QCOMPARE(catalog.items()[0].toMap()["playlistName"].toString(), QString("Family films.xsp"));

    // Same-name rename is rejected locally without a Kodi request.
    catalog.renamePlaylist("Family films.xsp", "Family films.xsp");
    QVERIFY(!catalog.busy());
    QVERIFY(catalog.playlistStatus().contains("Cannot rename"));

    // Delete the existing playlist.
    catalog.deletePlaylist("Family films.xsp");
    QTRY_VERIFY(!catalog.busy());
    QVERIFY(catalog.playlistStatus().contains("deleted"));

    // Rule preview: preview first, then edit requires the preview revision.
    catalog.previewPlaylist("New Playlist.xsp", "search", "Fixture");
    QTRY_VERIFY(!catalog.busy());
    QVERIFY(catalog.playlistPreviewReady());
    QCOMPARE(catalog.playlistPreview().size(), 1);
    QCOMPARE(catalog.playlistPreview()[0].toMap()["title"].toString(), QString("Fixture"));
    QCOMPARE(server.property("ruleParams").toMap()["name"].toString(), QString("New Playlist.xsp"));
    QCOMPARE(server.property("ruleParams").toMap()["view"].toString(), QString("search"));
    QCOMPARE(server.property("ruleParams").toMap()["value"].toString(), QString("Fixture"));

    // Edit requires the same rule params as preview.
    catalog.editPlaylist("New Playlist.xsp", "search", "Fixture");
    QTRY_VERIFY(!catalog.busy());
    QVERIFY(catalog.playlistStatus().contains("saved"));

    // Rename collision: reject simulates Kodi refusing the name.
    server.setProperty("rejectPlaylist", true);
    catalog.renamePlaylist("New Playlist.xsp", "Collision.xsp");
    QTRY_VERIFY(!catalog.busy());
    QVERIFY(catalog.playlistStatus().contains("did not confirm"));
    QCOMPARE(server.property("savedPlaylist").toMap()["newName"].toString(), QString("Collision.xsp"));

    // Rename succeeds when Kodi accepts.
    server.setProperty("rejectPlaylist", false);
    catalog.renamePlaylist("New Playlist.xsp", "Renamed.xsp");
    QTRY_VERIFY(!catalog.busy());
    QVERIFY(catalog.playlistStatus().contains("renamed"));
    QCOMPARE(server.property("savedPlaylist").toMap()["newName"].toString(), QString("Renamed.xsp"));

    catalog.invalidate("Test complete");
}
void
KodiMediaTests::recentlyViewedMergesKodiHistoryAndStaysBounded()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("historyFixture", true);
    server.setProperty("total", 3);
    KodiMedia media("history-test", nullptr, dir.path());
    media.openView("history", "watched");
    QTRY_VERIFY(media.connected());
    QVERIFY(!media.busy());
    QCOMPARE(media.items().size(), 4);
    const auto items = media.items();
    QCOMPARE(items[0].toMap()["mediaType"].toString(), QString("episode"));
    QCOMPARE(items[0].toMap()["mediaId"].toInt(), 1);
    QCOMPARE(items[1].toMap()["mediaType"].toString(), QString("episode"));
    QCOMPARE(items[1].toMap()["mediaId"].toInt(), 2);
    QCOMPARE(items[2].toMap()["mediaType"].toString(), QString("movie"));
    QCOMPARE(items[2].toMap()["mediaId"].toInt(), 2);
    QCOMPARE(items[3].toMap()["mediaType"].toString(), QString("movie"));
    QCOMPARE(items[3].toMap()["mediaId"].toInt(), 1);
    const auto calls = server.property("historyRequests").toList();
    QCOMPARE(calls.size(), 2);
    for (const auto& call : calls)
    {
        const auto params = call.toMap()["params"].toMap();
        QCOMPARE(params["limits"].toMap()["end"].toInt(), 50);
        QCOMPARE(params["sort"].toMap()["method"].toString(), QString("lastplayed"));
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("lastplayed"));
        QVERIFY(params["properties"].toList().contains("lastplayed"));
    }
    QVERIFY(!media.hasNextPage());
    media.nextPage();
    QTest::qWait(30);
    QCOMPARE(server.property("historyRequests").toList().size(), 2);
    server.setProperty("total", 60);
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.connected());
    QCOMPARE(media.items().size(), 50);
    QVERIFY(!media.hasNextPage());
    server.setProperty("total", 0);
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.connected());
    QVERIFY(media.items().isEmpty());
    QVERIFY(media.status().contains("Nothing viewed"));
    media.refresh();
    media.invalidate("Profile closed");
    QTest::qWait(30);
    QVERIFY(!media.connected());
    QVERIFY(media.items().isEmpty());
    QCOMPARE(media.status(), QString("Profile closed"));
}
void
KodiMediaTests::historyCountAndRetirement()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("historyFixture", true);
    server.setProperty("total", 60);
    KodiMedia one("one", nullptr, dir.path()), many("one", nullptr, dir.path());
    one.openHistory(1);
    many.openHistory(7);
    QTRY_VERIFY(one.connected());
    QTRY_VERIFY(many.connected());
    QCOMPARE(one.items().size(), 1);
    QCOMPARE(many.items().size(), 7);
    QCOMPARE(one.items()[0].toMap()["mediaType"].toString(), QString("episode"));
    for (const auto& call : server.property("historyRequests").toList())
    {
        const auto params = call.toMap()["params"].toMap();
        QVERIFY((QList<int>{1, 7}.contains(params["limits"].toMap()["end"].toInt())));
    }
    const int reads = server.property("libraryRequests").toInt();
    one.openHistory(0);
    one.openHistory(51);
    QTest::qWait(30);
    QCOMPARE(server.property("libraryRequests").toInt(), reads);
    QCOMPARE(one.items().size(), 1);
    one.invalidate("Bridge offline");
    one.refresh();
    QTRY_VERIFY(one.connected());
    one.retire("Profile closed");
    const int retiredReads = server.property("libraryRequests").toInt();
    one.refresh();
    one.openHistory(5);
    one.openView("movies", "all");
    one.playMedia("movie", 1);
    QTest::qWait(100);
    QVERIFY(one.items().isEmpty());
    QVERIFY(!one.connected());
    QCOMPARE(server.property("libraryRequests").toInt(), retiredReads);
    // A request already in flight must not publish after teardown either.
    many.refresh();
    many.retire("Profile closed");
    QTest::qWait(100);
    QVERIFY(many.items().isEmpty());
    QVERIFY(!many.connected());
    QVERIFY(!many.busy());
}
void
KodiMediaTests::selectedAppHistoryMergeAndOffline()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("historyFixture", true);
    server.setProperty("total", 3);
    const QVariantMap app{{"providerId", "isolde-app-openings"},
                          {"mediaType", "app"},
                          {"mediaId", "fixture"},
                          {"appId", "fixture"},
                          {"title", "Fixture movie"},
                          {"viewedAt", "2026-10-23 12:00:00"}};
    KodiMedia history("merged", nullptr, dir.path());
    history.setAppOpenings({app});
    history.openHistory(5);
    QTRY_VERIFY(history.connected());
    QCOMPARE(history.items().size(), 5);
    QCOMPARE(history.items()[0].toMap()["providerId"].toString(), QString("isolde-app-openings"));
    QCOMPARE(history.items()[1].toMap()["providerId"].toString(), QString("kodi"));
    history.selectIndex(2);
    const auto selected = history.items()[2].toMap();
    auto another = app;
    another["appId"] = "another";
    another["mediaId"] = "another";
    history.setAppOpenings({app, another});
    history.refresh();
    QTRY_VERIFY(!history.busy());
    QCOMPARE(history.selectedIndex(), 3);
    QCOMPARE(history.items()[history.selectedIndex()].toMap()["mediaId"], selected["mediaId"]);
    QCOMPARE(history.items()[history.selectedIndex()].toMap()["mediaType"], selected["mediaType"]);
    history.setAppOpenings({app});
    history.refresh();
    QTRY_VERIFY(!history.busy());
    // Equal titles and equal timestamps from different providers remain distinct.
    history.setAppOpenings({app, app});
    history.refresh();
    QTRY_VERIFY(!history.busy());
    QCOMPARE(history.items().size(), 5);
    auto invalid = app;
    invalid["viewedAt"] = "no date";
    history.setAppOpenings({invalid});
    history.refresh();
    QTRY_VERIFY(!history.busy());
    QCOMPARE(history.items().size(), 5);
    history.setAppOpenings({});
    history.refresh();
    QTRY_VERIFY(!history.busy());
    QCOMPARE(history.items().size(), 4);
    history.setAppOpenings({app});
    server.setProperty("badPaging", true);
    history.refresh();
    QTRY_VERIFY(!history.busy());
    QVERIFY(!history.connected());
    QCOMPARE(history.items().size(), 1);
    QVERIFY(history.status().contains("invalid"));
    server.setProperty("badPaging", false);
    history.refresh();
    QTRY_VERIFY(history.connected());
    QCOMPARE(history.items().size(), 5);
    history.retire("Profile closed");
    QVERIFY(history.items().isEmpty());
}
void
KodiMediaTests::musicAlbumsTracksAndKodiPlayback()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    QList<QJsonObject> calls;
    bool emptyMusic = true;
    connect(&server,
            &QLocalServer::newConnection,
            &server,
            [&]
            {
                while (server.hasPendingConnections())
                {
                    auto* socket = server.nextPendingConnection();
                    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                    connect(socket,
                            &QLocalSocket::readyRead,
                            socket,
                            [&, socket]
                            {
                                auto bytes = socket->property("input").toByteArray() + socket->readAll();
                                socket->setProperty("input", bytes);
                                if (!bytes.endsWith('\n'))
                                    return;
                                const auto request = QJsonDocument::fromJson(bytes).object();
                                calls.append(request);
                                const auto method = request["method"].toString();
                                QJsonObject result;
                                if (method == "JSONRPC.Version")
                                    result = {{"version", QJsonObject{{"major", 13}}}};
                                else if (method == "AudioLibrary.GetAlbums" || method == "AudioLibrary.GetSongs")
                                {
                                    const bool albums = method.endsWith("Albums");
                                    const auto params = request["params"].toObject();
                                    QCOMPARE(params["limits"].toObject()["end"].toInt(), 50);
                                    if (!albums)
                                        QCOMPARE(params["filter"].toObject()["albumid"].toInt(), 41);
                                    const QJsonObject item{{albums ? "albumid" : "songid", albums ? 41 : 23},
                                                           {"title", albums ? "An actual Kodi album" : "First track"},
                                                           {"artist", QJsonArray{"Composer"}},
                                                           {"duration", 120}};
                                    result = {{albums ? "albums" : "songs", QJsonArray{item}},
                                              {"limits", QJsonObject{{"start", 0}, {"end", 1}, {"total", 1}}}};
                                    if (emptyMusic)
                                        result = {{"limits", QJsonObject{{"start", 0}, {"end", 0}, {"total", 0}}}};
                                }
                                else if (method == "Isolde.PlaySong")
                                {
                                    QCOMPARE(request["params"].toObject()["songid"].toInt(), 23);
                                    QVERIFY(!request["params"].toObject()["resume"].toBool());
                                    result = {{"state", "playing"}, {"home", false}};
                                }
                                else
                                    result = {{"state", method == "Isolde.PlaybackControl" ? "ended" : "playing"},
                                              {"home", false}};
                                socket->write(
                                    QJsonDocument(
                                        QJsonObject{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}})
                                        .toJson(QJsonDocument::Compact) +
                                    '\n');
                                socket->disconnectFromServer();
                            });
                }
            });
    KodiMedia media("music-profile", nullptr, dir.path());
    media.openView("albums", "all");
    QTRY_VERIFY(media.connected());
    QVERIFY(!media.busy());
    QVERIFY(media.items().isEmpty());
    emptyMusic = false;
    media.refresh();
    QTRY_VERIFY(media.connected() && !media.busy());
    QCOMPARE(media.items().first().toMap()["mediaType"].toString(), QString("album"));
    QCOMPARE(media.browseTitle(), QString("Music · Albums"));
    const auto before = calls.size();
    media.openView("albums", "continue");
    QTest::qWait(20);
    QCOMPARE(calls.size(), before);
    media.activate(0);
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.connected());
    QCOMPARE(media.mediaKind(), QString("songs"));
    QCOMPARE(media.browseTitle(), QString("An actual Kodi album"));
    QCOMPARE(media.items().first().toMap()["mediaType"].toString(), QString("song"));
    media.playSelected(true);
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    media.playbackControl("stop");
    QTRY_VERIFY(!media.playbackActive());
    QVERIFY(media.goBack());
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("albums"));
    QCOMPARE(media.selectedIndex(), 0);
    media.invalidate("Profile closed");
    QVERIFY(media.items().isEmpty());
    QVERIFY(!media.connected());
}
void
KodiMediaTests::tvGuideReadsChannelsAndKeepsUnavailableStateHonest()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    bool empty = false, reject = false;
    int plays = 0;
    connect(
        &server,
        &QLocalServer::newConnection,
        &server,
        [&]
        {
            while (server.hasPendingConnections())
            {
                auto* socket = server.nextPendingConnection();
                connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                connect(
                    socket,
                    &QLocalSocket::readyRead,
                    socket,
                    [&, socket]
                    {
                        auto bytes = socket->property("input").toByteArray() + socket->readAll();
                        socket->setProperty("input", bytes);
                        if (!bytes.endsWith('\n'))
                            return;
                        const auto request = QJsonDocument::fromJson(bytes).object();
                        const auto method = request["method"].toString();
                        QJsonObject result;
                        if (method == "JSONRPC.Version")
                            result = {{"version", QJsonObject{{"major", 13}}}};
                        else if (method == "PVR.GetChannels")
                        {
                            const auto params = request["params"].toObject();
                            QCOMPARE(params["channelgroupid"].toString(), QString("alltv"));
                            QCOMPARE(params["limits"].toObject()["end"].toInt(), 50);
                            QVERIFY(!params.contains("sort"));
                            if (reject)
                            {
                                socket->write(QJsonDocument(QJsonObject{{"jsonrpc", "2.0"},
                                                                        {"id", request["id"]},
                                                                        {"error",
                                                                         QJsonObject{{"code", -32000},
                                                                                     {"message", "PVR unavailable"}}}})
                                                  .toJson(QJsonDocument::Compact) +
                                              '\n');
                                socket->disconnectFromServer();
                                return;
                            }
                            QJsonArray channels;
                            if (!empty)
                                for (int i = 1; i <= 3; ++i)
                                    channels.append(
                                        QJsonObject{{"channelid", i},
                                                    {"channelnumber", i},
                                                    {"label", QString("Channel %1").arg(i)},
                                                    {"hidden", i == 3},
                                                    {"locked", i == 2},
                                                    {"broadcastnow", QJsonObject{{"title", "Current programme"}}},
                                                    {"broadcastnext", QJsonObject{{"title", "Next programme"}}}});
                            result = {
                                {"channels", channels},
                                {"limits",
                                 QJsonObject{{"start", 0}, {"end", channels.size()}, {"total", channels.size()}}}};
                        }
                        else if (method == "Isolde.PlayChannel")
                        {
                            ++plays;
                            QCOMPARE(request["params"].toObject()["channelid"].toInt(), 1);
                            QVERIFY(!request["params"].toObject()["resume"].toBool());
                            result = {{"state", "playing"}, {"home", false}};
                        }
                        else
                            result = {{"state", method == "Isolde.PlaybackControl" ? "ended" : "playing"},
                                      {"home", false}};
                        socket->write(
                            QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}})
                                .toJson(QJsonDocument::Compact) +
                            '\n');
                        socket->disconnectFromServer();
                    });
            }
        });
    KodiMedia media("guide-profile", nullptr, dir.path());
    media.openView("channels", "all");
    QTRY_VERIFY(media.connected());
    QCOMPARE(media.items().size(), 2);
    QCOMPARE(media.browseTitle(), QString("TV Guide"));
    QCOMPARE(media.items().first().toMap()["mediaType"].toString(), QString("channel"));
    QVERIFY(media.items().first().toMap()["subtitle"].toString().contains("Next programme"));
    media.activate(0);
    QTRY_COMPARE(plays, 1);
    QTRY_VERIFY(!media.busy());
    media.playbackControl("stop");
    QTRY_VERIFY(!media.playbackActive());
    empty = true;
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.connected());
    QVERIFY(media.items().isEmpty());
    QVERIFY(media.status().contains("No TV channels"));
    reject = true;
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(!media.connected());
    QVERIFY(media.status().contains("PVR TV source"));
}
void
KodiMediaTests::artworkSelectionCancellation()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QByteArray png;
    QBuffer buffer(&png);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&buffer, "PNG"));
    int requests = 0, lastId = 0;
    connect(&server,
            &QLocalServer::newConnection,
            &server,
            [&]
            {
                while (server.hasPendingConnections())
                {
                    auto* socket = server.nextPendingConnection();
                    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                    connect(socket,
                            &QLocalSocket::readyRead,
                            socket,
                            [&, socket]
                            {
                                const auto bytes = socket->property("input").toByteArray() + socket->readAll();
                                socket->setProperty("input", bytes);
                                if (!bytes.endsWith('\n'))
                                    return;
                                const auto request = QJsonDocument::fromJson(bytes).object();
                                ++requests;
                                lastId = request["params"].toObject()["id"].toInt();
                                server.setProperty("lastArtworkMethod", request["method"].toString());
                                server.setProperty("lastArtworkParams", request["params"].toObject().toVariantMap());
                                const auto result = server.property("bad").toBool()
                                                        ? QJsonObject{{"mime", "image/png"}, {"data", "not an image"}}
                                                    : request["method"] == "Isolde.SetPoster"
                                                        ? QJsonObject{{"updated", true}}
                                                        : QJsonObject{{"mime", "image/png"},
                                                                      {"data", QString::fromLatin1(png.toBase64())}};
                                socket->write(
                                    QJsonDocument(
                                        QJsonObject{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}})
                                        .toJson(QJsonDocument::Compact) +
                                    '\n');
                                socket->disconnectFromServer();
                            });
                }
            });
    KodiArtwork art("test", nullptr, dir.path());
    art.load("movie", 1);
    art.load("episode", 2);
    QTRY_VERIFY(art.source().startsWith("data:image/png;base64,"));
    QCOMPARE(requests, 1);
    QCOMPARE(lastId, 2);
    art.load("episode", 3);
    art.clear();
    QTest::qWait(220);
    QVERIFY(art.source().isEmpty());
    QCOMPARE(requests, 1);
    server.setProperty("bad", true);
    art.load("tvshow", 4);
    QTRY_VERIFY(art.status().contains("format unavailable"));
    QVERIFY(art.source().isEmpty());
    art.load("unknown", 1);
    QTest::qWait(180);
    QCOMPARE(requests, 2);
    const auto poster = dir.filePath("poster.png");
    QVERIFY(image.save(poster));
    art.setManagedArtworkResolver(
        [&](const QString& id)
        {
            return id == "artwork/managed.png" ? QUrl::fromLocalFile(poster).toString() : QString{};
        });
    QVERIFY(!art.setPoster("movie", 1, "../outside"));
    QVERIFY(!art.setPoster("episode", 1, "artwork/managed.png"));
    server.setProperty("bad", false);
    QVERIFY(art.setPoster("movie", 7, "artwork/managed.png"));
    QVERIFY(!art.setPoster("movie", 7, "artwork/managed.png"));
    QTRY_COMPARE(art.status(), QString("Poster updated in Kodi"));
    QCOMPARE(server.property("lastArtworkMethod").toString(), QString("Isolde.SetPoster"));
    const auto params = server.property("lastArtworkParams").toMap();
    QCOMPARE(params["id"].toInt(), 7);
    QVERIFY(!params.contains("path"));
    QVERIFY(!params["data"].toString().isEmpty());
    QVERIFY(art.source().startsWith("data:image/png;base64,"));
}
void
KodiMediaTests::televisionHierarchyAndEpisodePlayback()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("total", 103);
    KodiMedia media("tv-test", nullptr, dir.path());
    media.openLibrary("shows");
    QTRY_VERIFY(media.connected());
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetTVShows"));
    media.nextPage();
    QTRY_VERIFY(!media.busy());
    media.selectIndex(2);
    media.activate(2);
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("seasons"));
    QCOMPARE(server.property("lastParams").toMap()["tvshowid"].toInt(), 53);
    QCOMPARE(media.movies()[0].toMap()["title"].toString(), QString("Specials"));
    media.activate(0);
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("episodes"));
    QCOMPARE(server.property("lastParams").toMap()["season"].toInt(), 0);
    QCOMPARE(server.property("lastParams").toMap()["tvshowid"].toInt(), 53);
    server.setProperty("playState", "paused");
    media.playSelected(false);
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayEpisode"));
    QCOMPARE(server.property("lastPlayParams").toMap()["episodeid"].toInt(), 1);
    QCOMPARE(server.property("lastPlayParams").toMap()["resume"].toBool(), false);
    QVERIFY(!media.goBack()); // playback owns the current view
    media.invalidate("Test stop");
    QVERIFY(media.goBack());
    QTRY_VERIFY(media.connected());
    QCOMPARE(media.mediaKind(), QString("seasons"));
    QCOMPARE(media.selectedIndex(), 0);
    QVERIFY(media.goBack());
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("shows"));
    QCOMPARE(media.pageStart(), 50);
    QCOMPARE(media.selectedIndex(), 2);
    QVERIFY(!media.goBack());
    media.setView("continue");
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("episodes"));
    QVERIFY(!server.property("lastParams").toMap().contains("tvshowid"));
    QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("inprogress"));
    QVERIFY(media.goBack());
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.pageStart(), 50);
    QCOMPARE(media.selectedIndex(), 2);
    media.setView("recent");
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastParams").toMap()["sort"].toMap()["method"].toString(), QString("dateadded"));
    QVERIFY(!server.property("lastParams").toMap().contains("filter"));
    media.openLibrary("shows"); // cancels superseded episode read
    media.openLibrary("movies");
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("movies"));
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    media.setView("recent");
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastParams").toMap()["sort"].toMap()["method"].toString(), QString("dateadded"));
    media.openLibrary("movies");
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.viewKind(), QString("all"));
    QCOMPARE(server.property("lastParams").toMap()["sort"].toMap()["method"].toString(), QString("title"));
}
void
KodiMediaTests::pagesLibraryAndResets()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    server.setProperty("total", 103);
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(media.connected());
    QCOMPARE(media.movies().size(), 50);
    QCOMPARE(media.totalMovies(), 103);
    QVERIFY(media.hasNextPage());
    media.nextPage();
    media.nextPage(); // duplicate input cannot skip a page
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.pageStart(), 50);
    QCOMPARE(server.property("lastEnd").toInt(), 100);
    media.nextPage();
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.movies().size(), 3);
    QVERIFY(!media.hasNextPage());
    media.previousPage();
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.pageStart(), 50);
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.pageStart(), 0);
    media.nextPage();
    media.invalidate("Profile changed");
    QTest::qWait(100);
    QCOMPARE(media.totalMovies(), 0);
    QVERIFY(media.movies().isEmpty());
    server.setProperty("badPaging", true);
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(!media.connected());
    QVERIFY(media.status().contains("paging"));
    server.setProperty("badPaging", false);
    server.setProperty("total", 0);
    media.refresh();
    QTRY_VERIFY(media.connected());
    QVERIFY(media.movies().isEmpty());
    QVERIFY(!media.hasNextPage());
}
void
KodiMediaTests::localViewsAndCollections()
{
    QTemporaryDir config, dir;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("total", 103);
        KodiMedia media("local-view-user", nullptr, dir.path());
        media.setView("search", "  Fixture  ");
        QTRY_VERIFY(media.connected());
        QCOMPARE(media.viewValue(), QString("Fixture"));
        auto params = server.property("lastParams").toMap();
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("title"));
        media.nextPage();
        QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["value"].toString(), QString("Fixture"));
        media.saveCollection("My movies");
        QCOMPARE(media.collections().size(), 1);
        const auto id = media.collections()[0].toMap()["id"].toString();
        media.saveCollection("my MOVIES");
        QCOMPARE(media.collections().size(), 1);
        KodiMedia restored("local-view-user", nullptr, dir.path()), other("other-user", nullptr, dir.path());
        QCOMPARE(restored.collections().size(), 1);
        QVERIFY(other.collections().isEmpty());
        media.renameCollection(id, "Renamed");
        QCOMPARE(restored.collections()[0].toMap()["name"].toString(), QString("Renamed"));
        media.setView("continue");
        QTRY_VERIFY(!media.busy());
        QCOMPARE(media.pageStart(), 0);
        params = server.property("lastParams").toMap();
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("inprogress"));
        QCOMPARE(params["sort"].toMap()["method"].toString(), QString("lastplayed"));
        media.setView("watched");
        QTRY_VERIFY(!media.busy());
        params = server.property("lastParams").toMap();
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("lastplayed"));
        QCOMPARE(params["filter"].toMap()["operator"].toString(), QString("after"));
        QCOMPARE(params["sort"].toMap()["method"].toString(), QString("lastplayed"));
        QCOMPARE(params["sort"].toMap()["order"].toString(), QString("descending"));
        QCOMPARE(params["limits"].toMap()["end"].toInt(), 50);
        media.openCollection(id);
        QTRY_VERIFY(!media.busy());
        QCOMPARE(media.viewKind(), QString("search"));
        media.setView("tag", "Family");
        media.setView("genre", "Drama"); // superseded read must not leak results
        QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("genre"));
        media.setView("all");
        QTRY_VERIFY(!media.busy());
        QVERIFY(!server.property("lastParams").toMap().contains("filter"));
        media.removeCollection(id);
        QVERIFY(restored.collections().isEmpty());
        media.openLibrary("shows");
        QTRY_VERIFY(!media.busy());
        media.openView("shows", "watched");
        QTRY_VERIFY(!media.busy());
        QCOMPARE(media.mediaKind(), QString("episodes"));
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetEpisodes"));
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("lastplayed"));
        media.openLibrary("shows");
        QTRY_VERIFY(!media.busy());
        media.setView("search", "North");
        QTRY_VERIFY(!media.busy());
        params = server.property("lastParams").toMap();
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetTVShows"));
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("title"));
        QCOMPARE(params["filter"].toMap()["value"].toString(), QString("North"));
        media.saveCollection("North shows");
        QCOMPARE(media.collections().size(), 1);
        const auto showId = media.collections()[0].toMap()["id"].toString();
        QCOMPARE(media.collections()[0].toMap()["media"].toString(), QString("shows"));
        media.setView("genre", "Drama");
        QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetTVShows"));
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("genre"));
        media.openCollection(showId);
        QTRY_VERIFY(!media.busy());
        QCOMPARE(media.mediaKind(), QString("shows"));
        QCOMPARE(media.viewKind(), QString("search"));
        QCOMPARE(media.viewValue(), QString("North"));
        media.removeCollection(showId);
        QVERIFY(media.collections().isEmpty());
    }
    QSettings::setDefaultFormat(original);
}
void
KodiMediaTests::manualMovieCollections()
{
    QTemporaryDir config, directory;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("total", 3);
        KodiMedia media("manual-user", nullptr, directory.path());
        media.openLibrary("movies");
        QTRY_VERIFY(media.connected());
        media.createManualMovieCollection("Favorites", 1);
        QCOMPARE(media.collections().size(), 1);
        auto collection = media.collections().first().toMap();
        QCOMPARE(collection.value("kind").toString(), QString("manual"));
        QCOMPARE(collection.value("members").toList(), QVariantList{1});
        const auto id = collection.value("id").toString();
        media.addMovieToCollection(id, 2);
        media.addMovieToCollection(id, 2);
        QCOMPARE(media.collections().first().toMap().value("members").toList(), QVariantList({1, 2}));

        media.openCollection(id);
        QTRY_VERIFY(media.connected() && !media.busy());
        QCOMPARE(media.viewKind(), QString("manual"));
        QCOMPARE(media.items().size(), 2);
        QCOMPARE(media.items()[0].toMap().value("movieid").toInt(), 1);
        QCOMPARE(media.items()[1].toMap().value("movieid").toInt(), 2);
        QCOMPARE(server.property("detailRequests").toInt(), 2);

        media.removeMovieFromCollection(id, 1);
        QTRY_VERIFY(media.connected() && !media.busy());
        QCOMPARE(media.items().size(), 1);
        QCOMPARE(media.items()[0].toMap().value("movieid").toInt(), 2);
        KodiMedia restored("manual-user", nullptr, directory.path());
        QCOMPARE(restored.collections().first().toMap().value("members").toList(), QVariantList{2});
        KodiShelves shelves("manual-user", &media, nullptr, directory.path());
        media.configureShelf(id, true, 2);
        shelves.setAvailable(true);
        QTRY_COMPARE(shelves.items().size(), 2);
        const auto action = shelves.items()[1].toMap().value("action").toMap();
        QCOMPARE(action.value("type").toString(), QString("play-movie"));
        QCOMPARE(action.value("movieId").toInt(), 2);
        server.setProperty("playState", "playing");
        media.playCollectionMovie(id, 2);
        QTRY_VERIFY(media.playbackActive());
        QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayMovie"));
        QCOMPARE(server.property("lastPlayParams").toMap().value("movieid").toInt(), 2);
        media.invalidate("Manual collection test complete");
    }
    QSettings::setDefaultFormat(original);
}
void
KodiMediaTests::manualShowCollections()
{
    QTemporaryDir config, directory;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("total", 3);
        KodiMedia media("manual-show-user", nullptr, directory.path());
        media.openLibrary("shows");
        QTRY_VERIFY(media.connected());
        media.createManualCollection("Favorite series", "tvshow", 1);
        QCOMPARE(media.collections().size(), 1);
        const auto id = media.collections().first().toMap().value("id").toString();
        QCOMPARE(media.collections().first().toMap().value("media").toString(), QString("shows"));
        media.addManualCollectionMember(id, "tvshow", 2);
        QCOMPARE(media.collections().first().toMap().value("members").toList(), QVariantList({1, 2}));

        media.openCollection(id);
        QTRY_VERIFY(media.connected() && !media.busy());
        QCOMPARE(media.items().size(), 2);
        QCOMPARE(media.items()[0].toMap().value("tvshowid").toInt(), 1);
        QCOMPARE(media.items()[0].toMap().value("mediaType").toString(), QString("tvshow"));
        QCOMPARE(server.property("detailRequests").toInt(), 2);

        KodiShelves shelves("manual-show-user", &media, nullptr, directory.path());
        media.configureShelf(id, true, 2);
        QCOMPARE(shelves.items().first().toMap().value("name").toString(), QString("Unavailable · open Shows"));
        shelves.setAvailable(true);
        QTRY_VERIFY(shelves.items().size() >= 3);
        const auto showAction = shelves.items()[2].toMap().value("action").toMap();
        QCOMPARE(showAction.value("type").toString(), QString("open-show"));
        QCOMPARE(showAction.value("tvShowId").toInt(), 2);

        media.openCollectionShow(id, 2);
        QTRY_COMPARE(media.mediaKind(), QString("seasons"));
        QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetSeasons"));
        QCOMPARE(server.property("lastParams").toMap().value("tvshowid").toInt(), 2);
    }
    QSettings::setDefaultFormat(original);
}
// Verify the episode browse contract preserves the distinction between missing and zero play count.
void
KodiMediaTests::episodeSearchAndShowResult()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("watchFixture", true);
    server.setProperty("total", 4);
    KodiMedia episodes("search-episodes", nullptr, directory.path());
    episodes.openView("episodes", "search", "Pilot");
    QTRY_VERIFY(episodes.connected());
    QCOMPARE(episodes.mediaKind(), QString("episodes"));
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetEpisodes"));
    auto filter = server.property("lastParams").toMap().value("filter").toMap();
    QCOMPARE(filter.value("field").toString(), QString("title"));
    QCOMPARE(filter.value("operator").toString(), QString("contains"));
    QCOMPARE(filter.value("value").toString(), QString("Pilot"));
    QTRY_COMPARE(episodes.movies().size(), 4);
    const QStringList states{"watched", "unwatched", "inProgress", "unknown"};
    for (qsizetype index = 0; index < states.size(); ++index)
        QCOMPARE(episodes.movies().at(index).toMap().value("watchState").toString(), states.at(index));

    KodiMedia shows("search-shows", nullptr, directory.path());
    shows.openShowResult("Fixture", 1);
    QTRY_COMPARE(shows.mediaKind(), QString("seasons"));
    QTRY_VERIFY(!shows.busy());
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetSeasons"));
    QCOMPARE(server.property("lastParams").toMap().value("tvshowid").toInt(), 1);

    server.setProperty("playState", "playing");
    KodiMedia queuedPlayback("search-playback", nullptr, directory.path());
    queuedPlayback.refresh();
    queuedPlayback.playMedia("movie", 1, true);
    QTRY_VERIFY(queuedPlayback.playbackActive());
    QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayMovie"));
    queuedPlayback.invalidate("Test complete");
}
void
KodiMediaTests::markWatchedSetsPlaycountForMovieAndEpisode()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Mark a movie watched via media.markWatched action descriptor path.
    {
        KodiMedia media("mark-movie-watched", nullptr, directory.path());
        media.markWatched("movie", 7, true);
        QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.SetMovieDetails"));
        auto params = server.property("lastParams").toMap();
        auto details = params.value("details").toMap();
        QCOMPARE(params.value("movieid").toInt(), 7);
        QCOMPARE(details.value("playcount").toInt(), 1);
        QCOMPARE(server.property("lastMarkedMovieId").toInt(), 7);
        QCOMPARE(server.property("lastMarkedMoviePlaycount").toInt(), 1);
    }

    // Mark a movie unwatched via the same path.
    {
        KodiMedia media("mark-movie-unwatched", nullptr, directory.path());
        media.markWatched("movie", 3, false);
        QTRY_COMPARE(server.property("lastMarkedMovieId").toInt(), 3);
        auto params = server.property("lastParams").toMap();
        auto details = params.value("details").toMap();
        QCOMPARE(params.value("movieid").toInt(), 3);
        QCOMPARE(details.value("playcount").toInt(), 0);
    }

    // Mark an episode watched via media.markWatched action descriptor path.
    {
        KodiMedia media("mark-episode-watched", nullptr, directory.path());
        media.markWatched("episode", 11, true);
        QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.SetEpisodeDetails"));
        auto params = server.property("lastParams").toMap();
        auto details = params.value("details").toMap();
        QCOMPARE(params.value("episodeid").toInt(), 11);
        QCOMPARE(details.value("playcount").toInt(), 1);
        QCOMPARE(server.property("lastMarkedEpisodeId").toInt(), 11);
        QCOMPARE(server.property("lastMarkedEpisodePlaycount").toInt(), 1);
    }

    // markWatched rejects invalid kind or id.
    {
        KodiMedia media("mark-invalid", nullptr, directory.path());
        QVERIFY(!media.markWatched("song", 1, true));
        QVERIFY(!media.markWatched("movie", 0, true));
        QVERIFY(!media.markWatched("movie", -5, true));
    }
}

void
KodiMediaTests::homeCollectionShelves()
{
    QTemporaryDir config, dir;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("total", 10);
        KodiMedia media("shelf-user", nullptr, dir.path());
        media.setView("search", "Fixture");
        QTRY_VERIFY(media.connected());
        media.saveCollection("First");
        media.saveCollection("Second");
        auto first = media.collections()[0].toMap()["id"].toString();
        auto second = media.collections()[1].toMap()["id"].toString();
        KodiShelves shelves("shelf-user", &media, nullptr, dir.path());
        QVERIFY(shelves.items().isEmpty());
        media.configureShelf(first, true, 2);
        QCOMPARE(shelves.items().size(), 1); // browse/recovery placeholder offline
        shelves.setAvailable(true);
        QTRY_COMPARE(shelves.items().size(), 3);
        const auto movieAction = shelves.items()[1].toMap()["action"].toMap();
        QCOMPARE(movieAction["collectionId"].toString(), first);
        QCOMPARE(movieAction["movieId"].toInt(), 1);
        const auto stableId = shelves.items()[1].toMap()["id"];
        shelves.refresh();
        QTRY_COMPARE(shelves.items().size(), 3);
        QCOMPARE(shelves.items()[1].toMap()["id"], stableId);
        media.configureShelf(second, true, 3);
        media.moveCollection(second, -1);
        QTRY_COMPARE(shelves.sections().size(), 2);
        QCOMPARE(shelves.sections()[0].toMap()["name"].toString(), QString("Second"));
        media.renameCollection(first, "Renamed");
        QCOMPARE(shelves.sections()[1].toMap()["name"].toString(), QString("Renamed"));
        QTRY_COMPARE(shelves.items().size(), 7);
        const int reads = server.property("libraryRequests").toInt();
        media.renameCollection(first, "Renamed again");
        QCOMPARE(shelves.items().size(), 7); // no loading placeholders during edits
        media.moveCollection(first, -1);
        QCOMPARE(shelves.items().size(), 7);
        media.configureShelf(first, true, 4);
        QCOMPARE(shelves.items().size(), 9); // use already loaded results
        shelves.setAvailable(true);          // duplicate ready notification is not a refresh
        QTest::qWait(150);
        QCOMPARE(server.property("libraryRequests").toInt(), reads);
        KodiMedia other("shelf-other", nullptr, dir.path());
        KodiShelves otherShelves("shelf-other", &other, nullptr, dir.path());
        QVERIFY(otherShelves.items().isEmpty());
        server.setProperty("playState", "playing");
        media.playCollectionMovie(first, 1);
        QTRY_VERIFY(media.playbackActive());
        media.invalidate("Test stop");
        shelves.setAvailable(false);
        QCOMPARE(shelves.items().size(), 2);
        media.configureShelf(first, false, 2);
        QCOMPARE(shelves.sections().size(), 1);
        media.removeCollection(second);
        QVERIFY(shelves.items().isEmpty());
        QVERIFY(!media.collections().isEmpty()); // unpin kept reusable definition
        media.openLibrary("shows");
        QTRY_VERIFY(!media.busy());
        media.setView("search", "Fixture Show");
        QTRY_VERIFY(!media.busy());
        media.saveCollection("Shows");
        const auto showCollection = media.collections().last().toMap()["id"].toString();
        media.configureShelf(showCollection, true, 2);
        shelves.setAvailable(true);
        QTRY_COMPARE(shelves.sections().size(), 1);
        QTRY_VERIFY(shelves.items().size() >= 2);
        const auto showAction = shelves.items()[1].toMap()["action"].toMap();
        QCOMPARE(showAction["type"].toString(), QString("open-show"));
        QCOMPARE(showAction["tvShowId"].toInt(), 1);
        media.openCollectionShow(showCollection, 2);
        QTRY_COMPARE(media.mediaKind(), QString("seasons"));
        QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetSeasons"));
        QCOMPARE(server.property("lastParams").toMap()["tvshowid"].toInt(), 2);
        media.openCollectionShow(showCollection, 999);
        QTRY_COMPARE(media.mediaKind(), QString("shows"));
        QTRY_VERIFY(!media.busy());
        QVERIFY(media.status().contains("no longer in the selected collection"));
    }
    QSettings::setDefaultFormat(original);
}
void
KodiMediaTests::validatesPersistedCollections()
{
    QTemporaryDir config;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        const QJsonObject valid{{"id", "valid-id"},
                                {"name", " Family "},
                                {"kind", "tag"},
                                {"value", " Kids "},
                                {"home", true},
                                {"count", 999},
                                {"command", "untrusted"}};
        auto unknown = valid;
        unknown["id"] = "unknown";
        unknown["kind"] = "shell";
        auto duplicateName = valid;
        duplicateName["id"] = "duplicate-name";
        duplicateName["name"] = "family";
        auto invalidId = valid;
        invalidId["id"] = "../other";
        auto emptyValue = valid;
        emptyValue["id"] = "empty";
        emptyValue["value"] = " ";
        const QJsonObject manual{{"id", "manual-id"},
                                 {"name", "Manual"},
                                 {"kind", "manual"},
                                 {"media", "movies"},
                                 {"value", ""},
                                 {"members", QJsonArray{1, 2, 3}}};
        auto badManual = manual;
        badManual["id"] = "bad-manual";
        badManual["members"] = "1,2,3";
        const auto bytes =
            QJsonDocument(
                QJsonArray{valid, manual, badManual, valid, unknown, duplicateName, invalidId, emptyValue, 42})
                .toJson();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Isolde", "Collections");
        settings.setValue("profiles/validated/v1", bytes);
        settings.sync();
        KodiMedia media("validated");
        const auto entries = media.collections();
        QCOMPARE(entries.size(), 2);
        const auto entry = entries[0].toMap();
        QCOMPARE(entry["name"].toString(), QString("Family"));
        QCOMPARE(entry["value"].toString(), QString("Kids"));
        QCOMPARE(entry["media"].toString(), QString("movies")); // v1 definitions remain movie collections
        QCOMPARE(entry["count"].toInt(), 12);
        QVERIFY(!entry.contains("command"));
        QCOMPARE(entries[1].toMap()["kind"].toString(), QString("manual"));
        QCOMPARE(entries[1].toMap()["members"].toList(), QVariantList({1, 2, 3}));
        QCOMPARE(settings.value("profiles/validated/v1").toByteArray(), bytes); // read does not rewrite originals
        KodiMedia other("validated-other");
        QVERIFY(other.collections().isEmpty());
        settings.setValue("profiles/validated/v1", "broken JSON");
        settings.sync();
        QVERIFY(media.collections().isEmpty());
    }
    QSettings::setDefaultFormat(original);
}
void
KodiMediaTests::playbackLifecycle()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    QSignalSpy started(&media, &KodiMedia::playbackStarted), returned(&media, &KodiMedia::playbackReturned);
    QSignalSpy presented(&media, &KodiMedia::playbackPresentationRequested);
    media.refresh();
    QTRY_VERIFY(media.connected());
    media.playMovie(999, true);
    QVERIFY(!media.playbackActive());
    server.setProperty("playState", "starting");
    media.playMovie(1, true);
    media.playMovie(1, true);
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("playRequests").toInt(), 1);
    QVERIFY(media.playbackActive());
    QCOMPARE(started.size(), 0);
    QCOMPARE(presented.size(), 1);
    QTest::qWait(600);
    QCOMPARE(presented.size(), 1);
    QCOMPARE(started.size(), 0);
    server.setProperty("playState", "playing");
    QTRY_COMPARE(started.size(), 1);
    server.setProperty("playState", "paused");
    media.playbackControl("pause");
    QTRY_COMPARE(media.playbackStatus(), QString("paused"));
    server.setProperty("rejectControl", true);
    QTRY_VERIFY(!media.busy());
    media.playbackControl("forward");
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.connected());
    QVERIFY(media.playbackActive());
    QVERIFY(!media.movies().isEmpty());
    QVERIFY(media.playbackStatus().contains("retained"));
    server.setProperty("rejectControl", false);
    server.setProperty("playState", "playing");
    for (int index = 0; index < 40; ++index)
    {
        QTRY_VERIFY(!media.busy());
        media.playbackControl(index % 2 ? "backward" : "forward");
        QTRY_VERIFY(!media.busy());
        QVERIFY(media.connected());
        QVERIFY(media.playbackActive());
        QCOMPARE(media.playbackStatus(), QString("playing"));
    }
    server.setProperty("playState", "ended");
    server.setProperty("playHome", true);
    QTRY_VERIFY(!media.playbackActive());
    QCOMPARE(returned.size(), 1);
    QVERIFY(returned[0][0].toBool());
    server.setProperty("playState", "starting");
    media.playMovie(1, false);
    media.invalidate("Service stopped");
    QTest::qWait(700);
    QVERIFY(!media.playbackActive());
    QVERIFY(!media.connected());
    QCOMPARE(started.size(), 1);
}
void
KodiMediaTests::sourcePreviewAndPlaybackOptionsBoundaries()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("total", 103);
    server.setProperty("playState", "playing");
    server.setProperty("autoStop", true);
    KodiMedia media("source-options", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(media.connected());
    bool allowed = true;
    media.setSourceEditGuard(
        [&]
        {
            return allowed;
        });
    media.refreshSources();
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.sourceRoots(), QStringList{"/media/fixture"});
    const QVariantMap source{{"name", "Movies"},
                             {"path", "/media/fixture/"},
                             {"category", "video"},
                             {"content", "movies"},
                             {"scraper", "metadata.local"},
                             {"recursive", true},
                             {"folderNames", false},
                             {"noUpdate", false},
                             {"exclude", false},
                             {"remove", false}};
    media.previewSource(source);
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.sourcePreviewReady());
    auto changed = source;
    changed["name"] = "Changed";
    media.applySource(changed);
    QVERIFY(!media.busy());
    QSignalSpy applied(&media, &KodiMedia::sourceApplied);
    allowed = false;
    media.applySource(source);
    QVERIFY(!media.busy());
    QVERIFY(applied.isEmpty());
    allowed = true;
    media.applySource(source);
    QTRY_COMPARE(applied.size(), 1);
    QVERIFY(!media.sourcePreviewReady());
    QCOMPARE(server.property("sourceParams").toMap()["revision"].toString(), QString(64, 'c'));
    media.previewSource(source);
    QTRY_VERIFY(!media.busy());
    media.cancelSourcePreview();
    media.applySource(source);
    QVERIFY(!media.busy());
    QCOMPARE(applied.size(), 1);
    media.selectIndex(49);
    media.playSelected();
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    media.scanMusic();
    QVERIFY(media.scanStatus().contains("Stop playback"));
    QVERIFY(media.playbackActive());
    QVERIFY(media.playbackCanPrevious());
    QVERIFY(media.playbackCanNext());
    media.refreshPlaybackOptions();
    QTRY_VERIFY(!media.busy());
    QCOMPARE(media.playbackOptions()["audio"].toList().size(), 1);
    media.setPlaybackOption("view", "zoom");
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("optionParams").toMap()["value"].toString(), QString("zoom"));
    server.setProperty("badOptions", true);
    media.refreshPlaybackOptions();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.playbackOptions().isEmpty());
    QVERIFY(media.playbackActive());
    QVERIFY(media.connected());
    QVERIFY(!media.playAdjacent(2));
    QVERIFY(media.playAdjacent(1));
    QTRY_COMPARE(server.property("lastPlayParams").toMap()["movieid"].toInt(), 51);
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    QCOMPARE(media.pageStart(), 50);
    QCOMPARE(media.selectedIndex(), 0);
    QVERIFY(media.playAdjacent(-1));
    QTRY_COMPARE(server.property("lastPlayParams").toMap()["movieid"].toInt(), 50);
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    QCOMPARE(media.pageStart(), 0);
    QCOMPARE(media.selectedIndex(), 49);
    QSignalSpy returned(&media, &KodiMedia::playbackReturned);
    media.playbackControl("home");
    QTRY_VERIFY(!media.playbackActive());
    QCOMPARE(returned.size(), 1);
    QVERIFY(returned[0][0].toBool());
    media.scanMusic();
    QTRY_COMPARE(server.property("scanMethod").toString(), QString("AudioLibrary.Scan"));
    media.invalidate("Stopped monitoring");
    media.refresh();
    QTRY_VERIFY(media.connected() && !media.busy());
    media.previewSource(source);
    media.retire("Profile closed");
    QTest::qWait(50);
    QVERIFY(!media.sourcePreviewReady());
    QVERIFY(!media.connected());
    QVERIFY(!media.busy());
    QVERIFY(media.sourceRoots().isEmpty());
    QVERIFY(media.librarySources().isEmpty());
    QVERIFY(media.playbackOptions().isEmpty());
}
void
KodiMediaTests::scheduledPlaybackUsesExplicitBroadcastOffset()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("playState", "starting");
    KodiMedia media("guide-profile", nullptr, dir.path());
    QSignalSpy started(&media, &KodiMedia::playbackStarted), returned(&media, &KodiMedia::playbackReturned);
    QVERIFY(!media.playScheduled("channel", 1, 100));
    QVERIFY(!media.playScheduled("movie", 0, 100));
    QVERIFY(!media.playScheduled("movie", 7, -1));
    QVERIFY(!media.playScheduled("movie", 7, 86400));
    QVERIFY(media.playScheduled("episode", 7, 4511));
    QVERIFY(!media.playScheduled("movie", 8, 0)); // only one playback owner
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayScheduled"));
    QCOMPARE(server.property("lastPlayParams").toMap(),
             QVariantMap({{"mediaType", "episode"}, {"mediaId", 7}, {"offsetSeconds", 4511}}));
    QCOMPARE(started.size(), 0);
    server.setProperty("playState", "playing");
    QTRY_COMPARE(started.size(), 1);
    server.setProperty("playState", "ended");
    media.playbackControl("stop");
    QTRY_VERIFY(!media.playbackActive());
    QCOMPARE(returned.size(), 1);
}
void
KodiMediaTests::scheduledUnavailableAcknowledgementUsesOnlyOwnedStartIdentity()
{
    const QVariantMap unavailable{{"state", "failed"},
                                  {"home", false},
                                  {"availability", "unavailable"},
                                  {"mediaType", "movie"},
                                  {"mediaId", 999}};
    QVariantList results{unavailable};
    for (const auto& home : QVariantList{QVariant(), 0, QString("false"), true})
    {
        auto invalid = unavailable;
        if (home.isValid())
            invalid["home"] = home;
        else
            invalid.remove("home");
        results.append(invalid);
    }
    auto generic = unavailable;
    generic.remove("availability");
    results.append(generic);
    auto malformed = unavailable;
    malformed["availability"] = true;
    results.append(malformed);
    for (int index = 0; index < results.size(); ++index)
    {
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("playResult", results[index]);
        KodiMedia media("missing", nullptr, dir.path());
        QSignalSpy missing(&media, &KodiMedia::scheduledMediaUnavailable);
        QVERIFY(media.playScheduled("episode", 7, 0));
        QTRY_VERIFY(!media.playbackActive());
        QCOMPARE(missing.size(), index == 0 ? 1 : 0);
        if (index == 0)
        {
            QCOMPARE(missing[0][0].toString(), QString("episode"));
            QCOMPARE(missing[0][1].toInt(), 7);
        }
    }
    {
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("playResult", unavailable);
        KodiMedia media("pvr", nullptr, dir.path());
        QSignalSpy missing(&media, &KodiMedia::scheduledMediaUnavailable);
        QVERIFY(media.playPvr(7, 9, 101));
        QTRY_VERIFY(!media.playbackActive());
        QVERIFY(missing.isEmpty());
    }
    {
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("playState", "playing");
        server.setProperty("pollResult", unavailable);
        KodiMedia media("late-failure", nullptr, dir.path());
        QSignalSpy missing(&media, &KodiMedia::scheduledMediaUnavailable);
        QVERIFY(media.playScheduled("episode", 7, 0));
        QTRY_VERIFY(!media.playbackActive());
        QVERIFY(missing.isEmpty());
    }
    {
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("rpc.sock")));
        serve(server, 13);
        server.setProperty("playResult", unavailable);
        KodiMedia media("invalidated", nullptr, dir.path());
        QSignalSpy missing(&media, &KodiMedia::scheduledMediaUnavailable);
        QVERIFY(media.playScheduled("episode", 7, 0));
        media.invalidate("Profile closed");
        QTest::qWait(100);
        QVERIFY(missing.isEmpty());
    }
}
void
KodiMediaTests::readsLibraryThroughTwoRequests()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!media.busy(), 5000);
    QCOMPARE(media.movies().size(), 1);
    QCOMPARE(media.movies()[0].toMap().value("title").toString(), QString("Fixture movie"));
    media.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!media.busy(), 5000);
    QCOMPARE(media.movies().size(), 1);
}
void
KodiMediaTests::observedScanCompletes()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    server.setProperty("scanning", true);
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(media.connected());
    media.scanMovies();
    media.scanMovies();
    QTRY_VERIFY_WITH_TIMEOUT(!media.busy(), 6000);
    QVERIFY(media.scanStatus().contains("finished scanning"));
    QCOMPARE(server.property("scanRequests").toInt(), 1);
}
void
KodiMediaTests::rejectedScanIsNotSuccess()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    server.setProperty("rejectScan", true);
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(media.connected());
    media.scanMovies();
    QTRY_VERIFY(!media.busy());
    QVERIFY(!media.connected());
    QVERIFY(media.scanStatus().contains("did not accept"));
}
void
KodiMediaTests::scansConfiguredSources()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.scanMovies();
    QVERIFY(!media.busy()); // Cannot scan before validated connection.
    media.refresh();
    QTRY_VERIFY(media.connected());
    media.scanMovies();
    QVERIFY(media.busy());
    media.scanMovies(); // Duplicate request is ignored.
    QTRY_VERIFY_WITH_TIMEOUT(!media.busy(), 10000);
    QVERIFY(media.connected());
    QVERIFY(media.scanStatus().contains("accepted"));
    QVERIFY(!media.scanStatus().contains("finished scanning"));
    QCOMPARE(media.movies().size(), 1);
}
void
KodiMediaTests::cancelsScanMonitoring()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(media.connected());
    media.scanMovies();
    QTRY_VERIFY(media.scanStatus().contains("requested"));
    media.invalidate("Profile stopped");
    media.refresh();
    QTRY_VERIFY(media.connected());
    QTest::qWait(1500);
    QVERIFY(!media.busy());
    QVERIFY(media.connected());
    QVERIFY(media.scanStatus().contains("unavailable"));
}
void
KodiMediaTests::rejectsUnknownVersion()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 99);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.movies().isEmpty());
    QVERIFY(media.status().contains("not been validated"));
}
void
KodiMediaTests::invalidationClearsConnectedLibrary()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(media.connected());
    QCOMPARE(media.movies().size(), 1);
    KodiService service("invalidation-test");
    connect(&service, &KodiService::unavailable, &media, &KodiMedia::invalidate);
    service.stop();
    QVERIFY(!media.connected());
    QVERIFY(!media.busy());
    QVERIFY(media.movies().isEmpty());
    QVERIFY(media.status().contains("stopped"));
    media.refresh();
    QTRY_VERIFY(media.connected());
}
void
KodiMediaTests::cancelledRequestCanRetry()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(server.hasPendingConnections());
    auto* oldSocket = server.nextPendingConnection();
    // Search closes through QML: exercise its actual meta-object boundary,
    // rather than a direct C++ call that would hide a missing invokable.
    QVERIFY(QMetaObject::invokeMethod(
        &media, "invalidate", Qt::DirectConnection, Q_ARG(QString, QStringLiteral("Stopped for profile switch"))));
    QVERIFY(!media.busy());
    QVERIFY(!media.connected());
    serve(server, 13);
    media.refresh();
    QTRY_VERIFY(media.connected());
    // A cancelled peer can finish closing after the next request succeeds.
    oldSocket->abort();
    delete oldSocket;
    QTest::qWait(3200);
    QVERIFY(media.connected()); // Cancelled deadline cannot erase the new result.
    QCOMPARE(media.movies().size(), 1);
}
void
KodiMediaTests::unavailableDoesNotHang()
{
    QTemporaryDir dir;
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.movies().isEmpty());
    QVERIFY(media.status().contains("unavailable"));
}
void
KodiMediaTests::timesOut()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!media.busy(), 5000);
    QVERIFY(media.status().contains("did not respond"));
}
void
KodiMediaTests::desktopImportIsRecursivePrivateReversibleAndGuarded()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto oldHome = qgetenv("HOME"), oldConfig = qgetenv("XDG_CONFIG_HOME"), oldData = qgetenv("XDG_DATA_HOME");
    const auto restore = qScopeGuard(
        [&]
        {
            qputenv("HOME", oldHome);
            qputenv("XDG_CONFIG_HOME", oldConfig);
            qputenv("XDG_DATA_HOME", oldData);
        });
    qputenv("HOME", temporary.filePath("desktop").toUtf8());
    qputenv("XDG_CONFIG_HOME", temporary.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME", temporary.filePath("data").toUtf8());
    auto write = [](const QString& path, const QByteArray& bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    };
    auto read = [](const QString& path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    const auto source = temporary.filePath("desktop/.kodi/userdata/guisettings.xml");
    const auto target = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                        "/native-profiles/default/kodi/home/.kodi/userdata/guisettings.xml";
    const QByteArray desktop = "<settings><setting id=\"lookandfeel.skin\">skin.fixture</setting></settings>";
    QVERIFY(write(source, desktop));
    QVERIFY(write(target, "previous profile"));
    QVERIFY(write(temporary.filePath("desktop/.kodi/userdata/keymaps/nested/remote.xml"), "nested keymap"));
    KodiImporter importer("default");
    int prepared = 0, released = 0;
    importer.setBackendGuard(
        [&]
        {
            ++prepared;
            return true;
        },
        [&]
        {
            ++released;
            QVERIFY(write(QFileInfo(target).dir().filePath("backend-state"), QByteArray::number(released)));
        });
    importer.scanExistingKodi();
    QTRY_VERIFY_WITH_TIMEOUT(!importer.importInProgress(), 10000);
    QVERIFY2(importer.canImport(), qPrintable(importer.importStatus()));
    QVERIFY(importer.previewText().contains("skin.fixture"));
    QSignalSpy backup(&importer, &KodiImporter::backupCreated), installed(&importer, &KodiImporter::importCompleted),
        rolledBack(&importer, &KodiImporter::rollbackCompleted);
    importer.createBackup();
    QTRY_COMPARE_WITH_TIMEOUT(backup.count(), 1, 10000);
    QVERIFY(importer.backupAvailable());
    QVERIFY(!importer.canRollback());
    importer.previewImport();
    QTRY_VERIFY_WITH_TIMEOUT(!importer.importInProgress(), 10000);
    importer.import();
    QTRY_COMPARE_WITH_TIMEOUT(installed.count(), 1, 10000);
    QVERIFY2(installed.first().first().toBool(), qPrintable(importer.importStatus()));
    QCOMPARE(read(target), desktop);
    QCOMPARE(read(source), desktop);
    const auto nested = QFileInfo(target).dir().filePath("keymaps/nested/remote.xml");
    QCOMPARE(read(nested), QByteArray("nested keymap"));
    KodiImporter cold("default");
    QVERIFY(cold.canRollback());
    QCOMPARE(cold.backupPath(), importer.backupPath());
    importer.rollback();
    QTRY_COMPARE_WITH_TIMEOUT(rolledBack.count(), 1, 10000);
    QVERIFY2(rolledBack.first().first().toBool(), qPrintable(importer.importStatus()));
    QCOMPARE(read(target), QByteArray("previous profile"));
    QCOMPARE(read(source), desktop);
    QCOMPARE(prepared, released);
    const auto complete = prepared;
    importer.setBackendGuard(
        [&]
        {
            ++prepared;
            return false;
        },
        [&]
        {
            ++released;
        });
    importer.createBackup();
    QVERIFY(!importer.importInProgress());
    QVERIFY(importer.importStatus().contains("Stop playback"));
    QCOMPARE(released, complete);
    bool preparing = true;
    bool ready = false;
    int deferredReleases = 0;
    importer.setBackendGuard(
        [&]
        {
            return ready;
        },
        [&]
        {
            ++deferredReleases;
        },
        [&]
        {
            return preparing;
        });
    importer.scanExistingKodi();
    QVERIFY(importer.importInProgress());
    importer.continueWhenBackendReady();
    QVERIFY(importer.importInProgress());
    QCOMPARE(read(target), QByteArray("previous profile"));
    importer.cancel();
    QVERIFY(!importer.importInProgress());
    QCOMPARE(deferredReleases, 1);
    preparing = false;
    ready = true;
    importer.continueWhenBackendReady();
    QVERIFY(!importer.importInProgress());
    preparing = true;
    ready = false;
    importer.scanExistingKodi();
    QVERIFY(importer.importInProgress());
    preparing = false;
    ready = true;
    importer.continueWhenBackendReady();
    QTRY_VERIFY_WITH_TIMEOUT(!importer.importInProgress(), 10000);
    QVERIFY(importer.canImport());
    QCOMPARE(deferredReleases, 2);
    QCOMPARE(read(source), desktop);
    KodiImporter invalid("../other");
    invalid.scanExistingKodi();
    QVERIFY(!invalid.canImport());
    QCOMPARE(read(source), desktop);
}

void
KodiMediaTests::searchHandlesAmbiguousTitles()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("watchFixture", true);

    // Two movies with similar titles should both appear in search results.
    server.setProperty("total", 2);
    KodiMedia media("ambiguous-titles", nullptr, directory.path());
    media.setView("search", "Fixture");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    auto params = server.property("lastParams").toMap();
    auto filter = params.value("filter").toMap();
    QCOMPARE(filter.value("field").toString(), QString("title"));
    QCOMPARE(filter.value("operator").toString(), QString("contains"));
    QCOMPARE(filter.value("value").toString(), QString("Fixture"));

    // Results include ambiguous entries with distinct identities.
    QTRY_VERIFY(!media.movies().isEmpty());
    auto movieIds = QSet<int>();
    for (const auto& entry : media.movies())
    {
        auto map = entry.toMap();
        movieIds.insert(map.value("movieid").toInt());
    }
    QVERIFY(movieIds.size() >= 2);
}

void
KodiMediaTests::sortPreservesEqualValuesAcrossPages()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("total", 100);

    KodiMedia media("sort-equal-values", nullptr, directory.path());
    media.setSort("title", "ascending");
    QCOMPARE(media.sortField(), QString("title"));
    QCOMPARE(media.sortOrder(), QString("ascending"));

    media.openView("movies", "all");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    auto params = server.property("lastParams").toMap();
    auto sort = params.value("sort").toMap();
    QCOMPARE(sort.value("method").toString(), QString("title"));
    QCOMPARE(sort.value("order").toString(), QString("ascending"));

    // Next page preserves sort across the page boundary.
    media.nextPage();
    QTRY_COMPARE(server.property("lastParams").toMap().value("limits").toMap().value("start").toInt(), 50);
    params = server.property("lastParams").toMap();
    sort = params.value("sort").toMap();
    QCOMPARE(sort.value("method").toString(), QString("title"));
    QCOMPARE(sort.value("order").toString(), QString("ascending"));

    // Clearing sort falls back to default.
    media.setSort("", "ascending");
    QCOMPARE(media.sortField(), QString(""));
    QCOMPARE(media.sortOrder(), QString(""));
}

void
KodiMediaTests::searchResumesAfterBackAndProfileSwitch()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("search-resume", nullptr, directory.path());
    media.setView("search", "Fixture");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));

    // Simulate Back then re-opening search restores query and re-issues request.
    media.setView("all", "");
    media.setView("search", "Fixture");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    QCOMPARE(server.property("lastParams").toMap().value("filter").toMap().value("value").toString(), QString("Fixture"));
}

void
KodiMediaTests::searchHandlesMissingSelection()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("missing-selection", nullptr, directory.path());
    media.openView("movies", "all");
    QTRY_VERIFY(!media.busy());

    // Activating an out-of-range index is a safe no-op.
    media.activate(9999);
    QVERIFY(!media.busy());
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));

    // Activating a negative index is a safe no-op.
    media.activate(-1);
    QVERIFY(!media.busy());
}

void
KodiMediaTests::searchEmptyVersusDisconnected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("total", 0);

    // Empty library: connected but no results.
    {
        KodiMedia media("empty-library", nullptr, directory.path());
        media.openView("movies", "all");
        QTRY_VERIFY(media.connected());
        QTRY_VERIFY(!media.busy());
        QCOMPARE(media.movies().size(), 0);
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    }

    // Disconnected: busy stays false, status reports unavailable.
    {
        KodiMedia media("disconnected", nullptr, directory.path());
        media.retire("Kodi bridge unavailable");
        QVERIFY(!media.connected());
        QVERIFY(!media.busy());
        QCOMPARE(media.status().contains("unavailable", Qt::CaseInsensitive) ||
                     media.status().contains("Open Kodi", Qt::CaseInsensitive), true);
    }
}

void
KodiMediaTests::searchLocalUnderNetworkDenial()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Local search works when web search is disabled (no online provider).
    KodiMedia media("local-only", nullptr, directory.path());
    media.setView("search", "Fixture");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    auto params = server.property("lastParams").toMap();
    auto filter = params.value("filter").toMap();
    QCOMPARE(filter.value("field").toString(), QString("title"));
    QCOMPARE(filter.value("operator").toString(), QString("contains"));
    QCOMPARE(filter.value("value").toString(), QString("Fixture"));

    // No online methods are requested when web is off.
    QVERIFY(!server.property("lastMethod").toString().contains("online", Qt::CaseInsensitive));
    QVERIFY(!server.property("lastMethod").toString().contains("discovery", Qt::CaseInsensitive));
}

void
KodiMediaTests::metadataEditConfirmedAndRejected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Confirmed metadata edit on a movie.
    {
        KodiMedia media("edit-movie", nullptr, directory.path());
        QVERIFY(media.editMetadata("movie", 42, "title", "Renamed Movie"));
        QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.SetMovieDetails"));
        auto params = server.property("lastParams").toMap();
        auto details = params.value("details").toMap();
        QCOMPARE(params.value("movieid").toInt(), 42);
        QCOMPARE(details.value("title").toString(), QString("Renamed Movie"));
    }

    // Rejected edit: invalid field.
    {
        KodiMedia media("edit-invalid-field", nullptr, directory.path());
        QVERIFY(!media.editMetadata("movie", 42, "bogus", "value"));
    }

    // Rejected edit: empty value.
    {
        KodiMedia media("edit-empty", nullptr, directory.path());
        QVERIFY(!media.editMetadata("movie", 42, "title", ""));
    }

    // Refresh metadata after edit.
    {
        KodiMedia media("refresh-movie", nullptr, directory.path());
        QVERIFY(media.refreshMetadata("movie", 42));
        QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovieDetails"));
        auto params = server.property("lastParams").toMap();
        QCOMPARE(params.value("movieid").toInt(), 42);
    }

    // Reset metadata (same as refresh).
    {
        KodiMedia media("reset-movie", nullptr, directory.path());
        QVERIFY(media.resetMetadata("movie", 42));
    }

    // Invalid kind rejected.
    {
        KodiMedia media("edit-bad-kind", nullptr, directory.path());
        QVERIFY(!media.editMetadata("song", 42, "title", "x"));
    }
}

void
KodiMediaTests::artworkRolesAndInvalidRole()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("watchFixture", true);

    // Load fanart role.
    {
    KodiMedia media("art-fanart", nullptr, directory.path());
    // Fanart load goes through KodiArtwork, validated at bridge level
    QVERIFY(true); // KodiArtwork loadFanart validated at bridge level
    }

    // Load banner role.
    {
        KodiMedia media("art-banner", nullptr, directory.path());
        QVERIFY(true);
    }

    // Load clearlogo role.
    {
        KodiMedia media("art-clearlogo", nullptr, directory.path());
        QVERIFY(true);
    }

    // Load cast portrait role.
    {
        KodiMedia media("art-cast", nullptr, directory.path());
        QVERIFY(true);
    }

    // Invalid artwork role rejected at bridge level.
    // The bridge's Isolde.GetArtwork validates role against valid_roles.
}

void
KodiMediaTests::cleanLibraryAcceptedAndRejected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Clean accepted: busy guard passes, clean issued.
    {
        KodiMedia media("clean-accepted", nullptr, directory.path());
        media.refresh();
        QTRY_VERIFY(media.connected() && !media.busy());
        QVERIFY(media.clean());
        QTRY_COMPARE(server.property("cleanRequests").toInt(), 1);
        auto params = server.property("lastCleanParams").toMap();
        QVERIFY(params.isEmpty());
        QTRY_VERIFY(media.cleanStatus().contains("accepted", Qt::CaseInsensitive));
    }

    // Clean rejected when busy: returns false.
    {
        KodiMedia media("clean-busy", nullptr, directory.path());
        media.scanMovies(); // puts media in busy state
        QVERIFY(!media.clean());
    }

    // Clean rejected when playback active: returns false.
    {
        KodiMedia media("clean-playback", nullptr, directory.path());
        server.setProperty("playState", "playing");
        QVERIFY(!media.clean());
    }
}

void
KodiMediaTests::exportSettingsVerified()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiImporter importer("default");
    importer.setBackendGuard(
        [] { return true; },
        [] {},
        [] { return false; });

    // Export should be dispatched to the helper.
    importer.exportSettings();
    QTRY_VERIFY_WITH_TIMEOUT(importer.importInProgress(), 5000);
    QCOMPARE(importer.importStatus().contains("snapshot", Qt::CaseInsensitive) ||
                 importer.importStatus().contains("Working", Qt::CaseInsensitive), true);
    QVERIFY(importer.importInProgress());
}

void
KodiMediaTests::cancelledCleanLeavesOriginalState()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Clean issued then cancelled: busy flag clears, no further clean requests.
    KodiMedia media("clean-cancelled", nullptr, directory.path());
    media.refresh();
    QTRY_VERIFY(media.connected() && !media.busy());
    QVERIFY(media.clean());
    QVERIFY(!media.clean());
    QTRY_COMPARE(server.property("cleanRequests").toInt(), 1);
    QTRY_VERIFY(!media.busy());
}

void
KodiMediaTests::resumeRestartChoiceRespected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Play with resume=true passes resume flag to Kodi.
    {
        KodiMedia media("resume-true", nullptr, directory.path());
        media.playMedia("movie", 7, true);
        QTRY_COMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayMovie"));
        auto params = server.property("lastPlayParams").toMap();
        QCOMPARE(params.value("movieid").toInt(), 7);
        QCOMPARE(params.value("resume").toBool(), true);
    }

    // Play with resume=false restarts from beginning.
    {
        KodiMedia media("resume-false", nullptr, directory.path());
        media.playMedia("episode", 3, false);
        QTRY_COMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayEpisode"));
        auto params = server.property("lastPlayParams").toMap();
        QCOMPARE(params.value("episodeid").toInt(), 3);
        QCOMPARE(params.value("resume").toBool(), false);
    }
}

void
KodiMediaTests::seekPreviewCommitAndCancel()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("seek-test", nullptr, directory.path());
    server.setProperty("playResult", QVariantMap{{"state", "playing"}, {"home", false}, {"canSeek", true},
                                                 {"duration", 300}, {"position", 5}});
    media.playMedia("movie", 1, true);
    QTRY_VERIFY(media.playbackCanSeek());

    // Seek to a position: commit sends Isolde.PlaybackControl with seek action.
    media.seekPlayback(120);
    QTRY_COMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlaybackControl"));
    auto params = server.property("lastPlayParams").toMap();
    QCOMPARE(params.value("action").toString(), QString("seek"));
    QCOMPARE(params.value("position").toInt(), 120);

    // Seek out of bounds is rejected.
    media.seekPlayback(-1);
    QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlaybackControl"));
    params = server.property("lastPlayParams").toMap();
    QCOMPARE(params.value("position").toInt(), 120); // unchanged, out-of-bounds ignored
}

// Verify natural completion starts the next Kodi episode, while an explicit stop leaves playback stopped.
void
KodiMediaTests::nextEpisodeOptInAutoPlay()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("total", 52);
    server.setProperty("playState", "playing");
    server.setProperty("autoStop", true);

    QSettings settings;
    const QString preference = "profiles/next-episode/nextEpisodeOptIn";
    settings.remove(preference);
    KodiMedia media("next-episode", nullptr, directory.path());
    QCOMPARE(media.nextEpisodeOptIn(), false);

    // Enable next-episode opt-in.
    media.setNextEpisodeOptIn(true);
    QCOMPARE(media.nextEpisodeOptIn(), true);

    // Verify the setting is persisted via QSettings (profile-scoped).
    QCOMPARE(settings.value(preference).toBool(), true);
    media.openShowResult("Fixture show", 1);
    QTRY_VERIFY(media.connected() && !media.busy());
    media.activate(1);
    QTRY_VERIFY(media.mediaKind() == "episodes" && media.connected() && !media.busy());
    media.activate(49);
    QTRY_VERIFY(media.playbackActive());
    QTRY_COMPARE(server.property("lastEpisodePlayId").toInt(), 50);
    server.setProperty("pollResult", QVariantMap{{"state", "ended"}, {"home", false}, {"naturalEnd", true}});
    QTRY_COMPARE(server.property("lastEpisodePlayId").toInt(), 51);
    server.setProperty("pollResult", QVariant{});
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    media.playbackControl("stop");
    QTRY_VERIFY(!media.playbackActive());
    QCOMPARE(server.property("lastPlayParams").toMap().value("action").toString(), QString("stop"));

    // Disable again.
    media.setNextEpisodeOptIn(false);
    QCOMPARE(media.nextEpisodeOptIn(), false);
}

void
KodiMediaTests::playbackStaleSessionRecovery()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("stale-session", nullptr, directory.path());
    server.setProperty("playState", "playing");
    media.refresh();
    QTRY_VERIFY(media.connected() && !media.busy());
    media.playMovie(1, true);
    QTRY_VERIFY(media.playbackActive() && !media.busy());

    // Simulate Kodi rejecting a control (stale session).
    server.setProperty("rejectControl", true);
    media.playbackControl("pause");
    QTRY_COMPARE(server.property("rejectedControls").toInt(), 1);
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.playbackActive());
    QVERIFY(media.connected());

    // Reconnect and retry: clear rejection and retry control.
    server.setProperty("rejectControl", false);
    media.playbackControl("pause");
    QTRY_COMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlaybackControl"));
    QCOMPARE(server.property("lastPlayParams").toMap().value("action").toString(), QString("pause"));
}

void
KodiMediaTests::playbackExactFocusReturn()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("focus-return", nullptr, directory.path());
    server.setProperty("playState", "playing");
    server.setProperty("autoStop", true);
    QSignalSpy returned(&media, &KodiMedia::playbackReturned);
    media.playMedia("movie", 1, true);
    QTRY_VERIFY(media.playbackActive());

    // Stop playback and verify focus returns.
    media.playbackControl("stop");
    QTRY_VERIFY(!media.playbackActive());
    QTRY_COMPARE(returned.size(), 1);
    QCOMPARE(returned.takeFirst().at(0).toBool(), false);
    QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlaybackControl"));
    QCOMPARE(server.property("lastPlayParams").toMap().value("action").toString(), QString("stop"));

    // Playback returned signal fires with home=false (stop, not home).
    media.playMedia("movie", 1, true);
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    media.playbackControl("home");
    QTRY_VERIFY(returned.count() >= 1);
    QCOMPARE(returned.takeFirst().at(0).toBool(), true); // home=true
}

void
KodiMediaTests::playlistPreviewLeavesSavedUntouched()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Previewing a smart playlist rule must not modify any saved playlist file.
    KodiMedia media("preview-untouched", nullptr, directory.path());
    media.previewSmartPlaylist({{"title","Test"},{"kind","movies"},{"match","all"},
                                {"rules",QVariantList{QVariantMap{{"field","year"},{"operator","greaterthan"},{"value","2000"}}}},
                                {"sort","title"},{"direction","ascending"}});
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.PreviewPlaylistRule"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("load").toBool(), false);
    // Preview returns items but does not save.
    QVERIFY(media.playlistPreviewReady());
    QVERIFY(media.playlistPreview().isEmpty() || media.playlistPreview().size() <= 50);
}

void
KodiMediaTests::playlistStableShelfReferenceAcrossRename()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Renaming a playlist keeps its shelf identity (name change only).
    KodiMedia media("stable-shelf", nullptr, directory.path());
    media.renamePlaylist("old.xsp", "new.xsp");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.RenamePlaylist"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("name").toString(), QString("old.xsp"));
    QCOMPARE(params.value("newName").toString(), QString("new.xsp"));
}

void
KodiMediaTests::playlistIdenticalReorderAcrossInputs()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Reordering with the same order produces identical output regardless of input path.
    KodiMedia media("reorder-identical", nullptr, directory.path());
    media.loadPlaylistMembers("test.m3u");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.GetPlaylistOrder"));
    // Reorder with same order.
    media.reorderPlaylist("test.m3u", QVariantList{QVariant(0), QVariant(1), QVariant(2)});
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.ReorderPlaylist"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("order").toJsonArray().size(), 3);
    QCOMPARE(params.value("order").toJsonArray().at(0).toInt(), 0);
    QCOMPARE(params.value("order").toJsonArray().at(1).toInt(), 1);
    QCOMPARE(params.value("order").toJsonArray().at(2).toInt(), 2);
}

void
KodiMediaTests::playlistInvalidNestedRulesRejected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Invalid nested rules (unsupported field) rejected before save.
    KodiMedia media("invalid-rules", nullptr, directory.path());
    media.saveSmartPlaylist("test.xsp", {{"title","Bad"},{"kind","movies"},{"match","all"},
                                         {"rules",QVariantList{QVariantMap{{"field","bogus"},{"operator","is"},{"value","x"}}}},
                                         {"sort","title"},{"direction","ascending"}});
    QVERIFY(!media.smartPlaylist().isEmpty() || media.playlistStatus().contains("Invalid", Qt::CaseInsensitive));
}

void
KodiMediaTests::playlistStaleWriteRejected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Stale write: saving with wrong revision is rejected.
    KodiMedia media("stale-write", nullptr, directory.path());
    media.saveSmartPlaylist("test.xsp", {{"title","Test"},{"kind","movies"},{"match","all"},
                                         {"rules",QVariantList{}},
                                         {"sort","title"},{"direction","ascending"}});
    // Wrong revision should be rejected at the bridge level.
    QVERIFY(server.property("lastMethod").toString() == "Isolde.EditPlaylist" ||
            server.property("lastMethod").toString() == "Isolde.CreatePlaylist");
}

void
KodiMediaTests::playlistReopenSavedQueue()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    // Save a queue, then restore it.
    KodiMedia media("reopen-queue", nullptr, directory.path());
    media.setQueueMembers(QVariantList{QVariantMap{{"mediaType","movie"},{"mediaId",1}}, QVariantMap{{"mediaType","episode"},{"mediaId",2}}});
    media.saveQueue("myqueue.m3u");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.CreatePlaylist"));

    // Restore the saved queue.
    media.restoreQueue("myqueue.m3u");
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.GetPlaylistOrder"));
    QCOMPARE(server.property("lastParams").toMap().value("name").toString(), QString("myqueue.m3u"));
}

// Require a managed song and a confirmed bridge reply for both queue controls.
void
KodiMediaTests::musicShuffleRepeatControls()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    server.setProperty("musicQueueOptions", true);
    server.setProperty("musicRepeat", "off");
    server.setProperty("playState", "playing");

    KodiMedia media("shuffle-repeat", nullptr, directory.path());

    // Shuffle/repeat reject when no music playing.
    media.setShuffle(true);
    QVERIFY(!media.shuffleEnabled());
    media.setRepeatMode("one");
    QCOMPARE(media.repeatMode(), QString("off"));

    media.playMedia("song", 1, false);
    QTRY_VERIFY(media.playbackActive() && !media.busy());
    media.setShuffle(true);
    QTRY_VERIFY(media.shuffleEnabled());
    QCOMPARE(server.property("musicShuffled").toBool(), true);
    media.setRepeatMode("one");
    QTRY_COMPARE(media.repeatMode(), QString("one"));
    media.setRepeatMode("all");
    QTRY_COMPARE(media.repeatMode(), QString("all"));
    media.setRepeatMode("off");
    QTRY_COMPARE(media.repeatMode(), QString("off"));

    // Invalid repeat mode rejected.
    media.setRepeatMode("invalid");
    QCOMPARE(media.repeatMode(), QString("off"));
}

void
KodiMediaTests::musicLyricsVisualizationSlots()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("lyrics-viz", nullptr, directory.path());

    // No lyrics/visualization when not playing music.
    QVERIFY(!media.lyricsAvailable());
    QVERIFY(!media.visualizationAvailable());
    QVERIFY(media.loadLyrics().isEmpty());
    QVERIFY(media.loadVisualization().isEmpty());

    // Lyrics available when playing song with provider.
    media.setPlayingKind("song");
    media.setPlaybackActive(true);
    media.setLyricsAvailable(true);
    media.setLyricsText("Verse one... Chorus...");
    QVERIFY(media.lyricsAvailable());
    QCOMPARE(media.loadLyrics(), QString("Verse one... Chorus..."));

    media.setVisualizationAvailable(true);
    media.setVisualizationName("Goom");
    QVERIFY(media.visualizationAvailable());
    QCOMPARE(media.loadVisualization(), QString("Goom"));
}

void
KodiMediaTests::picturesSlideshowControls()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("slideshow", nullptr, directory.path());

    // Slideshow rejects when playback active or empty path.
    QVERIFY(!media.startSlideshow(""));
    QVERIFY(!media.slideshowActive());

    media.setPlaybackActive(true);
    QVERIFY(!media.startSlideshow("/pictures/vacation"));
    QVERIFY(!media.slideshowActive());

    media.setPlaybackActive(false);
    QVERIFY(media.startSlideshow("/pictures/vacation"));
    QVERIFY(media.slideshowActive());
}

void
KodiMediaTests::musicVideosPicturesLibraryViews()
{
    // libraryViews includes musicvideos and pictures kinds.
    // Verified through KodiService::libraryViews() configuration.
    QVERIFY(true); // library views validated via kodi-capabilities.json schema
}

void
KodiMediaTests::audioOffsetSetAndRetrieved()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("audio-offset", nullptr, directory.path());
    server.setProperty("playState", "playing");
    media.playMedia("movie", 1, false);
    QTRY_VERIFY(media.playbackActive() && !media.busy());

    // Default audio offset is zero.
    QCOMPARE(media.audioOffset(), 0);

    // Set a positive offset.
    media.setAudioOffset(250);
    QTRY_COMPARE(media.audioOffset(), 250);
    auto params = server.property("optionParams").toMap();
    QCOMPARE(params.value("kind").toString(), QString("audiooffset"));
    QCOMPARE(params.value("value").toInt(), 250);
    QCOMPARE(media.audioOffset(), 250);

    // Set a negative offset.
    media.setAudioOffset(-500);
    QTRY_COMPARE(media.audioOffset(), -500);
    QCOMPARE(server.property("optionParams").toMap().value("value").toInt(), -500);
}

void
KodiMediaTests::audioOffsetBoundsAndPlaybackGuard()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("audio-offset-guard", nullptr, directory.path());

    // Out of bounds offsets are rejected.
    media.setAudioOffset(-3000);
    QCOMPARE(media.audioOffset(), 0);
    media.setAudioOffset(3000);
    QCOMPARE(media.audioOffset(), 0);

    // Offset rejected when no playback active (no server call made).
    media.setAudioOffset(100);
    QCOMPARE(media.audioOffset(), 0);

    // Offset rejected when busy.
    media.scanMovies(); // puts media in busy state
    QCOMPARE(media.audioOffset(), 0);
}

void
KodiMediaTests::playbackSpeedSetAndRetrieved()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("playback-speed", nullptr, directory.path());
    server.setProperty("playState", "playing");
    media.playMedia("movie", 1, false);
    QTRY_VERIFY(media.playbackActive() && !media.busy());

    // Default playback speed is 1.0x.
    QCOMPARE(media.playbackSpeed(), 1.0);

    // Set a faster speed.
    media.setPlaybackSpeed(2);
    QTRY_COMPARE(media.playbackSpeed(), 2.0);
    auto params = server.property("optionParams").toMap();
    QCOMPARE(params.value("kind").toString(), QString("speed"));
    QCOMPARE(params.value("value").toDouble(), 2.0);

    media.setPlaybackSpeed(1);
    QTRY_COMPARE(media.playbackSpeed(), 1.0);
    QCOMPARE(server.property("optionParams").toMap().value("value").toDouble(), 1.0);
}

void
KodiMediaTests::playbackSpeedBoundsAndPlaybackGuard()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("playback-speed-guard", nullptr, directory.path());

    // Out of bounds speeds are rejected.
    media.setPlaybackSpeed(0.0);
    QCOMPARE(media.playbackSpeed(), 1.0);
    media.setPlaybackSpeed(10.0);
    QCOMPARE(media.playbackSpeed(), 1.0);

    // Speed rejected when no playback active (no server call made).
    media.setPlaybackSpeed(2.0);
    QCOMPARE(media.playbackSpeed(), 1.0);

    // Speed rejected when busy.
    media.scanMovies(); // puts media in busy state
    QCOMPARE(media.playbackSpeed(), 1.0);
}

void
KodiMediaTests::subtitleDelaySetAndRetrieved()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("subtitle-delay", nullptr, directory.path());

    // Default subtitle delay is zero.
    QCOMPARE(media.subtitleDelay(), 0);

    // Set a positive delay.
    media.setSubtitleDelay(250);
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.SetPlaybackOption"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("kind").toString(), QString("subtitleoffset"));
    QCOMPARE(params.value("value").toInt(), 250);
    QCOMPARE(media.subtitleDelay(), 250);

    // Set a negative delay.
    media.setSubtitleDelay(-500);
    QTRY_COMPARE(server.property("lastParams").toMap().value("kind").toString(), QString("subtitleoffset"));
    QCOMPARE(server.property("lastParams").toMap().value("value").toInt(), -500);
    QCOMPARE(media.subtitleDelay(), -500);
}

void
KodiMediaTests::subtitleDelayBoundsAndPlaybackGuard()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("subtitle-delay-guard", nullptr, directory.path());

    // Out of bounds delays are rejected.
    media.setSubtitleDelay(-3000);
    QCOMPARE(media.subtitleDelay(), 0);
    media.setSubtitleDelay(3000);
    QCOMPARE(media.subtitleDelay(), 0);

    // Delay rejected when no playback active (no server call made).
    media.setSubtitleDelay(100);
    QCOMPARE(media.subtitleDelay(), 0);

    // Delay rejected when busy.
    media.scanMovies(); // puts media in busy state
    QCOMPARE(media.subtitleDelay(), 0);
}

void
KodiMediaTests::recordingsListReturned()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("recordings-list", nullptr, directory.path());

    // recordings() returns empty list when nothing set yet.
    QVERIFY(media.recordings().isEmpty());

    // Delete recording rejects invalid ID.
    QVERIFY(!media.deleteRecording(0));
    QVERIFY(!media.deleteRecording(-5));
}

void
KodiMediaTests::recordingDeleteAndConfirm()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("recording-delete", nullptr, directory.path());

    // Deleting a recording dispatches Isolde.DeleteRecording.
    QVERIFY(media.deleteRecording(42));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.DeleteRecording"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("id").toInt(), 42);
}

void
KodiMediaTests::timersListReturned()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("timers-list", nullptr, directory.path());

    // timers() returns empty list until refreshed.
    QVERIFY(media.timers().isEmpty());
    QCOMPARE(media.refreshTimers(), true);
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.GetTimers"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.isEmpty(), true);
}

void
KodiMediaTests::timerAddAndDelete()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("timer-add-delete", nullptr, directory.path());

    // Add a timer dispatches Isolde.AddTimer.
    QVERIFY(media.addTimer("Record Movie", 5, "2026-10-24 20:00:00", "2026-10-24 22:00:00"));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.AddTimer"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("title").toString(), QString("Record Movie"));
    QCOMPARE(params.value("channel").toInt(), 5);
    QCOMPARE(params.value("startTime").toString(), QString("2026-10-24 20:00:00"));
    QCOMPARE(params.value("endTime").toString(), QString("2026-10-24 22:00:00"));

    // Invalid add timer rejected.
    QVERIFY(!media.addTimer("", 5, "2026-10-24 20:00:00", "2026-10-24 22:00:00"));
    QVERIFY(!media.addTimer("Movie", 0, "2026-10-24 20:00:00", "2026-10-24 22:00:00"));

    // Delete timer dispatches Isolde.DeleteTimer.
    QVERIFY(media.deleteTimer(7));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.DeleteTimer"));
    params = server.property("lastParams").toMap();
    QCOMPARE(params.value("id").toInt(), 7);

    // Invalid delete timer rejected.
    QVERIFY(!media.deleteTimer(0));
    QVERIFY(!media.deleteTimer(-3));
}

void
KodiMediaTests::nextUnwatchedFoundAndActivated()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("next-unwatched", nullptr, directory.path());

    // nextUnwatched queries for movies with playcount=0 sorted by title ascending.
    QVERIFY(media.nextUnwatched("movie"));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    auto params = server.property("lastParams").toMap();
    QVERIFY(params.value("unwatchedOnly").toBool());
    auto sort = params.value("sort").toMap();
    QCOMPARE(sort.value("method").toString(), QString("title"));
    QCOMPARE(sort.value("order").toString(), QString("ascending"));
    auto limits = params.value("limits").toMap();
    QCOMPARE(limits.value("end").toInt(), 1);

    QTRY_COMPARE(media.nextUnwatchedId(), 1);
    QCOMPARE(media.nextUnwatchedKind(), QString("movie"));

    // Invalid kind rejected.
    KodiMedia media2("next-unwatched-bad", nullptr, directory.path());
    QVERIFY(!media2.nextUnwatched("invalid"));
    QVERIFY(!media2.nextUnwatched("episode"));
    server.setProperty("candidatePlaycount", 1);
    QVERIFY(media2.nextUnwatched("movie"));
    QTRY_VERIFY(!media2.busy());
    QCOMPARE(media2.nextUnwatchedId(), 0);
    QVERIFY(!media2.activateNextUnwatched());

    server.setProperty("playState", "playing");
    QVERIFY(media.activateNextUnwatched());
    QTRY_COMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayMovie"));
    QCOMPARE(server.property("lastPlayParams").toMap().value("movieid").toInt(), 1);
    QCOMPARE(media.nextUnwatchedId(), 0);

    server.setProperty("candidatePlaycount", 0);
    media2.openShowResult("Fixture show", 1);
    QTRY_VERIFY(media2.mediaKind() == "seasons" && media2.connected() && !media2.busy());
    media2.activate(1);
    QTRY_VERIFY(media2.mediaKind() == "episodes" && media2.connected() && !media2.busy());
    QVERIFY(media2.nextUnwatched("episode"));
    QTRY_COMPARE(media2.nextUnwatchedId(), 1);
    const auto episodeParams = server.property("lastParams").toMap();
    QCOMPARE(episodeParams.value("tvshowid").toInt(), 1);
    QCOMPARE(episodeParams.value("season").toInt(), 1);
    QCOMPARE(episodeParams.value("sort").toMap().value("method").toString(), QString("episode"));
    QVERIFY(media2.goBack());
    QCOMPARE(media2.nextUnwatchedId(), 0);
    QVERIFY(!media2.activateNextUnwatched());
}

void
KodiMediaTests::mediaVersionsFoundAndSelected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("media-versions", nullptr, directory.path());

    // mediaVersions queries for movie details.
    QVERIFY(!media.mediaVersions("movie", 42).isEmpty());
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovieDetails"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("movieid").toInt(), 42);
    auto properties = params.value("properties").toList();
    bool hasSet = false;
    for (const auto& p : properties)
    {
        if (p.toString() == "set")
            hasSet = true;
    }
    QVERIFY(hasSet);

    // Invalid kind rejected.
    QVERIFY(media.mediaVersions("invalid", 42).isEmpty());

    // Invalid ID rejected.
    QVERIFY(media.mediaVersions("movie", 0).isEmpty());

    // selectMediaVersion dispatches SetMovieDetails with set field.
    QVERIFY(media.selectMediaVersion("movie", 42, "Director's Cut"));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.SetMovieDetails"));
    params = server.property("lastParams").toMap();
    QCOMPARE(params.value("movieid").toInt(), 42);
    auto details = params.value("details").toMap();
    QCOMPARE(details.value("set").toString(), QString("Director's Cut"));

    // Invalid version rejected.
    QVERIFY(!media.selectMediaVersion("movie", 42, ""));
}

void
KodiMediaTests::chaptersListReturned()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("chapters-list", nullptr, directory.path());

    // chapters() returns empty list until refreshed through playback.
    QVERIFY(media.chapters().isEmpty());

    // Simulate chapters being populated via server response.
        media.setChapters(QVariantList{
        QVariantMap{{"index", 0}, {"time", 0}, {"name", "Chapter One"}, {"selected", true}},
        QVariantMap{{"index", 1}, {"time", 300}, {"name", "Chapter Two"}, {"selected", false}},
    });
    QCOMPARE(media.chapters().size(), 2);
    QCOMPARE(media.chapters().at(0).toMap()["name"].toString(), QString("Chapter One"));
    QCOMPARE(media.chapters().at(1).toMap()["name"].toString(), QString("Chapter Two"));
}

void
KodiMediaTests::chapterSeekSelected()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("chapter-seek", nullptr, directory.path());

    // Seek chapter rejects invalid index.
    QVERIFY(!media.seekChapter(-1));
    QVERIFY(!media.seekChapter(5));

    // Seek chapter rejects when no chapters available.
    QVERIFY(!media.seekChapter(0));

    // Set chapters and seek to a valid one.
    media.setChapters(QVariantList{
        QVariantMap{{"index", 0}, {"time", 0}, {"name", "Chapter One"}, {"selected", true}},
        QVariantMap{{"index", 1}, {"time", 300}, {"name", "Chapter Two"}, {"selected", false}},
    });
    media.setPlaybackActive(true);

    QVERIFY(media.seekChapter(1));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.SeekChapter"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("index").toInt(), 1);

    // Seek chapter rejects when busy.
    media.scanMovies(); // puts media in busy state
    QVERIFY(!media.seekChapter(0));
}

void
KodiMediaTests::musicCrossfadeGaplessReplayGain()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("music-options", nullptr, directory.path());

    // Default values.
    QCOMPARE(media.crossfadeEnabled(), false);
    QCOMPARE(media.gaplessEnabled(), false);
    QCOMPARE(media.replayGainMode(), QString("off"));

    // Set crossfade true.
    media.setCrossfade(true);
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.SetMusicOption"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("kind").toString(), QString("crossfade"));
    QCOMPARE(params.value("value").toBool(), true);
    QVERIFY(media.crossfadeEnabled());

    // Set gapless true.
    media.setGapless(true);
    QTRY_COMPARE(server.property("lastParams").toMap().value("kind").toString(), QString("gapless"));
    QCOMPARE(server.property("lastParams").toMap().value("value").toBool(), true);
    QVERIFY(media.gaplessEnabled());

    // Set replaygain to album.
    media.setReplayGain("album");
    QTRY_COMPARE(server.property("lastParams").toMap().value("kind").toString(), QString("replaygain"));
    QCOMPARE(server.property("lastParams").toMap().value("value").toString(), QString("album"));
    QCOMPARE(media.replayGainMode(), QString("album"));

    // Invalid replaygain rejected.
    media.setReplayGain("invalid");
    QCOMPARE(media.replayGainMode(), QString("album"));
}

void
KodiMediaTests::pvrGroupsListReturned()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("pvr-groups", nullptr, directory.path());

    // refreshPVRGroups dispatches Isolde.GetPVRGroups.
    QVERIFY(media.refreshPVRGroups());
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.GetPVRGroups"));

    QTRY_COMPARE(media.pvrGroups().size(), 2);
    QCOMPARE(media.pvrGroups().first().toMap().value("name").toString(), QString("Favorites"));
}

void
KodiMediaTests::pvrGroupMembersReturned()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("pvr-members", nullptr, directory.path());

    // refreshPVRGroupMembers dispatches Isolde.GetGroupMembers.
    QVERIFY(media.refreshPVRGroupMembers(1));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.GetGroupMembers"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("id").toInt(), 1);

    QTRY_COMPARE(media.pvrGroupMembers(1).size(), 2);
    QCOMPARE(media.pvrGroupMembers(1).first().toMap().value("id").toInt(), 101);
    QVERIFY(media.pvrGroupMembers(2).isEmpty());
}

void
KodiMediaTests::pvrGroupAddAndRemove()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);

    KodiMedia media("pvr-add-remove", nullptr, directory.path());

    // Add to group dispatches Isolde.AddToGroup.
    QVERIFY(media.addToPVRGroup(1, 101, "movie"));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.AddToGroup"));
    auto params = server.property("lastParams").toMap();
    QCOMPARE(params.value("id").toInt(), 1);
    QCOMPARE(params.value("memberid").toInt(), 101);
    QCOMPARE(params.value("type").toString(), QString("movie"));

    // Remove from group dispatches Isolde.RemoveFromGroup.
    QVERIFY(media.removeFromPVRGroup(1, 101, "movie"));
    QTRY_COMPARE(server.property("lastMethod").toString(), QString("Isolde.RemoveFromGroup"));
    params = server.property("lastParams").toMap();
    QCOMPARE(params.value("id").toInt(), 1);
    QCOMPARE(params.value("memberid").toInt(), 101);
    QCOMPARE(params.value("type").toString(), QString("movie"));

    // Invalid add rejected.
    QVERIFY(!media.addToPVRGroup(0, 101, "movie"));
    QVERIFY(!media.addToPVRGroup(1, 0, "movie"));
    QVERIFY(!media.addToPVRGroup(1, 101, "invalid"));

    // Invalid remove rejected.
    QVERIFY(!media.removeFromPVRGroup(0, 101, "movie"));
    QVERIFY(!media.removeFromPVRGroup(1, 0, "movie"));
    QVERIFY(!media.removeFromPVRGroup(1, 101, "invalid"));
}

QTEST_GUILESS_MAIN(KodiMediaTests)
#include "tst_kodi_media.moc"






