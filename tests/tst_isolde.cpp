#include "app/AppRegistry.h"
#include "app/AppArtwork.h"
#include "app/AppCatalog.h"
#include "input/InputManager.h"
#include "sessions/SessionManager.h"
#include "home/HomeLayout.h"
#include "settings/DisplaySettings.h"
#include "settings/AudioSettings.h"
#include "settings/RemoteSettings.h"
#include "settings/AmbientSettings.h"
#include "settings/ProfileManager.h"
#include "settings/SetupState.h"
#include "sources/SourceManager.h"
#include "theme/ArtPackCatalog.h"

#include <QSignalSpy>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QImage>
#include <QSettings>
#include <QUrl>
#include <QtTest>
#include <QScopeGuard>
#include <QStandardPaths>

class CountingStoreProvider final : public StoreProvider {
public:
    QString identity="fdroid";int searches=0;mutable int artifacts=0;
    QString providerId() const override{return identity;}
    QString providerName() const override{return "Test repository";}
    bool isOnline() const override{return true;}
    QVariantList search(const QString &) override{++searches;return {QVariantMap{{"id","test-app"},{"storeId","test-app"},{"type","android"},{"storeType","android"},{"name","Test app"}}};}
    QUrl artifactUrl(const QString &) const override{++artifacts;return QUrl("https://example.invalid/app.apk");}
    QByteArray artifactHash(const QString &) const override{return {};}
    QString provenanceText(const QString &) const override{return "test";}
};

class IsoldeTests final : public QObject {
    Q_OBJECT

private slots:
    void customWebAppsPersistAndRejectUnsafeInput() {
        QCoreApplication::setApplicationVersion("0.0.1.2");QTemporaryDir temp;
        const auto state=temp.filePath("state.json"),packages=temp.filePath("packages");
        AppRegistry registry;AppCatalog catalog(&registry);QVERIFY(catalog.load(QFINDTESTDATA("../config/catalog.json"),state,packages));
        for(const auto &url:QStringList{"javascript:alert(1)","file:///etc/passwd","http://example.org","https://user:secret@example.org"})QVERIFY(catalog.addWebApp("Example",url,"embedded").isEmpty());
        QVERIFY(catalog.addWebApp("Example","https://example.org","shell").isEmpty());
        const auto id=catalog.addWebApp("Example","https://example.org/","embedded");QVERIFY(!id.isEmpty());QVERIFY(catalog.isInstalled(id));
        QCOMPARE(catalog.addWebApp("Example","https://example.org/","embedded"),id);
        QCOMPARE(registry.find(id)->type,QString("web"));
        AppRegistry restoredRegistry;AppCatalog restored(&restoredRegistry);QVERIFY2(restored.load(QFINDTESTDATA("../config/catalog.json"),state,packages),qPrintable(restored.errorString()));
        QVERIFY(restored.isInstalled(id));QCOMPARE(restoredRegistry.find(id)->launchConfiguration.value("url").toString(),QString("https://example.org/"));
        QVERIFY(restored.uninstall(id));AppRegistry removedRegistry;AppCatalog removed(&removedRegistry);
        QVERIFY(removed.load(QFINDTESTDATA("../config/catalog.json"),state,packages));QVERIFY(!removed.isInstalled(id));
        const auto chrome=catalog.addWebApp("Remote site","https://example.net/","chromium");QVERIFY(!chrome.isEmpty());
        QCOMPARE(registry.find(chrome)->type,QString("desktop"));QCOMPARE(registry.find(chrome)->launchConfiguration.value("desktopId").toString(),QString("google-chrome"));
        const auto blocked=temp.filePath("not-a-state-file");QDir().mkpath(blocked);AppRegistry failedRegistry;AppCatalog failed(&failedRegistry);
        QVERIFY(failed.load(QFINDTESTDATA("../config/catalog.json"),blocked,temp.filePath("failed-packages")));
        QVERIFY(failed.addWebApp("Failure","https://example.edu/","embedded").isEmpty());QVERIFY(failedRegistry.apps().isEmpty());
    }
    void onlineStoreRefusesUnknownSourcesAndRequiresOptInAndHash() {
        AppRegistry registry;AppCatalog catalog(&registry);CountingStoreProvider provider;
        provider.identity="unknown";catalog.setStoreProvider(&provider);QVERIFY(!catalog.storeProvider());
        QVERIFY(catalog.errorString().contains("Unknown"));
        provider.identity="fdroid";catalog.setStoreProvider(&provider);
        catalog.searchStore("test");QCOMPARE(provider.searches,0);
        QVERIFY(!catalog.downloadAndInstall("test-app"));QCOMPARE(provider.artifacts,0);
        catalog.setOnlineStoreSearch(true);QCOMPARE(provider.searches,1);
        QCOMPARE(catalog.providerResults().size(),1);
        QVERIFY(!catalog.downloadAndInstall("test-app"));QVERIFY(catalog.errorString().contains("SHA-256"));
        QVERIFY(!catalog.isInstalled("test-app"));QVERIFY(registry.find("test-app")==nullptr);
        catalog.setOnlineStoreSearch(false);catalog.searchStore("again");QCOMPARE(provider.searches,1);
        QVERIFY(catalog.providerResults().isEmpty());
    }
    void storeChangesOnlyTheActiveProfile() {
        QCoreApplication::setApplicationVersion("0.0.1.2");QTemporaryDir directory;
        const auto previous=qApp->property("isoldeProfile");const auto oldConfig=qgetenv("XDG_CONFIG_HOME"),oldData=qgetenv("XDG_DATA_HOME");
        const auto restore=qScopeGuard([&]{qApp->setProperty("isoldeProfile",previous);if(oldConfig.isNull())qunsetenv("XDG_CONFIG_HOME");else qputenv("XDG_CONFIG_HOME",oldConfig);if(oldData.isNull())qunsetenv("XDG_DATA_HOME");else qputenv("XDG_DATA_HOME",oldData);});
        qputenv("XDG_CONFIG_HOME",directory.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",directory.filePath("data").toUtf8());
        const auto config=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),data=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QVERIFY(config.startsWith(directory.path()+"/"));QVERIFY(data.startsWith(directory.path()+"/"));
        QDir().mkpath(config);QDir().mkpath(data+"/packages/plex/1.0.0");
        QFile global(config+"/installed-apps.json");QVERIFY(global.open(QIODevice::WriteOnly));global.write(R"({"installed":["plex"]})");global.close();
        QVERIFY(QFile::copy(QFINDTESTDATA("../config/packages/plex/manifest.json"),data+"/packages/plex/1.0.0/manifest.json"));
        qApp->setProperty("isoldeProfile","3b86fe9a-8904-4dc2-bdb5-cfd8bb139c53");
        AppRegistry one;AppCatalog first(&one);QVERIFY(first.load(QFINDTESTDATA("../config/catalog.json")));QVERIFY(first.isInstalled("plex"));QVERIFY(first.uninstall("plex"));
        const auto website=first.addWebApp("Private website","https://example.org/private","embedded");QVERIFY(!website.isEmpty());
        qApp->setProperty("isoldeProfile","2ec32a48-e38d-4986-af69-a04cc4dd09e7");
        AppRegistry two;AppCatalog second(&two);QVERIFY(second.load(QFINDTESTDATA("../config/catalog.json")));QVERIFY(second.isInstalled("plex"));
        QVERIFY(!second.isInstalled(website));QVERIFY(two.find(website)==nullptr);
        qApp->setProperty("isoldeProfile","3b86fe9a-8904-4dc2-bdb5-cfd8bb139c53");
        AppRegistry again;AppCatalog reloaded(&again);QVERIFY(reloaded.load(QFINDTESTDATA("../config/catalog.json")));QVERIFY(!reloaded.isInstalled("plex"));
        QVERIFY(reloaded.isInstalled(website));
        QVERIFY(global.open(QIODevice::ReadOnly));QCOMPARE(global.readAll(),QByteArray(R"({"installed":["plex"]})"));
        QVERIFY(QFile::exists(data+"/packages/plex/1.0.0/manifest.json"));
        global.close();qApp->setProperty("isoldeProfile","default");
        AppRegistry householdRegistry;AppCatalog household(&householdRegistry);QVERIFY(household.load(QFINDTESTDATA("../config/catalog.json")));QVERIFY(household.uninstall("plex"));
        QVERIFY(global.open(QIODevice::ReadOnly));QCOMPARE(global.readAll(),QByteArray(R"({"installed":["plex"]})"));
        QVERIFY(second.isInstalled("plex"));
    }
    void androidDefinitionsRejectExecutableRecipes() {
        AppRegistry registry;
        const QByteArray valid=R"([{"id":"smarttube","name":"SmartTube","type":"android","runtimeApp":"smarttube","presentationMode":"desktop-bridge","section":"Apps"}])";
        QVERIFY(registry.loadFromData(valid));
        QCOMPARE(registry.find("smarttube")->type,QString("android"));
        auto bad=*registry.find("smarttube");bad.id="bad";
        bad.launchConfiguration["runtimeApp"]="../account";
        QVERIFY(!registry.addApp(bad));
        bad.launchConfiguration["runtimeApp"]="smarttube";
        bad.launchConfiguration["executable"]="/bin/sh";
        QVERIFY(!registry.addApp(bad));
        QVERIFY(!registry.loadFromData(QByteArray(valid).replace("smarttube\",\"presentationMode", "../bad\",\"presentationMode")));
        QVERIFY(registry.find("smarttube"));
        QCoreApplication::setApplicationVersion("0.0.1.2");
        QTemporaryDir directory;
        AppCatalog catalog(&registry);
        QVERIFY2(catalog.load(QFINDTESTDATA("../config/catalog.json"),directory.filePath("state.json"),directory.filePath("packages")),qPrintable(catalog.errorString()));
        // Use a clean registry: publication must come from the normal catalog.
        QVERIFY(registry.removeApp("smarttube"));
        bool found=false;
        for(const auto &entry:catalog.entries())if(entry.toMap().value("id")=="smarttube"){
            QCOMPARE(entry.toMap().value("type").toString(),QString("android"));found=true;
        }
        QVERIFY(found); // Actual Android installation is verified in the VM Store journey.
    }
    void appArtworkRemovesOnlyConnectedBackground() {
        QImage logo(20,20,QImage::Format_ARGB32);logo.fill(Qt::white);
        for(int y=4;y<16;++y)for(int x=4;x<16;++x)logo.setPixelColor(x,y,Qt::red);
        logo.setPixelColor(10,10,Qt::white);
        const auto result=AppArtwork::cutout(logo);
        QCOMPARE(result.pixelColor(0,0).alpha(),0);
        QCOMPARE(result.pixelColor(4,4),QColor(Qt::red));
        QCOMPARE(result.pixelColor(10,10),QColor(Qt::white));
        QCOMPARE(logo.pixelColor(0,0),QColor(Qt::white));
        logo.setPixelColor(0,0,Qt::transparent);
        QCOMPARE(AppArtwork::cutout(logo),logo);
        logo.fill(Qt::blue);QCOMPARE(AppArtwork::cutout(logo),logo);
        logo.fill(Qt::white);logo.setPixelColor(19,0,Qt::black);
        QCOMPARE(AppArtwork::cutout(logo),logo);
    }
    void setupProgressPersistsAndHandlesFailure();
    void profilesPersistAndIsolatePersonalSettings();
    void registryLoadsConfig();
    void registryCompatibilityIdsPreserveLayoutsWithoutDuplicateApps();
    void inputManagerMapsRemoteAndMediaKeys();
    void registryAcceptsLivingRoomPresentationModes();
    void registryAndHomeExposeNetworkRequirements();
    void homeLayoutSupportsTypedActionTiles();
    void sessionManagerLaunchesActionWithChosenHandler();
    void sourceManagerProjectsContextualTilesAndPromotionState();
    void artPackCatalogLoadsAndHomeAppliesArtwork();
    void presentationModeTravelsThroughRegistryCatalogAndSession();
    void webKioskSessionExposesSafeLaunchUrl();
    void registryRejectsUnsafeWebUrl();
    void registryRejectsDuplicateIds();
    void sessionLifecycleRestoresFocus();
    void secondLaunchIsRejected();
    void launchGuardRunsBeforeHandoff();
    void customizationSessionIsSupported();
    void catalogPersistsInstallLifecycle();
    void catalogTracksSetupAndFailureState();
    void catalogRejectsInvalidManifest();
    void catalogRejectsUnsafeDesktopManifest();
    void catalogAcceptsKodiMediaPackage();
    void homeLayoutPersistsAndRecovers();
    void displaySettingsPersistAndClamp();
    void audioSettingsPersistAndClamp();
    void remoteSettingsPersistAndCycle();
    void ambientSettingsPersistAndClamp();
};

