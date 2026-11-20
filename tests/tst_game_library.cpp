#include "../src/app/AppCatalog.h"
#include "../src/games/GameEmulators.h"
#include "../src/games/GameLibrary.h"
#include "../src/home/ActionDispatcher.h"
#include "../src/home/ComponentSchema.h"
#include "../src/home/HomeLayout.h"
#include "../src/home/PageModel.h"
#include "../src/media/CaptureController.h"
#include "../src/media/KodiMedia.h"
#include "../src/media/KodiService.h"
#include "../src/sessions/SessionManager.h"
#include "../src/settings/ProfileManager.h"
#include "tst_component_common.h"

#include <QScopeGuard>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

// Use a managed runtime fixture so tests exercise game registration without launching a provider.
static AppDefinition
gamesRuntime(const QString& id = "psp-test")
{
    AppDefinition app;
    app.id = id;
    app.name = "PSP runtime";
    app.type = "desktop";
    app.presentationMode = "game-hub";
    app.section = "Games";
    app.accent = QColor("#556677");
    app.handledActions = {"open-file"};
    app.launchConfiguration = {{"desktopId", "isolde-test-emulator"},
                               {"actionHandlers", QVariantMap{{"open-file", QVariantMap{{"appendTarget", true}}}}}};
    return app;
}

// Register files relative to the approved fixture root, matching the production data contract.
static QVariantMap
gameRegistration(const QString& root, const QString& target = "game.iso")
{
    return {{"schemaVersion", 1}, {"title", "Local title"}, {"platform", "PSP"},
            {"runtimeId", "psp-test"}, {"root", root}, {"target", target}};
}

class GameLibraryTests : public QObject
{
    Q_OBJECT
  private slots:
    void
    steamGamesUseProviderIdentityAndReviewedImport();
    void
    steamRecoveryWritesRemainBoundToManagerProfileAfterSwitch();
    void
    gamesCuratedPackagesBindThroughRealCatalog();
    void
    emulatorSettingsAreAtomicProfileLocalAndPreserveUnknownData();
    void
    emulatorManifestIsBoundedAndVersioned();
    void
    gamesEditionsKeepOneCardAndSeparateLaunchTargets();
    void
    gamesFolderImportIsReviewedAtomicAndCancelable();
    void
    gamesFolderImportPreservesManualInstallation();
    void
    gamesRelinkPreservesIdentityAndRollsBack();
    void
    gamesContractsRejectExecutableData();
    void
    gamesCatalogIsBoundedAtomicAndProfileLocal();
    void
    gamesLaunchRejectsMissingAndRedirectedTargets();
};

