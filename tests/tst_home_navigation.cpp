#include "input/InputManager.h"
#include "home/HomeLayout.h"
#include "media/KodiMedia.h"
#include "media/KodiArtwork.h"
#include "media/KodiService.h"
#include "home/PageData.h"
#include "home/PageModel.h"
#include "home/StudioSession.h"
#include "home/ComponentSchema.h"
#include "settings/SearchSettings.h"
#include "media/KodiImporter.h"
#include <QBuffer>
#include <QImage>
#include <functional>
#include <algorithm>
#include "sessions/SessionManager.h"
#include <QLocalServer>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QDir>
#include <QJsonArray>
#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QFile>
#include <QJSValue>
#include <QJsonDocument>

class HomeNavigationTests : public QObject {
    Q_OBJECT
private slots:
    void retainsExactOriginAndRecoversWithoutHomeButton();
    void customizeOpensStudioAndPreservesDirtyExit();
    void studioRemoteDraftApplyAndCancel();
    void studioRemotePointerAndInspectorParity();
    void studioRemotePointerAndInspectorParity_data();
    void studioPreservesLegacyGridMetadataWithoutConversion();
    void studioSectionGeometryMatchesRuntime();
    void studioPageMenuCreatesAndDesignatesHome();
    void studioNamedPageViewportMatchesRuntime();
    void studioRemoteGesturesCollisionsAndAnchors();
    void studioRemoteSectionTransferAndCancel();
    void studioLegacyRemoteOrdering();
    void studioScrollingAddMoveAndCancel();
    void repairsSourceWithRemoteAndCanCancel();
    void televisionLibraryRemoteFlow();
    void universalLocalSearchRemoteFlow();
    void searchHeaderSwitchRemoteFocusAndStableResults();
    void searchPreferenceProfileIsolationAndWriteFailure();
    void appSuppliedIconsUseManagedArtwork();
    void pageScopedSourcesAppearInSearch();
    void kodiSettingsFollowUserTheme();
    void appearancePreviewCancelAndApply();
    void composedReferenceNavigationAndGeometry();
    void pastelReferenceNavigationAndGeometry();
    void skinsAndDesignAreReachableFromSettings();
    void pageComposerAndIndependentKodiWidgets();
    void gridEditorMoveResizeDuplicateRemove();
    void studioNestedGroupCanvasAndRemoteScope();
    void studioCatalogAndContextualInspector();
};
void HomeNavigationTests::customizeOpensStudioAndPreservesDirtyExit()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    const auto user=dir.filePath("home.json");QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);
    engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(CUSTOMIZE_QML)));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    QQuickWindow window;window.resize(1280,720);
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    auto *studio=root->findChild<QObject *>("pageStudio");QVERIFY(studio);
    QTRY_VERIFY(studio->property("open").toBool());
    QCOMPARE(studio->property("mode").toString(),QString("canvas"));
    QVERIFY(studio->property("toolbarFocus").toBool());
    QVERIFY(!root->findChild<QObject *>("designEditor"));
    auto command=[&](int key){QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,key)));QCoreApplication::processEvents();};
    command(InputManager::Confirm);
    auto *keyboard=studio->findChild<QObject *>("studioKeyboard");QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("Library"))));QCoreApplication::processEvents();
    auto *session=qobject_cast<StudioSession *>(studio->property("session").value<QObject *>());QVERIFY(session);QVERIFY(session->isDirty());
    QVERIFY(!QFile::exists(user));
    command(InputManager::Back);command(InputManager::Back);
    QCOMPARE(studio->property("mode").toString(),QString("dirty"));
    command(InputManager::Confirm); // Keep editing, not a legacy editor underneath.
    QVERIFY(studio->property("open").toBool());QVERIFY(session->isDirty());
    root->setProperty("visible",false);QCoreApplication::processEvents();
    QVERIFY(!session->active());QVERIFY(!QFile::exists(user));
    root->setProperty("visible",true);
    QTRY_VERIFY2(studio->property("open").toBool(),qPrintable(QString("visible=%1 ready=%2 editing=%3 skins=%4 session=%5")
        .arg(root->property("visible").toBool()).arg(root->property("ready").toBool()).arg(root->property("editing").toBool())
        .arg(root->findChild<QObject *>("skinLibrary")->property("open").toBool()).arg(studio->property("session").value<QObject *>()!=nullptr)));
    QCOMPARE(studio->property("pageId").toString(),QString());
}