void IsoldeTests::setupProgressPersistsAndHandlesFailure()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("device/setup.json");
    SetupState state(path);
    QVERIFY(!state.complete());
    QCOMPARE(state.step(), 0);
    QVERIFY(state.advance());
    SetupState resumed(path);
    QCOMPARE(resumed.step(), 1);
    for (int i = 1; i < 7; ++i) QVERIFY(resumed.advance());
    QVERIFY(!resumed.complete());
    QVERIFY(resumed.advance());
    QVERIFY(SetupState(path).complete());
    QVERIFY(resumed.restart());
    QCOMPARE(resumed.step(), 0);
    QVERIFY(!resumed.complete());
    QVERIFY(resumed.back());
    QCOMPARE(resumed.step(), 0);
    QFile malformed(path);
    QVERIFY(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    malformed.write("{\"version\":1,\"step\":99,\"complete\":true}");
    malformed.close();
    SetupState invalid(path);
    QVERIFY(!invalid.complete());
    QVERIFY(!invalid.error().isEmpty());
    QVERIFY(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray legacy = "{\"version\":1,\"step\":6,\"complete\":true}";
    QCOMPARE(malformed.write(legacy), legacy.size());
    malformed.close();
    SetupState legacyCompleted(path);
    QVERIFY(legacyCompleted.complete());
    QCOMPARE(legacyCompleted.step(), 7);
    QVERIFY(malformed.open(QIODevice::ReadOnly));
    QCOMPARE(malformed.readAll(), legacy); // Compatibility load must not rewrite.
    malformed.close();
    SetupState blocked(path + "/not-a-directory.json");
    QVERIFY(!blocked.advance());
    QCOMPARE(blocked.step(), 0);
    QVERIFY(!blocked.error().isEmpty());
}

static const QByteArray config = R"([
  {"id":"youtube","name":"YouTube","type":"mock","section":"Apps","accent":"#ff0000"},
  {"id":"movies","name":"Movies","type":"mock","section":"Library","accent":"#5B8DEF","handledActions":["open-folder","open-file","open-disc"]},
  {"id":"switch","name":"Switch","type":"mock","section":"Games","accent":"#00ff00"}
])";

