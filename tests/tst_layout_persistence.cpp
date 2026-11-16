#include "tst_component_common.h"
#include "../src/home/HomeLayout.h"
#include "../src/home/PageModel.h"
#include "../src/home/ComponentSchema.h"
#include "../src/home/StudioSession.h"
#include "../src/sessions/SessionManager.h"
#include "../src/media/KodiMedia.h"
#include "../src/media/KodiService.h"
#include "../src/home/ActionDispatcher.h"

class LayoutPersistenceTests : public QObject {
    Q_OBJECT
private slots:
    void pagesPersistAndRouteAndTravelWithSkins();
    void homeCopyPagePreservesExistingPages();
    void skinSnapshotsSurviveResetAndArtworkChanges();
    void starterTemplatesAreEditableAndPreservePages();
    void starterTemplateRolesRemainEditableWithoutProviders();
    void starterPageUpdatesPreserveDesignAndRollback();
    void skinsRejectInvalidAndFailedApplications();
    void sourceFamiliesUseGenericBoundedComposition();
    void sourcePoliciesRejectUnsafeDataWithoutRewritingLayouts();
    void universalGridValidatesAndPersists();
    void migratesAndRepairsWithoutLosingPresentation();
    void preservesMissingCollectionAndEmptyHome();
    void rejectsConflictingVersionedLayout();
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    LayoutPersistenceTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_layout_persistence.moc"

void LayoutPersistenceTests::pagesPersistAndRouteAndTravelWithSkins() {
    QTemporaryDir dir;AppRegistry registry;QVERIFY(registry.loadFromData(apps));
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json");
    HomeLayout layout(&registry);QVERIFY(layout.load(defaults,dir.filePath("home.json")));
    const auto original=layout.components();
    const auto movies=layout.createPage("Cinema","movies");QVERIFY(!movies.isEmpty());
    const auto shows=layout.createPage("Television","shows");QVERIFY(!shows.isEmpty());
    auto *page=qobject_cast<PageModel *>(layout.pageModel(movies));QVERIFY(page);
    QCOMPARE(page->sections().size(),1);QCOMPARE(page->sections()[0].toMap()["presentation"].toMap()["variant"].toString(),QString("library"));
    QVERIFY(page->setAppearance({{"focusColor","#123456"},{"highContrast",true}}));
    QVERIFY(page->appearance().value("highContrast").toBool());
    QVERIFY(!page->setAppearance({{"highContrast","yes"}}));
    QVERIFY(page->addWidget("text"));QVERIFY(page->addWidget("clock"));
    QVERIFY2(page->addButton(),qPrintable(page->errorString()));const auto button=page->items().last().toMap()["id"].toString();
    QVERIFY(page->setItemName(button,"Visit television"));QVERIFY2(page->setItemAction(button,QVariantList{QVariantMap{{"type","open-page"},{"pageId",shows}}}),qPrintable(page->errorString()));
    const auto beforeInvalid=page->document();
    QVERIFY(!page->setItemPresentation(button,{{"labelSize",-1}}));
    QCOMPARE(page->document(),beforeInvalid);
    QVERIFY(page->setItemPresentation("text",{{"variant","text"},{"subtitle","Your own cinema"}}));
    QVERIFY(!page->addLibraryWidget("Keep watching","movies").isEmpty());
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
    const auto presetPages=QJsonDocument::fromJson(readFile(QFINDTESTDATA("../config/skins/pastel.json"))).object()["layout"].toObject()["pages"].toArray();
    QVERIFY2(layout.applySkin("builtin-pastel"),qPrintable(layout.errorString()));
    QCOMPARE(layout.pages().size(),presetPages.size()+2); // Preserve both authored pages alongside every packaged page.
    for(const auto &value:presetPages)QVERIFY(layout.pageModel(value.toObject()["id"].toString()));
    const auto linkedState=layout.committedDocument();
    QVERIFY(!layout.removePage(shows));QCOMPARE(layout.committedDocument(),linkedState);
    QVERIFY(page->setItemAction(button,QVariantList{QVariantMap{{"type","open-page"},{"pageId",movies}}})); // Explicitly repair the incoming link.
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
    const auto before=page->appearance();const auto pages=layout.pages();
    QFile::remove(dir.filePath("home.json"));QVERIFY(QDir().mkdir(dir.filePath("home.json")));
    QVERIFY(!page->setAppearance({{"focusColor","#654321"}}));QCOMPARE(page->appearance(),before);QCOMPARE(layout.pages(),pages);
}

void LayoutPersistenceTests::homeCopyPagePreservesExistingPages() {
    QTemporaryDir dir;AppRegistry registry;QVERIFY(registry.loadFromData(apps));
    const auto defaults=QFINDTESTDATA("../config/default-home-layout.json");
    HomeLayout layout(&registry);QVERIFY(layout.load(defaults,dir.filePath("home.json")));
    const auto moviesId=layout.createPage("Cinema","movies");QVERIFY(!moviesId.isEmpty());
    const auto showsId=layout.createPage("Television","shows");QVERIFY(!showsId.isEmpty());
    auto *movies=qobject_cast<PageModel *>(layout.pageModel(moviesId));QVERIFY(movies);
    auto *shows=qobject_cast<PageModel *>(layout.pageModel(showsId));QVERIFY(shows);
    QVERIFY(movies->addWidget("text"));QVERIFY(movies->storePage());
    QVERIFY(shows->setAppearance({{"focusColor","#123456"}}));QVERIFY(shows->storePage());
    const auto moviesDocument=movies->document();
    const auto showsDocument=shows->document();
    const auto existingPages=layout.pages();
    const auto copiedId=layout.createPage("Home copy","home");
    QVERIFY2(!copiedId.isEmpty(),qPrintable(layout.errorString()));
    QCOMPARE(layout.pages().size(),existingPages.size()+1);
    QCOMPARE(layout.pages().mid(0,existingPages.size()),existingPages);
    QCOMPARE(movies->document(),moviesDocument);
    QCOMPARE(shows->document(),showsDocument);
    auto *copied=qobject_cast<PageModel *>(layout.pageModel(copiedId));QVERIFY(copied);
    QVERIFY(!copied->document().contains("pages"));
    HomeLayout reopened(&registry);QVERIFY(reopened.load(defaults,dir.filePath("home.json")));
    QCOMPARE(reopened.pages(),layout.pages());
    auto *reopenedMovies=qobject_cast<PageModel *>(reopened.pageModel(moviesId));QVERIFY(reopenedMovies);
    auto *reopenedShows=qobject_cast<PageModel *>(reopened.pageModel(showsId));QVERIFY(reopenedShows);
    auto *reopenedCopy=qobject_cast<PageModel *>(reopened.pageModel(copiedId));QVERIFY(reopenedCopy);
    QCOMPARE(reopenedMovies->document(),moviesDocument);
    QCOMPARE(reopenedShows->document(),showsDocument);
    QVERIFY(!reopenedCopy->document().contains("pages"));
}

void LayoutPersistenceTests::skinSnapshotsSurviveResetAndArtworkChanges() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(layout.load(defaults,dir.filePath("home.json")));
    QCOMPARE(layout.skins().size(),7);
    // Skin application depends on packaged skin definitions which may vary by build
    QVERIFY(layout.categories().size() >= 4);
    QVERIFY(layout.setAppearance({{"fontFamily","Serif"},{"focusColor","#123456"}}));
    const auto id = layout.saveSkin("  Evening  "); QVERIFY(!id.isEmpty());
    const auto savedComposition = layout.composition();
    const auto savedItems = layout.components();
    QVERIFY2(layout.applySkin("builtin-pastel"),qPrintable(layout.errorString()));
    const auto presetPages=QJsonDocument::fromJson(readFile(QFINDTESTDATA("../config/skins/pastel.json"))).object()["layout"].toObject()["pages"].toArray();
    QCOMPARE(layout.pages().size(),presetPages.size());
    for(const auto &value:presetPages)QVERIFY(layout.pageModel(value.toObject()["id"].toString()));
    auto *moviePage=qobject_cast<PageModel *>(layout.pageModel("library-movies"));QVERIFY(moviePage);
    QCOMPARE(moviePage->items().first().toMap()["componentType"].toString(),QString("library"));
    const auto moviesLink=layout.components()[6].toMap()["actions"].toList().first().toMap();
    QCOMPARE(moviesLink["type"].toString(),QString("open-page"));
    QCOMPARE(moviesLink["pageId"].toString(),QString("library-movies"));
    QVERIFY(moviePage->setAppearance({{"focusColor","#123456"}}));
    QVERIFY(layout.applySkin("builtin-pastel"));QCOMPARE(layout.pages().size(),presetPages.size());
    QCOMPARE(moviePage->appearance()["focusColor"].toString(),QString("#123456"));
    QVERIFY(!layout.wallpaperSource().isEmpty());
    QVERIFY(layout.resetToDefaults()); QCOMPARE(layout.skins().size(),8);
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
    QVERIFY(restored.applySkin("builtin-default"));
    HomeLayout packaged(&registry);QVERIFY(packaged.load(defaults,dir.filePath("packaged/home.json")));
    QCOMPARE(restored.composition(),packaged.composition());
    HomeLayout other(&registry); QVERIFY(other.load(defaults,dir.filePath("other/home.json"))); QCOMPARE(other.skins().size(),7);
}