void HomeNavigationTests::pageComposerAndIndependentKodiWidgets()
{
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));
    int reads=0,plays=0,rejectedReads=0;QJsonObject played;
    QImage image(20,30,QImage::Format_RGB32);image.fill(QColor("#886677"));QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");
    connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{
            const auto bytes=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",bytes);if(!bytes.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(bytes).object();const auto method=request["method"].toString();QJsonObject result;
            if(method=="JSONRPC.Version")result={{"version",QJsonObject{{"major",13}}}};
            else if(method=="Isolde.GetArtwork")result={{"mime","image/png"},{"data",QString::fromLatin1(png.toBase64())}};
            else if(method=="Isolde.PlayMovie"||method=="Isolde.PlayEpisode"){++plays;played=request["params"].toObject();result={{"state","playing"},{"home",false}};}
            else if(method.startsWith("Isolde.Playback"))result={{"state",method=="Isolde.PlaybackControl"?"ended":"playing"},{"home",false}};
            else {++reads;if(rejectedReads>0){--rejectedReads;socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request["id"]},{"error",QJsonObject{{"code",-32000},{"message","Kodi starting"}}}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();return;}const auto key=method=="VideoLibrary.GetTVShows"?"tvshows":method=="VideoLibrary.GetEpisodes"?"episodes":"movies";QJsonArray items;
                for(int i=1;i<=8;++i)items.append(QJsonObject{{key==QString("movies")?"movieid":key==QString("episodes")?"episodeid":"tvshowid",i},{"title",QString("Kodi title %1").arg(i)},{"year",2020+i},{"lastplayed",QString("2026-10-%1 12:00:00").arg(i,2,10,QChar('0'))},{"resume",QJsonObject{{"position",i*10},{"total",120}}}});
                result={{key,items},{"limits",QJsonObject{{"start",0},{"end",8},{"total",8}}}};}
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request["id"]},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });}});
    AppRegistry registry;HomeLayout home(&registry);QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    KodiMedia player("page-ui",nullptr,dir.path());PageData data("page-ui",&player,nullptr,dir.path());InputManager input;QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);
    engine.rootContext()->setContextProperty("isoldeVersion","0.0.1.2");
    QQmlComponent settings(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("LegacyCustomizeHomeView.qml")));
    QVERIFY2(settings.isReady(),qPrintable(settings.errorString()));
    std::unique_ptr<QObject> controls(settings.createWithInitialProperties({{"items",home.items()},{"availableApps",QVariantList{}},{"artwork",QVariantList{}},{"themes",QVariantList{}},{"width",1280},{"height",800}}));QVERIFY(controls);
    auto control=[&](int value){return QMetaObject::invokeMethod(controls.get(),"handleCommand",Q_ARG(QVariant,value));};
    controls->setProperty("actionIndex",25);QVERIFY(control(InputManager::Confirm));
    auto *manager=controls->findChild<QObject *>("pageManager");QVERIFY(manager);manager->setProperty("operation",3);QVERIFY(control(InputManager::Confirm));
    auto *keyboard=manager->findChild<QObject *>("pageNameKeyboard");QVERIFY(keyboard);QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("My cinema"))));
    QCOMPARE(home.pages().size(),1);const auto pageId=home.pages()[0].toMap()["id"].toString();auto *page=qobject_cast<PageModel *>(home.pageModel(pageId));QVERIFY(page);
    manager->setProperty("operation",1);QVERIFY(control(InputManager::Confirm));QTRY_VERIFY(manager->property("editModel").value<QObject *>());
    QVERIFY(control(InputManager::Back));QVERIFY(control(InputManager::Back));
    const auto second=page->addLibraryWidget("Keep watching","movies");QVERIFY(!second.isEmpty());QVERIFY(page->setSectionPresentation(second,{{"variant","library"},{"height",300},{"libraryView","continue"},{"libraryLayout","row"},{"libraryColumns",3}}));
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(HOME_QML)).dir().filePath("PageSurface.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"provider",QVariant::fromValue(&data)},{"availableApps",QVariantList{}},{"artwork",QVariantList{}},{"themes",QVariantList{}},{"width",1280},{"height",800}}));QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window;window.resize(1280,800);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"showPage",Q_ARG(QVariant,pageId),Q_ARG(QVariant,QString("original-card")),Q_ARG(QVariant,false)));
    QTRY_COMPARE(reads,2);auto *one=qobject_cast<KodiMedia *>(data.client(pageId+".library",page->categories()[0].toMap()["presentation"].toMap()));
    auto *two=qobject_cast<KodiMedia *>(data.client(pageId+"."+second,page->categories()[1].toMap()["presentation"].toMap()));QVERIFY(one);QVERIFY(two);QVERIFY(one!=two);QTRY_VERIFY(one->connected());QTRY_VERIFY(two->connected());
    one->selectIndex(3);QCOMPARE(two->selectedIndex(),0);
    auto style=page->categories()[0].toMap()["presentation"].toMap();style["panelOpacity"]=0.7;style["libraryCardStyle"]="orb";QVERIFY(page->setSectionPresentation("library",style));QTest::qWait(100);QCOMPARE(reads,2);QCOMPARE(one->selectedIndex(),3);
    auto command=[&](int value){return QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value));};
    QVERIFY(command(InputManager::NavigateRight));QCOMPARE(one->selectedIndex(),4);
    QVERIFY(command(InputManager::NavigateLeft));QCOMPARE(one->selectedIndex(),3);
    QVERIFY(command(InputManager::Confirm));QTRY_COMPARE(plays,1);QCOMPARE(played["movieid"].toInt(),4);QCOMPARE(played["resume"].toBool(),true);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"restoreOrigin",Q_ARG(QVariant,QString("tab-original-home"))));
    auto *content=root->findChild<QObject *>("pageContent");QVERIFY(content);QVERIFY(!content->property("recoveryFocused").toBool());
    player.playbackControl("stop");QTRY_VERIFY(!player.playbackActive());
    QVERIFY(command(InputManager::Back)); // Leave widget without leaving page.
    QVERIFY(root->property("open").toBool());
    const auto another=home.createPage("Another","blank");QVERIFY(!another.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(root.get(),"showPage",Q_ARG(QVariant,another),Q_ARG(QVariant,QString("link")),Q_ARG(QVariant,false)));
    QVERIFY(command(InputManager::Back));QCOMPARE(root->property("activeId").toString(),pageId);
    QSignalSpy closed(root.get(),SIGNAL(closed(QString)));QVERIFY(command(InputManager::Home));QVERIFY(!root->property("open").toBool());QCOMPARE(closed.last()[0].toString(),QString("original-card"));
    root.reset();QTest::qWait(150);const auto readsBeforeWidget=reads;
    QQmlComponent widget(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(HOME_QML)).dir().filePath("PageLibraryWidget.qml")));
    QVERIFY2(widget.isReady(),qPrintable(widget.errorString()));
    std::unique_ptr<QObject> frozen(widget.createWithInitialProperties({{"provider",QVariant::fromValue(&data)},{"widgetKey","frozen-widget"},{"style",QVariantMap{{"libraryKind","movies"},{"libraryView","all"}}},{"readOnly",true},{"width",800},{"height",300}}));QVERIFY(frozen);
    QTRY_COMPARE(reads,readsBeforeWidget+1);QTest::qWait(100);const auto beforeReads=reads,beforePlays=plays;
    frozen->setProperty("frozen",true);frozen->setProperty("style",QVariantMap{{"libraryKind","movies"},{"libraryView","recent"},{"surfaceColor","#123456"}});QTest::qWait(100);QCOMPARE(reads,beforeReads);
    QVERIFY(QMetaObject::invokeMethod(frozen.get(),"handleCommand",Q_ARG(QVariant,InputManager::Confirm)));QCOMPARE(plays,beforePlays);
    frozen->setProperty("frozen",false);QTRY_COMPARE(reads,beforeReads+1);
    data.invalidate("Profile closed");QVERIFY(!data.client("late",{}));QVERIFY(!data.artwork("movie",1));data.refresh(one);QTest::qWait(100);QCOMPARE(reads,beforeReads+1);
    KodiMedia startingPlayer("starting-profile",nullptr,dir.path());PageData startingData("starting-profile",&startingPlayer,nullptr,dir.path());
    rejectedReads=1;
    auto *early=qobject_cast<KodiMedia *>(startingData.client("early",{{"libraryKind","movies"},{"libraryView","watched"}}));QVERIFY(early);
    QTRY_VERIFY(!early->busy());QVERIFY(!early->connected());
    startingPlayer.refresh();QTRY_VERIFY(startingPlayer.connected());QTRY_VERIFY(early->connected());QCOMPARE(early->movies().size(),8);
    const QVariantMap historyStyle{{"libraryKind","history"},{"libraryView","watched"},{"libraryLimit",16},{"libraryColumns",5},{"libraryChrome",false},{"libraryLayout","row"}};
    QVERIFY(!startingData.client("invalid-history",{{"libraryKind","history"},{"libraryView","recent"}}));
    std::unique_ptr<QObject> history(widget.createWithInitialProperties({{"provider",QVariant::fromValue(&startingData)},{"widgetKey","history-widget"},{"style",historyStyle},{"width",800},{"height",300}}));QVERIFY(history);
    auto *historyClient=qobject_cast<KodiMedia *>(startingData.client("history-widget",historyStyle));QVERIFY(historyClient);
    QTRY_VERIFY(historyClient->connected());QCOMPARE(historyClient->items().size(),16);
    const auto historyActions=history->property("actions");
    QCOMPARE((historyActions.metaType().id()==qMetaTypeId<QJSValue>()?historyActions.value<QJSValue>().toVariant():historyActions).toStringList(),QStringList{"Refresh"});
    historyClient->selectIndex(12);auto reduced=historyStyle;reduced["libraryLimit"]=2;history->setProperty("style",reduced);
    QTRY_COMPARE(historyClient->selectedIndex(),1);QCOMPARE(historyClient->items().size(),16);
    auto *otherHistory=qobject_cast<KodiMedia *>(startingData.client("independent-history",historyStyle));QVERIFY(otherHistory);QVERIFY(otherHistory!=historyClient);QTRY_VERIFY(otherHistory->connected());QCOMPARE(otherHistory->selectedIndex(),0);
    startingData.invalidate("Profile closed");QVERIFY(!startingData.client("history-widget",historyStyle));QVERIFY(historyClient->items().isEmpty());
    QVERIFY(!data.client("still-closed",{})); // Bridge readiness cannot revive a torn-down profile.
    QVERIFY(home.addWidget("text"));
    QCOMPARE(home.items().last().toMap()["presentation"].toMap()["subtitle"].toString(),QString("Your message"));
    QQmlComponent legacy(&engine,QUrl::fromLocalFile(QStringLiteral(HOME_QML)));
    std::unique_ptr<QObject> legacyHome(legacy.createWithInitialProperties({{"items",home.items()},{"sections",home.sections()},{"brandVisible",false},{"backgroundStart",QColor("#101014")},{"backgroundEnd",QColor("#101014")},{"tileRoundness",0.18},{"internetAvailable",true},{"localNetworkAvailable",true},{"hasContextualSources",false},{"width",1280},{"height",800}}));
    QVERIFY2(legacyHome,qPrintable(legacy.errorString()));
    qobject_cast<QQuickItem *>(legacyHome.get())->setParentItem(window.contentItem());
    // Repeater delegates belong to the visual tree, not necessarily QObject's ownership tree.
    std::function<QQuickItem *(QQuickItem *)> findText=[&](QQuickItem *item)->QQuickItem *{
        if(item->objectName()=="legacyTextWidget-text")return item;
        for(auto *child:item->childItems())if(auto *found=findText(child))return found;
        return nullptr;
    };
    QTRY_VERIFY(findText(qobject_cast<QQuickItem *>(legacyHome.get())));
    QCOMPARE(findText(qobject_cast<QQuickItem *>(legacyHome.get()))->property("kind").toString(),QString("text"));
    QVERIFY(!home.addLibraryWidget("Home movies","movies").isEmpty());QVERIFY(home.composition()["enabled"].toBool());
}
void HomeNavigationTests::skinsAndDesignAreReachableFromSettings()
{
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    InputManager input; QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("homeLayout",&home);
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("LegacyCustomizeHomeView.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items",home.items()},{"availableApps",QVariantList{}},{"artwork",QVariantList{}},{"themes",QVariantList{}},{"width",1280},{"height",800}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,800); qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem()); window.show();
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value)); };
    auto *skins = root->findChild<QObject *>("skinLibrary"); QVERIFY(skins);
    auto *editor = root->findChild<QObject *>("homeDesignEditor"); QVERIFY(editor);
    root->setProperty("actionIndex",23); QVERIFY(command(InputManager::Confirm)); QVERIFY(root->property("editing").toBool());
    QVERIFY(command(InputManager::NavigateDown)); QVERIFY(command(InputManager::NavigateDown)); // Pastel sky
    QVERIFY(command(InputManager::Confirm)); QVERIFY(skins->property("confirming").toBool());
    QVERIFY(home.composition().isEmpty()); // Confirmation does not mutate Home.
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(home.composition()["headerMode"].toString(),QString("identity"));
    QVERIFY(command(InputManager::NavigateRight)); QVERIFY(command(InputManager::Confirm));
    auto *keyboard = skins->findChild<QObject *>("skinNameKeyboard"); QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("My clouds")))); QCOMPARE(home.skins().size(),4);
    const auto id = home.skins().last().toMap()["id"].toString();
    QVERIFY(command(InputManager::Back)); QVERIFY(!root->property("editing").toBool());
    root->setProperty("actionIndex",24); QVERIFY(command(InputManager::Confirm));
    auto listValue = [](const QVariant &v) { return v.metaType() == QMetaType::fromType<QJSValue>() ? v.value<QJSValue>().toVariant().toList() : v.toList(); };
    auto select = [&](const QString &key) {
        const auto fields = listValue(editor->property("fields"));
        for (int i=0; i<fields.size(); ++i) if (fields[i].toMap()["key"] == key) { editor->setProperty("selected",i+2); return true; }
        return false;
    };
    QVERIFY(select("gap")); QVERIFY(command(InputManager::NavigateRight)); QCOMPARE(home.composition()["gap"].toInt(),10);
    editor->setProperty("scope",1); editor->setProperty("target",1); // favorites
    QVERIFY(select("blur")); QVERIFY(command(InputManager::NavigateLeft));
    QCOMPARE(home.categories()[1].toMap()["presentation"].toMap()["blur"].toDouble(),0.65);
    editor->setProperty("scope",2); editor->setProperty("target",0);
    QVERIFY(select("diameter")); QVERIFY(command(InputManager::NavigateRight));
    QCOMPARE(home.components()[0].toMap()["presentation"].toMap()["diameter"].toDouble(),112.0);
    editor->setProperty("target",10); QVERIFY(select("diameter")); QVERIFY(command(InputManager::NavigateRight));
    QCOMPARE(home.components()[10].toMap()["presentation"].toMap()["diameter"].toDouble(),72.0); // Inherits Favorites' 70, not the generic 110.
    // Save a changed design, switch away, and recover it using only the library's remote route.
    const auto customized = home.saveSkin("Changed clouds"); QVERIFY(!customized.isEmpty());
    QVERIFY(command(InputManager::Back)); QVERIFY(home.applySkin("builtin-default"));
    root->setProperty("actionIndex",23); QVERIFY(command(InputManager::Confirm));
    const auto entries = home.skins();
    for (int i=0; i<entries.size(); ++i) if (entries[i].toMap()["id"] == customized) skins->setProperty("selected",i);
    QVERIFY(command(InputManager::Confirm)); QVERIFY(command(InputManager::Confirm));
    QCOMPARE(home.composition()["gap"].toInt(),10);
    QVERIFY(command(InputManager::Back)); QVERIFY(home.applySkin(id)); QCOMPARE(home.composition()["gap"].toInt(),9);
    QVERIFY(home.applySkin("builtin-lounge"));
    // The legacy editor retains coverage of both standing reference documents.
    for (const auto &fixture : {QStringLiteral(REFERENCE_DIR), QStringLiteral(REFERENCE_DIR) + "/../home-pastel-reference"}) {
        QVERIFY(home.load(fixture + "/recreation-config.json",dir.filePath(QFileInfo(fixture).fileName() + ".json")));
        for (int scope : {1,2}) {
            editor->setProperty("scope",scope);
            QSet<QString> keys;
            for (const auto &field : listValue(editor->property("fields"))) keys.insert(field.toMap()["key"].toString().section('.',0,0));
            const auto entries = scope == 1 ? home.categories() : home.components();
            for (const auto &entry : entries) {
                const auto style = entry.toMap()["presentation"].toMap();
                // Placed v2 fixture controls belong to Page Studio. The legacy
                // inspector cannot edit grid/group/library composition data.
                if(home.composition().contains("grid")) {QVERIFY(ComponentSchema::validPresentation(style,true,entry.toMap()["componentType"]=="container"));continue;}
                for (auto it=style.cbegin(); it!=style.cend(); ++it) QVERIFY2(keys.contains(it.key()),qPrintable(it.key()));
            }
        }
    }
}
void HomeNavigationTests::retainsExactOriginAndRecoversWithoutHomeButton()
{
    QQmlEngine engine;
    InputManager input;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("isoldeVersion", "0.0.1.2");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(HOME_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    auto tile = [](const QString &id) -> QVariantMap { return {{"id", id}, {"kind", "action"}, {"name", id},
        {"symbol", "A"}, {"accent", "#556677"}, {"section", "Test"}, {"size", "standard"},
        {"action", QVariantMap{{"type", "launch-app"}, {"appId", "shared-handler"}}}}; };
    QVariantList items{tile("tile-a"), tile("tile-b"), tile("tile-c")};
    const auto sections = [](const QVariantList &values) -> QVariantList { return {QVariantMap{{"id", "test"}, {"name", "Test"}, {"items", values}}}; };
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items", items}, {"sections", sections(items)},
        {"brandVisible", false}, {"backgroundStart", QColor("#111111")}, {"backgroundEnd", QColor("#222222")},
        {"tileRoundness", 0.18}, {"internetAvailable", true}, {"localNetworkAvailable", true}, {"hasContextualSources", false},
        {"width", 1280}, {"height", 720}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280, 720);
    qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());
    window.show();
    QSignalSpy activated(root.get(), SIGNAL(actionActivated(QVariant,QString)));
    QSignalSpy settings(root.get(), SIGNAL(settingsRequested(int)));
    QVERIFY(activated.isValid()); QVERIFY(settings.isValid());
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value)); };
    auto restore = [&](const QString &id) { return QMetaObject::invokeMethod(root.get(), "restoreFocus", Q_ARG(QVariant, id)); };
    root->setProperty("focusedIndex", 1);
    QVERIFY(command(InputManager::Confirm));
    QCOMPARE(activated.last()[1].toString(), QString("tile-b")); // same handler, different tile
    items.move(1, 0);
    root->setProperty("sections", sections(items)); root->setProperty("items", items);
    QCOMPARE(root->property("focusedItemId").toString(), QString("tile-b"));
    QCOMPARE(root->property("focusedIndex").toInt(), 0);
    root->setProperty("focusedIndex", 2);
    QVERIFY(restore("tile-b")); QCOMPARE(root->property("focusedIndex").toInt(), 0);
    items.removeFirst();
    root->setProperty("sections", sections(items)); root->setProperty("items", items);
    QVERIFY(restore("tile-b")); // deleted origin falls back to surviving item
    QCOMPARE(root->property("focusedItemId").toString(), QString("tile-a"));
    QVERIFY(command(InputManager::NavigateUp));
    QVERIFY(root->property("recoveryFocused").toBool());
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(settings.last()[0].toInt(), -1);
    QVERIFY(command(InputManager::NavigateDown));
    QVERIFY(!root->property("recoveryFocused").toBool());
    auto missing = items[0].toMap(); missing["unavailableReason"] = "App missing · repair source"; items[0] = missing;
    root->setProperty("items", items); root->setProperty("sections", sections(items));
    const auto launches = activated.size();
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(settings.last()[0].toInt(), 1);
    QCOMPARE(activated.size(), launches);
    root->setProperty("items", QVariantList{}); root->setProperty("sections", QVariantList{});
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(settings.last()[0].toInt(), -1);
    QVERIFY(command(InputManager::NavigateRight));
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(settings.last()[0].toInt(), 1);
    root->setProperty("items", items); root->setProperty("sections", sections(items));
    QVERIFY(command(InputManager::NavigateDown));
    QVERIFY(!root->property("recoveryFocused").toBool());
    // A wide tile must never exceed the two-column renderer on a narrow display.
    root->setProperty("width", 800);
    QTest::qWait(50);
    auto *content = root->findChild<QQuickItem *>("homeContent");
    QVERIFY(content);
    for (const bool brand : {false, true}) for (const bool sources : {false, true}) {
        root->setProperty("brandVisible", brand);
        root->setProperty("hasContextualSources", sources);
        QTest::qWait(10);
        QVERIFY(content->height() > 0);
        QVERIFY(content->y() + content->height() <= content->parentItem()->height() + 1);
    }
}
void HomeNavigationTests::repairsSourceWithRemoteAndCanCancel()
{
    QTemporaryDir dir;
    QFile defaults(dir.filePath("default.json"));
    QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"backgroundStart":"#111111","backgroundEnd":"#222222"},"items":[{"id":"personal","kind":"app","appId":"missing","size":"wide","opacity":0.4}]})");
    defaults.close();
    AppRegistry registry;
    QVERIFY(registry.loadFromData(R"([{"id":"replacement","name":"Replacement","type":"mock","section":"Apps","accent":"#556677"}])"));
    HomeLayout home(&registry); QVERIFY(home.load(defaults.fileName(), dir.filePath("user.json")));
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("homeLayout", &home);
    engine.rootContext()->setContextProperty("isoldeVersion", "test");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("LegacyCustomizeHomeView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items", home.items()}, {"availableApps", registry.apps()},
        {"artwork", QVariantList{}}, {"themes", QVariantList{}}, {"width", 1280}, {"height", 720}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    root->setProperty("actionIndex", 20);
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value)); };
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(root->property("sourceRepairEditing").toBool());
    QVERIFY(root->property("editing").toBool());
    QVERIFY(command(InputManager::Back));
    QVERIFY(!root->property("sourceRepairEditing").toBool());
    QCOMPARE(home.items()[0].toMap()["appId"].toString(), QString("missing"));
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(!root->property("sourceRepairEditing").toBool());
    QCOMPARE(home.items()[0].toMap()["appId"].toString(), QString("replacement"));
    QCOMPARE(home.items()[0].toMap()["id"].toString(), QString("personal"));
    QCOMPARE(home.items()[0].toMap()["size"].toString(), QString("wide"));
    QCOMPARE(home.items()[0].toMap()["opacity"].toDouble(), 0.4);
}
void HomeNavigationTests::universalLocalSearchRemoteFlow()
{
    QTemporaryDir directory;
    QLocalServer server;
    QVERIFY(server.listen(directory.filePath("rpc.sock")));
    connect(&server, &QLocalServer::newConnection, &server, [&server] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [socket] {
                const auto bytes = socket->property("input").toByteArray() + socket->readAll();
                socket->setProperty("input", bytes);
                if (!bytes.endsWith('\n')) return;
                const auto request = QJsonDocument::fromJson(bytes).object();
                const auto method = request.value("method").toString();
                QJsonObject result;
                if (method == "JSONRPC.Version") result = {{"version", QJsonObject{{"major", 13}}}};
                else {
                    const auto params = request.value("params").toObject();
                    QCOMPARE(params.value("filter").toObject().value("field").toString(), QString("title"));
                    QCOMPARE(params.value("filter").toObject().value("operator").toString(), QString("contains"));
                    const auto key = method == "VideoLibrary.GetTVShows" ? QString("tvshows")
                        : method == "VideoLibrary.GetEpisodes" ? QString("episodes") : QString("movies");
                    const auto idKey = method == "VideoLibrary.GetTVShows" ? QString("tvshowid")
                        : method == "VideoLibrary.GetEpisodes" ? QString("episodeid") : QString("movieid");
                    QJsonObject item{{idKey, method == "VideoLibrary.GetTVShows" ? 2 : 5}, {"title", "Pilot " + key}};
                    if (method == "VideoLibrary.GetEpisodes") {
                        item.insert("showtitle", "Pilot Show");
                        item.insert("season", 1);
                        item.insert("episode", 2);
                    }
                    result = {{key, QJsonArray{item}}, {"limits", QJsonObject{{"start", 0}, {"end", 1}, {"total", 1}}}};
                }
                socket->write(QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request.value("id")}, {"result", result}})
                    .toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            });
        }
    });

    KodiMedia movieSearch("search-ui", nullptr, directory.path());
    KodiMedia showSearch("search-ui", nullptr, directory.path());
    KodiMedia episodeSearch("search-ui", nullptr, directory.path());
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(COMMAND_CENTER_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({
        {"apps", QVariantList{}}, {"packages", QVariantList{}}, {"movieSearchClient", QVariant::fromValue(&movieSearch)},
        {"showSearchClient", QVariant::fromValue(&showSearch)}, {"episodeSearchClient", QVariant::fromValue(&episodeSearch)},
        {"mediaCollections", QVariantList{}}, {"width", 1280}, {"height", 720}, {"open", true}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    QSignalSpy mediaRequested(root.get(), SIGNAL(mediaRequested(QString,int,QString,QString,int)));
    root->setProperty("query", "Pilot");
    QTRY_VERIFY(movieSearch.connected());
    QTRY_VERIFY(showSearch.connected());
    QTRY_VERIFY(episodeSearch.connected());
    const auto results = root->property("results").toList();
    bool sawMovie = false, sawShow = false, sawEpisode = false;
    int showResult = -1;
    for (qsizetype index = 0; index < results.size(); ++index) {
        const auto result = results[index].toMap();
        if (result.value("kind") != "media") continue;
        const auto type = result.value("mediaType").toString();
        sawMovie |= type == "movie";
        sawShow |= type == "show";
        sawEpisode |= type == "episode";
        if (type == "show") showResult = static_cast<int>(index);
    }
    QVERIFY(sawMovie); QVERIFY(sawShow); QVERIFY(sawEpisode); QVERIFY(showResult >= 0);
    root->setProperty("selectedIndex", showResult);
    QVERIFY(QMetaObject::invokeMethod(root.get(), "activateSelected"));
    QCOMPARE(mediaRequested.size(), 1);
    QCOMPARE(mediaRequested[0][0].toString(), QString("show"));
    QCOMPARE(mediaRequested[0][1].toInt(), 2);
    QCOMPARE(mediaRequested[0][2].toString(), QString("Pilot"));
}
void HomeNavigationTests::appSuppliedIconsUseManagedArtwork()
{
    QTemporaryDir dir;const auto previousData=qgetenv("XDG_DATA_HOME"),previousCache=qgetenv("XDG_CACHE_HOME");
    const auto restore=qScopeGuard([&]{qputenv("XDG_DATA_HOME",previousData);qputenv("XDG_CACHE_HOME",previousCache);});
    qputenv("XDG_DATA_HOME",dir.filePath("data").toUtf8());qputenv("XDG_CACHE_HOME",dir.filePath("cache").toUtf8());
    QVERIFY(QDir().mkpath(dir.filePath("data/applications")));QVERIFY(QDir().mkpath(dir.filePath("data/icons/hicolor/128x128/apps")));
    QImage image(128,128,QImage::Format_ARGB32);image.fill(QColor("#35a477"));
    const auto source=dir.filePath("data/icons/hicolor/128x128/apps/provided-icon.png");QVERIFY(image.save(source));
    QFile desktop(dir.filePath("data/applications/provided-app.desktop"));QVERIFY(desktop.open(QIODevice::WriteOnly));
    desktop.write("[Desktop Entry]\nType=Application\nName=Provided app\nIcon=provided-icon\nExec=/nonexistent/never-run-for-artwork\n");desktop.close();
    AppDefinition app;app.id="provided-app";app.name="Provided app";app.type="desktop";app.symbol="P";app.launchConfiguration={{"desktopId","provided-app"}};
    AppRegistry registry;QVERIFY(registry.addApp(app));const auto supplied=registry.apps().first().toMap()["logoSource"].toString();
    QVERIFY(!supplied.isEmpty());const auto managed=QUrl(supplied).toLocalFile();QVERIFY(managed.startsWith(dir.filePath("cache/")));QVERIFY(managed!=source);
    QVERIFY(QRegularExpression("^[0-9a-f]{64}\\.png$").match(QFileInfo(managed).fileName()).hasMatch());
    QImage cached(managed);QVERIFY(!cached.isNull());QVERIFY(cached.width()<=256);QVERIFY(cached.height()<=256);QCOMPARE(cached.pixelColor(cached.width()/2,cached.height()/2),QColor("#35a477"));
    AppRegistry again;QVERIFY(again.addApp(app));QCOMPARE(again.apps().first().toMap()["logoSource"].toString(),supplied);
    QFile wrapper(dir.filePath("data/applications/provided-wrapper.desktop"));QVERIFY(wrapper.open(QIODevice::WriteOnly));wrapper.write("[Desktop Entry]\nName=Isolde launch wrapper\n");wrapper.close();
    auto wrapped=app;wrapped.launchConfiguration["desktopId"]="provided-wrapper";AppRegistry wrappedRegistry;QVERIFY(wrappedRegistry.addApp(wrapped));QCOMPARE(wrappedRegistry.apps().first().toMap()["logoSource"].toString(),supplied);
    HomeLayout home(&registry);QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("layout.json")));home.addAppTile(app.id);
    bool bound=false;for(const auto &entry:home.items()){const auto item=entry.toMap();if(item["appId"]==app.id){QCOMPARE(item["logoSource"].toString(),supplied);bound=true;}}QVERIFY(bound);
    auto invalid=app;invalid.id="unsafe-icon";invalid.launchConfiguration["icon"]="../outside";QVERIFY(!registry.addApp(invalid));
    desktop.open(QIODevice::WriteOnly|QIODevice::Truncate);desktop.write("[Desktop Entry]\nIcon=https://example.invalid/icon.png\n");desktop.close();
    auto remote=app;remote.id="remote-icon";QVERIFY(registry.addApp(remote));QVERIFY(registry.apps().last().toMap()["logoSource"].toString().isEmpty());
}
void HomeNavigationTests::searchPreferenceProfileIsolationAndWriteFailure()
{
    QTemporaryDir dir;const auto path=dir.filePath("preferences.ini");
    const QString one="11111111-1111-4111-8111-111111111111",two="22222222-2222-4222-8222-222222222222";
    SearchSettings a(nullptr,path,one),b(nullptr,path,two),household(nullptr,path,"default");
    QVERIFY(!a.webEnabled());QVERIFY(!b.webEnabled());QVERIFY(!household.webEnabled());
    a.toggleWebEnabled();QVERIFY(a.webEnabled());QVERIFY(a.errorString().isEmpty());
    SearchSettings restored(nullptr,path,one);QVERIFY(restored.webEnabled());QVERIFY(!b.webEnabled());
    b.toggleWebEnabled();a.toggleWebEnabled();QVERIFY(b.webEnabled());QVERIFY(!a.webEnabled());
    SearchSettings restoredA(nullptr,path,one),restoredB(nullptr,path,two);
    QVERIFY(!restoredA.webEnabled());QVERIFY(restoredB.webEnabled());QVERIFY(!household.webEnabled());
    QFile blocked(dir.filePath("blocked"));QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.write("retain");blocked.close();
    SearchSettings failed(nullptr,dir.filePath("blocked/preferences.ini"),one);
    failed.toggleWebEnabled();QVERIFY(!failed.webEnabled());QVERIFY(!failed.errorString().isEmpty());
    QVERIFY(blocked.open(QIODevice::ReadOnly));QCOMPARE(blocked.readAll(),QByteArray("retain"));
    SearchSettings invalid(nullptr,path,"../other");invalid.toggleWebEnabled();QVERIFY(!invalid.webEnabled());QVERIFY(!invalid.errorString().isEmpty());
}
void HomeNavigationTests::searchHeaderSwitchRemoteFocusAndStableResults()
{
    QTemporaryDir dir;SearchSettings preferences(nullptr,dir.filePath("search.ini"),"default");InputManager input;QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(COMMAND_CENTER_QML)));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    const QVariantList apps{QVariantMap{{"id","local"},{"name","Local app"},{"section","Apps"},{"presentationMode","native-tv"},{"networkRequirement","none"},{"accent","#556677"}}};
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"apps",apps},{"packages",QVariantList{}},{"searchSettings",QVariant::fromValue(&preferences)},{"width",1280},{"height",720},{"open",true}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    auto command=[&](int value){QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value)));};
    auto *toggle=root->findChild<QObject *>("searchWebToggle");QVERIFY(toggle);
    auto list=[](const QVariant &v){return (v.metaType().id()==qMetaTypeId<QJSValue>()?v.value<QJSValue>().toVariant():v).toList();};
    const auto original=list(root->property("results"));QVERIFY(!original.isEmpty());
    for(const auto &entry:original)QVERIFY(entry.toMap()["kind"]!="searchToggle");
    QCOMPARE(root->property("focusArea").toString(),QString("query"));
    command(InputManager::NavigateRight);QCOMPARE(root->property("focusArea").toString(),QString("web"));QVERIFY(!preferences.webEnabled());
    command(InputManager::Confirm);QVERIFY(preferences.webEnabled());QTRY_VERIFY(toggle->property("checked").toBool());
    QCOMPARE(list(root->property("results")),original);QCOMPARE(root->property("selectedIndex").toInt(),0);
    command(InputManager::NavigateDown);QCOMPARE(root->property("focusArea").toString(),QString("results"));
    command(InputManager::NavigateUp);QCOMPARE(root->property("focusArea").toString(),QString("web"));
    root->setProperty("apps",QVariantList{apps[0],QVariantMap{{"id","late"},{"name","Late app"},{"section","Apps"},{"presentationMode","native-tv"},{"networkRequirement","none"},{"accent","#556677"}}});
    QCOMPARE(root->property("focusArea").toString(),QString("web"));QCOMPARE(root->property("selectedIndex").toInt(),0);
    command(InputManager::NavigateLeft);command(InputManager::Confirm);QVERIFY(root->property("keyboardOpen").toBool());
    command(InputManager::Back);QVERIFY(!root->property("keyboardOpen").toBool());QVERIFY(root->property("open").toBool());
    root->setProperty("query","zzzz-no-matches");QTRY_COMPARE(list(root->property("results")).size(),0);
    command(InputManager::NavigateRight);command(InputManager::NavigateDown);command(InputManager::NavigateUp);
    QCOMPARE(root->property("focusArea").toString(),QString("web"));
    command(InputManager::Confirm);QVERIFY(!preferences.webEnabled());QCOMPARE(root->property("query").toString(),QString("zzzz-no-matches"));
    auto *toggleItem=qobject_cast<QQuickItem *>(toggle);QVERIFY(toggleItem);QSignalSpy preferenceChanges(&preferences,&SearchSettings::changed);
    QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,toggleItem->mapToScene(QPointF(toggleItem->width()/2,toggleItem->height()/2)).toPoint());
    QTRY_VERIFY(preferences.webEnabled());QCOMPARE(preferenceChanges.count(),1);
    command(InputManager::Confirm);QVERIFY(!preferences.webEnabled());QCOMPARE(preferenceChanges.count(),2);
    for(const auto &theme:QVariantList{QVariantMap{{"textColor","#f5f1f3"},{"mutedColor","#aaa5af"},{"surfaceColor","#28262f"},{"focusColor","#c994a5"},{"selectedColor","#574052"}},QVariantMap{{"textColor","#514b67"},{"mutedColor","#625b73"},{"surfaceColor","#f5e7f4"},{"focusColor","#9b4568"},{"selectedColor","#edd7ef"}}}) {
        root->setProperty("appearance",theme);QTest::qWait(50);
        const auto artifact=qEnvironmentVariable("ISOLDE_TEST_ARTIFACTS");
        if(!artifact.isEmpty())QVERIFY(window.grabWindow().save(artifact+"/studio-test-search-header-"+(theme.toMap()["textColor"]=="#514b67"?"pastel":"dark")+".png"));
    }
    command(InputManager::Confirm);QVERIFY(preferences.webEnabled());
    auto *status=root->findChild<QObject *>("searchWebStatus");QVERIFY(status);QVERIFY(status->property("text").toString().contains("No web provider configured"));
    QFile blocked(dir.filePath("blocked"));QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.close();
    SearchSettings failed(nullptr,dir.filePath("blocked/preferences.ini"),"default");root->setProperty("searchSettings",QVariant::fromValue(&failed));
    command(InputManager::Confirm);QVERIFY(!failed.webEnabled());QVERIFY(!toggle->property("checked").toBool());
    QVERIFY(status->property("text").toString().contains("Could not save"));
    QCOMPARE(root->property("focusArea").toString(),QString("web"));QCOMPARE(root->property("query").toString(),QString("zzzz-no-matches"));
    QSignalSpy closed(root.get(),SIGNAL(closeRequested()));command(InputManager::Back);QCOMPARE(closed.count(),1);
}
void HomeNavigationTests::pageScopedSourcesAppearInSearch()
{
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(COMMAND_CENTER_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantList pageSources{QVariantMap{{"id", "library-1"}, {"name", "Keep watching"}, {"kind", "movies"}, {"view", "continue"}, {"value", ""}}};
    std::unique_ptr<QObject> root(component.createWithInitialProperties({
        {"apps", QVariantList{}}, {"packages", QVariantList{}},
        {"pageId", "page-abc"}, {"pageSources", pageSources},
        {"width", 1280}, {"height", 720}, {"open", true}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    QSignalSpy pageSourceRequested(root.get(), SIGNAL(pageSourceRequested(QString,QString,QString,QString,QString)));
    root->setProperty("query", "keep");
    const auto results = root->property("results").toList();
    int pageIndex = -1;
    for (qsizetype index = 0; index < results.size(); ++index) {
        const auto result = results[index].toMap();
        if (result.value("kind") == "pageSource" && result.value("pageSourceId").toString() == "library-1")
            pageIndex = static_cast<int>(index);
    }
    QVERIFY(pageIndex >= 0);
    root->setProperty("selectedIndex", pageIndex);
    QVERIFY(QMetaObject::invokeMethod(root.get(), "activateSelected"));
    QCOMPARE(pageSourceRequested.size(), 1);
    QCOMPARE(pageSourceRequested[0][0].toString(), QString("page-abc"));
    QCOMPARE(pageSourceRequested[0][1].toString(), QString("library-1"));
    QCOMPARE(pageSourceRequested[0][2].toString(), QString("movies"));
    QCOMPARE(pageSourceRequested[0][3].toString(), QString("continue"));
}
void HomeNavigationTests::kodiSettingsFollowUserTheme()
{
    QTemporaryDir directory;
    InputManager input;
    AppRegistry registry;
    SessionManager sessions(&registry);
    KodiService service("theme-settings-test");
    service.setEnabled(false);
    KodiMedia media("theme-settings-test", nullptr, directory.path());
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("kodiService", &service);
    engine.rootContext()->setContextProperty("kodiMedia", &media);
    engine.rootContext()->setContextProperty("sessionManager", &sessions);
    SearchSettings searchPreferences; KodiImporter importer("theme-test");
    engine.rootContext()->setContextProperty("searchPreferences", &searchPreferences);
    engine.rootContext()->setContextProperty("kodiImporter", &importer);
    engine.rootContext()->setContextProperty("profileManager", QVariantMap{{"activeName", "Theme test"}});
    const QVariantMap appearance{{"focusColor", "#E65D8F"}, {"surfaceColor", "#334455"},
        {"selectedColor", "#123456"}, {"textColor", "#E0E0E0"}, {"mutedColor", "#AAAAAA"},
        {"backgroundEnd", "#010203"}, {"tileRoundness", 0.8}};
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(KODI_SETTINGS_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"appearance", appearance}, {"width", 1280}, {"height", 720}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280, 720);
    qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());
    window.show();
    std::function<QQuickItem *(QQuickItem *, const QString &)> findItem = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (!parent) return nullptr;
        if (parent->objectName() == name) return parent;
        for (auto *child : parent->childItems()) if (auto *found = findItem(child, name)) return found;
        return nullptr;
    };
    auto *actionRow = findItem(qobject_cast<QQuickItem *>(root.get()), "mediaLibraryActionRow");
    QTRY_VERIFY(actionRow);
    QCOMPARE(actionRow->property("color").value<QColor>(), QColor("#123456"));

    root->setProperty("editingFolders", true);
    auto *folderPage = findItem(qobject_cast<QQuickItem *>(root.get()), "mediaFolderAccessPage");
    QTRY_VERIFY(folderPage);
    auto *folderRow = findItem(folderPage, "mediaFolderActionRow");
    QTRY_VERIFY(folderRow);
    QCOMPARE(folderRow->property("color").value<QColor>(), QColor("#123456"));
    QCOMPARE(root->property("appearance").toMap().value("backgroundEnd").toString(), QString("#010203"));
}
void HomeNavigationTests::appearancePreviewCancelAndApply()
{
    QTemporaryDir dir;
    QFile defaults(dir.filePath("default.json")); QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"backgroundStart":"#19191e","backgroundEnd":"#101014"},"items":[]})"); defaults.close();
    AppRegistry registry; HomeLayout home(&registry);
    QVERIFY(home.load(defaults.fileName(), dir.filePath("personal.json")));
    InputManager input; QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("homeLayout", &home);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("LegacyCustomizeHomeView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items", QVariantList{}}, {"availableApps", QVariantList{}},
        {"artwork", QVariantList{}}, {"themes", QVariantList{}}, {"width", 1280}, {"height", 720}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    root->setProperty("actionIndex", 21);
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value)); };
    auto *editor = root->findChild<QObject *>("appearanceEditor"); QVERIFY(editor);
    auto mapValue = [](const QVariant &value) { return value.metaType() == QMetaType::fromType<QJSValue>()
        ? value.value<QJSValue>().toVariant().toMap() : value.toMap(); };
    const auto original = home.appearance();
    QVERIFY(command(InputManager::Confirm)); QVERIFY(root->property("editing").toBool());
    editor->setProperty("selectedIndex", 7);
    QVERIFY(command(InputManager::NavigateRight));
    QCOMPARE(mapValue(editor->property("draft"))["typographyScale"].toDouble(), 1.05);
    QCOMPARE(home.appearance(), original); QVERIFY(!QFile::exists(dir.filePath("personal.json")));
    QVERIFY(command(InputManager::Back)); QVERIFY(!root->property("editing").toBool());
    QCOMPARE(home.appearance(), original);
    QVERIFY(command(InputManager::Confirm));
    // Enter an arbitrary accent through the same keyboard acceptance path as remote input.
    QVERIFY(command(InputManager::Confirm)); QVERIFY(editor->property("typing").toBool());
    auto *keyboard = editor->findChild<QObject *>("appearanceKeyboard"); QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(keyboard, "accepted", Q_ARG(QString, QString("#58A6BC"))));
    QVERIFY(!editor->property("typing").toBool());
    auto *preview = editor->findChild<QObject *>("appearancePreviewTile"); QVERIFY(preview);
    QCOMPARE(mapValue(preview->property("appearance"))["focusColor"].toString(), QString("#58A6BC"));
    editor->setProperty("selectedIndex", 11); QVERIFY(command(InputManager::Confirm));
    editor->setProperty("selectedIndex", 12); QVERIFY(command(InputManager::Confirm));
    QVERIFY(!root->property("editing").toBool());
    QCOMPARE(home.appearance()["focusColor"].toString(), QString("#58a6bc"));
    QCOMPARE(home.appearance()["motionEnabled"].toBool(), false);
}
void HomeNavigationTests::composedReferenceNavigationAndGeometry()
{
    QTemporaryDir dir; AppRegistry registry;
    QVERIFY(registry.loadFromData(R"([{"id":"movies","name":"Movies","type":"mock","section":"Library","accent":"#556677"},{"id":"shows","name":"Shows","type":"mock","section":"Library","accent":"#556677"}])"));
    HomeLayout home(&registry);
    QVERIFY(home.load(QStringLiteral(REFERENCE_DIR) + "/recreation-config.json",dir.filePath("personal.json")));
    QVERIFY(QDir().mkpath(dir.filePath("artwork")));
    QVERIFY(QFile::copy(QStringLiteral(REFERENCE_DIR) + "/reference.jpg",dir.filePath("artwork/reference.jpg")));
    InputManager input; QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("isoldeVersion","0.0.1.2");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(HOME_QML)));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items",home.components()},{"sections",home.sections()},
        {"composition",home.composition()},{"appearance",home.appearance()},{"brandVisible",false},
        {"backgroundStart",QColor(home.backgroundStart())},{"backgroundEnd",QColor(home.backgroundEnd())},{"tileRoundness",0.18},
        {"internetAvailable",true},{"localNetworkAvailable",true},{"hasContextualSources",false},{"width",1280},{"height",800}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,800); qobject_cast<QQuickItem*>(root.get())->setParentItem(window.contentItem()); window.show();
    QTest::qWait(80);
    auto *composed = root->findChild<QObject*>("composedHome"); QVERIFY(composed);
    auto *content = root->findChild<QQuickItem*>("composedContent"); QVERIFY(content);
    QVERIFY(content->property("contentHeight").toReal() <= content->height()+2);
    QSignalSpy activated(root.get(),SIGNAL(actionActivated(QVariant,QString))); QVERIFY(activated.isValid());
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value)); };
    QVERIFY(command(InputManager::NavigateRight)); QCOMPARE(root->property("focusedItemId").toString(),QString("once"));
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(activated.last()[1].toString(),QString("once"));
    QVERIFY(command(InputManager::NavigateRight)); QCOMPARE(root->property("focusedItemId").toString(),QString("glee"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("hdmi4")); // nearest visual center includes the dashboard gutter
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("guide"));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"restoreFocus",Q_ARG(QVariant,QString("once"))));
    QCOMPARE(root->property("focusedItemId").toString(),QString("once"));
    QVERIFY(command(InputManager::NavigateUp)); QVERIFY(composed->property("navigationFocused").toBool());
    for (int i=0;i<4;i++) QVERIFY(command(InputManager::NavigateRight));
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(composed->property("activeTabId").toString(),QString("games"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("switch"));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"restoreFocus",Q_ARG(QVariant,QString("once"))));
    QCOMPARE(composed->property("activeTabId").toString(),QString("home"));
    QVERIFY(command(InputManager::NavigateUp)); QVERIFY(command(InputManager::NavigateUp));
    QVERIFY(root->property("recoveryFocused").toBool());
}
void HomeNavigationTests::pastelReferenceNavigationAndGeometry()
{
    const auto fixture = QStringLiteral(REFERENCE_DIR) + "/../home-pastel-reference";
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
    QVERIFY(home.load(fixture+"/recreation-config.json",dir.filePath("personal.json")));
    QVERIFY(QDir().mkpath(dir.filePath("artwork")));
    QVERIFY(QFile::copy(fixture+"/reference.jpg",dir.filePath("artwork/reference.jpg")));
    QVERIFY(QFile::copy(fixture+"/wallpaper.png",dir.filePath("artwork/wallpaper.png")));
    InputManager input; QQmlEngine engine; engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("isoldeVersion","0.0.1.2");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(HOME_QML)));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items",home.components()},{"sections",home.sections()},
        {"composition",home.composition()},{"grid",home.grid()},{"placements",home.placements()},{"appearance",home.appearance()},{"wallpaperSource",home.wallpaperSource()},
        {"brandVisible",false},{"backgroundStart",QColor(home.backgroundStart())},{"backgroundEnd",QColor(home.backgroundEnd())},{"tileRoundness",0.18},
        {"internetAvailable",true},{"localNetworkAvailable",true},{"hasContextualSources",false},{"width",1280},{"height",800}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,800); qobject_cast<QQuickItem*>(root.get())->setParentItem(window.contentItem()); window.show();
    QTRY_VERIFY(root->property("hasWallpaper").toBool());
    QTRY_VERIFY(root->property("editorGeometryReady").toBool());
    auto *content = root->findChild<QQuickItem*>("composedContent"); QVERIFY(content);
    QVERIFY(content->property("contentHeight").toReal() <= content->height()+2);
    auto *composed = root->findChild<QObject*>("composedHome"); QVERIFY(composed);
    QSignalSpy search(root.get(),SIGNAL(searchRequested())), settings(root.get(),SIGNAL(settingsRequested(int)));
    QSignalSpy activated(root.get(),SIGNAL(actionActivated(QVariant,QString)));
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value)); };
    QCOMPARE(root->property("focusedItemId").toString(),QString("hdmi1"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("guide"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("favorite-apps"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("recently-watched"));
    QCOMPARE(home.components().last().toMap()["family"].toString(),QString("recently-viewed"));
    QCOMPARE(home.components().last().toMap()["source"].toMap()["kind"].toString(),QString("recently-viewed"));
    QVERIFY(command(InputManager::NavigateUp)); QCOMPARE(root->property("focusedItemId").toString(),QString("favorite-apps"));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"restoreFocus",Q_ARG(QVariant,QString("hdmi1"))));
    QVERIFY(command(InputManager::NavigateUp)); QVERIFY(composed->property("navigationFocused").toBool());
    QVERIFY(command(InputManager::NavigateUp)); QVERIFY(root->property("recoveryFocused").toBool());
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(search.size(),1);
    QVERIFY(command(InputManager::NavigateRight)); QVERIFY(command(InputManager::Confirm)); QCOMPARE(settings.last()[0].toInt(),-1);
    auto missing = home.composition(); missing["wallpaper"] = "artwork/missing.png";
    QVERIFY(home.setComposition(missing)); root->setProperty("wallpaperSource",home.wallpaperSource());
    QTRY_VERIFY(!root->property("hasWallpaper").toBool()); // fallback keeps the light palette readable
}
void HomeNavigationTests::televisionLibraryRemoteFlow()
{
    QTemporaryDir dir;
    QLocalServer server; QVERIFY(server.listen(dir.filePath("rpc.sock")));
    connect(&server, &QLocalServer::newConnection, &server, [&] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                const auto bytes = socket->property("input").toByteArray() + socket->readAll(); socket->setProperty("input", bytes);
                if (!bytes.endsWith('\n')) return;
                const auto request = QJsonDocument::fromJson(bytes).object();
                const auto method = request["method"].toString();
                QJsonObject result;
                if (method == "JSONRPC.Version") result = {{"version", QJsonObject{{"major", 13}}}};
                else if (method == "Isolde.GetArtwork") result = {{"mime", ""}, {"data", ""}};
                else if (method.startsWith("Isolde.Play")) {
                    const auto state = method == "Isolde.PlaybackControl" ? "ended" : "playing";
                    result = {{"state", state}, {"home", false}};
                    if (method == "Isolde.PlayEpisode") server.setProperty("episode", request["params"].toObject().toVariantMap());
                } else {
                    const auto key = method == "VideoLibrary.GetTVShows" ? "tvshows" : method == "VideoLibrary.GetSeasons" ? "seasons" : "episodes";
                    QJsonObject item{{"title", "Synthetic library fixture"}, {"plot", "Kodi-backed television navigation"}};
                    if (method == "VideoLibrary.GetTVShows") item["tvshowid"] = 7;
                    else if (method == "VideoLibrary.GetSeasons") { item["seasonid"] = 8; item["season"] = 0; }
                    else { item["episodeid"] = 9; item["season"] = 0; item["episode"] = 1; }
                    result = {{key, QJsonArray{item}}, {"limits", QJsonObject{{"start", 0}, {"end", 1}, {"total", 1}}}};
                }
                socket->write(QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}}).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            });
        }
    });
    AppRegistry registry; QVERIFY(registry.loadFromData(R"([{"id":"settings","name":"Settings","type":"settings","section":"System","accent":"#556677"}])"));
    SessionManager sessions(&registry);
    KodiMedia media("library-ui-test", nullptr, dir.path());
    KodiArtwork art("library-ui-test", nullptr, dir.path());
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("kodiMedia", &media);
    engine.rootContext()->setContextProperty("kodiArtwork", &art);
    engine.rootContext()->setContextProperty("sessionManager", &sessions);
    engine.rootContext()->setContextProperty("profileManager", QVariantMap{{"activeName", "Test profile"}});
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LIBRARY_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"width", 1280}, {"height", 720}, {"open", true}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280, 720); qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem()); window.show();
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value)); };
    media.openLibrary("shows"); QTRY_VERIFY(media.connected());
    QVERIFY(command(InputManager::Confirm)); QTRY_VERIFY(!media.busy()); QCOMPARE(media.mediaKind(), QString("seasons"));
    QVERIFY(command(InputManager::Confirm)); QTRY_VERIFY(!media.busy()); QCOMPARE(media.mediaKind(), QString("episodes"));
    QVERIFY(command(InputManager::Menu)); QVERIFY(!root->property("details").isNull());
    QVERIFY(command(InputManager::NavigateRight)); QTRY_VERIFY(!media.busy());
    QCOMPARE(server.property("episode").toMap()["episodeid"].toInt(), 9);
    QCOMPARE(server.property("episode").toMap()["resume"].toBool(), false);
    media.playbackControl("stop"); QTRY_VERIFY(!media.playbackActive());
    QVERIFY(root->property("open").toBool()); QCOMPARE(media.selectedIndex(), 0);
    QVERIFY(command(InputManager::Back)); // details close
    QVERIFY(command(InputManager::Back)); QTRY_VERIFY(!media.busy()); QCOMPARE(media.mediaKind(), QString("seasons"));
    QVERIFY(command(InputManager::Back)); QTRY_VERIFY(!media.busy()); QCOMPARE(media.mediaKind(), QString("shows"));
    QVERIFY(command(InputManager::NavigateUp)); QCOMPARE(media.selectedIndex(), -1);
    QVERIFY(command(InputManager::NavigateRight)); QVERIFY(command(InputManager::NavigateRight));
    QVERIFY(command(InputManager::Confirm)); QTRY_VERIFY(!media.busy());
    QCOMPARE(media.viewKind(), QString("recent")); QCOMPARE(media.mediaKind(), QString("episodes"));
    QVERIFY(command(InputManager::Back)); QTRY_VERIFY(!media.busy()); QCOMPARE(media.mediaKind(), QString("shows"));
    QVERIFY(command(InputManager::Back)); QVERIFY(!root->property("open").toBool());
}
void HomeNavigationTests::gridEditorMoveResizeDuplicateRemove()
{
    QTemporaryDir dir;
    QFile defaults(dir.filePath("default.json"));
    QVERIFY(defaults.open(QIODevice::WriteOnly));
    defaults.write(R"({"schemaVersion":1,"theme":{"backgroundStart":"#111111","backgroundEnd":"#222222"},"grid":{"columns":8,"rows":5},"items":[
        {"id":"alpha","kind":"app","appId":"app-a","name":"Alpha","placement":{"x":0,"y":0,"width":2,"height":1}},
        {"id":"beta","kind":"app","appId":"app-b","name":"Beta","placement":{"x":2,"y":0,"width":2,"height":1}},
        {"id":"gamma","kind":"app","appId":"app-c","name":"Gamma","placement":{"x":4,"y":0,"width":2,"height":1}}
    ]})");
    defaults.close();
    AppRegistry registry;
    QVERIFY(registry.loadFromData(R"([{"id":"app-a","name":"App A","type":"mock","section":"Apps","accent":"#556677"},{"id":"app-b","name":"App B","type":"mock","section":"Apps","accent":"#556677"},{"id":"app-c","name":"App C","type":"mock","section":"Apps","accent":"#556677"}] )"));
    HomeLayout home(&registry);
    QVERIFY(home.load(defaults.fileName(), dir.filePath("user.json")));
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("homeLayout", &home);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("LegacyCustomizeHomeView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"items", home.items()}, {"availableApps", QVariantList{}},
        {"artwork", QVariantList{}}, {"themes", QVariantList{}}, {"width", 1280}, {"height", 720}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    connect(&home, &HomeLayout::layoutChanged, root.get(), [&] { root->setProperty("items", home.items()); });
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value)); };
    auto items = [&]() { return root->property("items").toList(); };

    // Select "Alpha" (index 0), then choose "Change size" (actionIndex 2)
    root->setProperty("selectedIndex", 0);
    root->setProperty("actionIndex", 2);
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(root->property("spanEditing").toBool());
    QCOMPARE(root->property("spanTargetId").toString(), QString("alpha"));

    // Increase width from 2 to 3 through one remote step.
    QVERIFY(command(InputManager::NavigateRight));
    QCOMPARE(root->property("spanWidth").toInt(), 3);

    // Apply span (confirm) - should collide with "beta" at x=2
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(root->property("gridCollisionEditing").toBool());
    QCOMPARE(root->property("collisionTargets").toList().size(), 1);

    // Choose "Shift other" (collisionChoice 0), confirm
    root->setProperty("collisionChoice", 0);
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(!root->property("gridCollisionEditing").toBool());
    QVERIFY(!root->property("spanEditing").toBool());

    // Alpha should now span x=0..3, beta should have been shifted
    auto alpha = items()[0].toMap();
    QCOMPARE(alpha["id"].toString(), QString("alpha"));
    auto alphaPlace = alpha["placement"].toMap();
    QCOMPARE(alphaPlace["width"].toInt(), 3);

    // Select "Beta" (index 1), choose "Duplicate" (actionIndex 3)
    root->setProperty("selectedIndex", 1);
    root->setProperty("actionIndex", 30);
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(root->property("editing").toBool() == false); // duplicate completes immediately
    auto duplicated = items().last().toMap();
    QVERIFY(duplicated["id"].toString().startsWith("beta-copy"));

    // Select "Gamma" (index 2), choose "Remove item" (actionIndex 4)
    root->setProperty("selectedIndex", 2);
    root->setProperty("actionIndex", 3);
    QVERIFY(command(InputManager::Confirm));
    QCOMPARE(items().size(), 3); // alpha, beta, beta-copy (gamma removed)
    bool gammaFound = false;
    for (const auto &v : items()) if (v.toMap()["id"].toString() == "gamma") gammaFound = true;
    QVERIFY(!gammaFound);
}
void HomeNavigationTests::studioPageMenuCreatesAndDesignatesHome()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    const auto user=dir.filePath("home.json");QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString())));
    auto command=[&](int key){QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,key)));QCoreApplication::processEvents();};
    command(InputManager::Confirm);QCOMPARE(root->property("mode").toString(),QString("keyboard"));
    auto *keyboard=root->findChild<QObject *>("studioKeyboard");QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("Library"))));QCoreApplication::processEvents();
    const auto id=root->property("pageId").toString();QVERIFY(!id.isEmpty());
    QCOMPARE(root->property("mode").toString(),QString("canvas"));QVERIFY(!root->property("toolbarFocus").toBool());
    command(InputManager::Back);command(InputManager::NavigateDown);command(InputManager::NavigateDown);command(InputManager::Confirm);
    QCOMPARE(root->property("mode").toString(),QString("pageContext"));
    for(int i=0;i<4;++i)command(InputManager::NavigateDown);
    command(InputManager::Confirm);QCOMPARE(root->property("mode").toString(),QString("setHome"));
    command(InputManager::Confirm);
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);
    QCOMPARE(QJsonDocument::fromJson(session->draft()).object()["homePageId"].toString(),id);
    QVERIFY(!QFile::exists(user));QCOMPARE(home.homePageId(),QString());
    command(InputManager::Confirm);QCOMPARE(root->property("mode").toString(),QString("pageContext"));
    for(int i=0;i<7;++i)command(InputManager::NavigateDown);
    const auto before=session->draft();command(InputManager::Confirm);QCOMPARE(session->draft(),before);
    QVERIFY(root->property("message").toString().contains("another page"));
    command(InputManager::Back);command(InputManager::NavigateUp);command(InputManager::Confirm);
    QCOMPARE(root->property("pageId").toString(),QString());
    command(InputManager::NavigateDown);command(InputManager::NavigateDown);command(InputManager::Confirm);
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("Landing"))));QCoreApplication::processEvents();
    QCOMPARE(qobject_cast<HomeLayout *>(session->model())->rootPageName(),QString("Landing"));
    command(InputManager::NavigateDown);command(InputManager::Confirm); // Duplicate page.
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("Landing copy"))));QCoreApplication::processEvents();
    QVERIFY(root->property("pageId").toString()!=id);QVERIFY(!root->property("pageId").toString().isEmpty());
    command(InputManager::Back);command(InputManager::Confirm); // Root page menu origin restored.
    for(int i=0;i<7;++i)command(InputManager::NavigateDown);
    command(InputManager::Confirm);QCOMPARE(root->property("mode").toString(),QString("deletePage"));
    command(InputManager::NavigateDown);command(InputManager::Confirm);QCOMPARE(root->property("mode").toString(),QString("confirmDeletePage"));
    command(InputManager::Confirm);QVERIFY(!qobject_cast<HomeLayout *>(session->model())->rootPageVisible());
    QCOMPARE(root->property("pageId").toString(),id);QVERIFY(!QFile::exists(user));
    QVERIFY(session->undo());QVERIFY(qobject_cast<HomeLayout *>(session->model())->rootPageVisible());
}
void HomeNavigationTests::studioNamedPageViewportMatchesRuntime()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    const auto id=home.createPage("Library","blank");QVERIFY(!id.isEmpty());
    auto *page=qobject_cast<PageModel *>(home.pageModel(id));QVERIFY(page);
    QCOMPARE(page->composition().value("pageNavigation").toString(),QString("none"));
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    const auto qmlDir=QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir();
    QQmlComponent runtime(&engine,QUrl::fromLocalFile(qmlDir.filePath("PageSurface.qml")));
    QQmlComponent editor(&engine,QUrl::fromLocalFile(qmlDir.filePath("PageStudioView.qml")));
    QVERIFY2(runtime.isReady(),qPrintable(runtime.errorString()));QVERIFY2(editor.isReady(),qPrintable(editor.errorString()));
    for(const auto &navigation:QStringList{"left","top","none"}) {
        auto composition=page->composition();composition["pageNavigation"]=navigation;composition["sidebarWidth"]=260;
        QVERIFY(page->setComposition(composition));
        std::unique_ptr<QObject> live(runtime.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},
            {"provider",QVariant::fromValue(static_cast<QObject *>(nullptr))},{"availableApps",QVariantList{}},{"artwork",QVariantList{}},{"themes",QVariantList{}}}));
        std::unique_ptr<QObject> draft(editor.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)}}));
        QVERIFY(live);QVERIFY(draft);
        QVERIFY(QMetaObject::invokeMethod(live.get(),"showPage",Q_ARG(QVariant,id),Q_ARG(QVariant,QString()),Q_ARG(QVariant,false)));
        QVERIFY(QMetaObject::invokeMethod(draft.get(),"show",Q_ARG(QVariant,id),Q_ARG(QVariant,QString())));
        auto *content=live->findChild<QQuickItem *>("pageContent"),*preview=draft->findChild<QQuickItem *>("studioPreview");QVERIFY(content);QVERIFY(preview);
        for(const auto size:{QSize(1280,720),QSize(1920,1080),QSize(3840,2160)}) {
            for(auto *view:{live.get(),draft.get()}){view->setProperty("width",size.width());view->setProperty("height",size.height());}
            QCoreApplication::processEvents();
            QCOMPARE(content->x(),0.0);QCOMPARE(content->y(),0.0);
            QCOMPARE(content->width(),double(size.width()));QCOMPARE(content->height(),double(size.height()));
            QCOMPARE(preview->x(),content->x());QCOMPARE(preview->y(),content->y());
            QCOMPARE(preview->width(),content->width());QCOMPARE(preview->height(),content->height());
        }
        qobject_cast<StudioSession *>(draft->property("session").value<QObject *>())->invalidate();
    }
}
void HomeNavigationTests::studioRemoteGesturesCollisionsAndAnchors()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");QVERIFY(home.load(defaults,user));
    auto document=QJsonDocument::fromJson(home.committedDocument()).object();document["categories"]=QJsonArray{};
    document["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",4},{"rows",3},{"mode","screen"}}}};
    document["components"]=QJsonArray{
        QJsonObject{{"id","clock"},{"kind","widget"},{"widgetType","clock"},{"placement",QJsonObject{{"x",0},{"y",0},{"width",1},{"height",1}}}},
        QJsonObject{{"id","text"},{"kind","widget"},{"widgetType","text"},{"placement",QJsonObject{{"x",1},{"y",0},{"width",1},{"height",1}}}}};
    auto entries=document["components"].toArray();for(int i=0;i<entries.size();++i)entries[i]=QJsonObject::fromVariantMap(ComponentSchema::component(entries[i].toObject().toVariantMap(),true));document["components"]=entries;
    const auto disk=QJsonDocument(document).toJson();QFile saved(user);QVERIFY(saved.open(QIODevice::WriteOnly));QCOMPARE(saved.write(disk),disk.size());saved.close();QVERIFY(home.load(defaults,user));QVERIFY2(home.errorString().isEmpty(),qPrintable(home.errorString()));
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("clock"))));QTest::qWait(50);
    auto key=[&](int value){const bool ok=QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value));QCoreApplication::processEvents();return ok;};
    auto map=[](const QVariant &value){return (value.metaType().id()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant():value).toMap();};
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);const auto start=session->draft();
    auto *preview=root->findChild<QQuickItem *>("studioPreview");QVERIFY(preview);QSignalSpy actions(preview,SIGNAL(actionActivated(QVariant,QString)));QVERIFY(actions.isValid());
    std::function<QQuickItem *(QQuickItem *,const QString &)> findNode=[&](QQuickItem *item,const QString &name)->QQuickItem *{if(item->objectName()==name)return item;for(auto *child:item->childItems())if(auto *found=findNode(child,name))return found;return nullptr;};
    auto *clockNode=findNode(preview,"placed-clock"),*textNode=findNode(preview,"placed-text"),*ghost=root->findChild<QQuickItem *>("studioGhost");QVERIFY(clockNode);QVERIFY(textNode);QVERIFY(ghost);
    QVERIFY(key(InputManager::NavigateRight));
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("size"));
    auto *widthSlider=findNode(qobject_cast<QQuickItem *>(root.get()),"studioSize-width"),*heightSlider=findNode(qobject_cast<QQuickItem *>(root.get()),"studioSize-height");QVERIFY(widthSlider);QVERIFY(heightSlider);
    QCOMPARE(widthSlider->property("from").toInt(),1);QCOMPARE(widthSlider->property("to").toInt(),4);
    QCOMPARE(heightSlider->property("from").toInt(),1);QCOMPARE(heightSlider->property("to").toInt(),3);
    for(int i=0;i<3;++i)QVERIFY(key(InputManager::NavigateRight));
    QCOMPARE(map(root->property("candidate"))["width"].toInt(),4);QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::NavigateRight));QVERIFY(key(InputManager::NavigateRight));
    QCOMPARE(map(root->property("candidate"))["height"].toInt(),3);
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm)); // Full occupied page cannot be committed.
    QCOMPARE(root->property("mode").toString(),QString("size"));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    QVERIFY(key(InputManager::Back));QVERIFY(key(InputManager::Back));QVERIFY(key(InputManager::Back));
    QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(session->draft(),start);
    const QList<QPair<QString,QList<int>>> cancelCases{
        {"move",{InputManager::Confirm,InputManager::Confirm}},
        {"moveOptions",{InputManager::Confirm,InputManager::Confirm,InputManager::Menu}},
        {"geometry",{InputManager::Confirm,InputManager::Confirm,InputManager::Menu,InputManager::Confirm}},
        {"keyboard",{InputManager::Confirm,InputManager::Confirm,InputManager::Menu,InputManager::Confirm,InputManager::Confirm}},
        {"remove",{InputManager::Confirm,InputManager::NavigateDown,InputManager::NavigateDown,InputManager::NavigateDown,InputManager::Confirm}},
        {"catalog",{InputManager::Search}},
        {"move",{InputManager::Search,InputManager::NavigateDown,InputManager::NavigateDown,InputManager::NavigateRight,InputManager::Confirm,InputManager::Confirm}},
    };
    for(const auto &entry:cancelCases){
        for(int command:entry.second)QVERIFY(key(command));QCOMPARE(root->property("mode").toString(),entry.first);
        QVERIFY(key(InputManager::Back));if(entry.first=="keyboard" || entry.first=="collisionDetails")QVERIFY(key(InputManager::Back));if(root->property("mode").toString()=="inspector"){QVERIFY(key(InputManager::Back));QVERIFY(key(InputManager::Back));}
        QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(root->property("selectedId").toString(),QString("clock"));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    }
    QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("context"));
    QVariant menu;QVERIFY(QMetaObject::invokeMethod(root.get(),"dialogLabels",Q_RETURN_ARG(QVariant,menu)));
    const auto menuLabels=(menu.metaType().id()==qMetaTypeId<QJSValue>()?menu.value<QJSValue>().toVariant():menu).toStringList();
    QCOMPARE(menuLabels,QStringList({"Move","Edit component","Duplicate","Delete…"}));
    QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("move"));
    QVERIFY(!clockNode->isVisible());QVERIFY(textNode->isVisible());
    auto *ghostCard=findNode(ghost,"studioGhostCard"),*composed=preview->findChild<QQuickItem *>("composedHome");QVERIFY(ghostCard);QVERIFY(composed);QCOMPARE(ghostCard->property("unit").toReal(),composed->property("unit").toReal());
    QVERIFY(key(InputManager::NavigateRight));QCOMPARE(session->draft(),start);
    const auto liveProposal=map(root->property("collisionPreview"));QVERIFY(liveProposal["valid"].toBool());
    QCOMPARE(session->changeCount(),0);QVERIFY(!textNode->isVisible());
    QVERIFY(key(InputManager::Back));QCOMPARE(session->draft(),start);QVERIFY(textNode->isVisible());
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateRight));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"acceptCandidate",Q_ARG(QVariant,QString("cancel"))));QCOMPARE(root->property("mode").toString(),QString("collision"));
    QVERIFY(!clockNode->isVisible());QVERIFY(!textNode->isVisible());QVERIFY(!ghost->isVisible());
    auto *proposedClock=findNode(qobject_cast<QQuickItem *>(root.get()),"studioProposal-clock");QVERIFY(proposedClock);QVERIFY(proposedClock->isVisible());
    auto *proposedCard=findNode(proposedClock,"studioProposedCard-clock");QVERIFY(proposedCard);QCOMPARE(proposedCard->property("unit").toReal(),composed->property("unit").toReal());
    for(int i=0;i<3;++i)QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("collisionDetails"));QCOMPARE(session->draft(),start);
    QVERIFY(key(InputManager::Back));QCOMPARE(root->property("mode").toString(),QString("collision"));
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    QVERIFY(clockNode->isVisible());QVERIFY(textNode->isVisible());
    QCOMPARE(root->property("selectedId").toString(),QString("clock"));
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateRight));QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(session->changeCount(),1);QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"beginGesture",Q_ARG(QVariant,true)));QVERIFY(key(InputManager::NavigateRight));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"acceptCandidate",Q_ARG(QVariant,QString("cancel"))));QCOMPARE(root->property("mode").toString(),QString("collision"));
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QCOMPARE(session->draft(),start);
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));
    for(int i=0;i<100;++i)QVERIFY(key(InputManager::NavigateRight));
    QCOMPARE(map(root->property("candidate"))["x"].toInt(),3);QVERIFY(root->property("message").toString().contains("boundary"));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    QVERIFY(key(InputManager::Back));QCOMPARE(session->draft(),start);QCOMPARE(root->property("selectedId").toString(),QString("clock"));
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));for(int i=0;i<3;++i)QVERIFY(key(InputManager::NavigateRight));QVERIFY(key(InputManager::Confirm));
    const auto beforeAnchor=session->draft();QCOMPARE(session->changeCount(),1);
    for(bool accept:{false,true}) {
        QVERIFY(QMetaObject::invokeMethod(root.get(),"beginGesture",Q_ARG(QVariant,true)));QVERIFY(key(InputManager::Menu));QCOMPARE(root->property("mode").toString(),QString("moveOptions"));
        for(int i=0;i<4;++i)QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("anchors"));
        QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateRight));
        const auto candidate=map(root->property("candidate"));QCOMPARE(candidate["x"].toInt(),2);QCOMPARE(candidate["width"].toInt(),2);
        QVERIFY(key(accept?InputManager::Confirm:InputManager::Back));
        if(!accept){QCOMPARE(session->draft(),beforeAnchor);QCOMPARE(session->changeCount(),1);}
    }
    QCOMPARE(session->changeCount(),2);QVERIFY(session->undo());QCOMPARE(session->draft(),beforeAnchor);QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    // A blocked ordinary drop stays on the canvas. It never forces the old
    // Shift/Swap dialog, changes the draft, or strands the moving item.
    QVERIFY(session->execute("state",{{"id","text"},{"key","locked"},{"value",true}}));
    const auto locked=session->draft();const auto lockedCount=session->changeCount();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"beginGesture",Q_ARG(QVariant,false)));
    QVERIFY(key(InputManager::NavigateRight));QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("move"));
    QVERIFY(root->property("message").toString().contains("locked",Qt::CaseInsensitive));
    QCOMPARE(session->draft(),locked);QCOMPARE(session->changeCount(),lockedCount);
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("canvas"));
    QVERIFY(session->undo());QCOMPARE(session->draft(),locked);QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    // Duplicate and Edit contents must not dispatch to Delete after the
    // coder's context-menu index changes.
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(session->model()->property("items").toList().size(),3);
    QVERIFY(session->undo());QCOMPARE(session->draft(),start);
    QCOMPARE(actions.count(),0);QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),disk);
}
void HomeNavigationTests::studioRemoteSectionTransferAndCancel()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    const auto user=dir.filePath("home.json");QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    auto *setup=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(setup);QVERIFY(setup->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5},{"mode","sections"}}},{"sectionHeight",560}}));QVERIFY(setup->apply());
    QFile saved(user);QVERIFY(saved.open(QIODevice::ReadOnly));const auto disk=saved.readAll();saved.close();
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("clock"))));
    auto key=[&](int value){const bool ok=QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value));QCoreApplication::processEvents();return ok;};
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);const auto start=session->draft();QVERIFY(key(InputManager::NavigateRight));
    const QList<int> destinationKeys{InputManager::Confirm,InputManager::Confirm,InputManager::Menu,InputManager::NavigateDown,InputManager::NavigateDown,InputManager::NavigateDown,InputManager::Confirm};
    const auto openDestination=[&](){for(int command:destinationKeys)if(!key(command))return false;return true;};
    QVERIFY(openDestination());QCOMPARE(root->property("mode").toString(),QString("destination"));QVERIFY(key(InputManager::Back));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);QCOMPARE(root->property("scopeId").toString(),QString("widgets"));
    auto *viewport=root->findChild<QObject *>("studioPreview");QVERIFY(viewport);
    for(bool accept:{false,true}) {
        QCoreApplication::processEvents();
        const QVariantMap scroll{{"legacyY",0},{"composed",QVariantMap{{"y",137},{"navigationX",0}}},{"groups",QVariantMap{}}};
        QVERIFY(QMetaObject::invokeMethod(viewport,"restoreEditorViewport",Q_ARG(QVariant,scroll)));
        QVERIFY(openDestination());QCOMPARE(root->property("mode").toString(),QString("destination"));
        for(int i=0;i<3;++i)QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("move"));QCOMPARE(root->property("scopeId").toString(),QString("inputs"));
        for(int i=0;i<4;++i)QVERIFY(key(InputManager::NavigateDown));QCOMPARE(session->draft(),start);
        QVERIFY(key(accept?InputManager::Confirm:InputManager::Back));
        if(!accept){QCOMPARE(root->property("scopeId").toString(),QString("widgets"));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
            QTest::qWait(50);QVariant restored;QVERIFY(QMetaObject::invokeMethod(viewport,"editorViewport",Q_RETURN_ARG(QVariant,restored)));
            const auto value=restored.value<QJSValue>().toVariant().toMap();QCOMPARE(value["composed"].toMap()["y"].toInt(),137);
        }
    }
    QCOMPARE(session->changeCount(),1);QCOMPARE(QJsonDocument::fromJson(session->draft()).object()["components"].toArray().first().toObject()["categoryId"].toString(),QString("inputs"));
    QVERIFY(session->undo());QCoreApplication::processEvents();QCOMPARE(session->draft(),start);QTRY_COMPARE(root->property("scopeId").toString(),QString("widgets"));QCOMPARE(root->property("selectedId").toString(),QString("clock"));
    QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),disk);
}
void HomeNavigationTests::studioScrollingAddMoveAndCancel()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");QVERIFY(home.load(defaults,user));
    auto doc=QJsonDocument::fromJson(home.committedDocument()).object();doc["categories"]=QJsonArray{};
    doc["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",4},{"rows",3},{"mode","screen"}}}};
    QJsonArray nodes;for(int i=0;i<12;++i)nodes.append(QJsonObject::fromVariantMap(ComponentSchema::component({{"id",QString("cell-%1").arg(i)},{"kind","widget"},{"widgetType","text"},{"name",QString("Item %1").arg(i)},{"placement",QVariantMap{{"x",i%4},{"y",i/4},{"width",1},{"height",1}}}},true)));
    doc["components"]=nodes;const auto disk=QJsonDocument(doc).toJson();QFile file(user);QVERIFY(file.open(QIODevice::WriteOnly));file.write(disk);file.close();QVERIFY(home.load(defaults,user));
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("cell-0"))));
    auto key=[&](int value){const auto ok=QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value));QCoreApplication::processEvents();return ok;};
    const auto variant=[](const QVariant &v){return v.metaType().id()==qMetaTypeId<QJSValue>()?v.value<QJSValue>().toVariant():v;};
    std::function<QQuickItem *(QQuickItem *,const QString &)> findVisual=[&](QQuickItem *item,const QString &name)->QQuickItem *{if(item->objectName()==name)return item;for(auto *child:item->childItems())if(auto *found=findVisual(child,name))return found;return nullptr;};
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);const auto start=session->draft();
    auto *preview=root->findChild<QQuickItem *>("studioPreview");QVERIFY(preview);auto *composed=preview->findChild<QObject *>("composedHome");QVERIFY(composed);
    QTest::qWait(30);const auto initialCell=variant(composed->property("placementCell")).toMap();
    for(bool accept:{false,true}){
        QVERIFY(QMetaObject::invokeMethod(root.get(),"startAddition",Q_ARG(QVariant,QString("text")),Q_ARG(QVariant,QVariantMap{})));
        QCOMPARE(root->property("mode").toString(),QString("noRoom"));QCOMPARE(session->draft(),start);
        QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("move"));
        QVERIFY(root->property("pendingScrollEnable").toBool());QCOMPARE(variant(root->property("candidate")).toMap()["y"].toInt(),3);
        QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
        QVERIFY(key(accept?InputManager::Confirm:InputManager::Back));
        if(!accept){QTest::qWait(30);QCOMPARE(session->draft(),start);QVERIFY(!root->property("pendingScrollEnable").toBool());}
    }
    QCOMPARE(session->changeCount(),1);QVERIFY(session->model()->property("composition").toMap()["verticalScrolling"].toBool());
    QVERIFY(session->undo());QCOMPARE(session->draft(),start);root->setProperty("selectedId","cell-0");QCoreApplication::processEvents();
    // Enter the history-budget dialog while an Add is provisional: cancel
    // must clear the pending policy as well as the component.
    QVERIFY(QMetaObject::invokeMethod(root.get(),"startAddition",Q_ARG(QVariant,QString("text")),Q_ARG(QVariant,QVariantMap{})));
    QVERIFY(key(InputManager::Confirm));QVERIFY(root->property("pendingScrollEnable").toBool());
    root->setProperty("mode","history");root->setProperty("choice",0);QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("canvas"));QVERIFY(!root->property("pendingScrollEnable").toBool());QCOMPARE(session->draft(),start);
    // Exercise the real page-property checkbox and its draft preview panel.
    QVERIFY(QMetaObject::invokeMethod(root.get(),"openInspector",Q_ARG(QVariant,true)));
    auto *inspector=root->findChild<QObject *>("studioInspector");QVERIFY(inspector);inspector->setProperty("selected",2);
    QVERIFY(key(InputManager::Confirm));QCOMPARE(inspector->property("view").toString(),QString("fields"));
    const auto fields=variant(inspector->property("fields")).toList();QVERIFY(!fields.isEmpty());QCOMPARE(fields[0].toMap()["key"].toString(),QString("verticalScrolling"));
    QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("scrolling"));
    auto *toggle=root->findChild<QObject *>("studioScrollingToggle");QVERIFY(toggle);QVERIFY(!toggle->property("checked").toBool());
    QVERIFY(key(InputManager::Confirm));QVERIFY(toggle->property("checked").toBool());QCOMPARE(session->draft(),start);
    QVERIFY(key(InputManager::Back));QCOMPARE(session->draft(),start);QVERIFY(!session->proposalModel());
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));
    QCOMPARE(root->property("mode").toString(),QString("inspector"));QCOMPARE(session->changeCount(),1);
    QVERIFY(key(InputManager::Back));QVERIFY(key(InputManager::Back));QCOMPARE(root->property("mode").toString(),QString("canvas"));
    const auto enabled=session->draft();QCOMPARE(variant(composed->property("placementCell")).toMap()["cellH"],initialCell["cellH"]);
    for(bool accept:{false,true}){
        QVERIFY(QMetaObject::invokeMethod(root.get(),"beginGesture",Q_ARG(QVariant,false)));
        for(int i=0;i<12;++i)QVERIFY(key(InputManager::NavigateDown));QTest::qWait(40);
        QVERIFY(composed->property("scrollOffset").toReal()>0);QCOMPARE(session->draft(),enabled);
        QVariant placed;QVERIFY(QMetaObject::invokeMethod(preview,"placementRectFor",Q_RETURN_ARG(QVariant,placed),Q_ARG(QVariant,variant(root->property("candidate"))),Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString())));
        const auto visibleRect=variant(placed).toMap(),viewport=variant(preview->property("pageViewport")).toMap();
        auto *rendered=findVisual(qobject_cast<QQuickItem *>(root.get()),"studioProposal-cell-0");QVERIFY(rendered);QVERIFY(rendered->isVisible());
        const auto actual=rendered->mapToItem(preview,QPointF(0,0));
        QVERIFY(qAbs(actual.y()-visibleRect["y"].toReal())<0.01);QVERIFY(qAbs(actual.x()-visibleRect["x"].toReal())<0.01);
        QVERIFY(visibleRect["y"].toReal()>=viewport["y"].toReal()-0.01);
        QVERIFY2(visibleRect["y"].toReal()+visibleRect["height"].toReal()<=viewport["y"].toReal()+viewport["height"].toReal()+0.01,qPrintable(QString("target %1 + %2, viewport %3 + %4, scroll %5").arg(visibleRect["y"].toReal()).arg(visibleRect["height"].toReal()).arg(viewport["y"].toReal()).arg(viewport["height"].toReal()).arg(composed->property("scrollOffset").toReal())));
        QCOMPARE(variant(composed->property("placementCell")).toMap()["cellH"],initialCell["cellH"]);
        QVERIFY(key(accept?InputManager::Confirm:InputManager::Back));
        if(!accept){QTest::qWait(40);QCOMPARE(session->draft(),enabled);QCOMPARE(composed->property("scrollOffset").toInt(),0);}
    }
    QVERIFY(QMetaObject::invokeMethod(root.get(),"inspectorOperation",Q_ARG(QVariant,QString("scrolling"))));
    QVERIFY(key(InputManager::Confirm));QVERIFY(root->property("scrollingDraft").toBool());QVERIFY(root->property("message").toString().contains("below"));
    QVERIFY(key(InputManager::Back));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),disk);
    // Runtime consumes the same grid/placements as Main, not just Studio's
    // proposal geometry. Offscreen widgets instantiate when focused.
    auto *model=session->model();
    QQmlComponent runtime(&engine,QUrl::fromLocalFile(QStringLiteral(HOME_QML)));QVERIFY2(runtime.isReady(),qPrintable(runtime.errorString()));
    QVariantMap properties{{"width",1280},{"height",720},{"internetAvailable",false},{"localNetworkAvailable",false},{"hasContextualSources",false}};
    for(const auto &name:QStringList{"items","sections","brandVisible","backgroundStart","backgroundEnd","tileRoundness","appearance","composition","grid","placements"})properties[name]=model->property(name.toUtf8().constData());
    std::unique_ptr<QObject> live(runtime.createWithInitialProperties(properties));QVERIFY2(live,qPrintable(runtime.errorString()));
    auto *liveItem=qobject_cast<QQuickItem *>(live.get());liveItem->setParentItem(window.contentItem());
    QVERIFY(QMetaObject::invokeMethod(live.get(),"restoreFocus",Q_ARG(QVariant,QString("cell-0"))));QTest::qWait(50);
    auto *card=findVisual(liveItem,"placed-cell-0");QVERIFY(card);QVERIFY(card->isVisible());
    const auto actual=card->mapToItem(liveItem,QPointF(0,0));const auto viewport=variant(live->property("pageViewport")).toMap();
    QVERIFY(actual.y()>=viewport["y"].toReal()-0.01);QVERIFY2(actual.y()+card->height()<=viewport["y"].toReal()+viewport["height"].toReal()+0.01,qPrintable(QString("runtime target %1 + %2, viewport %3 + %4").arg(actual.y()).arg(card->height()).arg(viewport["y"].toReal()).arg(viewport["height"].toReal())));
}
void HomeNavigationTests::studioLegacyRemoteOrdering()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;const auto user=dir.filePath("home.json");
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("movies"))));
    auto key=[&](int value){const bool ok=QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value));QCoreApplication::processEvents();return ok;};
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);const auto start=session->draft();QVERIFY(key(InputManager::NavigateRight));
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("flowMove"));
    auto *preview=root->findChild<QQuickItem *>("studioPreview");QVERIFY(preview);
    QCOMPARE(preview->property("suppressedComponentIds").toList().size(),0);
    auto *insertion=root->findChild<QQuickItem *>("studioInsertionTarget");QVERIFY(insertion);
    QTRY_VERIFY(insertion->isVisible());QVERIFY(insertion->width()>0);QVERIFY(insertion->height()>0);
    QVERIFY(key(InputManager::NavigateDown));QVERIFY(session->proposalModel());QCOMPARE(session->draft(),start);QVERIFY(key(InputManager::Back));QVERIFY(!session->proposalModel());QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateDown));QCOMPARE(session->draft(),start);QCOMPARE(session->changeCount(),0);QVERIFY(session->proposalModel());
    QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(session->changeCount(),1);QVERIFY(!root->property("gridMode").toBool());
    const auto entries=QJsonDocument::fromJson(session->draft()).object()["components"].toArray();QCOMPARE(entries[1].toObject()["id"].toString(),QString("shows"));QCOMPARE(entries[2].toObject()["id"].toString(),QString("movies"));
    QVERIFY(session->undo());QCOMPARE(session->draft(),start);QCOMPARE(root->property("selectedId").toString(),QString("movies"));QVERIFY(!QFile::exists(user));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"inspectorOperation",Q_ARG(QVariant,QString("geometry"))));
    QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(session->draft(),start);QVERIFY(!session->proposalModel());
}
void HomeNavigationTests::studioSectionGeometryMatchesRuntime()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    auto *setup=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(setup);
    QVERIFY2(setup->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5},{"mode","sections"}}},{"sectionHeight",560}}),qPrintable(setup->errorString()));
    QVERIFY(setup->apply());
    InputManager input;QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("homeLayout",&home);
    engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("clock"))));
    auto *preview=root->findChild<QQuickItem *>("studioPreview");QVERIFY(preview);
    std::function<QQuickItem *(QQuickItem *)> find=[&](QQuickItem *parent)->QQuickItem *{
        if(parent->objectName()=="placed-clock")return parent;
        for(auto *child:parent->childItems())if(auto *found=find(child))return found;
        return nullptr;
    };
    for(const auto size:{QSize(1280,720),QSize(1920,1080),QSize(3840,2160),QSize(1280,800)}) {
        root->setProperty("width",size.width());root->setProperty("height",size.height());QTest::qWait(60);
        auto *placed=find(preview);QVERIFY(placed);QVERIFY(placed->isVisible());
        QVariant result;QVERIFY(QMetaObject::invokeMethod(preview,"placementRectFor",Q_RETURN_ARG(QVariant,result),
            Q_ARG(QVariant,QVariantMap({{"x",0},{"y",0},{"width",1},{"height",1}})),Q_ARG(QVariant,QString("widgets")),Q_ARG(QVariant,QString())));
        const auto rect=result.value<QJSValue>().toVariant().toMap();
        const auto point=placed->mapToItem(preview,QPointF(0,0));
        QVERIFY(qAbs(point.x()-rect["x"].toDouble())<0.01);QVERIFY(qAbs(point.y()-rect["y"].toDouble())<0.01);
        QVERIFY(qAbs(placed->height()-rect["height"].toDouble())<0.01);
        QCOMPARE(preview->width(),qreal(size.width()));QCOMPARE(preview->height(),qreal(size.height()));
    }
}
void HomeNavigationTests::studioRemoteDraftApplyAndCancel()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto user = dir.filePath("home.json");
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), user));
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("homeLayout", &home);
    const auto path = QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml");
    engine.rootContext()->setContextProperty("isoldeVersion", "test");
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel", QVariant::fromValue(&home)}, {"width",1280}, {"height",720}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,720);
    qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem()); window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(), "show", Q_ARG(QVariant, QString()), Q_ARG(QVariant, QString())));
    QTest::qWait(80);
    auto command = [&](int key) { QVERIFY(QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant,key))); QCoreApplication::processEvents(); };
    command(InputManager::NavigateDown);command(InputManager::NavigateDown);
    command(InputManager::Confirm); // Add component on legacy page opens explicit conversion.
    QCOMPARE(root->property("mode").toString(), QString("convert"));
    command(InputManager::Confirm); // Preview consequences, still no write.
    QCOMPARE(root->property("mode").toString(), QString("conversionPreview"));
    QVERIFY(!QFile::exists(user));
    command(InputManager::Confirm); // Accept into draft only.
    QCOMPARE(root->property("mode").toString(), QString("canvas"));
    QVERIFY(!QFile::exists(user));
    auto *preview = root->findChild<QQuickItem *>("studioPreview"); QVERIFY(preview);
    std::function<QQuickItem *(QQuickItem *,const QString &)> findVisual = [&](QQuickItem *item,const QString &name)->QQuickItem * {
        if(item->objectName()==name)return item;
        for(auto *child:item->childItems())if(auto *found=findVisual(child,name))return found;
        return nullptr;
    };
    for (const auto &size : {QSize(1280,720),QSize(1920,1080),QSize(3840,2160),QSize(1280,800)}) {
        root->setProperty("width",size.width()); root->setProperty("height",size.height());
        QCoreApplication::processEvents();
        auto *placed = findVisual(preview,"placed-clock"); QVERIFY(placed);
        QVERIFY2(placed->isVisible(),qPrintable(QString("Studio visible=%1, grid=%2, preview visible=%3, composed grid=%4")
            .arg(root->property("visible").toBool()).arg(root->property("gridMode").toBool()).arg(preview->isVisible())
            .arg(preview->findChild<QObject *>("composedHome")->property("placementMode").toBool())));
        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(preview,"placementRect",Q_RETURN_ARG(QVariant,result),
            Q_ARG(QVariant,QVariantMap({{"x",0},{"y",0},{"width",1},{"height",1}}))));
        const auto rect=result.metaType().id()==qMetaTypeId<QJSValue>() ? result.value<QJSValue>().toVariant().toMap():result.toMap();
        const auto point=placed->mapToItem(preview,QPointF(0,0));
        QVERIFY(qAbs(point.x()-rect["x"].toDouble())<0.01);
        QVERIFY(qAbs(point.y()-rect["y"].toDouble())<0.01);
        QVERIFY(qAbs(placed->width()-rect["width"].toDouble())<0.01);
        QVERIFY(point.y()+placed->height()<=preview->height());
        QVERIFY(placed->width()>0 && placed->height()>0);
    }
    root->setProperty("width",1280);root->setProperty("height",720);
    QVERIFY(QMetaObject::invokeMethod(root.get(), "chooseTool", Q_ARG(QVariant,0)));
    command(InputManager::NavigateDown);command(InputManager::NavigateDown);command(InputManager::NavigateRight);command(InputManager::Confirm);command(InputManager::Confirm); // Widgets -> Clock ghost.
    QCOMPARE(root->property("mode").toString(), QString("move"));
    command(InputManager::NavigateDown);
    command(InputManager::NavigateDown);
    command(InputManager::Confirm); // A free third-row cell beyond the default launchers.
    QCOMPARE(root->property("mode").toString(), QString("canvas"));
    QVERIFY(!QFile::exists(user));
    auto *session = qobject_cast<StudioSession *>(root->property("session").value<QObject *>());
    QVERIFY(session); QVERIFY(session->isDirty());
    auto *keyboard = root->findChild<QObject *>("studioKeyboard"); QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"context",Q_ARG(QVariant,6)));
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("0"))));
    QCoreApplication::processEvents();
    auto *selected = findVisual(preview,"placed-"+root->property("selectedId").toString()); QVERIFY(selected);
    auto *surface = findVisual(selected,"cardSurface"); QVERIFY(surface);
    QCOMPARE(surface->property("color").value<QColor>().alpha(),0);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"context",Q_ARG(QVariant,7)));
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("0"))));
    QCoreApplication::processEvents();
    selected = findVisual(preview,"placed-"+root->property("selectedId").toString()); QVERIFY(selected);
    surface = findVisual(selected,"cardSurface"); QVERIFY(surface);
    QCOMPARE(surface->property("radius").toReal(),0.0);
    const auto selectedId=root->property("selectedId").toString();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"context",Q_ARG(QVariant,9))); // Hide, but keep reachable in Layers.
    QVERIFY(QMetaObject::invokeMethod(root.get(),"chooseTool",Q_ARG(QVariant,2)));
    QVariant labels;QVERIFY(QMetaObject::invokeMethod(root.get(),"dialogLabels",Q_RETURN_ARG(QVariant,labels)));
    const auto layerLabels=(labels.metaType().id()==qMetaTypeId<QJSValue>()?labels.value<QJSValue>().toVariant():labels).toStringList();QVERIFY(!layerLabels.isEmpty());
    const auto hidden=std::find_if(layerLabels.cbegin(),layerLabels.cend(),[](const QString &label){return label.contains("Hidden");});QVERIFY(hidden!=layerLabels.cend());
    for(int i=0;i<std::distance(layerLabels.cbegin(),hidden);++i)command(InputManager::NavigateDown);
    command(InputManager::Confirm);QCOMPARE(root->property("selectedId").toString(),selectedId);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"context",Q_ARG(QVariant,9))); // Show again.
    const auto changed = session->draft();
    QVERIFY(session->undo()); QVERIFY(session->redo()); QCOMPARE(session->draft(), changed);
    QVERIFY(QMetaObject::invokeMethod(root.get(), "chooseTool", Q_ARG(QVariant,7)));
    QCOMPARE(root->property("mode").toString(), QString("preview"));
    command(InputManager::Back);
    QCOMPARE(root->property("mode").toString(), QString("canvas"));
    QVERIFY(!QFile::exists(user));
    QVERIFY(QMetaObject::invokeMethod(root.get(), "chooseTool", Q_ARG(QVariant,8)));
    command(InputManager::Confirm);
    QVERIFY2(!session->isDirty(), qPrintable(session->errorString()));
    QVERIFY(QFile::exists(user));
    HomeLayout restored(&registry); QVERIFY(restored.load(QFINDTESTDATA("../config/default-home-layout.json"),user));
    QCOMPARE(restored.committedDocument(), home.committedDocument());
    command(InputManager::Back);
    command(InputManager::Back); // First Back restores the page menu; second exits.
    QVERIFY(!root->property("open").toBool());
}
void HomeNavigationTests::studioRemotePointerAndInspectorParity_data()
{
    QTest::addColumn<bool>("nested");QTest::newRow("page")<<false;QTest::newRow("group")<<true;
}
void HomeNavigationTests::studioRemotePointerAndInspectorParity()
{
    QFETCH(bool,nested);
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    auto *setup=qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(setup);
    QVERIFY(setup->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5}}}}));
    if(nested){
        auto doc=QJsonDocument::fromJson(setup->draft()).object();auto group=QJsonObject::fromVariantMap(setup->componentPrototype("container")["root"].toMap());
        group["placement"]=QJsonObject{{"x",0},{"y",0},{"width",4},{"height",4}};auto style=group["presentation"].toObject();auto local=style["layout"].toObject();local["rows"]=4;style["layout"]=local;group["presentation"]=style;
        QJsonArray entries;entries.append(group);
        for(const auto &type:QStringList{"clock","text"}){auto child=QJsonObject::fromVariantMap(setup->componentPrototype(type)["root"].toMap());child["id"]=type;child["parentId"]=group["id"];child["placement"]=QJsonObject{{"x",type=="clock"?0:3},{"y",0},{"width",1},{"height",1}};entries.append(child);}
        doc["components"]=entries;doc["categories"]=QJsonArray{};QVERIFY2(setup->pushEdit(QJsonDocument(doc).toJson()),qPrintable(setup->errorString()));
    }
    QVERIFY(setup->apply());
    const auto baseline=home.committedDocument();
    InputManager input; QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("homeLayout",&home);
    engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,720); qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem()); window.show();
    auto show=[&] { QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("clock")))); QTest::qWait(80); };
    auto command=[&](int key) { QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,key))); QCoreApplication::processEvents(); };
    auto session=[&] {return qobject_cast<StudioSession *>(root->property("session").value<QObject *>());};
    show(); command(InputManager::NavigateRight); command(InputManager::Confirm); command(InputManager::Confirm);
    command(InputManager::NavigateDown); command(InputManager::NavigateDown); command(InputManager::Confirm);
    QCOMPARE(root->property("mode").toString(),QString("canvas"));
    QVERIFY(session()); const auto remote=session()->draft(); QVERIFY(remote!=baseline);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"close"));

    show();command(InputManager::NavigateRight); // Collapse the page menu before direct dragging.
    auto *preview=root->findChild<QQuickItem *>("studioPreview"); QVERIFY(preview);
    auto center=[&](int row) {
        QVariant result;
        QMetaObject::invokeMethod(preview,"placementRectFor",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QVariantMap({{"x",0},{"y",row},{"width",1},{"height",1}})),Q_ARG(QVariant,QString()),Q_ARG(QVariant,nested?QString("group"):QString()));
        const auto rect=result.metaType().id()==qMetaTypeId<QJSValue>()?result.value<QJSValue>().toVariant().toMap():result.toMap();
        return preview->mapToItem(window.contentItem(),QPointF(rect["x"].toDouble()+rect["width"].toDouble()/2,rect["y"].toDouble()+rect["height"].toDouble()/2)).toPoint();
    };
    const auto from=center(0), to=center(2);
    QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,from);
    QTest::mouseMove(&window,to,60); QTest::qWait(80);
    QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,to);
    QCoreApplication::processEvents();
    QCOMPARE(root->property("mode").toString(),QString("canvas"));
    QCOMPARE(session()->draft(),remote);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"close"));

    show(); command(InputManager::NavigateRight); command(InputManager::Menu);
    command(InputManager::NavigateDown);
    command(InputManager::Confirm); // Edit component.
    auto *inspector=root->findChild<QObject *>("studioInspector");QVERIFY(inspector);if(inspector->property("collapsed").toBool())command(InputManager::Confirm);
    command(InputManager::NavigateDown);command(InputManager::NavigateDown);command(InputManager::Confirm); // Layout group.
    command(InputManager::Confirm); QCOMPARE(root->property("mode").toString(),QString("size"));
    for(int i=0;i<4;++i)command(InputManager::NavigateDown);command(InputManager::Confirm); // Optional exact position controls.
    QCOMPARE(root->property("mode").toString(),QString("geometry"));
    command(InputManager::NavigateDown); command(InputManager::Confirm);
    auto *keyboard=root->findChild<QObject *>("studioKeyboard"); QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("3"))));
    for(int i=0;i<3;++i)command(InputManager::NavigateDown);
    command(InputManager::Confirm);
    QCOMPARE(root->property("mode").toString(),QString("inspector"));
    QCOMPARE(session()->draft(),remote);
    QCOMPARE(home.committedDocument(),baseline);
}
void HomeNavigationTests::studioPreservesLegacyGridMetadataWithoutConversion()
{
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry); InputManager input;
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json"),user=dir.filePath("home.json");
    QVERIFY(home.load(defaults,user));
    const auto baseline=QJsonDocument::fromJson(home.committedDocument()).object();
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    for(int placedCount : {0,1,16}) {
        auto document=baseline;
        document["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",placedCount!=16},{"grid",QJsonObject{{"columns",8},{"rows",5}}}};
        if(placedCount) {
            auto entries=document["components"].toArray();
            for(int i=0;i<qMin(placedCount,int(entries.size()));++i){auto entry=entries[i].toObject();entry["placement"]=QJsonObject{{"x",i%8},{"y",i/8},{"width",1},{"height",1}};entries[i]=entry;}document["components"]=entries;
        }
        const auto original=QJsonDocument(document).toJson();
        QFile file(user);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(original),original.size());file.close();
        QVERIFY(home.load(defaults,user));
        std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));
        QVERIFY2(root,qPrintable(component.errorString()));
        QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
        QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("clock"))));QTest::qWait(40);
        QVERIFY(!root->property("gridMode").toBool());
        auto *preview=root->findChild<QQuickItem *>("studioPreview");QVERIFY(preview);
        auto *composed=preview->findChild<QObject *>("composedHome");QVERIFY(composed);
        if(placedCount==16)QVERIFY(!composed->property("visible").toBool());else QVERIFY(!composed->property("placementMode").toBool());
        QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,InputManager::NavigateRight)));
        QCoreApplication::processEvents();QVERIFY(!root->property("toolbarFocus").toBool());
        auto rectValue=preview->property("editRect");
        const auto rect=(rectValue.metaType().id()==qMetaTypeId<QJSValue>()?rectValue.value<QJSValue>().toVariant():rectValue).toMap();
        QVERIFY(rect["width"].toDouble()>0);QVERIFY(rect["height"].toDouble()>0);
        QSignalSpy activated(preview,SIGNAL(actionActivated(QVariant,QString)));QVERIFY(activated.isValid());
        const auto center=preview->mapToScene(QPointF(rect["x"].toDouble()+rect["width"].toDouble()/2,rect["y"].toDouble()+rect["height"].toDouble()/2));
        const auto targetValue=preview->property("editTarget");
        auto *target=qobject_cast<QQuickItem *>(targetValue.metaType().id()==qMetaTypeId<QJSValue>()?targetValue.value<QJSValue>().toQObject():targetValue.value<QObject *>());QVERIFY(target);
        const auto actualCenter=target->mapToScene(QPointF(target->width()/2,target->height()/2));
        QVERIFY2(QLineF(center,actualCenter).length()<1,qPrintable(QString("Outline %1,%2 differs from %3 center %4,%5").arg(center.x()).arg(center.y()).arg(target->objectName()).arg(actualCenter.x()).arg(actualCenter.y())));
        QVERIFY2(window.contentItem()->contains(center),qPrintable(QString("Legacy selection outside viewport: %1,%2").arg(center.x()).arg(center.y())));
        QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,center.toPoint());
        QTRY_COMPARE(root->property("mode").toString(),QString("context"));QCOMPARE(root->property("selectedId").toString(),QString("clock"));QCOMPARE(activated.count(),0);
        QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,InputManager::Back)));
        QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(root->property("selectedId").toString(),QString("clock"));
        QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,InputManager::Back)));
        QVERIFY(QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,InputManager::Back)));
        QVERIFY(!root->property("open").toBool());
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
    }
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY(session->execute("convertGrid",{{"value",QVariantMap{{"columns",8},{"rows",5}}}}));QVERIFY(session->apply());
    auto combined=home.items();combined.append(QVariantMap{{"id","transient-notice"},{"name","Notice"}});
    QQmlComponent runtime(&engine,QUrl::fromLocalFile(QStringLiteral(HOME_QML)));
    std::unique_ptr<QObject> root(runtime.createWithInitialProperties({{"items",combined},{"authoredItems",home.items()},{"sections",home.sections()},
        {"composition",home.composition()},{"grid",home.grid()},{"placements",home.placements()},{"appearance",home.appearance()},
        {"brandVisible",false},{"backgroundStart",QColor(home.backgroundStart())},{"backgroundEnd",QColor(home.backgroundEnd())},{"tileRoundness",0.18},
        {"internetAvailable",false},{"localNetworkAvailable",false},{"hasContextualSources",true},{"width",1280},{"height",720}}));
    QVERIFY2(root,qPrintable(runtime.errorString()));
    auto *composed=root->findChild<QObject *>("composedHome");QVERIFY(composed);
    QVERIFY(composed->property("placementMode").toBool());
    const auto value=composed->property("placedItems");
    const auto rendered=value.metaType().id()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();
    QCOMPARE(rendered.size(),home.items().size());
}
void HomeNavigationTests::studioNestedGroupCanvasAndRemoteScope()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    auto *setup=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(setup);
    auto doc=QJsonDocument::fromJson(setup->draft()).object();doc["components"]=QJsonArray{};doc["categories"]=QJsonArray{};
    doc["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject::fromVariantMap(home.studioOptions()["defaultGrid"].toMap())}};
    QVERIFY(setup->pushEdit(QJsonDocument(doc).toJson()));QVERIFY(setup->execute("addResolved",{{"type","input-panel"},{"placement",QVariantMap{{"x",0},{"y",0},{"width",4},{"height",3}}}}));QVERIFY(setup->apply());
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(HOME_QML)).dir().filePath("PageStudioView.qml")));QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,QString()),Q_ARG(QVariant,QString("inputs-panel"))));
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);const auto baseline=session->draft();
    auto key=[&](int value){return QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value));};
    std::function<QQuickItem *(QQuickItem *,const QString &)> find=[&](QQuickItem *parent,const QString &name)->QQuickItem *{if(parent->objectName()==name)return parent;for(auto *child:parent->childItems())if(auto *result=find(child,name))return result;return nullptr;};
    auto *visual=qobject_cast<QQuickItem *>(root.get());QTRY_VERIFY(find(visual,"placed-input-8"));auto *last=find(visual,"placed-input-8");QVERIFY(last->width()>0);QVERIFY(last->height()>0);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"context",Q_ARG(QVariant,14)));QCOMPARE(root->property("parentScopeId").toString(),QString("inputs-panel"));QCOMPARE(root->property("selectedId").toString(),QString("input-1"));
    QTRY_VERIFY(find(visual,"studioSelectionName-input-1"));QVERIFY(!find(visual,"studioSelectionName-input-1")->isVisible());QVERIFY(root->property("footerText").toString().contains("HDMI 1"));
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("move"));QVERIFY(key(InputManager::NavigateRight));QVERIFY(key(InputManager::Back));QCOMPARE(session->draft(),baseline);QCOMPARE(session->changeCount(),0);
    QVERIFY(key(InputManager::Back));QCOMPARE(root->property("selectedId").toString(),QString("inputs-panel"));QCOMPARE(root->property("parentScopeId").toString(),QString());QVERIFY(!session->isDirty());
    root->setProperty("mode","view");root->setProperty("choice",0);QVERIFY(key(InputManager::Confirm));QCOMPARE(session->draft(),baseline);QCOMPARE(session->changeCount(),0);
    root->setProperty("choice",6);QVERIFY(key(InputManager::Confirm));QCOMPARE(session->draft(),baseline);QVERIFY(key(InputManager::Back));
    const QVariantMap narrowPlacement{{"x",0},{"y",0},{"width",1},{"height",3}};
    const QVariantList narrowChanges{QVariantMap{{"id","inputs-panel"},{"placement",narrowPlacement}}};
    QVERIFY(session->execute("place",{{"placements",narrowChanges}}));
    const auto panel=QJsonDocument::fromJson(session->draft()).object()["components"].toArray().first().toObject();auto internal=panel["presentation"].toObject()["layout"].toObject().toVariantMap();internal["gap"]=100;
    QVERIFY(session->execute("childLayout",{{"id","inputs-panel"},{"value",internal}}));
    auto *preview=root->findChild<QObject *>("studioPreview");QVERIFY(preview);QTRY_VERIFY(!preview->property("layoutError").toString().isEmpty());
    QVERIFY(session->undo());QVERIFY(session->undo());QCOMPARE(session->draft(),baseline);QTRY_VERIFY(preview->property("layoutError").toString().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(root.get(),"close"));
    auto *named=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(named);auto interface=QJsonDocument::fromJson(named->draft()).object();auto page=interface;
    for(const auto &key:QStringList{"pages","homePageId","rootPage","studioAppearance"})page.remove(key);
    interface["pages"]=QJsonArray{QJsonObject{{"id","nested-page"},{"name","Nested page"},{"layout",page}}};
    QVERIFY2(named->pushEdit(QJsonDocument(interface).toJson()),qPrintable(named->errorString()));QVERIFY(named->apply());
    QQmlComponent runtime(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(HOME_QML)).dir().filePath("PageSurface.qml")));QVERIFY2(runtime.isReady(),qPrintable(runtime.errorString()));
    std::unique_ptr<QObject> surface(runtime.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"provider",QVariant::fromValue<QObject *>(nullptr)},{"availableApps",QVariantList{}},{"artwork",QVariantList{}},{"themes",QVariantList{}},{"width",1280},{"height",720}}));QVERIFY2(surface,qPrintable(runtime.errorString()));
    qobject_cast<QQuickItem *>(surface.get())->setParentItem(window.contentItem());
    QVERIFY(QMetaObject::invokeMethod(surface.get(),"showPage",Q_ARG(QVariant,QString("nested-page")),Q_ARG(QVariant,QString()),Q_ARG(QVariant,true)));
    auto *content=surface->findChild<QObject *>("pageContent");QVERIFY(content);
    QVERIFY(QMetaObject::invokeMethod(surface.get(),"handleCommand",Q_ARG(QVariant,InputManager::Confirm)));QVERIFY(content->property("groupActive").toBool());
    QVERIFY(QMetaObject::invokeMethod(surface.get(),"handleCommand",Q_ARG(QVariant,InputManager::Back)));QVERIFY(!content->property("groupActive").toBool());QCOMPARE(surface->property("activeId").toString(),QString("nested-page"));QCOMPARE(content->property("focusedItemId").toString(),QString("inputs-panel"));
}
QTEST_MAIN(HomeNavigationTests)
#include "tst_home_navigation.moc"

