#include "media/KodiMedia.h"
#include "media/KodiShelves.h"
#include "media/KodiArtwork.h"
#include <QImage>
#include <QBuffer>
#include "media/KodiService.h"
#include "media/MediaGrants.h"
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QLocalServer>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtTest>

class KodiMediaTests : public QObject {
    Q_OBJECT
private slots:
    void startupPreferenceIsPerProfile();
    void frontendPreferenceRoundTrip();
    void mediaFolderPermissions();
    void readsLibraryThroughTwoRequests();
    void savedKodiPlaylistCatalogAndShelf();
    void recentlyViewedMergesKodiHistoryAndStaysBounded();
    void historyCountAndRetirement();
    void selectedAppHistoryMergeAndOffline();
    void musicAlbumsTracksAndKodiPlayback();
    void tvGuideReadsChannelsAndKeepsUnavailableStateHonest();
    void pagesLibraryAndResets();
    void playbackLifecycle();
    void scheduledPlaybackUsesExplicitBroadcastOffset();
    void scheduledUnavailableAcknowledgementUsesOnlyOwnedStartIdentity();
    void localViewsAndCollections();
    void manualMovieCollections();
    void manualShowCollections();
    void episodeSearchAndShowResult();
    void homeCollectionShelves();
    void validatesPersistedCollections();
    void televisionHierarchyAndEpisodePlayback();
    void artworkSelectionCancellation();
    void rejectsUnknownVersion();
    void unavailableDoesNotHang();
    void timesOut();
    void invalidationClearsConnectedLibrary();
    void cancelledRequestCanRetry();
    void scansConfiguredSources();
    void cancelsScanMonitoring();
    void observedScanCompletes();
    void rejectedScanIsNotSuccess();
};
void KodiMediaTests::mediaFolderPermissions()
{
    QTemporaryDir config, media;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        KodiService service("folder-test");
        service.setEnabled(false);
        service.addLibraryFolder(QUrl::fromLocalFile(media.path()));
        QVERIFY(service.libraryFolders().isEmpty());
        service.setFrontend("kodi");
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
        for (const auto &path : QStringList{"/", "/home", "/etc", "/proc", "/tmp", QDir::homePath(), QDir::homePath() + "/.ssh"})
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

void KodiMediaTests::frontendPreferenceRoundTrip()
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

void KodiMediaTests::startupPreferenceIsPerProfile()
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

static void serve(QLocalServer &server, int version)
{
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, version] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, version, &server] {
                const auto bytes = socket->property("input").toByteArray() + socket->readAll();
                socket->setProperty("input", bytes);
                if (!bytes.endsWith('\n')) return;
                const auto request = QJsonDocument::fromJson(bytes).object();
                QJsonValue result;
                if (request.value("method") == "JSONRPC.Version")
                    result = QJsonObject{{"version", QJsonObject{{"major", version}}}};
                else if (request.value("method").toString().startsWith("Isolde.Play")) {
                    server.setProperty("lastPlayMethod", request.value("method").toString());
                    server.setProperty("lastPlayParams", request.value("params").toObject().toVariantMap());
                    server.setProperty("playRequests", server.property("playRequests").toInt() + 1);
                    result = QJsonObject{{"state", server.property("playState").toString()}, {"home", server.property("playHome").toBool()}};
                    const auto overrideKey=request.value("method")=="Isolde.PlaybackStatus"?"pollResult":"playResult";
                    if(server.property(overrideKey).isValid())result=QJsonObject::fromVariantMap(server.property(overrideKey).toMap());
                }
                else if (request.value("method") == "Isolde.GetPlaylists") {
                    result=QJsonObject{{"playlists",QJsonArray{QJsonObject{{"playlistid",1},{"title","Family films"},{"playlistName","Family films.xsp"}}}},{"limits",QJsonObject{{"start",0},{"end",1},{"total",1}}}};
                }
                else if (request.value("method") == "Isolde.CreatePlaylist") {
                    const auto params=request.value("params").toObject();server.setProperty("savedPlaylist",params.toVariantMap());
                    result=QJsonObject{{"saved",!server.property("rejectPlaylist").toBool()},{"name",params["name"]},{"kind",params["kind"]},{"playlistName","Isolde-12345678.xsp"}};
                }
                else if (request.value("method") == "Isolde.GetPlaylistItems") {
                    server.setProperty("playlistParams",request.value("params").toObject().toVariantMap());
                    const auto params=request.value("params").toObject();const int start=params["start"].toInt();
                    result=QJsonObject{{"movies",QJsonArray{QJsonObject{{"movieid",start+41},{"title","Local film"},{"playlistMediaType","movie"}}}},{"limits",QJsonObject{{"start",start},{"end",start+1},{"total",51}}}};
                }
                else if (request.value("method") == "VideoLibrary.Scan") {
                    result = server.property("rejectScan").toBool() ? "rejected" : "OK";
                    server.setProperty("scanRequests", server.property("scanRequests").toInt() + 1);
                } else if (request.value("method") == "XBMC.GetInfoBooleans") {
                    const bool scanning = server.property("scanning").toBool();
                    server.setProperty("scanning", false);
                    result = QJsonObject{{"Library.IsScanningVideo", scanning}};
                }
                else if (request.value("method") == "VideoLibrary.GetMovieDetails") {
                    const int movieId = request.value("params").toObject().value("movieid").toInt();
                    server.setProperty("detailRequests", server.property("detailRequests").toInt() + 1);
                    result = QJsonObject{{"moviedetails", QJsonObject{{"movieid", movieId},
                        {"title", QString("Fixture movie %1").arg(movieId)}, {"year", 2024}}}};
                } else if (request.value("method") == "VideoLibrary.GetTVShowDetails") {
                    const int showId = request.value("params").toObject().value("tvshowid").toInt();
                    server.setProperty("detailRequests", server.property("detailRequests").toInt() + 1);
                    result = QJsonObject{{"tvshowdetails", QJsonObject{{"tvshowid", showId},
                        {"title", QString("Fixture show %1").arg(showId)}, {"episode", 4}, {"watchedepisodes", 1}}}};
                } else {
                    server.setProperty("libraryRequests", server.property("libraryRequests").toInt() + 1);
                    const auto params = request.value("params").toObject();
                    const auto method = request.value("method").toString();
                    server.setProperty("lastMethod", method);
                    server.setProperty("lastParams", params.toVariantMap());
                    if(server.property("historyFixture").toBool()) {
                        auto calls=server.property("historyRequests").toList();calls.append(QVariantMap{{"method",method},{"params",params.toVariantMap()}});server.setProperty("historyRequests",calls);
                    }
                    const int start = params.value("limits").toObject().value("start").toInt();
                    const int total = method == "VideoLibrary.GetSeasons" ? 2 : server.property("total").isValid() ? server.property("total").toInt() : 1;
                    QJsonArray movies;
                    const auto listKey = method == "VideoLibrary.GetTVShows" ? "tvshows" : method == "VideoLibrary.GetSeasons" ? "seasons" : method == "VideoLibrary.GetEpisodes" ? "episodes" : "movies";
                    const auto idKey = method == "VideoLibrary.GetTVShows" ? "tvshowid" : method == "VideoLibrary.GetSeasons" ? "seasonid" : method == "VideoLibrary.GetEpisodes" ? "episodeid" : "movieid";
                    for (int i = start; i < qMin(params.value("limits").toObject().value("end").toInt(), total); ++i) {
                        QJsonObject entry{{idKey, i + 1}, {"title", "Fixture movie"}};
                        if(server.property("historyFixture").toBool()) {
                            entry["dateadded"]="2026-10-24 00:00:00";
                            entry["lastplayed"]=i==2?(method=="VideoLibrary.GetEpisodes"?"2026-02-30 00:00:00":""):i==0?(method=="VideoLibrary.GetEpisodes"?"2026-10-23 12:00:00":"2026-10-20 12:00:00"):i==1?"2026-10-22 12:00:00":"2026-10-21 12:00:00";
                        }
                        if (method == "VideoLibrary.GetSeasons") { entry["season"] = i; entry.remove("title"); }
                        if (method == "VideoLibrary.GetEpisodes") { entry["season"] = params.value("season").toInt(); entry["episode"] = i + 1; }
                        movies.append(entry);
                    }
                    server.setProperty("lastStart", start);
                    server.setProperty("lastEnd", params.value("limits").toObject().value("end").toInt());
                    result = QJsonObject{{listKey, movies}, {"limits", QJsonObject{{"start", start},
                        {"end", start + movies.size()}, {"total", server.property("badPaging").toBool() ? -1 : total}}}};
                }
                socket->write(QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request.value("id")}, {"result", result}}).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            });
        }
    });
}
void KodiMediaTests::savedKodiPlaylistCatalogAndShelf()
{
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server,13);
    KodiMedia catalog("playlist-profile",nullptr,dir.path());catalog.openPlaylists();QTRY_VERIFY(catalog.connected());
    QCOMPARE(catalog.items().size(),1);QCOMPARE(catalog.items()[0].toMap()["playlistName"].toString(),QString("Family films.xsp"));
    KodiMedia shelf("playlist-profile",nullptr,dir.path());shelf.openView("movies","playlist","Family films.xsp");QTRY_VERIFY(shelf.connected());
    QCOMPARE(shelf.items()[0].toMap()["mediaType"].toString(),QString("movie"));QCOMPARE(shelf.items()[0].toMap()["mediaId"].toInt(),41);
    QCOMPARE(server.property("playlistParams").toMap()["name"].toString(),QString("Family films.xsp"));
    shelf.nextPage();QTRY_COMPARE(shelf.pageStart(),50);QCOMPARE(shelf.items()[0].toMap()["mediaId"].toInt(),91);
    const auto before=shelf.items();shelf.openView("movies","playlist","../secret.xsp");QCOMPARE(shelf.items(),before);
    shelf.refresh();QTRY_VERIFY(!shelf.busy());QCOMPARE(shelf.viewKind(),QString("playlist"));
    shelf.openView("movies","genre","Adventure");QTRY_VERIFY(!shelf.busy());
    const auto library=shelf.items();QSignalSpy saved(&shelf,&KodiMedia::playlistSaved);
    shelf.savePlaylist("Family & friends");QTRY_COMPARE(saved.size(),1);
    QCOMPARE(server.property("savedPlaylist").toMap()["view"].toString(),QString("genre"));
    QCOMPARE(server.property("savedPlaylist").toMap()["value"].toString(),QString("Adventure"));
    QCOMPARE(shelf.items(),library);QVERIFY(shelf.playlistStatus().contains("saved"));
    server.setProperty("rejectPlaylist",true);shelf.savePlaylist("Refused");QTRY_VERIFY(!shelf.busy());
    QCOMPARE(saved.size(),1);QCOMPARE(shelf.items(),library);QVERIFY(shelf.playlistStatus().contains("invalid"));
    catalog.invalidate("Profile closed");QVERIFY(catalog.items().isEmpty());
}
void KodiMediaTests::recentlyViewedMergesKodiHistoryAndStaysBounded()
{
    QTemporaryDir dir; QLocalServer server; QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server,13); server.setProperty("historyFixture",true);server.setProperty("total",3);
    KodiMedia media("history-test",nullptr,dir.path());media.openView("history","watched");
    QTRY_VERIFY(media.connected());QVERIFY(!media.busy());QCOMPARE(media.items().size(),4);
    const auto items=media.items();
    QCOMPARE(items[0].toMap()["mediaType"].toString(),QString("episode"));QCOMPARE(items[0].toMap()["mediaId"].toInt(),1);
    QCOMPARE(items[1].toMap()["mediaType"].toString(),QString("episode"));QCOMPARE(items[1].toMap()["mediaId"].toInt(),2);
    QCOMPARE(items[2].toMap()["mediaType"].toString(),QString("movie"));QCOMPARE(items[2].toMap()["mediaId"].toInt(),2);
    QCOMPARE(items[3].toMap()["mediaType"].toString(),QString("movie"));QCOMPARE(items[3].toMap()["mediaId"].toInt(),1);
    const auto calls=server.property("historyRequests").toList();QCOMPARE(calls.size(),2);
    for(const auto &call:calls){const auto params=call.toMap()["params"].toMap();
        QCOMPARE(params["limits"].toMap()["end"].toInt(),50);
        QCOMPARE(params["sort"].toMap()["method"].toString(),QString("lastplayed"));
        QCOMPARE(params["filter"].toMap()["field"].toString(),QString("lastplayed"));
        QVERIFY(params["properties"].toList().contains("lastplayed"));
    }
    QVERIFY(!media.hasNextPage());media.nextPage();QTest::qWait(30);QCOMPARE(server.property("historyRequests").toList().size(),2);
    server.setProperty("total",60);media.refresh();QTRY_VERIFY(!media.busy());QVERIFY(media.connected());QCOMPARE(media.items().size(),50);QVERIFY(!media.hasNextPage());
    server.setProperty("total",0);media.refresh();QTRY_VERIFY(!media.busy());QVERIFY(media.connected());QVERIFY(media.items().isEmpty());QVERIFY(media.status().contains("Nothing viewed"));
    media.refresh();media.invalidate("Profile closed");QTest::qWait(30);QVERIFY(!media.connected());QVERIFY(media.items().isEmpty());QCOMPARE(media.status(),QString("Profile closed"));
}
void KodiMediaTests::historyCountAndRetirement()
{
    QTemporaryDir dir; QLocalServer server; QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server,13);server.setProperty("historyFixture",true);server.setProperty("total",60);
    KodiMedia one("one",nullptr,dir.path()),many("one",nullptr,dir.path());
    one.openHistory(1);many.openHistory(7);
    QTRY_VERIFY(one.connected());QTRY_VERIFY(many.connected());
    QCOMPARE(one.items().size(),1);QCOMPARE(many.items().size(),7);
    QCOMPARE(one.items()[0].toMap()["mediaType"].toString(),QString("episode"));
    for(const auto &call:server.property("historyRequests").toList()) {
        const auto params=call.toMap()["params"].toMap();
        QVERIFY((QList<int>{1,7}.contains(params["limits"].toMap()["end"].toInt())));
    }
    const int reads=server.property("libraryRequests").toInt();
    one.openHistory(0);one.openHistory(51);QTest::qWait(30);
    QCOMPARE(server.property("libraryRequests").toInt(),reads);QCOMPARE(one.items().size(),1);
    one.invalidate("Bridge offline");one.refresh();QTRY_VERIFY(one.connected());
    one.retire("Profile closed");const int retiredReads=server.property("libraryRequests").toInt();
    one.refresh();one.openHistory(5);one.openView("movies","all");one.playMedia("movie",1);
    QTest::qWait(100);QVERIFY(one.items().isEmpty());QVERIFY(!one.connected());
    QCOMPARE(server.property("libraryRequests").toInt(),retiredReads);
    // A request already in flight must not publish after teardown either.
    many.refresh();many.retire("Profile closed");QTest::qWait(100);
    QVERIFY(many.items().isEmpty());QVERIFY(!many.connected());QVERIFY(!many.busy());
}
void KodiMediaTests::selectedAppHistoryMergeAndOffline()
{
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server,13);server.setProperty("historyFixture",true);server.setProperty("total",3);
    const QVariantMap app{{"providerId","isolde-app-openings"},{"mediaType","app"},{"mediaId","fixture"},{"appId","fixture"},
        {"title","Fixture movie"},{"viewedAt","2026-10-23 12:00:00"}};
    KodiMedia history("merged",nullptr,dir.path());history.setAppOpenings({app});history.openHistory(5);
    QTRY_VERIFY(history.connected());QCOMPARE(history.items().size(),5);
    QCOMPARE(history.items()[0].toMap()["providerId"].toString(),QString("isolde-app-openings"));
    QCOMPARE(history.items()[1].toMap()["providerId"].toString(),QString("kodi"));
    history.selectIndex(2);const auto selected=history.items()[2].toMap();
    auto another=app;another["appId"]="another";another["mediaId"]="another";
    history.setAppOpenings({app,another});history.refresh();QTRY_VERIFY(!history.busy());
    QCOMPARE(history.selectedIndex(),3);QCOMPARE(history.items()[history.selectedIndex()].toMap()["mediaId"],selected["mediaId"]);
    QCOMPARE(history.items()[history.selectedIndex()].toMap()["mediaType"],selected["mediaType"]);
    history.setAppOpenings({app});history.refresh();QTRY_VERIFY(!history.busy());
    // Equal titles and equal timestamps from different providers remain distinct.
    history.setAppOpenings({app,app});history.refresh();QTRY_VERIFY(!history.busy());QCOMPARE(history.items().size(),5);
    auto invalid=app;invalid["viewedAt"]="no date";history.setAppOpenings({invalid});
    history.refresh();QTRY_VERIFY(!history.busy());QCOMPARE(history.items().size(),5);
    history.setAppOpenings({});history.refresh();QTRY_VERIFY(!history.busy());QCOMPARE(history.items().size(),4);
    history.setAppOpenings({app});server.setProperty("badPaging",true);history.refresh();QTRY_VERIFY(!history.busy());
    QVERIFY(!history.connected());QCOMPARE(history.items().size(),1);QVERIFY(history.status().contains("invalid"));
    server.setProperty("badPaging",false);history.refresh();QTRY_VERIFY(history.connected());QCOMPARE(history.items().size(),5);
    history.retire("Profile closed");QVERIFY(history.items().isEmpty());
}
void KodiMediaTests::musicAlbumsTracksAndKodiPlayback()
{
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));QList<QJsonObject> calls;
    connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{
            auto bytes=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",bytes);if(!bytes.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(bytes).object();calls.append(request);const auto method=request["method"].toString();QJsonObject result;
            if(method=="JSONRPC.Version")result={{"version",QJsonObject{{"major",13}}}};
            else if(method=="AudioLibrary.GetAlbums" || method=="AudioLibrary.GetSongs"){
                const bool albums=method.endsWith("Albums");const auto params=request["params"].toObject();QCOMPARE(params["limits"].toObject()["end"].toInt(),50);
                if(!albums)QCOMPARE(params["filter"].toObject()["albumid"].toInt(),41);
                const QJsonObject item{{albums?"albumid":"songid",albums?41:23},{"title",albums?"An actual Kodi album":"First track"},{"artist",QJsonArray{"Composer"}},{"duration",120}};
                result={{albums?"albums":"songs",QJsonArray{item}},{"limits",QJsonObject{{"start",0},{"end",1},{"total",1}}}};
            } else if(method=="Isolde.PlaySong") {QCOMPARE(request["params"].toObject()["songid"].toInt(),23);QVERIFY(!request["params"].toObject()["resume"].toBool());result={{"state","playing"},{"home",false}};}
            else result={{"state",method=="Isolde.PlaybackControl"?"ended":"playing"},{"home",false}};
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request["id"]},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });}});
    KodiMedia media("music-profile",nullptr,dir.path());media.openView("albums","all");QTRY_VERIFY(media.connected());
    QCOMPARE(media.items().first().toMap()["mediaType"].toString(),QString("album"));QCOMPARE(media.browseTitle(),QString("Music · Albums"));
    const auto before=calls.size();media.openView("albums","continue");QTest::qWait(20);QCOMPARE(calls.size(),before);
    media.activate(0);QTRY_VERIFY(!media.busy());QVERIFY(media.connected());QCOMPARE(media.mediaKind(),QString("songs"));QCOMPARE(media.browseTitle(),QString("An actual Kodi album"));
    QCOMPARE(media.items().first().toMap()["mediaType"].toString(),QString("song"));media.playSelected(true);QTRY_VERIFY(media.playbackActive() && !media.busy());
    media.playbackControl("stop");QTRY_VERIFY(!media.playbackActive());QVERIFY(media.goBack());QTRY_VERIFY(!media.busy());QCOMPARE(media.mediaKind(),QString("albums"));QCOMPARE(media.selectedIndex(),0);
    media.invalidate("Profile closed");QVERIFY(media.items().isEmpty());QVERIFY(!media.connected());
}
void KodiMediaTests::tvGuideReadsChannelsAndKeepsUnavailableStateHonest()
{
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));bool empty=false,reject=false;int plays=0;
    connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{
            auto bytes=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",bytes);if(!bytes.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(bytes).object();const auto method=request["method"].toString();QJsonObject result;
            if(method=="JSONRPC.Version")result={{"version",QJsonObject{{"major",13}}}};
            else if(method=="PVR.GetChannels"){
                const auto params=request["params"].toObject();QCOMPARE(params["channelgroupid"].toString(),QString("alltv"));QCOMPARE(params["limits"].toObject()["end"].toInt(),50);QVERIFY(!params.contains("sort"));
                if(reject){socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request["id"]},{"error",QJsonObject{{"code",-32000},{"message","PVR unavailable"}}}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();return;}
                QJsonArray channels;if(!empty)for(int i=1;i<=3;++i)channels.append(QJsonObject{{"channelid",i},{"channelnumber",i},{"label",QString("Channel %1").arg(i)},{"hidden",i==3},{"locked",i==2},{"broadcastnow",QJsonObject{{"title","Current programme"}}},{"broadcastnext",QJsonObject{{"title","Next programme"}}}});
                result={{"channels",channels},{"limits",QJsonObject{{"start",0},{"end",channels.size()},{"total",channels.size()}}}};
            } else if(method=="Isolde.PlayChannel"){++plays;QCOMPARE(request["params"].toObject()["channelid"].toInt(),1);QVERIFY(!request["params"].toObject()["resume"].toBool());result={{"state","playing"},{"home",false}};}
            else result={{"state",method=="Isolde.PlaybackControl"?"ended":"playing"},{"home",false}};
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request["id"]},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });}});
    KodiMedia media("guide-profile",nullptr,dir.path());media.openView("channels","all");QTRY_VERIFY(media.connected());QCOMPARE(media.items().size(),2);QCOMPARE(media.browseTitle(),QString("TV Guide"));
    QCOMPARE(media.items().first().toMap()["mediaType"].toString(),QString("channel"));QVERIFY(media.items().first().toMap()["subtitle"].toString().contains("Next programme"));
    media.activate(0);QTRY_COMPARE(plays,1);QTRY_VERIFY(!media.busy());media.playbackControl("stop");QTRY_VERIFY(!media.playbackActive());
    empty=true;media.refresh();QTRY_VERIFY(!media.busy());QVERIFY(media.connected());QVERIFY(media.items().isEmpty());QVERIFY(media.status().contains("No TV channels"));
    reject=true;media.refresh();QTRY_VERIFY(!media.busy());QVERIFY(!media.connected());QVERIFY(media.status().contains("PVR TV source"));
}
void KodiMediaTests::artworkSelectionCancellation()
{
    QTemporaryDir dir;
    QLocalServer server; QVERIFY(server.listen(dir.filePath("rpc.sock")));
    QImage image(2, 2, QImage::Format_RGB32); image.fill(Qt::blue);
    QByteArray png; QBuffer buffer(&png); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(image.save(&buffer, "PNG"));
    int requests = 0, lastId = 0;
    connect(&server, &QLocalServer::newConnection, &server, [&] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                const auto bytes = socket->property("input").toByteArray() + socket->readAll();
                socket->setProperty("input", bytes);
                if (!bytes.endsWith('\n')) return;
                const auto request = QJsonDocument::fromJson(bytes).object(); ++requests;
                lastId = request["params"].toObject()["id"].toInt();
                const auto result = server.property("bad").toBool() ? QJsonObject{{"mime", "image/png"}, {"data", "not an image"}}
                    : QJsonObject{{"mime", "image/png"}, {"data", QString::fromLatin1(png.toBase64())}};
                socket->write(QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}}).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            });
        }
    });
    KodiArtwork art("test", nullptr, dir.path());
    art.load("movie", 1); art.load("episode", 2);
    QTRY_VERIFY(art.source().startsWith("data:image/png;base64,"));
    QCOMPARE(requests, 1); QCOMPARE(lastId, 2);
    art.load("episode", 3); art.clear(); QTest::qWait(220);
    QVERIFY(art.source().isEmpty()); QCOMPARE(requests, 1);
    server.setProperty("bad", true); art.load("tvshow", 4);
    QTRY_VERIFY(art.status().contains("format unavailable")); QVERIFY(art.source().isEmpty());
    art.load("unknown", 1); QTest::qWait(180); QCOMPARE(requests, 2);
}
void KodiMediaTests::televisionHierarchyAndEpisodePlayback()
{
    QTemporaryDir dir;
    QLocalServer server; QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13); server.setProperty("total", 103);
    KodiMedia media("tv-test", nullptr, dir.path());
    media.openLibrary("shows"); QTRY_VERIFY(media.connected());
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetTVShows"));
    media.nextPage(); QTRY_VERIFY(!media.busy());
    media.selectIndex(2); media.activate(2); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("seasons"));
    QCOMPARE(server.property("lastParams").toMap()["tvshowid"].toInt(), 53);
    QCOMPARE(media.movies()[0].toMap()["title"].toString(), QString("Specials"));
    media.activate(0); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("episodes"));
    QCOMPARE(server.property("lastParams").toMap()["season"].toInt(), 0);
    QCOMPARE(server.property("lastParams").toMap()["tvshowid"].toInt(), 53);
    server.setProperty("playState", "paused");
    media.playSelected(false); QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastPlayMethod").toString(), QString("Isolde.PlayEpisode"));
    QCOMPARE(server.property("lastPlayParams").toMap()["episodeid"].toInt(), 1);
    QCOMPARE(server.property("lastPlayParams").toMap()["resume"].toBool(), false);
    QVERIFY(!media.goBack()); // playback owns the current view
    media.invalidate("Test stop");
    QVERIFY(media.goBack()); QTRY_VERIFY(media.connected());
    QCOMPARE(media.mediaKind(), QString("seasons")); QCOMPARE(media.selectedIndex(), 0);
    QVERIFY(media.goBack()); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("shows")); QCOMPARE(media.pageStart(), 50); QCOMPARE(media.selectedIndex(), 2);
    QVERIFY(!media.goBack());
    media.setView("continue"); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("episodes"));
    QVERIFY(!server.property("lastParams").toMap().contains("tvshowid"));
    QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("inprogress"));
    QVERIFY(media.goBack()); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.pageStart(), 50); QCOMPARE(media.selectedIndex(), 2);
    media.setView("recent"); QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastParams").toMap()["sort"].toMap()["method"].toString(), QString("dateadded"));
    QVERIFY(!server.property("lastParams").toMap().contains("filter"));
    media.openLibrary("shows"); // cancels superseded episode read
    media.openLibrary("movies"); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.mediaKind(), QString("movies"));
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetMovies"));
    media.setView("recent"); QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastParams").toMap()["sort"].toMap()["method"].toString(), QString("dateadded"));
    media.openLibrary("movies"); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.viewKind(), QString("all"));
    QCOMPARE(server.property("lastParams").toMap()["sort"].toMap()["method"].toString(), QString("title"));
}
void KodiMediaTests::pagesLibraryAndResets()
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
    media.nextPage(); media.nextPage(); // duplicate input cannot skip a page
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
void KodiMediaTests::localViewsAndCollections()
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
        media.nextPage(); QTRY_VERIFY(!media.busy());
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
        media.setView("continue"); QTRY_VERIFY(!media.busy());
        QCOMPARE(media.pageStart(), 0);
        params = server.property("lastParams").toMap();
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("inprogress"));
        QCOMPARE(params["sort"].toMap()["method"].toString(), QString("lastplayed"));
        media.setView("watched"); QTRY_VERIFY(!media.busy());
        params = server.property("lastParams").toMap();
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("lastplayed"));
        QCOMPARE(params["filter"].toMap()["operator"].toString(), QString("after"));
        QCOMPARE(params["sort"].toMap()["method"].toString(), QString("lastplayed"));
        QCOMPARE(params["sort"].toMap()["order"].toString(), QString("descending"));
        QCOMPARE(params["limits"].toMap()["end"].toInt(),50);
        media.openCollection(id); QTRY_VERIFY(!media.busy());
        QCOMPARE(media.viewKind(), QString("search"));
        media.setView("tag", "Family");
        media.setView("genre", "Drama"); // superseded read must not leak results
        QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("genre"));
        media.setView("all"); QTRY_VERIFY(!media.busy());
        QVERIFY(!server.property("lastParams").toMap().contains("filter"));
        media.removeCollection(id);
        QVERIFY(restored.collections().isEmpty());
        media.openLibrary("shows"); QTRY_VERIFY(!media.busy());
        media.openView("shows","watched"); QTRY_VERIFY(!media.busy());
        QCOMPARE(media.mediaKind(),QString("episodes"));
        QCOMPARE(server.property("lastMethod").toString(),QString("VideoLibrary.GetEpisodes"));
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(),QString("lastplayed"));
        media.openLibrary("shows"); QTRY_VERIFY(!media.busy());
        media.setView("search", "North"); QTRY_VERIFY(!media.busy());
        params = server.property("lastParams").toMap();
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetTVShows"));
        QCOMPARE(params["filter"].toMap()["field"].toString(), QString("title"));
        QCOMPARE(params["filter"].toMap()["value"].toString(), QString("North"));
        media.saveCollection("North shows");
        QCOMPARE(media.collections().size(), 1);
        const auto showId = media.collections()[0].toMap()["id"].toString();
        QCOMPARE(media.collections()[0].toMap()["media"].toString(), QString("shows"));
        media.setView("genre", "Drama"); QTRY_VERIFY(!media.busy());
        QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetTVShows"));
        QCOMPARE(server.property("lastParams").toMap()["filter"].toMap()["field"].toString(), QString("genre"));
        media.openCollection(showId); QTRY_VERIFY(!media.busy());
        QCOMPARE(media.mediaKind(), QString("shows"));
        QCOMPARE(media.viewKind(), QString("search"));
        QCOMPARE(media.viewValue(), QString("North"));
        media.removeCollection(showId);
        QVERIFY(media.collections().isEmpty());
    }
    QSettings::setDefaultFormat(original);
}
void KodiMediaTests::manualMovieCollections()
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
void KodiMediaTests::manualShowCollections()
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
void KodiMediaTests::episodeSearchAndShowResult()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia episodes("search-episodes", nullptr, directory.path());
    episodes.openView("episodes", "search", "Pilot");
    QTRY_VERIFY(episodes.connected());
    QCOMPARE(episodes.mediaKind(), QString("episodes"));
    QCOMPARE(server.property("lastMethod").toString(), QString("VideoLibrary.GetEpisodes"));
    auto filter = server.property("lastParams").toMap().value("filter").toMap();
    QCOMPARE(filter.value("field").toString(), QString("title"));
    QCOMPARE(filter.value("operator").toString(), QString("contains"));
    QCOMPARE(filter.value("value").toString(), QString("Pilot"));

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
void KodiMediaTests::homeCollectionShelves()
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
        media.setView("search", "Fixture"); QTRY_VERIFY(media.connected());
        media.saveCollection("First"); media.saveCollection("Second");
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
        shelves.refresh(); QTRY_COMPARE(shelves.items().size(), 3);
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
        shelves.setAvailable(true); // duplicate ready notification is not a refresh
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
        media.openLibrary("shows"); QTRY_VERIFY(!media.busy());
        media.setView("search", "Fixture Show"); QTRY_VERIFY(!media.busy());
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
void KodiMediaTests::validatesPersistedCollections()
{
    QTemporaryDir config;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        const QJsonObject valid{{"id", "valid-id"}, {"name", " Family "}, {"kind", "tag"},
            {"value", " Kids "}, {"home", true}, {"count", 999}, {"command", "untrusted"}};
        auto unknown = valid; unknown["id"] = "unknown"; unknown["kind"] = "shell";
        auto duplicateName = valid; duplicateName["id"] = "duplicate-name"; duplicateName["name"] = "family";
        auto invalidId = valid; invalidId["id"] = "../other";
        auto emptyValue = valid; emptyValue["id"] = "empty"; emptyValue["value"] = " ";
        const QJsonObject manual{{"id", "manual-id"}, {"name", "Manual"}, {"kind", "manual"},
            {"media", "movies"}, {"value", ""}, {"members", QJsonArray{1, 2, 3}}};
        auto badManual = manual; badManual["id"] = "bad-manual"; badManual["members"] = "1,2,3";
        const auto bytes = QJsonDocument(QJsonArray{valid, manual, badManual, valid, unknown, duplicateName, invalidId, emptyValue, 42}).toJson();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Isolde", "Collections");
        settings.setValue("profiles/validated/v1", bytes); settings.sync();
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
        settings.setValue("profiles/validated/v1", "broken JSON"); settings.sync();
        QVERIFY(media.collections().isEmpty());
    }
    QSettings::setDefaultFormat(original);
}
void KodiMediaTests::playbackLifecycle()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    serve(server, 13);
    KodiMedia media("default", nullptr, dir.path());
    QSignalSpy started(&media, &KodiMedia::playbackStarted), returned(&media, &KodiMedia::playbackReturned);
    media.refresh(); QTRY_VERIFY(media.connected());
    media.playMovie(999, true);
    QVERIFY(!media.playbackActive());
    server.setProperty("playState", "starting");
    media.playMovie(1, true); media.playMovie(1, true);
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("playRequests").toInt(), 1);
    QVERIFY(media.playbackActive());
    QCOMPARE(started.size(), 0);
    server.setProperty("playState", "playing");
    QTRY_COMPARE(started.size(), 1);
    server.setProperty("playState", "paused");
    media.playbackControl("pause");
    QTRY_COMPARE(media.playbackStatus(), QString("paused"));
    server.setProperty("playState", "ended"); server.setProperty("playHome", true);
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
void KodiMediaTests::scheduledPlaybackUsesExplicitBroadcastOffset()
{
    QTemporaryDir dir; QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock"))); serve(server,13);
    server.setProperty("playState","starting");
    KodiMedia media("guide-profile",nullptr,dir.path());
    QSignalSpy started(&media,&KodiMedia::playbackStarted), returned(&media,&KodiMedia::playbackReturned);
    QVERIFY(!media.playScheduled("channel",1,100));
    QVERIFY(!media.playScheduled("movie",0,100));
    QVERIFY(!media.playScheduled("movie",7,-1));
    QVERIFY(!media.playScheduled("movie",7,86400));
    QVERIFY(media.playScheduled("episode",7,4511));
    QVERIFY(!media.playScheduled("movie",8,0)); // only one playback owner
    QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("lastPlayMethod").toString(),QString("Isolde.PlayScheduled"));
    QCOMPARE(server.property("lastPlayParams").toMap(),QVariantMap({{"mediaType","episode"},{"mediaId",7},{"offsetSeconds",4511}}));
    QCOMPARE(started.size(),0);
    server.setProperty("playState","playing"); QTRY_COMPARE(started.size(),1);
    server.setProperty("playState","ended"); media.playbackControl("stop");
    QTRY_VERIFY(!media.playbackActive()); QCOMPARE(returned.size(),1);
}
void KodiMediaTests::scheduledUnavailableAcknowledgementUsesOnlyOwnedStartIdentity()
{
    const QVariantMap unavailable{{"state","failed"},{"home",false},{"availability","unavailable"},{"mediaType","movie"},{"mediaId",999}};
    QVariantList results{unavailable};
    for(const auto &home:QVariantList{QVariant(),0,QString("false"),true}){auto invalid=unavailable;if(home.isValid())invalid["home"]=home;else invalid.remove("home");results.append(invalid);}
    auto generic=unavailable;generic.remove("availability");results.append(generic);
    auto malformed=unavailable;malformed["availability"]=true;results.append(malformed);
    for(int index=0;index<results.size();++index){
        QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server,13);server.setProperty("playResult",results[index]);
        KodiMedia media("missing",nullptr,dir.path());QSignalSpy missing(&media,&KodiMedia::scheduledMediaUnavailable);
        QVERIFY(media.playScheduled("episode",7,0));QTRY_VERIFY(!media.playbackActive());
        QCOMPARE(missing.size(),index==0?1:0);if(index==0){QCOMPARE(missing[0][0].toString(),QString("episode"));QCOMPARE(missing[0][1].toInt(),7);}
    }
    {
        QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server,13);server.setProperty("playResult",unavailable);
        KodiMedia media("pvr",nullptr,dir.path());QSignalSpy missing(&media,&KodiMedia::scheduledMediaUnavailable);QVERIFY(media.playPvr(7,9,101));QTRY_VERIFY(!media.playbackActive());QVERIFY(missing.isEmpty());
    }
    {
        QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server,13);server.setProperty("playState","playing");server.setProperty("pollResult",unavailable);
        KodiMedia media("late-failure",nullptr,dir.path());QSignalSpy missing(&media,&KodiMedia::scheduledMediaUnavailable);QVERIFY(media.playScheduled("episode",7,0));QTRY_VERIFY(!media.playbackActive());QVERIFY(missing.isEmpty());
    }
    {
        QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server,13);server.setProperty("playResult",unavailable);
        KodiMedia media("invalidated",nullptr,dir.path());QSignalSpy missing(&media,&KodiMedia::scheduledMediaUnavailable);QVERIFY(media.playScheduled("episode",7,0));media.invalidate("Profile closed");QTest::qWait(100);QVERIFY(missing.isEmpty());
    }
}
void KodiMediaTests::readsLibraryThroughTwoRequests()
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
void KodiMediaTests::observedScanCompletes()
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
void KodiMediaTests::rejectedScanIsNotSuccess()
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
void KodiMediaTests::scansConfiguredSources()
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
void KodiMediaTests::cancelsScanMonitoring()
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
void KodiMediaTests::rejectsUnknownVersion()
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
void KodiMediaTests::invalidationClearsConnectedLibrary()
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
void KodiMediaTests::cancelledRequestCanRetry()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(server.hasPendingConnections());
    auto *oldSocket = server.nextPendingConnection();
    media.invalidate("Stopped for profile switch");
    QVERIFY(!media.busy());
    QVERIFY(!media.connected());
    delete oldSocket;
    serve(server, 13);
    media.refresh();
    QTRY_VERIFY(media.connected());
    QTest::qWait(3200);
    QVERIFY(media.connected()); // Cancelled deadline cannot erase the new result.
    QCOMPARE(media.movies().size(), 1);
}
void KodiMediaTests::unavailableDoesNotHang()
{
    QTemporaryDir dir;
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY(!media.busy());
    QVERIFY(media.movies().isEmpty());
    QVERIFY(media.status().contains("unavailable"));
}
void KodiMediaTests::timesOut()
{
    QTemporaryDir dir;
    QLocalServer server;
    QVERIFY(server.listen(dir.filePath("rpc.sock")));
    KodiMedia media("default", nullptr, dir.path());
    media.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!media.busy(), 5000);
    QVERIFY(media.status().contains("did not respond"));
}
QTEST_GUILESS_MAIN(KodiMediaTests)
#include "tst_kodi_media.moc"