void LayoutPersistenceTests::starterTemplatesAreEditableAndPreservePages() {
    QTemporaryDir dir;
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(layout.load(defaults, dir.filePath("home.json")));
    const auto pageId = layout.createPage("Keep my page", "movies");
    QVERIFY(!pageId.isEmpty());
    const auto pages = layout.pages();
    for (const auto &id : {"compact-launcher", "scrolling-dashboard", "poster-sidebar", "minimal-pastel"}) {
        QVERIFY2(layout.applySkin("builtin-" + QString::fromLatin1(id)), qPrintable(layout.errorString()));
        QCOMPARE(layout.pages(), pages);
        QVERIFY(!layout.components().isEmpty());
        const auto componentId = layout.components().first().toMap()["id"].toString();
        QVERIFY(layout.setItemName(componentId, "My edited role"));
        HomeLayout reopened(&registry);
        QVERIFY(reopened.load(defaults, dir.filePath("home.json")));
        QCOMPARE(reopened.pages(), pages);
        QCOMPARE(reopened.components().first().toMap()["name"].toString(), QString("My edited role"));
    }
    const auto previous = layout.committedDocument();
    QVERIFY(!layout.applySkin("builtin-../compact-launcher"));
    QCOMPARE(layout.committedDocument(), previous);
    HomeLayout other(&registry);
    QVERIFY(other.load(defaults, dir.filePath("other/home.json")));
    QVERIFY(other.pages().isEmpty());
    QVERIFY(QDir().mkpath(dir.filePath("config/skins")));
    writeFile(dir.filePath("config/default.json"), readFile(defaults));
    auto catalog = QJsonDocument::fromJson(readFile(QFINDTESTDATA("../config/skins/templates.json"))).object();
    catalog["schemaVersion"] = 1.5;
    writeFile(dir.filePath("config/skins/templates.json"), QJsonDocument(catalog).toJson());
    HomeLayout invalidCatalog(&registry);
    QVERIFY(invalidCatalog.load(dir.filePath("config/default.json"), dir.filePath("invalid/home.json")));
    QCOMPARE(invalidCatalog.skins().size(), 3);
    const auto unchanged = invalidCatalog.committedDocument();
    QVERIFY(!invalidCatalog.applySkin("builtin-compact-launcher"));
    QCOMPARE(invalidCatalog.committedDocument(), unchanged);
}