void HomeNavigationTests::studioCatalogAndContextualInspector()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);InputManager input;
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    const auto pageId=home.createPage("Canvas","blank");QVERIFY(!pageId.isEmpty());
    auto *createdPage=qobject_cast<PageModel *>(home.pageModel(pageId));QVERIFY(createdPage);QVERIFY(createdPage->setGrid({{"columns",8},{"rows",5},{"mode","screen"}}));
    const auto disk=home.committedDocument();
    QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);engine.rootContext()->setContextProperty("isoldeVersion","test");
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(QStringLiteral(CUSTOMIZE_QML)).dir().filePath("PageStudioView.qml")));
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.createWithInitialProperties({{"layoutModel",QVariant::fromValue(&home)},{"width",1280},{"height",720}}));QVERIFY(root);
    QQuickWindow window;window.resize(1280,720);qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());window.show();
    QVERIFY(QMetaObject::invokeMethod(root.get(),"show",Q_ARG(QVariant,pageId),Q_ARG(QVariant,QString())));
    auto key=[&](int c){const auto ok=QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,c));QCoreApplication::processEvents();return ok;};
    auto *session=qobject_cast<StudioSession *>(root->property("session").value<QObject *>());QVERIFY(session);const auto baseline=session->draft();
    auto *catalog=root->findChild<QObject *>("studioCatalog"),*inspector=root->findChild<QObject *>("studioInspector");QVERIFY(catalog);QVERIFY(inspector);
    auto list=[](const QVariant &v){return (v.metaType().id()==qMetaTypeId<QJSValue>()?v.value<QJSValue>().toVariant():v).toList();};
    QVERIFY(session->execute("newPage",{{"name","Destination"},{"preset","blank"}}));QCoreApplication::processEvents();
    const auto namedActions=list(catalog->property("actions"));QVERIFY(std::any_of(namedActions.cbegin(),namedActions.cend(),[](const QVariant &v){return v.toMap()["name"].toString().contains("Destination");}));QVERIFY(session->undo());QCOMPARE(session->draft(),baseline);
    QVERIFY(key(InputManager::Search));QCOMPARE(root->property("mode").toString(),QString("catalog"));QCOMPARE(list(catalog->property("families")).size(),10);
    // Inspect every family's content step and cancel without writes/history.
    for(int i=0;i<6;++i){catalog->setProperty("selected",i);QVERIFY(key(InputManager::Confirm));QVERIFY(catalog->property("view").toString()!="families");QVERIFY(key(InputManager::Back));QCOMPARE(session->draft(),baseline);}
    QVERIFY(key(InputManager::Confirm));QCOMPARE(catalog->property("view").toString(),QString("links"));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("move"));
    QCOMPARE(session->draft(),baseline);QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("canvas"));QCOMPARE(session->changeCount(),1);
    const auto selected=root->property("selectedId").toString();QVERIFY(!selected.isEmpty());
    QVERIFY(key(InputManager::Confirm));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm));QCOMPARE(root->property("mode").toString(),QString("inspector"));
    if(inspector->property("collapsed").toBool())QVERIFY(key(InputManager::Confirm));
    QVERIFY(key(InputManager::Confirm));QCOMPARE(inspector->property("view").toString(),QString("fields"));
    QVERIFY(key(InputManager::Confirm));QCOMPARE(inspector->property("view").toString(),QString("keyboard"));auto *keyboard=inspector->findChild<QObject *>("inspectorKeyboard");QVERIFY(keyboard);
    QVERIFY(QMetaObject::invokeMethod(keyboard,"accepted",Q_ARG(QString,QString("My button"))));QCoreApplication::processEvents();QCOMPARE(inspector->property("view").toString(),QString("fields"));
    auto *model=qobject_cast<HomeLayout *>(session->model());auto *page=qobject_cast<PageModel *>(model->pageModel(pageId));QVERIFY(page);QCOMPARE(page->items()[0].toMap()["name"].toString(),QString("My button"));
    QVERIFY(key(InputManager::Back));QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::Confirm)); // Appearance
    const auto fields=list(inspector->property("fields"));int opacity=-1;for(int i=0;i<fields.size();++i)if(fields[i].toMap()["key"]=="surfaceOpacity")opacity=i;QVERIFY(opacity>=0);
    const auto opacityBaseline=session->draft();
    for(int i=0;i<opacity;++i)QVERIFY(key(InputManager::NavigateDown));QVERIFY(key(InputManager::NavigateLeft));
    QCOMPARE(inspector->property("view").toString(),QString("scalar"));
    QCOMPARE(session->draft(),opacityBaseline);
    inspector->setProperty("scalarControl",2);QVERIFY(key(InputManager::Confirm));
    QCOMPARE(page->items()[0].toMap()["presentation"].toMap()["surfaceOpacity"].toDouble(),0.95);
    QVERIFY(key(InputManager::Menu));QVERIFY(!page->items()[0].toMap()["presentation"].toMap().contains("surfaceOpacity"));
    QVERIFY(key(InputManager::Back));QVERIFY(key(InputManager::Back));QCOMPARE(root->property("selectedId").toString(),selected);QCOMPARE(home.committedDocument(),disk);
    while(session->canUndo())QVERIFY(session->undo());QCOMPARE(session->draft(),baseline);
    std::function<QQuickItem *(QQuickItem *,const QString &)> find=[&](QQuickItem *item,const QString &name)->QQuickItem *{if(item->objectName()==name)return item;for(auto *child:item->childItems())if(auto *match=find(child,name))return match;return nullptr;};
    for(const auto &family:QStringList{"button","image","library","carousel","app-bar"}){
        const auto prototype=session->componentPrototype(family);QVERIFY(!prototype.isEmpty());
        QVERIFY2(session->execute("addResolved",{{"type",family},{"placement",prototype["root"].toMap()["placement"]}},pageId),qPrintable(session->errorString()));
        QCoreApplication::processEvents();QTRY_VERIFY(find(qobject_cast<QQuickItem *>(root.get()),"placed-"+prototype["rootId"].toString()));
        QVERIFY(session->undo());QCOMPARE(session->draft(),baseline);
    }
}