void IsoldeTests::registryLoadsConfig()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    QCOMPARE(registry.apps().size(), 3);
    QCOMPARE(registry.find("switch")->name, QString("Switch"));
}
void IsoldeTests::registryCompatibilityIdsPreserveLayoutsWithoutDuplicateApps()
{
    AppRegistry registry;
    const auto valid=QByteArray(R"([{"id":"reader","name":"Reader","type":"mock","legacyIds":["old-reader"]}])");
    QVERIFY(registry.loadFromData(valid));QCOMPARE(registry.apps().size(),1);QCOMPARE(registry.find("old-reader"),registry.find("reader"));
    QTemporaryDir dir;const auto defaults=dir.filePath("default.json"),saved=dir.filePath("home.json");
    const QByteArray layout=R"({"schemaVersion":1,"items":[{"id":"saved-tile","kind":"app","appId":"old-reader"}]})";
    QFile file(defaults);QVERIFY(file.open(QIODevice::WriteOnly));file.write(layout);file.close();
    HomeLayout home(&registry);QVERIFY(home.load(defaults,saved));QVERIFY(!QFile::exists(saved));QCOMPARE(home.items().first().toMap()["name"].toString(),QString("Reader"));
    QVERIFY(!registry.loadFromData(R"([{"id":"reader","name":"Reader","type":"mock","legacyIds":["other"]},{"id":"other","name":"Other","type":"mock"}])"));
    QCOMPARE(registry.find("old-reader"),registry.find("reader"));QCOMPARE(registry.apps().size(),1);
    AppDefinition conflict;conflict.id="other";conflict.name="Other";conflict.type="mock";conflict.launchConfiguration={{"legacyIds",QVariantList{"old-reader"}}};QVERIFY(!registry.addApp(conflict));
    conflict.launchConfiguration={{"legacyIds",QVariantList{"../unsafe"}}};QVERIFY(!registry.addApp(conflict));
    QVERIFY(registry.removeApp("reader"));QVERIFY(!registry.find("old-reader"));QVERIFY(!QFile::exists(saved));
}

void IsoldeTests::inputManagerMapsRemoteAndMediaKeys()
{
    InputManager input;
    QCOMPARE(input.commandForKey(Qt::Key_Up), InputManager::NavigateUp);
    QCOMPARE(input.commandForKey(Qt::Key_Return), InputManager::Confirm);
    QCOMPARE(input.commandForKey(Qt::Key_Back), InputManager::Back);
    QCOMPARE(input.commandForKey(Qt::Key_Home), InputManager::Home);
    QCOMPARE(input.commandForKey(Qt::Key_VolumeUp), InputManager::VolumeUp);
    QCOMPARE(input.commandForKey(Qt::Key_VolumeDown), InputManager::VolumeDown);
    QCOMPARE(input.commandForKey(Qt::Key_VolumeMute), InputManager::Mute);
    QCOMPARE(input.commandForKey(Qt::Key_PowerOff), InputManager::Power);
    QCOMPARE(input.commandForKey(Qt::Key_MediaTogglePlayPause), InputManager::PlayPause);
    QCOMPARE(input.commandForKey(Qt::Key_Menu), InputManager::Menu);
    QCOMPARE(input.commandForKey(Qt::Key_Search), InputManager::Search);
    QCOMPARE(input.labelForCommand(InputManager::VolumeUp), QString("Volume +"));
    QCOMPARE(input.labelForCommand(InputManager::Power), QString("Power"));
}

void IsoldeTests::registryAcceptsLivingRoomPresentationModes()
{
    AppRegistry registry;
    const QByteArray data = "["
        "{\"id\":\"steam\",\"name\":\"Steam\",\"type\":\"mock\",\"presentationMode\":\"game-hub\",\"section\":\"Games\",\"accent\":\"#4F83A8\"},"
        "{\"id\":\"game-disc\",\"name\":\"Game Disc\",\"type\":\"mock\",\"presentationMode\":\"disc-game\",\"section\":\"Games\",\"accent\":\"#8B5CF6\"},"
        "{\"id\":\"movie-disc\",\"name\":\"Movie Disc\",\"type\":\"mock\",\"presentationMode\":\"disc-media\",\"section\":\"Library\",\"accent\":\"#06B6D4\"},"
        "{\"id\":\"desktop\",\"name\":\"Desktop Mode\",\"type\":\"mock\",\"presentationMode\":\"utility-desktop\",\"section\":\"System\",\"accent\":\"#64748B\"}"
    "]";
    QVERIFY(registry.loadFromData(data));
    QCOMPARE(registry.apps().size(), 4);
    QCOMPARE(registry.find("steam")->presentationMode, QString("game-hub"));
    QCOMPARE(registry.find("game-disc")->presentationMode, QString("disc-game"));
    QCOMPARE(registry.find("movie-disc")->presentationMode, QString("disc-media"));
    QCOMPARE(registry.find("desktop")->presentationMode, QString("utility-desktop"));
}

void IsoldeTests::registryAndHomeExposeNetworkRequirements()
{
    AppRegistry registry;
    const QByteArray data = "["
        "{\"id\":\"netflix\",\"name\":\"Netflix\",\"type\":\"web\",\"presentationMode\":\"web-kiosk\",\"networkRequirement\":\"internet\",\"url\":\"https://www.netflix.com\",\"allowedOrigins\":[\"https://www.netflix.com\"],\"section\":\"Apps\",\"accent\":\"#E50914\"},"
        "{\"id\":\"nas\",\"name\":\"NAS Media\",\"type\":\"mock\",\"presentationMode\":\"media\",\"networkRequirement\":\"local-network\",\"section\":\"Library\",\"accent\":\"#06B6D4\"},"
        "{\"id\":\"kodi\",\"name\":\"Kodi\",\"type\":\"mock\",\"presentationMode\":\"media\",\"networkRequirement\":\"none\",\"section\":\"Library\",\"accent\":\"#1685D9\"}"
    "]";
    QVERIFY(registry.loadFromData(data));
    QCOMPARE(registry.find("netflix")->networkRequirement, QString("internet"));
    QCOMPARE(registry.find("nas")->networkRequirement, QString("local-network"));
    QCOMPARE(registry.find("kodi")->networkRequirement, QString("none"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile defaults(directory.filePath("default.json"));
    QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"brandVisible":false,"backgroundStart":"#111111","backgroundEnd":"#222222"},"items":[{"kind":"app","id":"netflix","appId":"netflix"},{"kind":"app","id":"nas","appId":"nas"},{"kind":"app","id":"kodi","appId":"kodi"}]})");
    defaults.close();
    HomeLayout layout(&registry);
    QVERIFY(layout.load(directory.filePath("default.json"), directory.filePath("user.json")));
    QCOMPARE(layout.items().at(0).toMap().value("networkRequirement").toString(), QString("internet"));
    QCOMPARE(layout.items().at(1).toMap().value("networkRequirement").toString(), QString("local-network"));
    QCOMPARE(layout.items().at(2).toMap().value("networkRequirement").toString(), QString("none"));
}

void IsoldeTests::homeLayoutSupportsTypedActionTiles()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile defaults(directory.filePath("default.json"));
    QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"brandVisible":false,"backgroundStart":"#111111","backgroundEnd":"#222222"},"categories":[{"id":"media","name":"Media"}],"items":[]})");
    defaults.close();
    HomeLayout layout(&registry);
    QVERIFY(layout.load(directory.filePath("default.json"), directory.filePath("user.json")));
    QVERIFY(layout.addActionTile("Movies Drive", "open-folder", "/media/movies", "media", "system"));
    QVERIFY(layout.addActionTile("Blu-ray Disc", "open-disc", "/dev/sr0", "media", "movies"));
    QCOMPARE(layout.items().size(), 2);
    const auto folderTile = layout.items().at(0).toMap();
    QCOMPARE(folderTile.value("name").toString(), QString("Movies Drive"));
    QCOMPARE(folderTile.value("actionType").toString(), QString("open-folder"));
    QCOMPARE(folderTile.value("action").toMap().value("target").toString(), QString("/media/movies"));
    QCOMPARE(folderTile.value("action").toMap().value("openWith").toString(), QString("system"));
    QCOMPARE(folderTile.value("networkRequirement").toString(), QString("none"));
    QVERIFY(layout.setTileOption(folderTile.value("id").toString(), "showTitle", false));
    QVERIFY(layout.setTileOption(folderTile.value("id").toString(), "logoFill", true));
    const auto discTile = layout.items().at(1).toMap();
    QCOMPARE(discTile.value("actionType").toString(), QString("open-disc"));
    QCOMPARE(discTile.value("action").toMap().value("openWith").toString(), QString("movies"));
    QVERIFY(!layout.addActionTile("Unsafe", "launch-app", "youtube", "media"));
    QVERIFY(!layout.addActionTile("Missing Target", "open-folder", "", "media"));
    QVERIFY(!layout.addActionTile("Unknown Handler", "open-folder", "/media/movies", "media", "missing-app"));
    QVERIFY(!layout.addActionTile("Wrong Handler", "open-disc", "/dev/sr0", "media", "youtube"));
}