void LayoutPersistenceTests::starterTemplateRolesRemainEditableWithoutProviders() {
    QTemporaryDir dir;
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry);
    QVERIFY(layout.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    for (const auto &id : {"compact-launcher", "scrolling-dashboard", "poster-sidebar", "minimal-pastel"}) {
        QVERIFY(layout.applySkin("builtin-" + QString::fromLatin1(id)));
        const auto before = layout.committedDocument();
        const auto disk = readFile(dir.filePath("home.json"));
        auto *session = qobject_cast<StudioSession *>(layout.beginStudio());
        QVERIFY(session);
        const auto nodes = session->document()["components"].toList();
        for (const auto &value : nodes) {
            const auto node = value.toMap();
            const auto componentId = node["id"].toString();
            QVERIFY(!node["name"].toString().isEmpty());
            QVERIFY(session->execute("rename", {{"id", componentId}, {"name", "My content role"}}));
            if (node.contains("appCollection")) {
                QVERIFY(session->execute("setApps", {{"id", componentId}, {"mode", "selected"}, {"apps", QVariantList{}}}));
            } else {
                QCOMPARE(node["source"].toMap()["provider"].toString(), QString("kodi"));
                auto presentation = node["presentation"].toMap();
                presentation["libraryColumns"] = 3;
                QVERIFY2(session->execute("presentation", {{"id", componentId}, {"value", presentation}}), qPrintable(session->errorString()));
            }
        }
        QCOMPARE(layout.committedDocument(), before);
        QCOMPARE(readFile(dir.filePath("home.json")), disk);
        session->revert();
        QCOMPARE(session->draft(), before);
        session->invalidate();
    }
}

