#include "games/GameLibrary.h"
#include "games/GameEmulators.h"
#include <QStandardPaths>
#include <QUuid>
#include "app/AppRegistry.h"
#include "input/InputManager.h"
#include "media/MediaGrants.h"
#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QJSValue>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QFile>
#include <QScopeGuard>

class GameDispatchProbe : public QObject {
    Q_OBJECT
public:
    QVariantMap lastAction;
    QString origin;
    Q_INVOKABLE bool dispatch(const QVariantMap &action,const QString &from) {lastAction=action;origin=from;return true;}
signals:
    void rejected(const QString &reason);
};
class GamesUiTests : public QObject {
    Q_OBJECT
private slots:
    void remoteSteamImportCanCancelReviewAndPlay();
    void remoteDetailsFavoriteRemoveAndCancel();
    void remoteAddReviewWritesOnlyOnConfirm();
    void remoteEditCanCancelAndApplyWithoutMouse();
    void remoteEditionChooserDispatchesOwnedVersion();
    void remoteEmulatorSetupCancelsAndSavesWithoutMouse();
    void platformShelvesFilterCanonicalGamesRemotely();
    void organizationAndManualSmartCollectionsStayProfileLocal();
    void catalogExportRestoreIsBoundedAndTransactional();
};
static AppDefinition runtime() {
    AppDefinition app;app.id="local-test";app.name="Local game";app.type="desktop";
    app.presentationMode="game";app.section="Games";app.accent=QColor("#556677");
    app.launchConfiguration={{"desktopId","isolde-test-game"}};return app;
}
static void command(QObject *widget,int value) {
    QVariant result;QVERIFY(QMetaObject::invokeMethod(widget,"handleCommand",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,value)));
}
static void doneOnKeyboard(QObject *widget) {
    for(int i=0;i<5;++i)command(widget,InputManager::NavigateDown);
    for(int i=0;i<9;++i)command(widget,InputManager::NavigateRight);
    command(widget,InputManager::Confirm);
}
void GamesUiTests::remoteSteamImportCanCancelReviewAndPlay(){
    QStandardPaths::setTestModeEnabled(true);const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);qApp->setProperty("isoldeProfile",profile);
    const auto parent=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+profile;
    const auto path=parent+"/steam/home/.local/share/Steam/steamapps";QVERIFY(QDir().mkpath(path+"/common/Fixture"));
    QFile manifest(path+"/appmanifest_42.acf");QVERIFY(manifest.open(QIODevice::WriteOnly));manifest.write("\"AppState\" { \"appid\" \"42\" \"name\" \"Steam fixture\" \"StateFlags\" \"4\" \"installdir\" \"Fixture\" }");manifest.close();
    QTemporaryDir dir;AppRegistry apps;auto steam=runtime();steam.id="steam";steam.name="Steam";steam.presentationMode="game-hub";steam.launchConfiguration={{"desktopId","isolde-steam"}};QVERIFY(apps.addApp(steam));
    GameLibrary library(dir.filePath("games.sqlite"),&apps);InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"originId","steam-origin"},{"width",1000},{"height",600},{"engaged",true}}));QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QTRY_COMPARE_WITH_TIMEOUT(widget->property("mode").toString(),QString("importPreview"),10000);QVERIFY(library.page().isEmpty());
    command(widget.data(),InputManager::Confirm);QVERIFY(library.page().isEmpty()); // Default Cancel.
    QVERIFY(manifest.open(QIODevice::ReadOnly));const auto validManifest=manifest.readAll();manifest.close();
    QVERIFY(manifest.open(QIODevice::WriteOnly|QIODevice::Truncate));manifest.write("invalid Steam metadata");manifest.close();
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QTRY_COMPARE_WITH_TIMEOUT(widget->property("mode").toString(),QString("browse"),10000);
    QVERIFY(widget->property("message").toString().contains("Steam discovery failed"));QVERIFY(library.page().isEmpty());
    QVERIFY(manifest.open(QIODevice::WriteOnly|QIODevice::Truncate));manifest.write(validManifest);manifest.close();
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QTRY_COMPARE_WITH_TIMEOUT(widget->property("mode").toString(),QString("importPreview"),10000);
    command(widget.data(),InputManager::NavigateUp);command(widget.data(),InputManager::Confirm);QCOMPARE(library.page().size(),1);
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QCOMPARE(dispatch.lastAction.value("type").toString(),QString("launch-game"));QCOMPARE(dispatch.origin,QString("steam-origin"));
    QCOMPARE(library.resolveLaunch(dispatch.lastAction.value("gameId").toString()).value("steamAppId").toString(),QString("42"));
    qApp->setProperty("isoldeProfile",QVariant());QVERIFY(QDir(parent).removeRecursively());
}
void GamesUiTests::remoteEmulatorSetupCancelsAndSavesWithoutMouse(){
    QStandardPaths::setTestModeEnabled(true);const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    qApp->setProperty("isoldeProfile",profile);
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("games.sqlite"),&apps);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    for(int i=0;i<8;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("emulators"));
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("emulatorSettings"));
    const auto file=GameEmulators::home(profile,"ppsspp")+"/.config/isolde-emulator/settings.json";
    command(widget.data(),InputManager::Confirm);QVERIFY(!QFileInfo::exists(file)); // default Cancel
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    for(int i=0;i<4;++i)command(widget.data(),InputManager::NavigateUp);
    command(widget.data(),InputManager::Confirm); // automatic -> software, draft only
    QVERIFY(!QFileInfo::exists(file));
    for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateDown);
    command(widget.data(),InputManager::Confirm);QCOMPARE(library.emulatorSettings("ppsspp").value("video").toString(),QString("software"));
    QVERIFY(!library.installEmulators(false));QVERIFY(!library.setupBusy());
    QVERIFY(QDir(QFileInfo(GameEmulators::home(profile,"ppsspp")).absolutePath()).removeRecursively());
    qApp->setProperty("isoldeProfile",QVariant());
}
void GamesUiTests::platformShelvesFilterCanonicalGamesRemotely(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);QVERIFY(library.ready());
    const auto add=[&library](const QString &title,const QString &platform){
        return library.addGame({{"schemaVersion",1},{"title",title},{"platform",platform},
            {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    };
    const auto pc=add("Desk fixture","PC"),psp=add("Handheld fixture","PSP");
    QVERIFY(!pc.isEmpty());QVERIFY(!psp.isEmpty());
    QVERIFY(library.setFavorite(psp,true));
    QCOMPARE(library.platforms().size(),2);
    QCOMPARE(library.page("platform",{},0,"psp").size(),1);
    QCOMPARE(library.page("platform",{},0,"PSP").first().toMap().value("id").toString(),psp);
    QCOMPARE(library.page("favorites").size(),1);
    QVERIFY(library.page("platform",{},0,QString()).isEmpty());
    QVERIFY(library.page("continue").isEmpty());
    QVERIFY(!library.recordCompletedSession("missing-game"));
    QVERIFY(library.recordCompletedSession(psp));
    QCOMPARE(library.page("continue").size(),1);
    GameLibrary reopened(dir.filePath("profile/games.sqlite"),&apps);
    QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.page("continue").size(),1); // v3 activity without an editions table

    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({
        {"library",QVariant::fromValue(&library)},{"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY2(widget,qPrintable(component.errorString()));
    for(int i=0;i<4;++i)command(widget.data(),InputManager::NavigateRight); // Platforms
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("platformChooser"));
    command(widget.data(),InputManager::NavigateDown);
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("browse"));
    QCOMPARE(widget->property("queryView").toString(),QString("platform"));
    QCOMPARE(widget->property("selectedPlatform").toString(),QString("PSP"));
    const auto items=widget->property("items");
    const auto listed=items.metaType()==QMetaType::fromType<QJSValue>()?items.value<QJSValue>().toVariant().toList():items.toList();
    QCOMPARE(listed.size(),1);QCOMPARE(listed.first().toMap().value("id").toString(),psp);

    QScopedPointer<QObject> continueWidget(component.createWithInitialProperties({
        {"library",QVariant::fromValue(&library)},{"style",QVariantMap{{"libraryView","continue"}}},
        {"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY2(continueWidget,qPrintable(component.errorString()));
    QCOMPARE(continueWidget->property("queryView").toString(),QString("continue"));
    const auto continued=continueWidget->property("items");
    const auto continueItems=continued.metaType()==QMetaType::fromType<QJSValue>()?continued.value<QJSValue>().toVariant().toList():continued.toList();
    QCOMPARE(continueItems.size(),1);QCOMPARE(continueItems.first().toMap().value("id").toString(),psp);
}
void GamesUiTests::organizationAndManualSmartCollectionsStayProfileLocal(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    const auto id=library.addGame({{"schemaVersion",1},{"title","Example title"},{"platform","PC"},
        {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    QVERIFY(!id.isEmpty());
    QVERIFY(library.setOrganization(id,"Example Saga",{"Puzzle","Co-op"}));
    QCOMPARE(library.details(id).value("franchise").toString(),QString("Example Saga"));
    QCOMPARE(library.details(id).value("tags").toStringList(),QStringList({"Puzzle","Co-op"}));
    QCOMPARE(library.page("search","saga").size(),1);
    QCOMPARE(library.page("search","co-op").size(),1);
    QVERIFY(!library.setOrganization(id,"",{"same","SAME"}));
    QCOMPARE(library.details(id).value("franchise").toString(),QString("Example Saga"));

    const auto manual=library.createCollection("Favorites to play","manual");
    const auto smart=library.createCollection("Puzzle games","smart","title","example");
    const auto favorites=library.createCollection("My favorites","smart","favorite");
    const auto literal=library.createCollection("Literal query","smart","title","x' OR 1=1 --");
    QVERIFY(!manual.isEmpty());QVERIFY(!smart.isEmpty());QVERIFY(!favorites.isEmpty());QVERIFY(!literal.isEmpty());
    QVERIFY(library.setCollectionMember(manual,id,true));
    QVERIFY(library.collectionContains(manual,id));
    QCOMPARE(library.pageCollection(manual).size(),1);
    QCOMPARE(library.pageCollection(smart).size(),1);
    QVERIFY(library.setFavorite(id,true));
    QCOMPARE(library.pageCollection(favorites).size(),1);
    QCOMPARE(library.pageCollection(literal).size(),0);
    QVERIFY(!library.setCollectionMember(smart,id,true));
    QVERIFY(library.setCollectionMember(manual,id,false));
    QVERIFY(!library.collectionContains(manual,id));
    QVERIFY(library.setCollectionMember(manual,id,true));
    QVERIFY(library.removeGame(id));
    QCOMPARE(library.pageCollection(manual).size(),0);
    QCOMPARE(library.collections().size(),4);

    GameLibrary reopened(dir.filePath("profile/games.sqlite"),&apps);
    QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.collections().size(),4);
    QCOMPARE(reopened.pageCollection(smart).size(),0);
    QVERIFY(reopened.removeCollection(manual));
    QCOMPARE(reopened.collections().size(),3);
}
void GamesUiTests::catalogExportRestoreIsBoundedAndTransactional(){
    QTemporaryDir dir;QVERIFY(dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    const auto previousProfile=qApp->property("isoldeProfile");
    const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);qApp->setProperty("isoldeProfile",profile);
    auto cleanup=qScopeGuard([&]{qApp->setProperty("isoldeProfile",previousProfile);MediaGrants::save(profile,{});});
    QVERIFY2(MediaGrants::save(profile,{dir.path()}).isEmpty(),"Test export folder grant could not be prepared.");
    AppRegistry apps;QVERIFY(apps.addApp(runtime()));auto second=runtime();second.id="other-test";QVERIFY(apps.addApp(second));
    const auto database=dir.filePath("profile/games.sqlite");
    GameLibrary library(database,&apps);
    QVariantMap registration{{"schemaVersion",1},{"title","Portable fixture"},{"platform","PC"},
        {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}};
    const auto game=library.addGame(registration);QVERIFY(!game.isEmpty());
    registration["title"]="Alternate edition";registration["platform"]="PSP";registration["runtimeId"]="other-test";
    const auto edition=library.addEdition(game,registration);QVERIFY(!edition.isEmpty());
    QVERIFY(library.setOrganization(game,"Fixture series",{"Puzzle"}));
    QVERIFY(library.setFavorite(game,true));QVERIFY(library.recordCompletedSession(game));
    const auto collection=library.createCollection("Export set","manual");QVERIFY(!collection.isEmpty());
    QVERIFY(library.setCollectionMember(collection,game,true));
    const auto backup=dir.filePath("games-backup.json");
    QVERIFY2(library.exportCatalog(backup),qPrintable(library.errorString()));
    QVERIFY(QFileInfo(backup).size()>0);
    QVERIFY(!library.restoreCatalog(backup,false));
    const auto invalidBackup=dir.filePath("invalid-backup.json");
    QFile invalid(invalidBackup);QVERIFY(invalid.open(QIODevice::WriteOnly));
    QVERIFY(invalid.write("{\"schemaVersion\":1}")>0);invalid.close();
    QVERIFY(!library.restoreCatalog(invalidBackup,true));
    QCOMPARE(library.page().size(),1);
    QVERIFY(library.removeGame(game));QVERIFY(library.page().isEmpty());
    QVERIFY2(library.restoreCatalog(backup,true),qPrintable(library.errorString()));
    QCOMPARE(library.details(game).value("franchise").toString(),QString("Fixture series"));
    QCOMPARE(library.details(game).value("tags").toStringList(),QStringList({"Puzzle"}));
    QCOMPARE(library.editions(game).size(),2);QCOMPARE(library.page("continue").size(),1);
    QCOMPARE(library.pageCollection(collection).size(),1);QVERIFY(library.collectionContains(collection,game));
    QCOMPARE(QDir(QFileInfo(database).absolutePath()).entryList({"games.sqlite.pre-restore-*.sqlite"},QDir::Files).size(),1);
    GameLibrary reopened(database,&apps);QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.pageCollection(collection).size(),1);QCOMPARE(reopened.editions(game).size(),2);
}
void GamesUiTests::remoteEditionChooserDispatchesOwnedVersion() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));auto secondRuntime=runtime();secondRuntime.id="other-test";QVERIFY(apps.addApp(secondRuntime));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    QVariantMap registration{{"schemaVersion",1},{"title","Shared title"},{"platform","PC"},{"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}};
    const auto game=library.addGame(registration);QVERIFY(!game.isEmpty());registration["runtimeId"]="other-test";registration["platform"]="PSP";registration["title"]="Alternate";
    const auto edition=library.addEdition(game,registration);QVERIFY2(!edition.isEmpty(),qPrintable(library.errorString()));
    QCOMPARE(library.platforms().size(),2);QCOMPARE(library.page("platform",{},0,"psp").size(),1);
    InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"originId","game-origin"},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("editions"));QVERIFY(dispatch.lastAction.isEmpty());
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    QCOMPARE(dispatch.lastAction.value("gameId").toString(),game);QCOMPARE(dispatch.lastAction.value("editionId").toString(),edition);
    QCOMPARE(dispatch.origin,QString("game-origin"));QCOMPARE(library.page().size(),1);
    QVERIFY(library.recordCompletedSession(game));
    GameLibrary reopened(dir.filePath("profile/games.sqlite"),&apps);QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.editions(game).size(),2);QCOMPARE(reopened.page("continue").size(),1);
}
void GamesUiTests::remoteEditCanCancelAndApplyWithoutMouse() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    const auto id=library.addGame({{"schemaVersion",1},{"title","Original"},{"platform","PC"},{"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});QVERIFY(!id.isEmpty());
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("runtime"));
    command(widget.data(),InputManager::Back);QCOMPARE(library.details(id).value("title").toString(),QString("Original"));
    command(widget.data(),InputManager::Confirm);for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::Confirm); // append digit 1 to existing title
    doneOnKeyboard(widget.data());doneOnKeyboard(widget.data());
    QCOMPARE(widget->property("mode").toString(),QString("review"));QCOMPARE(library.details(id).value("title").toString(),QString("Original"));
    command(widget.data(),InputManager::Confirm);QCOMPARE(library.details(id).value("title").toString(),QString("Original1"));
    QCOMPARE(library.page().size(),1);
}
void GamesUiTests::remoteDetailsFavoriteRemoveAndCancel() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);QVERIFY(library.ready());
    const auto id=library.addGame({{"schemaVersion",1},{"title","Playable fixture"},{"platform","PC"},{"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    QVERIFY2(!id.isEmpty(),qPrintable(library.errorString()));
    InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QTRY_VERIFY_WITH_TIMEOUT(component.status()!=QQmlComponent::Loading,5000);
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"originId","games-shelf"},{"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY(widget);
    const auto items=widget->property("items");
    QCOMPARE(items.metaType()==QMetaType::fromType<QJSValue>()?items.value<QJSValue>().toVariant().toList().size():items.toList().size(),1);
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("details"));
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(dispatch.lastAction.value("gameId").toString(),id);QCOMPARE(dispatch.origin,QString("games-shelf"));
    QCOMPARE(widget->property("selected").toInt(),0);
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    QVERIFY(library.details(id).value("favorite").toBool());
    command(widget.data(),InputManager::Back);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("remove"));
    command(widget.data(),InputManager::Confirm);QVERIFY(!library.details(id).isEmpty()); // default Cancel
    command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    QVERIFY(library.details(id).isEmpty());
}
void GamesUiTests::remoteAddReviewWritesOnlyOnConfirm() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));
    QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("runtime"));
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("keyboard"));
    command(widget.data(),InputManager::Back);QVERIFY(library.page().isEmpty());
    // Enter the title using only the real on-screen keyboard's remote path.
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::Confirm); // first key = 1
    for(int i=0;i<5;++i)command(widget.data(),InputManager::NavigateDown);
    for(int i=0;i<9;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm); // Done: title
    QCOMPARE(widget->property("field").toInt(),1);
    for(int i=0;i<5;++i)command(widget.data(),InputManager::NavigateDown);
    for(int i=0;i<9;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm); // Done: default platform
    QCOMPARE(widget->property("mode").toString(),QString("review"));QVERIFY(library.page().isEmpty());
    command(widget.data(),InputManager::Confirm);QCOMPARE(library.page().size(),1);
    QCOMPARE(library.page()[0].toMap().value("title").toString(),QString("1"));
}
QTEST_MAIN(GamesUiTests)
#include "games_ui.moc"