void IsoldeTests::sessionManagerLaunchesActionWithChosenHandler()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    SessionManager manager(&registry);
    QSignalSpy rejected(&manager, &SessionManager::sessionRejected);
    QVERIFY(!manager.launchAction(QVariantMap{{"type", "open-folder"}, {"target", "/media/movies"}, {"openWith", "system"}}));
    QCOMPARE(rejected.count(), 1);
    QVERIFY(!manager.hasActiveSession());
    QVERIFY(!manager.launchAction(QVariantMap{{"type", "open-disc"}, {"target", "/dev/sr0"}, {"openWith", "youtube"}}));
    QCOMPARE(rejected.count(), 2);
    QVERIFY(manager.launchAction(QVariantMap{{"type", "open-folder"}, {"target", "/media/movies"}, {"openWith", "movies"}}));
    QVERIFY(manager.hasActiveSession());
    QCOMPARE(manager.currentAppId(), QString("movies"));
    QCOMPARE(manager.previousFocusId(), QString("movies"));
    manager.exitCurrent();
}

void IsoldeTests::sourceManagerProjectsContextualTilesAndPromotionState()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IsoldeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SourceManagerTest"));
    QSettings().clear();

    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath("USB Movies")));
    QVERIFY(QDir().mkpath(directory.filePath("Movie Disc/VIDEO_TS")));
    QVERIFY(QDir().mkpath(directory.filePath("PS5 Game")));

    SourceManager sources(&registry);
    sources.setProbeRootsForTests({directory.path()});
    QCOMPARE(sources.items().size(), 3);
    const auto tile = sources.items().first().toMap();
    QCOMPARE(tile.value("kind").toString(), QString("action"));
    QCOMPARE(tile.value("contextual").toBool(), true);
    QCOMPARE(tile.value("section").toString(), QString("Detected Sources"));
    QCOMPARE(tile.value("actionType").toString(), QString("open-folder"));
    QCOMPARE(tile.value("action").toMap().value("openWith").toString(), QString("movies"));
    QCOMPARE(sources.sections().size(), 1);
    bool sawMovieDisc = false;
    bool sawGameDisc = false;
    for (const auto &sourceValue : sources.items()) {
        const auto source = sourceValue.toMap();
        sawMovieDisc = sawMovieDisc || source.value("sourceType").toString() == QString("movie-disc");
        sawGameDisc = sawGameDisc || source.value("sourceType").toString() == QString("game-disc");
    }
    QVERIFY(sawMovieDisc);
    QVERIFY(sawGameDisc);

    QVERIFY(sources.markPermanent(tile.value("id").toString()));
    QCOMPARE(sources.items().size(), 2);
    QVERIFY(sources.forgetSourceDecision(tile.value("id").toString()));
    QCOMPARE(sources.items().size(), 3);

    QSettings().clear();
    SourceManager hiddenSources(&registry);
    hiddenSources.setProbeRootsForTests({directory.path()});
    QCOMPARE(hiddenSources.items().size(), 3);
    QVERIFY(hiddenSources.hideSource(hiddenSources.items().first().toMap().value("id").toString()));
    QCOMPARE(hiddenSources.items().size(), 2);
    hiddenSources.setAutomaticSourcesEnabled(false);
    QCOMPARE(hiddenSources.visibleSourceCount(), 0);

    QSettings().clear();
    AppRegistry registryWithoutHandlers;
    QVERIFY(registryWithoutHandlers.loadFromData(R"([{"id":"youtube","name":"YouTube","type":"mock","section":"Apps","accent":"#ff0000"}])"));
    SourceManager noHandlerSources(&registryWithoutHandlers);
    noHandlerSources.setProbeRootsForTests({directory.path()});
    QCOMPARE(noHandlerSources.items().size(), 0);
}

void IsoldeTests::artPackCatalogLoadsAndHomeAppliesArtwork()
{
    ArtPackCatalog catalog;
    QVERIFY(catalog.loadFromData(R"({"schemaVersion":1,"packs":[{"id":"starter","name":"Starter","description":"Test","themes":[{"id":"midnight","name":"Midnight","description":"Dark","backgroundStart":"#111A32","backgroundEnd":"#080C18","focusColor":"#F8FAFC","typographyScale":1.05,"tileRoundness":0.22,"tileOpacity":0.88,"suggestedArtwork":{"switch":{"symbol":"NX","accent":"#E60012"},"ps5-input":{"symbol":"PS","accent":"#315DAA"}}}],"artwork":[{"id":"steam","name":"Steam","category":"Games","symbol":"S","accent":"#4F83A8"},{"id":"ps5","name":"PlayStation","category":"Games","symbol":"PS","accent":"#315DAA"}]}]})"));
    QCOMPARE(catalog.packs().size(), 1);
    QCOMPARE(catalog.themes().size(), 1);
    QCOMPARE(catalog.artwork().size(), 2);
    QCOMPARE(catalog.themes().first().toMap().value("tileRoundness").toDouble(), 0.22);
    QCOMPARE(catalog.themes().first().toMap().value("tileOpacity").toDouble(), 0.88);
    QCOMPARE(catalog.artwork().at(1).toMap().value("symbol").toString(), QString("PS"));
    QVERIFY(!catalog.loadFromData(R"({"schemaVersion":1,"packs":[{"id":"starter","name":"Starter","artwork":[{"id":"bad","name":"Bad","symbol":"","accent":"not-a-color"}]}]})"));

    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile defaults(directory.filePath("default.json"));
    QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"brandVisible":false,"backgroundStart":"#111111","backgroundEnd":"#222222"},"items":[{"kind":"app","id":"switch","appId":"switch","size":"standard"},{"kind":"action","id":"ps5-input","name":"PS5","size":"standard","action":{"type":"open-folder","target":"/media/ps5","openWith":"movies"}}]})");
    defaults.close();

    HomeLayout layout(&registry);
    QVERIFY(layout.load(directory.filePath("default.json"), directory.filePath("user.json")));
    QVERIFY(layout.setTileAppearance("switch", "N", "#00FF00"));
    QVERIFY(layout.setTileAppearance("ps5-input", "G", "#111111"));
    QVERIFY(layout.applyTheme(catalog.themes().first().toMap(), false));
    QCOMPARE(layout.items().at(0).toMap().value("symbol").toString(), QString("N"));
    QCOMPARE(layout.items().at(0).toMap().value("accent").toString(), QString("#00ff00"));
    QCOMPARE(layout.items().at(0).toMap().value("opacity").toDouble(), 0.88);
    QVERIFY(layout.applyTheme(catalog.themes().first().toMap(), true));
    QCOMPARE(layout.items().at(0).toMap().value("symbol").toString(), QString("NX"));
    QCOMPARE(layout.items().at(0).toMap().value("accent").toString(), QString("#e60012"));
    QCOMPARE(layout.items().at(1).toMap().value("symbol").toString(), QString("PS"));

    HomeLayout restored(&registry);
    QVERIFY(restored.load(directory.filePath("default.json"), directory.filePath("user.json")));
    QCOMPARE(restored.items().at(0).toMap().value("symbol").toString(), QString("NX"));
    QCOMPARE(restored.items().at(1).toMap().value("accent").toString(), QString("#315daa"));
    QCOMPARE(restored.backgroundStart(), QString("#111a32"));
    QCOMPARE(restored.backgroundEnd(), QString("#080c18"));
    QCOMPARE(restored.tileRoundness(), 0.22);
    QCOMPARE(restored.appearance()["focusColor"].toString(), QString("#f8fafc"));
    QCOMPARE(restored.appearance()["typographyScale"].toDouble(), 1.05);
    const auto originalItems = restored.items();
    QVERIFY(restored.setAppearance({{"focusColor", "#12ABCD"}, {"typographyScale", 1.2},
        {"motionEnabled", false}, {"homeColumns", 5}, {"spacing", 30}, {"backgroundStart", "#19191e"}}));
    QCOMPARE(restored.items(), originalItems); // appearance never alters launchers or artwork
    const auto personal = restored.appearance();
    QVERIFY(!restored.setAppearance({{"focusColor", "invalid"}}));
    QVERIFY(!restored.setAppearance({{"focusColor", "#00112233"}}));
    QVERIFY(!restored.setAppearance({{"homeColumns", 3.5}}));
    QVERIFY(!restored.setAppearance({{"typographyScale", 99}}));
    QVERIFY(!restored.setAppearance({{"motionEnabled", "false"}}));
    QVERIFY(!restored.setAppearance({{"unknownColor", "#123456"}}));
    QCOMPARE(restored.appearance(), personal);
    HomeLayout reloaded(&registry);
    QVERIFY(reloaded.load(defaults.fileName(), directory.filePath("user.json")));
    QCOMPARE(reloaded.appearance(), personal);
    HomeLayout otherProfile(&registry);
    QVERIFY(otherProfile.load(defaults.fileName(), directory.filePath("other-user.json")));
    QVERIFY(otherProfile.appearance() != personal);
    // A failed atomic save must leave the running appearance untouched.
    QDir().mkdir(directory.filePath("blocked.json"));
    HomeLayout blocked(&registry);
    QVERIFY(blocked.load(defaults.fileName(), directory.filePath("blocked.json")));
    const auto beforeFailure = blocked.appearance();
    QVERIFY(!blocked.setAppearance({{"focusColor", "#123456"}}));
    QCOMPARE(blocked.appearance(), beforeFailure);
}