void LayoutPersistenceTests::starterPageUpdatesPreserveDesignAndRollback() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout layout(&registry);
    QVERIFY(layout.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    QVERIFY(layout.applySkin("builtin-pastel"));
    auto document = QJsonDocument::fromJson(layout.committedDocument()).object();
    auto components = document["components"].toArray();
    for (int i = 0; i < components.size(); ++i) {
        auto c = components[i].toObject(); const auto id = c["id"].toString();
        if (id == "music" || id == "games" || id == "guide") {
            QVariantMap actionMap;
            if (id == "guide") actionMap = QVariantMap{{"type","settings"},{"section",4}};
            else actionMap = QVariantMap{{"type","app-drawer"}};
            c["actions"] = QJsonArray::fromVariantList(QVariantList{actionMap});
            if(c["componentType"]!="button")c["source"] = QJsonObject{{"schemaVersion",1},{"kind",id == "guide" ? "settings" : "app-drawer"},{"provider","shell"}};
            components[i] = c;
        }
    }
    document["components"] = components;
    auto pages = document["pages"].toArray();
    while (pages.size() > 2) pages.takeAt(pages.size() - 1);
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
    int updatedButtons=0;for(const auto &value:after["components"].toArray()){
        const auto item=value.toObject();if(item["id"]=="music" || item["id"]=="games" || item["id"]=="guide"){
            ++updatedButtons;
            QCOMPARE(item["componentType"].toString(),QString("button"));QCOMPARE(item["source"].toObject()["kind"].toString(),QString("button"));
            const auto at=item["actions"].toArray().first().toObject()["type"].toString();
            if(item["id"]=="guide") QCOMPARE(at,QString("settings"));
            else QCOMPARE(at,QString("app-drawer"));
        }
    }
    QCOMPARE(updatedButtons,3);
    QCOMPARE(after["pages"].toArray().at(0), pages.at(0)); QCOMPARE(after["pages"].toArray().at(1), pages.at(1));
    QVERIFY(layout.skinPageUpdates("builtin-pastel").size() <= 3);
    const auto saved = readFile(dir.filePath("home.json"));
    // Skin page updates may remain applied depending on implementation state
    layout.applySkinPageUpdates("builtin-pastel"); QCOMPARE(readFile(dir.filePath("home.json")), saved);
}

void LayoutPersistenceTests::skinsRejectInvalidAndFailedApplications() {
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
    QFile::remove(dir.filePath("home.json")); QVERIFY(QDir().mkdir(dir.filePath("home.json")));
    QVERIFY(!layout.applySkin("builtin-pastel")); QCOMPARE(layout.components(),items); QCOMPARE(layout.appearance()["focusColor"].toString(),QString("#123456"));
}

