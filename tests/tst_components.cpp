#include "home/ComponentSchema.h"
#include "home/ActionDispatcher.h"
#include "home/HomeLayout.h"
#include "home/PageModel.h"
#include "sessions/SessionManager.h"
#include "media/KodiMedia.h"
#include "media/CaptureController.h"
#include "media/KodiService.h"
#include "home/StudioSession.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>

static const QByteArray apps = R"([
 {"id":"movies","name":"Movies","type":"mock","section":"Library","accent":"#556677","handledActions":["open-folder","open-file","open-disc"]},
 {"id":"other","name":"Other","type":"mock","section":"Apps","accent":"#445566","handledActions":["open-folder"]},
 {"id":"capture-test","name":"Capture test","type":"mock","presentationMode":"capture","section":"Inputs","accent":"#445566"},
 {"id":"settings","name":"Settings","type":"settings","section":"System","accent":"#556677"},
 {"id":"app-store","name":"Store","type":"store","section":"System","accent":"#556677"}
])";
static const QByteArray legacy = R"({"schemaVersion":1,"theme":{"backgroundStart":"#123456","backgroundEnd":"#010203","tileRoundness":0.73},
 "categories":[{"id":"personal","name":"Personal"}],"items":[
 {"id":"my-folder","kind":"action","name":"My films","categoryId":"personal","size":"wide","opacity":0.42,"showTitle":false,"customLogo":"artwork/my-folder.png","action":{"type":"open-folder","target":"/media/films","openWith":"gone"}},
 {"id":"my-app","kind":"app","appId":"absent","size":"compact"},
 {"id":"clock","kind":"widget","widgetType":"clock"}]})";
static void writeFile(const QString &path, const QByteArray &data) { QFile file(path); if (file.open(QIODevice::WriteOnly)) file.write(data); }
static QByteArray readFile(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }

class ComponentTests : public QObject {
    Q_OBJECT
private slots:
    void validatesContracts();
    void compositionPersistenceAndValidation();
    void universalGridValidatesAndPersists();
    void migratesAndRepairsWithoutLosingPresentation();
    void preservesMissingCollectionAndEmptyHome();
    void rejectsConflictingVersionedLayout();
    void dispatchesValidatedActionsAndOrigins();
    void failedEditRetainsProfileLayout();
    void skinSnapshotsSurviveResetAndArtworkChanges();
    void skinsRejectInvalidAndFailedApplications();
    void pagesPersistAndRouteAndTravelWithSkins();
    void studioSessionDraftUndoBoundsAndNoWrite();
    void studioSessionCanonicalIdsAndRevisionConflict();
    void studioApplyAtomicFailureAndFocusPreserved();
    void studioCommandsAreAtomicAndProfileScoped();
    void studioCollisionProposalsAndParity();
    void studioSectionGridsAreIndependentAndAtomic();
    void studioHomeDesignationIsAtomicAndProtected();
    void studioArtworkUsesActiveProfileAndExpires();
    void studioPageLifecycleRepairsLinksAndPreservesRoot();
    void studioChromeRemainsReadableAndProfileLocal();
    void studioHiddenAndLockedStatesAreRecoverable();
};
void ComponentTests::pagesPersistAndRouteAndTravelWithSkins()
{
    QTemporaryDir dir;AppRegistry registry;QVERIFY(registry.loadFromData(apps));
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json");
    HomeLayout layout(&registry);QVERIFY(layout.load(defaults,dir.filePath("home.json")));
    const auto original=layout.components();
    const auto movies=layout.createPage("Cinema","movies");QVERIFY(!movies.isEmpty());
    const auto shows=layout.createPage("Television","shows");QVERIFY(!shows.isEmpty());
    auto *page=qobject_cast<PageModel *>(layout.pageModel(movies));QVERIFY(page);
    QCOMPARE(page->sections().size(),1);QCOMPARE(page->sections()[0].toMap()["presentation"].toMap()["variant"].toString(),QString("library"));
    QVERIFY(page->setAppearance({{"focusColor","#123456"}}));
    QVERIFY(page->addWidget("text"));QVERIFY(page->addWidget("clock"));
    QVERIFY(page->addButton());const auto button=page->items().last().toMap()["id"].toString();
    QVERIFY(page->setItemName(button,"Visit television"));QVERIFY2(page->setItemAction(button,{{"type","open-page"},{"pageId",shows}}),qPrintable(page->errorString()));
    QVERIFY(page->setItemPresentation("text",{{"variant","text"},{"subtitle","Your own cinema"}}));
    QVERIFY(!page->addLibraryWidget("Keep watching","movies").isEmpty());
    // Page-scoped search sources: the movies preset library section plus the added library widget.
    const auto searchSources=page->pageSearchSources();
    QCOMPARE(searchSources.size(),2);
    bool sawSection=false,sawWidget=false;
    for(const auto &value:searchSources){
        const auto source=value.toMap();
        QCOMPARE(source["kind"].toString(),QString("movies"));
        QCOMPARE(source["view"].toString(),QString("all"));
        if(source["id"].toString()==QString("library")){sawSection=true;QCOMPARE(source["name"].toString(),QString("Movies"));}
        if(source["id"].toString().startsWith("library-")){sawWidget=true;QCOMPARE(source["name"].toString(),QString("Keep watching"));}
    }
    QVERIFY(sawSection);QVERIFY(sawWidget);
    // Grid/placement regression: page-scoped grid and placements persist through the page document.
    QVERIFY(page->setGrid({{"columns",8},{"rows",5}}));
    QCOMPARE(page->grid()["columns"].toInt(),8);
    QCOMPARE(page->grid()["rows"].toInt(),5);
    const auto firstItem=page->items().first().toMap()["id"].toString();
    QVERIFY(!firstItem.isEmpty());
    QVERIFY(page->setItemPlacement(firstItem,{{"x",0},{"y",0},{"width",2},{"height",1}}));
    const auto placements=page->placements();
    QCOMPARE(placements.size(),1);
    QCOMPARE(placements[0].toMap()["id"].toString(),firstItem);
    QCOMPARE(placements[0].toMap()["placement"].toMap()["x"].toInt(),0);
    QCOMPARE(placements[0].toMap()["placement"].toMap()["y"].toInt(),0);
    QCOMPARE(placements[0].toMap()["placement"].toMap()["width"].toInt(),2);
    QCOMPARE(placements[0].toMap()["placement"].toMap()["height"].toInt(),1);
    QVERIFY(page->clearItemPlacement(firstItem));
    QCOMPARE(page->placements().size(),0);
    // Grid/placements survive a reload through storePage()/loadDocument().
    QVERIFY(page->setGrid({{"columns",8},{"rows",5}}));
    QVERIFY(page->setItemPlacement(firstItem,{{"x",1},{"y",1},{"width",3},{"height",2}}));
    QVERIFY(page->storePage());
    HomeLayout reloaded(&registry);QVERIFY(reloaded.load(defaults,dir.filePath("home.json")));
    auto *reloadedPage=qobject_cast<PageModel *>(reloaded.pageModel(movies));QVERIFY(reloadedPage);
    QCOMPARE(reloadedPage->grid()["columns"].toInt(),8);
    QCOMPARE(reloadedPage->grid()["rows"].toInt(),5);
    const auto reloadedPlacements=reloadedPage->placements();
    QCOMPARE(reloadedPlacements.size(),1);
    QCOMPARE(reloadedPlacements[0].toMap()["placement"].toMap()["x"].toInt(),1);
    QCOMPARE(reloadedPlacements[0].toMap()["placement"].toMap()["y"].toInt(),1);
    QCOMPARE(reloadedPlacements[0].toMap()["placement"].toMap()["width"].toInt(),3);
    QCOMPARE(reloadedPlacements[0].toMap()["placement"].toMap()["height"].toInt(),2);
    QCOMPARE(layout.components(),original);QCOMPARE(page->sections().size(),3);
    QVERIFY(layout.linkPage(movies,true));QVERIFY(layout.linkPage(shows,false));
    auto config=layout.composition();config["moviePage"]=movies;QVERIFY(layout.setComposition(config));
    HomeLayout restored(&registry);QVERIFY(restored.load(defaults,dir.filePath("home.json")));
    QCOMPARE(restored.pages(),layout.pages());
    auto *restoredPage=qobject_cast<PageModel *>(restored.pageModel(movies));QVERIFY(restoredPage);
    QCOMPARE(restoredPage->appearance()["focusColor"].toString(),QString("#123456"));
    const auto snapshot=layout.saveSkin("Whole interface");QVERIFY(!snapshot.isEmpty());
    QVERIFY(layout.applySkin("builtin-pastel"));QCOMPARE(layout.pages().size(),2); // Home presets preserve authored pages.
    const auto linkedState=layout.committedDocument();
    QVERIFY(!layout.removePage(shows));QCOMPARE(layout.committedDocument(),linkedState);
    QVERIFY(page->setItemAction(button,{{"type","open-page"},{"pageId",movies}})); // Explicitly repair the incoming link.
    QVERIFY(layout.removePage(shows));QVERIFY(!layout.pageModel(shows));
    QVERIFY(layout.applySkin(snapshot));QCOMPARE(layout.pages().size(),2);
    QCOMPARE(layout.pageModel(movies),page); // Existing bindings survive a skin switch.
    QCOMPARE(page->appearance()["focusColor"].toString(),QString("#123456"));
    SessionManager sessions(&registry);KodiMedia media("pages-test",nullptr,dir.path());KodiService service("pages-test");
    ActionDispatcher dispatcher(&registry,&sessions,&media,&service);dispatcher.setLayout(&layout);
    QSignalSpy requested(&dispatcher,&ActionDispatcher::pageRequested);QSignalSpy rejected(&dispatcher,&ActionDispatcher::rejected);
    QVERIFY(dispatcher.dispatch({{"type","open-page"},{"pageId",movies}},"my-link"));QCOMPARE(requested.last()[0].toString(),movies);
    QCOMPARE(requested.last()[1].toString(),QString("my-link"));
    QVERIFY(dispatcher.dispatch({{"type","launch-app"},{"appId","movies"}},"movie-button"));QCOMPARE(requested.size(),2);
    QVERIFY(!dispatcher.dispatch({{"type","open-page"},{"pageId","missing"}}));QCOMPARE(rejected.size(),1);
    QVERIFY(ComponentSchema::action({{"type","open-page"},{"pageId","../bad"}}).isEmpty());
    // Failed child writes roll back both page and root documents.
    const auto before=page->appearance();const auto pages=layout.pages();
    QFile::remove(dir.filePath("home.json"));QVERIFY(QDir().mkdir(dir.filePath("home.json")));
    QVERIFY(!page->setAppearance({{"focusColor","#654321"}}));QCOMPARE(page->appearance(),before);QCOMPARE(layout.pages(),pages);
}
void ComponentTests::skinSnapshotsSurviveResetAndArtworkChanges()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(layout.load(defaults,dir.filePath("home.json")));
    QCOMPARE(layout.skins().size(),3);
    QVERIFY(layout.applySkin("builtin-lounge"));
    QCOMPARE(layout.categories().size(),4);
    QVERIFY(layout.setAppearance({{"fontFamily","Serif"},{"focusColor","#123456"}}));
    const auto id = layout.saveSkin("  Evening  "); QVERIFY(!id.isEmpty());
    const auto savedComposition = layout.composition();
    const auto savedItems = layout.components();
    QVERIFY(layout.applySkin("builtin-pastel"));
    QVERIFY(!layout.wallpaperSource().isEmpty());
    QVERIFY(layout.resetToDefaults()); QCOMPARE(layout.skins().size(),4);
    // The snapshot is independent of the live artwork store, and survives a process reload.
    QDir(dir.filePath("artwork")).removeRecursively();
    HomeLayout restored(&registry); QVERIFY(restored.load(defaults,dir.filePath("home.json")));
    QVERIFY(restored.applySkin(id));
    QCOMPARE(restored.appearance()["fontFamily"].toString(),QString("Serif"));
    QCOMPARE(restored.appearance()["focusColor"].toString(),QString("#123456"));
    QCOMPARE(restored.composition(),savedComposition);
    QCOMPARE(restored.components(),savedItems);
    QVERIFY(restored.renameSkin(id,"Movie night"));
    QCOMPARE(restored.skins().last().toMap()["name"].toString(),QString("Movie night"));
    const auto live = readFile(dir.filePath("home.json"));
    QVERIFY(restored.deleteSkin(id)); QCOMPARE(readFile(dir.filePath("home.json")),live);
    QVERIFY(!restored.deleteSkin("builtin-default"));
    QVERIFY(restored.applySkin("builtin-default")); QVERIFY(restored.composition().isEmpty());
    HomeLayout other(&registry); QVERIFY(other.load(defaults,dir.filePath("other/home.json"))); QCOMPARE(other.skins().size(),3);
}
void ComponentTests::skinsRejectInvalidAndFailedApplications()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry); const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(layout.load(defaults,dir.filePath("home.json"))); QVERIFY(layout.setAppearance({{"focusColor","#123456"}}));
    const auto live = readFile(dir.filePath("home.json")); const auto items = layout.components();
    QVERIFY(layout.saveSkin("  ").isEmpty()); QVERIFY(layout.saveSkin(QString(81,'x')).isEmpty());
    QVERIFY(!layout.applySkin("../home")); QCOMPARE(readFile(dir.filePath("home.json")),live);
    const auto id = layout.saveSkin("Test"); QVERIFY(!id.isEmpty());
    const auto path = dir.filePath("skins/" + id + ".json");
    auto skin = QJsonDocument::fromJson(readFile(path)).object();
    auto document = skin["layout"].toObject(); document["composition"] = QJsonObject{{"schemaVersion",1},{"wallpaper","artwork/missing.png"}}; skin["layout"] = document;
    writeFile(path,QJsonDocument(skin).toJson());
    QVERIFY(!layout.applySkin(id)); QCOMPARE(layout.components(),items); QCOMPARE(readFile(dir.filePath("home.json")),live);
    // A valid skin must roll back memory as well as disk if the profile cannot be written.
    QFile::remove(dir.filePath("home.json")); QVERIFY(QDir().mkdir(dir.filePath("home.json")));
    QVERIFY(!layout.applySkin("builtin-pastel")); QCOMPARE(layout.components(),items);
    QCOMPARE(layout.appearance()["focusColor"].toString(),QString("#123456"));
}
void ComponentTests::validatesContracts()
{
    for (const auto &value : QVariantList{
        QVariantMap{{"type", "shell"}, {"target", "touch /tmp/no"}},
        QVariantMap{{"type", "launch-app"}, {"appId", "../bad"}},
        QVariantMap{{"schemaVersion", 2}, {"type", "launch-app"}, {"appId", "movies"}},
        QVariantMap{{"type", "open-url"}, {"target", "javascript:alert(1)"}},
        QVariantMap{{"type", "open-url"}, {"target", "https://user:secret@example.com"}},
        QVariantMap{{"type", "open-file"}, {"target", "relative/file"}},
        QVariantMap{{"type", "play-movie"}, {"collectionId", "good"}, {"movieId", 1.5}},
        QVariantMap{{"type", "open-show"}, {"collectionId", "good"}, {"tvShowId", 0}},
        QVariantMap{{"type", "switch-input"}, {"input", 9}},
        QVariantMap{{"type", "switch-input"}, {"input", 2.5}},
        QVariantMap{{"type", "settings"}, {"section", 100}}}) {
        QString error;
        QVERIFY(ComponentSchema::action(value.toMap(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    auto tile = ComponentSchema::component({{"id", "movie-tile"}, {"kind", "app"}, {"appId", "movies"}}, true);
    QVERIFY(!tile.isEmpty());
    QCOMPARE(tile["componentType"].toString(), QString("launcher"));
    QCOMPARE(tile["source"].toMap()["provider"].toString(), QString("apps"));
    QVERIFY(!ComponentSchema::component(tile, false).isEmpty());
    tile["source"] = QVariantMap{{"schemaVersion", 1}, {"provider", "shell"}, {"kind", "shell"}};
    QVERIFY(ComponentSchema::component(tile, false).isEmpty());
    const auto old = ComponentSchema::action({{"type", "kodi-collection"}, {"collectionId", "test"}, {"movieId", 12}});
    QCOMPARE(old["type"].toString(), QString("play-movie"));
    const auto show = ComponentSchema::action({{"type", "open-show"}, {"collectionId", "shows"}, {"tvShowId", 12}});
    QCOMPARE(show["tvShowId"].toInt(), 12);
    const auto showTile = ComponentSchema::component({{"id", "show-card"}, {"kind", "action"}, {"name", "Fixture show"},
        {"action", show}}, true);
    QCOMPARE(showTile["source"].toMap()["kind"].toString(), QString("collection"));
    const auto inputAction = ComponentSchema::action({{"type", "switch-input"}, {"input", 4}});
    QCOMPARE(inputAction["input"].toInt(), 4);
    const auto inputTile = ComponentSchema::component({{"id", "hdmi-four"}, {"kind", "action"}, {"name", "Console"},
        {"action", inputAction}}, true);
    QCOMPARE(inputTile["source"].toMap()["provider"].toString(), QString("isolde"));
    QCOMPARE(inputTile["source"].toMap()["id"].toString(), QString("4"));
    QTemporaryDir dir;
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    writeFile(dir.filePath("defaults.json"), legacy);
    HomeLayout home(&registry);
    QVERIFY(home.load(dir.filePath("defaults.json"), dir.filePath("profile.json")));
    QVERIFY(home.addActionTile("Console", "switch-input", "4"));
    QVERIFY(home.items().last().toMap()["unavailableReason"].toString().contains("HDMI Inputs"));
    home.setCaptureInputsReady(true);
    QVERIFY(home.items().last().toMap()["unavailableReason"].toString().isEmpty());
}
void ComponentTests::compositionPersistenceAndValidation()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    writeFile(dir.filePath("default.json"), legacy);
    HomeLayout layout(&registry); QVERIFY(layout.load(dir.filePath("default.json"),dir.filePath("user.json")));
    const QVariantMap config{{"schemaVersion",1},{"enabled",true},{"columns",2},{"fit",true},
        {"navigation",QVariantList{QVariantMap{{"id","home"},{"name","My Home"}},QVariantMap{{"id","films"},{"name","Films"},{"sections",QVariantList{"personal"}}}}}};
    QVERIFY(layout.setComposition(config));
    QVERIFY(layout.setSectionPresentation("personal",{{"variant","media"},{"columns",3},{"height",250},{"span",1},{"subtitle","Keep watching"}}));
    QVERIFY(layout.setItemPresentation("my-folder",{{"progress",0.5},{"imageCrop",QVariantMap{{"x",0},{"y",1},{"width",100},{"height",90}}}}));
    const auto sections = layout.sections();
    QCOMPARE(sections[0].toMap()["presentation"].toMap()["columns"].toInt(),3);
    HomeLayout restored(&registry); QVERIFY(restored.load(dir.filePath("default.json"),dir.filePath("user.json")));
    QCOMPARE(restored.composition(),config); QCOMPARE(restored.sections(),sections);
    const auto original = readFile(dir.filePath("user.json"));
    auto invalid = config; invalid["navigation"] = QVariantList{QVariantMap{{"id","same"},{"name","A"}},QVariantMap{{"id","same"},{"name","B"}}};
    QVERIFY(!layout.setComposition(invalid));
    invalid = config; invalid["navigation"] = QVariantList{QVariantMap{{"id","bad"},{"name","Bad"},{"action",QVariantMap{{"type","shell"}}}}};
    QVERIFY(!layout.setComposition(invalid));
    QVERIFY(!layout.setSectionPresentation("personal",{{"height",-1}}));
    QVERIFY(!layout.setItemPresentation("my-folder",{{"progress",1.1}}));
    QVERIFY(!layout.setItemPresentation("my-folder",{{"imageCrop",QVariantMap{{"x",0},{"y",0},{"width",-1},{"height",100}}}}));
    QCOMPARE(readFile(dir.filePath("user.json")),original);
    auto jpg = ComponentSchema::component({{"id","art"},{"kind","app"},{"appId","movies"},{"customLogo","artwork/local.jpg"}},true);
    QVERIFY(!jpg.isEmpty()); jpg["customLogo"] = "artwork/../secret.jpg"; QVERIFY(ComponentSchema::component(jpg,true).isEmpty());
    QVERIFY(layout.resetToDefaults()); QVERIFY(layout.composition().isEmpty()); QVERIFY(!QFile::exists(dir.filePath("user.json")));
    auto pastel = config; pastel["wallpaper"] = "artwork/clouds.png"; pastel["headerMode"] = "identity";
    pastel["headerControls"] = QVariantList{"search","settings"}; pastel["showClock"] = true; pastel["designHeight"] = 720;
    QVERIFY(layout.setComposition(pastel)); QVERIFY(layout.wallpaperSource().endsWith("/artwork/clouds.png"));
    invalid = pastel; invalid["wallpaper"] = "../secret.png"; QVERIFY(!layout.setComposition(invalid));
    invalid = pastel; invalid["wallpaper"] = "https://example.com/wallpaper.png"; QVERIFY(!layout.setComposition(invalid));
    invalid = pastel; invalid["headerControls"] = QVariantList{"search"}; QVERIFY(!layout.setComposition(invalid));
    invalid = pastel; invalid["headerControls"] = QVariantList{"settings","settings"}; QVERIFY(!layout.setComposition(invalid));
    invalid = pastel; invalid["designHeight"] = "720"; QVERIFY(!layout.setComposition(invalid));
    QVERIFY(layout.setSectionPresentation("personal",{{"variant","orb"},{"panelOpacity",0.6},{"blur",0.8},{"padding",14},{"contentWidth",780}}));
    QVERIFY(!layout.setSectionPresentation("personal",{{"blur",1.1}}));
    QVERIFY(!layout.setSectionPresentation("personal",{{"trailingAction",QVariantMap{{"type","shell"}}}}));
    QVERIFY(layout.resetToDefaults()); QVERIFY(layout.wallpaperSource().isEmpty());
    const auto wallpaper = QFINDTESTDATA("visual/home-pastel-reference/reference.jpg");
    QVERIFY(!wallpaper.isEmpty()); QVERIFY(layout.setWallpaper(QUrl::fromLocalFile(wallpaper).toString()));
    QCOMPARE(readFile(QUrl(layout.wallpaperSource()).toLocalFile()),readFile(wallpaper));
    const auto savedWallpaper = layout.composition();
    QVERIFY(!layout.setWallpaper("https://example.com/clouds.png")); QCOMPARE(layout.composition(),savedWallpaper);
}
void ComponentTests::universalGridValidatesAndPersists()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    writeFile(dir.filePath("default.json"), legacy);
    HomeLayout layout(&registry); QVERIFY(layout.load(dir.filePath("default.json"), dir.filePath("user.json")));
    // The plan's proposed starting grid appears without any stored configuration, and reads never write.
    QCOMPARE(layout.grid().value("columns").toInt(), 8);
    QCOMPARE(layout.grid().value("rows").toInt(), 5);
    QCOMPARE(layout.grid().value("gap").toInt(), 12);
    QCOMPARE(layout.grid().value("margin").toInt(), 20);
    QCOMPARE(layout.grid().value("snap").toBool(), true);
    QCOMPARE(layout.grid().value("overlap").toBool(), false);
    QVERIFY(!QFile::exists(dir.filePath("user.json")));
    // Invalid grids are rejected and leave the composition untouched.
    QVERIFY(!layout.setGrid({{"columns",3}}));
    QVERIFY(!layout.setGrid({{"columns",17}}));
    QVERIFY(!layout.setGrid({{"columns",8.5}}));
    QVERIFY(!layout.setGrid({{"rows",2}}));
    QVERIFY(!layout.setGrid({{"rows",11}}));
    QVERIFY(!layout.setGrid({{"gap",-1}}));
    QVERIFY(!layout.setGrid({{"margin",101}}));
    QVERIFY(!layout.setGrid({{"snap","always"}}));
    QVERIFY(!layout.setGrid({{"mystery",1}}));
    QVERIFY(layout.composition().isEmpty());
    QVERIFY(!layout.errorString().isEmpty());
    // A valid grid merges with existing composition, persists and reloads.
    QVERIFY(layout.setComposition({{"schemaVersion",1},{"enabled",true},{"columns",2}}));
    QVERIFY2(layout.setGrid({{"columns",12},{"rows",6},{"gap",10}}), qPrintable(layout.errorString()));
    QCOMPARE(layout.composition().value("columns").toInt(), 2);
    QCOMPARE(layout.grid().value("margin").toInt(), 20);
    HomeLayout restored(&registry); QVERIFY(restored.load(dir.filePath("default.json"), dir.filePath("user.json")));
    QCOMPARE(restored.grid().value("columns").toInt(), 12);
    QCOMPARE(restored.grid().value("rows").toInt(), 6);
    QCOMPARE(restored.grid().value("gap").toInt(), 10);
    // A composition carrying an invalid grid is refused.
    auto invalidComposition = layout.composition();
    invalidComposition["grid"] = QVariantMap{{"columns",20}};
    QVERIFY(!layout.setComposition(invalidComposition));
    // Placements validate against the actual grid and survive reload.
    QVERIFY2(layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",3},{"height",2}}), qPrintable(layout.errorString()));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",11},{"y",0},{"width",4},{"height",1}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",0},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",-1},{"y",1},{"width",2},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0.5},{"y",1},{"width",2},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",2},{"height",2},{"z",0}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x","0"},{"y",1},{"width",2},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("absent", {{"x",5},{"y",0},{"width",1},{"height",1}}));
    // Collisions are refused while overlap is disabled, and the occupier is reported for the future editor dialog.
    QVERIFY(!layout.setItemPlacement("my-app", {{"x",1},{"y",1},{"width",2},{"height",1}}));
    QVERIFY(!layout.errorString().isEmpty());
    const auto collisions = layout.gridCollisions("my-app", {{"x",1},{"y",1},{"width",2},{"height",1}});
    QCOMPARE(collisions.size(), 1);
    QCOMPARE(collisions.first().toMap().value("id").toString(), QString("my-folder"));
    QCOMPARE(collisions.first().toMap().value("name").toString(), QString("My films"));
    QVERIFY(layout.gridCollisions("my-app", {{"x",11},{"y",0},{"width",1},{"height",1}}).isEmpty());
    QVERIFY(layout.gridCollisions("my-app", {{"x",0},{"y",0},{"width",0},{"height",1}}).isEmpty());
    QVERIFY2(layout.setItemPlacement("my-app", {{"x",9},{"y",0},{"width",3},{"height",1}}), qPrintable(layout.errorString()));
    // Shrinking the grid below placed widgets is refused and leaves the grid unchanged.
    QVERIFY(!layout.setGrid({{"columns",10}}));
    QVERIFY(!layout.errorString().isEmpty());
    QCOMPARE(layout.grid().value("columns").toInt(), 12);
    // Overlap mode permits shared cells, and clearing a placement persists.
    QVERIFY(layout.setGrid({{"overlap",true}}));
    QVERIFY2(layout.setItemPlacement("my-app", {{"x",1},{"y",1},{"width",2},{"height",1}}), qPrintable(layout.errorString()));
    QVERIFY(!layout.clearItemPlacement("absent"));
    QVERIFY(layout.clearItemPlacement("my-app"));
    QVERIFY(!layout.clearItemPlacement("my-app"));
    // Grid, placement and clearing all persist across reload.
    HomeLayout reloaded(&registry); QVERIFY(reloaded.load(dir.filePath("default.json"), dir.filePath("user.json")));
    QCOMPARE(reloaded.grid().value("columns").toInt(), 12);
    QCOMPARE(reloaded.grid().value("overlap").toBool(), true);
    bool placed = false;
    for (const auto &entry : reloaded.items()) {
        const auto item = entry.toMap();
        if (item.value("id").toString() == "my-folder") {
            placed = true;
            QCOMPARE(item.value("placement").toMap().value("x").toInt(), 0);
            QCOMPARE(item.value("placement").toMap().value("width").toInt(), 3);
        } else if (item.value("id").toString() == "my-app") {
            QVERIFY(!item.contains("placement"));
        }
    }
    QVERIFY(placed);
    // A stored placement outside the stored grid falls back to packaged defaults without rewriting the file.
    auto document = QJsonDocument::fromJson(readFile(dir.filePath("user.json"))).object();
    QJsonArray components = document.value("components").toArray();
    for (qsizetype i = 0; i < components.size(); ++i) {
        auto component = components[i].toObject();
        if (component.value("id").toString() != "my-folder") continue;
        auto placement = component.value("placement").toObject();
        placement.insert("x", 99);
        component.insert("placement", placement);
        components[i] = component;
    }
    document.insert("components", components);
    const auto broken = QJsonDocument(document).toJson();
    writeFile(dir.filePath("broken.json"), broken);
    HomeLayout guarded(&registry);
    QVERIFY(guarded.load(dir.filePath("default.json"), dir.filePath("broken.json")));
    QCOMPARE(guarded.grid().value("columns").toInt(), 8);
    QVERIFY(guarded.composition().isEmpty());
    QCOMPARE(readFile(dir.filePath("broken.json")), broken);
}
void ComponentTests::migratesAndRepairsWithoutLosingPresentation()
{
    QTemporaryDir dir;
    writeFile(dir.filePath("defaults.json"), legacy);
    writeFile(dir.filePath("user.json"), legacy);
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);
    QVERIFY(home.load(dir.filePath("defaults.json"), dir.filePath("user.json")));
    QCOMPARE(readFile(dir.filePath("user.json")), legacy); // no migration write on read
    QCOMPARE(home.components().size(), 3); // missing sources retained
    QVERIFY(home.components()[0].toMap()["unavailableReason"].toString().contains("missing"));
    QCOMPARE(home.sourceChoices("my-folder").size(), 2);
    QVERIFY(!home.rebindSource("my-folder", "settings"));
    QVERIFY(home.rebindSource("my-folder", "other"));
    QCOMPARE(readFile(dir.filePath("user.json.v1.backup.json")), legacy);
    auto document = QJsonDocument::fromJson(readFile(dir.filePath("user.json"))).object();
    QCOMPARE(document["schemaVersion"].toInt(), 2);
    QVERIFY(!document.contains("items"));
    HomeLayout restored(&registry);
    QVERIFY(restored.load(dir.filePath("defaults.json"), dir.filePath("user.json")));
    QVERIFY(restored.errorString().isEmpty());
    const auto tile = restored.items()[0].toMap();
    QCOMPARE(tile["id"].toString(), QString("my-folder"));
    QCOMPARE(tile["name"].toString(), QString("My films"));
    QCOMPARE(tile["size"].toString(), QString("wide"));
    QCOMPARE(tile["opacity"].toDouble(), 0.42);
    QCOMPARE(tile["showTitle"].toBool(), false);
    QCOMPARE(tile["customLogo"].toString(), QString("artwork/my-folder.png"));
    QCOMPARE(tile["categoryId"].toString(), QString("personal"));
    QCOMPARE(tile["action"].toMap()["target"].toString(), QString("/media/films"));
    QCOMPARE(restored.backgroundStart(), QString("#123456"));
    QCOMPARE(restored.tileRoundness(), 0.73);
    QVERIFY(registry.removeApp("other"));
    QVERIFY(!restored.items()[0].toMap()["unavailableReason"].toString().isEmpty());
    QVERIFY(restored.rebindSource("my-folder", "movies"));
    QVERIFY(restored.rebindSource("my-app", "other") == false);
    QVERIFY(restored.rebindSource("my-app", "settings"));
    QCOMPARE(restored.items()[1].toMap()["id"].toString(), QString("my-app"));
    QCOMPARE(readFile(dir.filePath("user.json.v1.backup.json")), legacy); // immutable original
}
void ComponentTests::preservesMissingCollectionAndEmptyHome()
{
    QTemporaryDir dir;
    auto component = ComponentSchema::component({{"id", "curated"}, {"kind", "action"}, {"name", "Curated"},
        {"action", QVariantMap{{"type", "open-collection"}, {"collectionId", "missing"}}}}, true);
    const QJsonValue theme = QJsonDocument::fromJson(legacy).object().value("theme");
    writeFile(dir.filePath("defaults.json"), QJsonDocument(QJsonObject{{"schemaVersion", 2}, {"theme", theme},
        {"components", QJsonArray{QJsonObject::fromVariantMap(component)}}}).toJson());
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(dir.filePath("defaults.json"), dir.filePath("user.json")));
    QVERIFY(!home.items()[0].toMap()["unavailableReason"].toString().isEmpty());
    home.setCollections({QVariantMap{{"id", "new"}, {"name", "Family"}}});
    QVERIFY(home.rebindSource("curated", "new"));
    QVERIFY(home.items()[0].toMap()["unavailableReason"].toString().isEmpty());
    home.setCollections({});
    QVERIFY(!home.items()[0].toMap()["unavailableReason"].toString().isEmpty());
    QVERIFY(home.removeItem("curated"));
    HomeLayout empty(&registry); QVERIFY(empty.load(dir.filePath("defaults.json"), dir.filePath("user.json")));
    QVERIFY(empty.components().isEmpty()); // recovery is outside deletable layout
}
void ComponentTests::rejectsConflictingVersionedLayout()
{
    QTemporaryDir dir;
    writeFile(dir.filePath("defaults.json"), legacy);
    auto tile = ComponentSchema::component({{"id", "unsafe"}, {"kind", "app"}, {"appId", "movies"}}, true);
    tile["source"] = QVariantMap{{"schemaVersion", 1}, {"provider", "apps"}, {"kind", "app"}, {"id", "settings"}};
    const auto bytes = QJsonDocument(QJsonObject{{"schemaVersion", 2}, {"theme", QJsonDocument::fromJson(legacy).object()["theme"]},
        {"components", QJsonArray{QJsonObject::fromVariantMap(tile)}}}).toJson();
    writeFile(dir.filePath("user.json"), bytes);
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(dir.filePath("defaults.json"), dir.filePath("user.json")));
    QVERIFY(!home.errorString().isEmpty());
    QCOMPARE(home.items().size(), 3);
    QCOMPARE(readFile(dir.filePath("user.json")), bytes);
}
void ComponentTests::dispatchesValidatedActionsAndOrigins()
{
    QTemporaryDir config;
    const auto format = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    {
        AppRegistry registry; QVERIFY(registry.loadFromData(apps));
        SessionManager sessions(&registry);
        KodiMedia media("dispatch", nullptr, config.path());
        CaptureController capture;
        KodiService service("dispatch"); service.setEnabled(false);
        ActionDispatcher dispatcher(&registry, &sessions, &media, &service, nullptr, &capture);
        QSignalSpy rejected(&dispatcher, &ActionDispatcher::rejected), focus(&dispatcher, &ActionDispatcher::restoreFocusRequested);
        QSignalSpy library(&dispatcher, &ActionDispatcher::libraryRequested), settings(&dispatcher, &ActionDispatcher::settingsRequested);
        QSignalSpy drawer(&dispatcher, &ActionDispatcher::drawerRequested);
        QVERIFY(dispatcher.dispatch({{"type","app-drawer"}},"more-apps")); QCOMPARE(drawer.size(),1);
        QVERIFY(!dispatcher.dispatch({{"type", "shell"}, {"target", "bad"}}));
        QVERIFY(!dispatcher.dispatch({{"type", "launch-app"}, {"appId", "missing"}}));
        QVERIFY(!dispatcher.dispatch({{"type", "open-folder"}, {"target", "/media/movies"}, {"openWith", "settings"}}));
        QCOMPARE(rejected.size(), 3); QVERIFY(!sessions.hasActiveSession());
        QVERIFY(dispatcher.dispatch({{"type", "open-folder"}, {"target", "/media/movies"}, {"openWith", "other"}}, "my-folder"));
        QVERIFY(!dispatcher.dispatch({{"type", "launch-app"}, {"appId", "settings"}}, "wrong-origin"));
        sessions.exitCurrent();
        QCOMPARE(focus.last()[0].toString(), QString("my-folder"));
        QVERIFY(dispatcher.dispatch({{"type", "settings"}, {"section", 1}}, "recovery"));
        QCOMPARE(settings.last()[0].toInt(), 1);
        sessions.exitCurrent(); QCOMPARE(focus.last()[0].toString(), QString("recovery"));
        QVERIFY(sessions.launch("other")); // legacy callers do not inherit old tile origin
        sessions.exitCurrent(); QCOMPARE(focus.last()[0].toString(), QString("other"));
        QSignalSpy captureRequested(&dispatcher, &ActionDispatcher::captureSessionRequested);
        QVERIFY(dispatcher.dispatch({{"type", "launch-app"}, {"appId", "capture-test"}}, "capture-origin"));
        QCOMPARE(captureRequested.size(), 1);
        QCOMPARE(captureRequested.last()[0].toInt(), 0); // zero opens the input picker, not a guessed route
        QCOMPARE(captureRequested.last()[1].toString(), QString("capture-origin"));
        QVERIFY(!sessions.hasActiveSession());
        QVERIFY(!dispatcher.dispatch({{"type", "open-collection"}, {"collectionId", "missing"}}));
        QCOMPARE(library.size(), 0);
        QVERIFY(dispatcher.dispatch({{"type", "launch-app"}, {"appId", "movies"}}, "my-movies"));
        QCOMPARE(library.size(), 1);
        media.invalidate("Done");
        QVERIFY(!sessions.launchAction({{"type", "shell"}, {"target", "bad"}, {"openWith", "other"}}));
    }
    QSettings::setDefaultFormat(format);
}
void ComponentTests::failedEditRetainsProfileLayout()
{
    QTemporaryDir dir;
    const auto defaults = dir.filePath("defaults.json"), user = dir.filePath("profile.json");
    writeFile(defaults, legacy); writeFile(user, legacy);
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(defaults, user));
    const auto before = home.components();
    QVERIFY(QFile::rename(user, user + ".held"));
    QVERIFY(QDir().mkdir(user)); // deterministic write rejection, even under root
    QVERIFY(!home.moveItem("clock", -1));
    QCOMPARE(home.components(), before);
    QCOMPARE(home.backgroundStart(), QString("#123456"));
    QVERIFY(!home.errorString().isEmpty());
    QVERIFY(!home.rebindSource("my-app", "settings"));
    QCOMPARE(home.components(), before);
    QVERIFY(QDir().rmdir(user));
    QVERIFY(QFile::rename(user + ".held", user));
    QVERIFY(home.rebindSource("my-app", "settings"));
    QCOMPARE(QJsonDocument::fromJson(readFile(user)).object()["schemaVersion"].toInt(), 2);
}
void ComponentTests::studioSessionDraftUndoBoundsAndNoWrite()
{
    // T16.1/T16.2: shared draft + bounded undo over a canonical document.
    // StudioSession holds bytes only — opening/editing/undo must not touch
    // the profile file; the applied pipeline (HomeLayout save) stays the
    // sole disk writer (T16.3 covers that side).
    QTemporaryDir dir;
    const auto user = dir.filePath("profile.json");
    writeFile(user, legacy);
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults, user));
    QFile committedFile(user);
    QVERIFY(committedFile.open(QIODevice::ReadOnly));
    const QByteArray committed = committedFile.readAll();
    committedFile.close();

    StudioSession session;
    QVERIFY(session.open(committed));
    QVERIFY(!session.isDirty());
    QCOMPARE(session.changeCount(), 0);

    QJsonDocument doc = QJsonDocument::fromJson(committed);
    auto object = doc.object();
    auto theme = object.value("theme").toObject();
    theme["backgroundStart"] = "#aabbcc";
    object["theme"] = theme;
    const QByteArray edited = QJsonDocument(object).toJson();
    QVERIFY(session.pushEdit(edited));
    QVERIFY(session.isDirty());
    QCOMPARE(session.changeCount(), 1);
    // Draft churn must not write the applied profile document.
    QCOMPARE(readFile(user), committed);

    QVERIFY(session.undo());
    QVERIFY(!session.isDirty());
    QCOMPARE(session.draft(), committed);
    QCOMPARE(readFile(user), committed);

    QVERIFY(session.redo());
    QVERIFY(session.isDirty());
    QCOMPARE(readFile(user), committed);

    session.revert();
    QVERIFY(!session.isDirty());
    QCOMPARE(session.draft(), committed);
    QCOMPARE(readFile(user), committed);

    // Bounds: oversize document refused, zero writes, prior draft kept.
    QVERIFY(session.pushEdit(edited));
    QByteArray huge = edited;
    huge += QByteArray(5 * 1024 * 1024, ' ');
    QVERIFY(!session.pushEdit(huge));
    QVERIFY(!session.errorString().isEmpty());
    QCOMPARE(session.draft(), edited);
    QCOMPARE(readFile(user), committed);

    // Invalid JSON refused with actionable error, draft intact.
    QVERIFY(!session.pushEdit("not-json{{{"));
    QVERIFY(!session.errorString().isEmpty());
    QCOMPARE(session.draft(), edited);

    // Unknown schema version refused, never clamped.
    QJsonDocument vdoc = QJsonDocument::fromJson(edited);
    auto vobject = vdoc.object();
    vobject["schemaVersion"] = 99;
    QVERIFY(!session.pushEdit(QJsonDocument(vobject).toJson()));
    QCOMPARE(session.draft(), edited);
    QCOMPARE(readFile(user), committed);

    // Component-count bound enforced.
    QJsonArray many;
    for (int i = 0; i < 1025; ++i)
        many.append(QJsonObject{{"id", QString("c%1").arg(i)}, {"kind", "widget"}, {"widgetType", "clock"}});
    vobject = QJsonDocument::fromJson(edited).object();
    vobject["components"] = many;
    QVERIFY(!session.pushEdit(QJsonDocument(vobject).toJson()));
    QCOMPARE(readFile(user), committed);

    // History refuses overflow without silently losing undo or accepting the edit.
    session.revert();
    for (int i = 0; i < StudioSession::kMaxUndoSteps; ++i) {
        QJsonDocument step = QJsonDocument::fromJson(session.draft());
        auto stepObject = step.object();
        auto stepTheme = stepObject.value("theme").toObject();
        stepTheme["tileRoundness"] = 0.1 + 0.01 * (i % 80);
        stepObject["theme"] = stepTheme;
        QVERIFY(session.pushEdit(QJsonDocument(stepObject).toJson()));
    }
    QCOMPARE(session.changeCount(), StudioSession::kMaxUndoSteps);
    const auto fullDraft = session.draft();
    QVERIFY(!session.pushEdit(edited));
    QCOMPARE(session.draft(), fullDraft);
    session.continueWithNewHistoryBaseline();
    QVERIFY(session.pushEdit(edited));
    QCOMPARE(readFile(user), committed);

    // markApplied adopts the draft as the new committed baseline.
    const QByteArray finalDraft = session.draft();
    session.markApplied();
    QVERIFY(!session.isDirty());
    QCOMPARE(session.committed(), finalDraft);
    QCOMPARE(readFile(user), committed); // still no write — caller persists
}
void ComponentTests::studioSessionCanonicalIdsAndRevisionConflict()
{
    // T16.2: identical edits from different key orders canonicalize equal.
    // T16.3: centrally allocated ids converge; unsafe ids refused.
    // T16.4: dirty draft + moved applied doc = conflict; clean draft never is.
    QTemporaryDir dir;
    const auto user = dir.filePath("profile.json");
    writeFile(user, legacy);
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults, user));
    const QByteArray committed = readFile(user);

    StudioSession session;
    QVERIFY(session.open(committed));

    // allocateId: free base passes through, taken ids walk deterministically.
    QCOMPARE(session.allocateId("brand-new-widget"), QString("brand-new-widget"));
    QCOMPARE(session.allocateId("clock"), QString("clock-copy"));
    QCOMPARE(session.allocateId("clock-copy"), QString("clock-copy"));
    QVERIFY(session.allocateId("has spaces!").isEmpty());
    QVERIFY(!session.errorString().isEmpty());

    // canonicalize: same object, different key order → identical bytes.
    const QByteArray a = R"({"schemaVersion":2,"theme":{"b":1,"a":2}})";
    const QByteArray b = R"({"theme":{"a":2,"b":1},"schemaVersion":2})";
    QByteArray aOut, bOut;
    QVERIFY(session.canonicalize(a, b, aOut, bOut));
    QCOMPARE(aOut, bOut);
    QVERIFY(!session.canonicalize("not json", b, aOut, bOut));

    // Revision conflict: clean draft is never stale, even against other bytes.
    QVERIFY(!session.isStaleAgainst(committed + " "));
    QJsonDocument doc = QJsonDocument::fromJson(committed);
    auto object = doc.object();
    auto changedTheme = object["theme"].toObject();
    changedTheme["backgroundStart"] = "#112233";
    object["theme"] = changedTheme;
    QVERIFY(session.pushEdit(QJsonDocument(object).toJson()));
    QVERIFY(session.isDirty());
    QVERIFY(!session.isStaleAgainst(committed)); // applied doc unchanged
    QJsonDocument other = QJsonDocument::fromJson(committed);
    auto otherObject = other.object();
    otherObject["theme"] = QJsonObject{{"backgroundStart", "#445566"}};
    QVERIFY(session.isStaleAgainst(QJsonDocument(otherObject).toJson()));
    QVERIFY(session.isStaleAgainst({})); // vanished applied doc = conflict

    // Cross-profile isolation: a second profile's session is independent —
    // edits in one never leak into the other's draft or the other file.
    const auto user2 = dir.filePath("profile2.json");
    writeFile(user2, legacy);
    const QByteArray committed2 = readFile(user2);
    StudioSession other2;
    QVERIFY(other2.open(committed2));
    QCOMPARE(other2.draft(), committed2);
    QVERIFY(session.draft() != other2.draft());
    QVERIFY(other2.pushEdit(QJsonDocument(object).toJson()));
    QCOMPARE(readFile(user), committed); // no-write invariant holds for both
    QCOMPARE(readFile(user2), committed2);
    QVERIFY(!other2.isStaleAgainst(committed2));
    QCOMPARE(session.allocateId("clock"), QString("clock-copy"));
    other2.revert(); // teardown: drop draft, history and errors, no writes
    QVERIFY(!other2.isDirty());
    QCOMPARE(readFile(user2), committed2);
}
void ComponentTests::studioApplyAtomicFailureAndFocusPreserved()
{
    // T16.5: failed applies (invalid draft, then deterministic write failure)
    // preserve original bytes, the applied model, the recoverable draft and
    // the focus origin; success records the focus origin for canvas restore.
    QTemporaryDir dir;
    const auto user = dir.filePath("profile.json");
    writeFile(user, legacy);
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults, user));
    const QByteArray committed = home.committedDocument();
    const QByteArray originalFile = readFile(user);
    const auto before = home.components();

    StudioSession session;
    QVERIFY(session.open(committed));
    home.setFocusOrigin(QStringLiteral("clock"));

    // Candidate edit: rename the first tile in the draft bytes.
    QJsonDocument doc = QJsonDocument::fromJson(committed);
    auto object = doc.object();
    auto entries = object.value(QStringLiteral("components")).toArray();
    if (entries.isEmpty()) entries = object.value(QStringLiteral("items")).toArray();
    QVERIFY(!entries.isEmpty());
    auto first = entries.first().toObject();
    first[QStringLiteral("name")] = QStringLiteral("Studio clock");
    entries[0] = first;
    if (object.contains(QStringLiteral("components"))) object[QStringLiteral("components")] = entries;
    else object[QStringLiteral("items")] = entries;
    const QByteArray edited = QJsonDocument(object).toJson();
    QVERIFY(session.pushEdit(edited));

    // Invalid draft apply: bytes, model and draft all survive.
    QVERIFY(!home.applyStudioDraft("not-json{{{", QStringLiteral("clock")));
    QVERIFY(!home.errorString().isEmpty());
    QCOMPARE(readFile(user), originalFile);
    QCOMPARE(home.components(), before);
    QCOMPARE(session.draft(), edited);
    QCOMPARE(home.focusOrigin(), QStringLiteral("clock"));

    // Deterministic write failure: user path blocked by a directory, so
    // save() fails after in-memory adoption — the model must roll back.
    QVERIFY(QFile::rename(user, user + ".held"));
    QVERIFY(QDir().mkdir(user));
    QVERIFY(!home.applyStudioDraft(edited, QStringLiteral("my-folder")));
    QVERIFY(!home.errorString().isEmpty());
    QCOMPARE(home.components(), before);
    QCOMPARE(home.committedDocument(), committed);
    QCOMPARE(session.draft(), edited); // recoverable draft stays with caller
    QCOMPARE(home.focusOrigin(), QStringLiteral("clock")); // origin preserved
    QVERIFY(QDir().rmdir(user));
    QVERIFY(QFile::rename(user + ".held", user));

    // Success path: model + file advance, focus origin records the canvas
    // component the Studio passes for post-apply restoration.
    QVERIFY(home.applyStudioDraft(edited, QStringLiteral("my-folder")));
    QCOMPARE(home.focusOrigin(), QStringLiteral("my-folder"));
    QVERIFY(readFile(user) != committed);
    session.markApplied();
    QVERIFY(!session.isDirty());
}
void ComponentTests::studioCommandsAreAtomicAndProfileScoped()
{
    QTemporaryDir dir;
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    const auto user = dir.filePath("one.json");
    writeFile(user, legacy);
    HomeLayout home(&registry); QVERIFY(home.load(defaults, user));
    auto *session = qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(session);
    const auto file = readFile(user);
    const auto baseline = session->draft();
    QVERIFY(session->execute("rename", {{"id", "my-folder"}, {"name", "Draft name"}}));
    QCOMPARE(readFile(user), file);
    QVERIFY(home.items()[0].toMap()["name"] != QString("Draft name"));
    QVERIFY(session->model()->property("items").toList()[0].toMap()["name"] == QString("Draft name"));
    const auto renamed = session->draft();
    QVERIFY(!session->execute("place", {{"placements", QVariantList{
        QVariantMap{{"id", "clock"}, {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}}},
        QVariantMap{{"id", "missing"}, {"placement", QVariantMap{{"x", 1}, {"y", 0}, {"width", 1}, {"height", 1}}}}
    }}}));
    QCOMPARE(session->draft(), renamed);
    QVERIFY(session->undo()); QCOMPARE(session->draft(), baseline);
    QVERIFY(session->redo()); QCOMPARE(session->draft(), renamed);
    QVERIFY(session->execute("newPage", {{"name", "Draft page"}}));
    QCOMPARE(home.pages().size(), 0);
    QCOMPARE(readFile(user), file);
    QVERIFY(session->apply("clock"));
    QCOMPARE(home.pages().size(), 1);
    QVERIFY(readFile(user) != file);
    QVERIFY(!session->isDirty());
    QVERIFY(session->execute("rename", {{"id", "my-folder"}, {"name", "Unapplied"}}));
    const auto saved = readFile(user), recoverable = session->draft();
    writeFile(user,saved + "\n"); // Another process changed the file without updating the live model.
    QVERIFY(!session->apply()); QCOMPARE(readFile(user),saved + "\n");
    QCOMPARE(session->draft(),recoverable);
    writeFile(user,saved);
    QVERIFY(home.setItemName("my-folder", "Concurrent change"));
    QVERIFY(!session->apply());
    QVERIFY(session->errorString().contains("changed"));
    const auto second = dir.filePath("two.json");
    writeFile(second, legacy);
    bool teardownAttempted = false, teardownAccepted = true;
    connect(session,&StudioSession::changed,session,[&] {
        if(teardownAttempted)return;
        teardownAttempted=true;
        teardownAccepted=session->execute("remove",{{"id","clock"}});
    });
    QVERIFY(home.load(defaults, second));
    QVERIFY(teardownAttempted); QVERIFY(!teardownAccepted);
    QVERIFY(!session->active());
    QVERIFY(!session->apply());
    QVERIFY(!session->execute("remove", {{"id", "clock"}}));
    QCOMPARE(readFile(second), legacy);
}
void ComponentTests::studioCollisionProposalsAndParity()
{
    QTemporaryDir dir; AppRegistry registry;
    HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    auto *session = qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(session);
    QVERIFY(session->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5}}}}));
    const auto start = session->draft();
    const auto history = session->changeCount();
    QVERIFY(session->previewCommand("convertGrid",{{"value",QVariantMap{{"columns",4},{"rows",5}}}}));
    QCOMPARE(session->draft(),start); QCOMPARE(session->changeCount(),history);
    QVERIFY(session->proposalModel()); session->cancelPreview();
    QCOMPARE(session->draft(),start);
    const QVariantMap p{{"x",1},{"y",0},{"width",1},{"height",1}};
    QVERIFY(!session->placementProposal("clock",p,"cancel").value("valid").toBool());
    auto swap = session->placementProposal("clock",p,"swap");
    QVERIFY2(swap["valid"].toBool(),qPrintable(swap["reason"].toString()));
    QCOMPARE(swap["placements"].toList().size(),2);
    QVERIFY(session->execute("place",{{"placements",swap["placements"]}}));
    const auto swapped = session->draft();
    QVERIFY(session->undo()); QCOMPARE(session->draft(),start);
    QVERIFY(session->execute("place",{{"placements",session->placementProposal("clock",p,"swap")["placements"]}}));
    QCOMPARE(session->draft(),swapped); // Same semantic drop via another input adapter.
    QVERIFY(session->undo());
    auto shift = session->placementProposal("clock",p,"shift");
    QVERIFY2(shift["valid"].toBool(),qPrintable(shift["reason"].toString()));
    QVERIFY(session->execute("place",{{"placements",shift["placements"]}}));
    QVERIFY(session->undo()); QCOMPARE(session->draft(),start);
    QVERIFY(!session->placementProposal("clock",{{"x",8},{"y",0},{"width",1},{"height",1}},"shift")["valid"].toBool());
    QCOMPARE(session->draft(),start);
    const auto addition = session->additionProposal("clock",p,"shift");
    QVERIFY2(addition["valid"].toBool(),qPrintable(addition["reason"].toString()));
    const auto beforeAdd = session->changeCount();
    QVERIFY(session->execute("batch",{{"commands",addition["commands"]}}));
    QCOMPARE(session->changeCount(),beforeAdd+1);
    QVERIFY(session->undo()); QCOMPARE(session->draft(),start);
    const auto beforeBatch = session->changeCount();
    QVERIFY(session->execute("batch",{{"commands",QVariantList{
        QVariantMap{{"operation","rename"},{"arguments",QVariantMap{{"id","clock"},{"name","Group rename"}}}},
        QVariantMap{{"operation","rename"},{"arguments",QVariantMap{{"id","movies"},{"name","Library"}}}}
    }}}));
    QCOMPARE(session->changeCount(),beforeBatch+1);
    QVERIFY(session->undo()); QCOMPARE(session->draft(),start);
}
void ComponentTests::studioSectionGridsAreIndependentAndAtomic()
{
    QTemporaryDir dir; AppRegistry registry;
    HomeLayout home(&registry);
    const auto user=dir.filePath("home.json");
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(session);
    const QVariantMap grid{{"columns",8},{"rows",5},{"mode","sections"}};
    QVERIFY2(session->execute("convertGrid",{{"value",grid},{"sectionHeight",560}}),qPrintable(session->errorString()));
    const auto document=QJsonDocument::fromJson(session->draft()).object();
    const auto categories=document["categories"].toArray(); QVERIFY(!categories.isEmpty());
    const auto category=categories.first().toObject()["id"].toString(); QVERIFY(!category.isEmpty());
    const QVariantMap cell{{"x",0},{"y",4},{"width",1},{"height",1}};
    QVERIFY2(session->execute("addPlaced",{{"type","text"},{"placement",cell},{"categoryId",category}}),qPrintable(session->errorString()));
    // The same coordinates in another section do not collide.
    QVERIFY2(session->execute("addPlaced",{{"type","clock"},{"placement",cell}}),qPrintable(session->errorString()));
    const auto before=session->draft(); const auto count=session->changeCount();
    // A collision in the same section refuses the entire operation.
    QVERIFY(!session->execute("addPlaced",{{"type","text"},{"placement",cell},{"categoryId",category}}));
    QCOMPARE(session->draft(),before); QCOMPARE(session->changeCount(),count);
    QVERIFY(!session->execute("convertGrid",{{"value",grid},{"sectionHeight",59}}));
    QCOMPARE(session->draft(),before);
    QVERIFY(!QFile::exists(user));
    QVERIFY(session->undo()); QVERIFY(session->redo()); QCOMPARE(session->draft(),before);
    QVERIFY2(session->apply(),qPrintable(session->errorString()));
    HomeLayout restored(&registry); QVERIFY(restored.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    QCOMPARE(restored.committedDocument(),home.committedDocument());
    // Section grid overrides use their own bounds and reject unknown modes.
    auto composition=document["composition"].toObject().toVariantMap();
    auto modified=categories;auto section=modified.first().toObject();
    section["presentation"]=QJsonObject{{"grid",QJsonObject{{"columns",4},{"rows",3}}}}; modified[0]=section;
    QCOMPARE(ComponentSchema::sectionGrid(composition,modified,category)["columns"].toInt(),4);
    QVERIFY(!ComponentSchema::validPresentation({{"grid",QVariantMap{{"mode","sections"}}}},true));
}
void ComponentTests::studioHomeDesignationIsAtomicAndProtected()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto user=dir.filePath("home.json"),defaults=QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults,user));QCOMPARE(home.homePageId(),QString());
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY2(session->execute("newPage",{{"name","Library"}}),qPrintable(session->errorString()));
    const auto document=QJsonDocument::fromJson(session->draft()).object();
    const auto id=document["pages"].toArray().last().toObject()["id"].toString();QVERIFY(!id.isEmpty());
    QVERIFY(session->execute("appearance",{{"value",QVariantMap{{"focusColor","#123456"}}}},id));
    QVERIFY(session->execute("setHomePage",{{"id",id}}));
    const auto designated=session->draft();
    QVERIFY(!session->execute("removePage",{{"id",id}}));QCOMPARE(session->draft(),designated);
    QVERIFY(!session->execute("setHomePage",{{"id","missing"}}));QCOMPARE(session->draft(),designated);
    QVERIFY(!QFile::exists(user));QCOMPARE(home.homePageId(),QString());
    QVERIFY(session->undo());QVERIFY(session->redo());QCOMPARE(session->draft(),designated);
    QVERIFY2(session->apply(),qPrintable(session->errorString()));
    QCOMPARE(home.homePageId(),id);QCOMPARE(home.homeAppearance()["focusColor"].toString(),QString("#123456"));
    HomeLayout restored(&registry);QVERIFY(restored.load(defaults,user));QCOMPARE(restored.homePageId(),id);
    // Page edits and widget insertion keep the interface-level designation intact.
    QVERIFY2(session->execute("addPlaced",{{"type","clock"},{"placement",QVariantMap{{"x",0},{"y",0},{"width",1},{"height",1}}}},id),qPrintable(session->errorString()));
    QVERIFY(session->execute("setHomePage",{{"id",""}}));
    QVERIFY(session->execute("removePage",{{"id",id}}));
    QVERIFY(session->undo());QCOMPARE(QJsonDocument::fromJson(session->draft()).object()["pages"].toArray().size(),1);
}
void ComponentTests::studioArtworkUsesActiveProfileAndExpires()
{
    QTemporaryDir dir;AppRegistry registry;
    auto document=QJsonDocument::fromJson(legacy).object();
    document["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"wallpaper","artwork/scene.png"}};
    writeFile(dir.filePath("default.json"),QJsonDocument(document).toJson());
    HomeLayout home(&registry);QVERIFY(home.load(dir.filePath("default.json"),dir.filePath("home.json")));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    auto *model=qobject_cast<HomeLayout *>(session->model());QVERIFY(model);
    QCOMPARE(model->wallpaperSource(),home.wallpaperSource());
    QCOMPARE(model->components().first().toMap()["logoSource"],home.components().first().toMap()["logoSource"]);
    QVERIFY(!QFile::exists(dir.filePath("home.json")));
    session->invalidate();QVERIFY(model->wallpaperSource().isEmpty());QVERIFY(model->components().isEmpty());
}
void ComponentTests::studioPageLifecycleRepairsLinksAndPreservesRoot()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");
    QVERIFY(home.load(defaults,user));auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY2(session->execute("renamePage",{{"id",""},{"name","Landing"}}),qPrintable(session->errorString()));
    QVERIFY2(session->execute("duplicatePage",{{"id",""},{"name","Library"}}),qPrintable(session->errorString()));
    auto document=QJsonDocument::fromJson(session->draft()).object();
    const auto page=document["pages"].toArray().last().toObject();const auto id=page["id"].toString();
    QVERIFY(page["layout"].toObject()["components"].toArray().first().toObject()["id"]!=document["components"].toArray().first().toObject()["id"]);
    QVERIFY(session->execute("reorderPage",{{"id",""},{"offset",1}}));
    auto *draft=qobject_cast<HomeLayout *>(session->model());QVERIFY(draft);QCOMPARE(draft->studioPages().last().toMap()["name"].toString(),QString("Landing"));
    QVERIFY(session->execute("addButton",{},id));
    document=QJsonDocument::fromJson(session->draft()).object();
    const auto button=document["pages"].toArray().first().toObject()["layout"].toObject()["components"].toArray().last().toObject()["id"].toString();
    QVERIFY2(session->execute("action",{{"id",button},{"value",QVariantMap{{"type","root-page"}}}},id),qPrintable(session->errorString()));
    QVERIFY(session->execute("setHomePage",{{"id",id}}));const auto before=session->draft();
    QVERIFY(!session->execute("removePage",{{"id",""}}));QCOMPARE(session->draft(),before);
    QVERIFY2(session->execute("removePage",{{"id",""},{"replacementPageId",id}}),qPrintable(session->errorString()));
    const auto removed=session->draft();QVERIFY(!draft->rootPageVisible());QCOMPARE(draft->studioPages().size(),1);
    document=QJsonDocument::fromJson(removed).object();QVERIFY(document["components"].toArray().isEmpty());
    const auto action=document["pages"].toArray().first().toObject()["layout"].toObject()["components"].toArray().last().toObject()["action"].toObject();
    QCOMPARE(action["type"].toString(),QString("open-page"));QCOMPARE(action["pageId"].toString(),id);
    QVERIFY(!session->execute("setHomePage",{{"id",""}}));QCOMPARE(session->draft(),removed);
    QVERIFY(session->undo());QCOMPARE(session->draft(),before);QVERIFY(session->redo());QCOMPARE(session->draft(),removed);
    QVERIFY(!QFile::exists(user));QVERIFY(session->apply());
    HomeLayout restored(&registry);QVERIFY(restored.load(defaults,user));QVERIFY(!restored.rootPageVisible());QCOMPARE(restored.homePageId(),id);
    // A legacy single-page preset applies to the designated Home, not to a deleted root.
    QVERIFY2(restored.applySkin("builtin-default"),qPrintable(restored.errorString()));QVERIFY(!restored.rootPageVisible());QCOMPARE(restored.homePageId(),id);
    auto *homePage=qobject_cast<PageModel *>(restored.pageModel(id));QVERIFY(homePage);QVERIFY(!homePage->items().isEmpty());
    HomeLayout visibleRoot(&registry);QVERIFY(visibleRoot.load(defaults,dir.filePath("visible-root.json")));
    const auto another=visibleRoot.createPage("Another Home","blank");QVERIFY(!another.isEmpty());
    auto *changeHome=qobject_cast<StudioSession *>(visibleRoot.beginStudio());QVERIFY(changeHome);
    QVERIFY(changeHome->execute("setHomePage",{{"id",another}}));QVERIFY(changeHome->apply());
    const auto rootComponents=QJsonDocument::fromJson(visibleRoot.committedDocument()).object().value("components");
    QVERIFY2(visibleRoot.applySkin("builtin-default"),qPrintable(visibleRoot.errorString()));QVERIFY(visibleRoot.rootPageVisible());
    QCOMPARE(QJsonDocument::fromJson(visibleRoot.committedDocument()).object().value("components"),rootComponents);
    QVERIFY(!qobject_cast<PageModel *>(visibleRoot.pageModel(another))->items().isEmpty());
}
void ComponentTests::studioChromeRemainsReadableAndProfileLocal()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");QVERIFY(home.load(defaults,user));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY(session->execute("appearance",{{"value",QVariantMap{{"surfaceColor","#ffffff"},{"textColor","#ffffff"}}}}));
    auto *draft=qobject_cast<HomeLayout *>(session->model());QVERIFY(draft);
    QVERIFY(draft->studioAppearance()["surfaceColor"]!=draft->studioAppearance()["textColor"]);
    const auto baseline=session->draft();
    QVERIFY(!session->execute("studioAppearance",{{"value",QVariantMap{{"textColor","#202126"}}}}));QCOMPARE(session->draft(),baseline);
    QVERIFY2(session->execute("studioAppearance",{{"value",QVariantMap{{"focusColor","#ffffff"}}}}),qPrintable(session->errorString()));
    QVERIFY(session->apply());QCOMPARE(home.studioAppearance()["focusColor"].toString(),QString("#ffffff"));
    QVERIFY(home.load(defaults,dir.filePath("other.json")));QVERIFY(!session->active());
    QVERIFY(home.studioAppearance()["focusColor"].toString()!=QString("#ffffff"));
}
void ComponentTests::studioHiddenAndLockedStatesAreRecoverable()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");
    QVERIFY(home.load(defaults,user));auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY(session->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5}}}}));
    QVERIFY(session->execute("state",{{"id","clock"},{"key","hidden"},{"value",true}}));
    QVERIFY(session->execute("state",{{"id","clock"},{"key","locked"},{"value",true}}));
    const auto before=session->draft();const QVariantMap target{{"x",0},{"y",3},{"width",1},{"height",1}};
    QVERIFY(!session->placementProposal("clock",target,"shift")["valid"].toBool());
    QVERIFY(!session->execute("place",{{"placements",QVariantList{QVariantMap{{"id","clock"},{"placement",target}}}}}));QCOMPARE(session->draft(),before);
    QVERIFY(!session->execute("state",{{"id","clock"},{"key","hidden"},{"value","yes"}}));QCOMPARE(session->draft(),before);
    auto *model=qobject_cast<HomeLayout *>(session->model());QVERIFY(model);QCOMPARE(model->items().size(),home.items().size());
    QVERIFY(session->apply());QVERIFY(!home.items().isEmpty());
    for(const auto &entry:home.items())QVERIFY(entry.toMap()["id"]!="clock");
    QVERIFY(session->execute("state",{{"id","clock"},{"key","hidden"},{"value",false}}));
    QVERIFY(session->execute("state",{{"id","clock"},{"key","locked"},{"value",false}}));
    QVERIFY(session->execute("place",{{"placements",QVariantList{QVariantMap{{"id","clock"},{"placement",target}}}}}));QVERIFY(session->apply());
    bool found=false;for(const auto &entry:home.items())if(entry.toMap()["id"]=="clock")found=true;QVERIFY(found);
}
QTEST_GUILESS_MAIN(ComponentTests)
#include "tst_components.moc"