void IsoldeTests::presentationModeTravelsThroughRegistryCatalogAndSession()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1.2"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath("packages/spotify")));
    QFile catalogFile(directory.filePath("catalog.json"));
    QVERIFY(catalogFile.open(QIODevice::WriteOnly));
    catalogFile.write(R"({"schemaVersion":1,"packages":["packages/spotify/manifest.json"]})");
    catalogFile.close();
    QFile manifestFile(directory.filePath("packages/spotify/manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly));
    manifestFile.write(R"({"schemaVersion":1,"packageId":"spotify","version":"1.0.0","minIsoldeVersion":"0.0.1.2","name":"Spotify","description":"Desktop bridge test","category":"Music","publisher":"Test","capabilities":["desktop.external"],"app":{"id":"spotify","type":"desktop","presentationMode":"desktop-bridge","section":"Music","accent":"#1db954","symbol":"S","desktopId":"isolde-spotify","windowClass":"spotify.Spotify","containment":"fullscreen"}})");
    manifestFile.close();

    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    AppCatalog catalog(&registry);
    QVERIFY(catalog.load(directory.filePath("catalog.json"), directory.filePath("state.json"), directory.filePath("installed")));
    QCOMPARE(catalog.entries().first().toMap().value("presentationMode").toString(), QString("desktop-bridge"));
    QVERIFY(catalog.install("spotify"));
    QCOMPARE(registry.find("spotify")->presentationMode, QString("desktop-bridge"));
    QCOMPARE(registry.find("spotify")->launchConfiguration.value("windowClass").toString(), QString("spotify.Spotify"));
    QCOMPARE(registry.find("spotify")->launchConfiguration.value("containment").toString(), QString("fullscreen"));

    AppRegistry sessionRegistry;
    QVERIFY(sessionRegistry.loadFromData(R"([{"id":"bridge","name":"Bridge","type":"mock","presentationMode":"desktop-bridge"}])"));
    SessionManager manager(&sessionRegistry);
    QVERIFY(manager.launch("bridge"));
    QCOMPARE(manager.currentAppType(), QString("mock"));
    QCOMPARE(manager.currentPresentationMode(), QString("desktop-bridge"));
}

void IsoldeTests::webKioskSessionExposesSafeLaunchUrl()
{
    AppRegistry registry;
    const QByteArray webConfig = R"([{"id":"youtube","name":"YouTube","type":"web","presentationMode":"web-kiosk","url":"https://www.youtube.com/tv","allowedOrigins":["https://www.youtube.com","https://accounts.google.com"],"accent":"#ff0000"}])";
    QVERIFY(registry.loadFromData(webConfig));
    SessionManager manager(&registry);
    QVERIFY(manager.launch("youtube"));
    QCOMPARE(manager.currentAppType(), QString("web"));
    QCOMPARE(manager.currentPresentationMode(), QString("web-kiosk"));
    QCOMPARE(manager.currentLaunchUrl(), QString("https://www.youtube.com/tv"));
    QCOMPARE(manager.currentAllowedOrigins().size(), 2);
    QCOMPARE(manager.currentAllowedOrigins().first(), QString("https://www.youtube.com"));
}

void IsoldeTests::registryRejectsUnsafeWebUrl()
{
    AppRegistry registry;
    const QByteArray webConfig = R"([{"id":"bad","name":"Bad","type":"web","presentationMode":"web-kiosk","url":"file:///etc/passwd","accent":"#ff0000"}])";
    QVERIFY(!registry.loadFromData(webConfig));
    QVERIFY(!registry.errorString().isEmpty());
}

void IsoldeTests::registryRejectsDuplicateIds()
{
    AppRegistry registry;
    QVERIFY(!registry.loadFromData(R"([{"id":"x","name":"X","type":"mock"},{"id":"x","name":"Y","type":"mock"}])"));
    QVERIFY(!registry.errorString().isEmpty());
}

void IsoldeTests::sessionLifecycleRestoresFocus()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    SessionManager manager(&registry);
    QSignalSpy returned(&manager, &SessionManager::returnedHome);
    QSignalSpy rejected(&manager, &SessionManager::sessionRejected);
    QSignalSpy failed(&manager, &SessionManager::sessionFailed);
    QVERIFY(manager.launch("switch"));
    QVERIFY(manager.hasActiveSession());
    QCOMPARE(manager.currentAppId(), QString("switch"));
    manager.exitCurrent();
    QVERIFY(!manager.hasActiveSession());
    QCOMPARE(manager.previousFocusId(), QString("switch"));
    QCOMPARE(returned.count(), 1);
    QCOMPARE(returned.takeFirst().at(0).toString(), QString("switch"));
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(failed.count(), 0);
}

void IsoldeTests::secondLaunchIsRejected()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    SessionManager manager(&registry);
    QVERIFY(manager.launch("switch"));
    QVERIFY(!manager.launch("youtube"));
    QCOMPARE(manager.currentAppId(), QString("switch"));
}
void IsoldeTests::launchGuardRunsBeforeHandoff()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    SessionManager manager(&registry);
    QSignalSpy launching(&manager, &SessionManager::launching);
    QSignalSpy failed(&manager, &SessionManager::sessionFailed);
    QSignalSpy deferred(&manager, &SessionManager::launchDeferred);
    QSignalSpy rejected(&manager, &SessionManager::sessionRejected);
    manager.setLaunchGuard([](const QString &) { return QString("Wait for clean shutdown"); });
    QVERIFY(!manager.launch("switch"));
    QVERIFY(!manager.hasActiveSession());
    QCOMPARE(launching.count(), 0);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(deferred.count(), 1);
    QCOMPARE(rejected.count(), 1);
    manager.setLaunchGuard({});
    QVERIFY(manager.launch("switch"));
    QCOMPARE(launching.count(), 1);
}

void IsoldeTests::customizationSessionIsSupported()
{
    AppRegistry registry;
    QVERIFY(registry.loadFromData(R"([{"id":"settings","name":"Settings","type":"settings"}])"));
    SessionManager manager(&registry);
    QVERIFY(manager.launch("settings"));
    QCOMPARE(manager.currentAppType(), QString("settings"));
}