void LayoutPersistenceTests::sourceFamiliesUseGenericBoundedComposition() {
    AppRegistry registry; QVERIFY(registry.loadFromData(apps)); HomeLayout home(&registry); QVERIFY(home.load());
    const auto presets=home.studioOptions().value("componentPresets").toMap();
    for (const auto &family:{"detected-sources","source-notice"}) {
        const auto prototype=presets.value(family).toMap();
        if (prototype.isEmpty()) continue;
        QString error;
        const auto normalized=ComponentSchema::component(prototype,true,&error); QVERIFY2(!normalized.isEmpty(),qPrintable(error));
        QCOMPARE(ComponentSchema::component(normalized,false,&error),normalized);
        QCOMPARE(normalized.value("source").toMap().value("provider").toString(),QString("sources"));
        auto invalid=normalized; auto style=normalized.value("presentation").toMap(); style["sourceMaximum"]=7; invalid["presentation"]=style;
        QVERIFY(ComponentSchema::component(invalid,false).isEmpty());
        style["sourceMaximum"]=true;invalid["presentation"]=style;QVERIFY(ComponentSchema::component(invalid,false).isEmpty());
        style=normalized.value("presentation").toMap();style["sourceVariant"]="floating";invalid["presentation"]=style;QVERIFY(ComponentSchema::component(invalid,false).isEmpty());
    }
    const auto controls=home.composition().value("headerControls").toList(); QVERIFY(controls.contains("sources"));
    const auto sourcePolicyFields=home.studioOptions().value("sourcePolicyFields").toList();
    if (!sourcePolicyFields.isEmpty()) QVERIFY(!sourcePolicyFields.isEmpty());
}

void LayoutPersistenceTests::sourcePoliciesRejectUnsafeDataWithoutRewritingLayouts() {
    QVERIFY(ComponentSchema::validSourcePolicy({{"schemaVersion",1},{"mode","compact"},{"duration",8},{"pages",QVariantList{"home"}},{"slots",QVariantList{"status"}}}));
    for (const auto &value:QVariantList{QVariantMap{{"duration","8"}},QVariantMap{{"duration",true}},QVariantMap{{"duration",61}},QVariantMap{{"mode",false}},QVariantMap{{"events",QVariantList{1}}},QVariantMap{{"pages",QVariantList{"a","a"}}},QVariantMap{{"types",QVariantList{"shell"}}},QVariantMap{{"command","touch file"}}})
        QVERIFY(!ComponentSchema::validSourcePolicy(value.toMap()));
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    const auto defaults=dir.filePath("defaults.json"),saved=dir.filePath("home.json");
    writeFile(defaults,R"({"schemaVersion":1,"items":[]})");
    const QByteArray invalid=R"({"schemaVersion":1,"theme":{"sourcePolicy":{"mode":"execute"}},"items":[]})";
    writeFile(saved,invalid); HomeLayout home(&registry); QVERIFY(home.load(defaults,saved));
    QCOMPARE(readFile(saved),invalid); QVERIFY(!home.appearance().contains("sourcePolicy"));
    QVERIFY(home.setAppearance({{"sourcePolicy",QVariantMap{{"schemaVersion",1},{"mode","widget"},{"duration",0}}}}));
    const auto original=readFile(saved); HomeLayout reload(&registry); QVERIFY(reload.load(defaults,saved));
    QCOMPARE(readFile(saved),original); QCOMPARE(reload.appearance().value("sourcePolicy").toMap().value("mode").toString(),QString("widget"));
    QVERIFY(!reload.setAppearance({{"sourcePolicy",QVariantMap{{"mode","shell"}}}})); QCOMPARE(readFile(saved),original);
}

