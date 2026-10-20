#include "input/InputManager.h"
#include "home/HomeLayout.h"
#include "media/KodiMedia.h"
#include "media/KodiArtwork.h"
#include "media/KodiService.h"
#include "home/PageData.h"
#include "home/PageModel.h"
#include "home/StudioSession.h"
#include "settings/SearchSettings.h"
#include "media/KodiImporter.h"
#include <QBuffer>
#include <QImage>
#include <functional>
#include "sessions/SessionManager.h"
#include <QLocalServer>
#include <QJsonDocument>
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
    void studioRemoteDraftApplyAndCancel();
    void repairsSourceWithRemoteAndCanCancel();
    void televisionLibraryRemoteFlow();
    void universalLocalSearchRemoteFlow();
    void pageScopedSourcesAppearInSearch();
    void kodiSettingsFollowUserTheme();
    void appearancePreviewCancelAndApply();
    void composedReferenceNavigationAndGeometry();
    void pastelReferenceNavigationAndGeometry();
    void skinsAndDesignAreReachableFromSettings();
    void pageComposerAndIndependentKodiWidgets();
    void gridEditorMoveResizeDuplicateRemove();
};
void HomeNavigationTests::pageComposerAndIndependentKodiWidgets()
{
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));
    int reads=0,plays=0;QJsonObject played;
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
            else {++reads;const auto key=method=="VideoLibrary.GetTVShows"?"tvshows":"movies";QJsonArray items;
                for(int i=1;i<=8;++i)items.append(QJsonObject{{key==QString("movies")?"movieid":"tvshowid",i},{"title",QString("Kodi title %1").arg(i)},{"year",2020+i},{"resume",QJsonObject{{"position",i*10},{"total",120}}}});
                result={{key,items},{"limits",QJsonObject{{"start",0},{"end",8},{"total",8}}}};}
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request["id"]},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });}});
    AppRegistry registry;HomeLayout home(&registry);QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    KodiMedia player("page-ui",nullptr,dir.path());PageData data("page-ui",&player,nullptr,dir.path());InputManager input;QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("homeLayout",&home);
    engine.rootContext()->setContextProperty("isoldeVersion","0.0.1.2");
    QQmlComponent settings(&engine,QUrl::fromLocalFile(QStringLiteral(CUSTOMIZE_QML)));
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
    QVERIFY(command(InputManager::Confirm));QVERIFY(command(InputManager::Confirm));QTRY_COMPARE(plays,1);QCOMPARE(played["movieid"].toInt(),4);QCOMPARE(played["resume"].toBool(),true);
    QVERIFY(QMetaObject::invokeMethod(root.get(),"restoreOrigin",Q_ARG(QVariant,QString("tab-original-home"))));
    auto *content=root->findChild<QObject *>("pageContent");QVERIFY(content);QVERIFY(!content->property("recoveryFocused").toBool());
    player.playbackControl("stop");QTRY_VERIFY(!player.playbackActive());
    QVERIFY(command(InputManager::Back)); // Leave widget without leaving page.
    QVERIFY(root->property("open").toBool());
    const auto another=home.createPage("Another","blank");QVERIFY(!another.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(root.get(),"showPage",Q_ARG(QVariant,another),Q_ARG(QVariant,QString("link")),Q_ARG(QVariant,false)));
    QVERIFY(command(InputManager::Back));QCOMPARE(root->property("activeId").toString(),pageId);
    QSignalSpy closed(root.get(),SIGNAL(closed(QString)));QVERIFY(command(InputManager::Home));QVERIFY(!root->property("open").toBool());QCOMPARE(closed.last()[0].toString(),QString("original-card"));
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
    QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(CUSTOMIZE_QML)));
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
    // Every presentation property used by either reference is represented in the structured editor.
    for (const auto &skin : {QString("builtin-lounge"),QString("builtin-pastel")}) {
        QVERIFY(home.applySkin(skin));
        for (int scope : {1,2}) {
            editor->setProperty("scope",scope);
            QSet<QString> keys;
            for (const auto &field : listValue(editor->property("fields"))) keys.insert(field.toMap()["key"].toString().section('.',0,0));
            const auto entries = scope == 1 ? home.categories() : home.components();
            for (const auto &entry : entries) {
                const auto style = entry.toMap()["presentation"].toMap();
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
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CUSTOMIZE_QML)));
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
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CUSTOMIZE_QML)));
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
        {"composition",home.composition()},{"appearance",home.appearance()},{"wallpaperSource",home.wallpaperSource()},
        {"brandVisible",false},{"backgroundStart",QColor(home.backgroundStart())},{"backgroundEnd",QColor(home.backgroundEnd())},{"tileRoundness",0.18},
        {"internetAvailable",true},{"localNetworkAvailable",true},{"hasContextualSources",false},{"width",1280},{"height",800}}));
    QVERIFY2(root,qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,800); qobject_cast<QQuickItem*>(root.get())->setParentItem(window.contentItem()); window.show();
    QTRY_VERIFY(root->property("hasWallpaper").toBool());
    auto *content = root->findChild<QQuickItem*>("composedContent"); QVERIFY(content);
    QVERIFY(content->property("contentHeight").toReal() <= content->height()+2);
    auto *composed = root->findChild<QObject*>("composedHome"); QVERIFY(composed);
    QSignalSpy search(root.get(),SIGNAL(searchRequested())), settings(root.get(),SIGNAL(settingsRequested(int)));
    QSignalSpy activated(root.get(),SIGNAL(actionActivated(QVariant,QString)));
    auto command = [&](int value) { return QMetaObject::invokeMethod(root.get(),"handleCommand",Q_ARG(QVariant,value)); };
    QCOMPARE(root->property("focusedItemId").toString(),QString("hdmi1"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("guide"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("disney"));
    QVERIFY(command(InputManager::NavigateDown)); QCOMPARE(root->property("focusedItemId").toString(),QString("challengers"));
    for(int i=0;i<4;i++) QVERIFY(command(InputManager::NavigateRight));
    QCOMPARE(composed->property("sectionActionId").toString(),QString("section-recent"));
    QVERIFY(command(InputManager::Confirm)); QCOMPARE(activated.last()[1].toString(),QString("section-recent"));
    QVERIFY(QMetaObject::invokeMethod(root.get(),"restoreFocus",Q_ARG(QVariant,QString("hdmi1"))));
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
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CUSTOMIZE_QML)));
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
    command(InputManager::Confirm); // Add on legacy page opens explicit conversion.
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
    command(InputManager::Confirm); // Clock ghost.
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
    QVERIFY(!root->property("open").toBool());
}
QTEST_MAIN(HomeNavigationTests)
#include "tst_home_navigation.moc"