void IsoldeTests::catalogPersistsInstallLifecycle()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1.2"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto packageDirectory = directory.filePath("packages/weather");
    QVERIFY(QDir().mkpath(packageDirectory));
    const auto catalogPath = directory.filePath("catalog.json");
    const auto statePath = directory.filePath("installed.json");
    const auto installRoot = directory.filePath("installed-packages");
    QFile catalogFile(catalogPath);
    QVERIFY(catalogFile.open(QIODevice::WriteOnly));
    catalogFile.write(R"({"schemaVersion":1,"packages":["packages/weather/manifest.json"]})");
    catalogFile.close();
    QFile manifestFile(QDir(packageDirectory).filePath("manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly));
    manifestFile.write(R"({"schemaVersion":1,"packageId":"weather","version":"1.0.0","minIsoldeVersion":"0.0.1.2","name":"Weather","description":"Weather display","category":"Info","publisher":"Test","capabilities":[],"app":{"id":"weather","type":"mock","section":"Apps","accent":"#123456","symbol":"W"}})");
    manifestFile.close();

    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    AppCatalog catalog(&registry);
    QVERIFY(catalog.load(catalogPath, statePath, installRoot));
    QVERIFY(catalog.install("weather"));
    QVERIFY(catalog.isInstalled("weather"));
    QVERIFY(registry.find("weather"));
    QVERIFY(QFile::exists(QDir(installRoot).filePath("weather/1.0.0/manifest.json")));

    AppRegistry restoredRegistry;
    QVERIFY(restoredRegistry.loadFromData(config));
    AppCatalog restored(&restoredRegistry);
    QVERIFY(restored.load(catalogPath, statePath, installRoot));
    QVERIFY(restored.isInstalled("weather"));
    QVERIFY(restoredRegistry.find("weather"));
    QVERIFY(restored.uninstall("weather"));
    QVERIFY(!restoredRegistry.find("weather"));
    QVERIFY(!QDir(installRoot).exists("weather"));
}

void IsoldeTests::catalogTracksSetupAndFailureState()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1.2"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath("packages/kodi")));
    QFile catalogFile(directory.filePath("catalog.json"));
    QVERIFY(catalogFile.open(QIODevice::WriteOnly));
    catalogFile.write(R"({"schemaVersion":1,"packages":["packages/kodi/manifest.json"]})");
    catalogFile.close();
    QFile manifestFile(directory.filePath("packages/kodi/manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly));
    manifestFile.write(R"({"schemaVersion":1,"packageId":"kodi","version":"21.3","minIsoldeVersion":"0.0.1.2","name":"Kodi","description":"Media center","category":"Photo & Video","publisher":"Kodi Foundation","provenance":"Ubuntu package","setup":{"required":true,"prompt":"Choose a media source before everyday use."},"capabilities":["desktop.external","video.output"],"app":{"id":"kodi","type":"desktop","presentationMode":"media","handledActions":["open-folder"],"actionHandlers":{"open-folder":{"appendTarget":true}},"section":"Library","accent":"#1685D9","symbol":"K","desktopId":"isolde-kodi","windowClass":"Kodi.Kodi","containment":"fullscreen"}})");
    manifestFile.close();

    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    AppCatalog catalog(&registry);
    const auto statePath = directory.filePath("state.json");
    const auto installRoot = directory.filePath("installed");
    QVERIFY(catalog.load(directory.filePath("catalog.json"), statePath, installRoot));
    QCOMPARE(catalog.entries().first().toMap().value("state").toString(), QString("available"));
    QCOMPARE(catalog.entries().first().toMap().value("setupRequired").toBool(), true);
    QVERIFY(catalog.entries().first().toMap().value("setupPrompt").toString().contains(QStringLiteral("media source")));
    QVERIFY(catalog.install("kodi"));
    QCOMPARE(catalog.packageState("kodi"), QString("needs-setup"));
    QCOMPARE(catalog.entries().first().toMap().value("state").toString(), QString("needs-setup"));
    QVERIFY(catalog.completeSetup("kodi"));
    QCOMPARE(catalog.packageState("kodi"), QString("ready"));
    QVERIFY(catalog.markFailed("kodi", "Desktop entry was not found."));
    QCOMPARE(catalog.packageState("kodi"), QString("failed"));
    QVERIFY(catalog.entries().first().toMap().value("failureReason").toString().contains(QStringLiteral("Desktop entry")));

    AppRegistry restoredRegistry;
    QVERIFY(restoredRegistry.loadFromData(config));
    AppCatalog restored(&restoredRegistry);
    QVERIFY(restored.load(directory.filePath("catalog.json"), statePath, installRoot));
    QCOMPARE(restored.packageState("kodi"), QString("failed"));
    QVERIFY(restored.completeSetup("kodi"));
    QCOMPARE(restored.packageState("kodi"), QString("ready"));
}

void IsoldeTests::catalogRejectsInvalidManifest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath("packages/bad")));
    QFile catalog(directory.filePath("catalog.json"));
    QVERIFY(catalog.open(QIODevice::WriteOnly));
    catalog.write(R"({"schemaVersion":1,"packages":["packages/bad/manifest.json"]})");
    catalog.close();
    QFile manifest(directory.filePath("packages/bad/manifest.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"schemaVersion":1,"packageId":"../unsafe","version":"broken"})");
    manifest.close();
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    AppCatalog appCatalog(&registry);
    QVERIFY(!appCatalog.load(directory.filePath("catalog.json"), directory.filePath("state.json"), directory.filePath("installed")));
    QVERIFY(!appCatalog.errorString().isEmpty());
}

void IsoldeTests::catalogRejectsUnsafeDesktopManifest()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1.2"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath("packages/spotify")));
    QFile catalog(directory.filePath("catalog.json"));
    QVERIFY(catalog.open(QIODevice::WriteOnly));
    catalog.write(R"({"schemaVersion":1,"packages":["packages/spotify/manifest.json"]})");
    catalog.close();
    QFile manifest(directory.filePath("packages/spotify/manifest.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"schemaVersion":1,"packageId":"spotify","version":"1.0.0","minIsoldeVersion":"0.0.1.2","name":"Spotify","description":"Desktop launch test","category":"Music","publisher":"Test","capabilities":["desktop.external"],"app":{"id":"spotify","type":"desktop","section":"Music","accent":"#1db954","symbol":"S","desktopId":"../../bin/sh","mprisBusName":"org.mpris.MediaPlayer2.spotify"}})");
    manifest.close();
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    AppCatalog appCatalog(&registry);
    QVERIFY(!appCatalog.load(directory.filePath("catalog.json"), directory.filePath("state.json"), directory.filePath("installed")));
    QVERIFY(!appCatalog.errorString().isEmpty());
}

void IsoldeTests::catalogAcceptsKodiMediaPackage()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1.2"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath("packages/kodi")));
    QFile catalog(directory.filePath("catalog.json"));
    QVERIFY(catalog.open(QIODevice::WriteOnly));
    catalog.write(R"({"schemaVersion":1,"packages":["packages/kodi/manifest.json"]})");
    catalog.close();
    QFile manifest(directory.filePath("packages/kodi/manifest.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"schemaVersion":1,"packageId":"kodi","version":"21.3","minIsoldeVersion":"0.0.1.2","name":"Kodi","description":"Media center","category":"Photo & Video","publisher":"Kodi Foundation","capabilities":["desktop.external","video.output"],"app":{"id":"kodi","type":"desktop","presentationMode":"media","handledActions":["open-folder","open-file","open-disc"],"actionHandlers":{"open-folder":{"appendTarget":true},"open-file":{"appendTarget":true},"open-disc":{"appendTarget":true}},"section":"Library","accent":"#1685D9","symbol":"K","desktopId":"isolde-kodi","windowClass":"Kodi.Kodi","containment":"fullscreen"}})");
    manifest.close();
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    AppCatalog appCatalog(&registry);
    QVERIFY(appCatalog.load(directory.filePath("catalog.json"), directory.filePath("state.json"), directory.filePath("installed")));
    QCOMPARE(appCatalog.entries().first().toMap().value("presentationMode").toString(), QString("media"));
    QVERIFY(appCatalog.install("kodi"));
    QVERIFY(registry.find("kodi"));
    QCOMPARE(registry.find("kodi")->launchConfiguration.value("desktopId").toString(), QString("isolde-kodi"));
    QCOMPARE(registry.find("kodi")->launchConfiguration.value("windowClass").toString(), QString("Kodi.Kodi"));
    QVERIFY(registry.find("kodi")->handledActions.contains(QStringLiteral("open-disc")));
    const auto handlers = registry.find("kodi")->launchConfiguration.value("actionHandlers").toMap();
    QVERIFY(handlers.value("open-disc").toMap().value("appendTarget").toBool());
}

void IsoldeTests::homeLayoutPersistsAndRecovers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto defaultPath = directory.filePath("default.json");
    const auto userPath = directory.filePath("user.json");
    QFile defaults(defaultPath);
    QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"brandVisible":false,"backgroundStart":"#111111","backgroundEnd":"#222222"},"items":[{"kind":"widget","id":"clock","widgetType":"clock","size":"standard"},{"kind":"app","id":"youtube","appId":"youtube","size":"standard"}]})");
    defaults.close();
    AppRegistry registry;
    QVERIFY(registry.loadFromData(config));
    HomeLayout layout(&registry);
    QVERIFY(layout.load(defaultPath, userPath));
    QCOMPARE(layout.items().size(), 2);
    QVERIFY(!layout.brandVisible());
    const auto categoryId = layout.addCategory("Streaming Apps");
    QVERIFY(!categoryId.isEmpty());
    QVERIFY(layout.assignItemCategory("youtube", categoryId));
    QVERIFY(layout.renameCategory(categoryId, "Entertainment"));
    const auto secondCategoryId = layout.addCategory("Inputs");
    QVERIFY(layout.moveCategory(secondCategoryId, -1));
    QVERIFY(layout.moveItem("youtube", -1));
    QVERIFY(layout.setBrandVisible(true));
    QVERIFY(layout.setItemSize("youtube", "wide"));
    QVERIFY(layout.setItemOpacity("youtube", 0.42));
    QVERIFY(layout.setTileOption("youtube", "showTitle", false));
    QVERIFY(layout.setTileOption("youtube", "showType", false));
    QVERIFY(layout.setTileOption("youtube", "showLogo", true));
    QVERIFY(layout.setTileOption("youtube", "logoFill", true));
    const auto pngPath = directory.filePath("custom.png");
    QImage png(2, 2, QImage::Format_ARGB32);
    png.fill(Qt::transparent);
    png.setPixelColor(0, 0, QColor("#ff0000"));
    QVERIFY(png.save(pngPath, "PNG"));
    QVERIFY(layout.setTileLogo("youtube", pngPath));
    QVERIFY(layout.setBackground("#123456", "#010203"));
    QVERIFY(layout.setTileRoundness(0.75));
    QVERIFY(layout.addWidget("clock"));

    HomeLayout restored(&registry);
    QVERIFY(restored.load(defaultPath, userPath));
    QVERIFY(restored.brandVisible());
    QCOMPARE(restored.items().first().toMap().value("id").toString(), QString("youtube"));
    QCOMPARE(restored.items().first().toMap().value("size").toString(), QString("wide"));
    QCOMPARE(restored.items().first().toMap().value("opacity").toDouble(), 0.42);
    QCOMPARE(restored.items().first().toMap().value("showTitle").toBool(), false);
    QCOMPARE(restored.items().first().toMap().value("showType").toBool(), false);
    QCOMPARE(restored.items().first().toMap().value("logoFill").toBool(), true);
    QVERIFY(!restored.items().first().toMap().value("logoSource").toString().isEmpty());
    const QImage preservedLogo(QUrl(restored.items().first().toMap().value("logoSource").toString()).toLocalFile());
    QVERIFY(!preservedLogo.isNull());
    QCOMPARE(preservedLogo.pixelColor(1, 1).alpha(), 0);
    QVERIFY(restored.clearTileLogo("youtube"));
    QCOMPARE(restored.backgroundStart(), QString("#123456"));
    QCOMPARE(restored.tileRoundness(), 0.75);
    QCOMPARE(restored.items().size(), 3);
    QCOMPARE(restored.categories().size(), 2);
    QCOMPARE(restored.categories().first().toMap().value("name").toString(), QString("Inputs"));
    QCOMPARE(restored.sections().first().toMap().value("name").toString(), QString("Entertainment"));
    QVERIFY(restored.removeCategory(categoryId));
    QVERIFY(restored.removeCategory(secondCategoryId));
    QVERIFY(restored.categories().isEmpty());
    QCOMPARE(restored.items().first().toMap().value("categoryId").toString(), QString());
    QVERIFY(restored.removeItem("clock2"));

    QFile corrupt(userPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
    corrupt.write("not-json");
    corrupt.close();
    HomeLayout recovered(&registry);
    QVERIFY(recovered.load(defaultPath, userPath));
    QVERIFY(!recovered.brandVisible());
    QCOMPARE(recovered.items().first().toMap().value("id").toString(), QString("clock"));
    QVERIFY(!recovered.errorString().isEmpty());
}

void IsoldeTests::displaySettingsPersistAndClamp()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IsoldeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("DisplaySettingsTest"));
    QSettings().clear();

    DisplaySettings settings;
    QCOMPARE(settings.safeArea(), 4);
    QCOMPARE(settings.uiScale(), 100);
    QVERIFY(!settings.reduceMotion());

    settings.adjustSafeArea(20);
    settings.adjustUiScale(-40);
    settings.toggleReduceMotion();
    QCOMPARE(settings.safeArea(), 12);
    QCOMPARE(settings.uiScale(), 85);
    QVERIFY(settings.reduceMotion());

    DisplaySettings restored;
    QCOMPARE(restored.safeArea(), 12);
    QCOMPARE(restored.uiScale(), 85);
    QVERIFY(restored.reduceMotion());

    restored.reset();
    QCOMPARE(restored.safeArea(), 4);
    QCOMPARE(restored.uiScale(), 100);
    QVERIFY(!restored.reduceMotion());
}

