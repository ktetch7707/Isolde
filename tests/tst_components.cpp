#include "home/ComponentSchema.h"
#include "home/ActionDispatcher.h"
#include "home/HomeLayout.h"
#include "home/PageModel.h"
#include "sessions/SessionManager.h"
#include "media/KodiMedia.h"
#include "media/CaptureController.h"
#include "media/KodiService.h"
#include "home/StudioSession.h"
#include "games/GameLibrary.h"
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImage>
#include <QUrl>

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
    void gamesCatalogIsBoundedAtomicAndProfileLocal();
    void gamesLaunchRejectsMissingAndRedirectedTargets();
    void gamesContractsRejectExecutableData();
    void gamesRelinkPreservesIdentityAndRollsBack();
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
    void starterPageUpdatesPreserveDesignAndRollback();
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
    void studioShiftCascadesAreBoundedAndAtomic();
    void studioScrollingPagesAreBoundedAndAtomic();
    void studioSectionTransfersDuplicationAndOrdering();
    void studioNestedGroupsValidateAndReparentAtomically();
    void studioOrderedInsertionPreservesSlotsAndIsAtomic();
    void studioOverlayMovementKeepsNeighborsAndBounds();
    void studioCatalogLinksInspectorAndStagedArtwork();
};
static AppDefinition gamesRuntime(const QString &id="psp-test") {
    AppDefinition app;app.id=id;app.name="PSP runtime";app.type="desktop";
    app.presentationMode="game-hub";app.section="Games";app.accent=QColor("#556677");
    app.handledActions={"open-file"};
    app.launchConfiguration={{"desktopId","isolde-test-emulator"},{"actionHandlers",QVariantMap{{"open-file",QVariantMap{{"appendTarget",true}}}}}};
    return app;
}
static QVariantMap gameRegistration(const QString &root,const QString &target="game.iso") {
    return {{"schemaVersion",1},{"title","Local title"},{"platform","PSP"},{"runtimeId","psp-test"},{"root",root},{"target",target}};
}
void ComponentTests::gamesRelinkPreservesIdentityAndRollsBack() {
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
void ComponentTests::gamesContractsRejectExecutableData() {
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
}
void ComponentTests::gamesCatalogIsBoundedAtomicAndProfileLocal() {
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
void ComponentTests::gamesLaunchRejectsMissingAndRedirectedTargets() {
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
    QVERIFY2(layout.applySkin("builtin-pastel"),qPrintable(layout.errorString()));QCOMPARE(layout.pages().size(),7); // Five starter pages preserve both authored pages.
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
    QVERIFY2(layout.applySkin("builtin-pastel"),qPrintable(layout.errorString()));
    QCOMPARE(layout.pages().size(),5);
    auto *moviePage=qobject_cast<PageModel *>(layout.pageModel("library-movies"));QVERIFY(moviePage);
    QCOMPARE(moviePage->items().first().toMap()["componentType"].toString(),QString("library"));
    const auto moviesLink=layout.components()[6].toMap()["action"].toMap();
    QCOMPARE(moviesLink["type"].toString(),QString("open-page"));
    QCOMPARE(moviesLink["pageId"].toString(),QString("library-movies"));
    QVERIFY(moviePage->setAppearance({{"focusColor","#123456"}}));
    QVERIFY(layout.applySkin("builtin-pastel"));QCOMPARE(layout.pages().size(),5);
    QCOMPARE(moviePage->appearance()["focusColor"].toString(),QString("#123456"));
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
void ComponentTests::starterPageUpdatesPreserveDesignAndRollback()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry);
    QVERIFY(layout.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    QVERIFY(layout.applySkin("builtin-pastel"));
    auto document = QJsonDocument::fromJson(layout.committedDocument()).object();
    auto components = document["components"].toArray();
    for (int i = 0; i < components.size(); ++i) {
        auto c = components[i].toObject(); const auto id = c["id"].toString();
        if (id == "music" || id == "games" || id == "guide") {
            c["action"] = id == "guide" ? QJsonObject{{"type","settings"},{"section",4}} : QJsonObject{{"type","app-drawer"}};
            c["source"] = QJsonObject{{"schemaVersion",1},{"kind",id == "guide" ? "settings" : "app-drawer"},{"provider","shell"}};
            components[i] = c;
        }
    }
    document["components"] = components;
    auto pages = document["pages"].toArray();
    while (pages.size() > 2) pages.removeLast();
    document["pages"] = pages;
    QVERIFY2(layout.applyStudioDraft(QJsonDocument(document).toJson()), qPrintable(layout.errorString()));
    QVERIFY(layout.setAppearance({{"fontFamily","Serif"}}));
    const auto before = layout.committedDocument();
    const auto disk = readFile(dir.filePath("home.json"));
    QCOMPARE(layout.skinPageUpdates("builtin-pastel").size(), 3);
    QCOMPARE(layout.committedDocument(), before); QCOMPARE(readFile(dir.filePath("home.json")), disk);
    QVERIFY(QFile::rename(dir.filePath("home.json"), dir.filePath("home.held")));
    QVERIFY(QDir().mkdir(dir.filePath("home.json")));
    QVERIFY(!layout.applySkinPageUpdates("builtin-pastel")); QCOMPARE(layout.committedDocument(), before);
    QVERIFY(QDir().rmdir(dir.filePath("home.json")));
    QVERIFY(QFile::rename(dir.filePath("home.held"), dir.filePath("home.json")));
    QVERIFY2(layout.applySkinPageUpdates("builtin-pastel"), qPrintable(layout.errorString()));
    QCOMPARE(layout.pages().size(), 5); QCOMPARE(layout.appearance()["fontFamily"].toString(), QString("Serif"));
    const auto after = QJsonDocument::fromJson(layout.committedDocument()).object();
    QCOMPARE(after["pages"].toArray().at(0), pages.at(0)); QCOMPARE(after["pages"].toArray().at(1), pages.at(1));
    QVERIFY(layout.skinPageUpdates("builtin-pastel").isEmpty());
    const auto saved = readFile(dir.filePath("home.json"));
    QVERIFY(!layout.applySkinPageUpdates("builtin-pastel")); QCOMPARE(readFile(dir.filePath("home.json")), saved);
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
        AppDefinition pending;
        sessions.setLaunchPreparation([&](const AppDefinition &definition){pending=definition;return false;});
        QSignalSpy deferred(&sessions,&SessionManager::launchDeferred);
        QVERIFY(dispatcher.dispatch({{"type","open-folder"},{"target","/media/movies"},{"openWith","other"}},"deferred-folder"));
        QCOMPARE(deferred.size(),0);QVERIFY(!sessions.hasActiveSession());
        QCOMPARE(pending.launchConfiguration.value("actionTarget").toString(),QString("/media/movies"));
        sessions.setLaunchPreparation([](const AppDefinition &){return true;});
        QVERIFY(sessions.resumePreparedLaunch(pending));QVERIFY(sessions.hasActiveSession());
        sessions.goHome();QCOMPARE(focus.last()[0].toString(),QString("deferred-folder"));
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
void ComponentTests::studioScrollingPagesAreBoundedAndAtomic()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");QVERIFY(home.load(defaults,user));
    auto document=QJsonDocument::fromJson(home.committedDocument()).object();document["categories"]=QJsonArray{};
    const QJsonObject composition{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",4},{"rows",3},{"mode","screen"}}}};
    document["composition"]=composition;
    document["components"]=QJsonArray{QJsonObject::fromVariantMap(ComponentSchema::component({{"id","clock"},{"kind","widget"},{"widgetType","clock"},{"placement",QVariantMap{{"x",0},{"y",0},{"width",1},{"height",1}}}},true))};
    writeFile(user,QJsonDocument(document).toJson());QVERIFY(home.load(defaults,user));const auto disk=readFile(user);
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);const auto start=session->draft();
    const QVariantMap below{{"x",0},{"y",12},{"width",1},{"height",1}};
    QVERIFY(!session->placementProposal("clock",below,"shift")["valid"].toBool());
    QVERIFY(!session->execute("scrolling",{{"enabled","yes"}}));QCOMPARE(session->draft(),start);
    QVERIFY(session->previewCommand("scrolling",{{"enabled",true}}));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    session->cancelPreview();QCOMPARE(session->draft(),start);
    QVERIFY(session->execute("scrolling",{{"enabled",true}}));
    const auto enabled=session->draft();
    const auto proposal=session->placementProposal("clock",below,"shift");QVERIFY2(proposal["valid"].toBool(),qPrintable(proposal["reason"].toString()));
    QVERIFY(session->execute("place",{{"placements",proposal["placements"]}}));const auto longPage=session->draft();
    QVERIFY(!session->previewCommand("scrolling",{{"enabled",false}}));QCOMPARE(session->draft(),longPage);QVERIFY(!session->proposalModel());
    QVERIFY(!session->placementProposal("clock",{{"x",0},{"y",ComponentSchema::MaxScrollableRows},{"width",1},{"height",1}},"shift")["valid"].toBool());
    QVERIFY(!session->placementProposal("clock",{{"x",0},{"y",0},{"width",1},{"height",11}},"shift")["valid"].toBool());
    QCOMPARE(readFile(user),disk);QVERIFY(session->undo());QCOMPARE(session->draft(),enabled);QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    // Enabling scrolling from a blocked Add is one transaction, not a
    // separately applied setting that survives cancellation/undo.
    const auto addition=session->additionProposal("text",below,"shift",{},{},{},{},true);
    QVERIFY2(addition["valid"].toBool(),qPrintable(addition["reason"].toString()));QCOMPARE(session->draft(),start);
    QVERIFY(session->execute("batch",{{"commands",addition["commands"]}}));QCOMPARE(session->changeCount(),1);
    QVERIFY(session->undo());QCOMPARE(session->draft(),start);QVERIFY(session->redo());
    QVERIFY(session->apply());HomeLayout reload(&registry);QVERIFY(reload.load(defaults,user));
    QVERIFY(reload.composition()["verticalScrolling"].toBool());QCOMPARE(reload.grid()["rows"].toInt(),3);
    QCOMPARE(reload.components().size(),2);QVERIFY(!session->canUndo());
    auto malformed=composition.toVariantMap();malformed["verticalScrolling"]="true";QVERIFY(!ComponentSchema::validComposition(malformed));
    malformed["verticalScrolling"]=true;malformed["grid"]=QVariantMap{{"rows",100},{"columns",4},{"mode","screen"}};QVERIFY(!ComponentSchema::validComposition(malformed));
}
void ComponentTests::studioShiftCascadesAreBoundedAndAtomic()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");QVERIFY(home.load(defaults,user));
    auto document=QJsonDocument::fromJson(home.committedDocument()).object();document["categories"]=QJsonArray{};
    document["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",4},{"rows",3},{"mode","screen"}}}};
    const auto node=[](QString id,int x,int y,int width=1){return QJsonObject::fromVariantMap(ComponentSchema::component(QVariantMap{{"id",id},{"kind","widget"},{"widgetType","text"},{"name",id},{"placement",QVariantMap{{"x",x},{"y",y},{"width",width},{"height",1}}}},true));};
    QJsonArray entries{node("moving",0,0),node("wide",1,0,2)};
    for(int y=0;y<3;++y)for(int x=0;x<4;++x)if(!(y==0 && x<3) && !(x==3 && y==2))entries.append(node(QString("cell-%1-%2").arg(y).arg(x),x,y));
    document["components"]=entries;writeFile(user,QJsonDocument(document).toJson());QVERIFY(home.load(defaults,user));QVERIFY2(home.errorString().isEmpty(),qPrintable(home.errorString()));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);const auto original=session->draft(),disk=readFile(user);const auto count=session->changeCount();
    const QVariantMap target{{"x",1},{"y",0},{"width",2},{"height",1}};
    QVERIFY(!session->placementProposal("moving",target,"cancel")["valid"].toBool());
    QVERIFY(!session->placementProposal("moving",target,"swap")["valid"].toBool());
    QVERIFY(!session->placementProposal("moving",target,"unknown")["valid"].toBool());
    const auto shift=session->placementProposal("moving",target,"shift");
    QVERIFY2(shift["valid"].toBool(),qPrintable(shift["reason"].toString()));
    QVERIFY(shift["placements"].toList().size()>2); // No free 2x1 region: an indirect neighbor must move.
    QCOMPARE(session->placementProposal("moving",target,"shift"),shift);QCOMPARE(session->draft(),original);QCOMPARE(session->changeCount(),count);
    QVERIFY2(session->execute("place",{{"placements",shift["placements"]}}),qPrintable(session->errorString()));QCOMPARE(session->changeCount(),count+1);QCOMPARE(readFile(user),disk);
    QVERIFY(session->undo());QCOMPARE(session->draft(),original);
    QVERIFY(!session->execute("duplicate",{{"id","wide"}}));QCOMPARE(session->draft(),original);QCOMPARE(session->changeCount(),count);
    const auto fill=session->additionProposal("text",{{"x",3},{"y",2},{"width",1},{"height",1}},"cancel");QVERIFY2(fill["valid"].toBool(),qPrintable(fill["reason"].toString()));
    QVERIFY(session->execute("batch",{{"commands",fill["commands"]}}));const auto full=session->draft();const auto fullCount=session->changeCount();
    QVERIFY(!session->additionProposal("clock",{{"x",0},{"y",0},{"width",1},{"height",1}},"shift")["valid"].toBool());QCOMPARE(session->draft(),full);QCOMPARE(session->changeCount(),fullCount);
    QVERIFY(session->undo());QCOMPARE(session->draft(),original);
    QVERIFY(!session->placementProposal("moving",{{"x",1},{"y",0},{"width",3},{"height",1}},"shift")["valid"].toBool());QCOMPARE(session->draft(),original);QCOMPARE(session->changeCount(),count);
    QVERIFY(session->execute("state",{{"id","wide"},{"key","locked"},{"value",true}}));
    const auto locked=session->draft();QVERIFY(!session->placementProposal("moving",target,"shift")["valid"].toBool());QCOMPARE(session->draft(),locked);
    session->invalidate();
    document["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",16},{"rows",10},{"mode","screen"}}}};
    entries=QJsonArray{node("moving",15,9)};for(int y=0;y<8;++y)for(int x=0;x<8;++x)entries.append(node(QString("node-%1-%2").arg(y).arg(x),x,y));
    document["components"]=entries;writeFile(user,QJsonDocument(document).toJson());QVERIFY(home.load(defaults,user));
    session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);const auto bounded=session->draft();
    const auto tooMany=session->placementProposal("moving",{{"x",0},{"y",0},{"width",8},{"height",8}},"shift");
    QVERIFY(!tooMany["valid"].toBool());QVERIFY(tooMany["reason"].toString().contains("64"));QCOMPARE(session->draft(),bounded);
}
void ComponentTests::studioSectionTransfersDuplicationAndOrdering()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");QVERIFY(home.load(defaults,user));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY(session->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5},{"mode","sections"}}},{"sectionHeight",560}}));
    const auto beforeDuplicate=session->draft();const auto count=session->changeCount();
    QVERIFY2(session->execute("duplicate",{{"id","clock"}}),qPrintable(session->errorString()));QCOMPARE(session->changeCount(),count+1);
    const auto copy=QJsonDocument::fromJson(session->draft()).object()["components"].toArray().last().toObject();
    QCOMPARE(copy["categoryId"].toString(),QString("widgets"));QCOMPARE(copy["id"].toString(),QString("clock-copy"));QVERIFY(copy["placement"].isObject());
    QVERIFY(session->undo());QCOMPARE(session->draft(),beforeDuplicate);
    const auto start=session->draft();const QVariantMap target{{"x",0},{"y",4},{"width",1},{"height",1}};
    const auto transfer=session->placementProposal("clock",target,"cancel",{},{{"categoryId","inputs"}});QVERIFY2(transfer["valid"].toBool(),qPrintable(transfer["reason"].toString()));QCOMPARE(session->draft(),start);
    QVERIFY(session->execute("place",{{"placements",transfer["placements"]}}));
    const auto moved=QJsonDocument::fromJson(session->draft()).object()["components"].toArray().first().toObject();QCOMPARE(moved["categoryId"].toString(),QString("inputs"));QCOMPARE(moved["placement"].toObject()["y"].toInt(),4);
    QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    const auto swap=session->placementProposal("clock",{{"x",0},{"y",0},{"width",1},{"height",1}},"swap",{},{{"categoryId","inputs"}});QVERIFY2(swap["valid"].toBool(),qPrintable(swap["reason"].toString()));
    QVERIFY(session->execute("place",{{"placements",swap["placements"]}}));QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    QVERIFY(!session->placementProposal("clock",target,"shift",{},{{"categoryId","missing"}})["valid"].toBool());
    QVERIFY(!session->execute("place",{{"placements",QVariantList{QVariantMap{{"id","clock"},{"placement",target},{"categoryId","missing"}}}}}));QCOMPARE(session->draft(),start);
    QVERIFY(session->execute("orderRelative",{{"id","movies"},{"relativeId","shows"},{"before",false}}));
    const auto ordered=QJsonDocument::fromJson(session->draft()).object()["components"].toArray();int movies=-1,shows=-1;
    for(int i=0;i<ordered.size();++i){const auto item=ordered[i].toObject();if(item["id"]=="movies")movies=i;if(item["id"]=="shows")shows=i;}
    QVERIFY(movies>shows);QCOMPARE(ordered[movies].toObject()["placement"].toObject()["x"].toInt(),0);
    QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    QVERIFY(!session->execute("orderRelative",{{"id","clock"},{"relativeId","shows"},{"before",true}}));QCOMPARE(session->draft(),start);QVERIFY(!QFile::exists(user));
    QVERIFY(session->execute("state",{{"id","movies"},{"key","locked"},{"value",true}}));const auto locked=session->draft();
    QVERIFY(!session->execute("orderRelative",{{"id","movies"},{"relativeId","shows"},{"before",false}}));QCOMPARE(session->draft(),locked);
}
void ComponentTests::studioNestedGroupsValidateAndReparentAtomically()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto user=dir.filePath("home.json");QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    auto doc=QJsonDocument::fromJson(session->draft()).object();doc["components"]=QJsonArray{};doc["categories"]=QJsonArray{};
    doc["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject::fromVariantMap(home.studioOptions()["defaultGrid"].toMap())}};
    QVERIFY2(session->pushEdit(QJsonDocument(doc).toJson()),qPrintable(session->errorString()));
    const auto empty=session->draft();
    const auto panel=session->additionProposal("input-panel",{{"x",0},{"y",0},{"width",4},{"height",3}},"cancel");
    QVERIFY2(panel["valid"].toBool(),qPrintable(panel["reason"].toString()));QCOMPARE(session->draft(),empty);
    QVERIFY(session->execute("addResolved",{{"type","input-panel"},{"placement",QVariantMap{{"x",0},{"y",0},{"width",4},{"height",3}}}}));
    const auto added=session->draft();const auto entries=QJsonDocument::fromJson(added).object()["components"].toArray();QCOMPARE(entries.size(),9);
    QVERIFY2(session->execute("duplicatePage",{{"id",""},{"name","Nested copy"}}),qPrintable(session->errorString()));
    const auto copied=QJsonDocument::fromJson(session->draft()).object()["pages"].toArray().last().toObject()["layout"].toObject()["components"].toArray();QCOMPARE(copied.size(),9);
    QVERIFY(copied.first().toObject()["id"]!=entries.first().toObject()["id"]);QCOMPARE(copied[1].toObject()["parentId"].toString(),copied[0].toObject()["id"].toString());QVERIFY(session->undo());QCOMPARE(session->draft(),added);
    const auto parent=entries.first().toObject()["id"].toString();const auto child=entries[1].toObject()["id"].toString();
    QCOMPARE(ComponentSchema::componentGrid(entries,doc["composition"].toObject().toVariantMap(),{},parent,{})["columns"].toInt(),4);
    const auto transfer=session->placementProposal(child,{{"x",5},{"y",0},{"width",1},{"height",1}},"cancel",{},{{"parentId",""}});
    QVERIFY2(transfer["valid"].toBool(),qPrintable(transfer["reason"].toString()));
    QVERIFY2(session->execute("place",{{"placements",transfer["placements"]}}),qPrintable(session->errorString()));
    QVERIFY(session->undo());QCOMPARE(session->draft(),added);
    QVERIFY(!session->placementProposal(parent,{{"x",0},{"y",0},{"width",1},{"height",1}},"cancel",{},{{"parentId",parent}})["valid"].toBool());QCOMPARE(session->draft(),added);
    auto broken=QJsonDocument::fromJson(added).object();auto badEntries=entries;auto root=badEntries[0].toObject();root["parentId"]=child;badEntries[0]=root;broken["components"]=badEntries;
    QVERIFY(!session->pushEdit(QJsonDocument(broken).toJson()));QCOMPARE(session->draft(),added);
    QVERIFY(session->execute("state",{{"id",parent},{"key","locked"},{"value",true}}));const auto locked=session->draft();
    QVERIFY(!session->placementProposal(child,{{"x",0},{"y",0},{"width",1},{"height",1}},"shift")["valid"].toBool());
    QVERIFY(!session->execute("place",{{"placements",QVariantList{QVariantMap{{"id",child},{"placement",QVariantMap{{"x",5},{"y",0},{"width",1},{"height",1}}},{"parentId",""}}}}}));QCOMPARE(session->draft(),locked);
    QVERIFY(session->undo());QVERIFY(session->undo());QCOMPARE(session->draft(),empty);QVERIFY(!QFile::exists(user));
    QVERIFY(session->execute("addResolved",{{"type","text"},{"placement",QVariantMap{{"x",1},{"y",1},{"width",1},{"height",1}}}}));
    QVERIFY(session->execute("addResolved",{{"type","clock"},{"placement",QVariantMap{{"x",2},{"y",1},{"width",1},{"height",1}}}}));
    const auto beforeGroup=session->draft();const auto count=session->changeCount();
    QVERIFY(!session->execute("arrange",{{"ids",QVariantList{"text","clock"}},{"mode","left"}}));QCOMPARE(session->draft(),beforeGroup);
    QVERIFY(!session->execute("group",{{"ids",QVariantList{"text","text"}}}));QCOMPARE(session->draft(),beforeGroup);
    QVERIFY2(session->execute("group",{{"ids",QVariantList{"text","clock"}}}),qPrintable(session->errorString()));QCOMPARE(session->changeCount(),count+1);
    const auto grouped=QJsonDocument::fromJson(session->draft()).object()["components"].toArray();QCOMPARE(grouped.size(),3);
    QCOMPARE(grouped[0].toObject()["componentType"].toString(),QString("container"));QCOMPARE(grouped[1].toObject()["parentId"].toString(),grouped[0].toObject()["id"].toString());
    QCOMPARE(grouped[1].toObject()["placement"].toObject()["x"].toInt(),0);QCOMPARE(grouped[2].toObject()["placement"].toObject()["x"].toInt(),1);
    const auto groupId=grouped[0].toObject()["id"].toString();const auto groupedBytes=session->draft();
    QVERIFY2(session->execute("duplicate",{{"id",groupId}}),qPrintable(session->errorString()));
    const auto duplicated=QJsonDocument::fromJson(session->draft()).object()["components"].toArray();QCOMPARE(duplicated.size(),6);
    QCOMPARE(duplicated[4].toObject()["parentId"].toString(),duplicated[3].toObject()["id"].toString());QVERIFY(session->undo());QCOMPARE(session->draft(),groupedBytes);
    QVERIFY2(session->execute("remove",{{"id",groupId}}),qPrintable(session->errorString()));QVERIFY(QJsonDocument::fromJson(session->draft()).object()["components"].toArray().isEmpty());QVERIFY(session->undo());QCOMPARE(session->draft(),groupedBytes);
    QVERIFY2(session->execute("ungroup",{{"id",groupId}}),qPrintable(session->errorString()));QCOMPARE(session->draft(),beforeGroup);QVERIFY(session->undo());QCOMPARE(session->draft(),groupedBytes);
    QVERIFY(session->undo());QCOMPARE(session->draft(),beforeGroup);QVERIFY(!QFile::exists(user));
    auto group=QJsonObject::fromVariantMap(session->componentPrototype("container")["root"].toMap());
    group["placement"]=QJsonObject{{"x",0},{"y",0},{"width",1},{"height",1}};
    auto style=group["presentation"].toObject(),local=style["layout"].toObject();local["columns"]=1;local["rows"]=1;style["layout"]=local;group["presentation"]=style;
    QJsonArray deep;for(int level=0;level<4;++level){auto node=group;node["id"]=QString("depth-%1").arg(level);if(level)node["parentId"]=QString("depth-%1").arg(level-1);deep.append(node);}
    QString error;QVERIFY2(ComponentSchema::hierarchyFits(deep,doc["composition"].toObject().toVariantMap(),{},&error),qPrintable(error));
    auto fifth=group;fifth["id"]="depth-4";fifth["parentId"]="depth-3";deep.append(fifth);
    QVERIFY(!ComponentSchema::hierarchyFits(deep,doc["composition"].toObject().toVariantMap(),{},&error));QVERIFY(error.contains("four"));
    auto invalid=local.toVariantMap();invalid["columns"]=17;QVERIFY(!ComponentSchema::validChildLayout(invalid));invalid=local.toVariantMap();invalid["mode"]="script";QVERIFY(!ComponentSchema::validChildLayout(invalid));
}
void ComponentTests::studioCatalogLinksInspectorAndStagedArtwork()
{
    QVERIFY(ComponentSchema::validPresentation({{"libraryKind","movies"},{"libraryView","playlist"},{"libraryValue","Family films.xsp"}},true));
    QVERIFY(!ComponentSchema::validPresentation({{"libraryView","playlist"},{"libraryValue","../private.xsp"}},true));
    QVERIFY(!ComponentSchema::validPresentation({{"libraryView","playlist"},{"libraryValue","https://private.xsp"}},true));
    QTemporaryDir dir;AppRegistry registry;QVERIFY(registry.loadFromData(apps));HomeLayout home(&registry);const auto user=dir.filePath("profile/home.json");QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    const auto options=home.studioOptions();QCOMPARE(options["catalog"].toList().size(),10);QVERIFY(!options["inspector"].toMap()["fields"].toList().isEmpty());
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);auto doc=QJsonDocument::fromJson(session->draft()).object();doc["components"]=QJsonArray{};doc["categories"]=QJsonArray{};
    doc["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject::fromVariantMap(options["defaultGrid"].toMap())}};QVERIFY(session->pushEdit(QJsonDocument(doc).toJson()));const auto empty=session->draft();
    for(const auto &type:QStringList{"button","image","library","recently-viewed","music-library","tv-guide","carousel","app-bar","page-menu"}){const auto prototype=session->componentPrototype(type);QVERIFY2(!prototype.isEmpty(),qPrintable(type+": "+session->errorString()));QCOMPARE(session->draft(),empty);}
    const auto appPrototype=session->componentPrototype("app-bar");
    const auto historyPrototype=session->componentPrototype("recently-viewed");
    const auto historyRoot=historyPrototype["root"].toMap();
    QCOMPARE(historyRoot["family"].toString(),QString("recently-viewed"));
    QCOMPARE(historyRoot["source"].toMap()["kind"].toString(),QString("recently-viewed"));
    const auto musicRoot=session->componentPrototype("music-library")["root"].toMap();
    QCOMPARE(musicRoot["source"].toMap()["kind"].toString(),QString("music"));
    auto wrongMusic=musicRoot;auto musicStyle=musicRoot["presentation"].toMap();musicStyle["libraryView"]="continue";wrongMusic["presentation"]=musicStyle;
    QVERIFY(ComponentSchema::component(wrongMusic,true).isEmpty());wrongMusic=musicRoot;wrongMusic.remove("family");QVERIFY(ComponentSchema::component(wrongMusic,true).isEmpty());
    auto wrongHistory=historyRoot;auto historyStyle=historyRoot["presentation"].toMap();
    historyStyle["libraryView"]="recent";wrongHistory["presentation"]=historyStyle;
    QVERIFY(ComponentSchema::component(wrongHistory,true).isEmpty());
    historyStyle["libraryView"]="watched";wrongHistory["presentation"]=historyStyle;wrongHistory.remove("family");
    QVERIFY(ComponentSchema::component(wrongHistory,true).isEmpty());
    QCOMPARE(appPrototype["components"].toList().size(),1);
    auto appNode=appPrototype["root"].toMap();
    appNode["appCollection"]=QVariantMap{{"mode","all"},{"ids",QVariantList{}},{"showStore",false}};
    QVERIFY(!ComponentSchema::component(appNode,true).isEmpty());
    appNode["appCollection"]=QVariantMap{{"mode","all"},{"ids",QVariantList{}},{"showStore","false"}};
    QVERIFY(ComponentSchema::component(appNode,true).isEmpty());
    appNode["appCollection"]=QVariantMap{{"mode","all"},{"ids",QVariantList{}},{"section","Games"}};
    QVERIFY(!ComponentSchema::component(appNode,true).isEmpty());
    appNode["appCollection"]=QVariantMap{{"mode","all"},{"ids",QVariantList{}},{"section",""}};
    QVERIFY(ComponentSchema::component(appNode,true).isEmpty());
    appNode=appPrototype["root"].toMap();
    QCOMPARE(appNode["appCollection"].toMap()["mode"].toString(),QString("all"));
    appNode["appCollection"]=QVariantMap{{"mode","selected"},{"ids",QVariantList{"duplicate","duplicate"}}};
    QVERIFY(ComponentSchema::component(appNode,true).isEmpty());
    appNode["appCollection"]=QVariantMap{{"mode","all"},{"ids",QVariantList{"unsafe/identity"}}};
    QVERIFY(ComponentSchema::component(appNode,true).isEmpty());
    const QVariantMap box{{"x",0},{"y",0},{"width",2},{"height",1}};QVERIFY(session->execute("addResolved",{{"type","button"},{"placement",box}}));
    QVERIFY(session->execute("patch",{{"id","button"},{"value",QVariantMap{{"action",QVariantMap{{"type","root-page"}}}}}}));
    const auto linked=session->draft();QVERIFY(!session->execute("patch",{{"id","button"},{"value",QVariantMap{{"action",QVariantMap{{"type","open-page"},{"pageId","absent"}}}}}}));QCOMPARE(session->draft(),linked);
    QVERIFY(session->execute("patch",{{"id","button"},{"value",QVariantMap{{"action",QVariantMap{}}}}}));QVERIFY(!QJsonDocument::fromJson(session->draft()).object()["components"].toArray().first().toObject().contains("action"));
    QVERIFY(session->execute("patch",{{"id","button"},{"value",QVariantMap{{"presentation",QVariantMap{{"surfaceColor","#123456"}}}}}}));
    QVERIFY(session->execute("patch",{{"id","button"},{"value",QVariantMap{{"presentation",QVariantMap{{"surfaceColor",QVariant()}}}}}}));QVERIFY(!QJsonDocument::fromJson(session->draft()).object()["components"].toArray().first().toObject()["presentation"].toObject().contains("surfaceColor"));
    QImage image(32,24,QImage::Format_RGB32);image.fill(QColor("#56789a"));const auto source=dir.filePath("chosen.png");QVERIFY(image.save(source));
    const auto relative=session->stageArtwork(source);QVERIFY2(relative.startsWith("artwork/studio-"),qPrintable(session->errorString()));QVERIFY(!QFile::exists(dir.filePath("profile/"+relative)));QVERIFY(!QFile::exists(user));
    QVERIFY(session->execute("patch",{{"id","button"},{"value",QVariantMap{{"customLogo",relative}}}}));
    auto *model=qobject_cast<HomeLayout *>(session->model());QVERIFY(model);const auto staged=QUrl(model->items().first().toMap()["logoSource"].toString()).toLocalFile();QVERIFY(QFile::exists(staged));QVERIFY(!staged.startsWith(dir.filePath("profile")));
    QVERIFY(session->execute("pagePatch",{{"scope","theme"},{"value",QVariantMap{{"focusColor",QVariant()}}}}));QVERIFY(!QJsonDocument::fromJson(session->draft()).object()["theme"].toObject().contains("focusColor"));
    QVERIFY(session->execute("rename",{{"id","button"},{"name","Artwork button"}}));QVERIFY(!QJsonDocument::fromJson(session->draft()).object()["theme"].toObject().contains("focusColor"));
    const auto beforeFailure=session->draft();QVERIFY(QDir().mkpath(dir.filePath("profile")));QFile blocked(dir.filePath("profile/artwork"));QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.write("blocked");blocked.close();
    QVERIFY(!session->apply("button"));QCOMPARE(session->draft(),beforeFailure);QVERIFY(!QFile::exists(user));QCOMPARE(home.focusOrigin(),QString());QVERIFY(QFile::remove(blocked.fileName()));
    QVERIFY(session->apply("button"));QVERIFY(QFile::exists(dir.filePath("profile/"+relative)));QCOMPARE(readFile(dir.filePath("profile/"+relative)),readFile(source));QVERIFY(!QJsonDocument::fromJson(readFile(user)).object()["theme"].toObject().contains("focusColor"));
    QVERIFY(session->stageArtwork("https://example.com/image.png").isEmpty());QVERIFY(session->stageArtwork(user).isEmpty());
    session->invalidate();QVERIFY(!QFile::exists(staged));QVERIFY(QFile::exists(dir.filePath("profile/"+relative)));
    auto invalid=options["componentPresets"].toMap()["image"].toMap();invalid["action"]=QVariantMap{{"type","exec"},{"target","bad"}};QVERIFY(ComponentSchema::component(invalid,true).isEmpty());
    invalid=options["componentPresets"].toMap()["library"].toMap();auto style=invalid["presentation"].toMap();style["libraryLimit"]=51;invalid["presentation"]=style;QVERIFY(ComponentSchema::component(invalid,true).isEmpty());
}
void ComponentTests::studioOrderedInsertionPreservesSlotsAndIsAtomic()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("ordered.json");QVERIFY(home.load(defaults,user));
    auto *setup=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(setup);
    auto group=QJsonObject::fromVariantMap(setup->componentPrototype("container")["root"].toMap());group["id"]="ordered";
    group["placement"]=QJsonObject{{"x",0},{"y",0},{"width",4},{"height",2}};
    auto style=group["presentation"].toObject();style["layout"]=QJsonObject{{"schemaVersion",1},{"mode","row"},{"columns",6},{"rows",1},{"visibleColumns",2},{"wrap",true},{"gap",12},{"padding",0}};group["presentation"]=style;
    auto doc=QJsonDocument::fromJson(home.committedDocument()).object();doc["categories"]=QJsonArray{};
    doc["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",4},{"rows",3},{"mode","screen"}}}};
    QJsonArray nodes{group};
    for(int i=0;i<3;++i){auto node=QJsonObject::fromVariantMap(ComponentSchema::component({{"id",QString("ordered-%1").arg(i)},{"kind","widget"},{"widgetType","text"},{"name",QString("Child %1").arg(i)},{"parentId","ordered"},{"placement",QVariantMap{{"x",i==0?0:i==1?3:5},{"y",0},{"width",i==0?2:1},{"height",1}}}},true));QVERIFY(!node.isEmpty());nodes.append(node);}
    auto outside=QJsonObject::fromVariantMap(ComponentSchema::component({{"id","outside"},{"kind","widget"},{"widgetType","text"},{"name","Outside"},{"placement",QVariantMap{{"x",0},{"y",2},{"width",1},{"height",1}}}},true));nodes.append(outside);doc["components"]=nodes;
    setup->invalidate();writeFile(user,QJsonDocument(doc).toJson());QVERIFY(home.load(defaults,user));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    const auto before=session->draft(),disk=readFile(user);const auto count=session->changeCount();
    for(const auto &args:QVariantList{
        QVariantMap{{"id","ordered-0"},{"relativeId","outside"},{"before",false}},
        QVariantMap{{"id","ordered-0"},{"relativeId","ordered-0"},{"before",false}},
        QVariantMap{{"id","ordered-0"},{"relativeId","ordered-2"},{"before","false"}},
        QVariantMap{{"id","outside"},{"relativeId","ordered-2"},{"before",false}}}){
        QVERIFY(!session->execute("insertRelative",args.toMap()));QVERIFY(!session->errorString().isEmpty());QCOMPARE(session->draft(),before);QCOMPARE(session->changeCount(),count);
    }
    const QVariantMap insert{{"id","ordered-0"},{"relativeId","ordered-2"},{"before",false}};
    QVERIFY2(session->previewCommand("insertRelative",insert),qPrintable(session->errorString()));QCOMPARE(session->draft(),before);QCOMPARE(session->changeCount(),count);
    QVERIFY(session->acceptPreview());QCOMPARE(session->changeCount(),count+1);
    const auto inserted=QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
    QCOMPARE(inserted[0],nodes[0]);QCOMPARE(inserted[4],nodes[4]);
    for(int i=0;i<3;++i){auto expected=nodes[i+1].toObject(),p=expected["placement"].toObject();p["x"]=i==0?4:i==1?0:2;expected["placement"]=p;QCOMPARE(inserted[i+1].toObject(),expected);}
    QCOMPARE(readFile(user),disk);QVERIFY(session->undo());QCOMPARE(session->draft(),before);
    QVERIFY(session->execute("state",{{"id","ordered-1"},{"key","locked"},{"value",true}}));const auto locked=session->draft();const auto lockedCount=session->changeCount();
    QVERIFY(!session->execute("insertRelative",insert));QCOMPARE(session->draft(),locked);QCOMPARE(session->changeCount(),lockedCount);QCOMPARE(readFile(user),disk);
    QVERIFY(session->undo());QCOMPARE(session->draft(),before);
    QVERIFY(session->execute("orderRelative",insert));
    for(const auto &entry:QJsonDocument::fromJson(session->draft()).object()["components"].toArray())for(const auto &original:nodes)if(entry.toObject()["id"]==original.toObject()["id"])QCOMPARE(entry.toObject()["placement"],original.toObject()["placement"]);
    QVERIFY(session->undo());QCOMPARE(session->draft(),before);
}
void ComponentTests::studioOverlayMovementKeepsNeighborsAndBounds()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    const auto user=dir.filePath("layers.json");QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY(session->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5}}}}));
    QVERIFY(session->execute("grid",{{"value",QVariantMap{{"overlap",true}}}}));
    QVERIFY(session->execute("state",{{"id","movies"},{"key","locked"},{"value",true}}));
    const auto before=session->draft();const auto count=session->changeCount();
    const QVariantMap target{{"x",1},{"y",0},{"width",1},{"height",1}};
    for(const auto &strategy:QStringList{"shift","cancel"}){
        const auto proposal=session->placementProposal("clock",target,strategy);QVERIFY2(proposal["valid"].toBool(),qPrintable(proposal["reason"].toString()));
        QCOMPARE(proposal["placements"].toList().size(),1);QCOMPARE(session->draft(),before);QCOMPARE(session->changeCount(),count);
        QVERIFY(session->execute("place",{{"placements",proposal["placements"]}}));QCOMPARE(session->changeCount(),count+1);
        const auto original=QJsonDocument::fromJson(before).object()["components"].toArray(),moved=QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
        for(int i=0;i<original.size();++i)if(original[i].toObject()["id"]!="clock")QCOMPARE(moved[i],original[i]);
        const auto overlapping=session->draft();QVERIFY(!session->execute("grid",{{"value",QVariantMap{{"overlap",false}}}}));QCOMPARE(session->draft(),overlapping);QCOMPARE(session->changeCount(),count+1);
        QVERIFY(session->undo());QCOMPARE(session->draft(),before);
    }
    QVERIFY(!session->placementProposal("clock",target,"swap")["valid"].toBool()); // Explicit swap cannot move a locked neighbor.
    QVERIFY(!session->placementProposal("clock",{{"x",8},{"y",0},{"width",1},{"height",1}},"shift")["valid"].toBool());
    QVERIFY(session->execute("state",{{"id","clock"},{"key","locked"},{"value",true}}));const auto locked=session->draft();
    QVERIFY(!session->placementProposal("clock",target,"shift")["valid"].toBool());QCOMPARE(session->draft(),locked);
    QVERIFY(!QFile::exists(user));
}
QTEST_GUILESS_MAIN(ComponentTests)
#include "tst_components.moc"