int
main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    GameLibraryTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_game_library.moc"

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::steamGamesUseProviderIdentityAndReviewedImport(){
    QStandardPaths::setTestModeEnabled(true);const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    qApp->setProperty("isoldeProfile",profile);
    const auto parent=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+profile;
    const auto steamapps=parent+"/steam/home/.local/share/Steam/steamapps";
    QVERIFY(QDir().mkpath(steamapps+"/common/Fixture"));
    writeFile(steamapps+"/appmanifest_42.acf","\"AppState\" { \"appid\" \"42\" \"name\" \"Steam fixture\" \"StateFlags\" \"4\" \"installdir\" \"Fixture\" }");
    QTemporaryDir dir;AppRegistry apps;auto steam=gamesRuntime("steam");steam.name="Steam";steam.handledActions.clear();steam.launchConfiguration={{"desktopId","isolde-steam"}};QVERIFY(apps.addApp(steam));
    GameLibrary library(dir.filePath("games.sqlite"),&apps);QVERIFY(library.ready());
    QVERIFY(library.beginSteamImport());QTRY_VERIFY_WITH_TIMEOUT(library.importReady(),10000);QCOMPARE(library.importCount(),1);QVERIFY(library.page().isEmpty());
    library.cancelImport();QVERIFY(library.page().isEmpty());
    QVERIFY(library.beginSteamImport());QTRY_VERIFY_WITH_TIMEOUT(library.importReady(),10000);QVERIFY(library.commitImport());
    QCOMPARE(library.page().size(),1);const auto id=library.page()[0].toMap().value("id").toString();
    auto registration=library.registration(id);QCOMPARE(registration.value("target").toString(),QString("42"));QVERIFY(registration.value("root").toString().isEmpty());
    const auto provenance=library.details(id).value("acquisitionProvenance").toMap();
    QVERIFY(provenance.value("recorded").toBool());QCOMPARE(provenance.value("sourceKind").toString(),QString("steam-app"));
    QCOMPARE(provenance.value("origin").toString(),QString("steam-import"));
    QCOMPARE(provenance.value("sourceReference").toString(),QString("42"));
    QCOMPARE(library.resolveLaunch(id),QVariantMap({{"type","steam-game"},{"steamAppId","42"}}));
    QVERIFY(library.setFavorite(id,true));registration["title"]="My corrected title";QVERIFY(library.updateGame(id,registration));
    QVERIFY(library.beginSteamImport());QTRY_VERIFY_WITH_TIMEOUT(library.importReady(),10000);QVERIFY(library.commitImport());
    QCOMPARE(library.page().size(),1);QCOMPARE(library.details(id).value("title").toString(),QString("My corrected title"));QVERIFY(library.details(id).value("favorite").toBool());
    {
        SessionManager sessions(&apps);
        KodiMedia media(profile,nullptr,dir.path()); KodiService service(profile);service.setEnabled(false);
        CaptureController capture;
        ActionDispatcher dispatcher(&apps,&sessions,&media,&service,nullptr,&capture);dispatcher.setGames(&library);
        sessions.setLaunchPreparation([](const AppDefinition &){return false;});
        QSignalSpy focus(&dispatcher,&ActionDispatcher::restoreFocusRequested);
        const QVariantMap action{{"type","launch-game"},{"gameId",id}};
        QVERIFY(dispatcher.dispatch(action,"steam-tile"));sessions.goHome();
        QCOMPARE(focus.last().at(0).toString(),QString("steam-tile"));
        QVERIFY(library.page("continue").isEmpty()); // Cancel before any game process.
        QVERIFY(dispatcher.dispatch(action,"steam-tile"));
        emit sessions.steamGameProcessObserved("43");sessions.goHome();
        QVERIFY(library.page("continue").isEmpty());
        QVERIFY(dispatcher.dispatch(action,"steam-tile"));
        emit sessions.steamGameProcessObserved("42");
        emit sessions.steamGameFailed("42","fixture denial");sessions.goHome();
        QVERIFY(library.page("continue").isEmpty());
        QVERIFY(dispatcher.dispatch(action,"steam-tile"));
        emit sessions.steamGameProcessObserved("42");
        qApp->setProperty("isoldeProfile","different-profile");sessions.goHome();
        QVERIFY(library.page("continue").isEmpty());
        qApp->setProperty("isoldeProfile",profile);
        QVERIFY(dispatcher.dispatch(action,"steam-tile"));
        emit sessions.steamGameProcessObserved("42");sessions.goHome();
        QCOMPARE(library.page("continue").size(),1);
        QCOMPARE(library.page("continue").first().toMap().value("id").toString(),id);
    }
    for(const auto &bad:QStringList{"0","01","4294967296","42; touch x","steam://run/42","/tmp/game","42\n"}){auto invalid=registration;invalid["target"]=bad;QVERIFY(ComponentSchema::gameRegistration(invalid).isEmpty());}
    auto invalid=registration;invalid["root"]=steamapps;QVERIFY(ComponentSchema::gameRegistration(invalid).isEmpty());
    steam.launchConfiguration["steamGameId"]="42";QVERIFY(!steam.isValid());
    QVERIFY(ComponentSchema::action({{"type","steam-game"},{"steamAppId","42"}}).isEmpty());
    GameLibrary other(dir.filePath("other/games.sqlite"),&apps);QVERIFY(other.resolveLaunch(id).isEmpty());
    QVERIFY(library.beginSteamImport());library.invalidate();QTRY_VERIFY(!library.scanning());QVERIFY(!library.importReady());QVERIFY(library.resolveLaunch(id).isEmpty());
    QVERIFY(QFileInfo::exists(steamapps+"/appmanifest_42.acf"));
    qApp->setProperty("isoldeProfile",QVariant());QVERIFY(QDir(parent).removeRecursively());
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::steamRecoveryWritesRemainBoundToManagerProfileAfterSwitch(){
    QStandardPaths::setTestModeEnabled(true);
    const auto previous=qApp->property("isoldeProfile");
    const auto ownerProfile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto activeProfile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto cleanup=qScopeGuard([&]{
        qApp->setProperty("isoldeProfile",previous);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+ownerProfile).removeRecursively();
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+activeProfile).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    });
    qApp->setProperty("isoldeProfile",ownerProfile);
    const auto appData=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const auto ownerHome=appData+"/native-profiles/"+ownerProfile+"/steam/home/.local/state/isolde";
    const auto activeHome=appData+"/native-profiles/"+activeProfile+"/steam/home/.local/state/isolde";
    AppRegistry registry;
    {
        SessionManager sessions(&registry);
        qApp->setProperty("isoldeProfile",activeProfile);
        const QVariantMap state{{"version",2},{"appId","42"},{"phase","failed"},{"process","unknown"},
            {"installation","unknown"},{"client","unknown"},{"ownership","unknown"},{"account","unknown"},
            {"connection","unknown"},{"offline","unknown"},{"exit","unknown"},{"local_save","unknown"},
            {"cloud_save","unknown"},{"session_end","incomplete"},{"error","process_observation_failed"}};
        sessions.handleSteamProviderState(state);
        QVERIFY(!sessions.hasActiveSession());
        QCOMPARE(sessions.steamRecoveryStatuses().value("42").toMap().value("session_end").toString(),
                 QString("incomplete"));
        QVERIFY(QFileInfo::exists(ownerHome+"/steam-recovery.json"));
        QVERIFY(!QFileInfo::exists(activeHome+"/steam-recovery.json"));
        const auto journal=QJsonDocument::fromJson(readFile(ownerHome+"/steam-recovery.json")).object()
                               .value("games").toObject().value("42").toObject();
        QVERIFY(!journal.contains("pid"));
        QVERIFY(!journal.contains("pids"));
        const auto pendingBytes=readFile(ownerHome+"/steam-recovery.json");
        auto current=state;
        current.insert("phase","starting");current.insert("session_end","unknown");
        sessions.handleSteamProviderState(current);
        QVERIFY(sessions.steamRecoveryStatuses().contains("42"));
        QCOMPARE(readFile(ownerHome+"/steam-recovery.json"),pendingBytes);
        current.insert("phase","ended");current.insert("session_end","normal");
        sessions.handleSteamProviderState(current);
        QVERIFY(sessions.steamRecoveryStatuses().contains("42"));
        QCOMPARE(readFile(ownerHome+"/steam-recovery.json"),pendingBytes);
        QVERIFY(sessions.acknowledgeSteamRecovery("42"));
        QVERIFY(!sessions.steamRecoveryStatuses().contains("42"));
        QVERIFY(QFileInfo::exists(ownerHome+"/steam-recovery.json"));
        QVERIFY(!QFileInfo::exists(activeHome+"/steam-recovery.json"));
    }
    QVERIFY(QDir(appData+"/native-profiles/"+ownerProfile).removeRecursively());
    QVERIFY(!QFileInfo::exists(activeHome));
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesCuratedPackagesBindThroughRealCatalog() {
    QCoreApplication::setApplicationVersion("0.0.1.2");
    const auto previousProfile=qApp->property("isoldeProfile");
    const auto restoreProfile=qScopeGuard([&]{qApp->setProperty("isoldeProfile",previousProfile);});
    qApp->setProperty("isoldeProfile",QUuid::createUuid().toString(QUuid::WithoutBraces));
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.loadFromData(::apps));AppCatalog catalog(&apps);
    QVERIFY2(catalog.load(QFINDTESTDATA("../config/catalog.json"),dir.filePath("installed.json"),dir.filePath("packages")),qPrintable(catalog.errorString()));
    HomeLayout home(&apps);QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    const auto originalHome=home.committedDocument();QSignalSpy homeChanges(&home,&HomeLayout::layoutChanged);
    connect(&catalog,&AppCatalog::packageInstalled,&home,&HomeLayout::addAppTile);
    const QStringList runtimeIds{"dolphin","pcsx2","ppsspp","retroarch-nes","retroarch-gb","retroarch-gba","retroarch-snes","retroarch-psx","retroarch-ds","retroarch-vb","supertux"};
    for(const auto &id:runtimeIds){
        QVERIFY2(catalog.install(id),qPrintable(catalog.errorString()));
        if(id!="supertux"){QCOMPARE(home.committedDocument(),originalHome);QCOMPARE(homeChanges.count(),0);}
    }
    QVERIFY(apps.addApp(gamesRuntime("watched")));QVERIFY(home.addAppTile("watched"));
    homeChanges.clear();const auto authoredHome=home.committedDocument();
    QVERIFY(apps.removeApp("watched"));QCOMPARE(homeChanges.count(),1);
    QCOMPARE(home.committedDocument(),authoredHome); // Availability refresh writes nothing.
    const auto root=dir.filePath("games");QVERIFY(QDir().mkpath(root));writeFile(root+"/game.iso","inert-security-fixture");
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);QCOMPARE(library.runtimes().size(),runtimeIds.size());
    auto registration=gameRegistration(root);registration["runtimeId"]="dolphin";registration["platform"]="GameCube";
    const auto disc=library.addGame(registration);QVERIFY2(!disc.isEmpty(),qPrintable(library.errorString()));
    QCOMPARE(library.resolveLaunch(disc).value("openWith").toString(),QString("dolphin"));
    QCOMPARE(library.resolveLaunch(disc).value("sourceRoot").toString(),root);
    writeFile(root+"/game.chd","inert");
    auto psx=gameRegistration(root);psx["runtimeId"]="retroarch-psx";psx["platform"]="PlayStation";psx["target"]="game.chd";
    const auto psxGame=library.addGame(psx);QVERIFY2(!psxGame.isEmpty(),qPrintable(library.errorString()));
    QVERIFY(library.resolveLaunch(psxGame).isEmpty());QVERIFY(library.errorString().contains("firmware"));
    auto forged=gamesRuntime("forged");QVERIFY(forged.isValid());forged.launchConfiguration["gameSourceRoot"]=root;
    QVERIFY(!forged.isValid());QVERIFY(!apps.addApp(forged));
    for(const auto &key:{"managedGameRoot","managedGameExecutable","opticalMediaId","opticalReference","opticalObservationId"}){
        auto injected=gamesRuntime("injected");injected.launchConfiguration[key]=root;
        QVERIFY(!injected.isValid());QVERIFY(!apps.addApp(injected));
    }
    registration["runtimeId"]="supertux";registration["root"]=QStringLiteral("");registration["target"]=QStringLiteral("");registration["title"]="SuperTux";registration["platform"]="PC";
    const auto native=library.addGame(registration);QVERIFY2(!native.isEmpty(),qPrintable(library.errorString()));
    QCOMPARE(library.resolveLaunch(native).value("appId").toString(),QString("supertux"));
    QVERIFY(catalog.uninstall("dolphin"));QVERIFY(library.resolveLaunch(disc).isEmpty());
    QVERIFY(!library.details(disc).isEmpty());QCOMPARE(readFile(root+"/game.iso"),QByteArray("inert-security-fixture"));
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::emulatorSettingsAreAtomicProfileLocalAndPreserveUnknownData(){
    QStandardPaths::setTestModeEnabled(true);
    const auto first=QUuid::createUuid().toString(QUuid::WithoutBraces),second=QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString error;const auto home=GameEmulators::home(first,"ppsspp");QVERIFY(!home.isEmpty());
    const auto file=home+"/.config/isolde-emulator/settings.json";
    const auto defaults=GameEmulators::settings(first,"ppsspp",&error);QCOMPARE(defaults.value("video").toString(),QString("automatic"));QVERIFY(!QFileInfo::exists(file));
    QVERIFY(GameEmulators::configure(first,"ppsspp",{{"video","software"},{"controller","xbox"}},{},&error));
    QCOMPARE(GameEmulators::settings(first,"ppsspp",&error).value("video").toString(),QString("software"));
    QCOMPARE(GameEmulators::settings(second,"ppsspp",&error).value("video").toString(),QString("automatic"));
    const auto before=readFile(file);
    QVERIFY(!GameEmulators::configure(first,"ppsspp",{{"video","arbitrary"},{"controller","xbox"}},{},&error));QCOMPARE(readFile(file),before);
    QVERIFY(!GameEmulators::configure(first,"ppsspp",{{"video","automatic"},{"controller","xbox"},{"command","evil"}},{},&error));QCOMPARE(readFile(file),before);
    QVERIFY(!GameEmulators::playStationFirmwareReady(first,&error));QVERIFY(error.contains("firmware"));
    QCOMPARE(GameEmulators::settings(second,"retroarch-psx",&error).value("firmware").toString(),QString());
    writeFile(file,"{\"schemaVersion\":99}");QVERIFY(GameEmulators::settings(first,"ppsspp",&error).isEmpty());
    QVERIFY(!GameEmulators::configure(first,"ppsspp",{{"video","automatic"},{"controller","xbox"}},{},&error));QCOMPARE(readFile(file),QByteArray("{\"schemaVersion\":99}"));
    QVERIFY(GameEmulators::home("../other","ppsspp").isEmpty());QVERIFY(GameEmulators::home(first,"unknown").isEmpty());
    // This unique test profile is under Qt's test storage only.
    QVERIFY(QDir(QFileInfo(home).absolutePath()).removeRecursively());
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::emulatorManifestIsBoundedAndVersioned(){
    const auto runtimes=GameEmulators::definitions();QCOMPARE(runtimes.size(),16);
    QSet<QString> ids,platforms;
    for(const auto &entry:runtimes){
        const auto runtime=entry.toMap();const auto id=runtime.value("id").toString();
        QVERIFY(!id.isEmpty());QVERIFY(!ids.contains(id));ids.insert(id);
        QVERIFY(!runtime.value("packages").toStringList().isEmpty());
        QVERIFY(!runtime.value("formats").toStringList().isEmpty());
        for(const auto &platform:runtime.value("platforms").toStringList())platforms.insert(platform);
    }
    QCOMPARE(platforms,QSet<QString>({"PSP","GameCube","Wii","PlayStation","PlayStation 2","NES","Game Boy","Game Boy Color","Game Boy Advance","SNES","Nintendo DS","Virtual Boy","PlayStation 3","PlayStation 4","PS Vita","Original Xbox","Xbox 360","Wii U"}));
    QVERIFY(GameEmulators::definition("xbox-360").isEmpty());
    QVERIFY(GameEmulators::definition("pcsx2").value("firmwareRequired").toBool());
    QVERIFY(GameEmulators::definition("retroarch-psx").value("firmwareRequired").toBool());
    for(const auto &id:QStringList{"rpcs3","shadps4","vita3k","xemu","xenia","cemu"}){
        const auto route=GameEmulators::definition(id);QVERIFY(!route.isEmpty());
        QCOMPARE(route.value("packages").toStringList(),QStringList{"isolde-"+id});
        QCOMPARE(route.value("setup").toMap().value("videoModes").toStringList(),QStringList{"automatic"});
    }
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesEditionsKeepOneCardAndSeparateLaunchTargets() {
    QTemporaryDir dir;const auto root=dir.filePath("games");QVERIFY(QDir().mkpath(root));writeFile(root+"/game.iso","one");writeFile(root+"/other.iso","two");
    AppRegistry apps;QVERIFY(apps.addApp(gamesRuntime()));const auto path=dir.filePath("profile/games.sqlite");QString game,edition;
    {GameLibrary library(path,&apps);game=library.addGame(gameRegistration(root));QVERIFY(!game.isEmpty());
        QVERIFY(library.setFavorite(game,true));auto second=gameRegistration(root,"other.iso");second["platform"]="PSP alternate";second["title"]="Second edition";
        edition=library.addEdition(game,second);QVERIFY2(!edition.isEmpty(),qPrintable(library.errorString()));
        QVERIFY(QFileInfo::exists(path+".v1-backup.sqlite"));QCOMPARE(library.editions(game).size(),2);QCOMPARE(library.page().size(),1);
        QVERIFY(library.resolveLaunch(game).isEmpty());QCOMPARE(library.resolveLaunch(game,game).value("target").toString(),root+"/game.iso");
        QCOMPARE(library.resolveLaunch(game,edition).value("target").toString(),root+"/other.iso");
        QVERIFY(library.resolveLaunch("game-other",edition).isEmpty());QVERIFY(library.resolveLaunch(game,"edition-forged").isEmpty());
        QVERIFY(library.addEdition(game,second).isEmpty());QVERIFY(library.addGame(second).isEmpty());QVERIFY(!library.updateGame(game,second));
        QCOMPARE(library.registration(game).value("target").toString(),QString("game.iso"));QVERIFY(library.details(game).value("favorite").toBool());
        GameLibrary other(dir.filePath("different/games.sqlite"),&apps);QVERIFY(other.resolveLaunch(game,edition).isEmpty());}
    {GameLibrary reopened(path,&apps);QVERIFY(reopened.ready());QCOMPARE(reopened.editions(game).size(),2);QCOMPARE(reopened.page().size(),1);
        QVERIFY(reopened.removeGame(game));QVERIFY(reopened.editions(game).isEmpty());QVERIFY(reopened.resolveLaunch(game,edition).isEmpty());}
    QCOMPARE(readFile(root+"/game.iso"),QByteArray("one"));QCOMPARE(readFile(root+"/other.iso"),QByteArray("two"));
    {GameLibrary backup(path+".v1-backup.sqlite",&apps);QVERIFY(backup.ready());QCOMPARE(backup.page().size(),1);QCOMPARE(backup.editions(game).size(),1);}
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesFolderImportIsReviewedAtomicAndCancelable() {
    QTemporaryDir dir;const auto root=dir.filePath("games");QVERIFY(QDir().mkpath(root+"/nested"));
    writeFile(root+"/a.iso","a");writeFile(root+"/nested/b.iso","b");writeFile(root+"/ignore.txt","x");
    AppRegistry apps;auto runtime=gamesRuntime();runtime.launchConfiguration["gameFormats"]=QVariantList{"iso"};QVERIFY(apps.addApp(runtime));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    QVERIFY(library.beginImport(root,"psp-test","PSP"));QVERIFY(!library.commitImport());library.cancelImport();
    QVERIFY(!library.scanning());QCOMPARE(library.importCount(),0);QVERIFY(library.page().isEmpty());
    QVERIFY(library.beginImport(root,"psp-test","PSP"));QTRY_VERIFY(library.importReady());
    QCOMPARE(library.importCount(),2);QVERIFY(library.page().isEmpty());
    QVERIFY(QFile::remove(root+"/nested/b.iso"));QVERIFY(!library.commitImport());QVERIFY(library.page().isEmpty());
    writeFile(root+"/nested/b.iso","b");QVERIFY(library.commitImport());QCOMPARE(library.page().size(),2);
    const auto id=library.page()[0].toMap().value("id").toString();auto corrected=library.registration(id);corrected["title"]="Manual correction";QVERIFY(library.updateGame(id,corrected));
    const auto provenance=library.details(id).value("acquisitionProvenance").toMap();
    QVERIFY(provenance.value("recorded").toBool());QCOMPARE(provenance.value("sourceKind").toString(),QString("external-folder"));
    QCOMPARE(provenance.value("origin").toString(),QString("folder-import"));
    QVERIFY(library.beginImport(root,"psp-test","PSP"));QTRY_VERIFY(library.importReady());QVERIFY(library.commitImport());
    QCOMPARE(library.page().size(),2);QCOMPARE(library.details(id).value("title").toString(),QString("Manual correction"));
    QCOMPARE(library.details(id).value("acquisitionProvenance").toMap(),provenance);
    QVERIFY(library.beginImport(root,"psp-test","PSP"));library.invalidate();QVERIFY(!library.scanning());QVERIFY(!library.commitImport());
    QCOMPARE(readFile(root+"/a.iso"),QByteArray("a"));QCOMPARE(readFile(root+"/nested/b.iso"),QByteArray("b"));
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesFolderImportPreservesManualInstallation() {
    QTemporaryDir dir;const auto root=dir.filePath("games");QVERIFY(QDir().mkpath(root));
    writeFile(root+"/a.iso","a");
    AppRegistry apps;auto runtime=gamesRuntime();runtime.launchConfiguration["gameFormats"]=QVariantList{"iso"};QVERIFY(apps.addApp(runtime));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    auto registration=gameRegistration(root,"a.iso");registration["title"]="My manual title";
    const auto id=library.addGame(registration);QVERIFY(!id.isEmpty());QVERIFY(library.setFavorite(id,true));
    const auto provenance=library.details(id).value("acquisitionProvenance").toMap();
    QVERIFY(provenance.value("recorded").toBool());QCOMPARE(provenance.value("origin").toString(),QString("manual-registration"));
    QVERIFY(library.beginImport(root,"psp-test","PSP"));QTRY_VERIFY_WITH_TIMEOUT(library.importReady(),10000);
    QCOMPARE(library.importPage(0).size(),1);QVERIFY(library.commitImport());
    QCOMPARE(library.page().size(),1);QCOMPARE(library.page()[0].toMap().value("id").toString(),id);
    const auto details=library.details(id);QCOMPARE(details.value("title").toString(),QString("My manual title"));
    QVERIFY(details.value("favorite").toBool());
    QCOMPARE(details.value("acquisitionProvenance").toMap(),provenance);
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesRelinkPreservesIdentityAndRollsBack() {
    QTemporaryDir dir;const auto oldRoot=dir.filePath("original"),newRoot=dir.filePath("moved");
    QVERIFY(QDir().mkpath(oldRoot));QVERIFY(QDir().mkpath(newRoot));
    writeFile(oldRoot+"/game.iso","game-content");writeFile(newRoot+"/game.iso","game-content");
    AppRegistry apps;QVERIFY(apps.addApp(gamesRuntime()));GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    const auto id=library.addGame(gameRegistration(oldRoot));QVERIFY(!id.isEmpty());QVERIFY(library.setFavorite(id,true));
    const auto original=library.registration(id);auto edited=original;edited["title"]="My corrected title";edited["root"]=newRoot;
    edited["target"]="missing.iso";QVERIFY(!library.updateGame(id,edited));QCOMPARE(library.registration(id),original);
    edited["target"]="game.iso";QVERIFY(library.updateGame(id,edited));QCOMPARE(library.registration(id),edited);
    QCOMPARE(library.resolveLaunch(id).value("target").toString(),newRoot+"/game.iso");QVERIFY(library.details(id).value("favorite").toBool());
    const auto other=library.addGame(gameRegistration(oldRoot));QVERIFY(!other.isEmpty());
    // Relinking onto another installation fails the unique constraint atomically.
    edited["root"]=oldRoot;edited["title"]="Must not survive conflict";QVERIFY(!library.updateGame(id,edited));
    QCOMPARE(library.details(id).value("title").toString(),QString("My corrected title"));
    QCOMPARE(library.registration(id).value("root").toString(),newRoot);
    GameLibrary differentProfile(dir.filePath("other/games.sqlite"),&apps);QVERIFY(!differentProfile.updateGame(id,edited));
    QCOMPARE(readFile(oldRoot+"/game.iso"),QByteArray("game-content"));QCOMPARE(readFile(newRoot+"/game.iso"),QByteArray("game-content"));
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesContractsRejectExecutableData() {
    QString error;const auto registration=gameRegistration("/tmp/games");
    QVERIFY(!ComponentSchema::gameRegistration(registration,&error).isEmpty());
    auto invalid=registration;invalid["executable"]="/bin/sh";QVERIFY(ComponentSchema::gameRegistration(invalid,&error).isEmpty());
    for(const auto &version:QVariantList{true,QString("1"),2}){invalid=registration;invalid["schemaVersion"]=version;QVERIFY(ComponentSchema::gameRegistration(invalid).isEmpty());}
    for(const auto &target:QStringList{"../game.iso","nested/../../game.iso","/tmp/game.iso","folder/../game.iso","folder\\game.iso"}){
        invalid=registration;invalid["target"]=target;QVERIFY(ComponentSchema::gameRegistration(invalid).isEmpty());
    }
    QVariantMap action{{"schemaVersion",1},{"type","launch-game"},{"gameId","game-valid"}};
    QCOMPARE(ComponentSchema::action(action),action);
    action["arguments"]=QStringList{"--unsafe"};QVERIFY(ComponentSchema::action(action,&error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVariantMap shelf{{"schemaVersion",1},{"id","games-shelf"},{"kind","widget"},{"widgetType","library"},{"componentType","library"},
        {"name","Games"},{"presentation",QVariantMap{{"libraryKind","games"},{"libraryView","continue"}}}};
    QVERIFY(!ComponentSchema::component(shelf,true,&error).isEmpty());
    shelf["presentation"]=QVariantMap{{"libraryKind","games"},{"libraryView","favorites"}};
    QVERIFY(!ComponentSchema::component(shelf,true,&error).isEmpty());
    shelf["presentation"]=QVariantMap{{"libraryKind","games"},{"libraryView","platform"},{"libraryValue","PSP"}};
    const auto canonical=ComponentSchema::component(shelf,true,&error);QVERIFY(!canonical.isEmpty());
    QCOMPARE(ComponentSchema::component(canonical,false,&error),canonical);
    shelf["presentation"]=QVariantMap{{"libraryKind","games"},{"libraryView","platform"},{"libraryValue",""}};
    QVERIFY(ComponentSchema::component(shelf,true,&error).isEmpty());QVERIFY(!error.isEmpty());
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesCatalogIsBoundedAtomicAndProfileLocal() {
    QTemporaryDir dir;QVERIFY(dir.isValid());const auto root=dir.filePath("library");QVERIFY(QDir().mkpath(root));
    const auto content=QByteArray("immutable fixture");writeFile(root+"/game.iso",content);
    AppRegistry registry;QVERIFY(registry.addApp(gamesRuntime()));
    const auto path=dir.filePath("one/games.sqlite");QString id;
    {
        GameLibrary catalog(path,&registry);QVERIFY2(catalog.ready(),qPrintable(catalog.errorString()));
        id=catalog.addGame(gameRegistration(root));QVERIFY2(!id.isEmpty(),qPrintable(catalog.errorString()));
        QCOMPARE(catalog.page().size(),1);QCOMPARE(catalog.details(id)["title"].toString(),QString("Local title"));
        auto duplicate=gameRegistration(root);duplicate["title"]="Do not overwrite";
        QVERIFY(catalog.addGame(duplicate).isEmpty());QCOMPARE(catalog.details(id)["title"].toString(),QString("Local title"));
        QVERIFY(catalog.setFavorite(id,true));QCOMPARE(catalog.page("favorites").size(),1);
        QVERIFY(catalog.page("search","% OR 1=1").isEmpty());QVERIFY(catalog.page("all","",1).isEmpty());
        GameLibrary other(dir.filePath("two/games.sqlite"),&registry);QVERIFY(other.ready());
        QVERIFY(other.details(id).isEmpty());QVERIFY(other.resolveLaunch(id).isEmpty());QVERIFY(!other.removeGame(id));
        // A real SQLite abort proves failed writes leave the previous state.
        const auto connection=QStringLiteral("games-fault-test");
        {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(path);QVERIFY(db.open());
            {QSqlQuery q(db);QVERIFY(q.exec("CREATE TRIGGER reject_favorite BEFORE UPDATE ON games BEGIN SELECT RAISE(ABORT,'injected write failure'); END"));}
            QVERIFY(!catalog.setFavorite(id,false));QVERIFY(catalog.details(id)["favorite"].toBool());
            {QSqlQuery q(db);QVERIFY(q.exec("DROP TRIGGER reject_favorite"));}db.close();}
        QSqlDatabase::removeDatabase(connection);
    }
    {GameLibrary reopened(path,&registry);QVERIFY(reopened.ready());QCOMPARE(reopened.page().size(),1);
        QCOMPARE(reopened.resolveLaunch(id)["target"].toString(),root+"/game.iso");
        QVERIFY(reopened.removeGame(id));QVERIFY(reopened.page().isEmpty());
        QCOMPARE(readFile(root+"/game.iso"),content);reopened.invalidate();QVERIFY(reopened.addGame(gameRegistration(root)).isEmpty());}
    // Unknown versions must never be reset, migrated or rewritten on read.
    const auto connection=QStringLiteral("games-version-test");
    {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(path);QVERIFY(db.open());{QSqlQuery q(db);QVERIFY(q.exec("PRAGMA user_version=99"));}db.close();}
    QSqlDatabase::removeDatabase(connection);const auto before=readFile(path);
    {GameLibrary unknown(path,&registry);QVERIFY(!unknown.ready());QVERIFY(!unknown.errorString().isEmpty());}
    QCOMPARE(readFile(path),before);
}

// Exercise this boundary against the real profile-owned game library.
void GameLibraryTests::gamesLaunchRejectsMissingAndRedirectedTargets() {
    QTemporaryDir dir;const auto root=dir.filePath("library");QVERIFY(QDir().mkpath(root));writeFile(root+"/game.iso","original");
    AppRegistry registry;QVERIFY(registry.addApp(gamesRuntime()));GameLibrary catalog(dir.filePath("profile/games.sqlite"),&registry);
    const auto id=catalog.addGame(gameRegistration(root));QVERIFY(!id.isEmpty());
    QCOMPARE(catalog.resolveLaunch(id).value("openWith").toString(),QString("psp-test"));
    QVERIFY(QFile::rename(root+"/game.iso",dir.filePath("outside.iso")));QVERIFY(catalog.resolveLaunch(id).isEmpty());
#ifdef Q_OS_UNIX
    QVERIFY(QFile::link(dir.filePath("outside.iso"),root+"/game.iso"));QVERIFY(catalog.resolveLaunch(id).isEmpty());
    QVERIFY(catalog.addGame(gameRegistration(root)).isEmpty());
#endif
    registry.removeApp("psp-test");QVERIFY(catalog.resolveLaunch(id).isEmpty());
    QCOMPARE(readFile(dir.filePath("outside.iso")),QByteArray("original"));
}