void IsoldeTests::audioSettingsPersistAndClamp()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IsoldeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AudioSettingsTest"));
    QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", QCoreApplication::applicationName()).clear();

    AudioSettings settings;
    settings.setMixerEnabledForTests(false);
    QCOMPARE(settings.volume(), 50);
    QVERIFY(!settings.muted());
    QCOMPARE(settings.startupPolicy(), QString("remember"));
    QCOMPARE(settings.startupVolume(), 35);
    QCOMPARE(settings.volumeStep(), 5);
    QVERIFY(!settings.nightMode());
    QCOMPARE(settings.lipSyncMs(), 0);
    QCOMPARE(settings.volumeMode(), QString("isolde"));
    QCOMPARE(settings.tvReferenceVolume(), 50);
    QVERIFY(!settings.backendStatus().isEmpty());

    settings.adjustVolume(200);
    QCOMPARE(settings.volume(), 100);
    settings.toggleMute();
    QVERIFY(settings.muted());
    settings.adjustVolume(-90);
    QCOMPARE(settings.volume(), 10);
    QVERIFY(!settings.muted());
    settings.nextStartupPolicy();
    QCOMPARE(settings.startupPolicy(), QString("fixed"));
    settings.adjustStartupVolume(100);
    QCOMPARE(settings.startupVolume(), 100);
    settings.adjustVolumeStep(30);
    QCOMPARE(settings.volumeStep(), 20);
    settings.toggleNightMode();
    QVERIFY(settings.nightMode());
    settings.adjustLipSync(-1000);
    QCOMPARE(settings.lipSyncMs(), -250);
    settings.nextVolumeMode();
    QCOMPARE(settings.volumeMode(), QString("isolde"));
    settings.adjustTvReferenceVolume(100);
    QCOMPARE(settings.tvReferenceVolume(), 100);

    AudioSettings restored;
    restored.setMixerEnabledForTests(false);
    QCOMPARE(restored.volume(), 10);
    QCOMPARE(restored.startupPolicy(), QString("fixed"));
    QCOMPARE(restored.startupVolume(), 100);
    QCOMPARE(restored.volumeStep(), 20);
    QVERIFY(restored.nightMode());
    QCOMPARE(restored.lipSyncMs(), -250);
    QCOMPARE(restored.volumeMode(), QString("isolde"));
    QCOMPARE(restored.tvReferenceVolume(), 100);

    restored.reset();
    QCOMPARE(restored.volume(), 50);
    QVERIFY(!restored.muted());
    QCOMPARE(restored.startupPolicy(), QString("remember"));
    QCOMPARE(restored.startupVolume(), 35);
    QCOMPARE(restored.volumeStep(), 5);
    QVERIFY(!restored.nightMode());
    QCOMPARE(restored.lipSyncMs(), 0);
    QCOMPARE(restored.volumeMode(), QString("isolde"));
    QCOMPARE(restored.tvReferenceVolume(), 50);
}