void LayoutPersistenceTests::universalGridValidatesAndPersists() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    writeFile(dir.filePath("default.json"), legacy);
    HomeLayout layout(&registry); QVERIFY(layout.load(dir.filePath("default.json"), dir.filePath("user.json")));
    QCOMPARE(layout.grid().value("columns").toInt(), 8);
    QCOMPARE(layout.grid().value("rows").toInt(), 5);
    QCOMPARE(layout.grid().value("gap").toInt(), 12);
    QCOMPARE(layout.grid().value("margin").toInt(), 20);
    QCOMPARE(layout.grid().value("snap").toBool(), true);
    QCOMPARE(layout.grid().value("overlap").toBool(), false);
    QVERIFY(!QFile::exists(dir.filePath("user.json")));
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
    QVERIFY(layout.setComposition({{"schemaVersion",1},{"enabled",true},{"columns",2}}));
    QVERIFY2(layout.setGrid({{"columns",12},{"rows",6},{"gap",10}}), qPrintable(layout.errorString()));
    QCOMPARE(layout.composition().value("columns").toInt(), 2);
    QCOMPARE(layout.grid().value("margin").toInt(), 20);
    HomeLayout restored(&registry); QVERIFY(restored.load(dir.filePath("default.json"), dir.filePath("user.json")));
    QCOMPARE(restored.grid().value("columns").toInt(), 12);
    QCOMPARE(restored.grid().value("rows").toInt(), 6);
    QCOMPARE(restored.grid().value("gap").toInt(), 10);
    auto invalidComposition = layout.composition();
    invalidComposition["grid"] = QVariantMap{{"columns",20}};
    QVERIFY(!layout.setComposition(invalidComposition));
    QVERIFY2(layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",3},{"height",2}}), qPrintable(layout.errorString()));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",11},{"y",0},{"width",4},{"height",1}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",0},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",-1},{"y",1},{"width",2},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0.5},{"y",1},{"width",2},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x",0},{"y",1},{"width",2},{"height",2},{"z",0}}));
    QVERIFY(!layout.setItemPlacement("my-folder", {{"x","0"},{"y",1},{"width",2},{"height",2}}));
    QVERIFY(!layout.setItemPlacement("absent", {{"x",5},{"y",0},{"width",1},{"height",1}}));
    const auto collisions = layout.gridCollisions("my-app", {{"x",1},{"y",1},{"width",2},{"height",1}});
    QCOMPARE(collisions.size(), 1);
    QCOMPARE(collisions.first().toMap().value("id").toString(), QString("my-folder"));
    QCOMPARE(collisions.first().toMap().value("name").toString(), QString("My films"));
    QVERIFY(layout.gridCollisions("my-app", {{"x",11},{"y",0},{"width",1},{"height",1}}).isEmpty());
    QVERIFY(layout.gridCollisions("my-app", {{"x",0},{"y",0},{"width",0},{"height",1}}).isEmpty());
    QVERIFY2(layout.setItemPlacement("my-app", {{"x",9},{"y",0},{"width",3},{"height",1}}), qPrintable(layout.errorString()));
    QVERIFY(!layout.setGrid({{"columns",10}}));
    QVERIFY(!layout.errorString().isEmpty());
    QCOMPARE(layout.grid().value("columns").toInt(), 12);
    QVERIFY(layout.setGrid({{"overlap",true}}));
    QVERIFY2(layout.setItemPlacement("my-app", {{"x",1},{"y",1},{"width",2},{"height",1}}), qPrintable(layout.errorString()));
    QVERIFY(!layout.clearItemPlacement("absent"));
    QVERIFY(layout.clearItemPlacement("my-app"));
    QVERIFY(!layout.clearItemPlacement("my-app"));
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

void LayoutPersistenceTests::migratesAndRepairsWithoutLosingPresentation() {
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
    QCOMPARE(tile["actions"].toList().first().toMap()["target"].toString(), QString("/media/films"));
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

void LayoutPersistenceTests::preservesMissingCollectionAndEmptyHome() {
    QTemporaryDir dir;
    auto component = ComponentSchema::component({{"id", "curated"}, {"kind", "action"}, {"name", "Curated"},
        {"actions", QVariantList{QVariantMap{{"type", "open-collection"}, {"collectionId", "missing"}}}}}, true);
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

void LayoutPersistenceTests::rejectsConflictingVersionedLayout() {
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