void IsoldeTests::remoteSettingsPersistAndCycle()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IsoldeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("RemoteSettingsTest"));
    QSettings().clear();

    RemoteSettings settings;
    QCOMPARE(settings.profile(), QString("simple"));
    QVERIFY(!settings.setupMode());
    QCOMPARE(settings.lastInput(), QString("No button tested yet"));
    QCOMPARE(settings.requiredButtons().size(), 10);
    QVERIFY(settings.setupStatus().contains(QStringLiteral("Ready")));
    QCOMPARE(settings.powerAction(), QString("ambient"));
    QVERIFY(!settings.cecStatus().isEmpty());

    settings.setCecEnabledForTests(false);
    settings.toggleSetupMode();
    QVERIFY(settings.setupMode());
    QVERIFY(settings.setupStatus().contains(QStringLiteral("Listening")));
    settings.nextProfile();
    QCOMPARE(settings.profile(), QString("media"));
    settings.nextProfile();
    QCOMPARE(settings.profile(), QString("gamepad"));
    settings.recordTestInput("Volume +");
    QCOMPARE(settings.lastInput(), QString("Volume +"));
    settings.nextPowerAction();
    QCOMPARE(settings.powerAction(), QString("tv-standby"));
    QVERIFY(!settings.sendTvPowerCommand());
    QVERIFY(settings.cecStatus().contains(QStringLiteral("tests")));
    settings.nextPowerAction();
    QCOMPARE(settings.powerAction(), QString("tv-on"));

    RemoteSettings restored;
    restored.setCecEnabledForTests(false);
    QVERIFY(restored.setupMode());
    QCOMPARE(restored.profile(), QString("gamepad"));
    QCOMPARE(restored.lastInput(), QString("Volume +"));
    QCOMPARE(restored.powerAction(), QString("tv-on"));

    restored.reset();
    QCOMPARE(restored.profile(), QString("simple"));
    QVERIFY(!restored.setupMode());
    QCOMPARE(restored.lastInput(), QString("No button tested yet"));
    QCOMPARE(restored.powerAction(), QString("ambient"));
}

void IsoldeTests::ambientSettingsPersistAndClamp()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IsoldeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AmbientSettingsTest"));
    QSettings().clear();

    AmbientSettings settings;
    QVERIFY(settings.enabled());
    QCOMPARE(settings.timeoutMinutes(), 5);
    QCOMPARE(settings.mode(), QString("slideshow"));
    QCOMPARE(settings.slideshowSeconds(), 15);
    QVERIFY(settings.showClock());
    QVERIFY(!settings.photoDirectory().isEmpty());

    settings.adjustTimeout(200);
    settings.adjustSlideshowSeconds(-100);
    settings.nextMode();
    settings.nextImageFit();
    settings.toggleShuffle();
    QVERIFY(settings.importSamplePhotos());
    QVERIFY(settings.photoCount() >= 3);
    settings.toggleClock();
    settings.toggleDate();
    QCOMPARE(settings.timeoutMinutes(), 120);
    QCOMPARE(settings.slideshowSeconds(), 5);
    QCOMPARE(settings.mode(), QString("gradient"));
    QCOMPARE(settings.imageFit(), QString("fit"));
    QVERIFY(settings.shuffle());
    QVERIFY(!settings.showClock());
    QVERIFY(!settings.showDate());

    AmbientSettings restored;
    QCOMPARE(restored.timeoutMinutes(), 120);
    QCOMPARE(restored.slideshowSeconds(), 5);
    QCOMPARE(restored.mode(), QString("gradient"));
    QCOMPARE(restored.imageFit(), QString("fit"));
    QVERIFY(restored.shuffle());
    QVERIFY(!restored.showClock());

    restored.reset();
    QCOMPARE(restored.timeoutMinutes(), 5);
    QCOMPARE(restored.slideshowSeconds(), 15);
    QCOMPARE(restored.mode(), QString("slideshow"));
    QCOMPARE(restored.imageFit(), QString("fill"));
    QVERIFY(!restored.shuffle());
    QVERIFY(restored.showClock());
}

void IsoldeTests::profilesPersistAndIsolatePersonalSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto previous = qApp->property("isoldeProfile");
    ProfileManager profiles(directory.path());
    QCOMPARE(profiles.activeName(), QString("Household"));
    const auto householdPath = profiles.layoutPath();
    QVERIFY(!profiles.createProfile("  "));
    QVERIFY(profiles.createProfile("Alex"));
    QVERIFY(!profiles.createProfile("alex"));
    const auto alex = profiles.profiles().last().toMap().value("id").toString();
    QSignalSpy switched(&profiles, &ProfileManager::switchRequested);
    QVERIFY(!profiles.selectProfile("../escape"));
    QVERIFY(profiles.selectProfile(alex));
    QCOMPARE(switched.count(), 1);
    QVERIFY(profiles.layoutPath() != householdPath);
    AppRegistry registry;
    const auto configPath = directory.filePath("apps.json");
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write(config); file.close();
    QVERIFY(registry.load(configPath));
    HomeLayout alexLayout(&registry);
    QVERIFY(alexLayout.load({}, profiles.layoutPath()));
    QVERIFY(alexLayout.setBackground("#112233", "#445566"));
    AmbientSettings alexAmbient;
    alexAmbient.adjustTimeout(12);
    const auto alexTimeout = alexAmbient.timeoutMinutes();
    QVERIFY(profiles.renameProfile("Alex TV"));
    QVERIFY(profiles.selectProfile("default"));
    HomeLayout household(&registry);
    QVERIFY(household.load({}, householdPath));
    QVERIFY(household.backgroundStart() != "#112233");
    AmbientSettings householdAmbient;
    const auto householdTimeout = householdAmbient.timeoutMinutes();
    // An outgoing object must keep writing to its original profile.
    alexAmbient.adjustTimeout(1);
    AmbientSettings householdAgain;
    QCOMPARE(householdAgain.timeoutMinutes(), householdTimeout);
    QVERIFY(profiles.selectProfile(alex));
    ProfileManager restored(directory.path());
    QCOMPARE(restored.activeName(), QString("Alex TV"));
    HomeLayout restoredLayout(&registry);
    QVERIFY(restoredLayout.load({}, restored.layoutPath()));
    QCOMPARE(restoredLayout.backgroundStart(), QString("#112233"));
    AmbientSettings restoredAmbient;
    QCOMPARE(restoredAmbient.timeoutMinutes(), alexTimeout + 1);
    QSettings settings;
    settings.remove(ProfileManager::ambientGroup());
    qApp->setProperty("isoldeProfile", previous);
}

QTEST_GUILESS_MAIN(IsoldeTests)
#include "tst_isolde.moc"
