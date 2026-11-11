#include "home/HomeLayout.h"
#include "home/StudioSession.h"
#include "home/PageModel.h"
#include "home/DesignSystem.h"
#include "home/ComponentSchema.h"
#include "app/AppRegistry.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QUrl>

namespace {
QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }
bool write(const QString &path, const QByteArray &data) { QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size(); }
const QByteArray apps = R"([{"id":"installed-app","name":"Installed app","type":"web","url":"https://example.invalid"}])";
}
class StudioDesignTests : public QObject {
    Q_OBJECT
private slots:
    void pageAndInterfacePreviewsAreAtomicAndCancelable();
    void damagedGalleryThumbnailsRemainBoundedAndCannotChangeDraft();
    void portableRoundTripStagesArtworkAndRemapsMissingApps();
    void invalidPortablePackagesCannotMutateOrPublish();
    void referenceImagesArePrivateBoundedAndTornDown();
    void sharedStylesPreserveContentAndOverrides();
    void reusableGroupsPreviewPropagateDetachAndRollback();
    void responsiveRulesRemainDeclarativeAndBounded();
    void cachedPageStylesAndHomeTokensRefreshWithoutWrites();
    void editorViewportDefaultsRejectInvalidDimensionsWithoutWrites();
    void navigationOnlyReorderingAppearsInTheApplySummary();
    void appearancePasteAndIndependentArtworkCopiesKeepOwnership();
};
void StudioDesignTests::damagedGalleryThumbnailsRemainBoundedAndCannotChangeDraft()
{
    QTemporaryDir dir;AppRegistry registry;HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    const auto id=home.saveSkin("Damaged preview");QVERIFY(!id.isEmpty());
    const auto file=dir.filePath("skins/"+id+".json");
    auto package=QJsonDocument::fromJson(read(file)).object();QVERIFY(!package.isEmpty());
    auto layout=package["layout"].toObject(),theme=layout["theme"].toObject();theme["backgroundStart"]="invalid-color";layout["theme"]=theme;
    layout["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true},{"grid",QJsonObject{{"columns",4},{"rows",3}}}};
    layout["components"]=QJsonArray{QJsonObject{{"id","bad"},{"placement",QJsonObject{{"x","invalid"},{"y",0},{"width",1},{"height",1}}}}};package["layout"]=layout;
    QVERIFY(write(file,QJsonDocument(package).toJson()));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    const auto draft=session->draft(),disk=read(dir.filePath("home.json"));
    bool found=false;
    for(const auto &value:session->designChoices()){
        const auto entry=value.toMap();if(entry.value("id").toString()!=id)continue;
        found=true;const auto preview=entry.value("preview").toMap();
        QVERIFY(QColor(preview.value("background").toString()).isValid());
        QVERIFY(preview.value("blocks").toList().isEmpty());
    }
    QVERIFY(found);QVERIFY(session->previewDesign(id,"page").isEmpty());
    QCOMPARE(session->draft(),draft);QCOMPARE(read(dir.filePath("home.json")),disk);
}

void StudioDesignTests::pageAndInterfacePreviewsAreAtomicAndCancelable()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    QVERIFY(home.applySkin("builtin-compact-launcher"));
    const auto first = home.createPage("Cinema", "movies"), second = home.createPage("Television", "shows");
    QVERIFY(!first.isEmpty()); QVERIFY(!second.isEmpty());
    const auto applied = home.committedDocument(), disk = read(dir.filePath("home.json"));
    auto *session = qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(session);
    QVERIFY(session->execute("rename", {{"id", "main-apps"}, {"name", "Unapplied apps"}}));
    const auto before = session->draft(); const auto count = session->changeCount();
    const auto proposal = session->previewDesign("builtin-minimal-pastel", "page", second);
    QVERIFY2(proposal["valid"].toBool(), qPrintable(session->errorString()));
    QCOMPARE(proposal["changes"].toList().size(), 1);
    QCOMPARE(proposal["changes"].toList()[0].toMap()["name"].toString(), QString("Television"));
    QCOMPARE(session->draft(), before); QCOMPARE(home.committedDocument(), applied); QCOMPARE(read(dir.filePath("home.json")), disk);
    session->cancelPreview(); QVERIFY(!session->proposalModel()); QCOMPARE(session->draft(), before);
    QVERIFY(session->previewDesign("builtin-minimal-pastel", "page", second)["valid"].toBool());
    QVERIFY(session->acceptPreview()); QCOMPARE(session->changeCount(), count + 1);
    const auto changed = session->document();
    QCOMPARE(changed["pages"].toList()[0].toMap(), QJsonDocument::fromJson(before).object()["pages"].toArray()[0].toObject().toVariantMap());
    QVERIFY(session->undo()); QCOMPARE(session->draft(), before); QVERIFY(session->redo());
    QVERIFY(session->apply("main-apps")); QCOMPARE(home.pages().size(), 2);
    const auto saved = home.committedDocument();
    auto whole = session->previewDesign("builtin-scrolling-dashboard", "interface");
    QVERIFY2(whole["valid"].toBool(), qPrintable(session->errorString()));
    QCOMPARE(whole["changes"].toList().size(), 3); // Home replacement and two explicit removals.
    QCOMPARE(home.committedDocument(), saved);
    QVERIFY(session->acceptPreview()); QVERIFY(session->document()["pages"].toList().isEmpty());
    QVERIFY(session->undo()); QCOMPARE(session->draft(), saved);
    QVERIFY(session->previewDesign("builtin-compact-launcher", "page")["valid"].toBool());
    QVERIFY(session->previewDesign("builtin-../bad", "page").isEmpty()); QVERIFY(!session->proposalModel());
    QVERIFY(session->previewDesign("builtin-compact-launcher", "page", "missing-page").isEmpty());
    QCOMPARE(session->draft(), saved);
    // Navigation metadata must remain visible even when all page payloads match.
    const auto metadataPath = dir.filePath("metadata.json");
    QVERIFY(session->exportDesign(metadataPath, "interface"));
    auto package = QJsonDocument::fromJson(read(metadataPath)).object();
    auto layout = package["layout"].toObject();
    layout["homePageId"] = first;
    auto root = layout["rootPage"].toObject(); root["name"] = "Renamed Home"; layout["rootPage"] = root;
    package["layout"] = layout;
    QVERIFY(write(metadataPath, QJsonDocument(package).toJson()));
    const auto metadata = session->previewImport(metadataPath, "interface");
    QVERIFY2(metadata["valid"].toBool(), qPrintable(session->errorString()));
    const auto changes = metadata["changes"].toList();
    QCOMPARE(changes.size(), 2);
    QCOMPARE(changes[1].toMap()["name"].toString(), QString("Startup page"));
    session->cancelPreview(); QCOMPARE(session->draft(), saved);
    session->invalidate(); QVERIFY(session->previewDesign("builtin-compact-launcher", "page").isEmpty());
}
void StudioDesignTests::portableRoundTripStagesArtworkAndRemapsMissingApps()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    HomeLayout source(&registry); QVERIFY(source.load(defaults, dir.filePath("source/home.json")));
    QVERIFY(source.applySkin("builtin-minimal-pastel"));
    auto doc = QJsonDocument::fromJson(source.committedDocument()).object();
    auto nodes = doc["components"].toArray(); auto node = nodes[0].toObject();
    node["appCollection"] = QJsonObject{{"mode", "selected"}, {"ids", QJsonArray{"old-app"}}}; nodes[0] = node; doc["components"] = nodes;
    QVERIFY(source.applyStudioDraft(QJsonDocument(doc).toJson()));
    QImage image(24, 24, QImage::Format_RGB32); image.fill(Qt::blue);
    const auto imagePath = dir.filePath("wallpaper.png"); QVERIFY(image.save(imagePath)); QVERIFY(source.setWallpaper(QUrl::fromLocalFile(imagePath).toString()));
    const auto sourceBefore = source.committedDocument();
    const auto personalSkin = source.saveSkin("Keep this personal copy"); QVERIFY(!personalSkin.isEmpty());
    const auto skinBytes = read(dir.filePath("source/skins/" + personalSkin + ".json"));
    auto *exporter = qobject_cast<StudioSession *>(source.beginStudio()); QVERIFY(exporter);
    const auto packagePath = dir.filePath("interface.json"); QVERIFY2(exporter->exportDesign(packagePath, "interface"), qPrintable(exporter->errorString()));
    const auto package = read(packagePath); QVERIFY(!package.contains(dir.path().toUtf8())); QVERIFY(!package.contains("studioAppearance"));
    QVERIFY(!exporter->exportDesign(packagePath, "interface")); QCOMPARE(read(packagePath), package);
    HomeLayout target(&registry); QVERIFY(target.load(defaults, dir.filePath("target/home.json")));
    const auto keepPage = target.createPage("Keep", "movies"); QVERIFY(!keepPage.isEmpty());
    const auto targetBefore = target.committedDocument(); const auto targetPages = target.pages();
    auto *importer = qobject_cast<StudioSession *>(target.beginStudio()); QVERIFY(importer);
    auto summary = importer->previewImport(packagePath, "page");
    QVERIFY2(summary["valid"].toBool(), qPrintable(importer->errorString()));
    QVERIFY(summary["missingApps"].toStringList().contains("old-app"));
    QCOMPARE(target.committedDocument(), targetBefore); QVERIFY(!QFileInfo::exists(dir.filePath("target/artwork")));
    importer->cancelPreview(); QCOMPARE(importer->draft(), targetBefore);
    summary = importer->previewImport(packagePath, "page", {}, {{"old-app", "installed-app"}});
    QVERIFY2(summary["valid"].toBool(), qPrintable(importer->errorString())); QVERIFY(summary["missingApps"].toStringList().isEmpty());
    QVERIFY(importer->acceptPreview()); QCOMPARE(importer->changeCount(), 1);
    QCOMPARE(importer->document()["pages"].toList(), targetPages);
    const auto draft = importer->draft();
    QVERIFY(QFile::rename(dir.filePath("target/home.json"), dir.filePath("target/home.held"))); QVERIFY(QDir().mkdir(dir.filePath("target/home.json")));
    QVERIFY(!importer->apply()); QCOMPARE(importer->draft(), draft); QCOMPARE(target.committedDocument(), targetBefore);
    QVERIFY(!QFileInfo::exists(dir.filePath("target/artwork")));
    QVERIFY(QDir().rmdir(dir.filePath("target/home.json"))); QVERIFY(QFile::rename(dir.filePath("target/home.held"), dir.filePath("target/home.json")));
    QVERIFY2(importer->apply(), qPrintable(importer->errorString())); QCOMPARE(target.pages(), targetPages);
    const auto applied = QJsonDocument::fromJson(target.committedDocument()).object();
    QCOMPARE(applied["components"].toArray()[0].toObject()["appCollection"].toObject()["ids"].toArray(), QJsonArray{"installed-app"});
    const auto relative = applied["composition"].toObject()["wallpaper"].toString(); QVERIFY(!relative.isEmpty());
    QCOMPARE(read(dir.filePath("target/" + relative)), read(imagePath));
    QCOMPARE(source.committedDocument(), sourceBefore); QCOMPARE(read(dir.filePath("source/skins/" + personalSkin + ".json")), skinBytes);
    const auto secondPackage = dir.filePath("round-trip.json"); QVERIFY(importer->exportDesign(secondPackage, "page"));
    QVERIFY(!importer->previewImport(secondPackage, "interface")["valid"].toBool());
}
void StudioDesignTests::invalidPortablePackagesCannotMutateOrPublish()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    QVERIFY(home.applySkin("builtin-compact-launcher")); auto *session = qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(session);
    const auto baseline = session->draft(), disk = read(dir.filePath("home.json"));
    QVERIFY(session->exportDesign(dir.filePath("valid.json"), "interface")); const auto valid = QJsonDocument::fromJson(read(dir.filePath("valid.json"))).object();
    QList<QJsonObject> bad;
    auto package = valid; package["schemaVersion"] = 1.5; bad.append(package);
    package = valid; package["exec"] = "arbitrary"; bad.append(package);
    package = valid; auto layout = package["layout"].toObject(); layout["accounts"] = QJsonObject{{"password", "secret"}}; package["layout"] = layout; bad.append(package);
    package = valid; layout = package["layout"].toObject(); auto nodes = layout["components"].toArray(); auto node = nodes[0].toObject(); node["script"] = "arbitrary"; nodes[0] = node; layout["components"] = nodes; package["layout"] = layout; bad.append(package);
    package = valid; layout = package["layout"].toObject(); auto composition = layout["composition"].toObject(); composition["wallpaper"] = "/private/password.png"; layout["composition"] = composition; package["layout"] = layout; bad.append(package);
    package = valid; layout = package["layout"].toObject(); nodes = layout["components"].toArray(); node = nodes[0].toObject(); node["schemaVersion"] = 99; nodes[0] = node; layout["components"] = nodes; package["layout"] = layout; bad.append(package);
    for (const auto &entry : bad) {
        QVERIFY(write(dir.filePath("bad.json"), QJsonDocument(entry).toJson()));
        QVERIFY(session->previewImport(dir.filePath("bad.json"), "page").isEmpty());
        QVERIFY(!session->errorString().isEmpty()); QVERIFY(!session->proposalModel());
        QCOMPARE(session->draft(), baseline); QCOMPARE(read(dir.filePath("home.json")), disk); QVERIFY(!QFileInfo::exists(dir.filePath("artwork")));
    }
    QVERIFY(!session->exportDesign("relative.json", "page"));
    QVERIFY(!session->exportDesign(dir.filePath("bad-extension.txt"), "page"));
    QVERIFY(session->previewImport(dir.filePath("valid.json"), "page", {}, {{"old-app", "not-installed"}}).isEmpty());
    auto privateDoc = QJsonDocument::fromJson(baseline).object(); nodes = privateDoc["components"].toArray(); node = nodes[0].toObject();
    node["name"] = "/private/account-data"; nodes[0] = node; privateDoc["components"] = nodes;
    QVERIFY(session->pushEdit(QJsonDocument(privateDoc).toJson())); const auto privateDraft = session->draft();
    QVERIFY(!session->exportDesign(dir.filePath("private.json"), "interface")); QCOMPARE(session->draft(), privateDraft); QVERIFY(!QFileInfo::exists(dir.filePath("private.json")));
}
void StudioDesignTests::referenceImagesArePrivateBoundedAndTornDown()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    QVERIFY(home.applySkin("builtin-compact-launcher")); auto *session = qobject_cast<StudioSession *>(home.beginStudio()); QVERIFY(session);
    const auto baseline = session->draft(); const auto disk = read(dir.filePath("home.json"));
    QImage image(32, 32, QImage::Format_RGB32); image.fill(Qt::green); QVERIFY(image.save(dir.filePath("reference.png")));
    QVERIFY(session->setReferenceImage(dir.filePath("reference.png"))); const auto privatePath = QUrl(session->referenceSource()).toLocalFile(); QVERIFY(QFileInfo::exists(privatePath));
    QCOMPARE(session->draft(), baseline); QCOMPARE(read(dir.filePath("home.json")), disk); QCOMPARE(session->changeCount(), 0);
    QVERIFY(session->exportDesign(dir.filePath("export.json"), "interface")); const auto package = QJsonDocument::fromJson(read(dir.filePath("export.json"))).object();
    QVERIFY(package["assets"].toObject().isEmpty()); QVERIFY(!QJsonDocument(package).toJson().contains(privatePath.toUtf8()));
    QVERIFY(write(dir.filePath("invalid.png"), "not an image")); QVERIFY(!session->setReferenceImage(dir.filePath("invalid.png"))); QCOMPARE(QUrl(session->referenceSource()).toLocalFile(), privatePath);
    session->clearReferenceImage(); QVERIFY(session->referenceSource().isEmpty()); QVERIFY(!QFileInfo::exists(privatePath));
    QVERIFY(session->setReferenceImage(dir.filePath("reference.png"))); const auto secondPath = QUrl(session->referenceSource()).toLocalFile();
    session->invalidate(); QVERIFY(session->referenceSource().isEmpty()); QVERIFY(!QFileInfo::exists(secondPath)); QVERIFY(!session->setReferenceImage(dir.filePath("reference.png")));
    QCOMPARE(read(dir.filePath("home.json")), disk);
}
void StudioDesignTests::sharedStylesPreserveContentAndOverrides()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    QVERIFY(home.applySkin("builtin-compact-launcher"));
    const auto pageId=home.createPage("Other", "blank");QVERIFY(!pageId.isEmpty());
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    const auto disk=read(dir.filePath("home.json"));
    QVERIFY2(session->execute("patch",{{"id","main-apps"},{"value",QVariantMap{{"presentation",QVariantMap{{"labelColor","#abcdef"}}}}}}),qPrintable(session->errorString()));
    auto summary=session->previewReuse("saveStyle",{{"id","main-apps"},{"name","Shared ink"}});
    QVERIFY2(summary["valid"].toBool(),qPrintable(session->errorString()));QVERIFY(session->acceptPreview());
    const auto styleId=session->reusableChoices("styles")[0].toMap()["id"].toString();
    const auto before=session->draft();
    QVERIFY(session->previewReuse("useStyle",{{"id","main-apps"},{"libraryId",styleId}})["valid"].toBool());QVERIFY(session->acceptPreview());
    const auto authored=session->document()["components"].toList()[0].toMap();
    QVERIFY(authored["appCollection"].toMap()==QJsonDocument::fromJson(before).object()["components"].toArray()[0].toObject()["appCollection"].toObject().toVariantMap());
    QVERIFY(authored["presentation"].toMap().contains("layout"));
    QVERIFY(session->execute("patch",{{"id","main-apps"},{"value",QVariantMap{{"presentation",QVariantMap{{"labelColor","#fedcba"}}}}}}));
    summary=session->previewReuse("editStyle",{{"libraryId",styleId},{"presentation",QVariantMap{{"labelColor","#123456"}}}});
    QVERIFY2(summary["valid"].toBool(),qPrintable(session->errorString()));QCOMPARE(summary["changes"].toList().size(),1);
    QVERIFY(session->acceptPreview());QCOMPARE(session->model()->property("items").toList()[0].toMap()["presentation"].toMap()["labelColor"].toString(),QString("#fedcba"));
    QVERIFY(session->previewReuse("resetOverrides",{{"id","main-apps"}})["valid"].toBool());QVERIFY(session->acceptPreview());
    QCOMPARE(session->model()->property("items").toList()[0].toMap()["presentation"].toMap()["labelColor"].toString(),QString("#123456"));
    const auto count=session->changeCount();const auto baseline=session->draft();
    QVERIFY(session->previewReuse("detachStyle",{{"id","main-apps"}})["valid"].toBool());session->cancelPreview();QCOMPARE(session->draft(),baseline);QCOMPARE(session->changeCount(),count);
    QCOMPARE(read(dir.filePath("home.json")),disk);
}
void StudioDesignTests::reusableGroupsPreviewPropagateDetachAndRollback()
{
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));QVERIFY(home.applySkin("builtin-compact-launcher"));
    const auto pageId=home.createPage("Instances","blank"),otherId=home.createPage("Second instance","blank");QVERIFY(!pageId.isEmpty());QVERIFY(!otherId.isEmpty());
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    for(const auto &target:QStringList{pageId,otherId})QVERIFY(session->execute("pagePatch",{{"scope","composition"},{"value",QVariantMap{{"grid",ComponentSchema::normalizedGrid({})}}}},target));
    QVERIFY(session->previewReuse("saveComponent",{{"id","main-apps"},{"name","My app row"}})["valid"].toBool());QVERIFY(session->acceptPreview());
    const auto libraryId=session->reusableChoices("components")[0].toMap()["id"].toString();
    auto summary=session->previewReuse("insert",{{"libraryId",libraryId},{"linked",true}},pageId);
    QVERIFY2(summary["valid"].toBool(),qPrintable(session->errorString()));QVERIFY(session->acceptPreview());
    QVERIFY(session->previewReuse("insert",{{"libraryId",libraryId},{"linked",true}},otherId)["valid"].toBool());QVERIFY(session->acceptPreview());
    auto nodes=session->document()["pages"].toList()[0].toMap()["layout"].toMap()["components"].toList();QVERIFY(!nodes.isEmpty());
    QVERIFY(nodes[0].toMap()["id"]!=session->document()["pages"].toList()[1].toMap()["layout"].toMap()["components"].toList()[0].toMap()["id"]);
    const auto id=nodes[0].toMap()["id"].toString();QVERIFY(id!="main-apps");QCOMPARE(nodes[0].toMap()["linkedDesign"].toString(),libraryId);
    QVERIFY(session->execute("patch",{{"id",id},{"value",QVariantMap{{"name","Edited linked row"}}}},pageId));
    const auto baseline=session->draft();const auto count=session->changeCount();
    summary=session->previewReuse("editAll",{{"id",id}},pageId);QVERIFY2(summary["valid"].toBool(),qPrintable(session->errorString()));QCOMPARE(summary["changes"].toList().size(),2);
    QCOMPARE(session->draft(),baseline);QVERIFY(session->acceptPreview());QCOMPARE(session->changeCount(),count+1);QVERIFY(session->undo());QCOMPARE(session->draft(),baseline);
    QVERIFY(session->previewReuse("detach",{{"id",id}},pageId)["valid"].toBool());QVERIFY(session->acceptPreview());
    nodes=session->document()["pages"].toList()[0].toMap()["layout"].toMap()["components"].toList();QVERIFY(!nodes[0].toMap().contains("linkedDesign"));
    const auto stable=session->draft();
    QVERIFY(session->previewReuse("insert",{{"libraryId",libraryId},{"linked",true},{"placement",QVariantMap{{"x",0},{"y",0},{"width",4},{"height",3}}}},pageId).isEmpty());QCOMPARE(session->draft(),stable);
    auto library=QJsonDocument::fromJson(stable).object()["composition"].toObject()["designLibrary"].toObject().toVariantMap();
    auto definitions=library["components"].toList();auto definition=definitions[0].toMap();auto prototype=definition["nodes"].toList();auto root=prototype[0].toMap();root["linkedDesign"]=libraryId;prototype[0]=root;definition["nodes"]=prototype;definitions[0]=definition;library["components"]=definitions;
    QVERIFY(!DesignSystem::valid(library));
    const auto exportPath=dir.filePath("portable.json");QVERIFY2(session->exportDesign(exportPath,"page",pageId),qPrintable(session->errorString()));
    QVERIFY(QJsonDocument::fromJson(read(exportPath)).object()["layout"].toObject()["composition"].toObject().contains("designLibrary"));
}
void StudioDesignTests::responsiveRulesRemainDeclarativeAndBounded()
{
    QVariantMap rule{{"schemaVersion",1},{"policy","wrap"},{"minimumWidth",96},{"minimumHeight",48},{"aspect",16.0/9}};
    QVERIFY(DesignSystem::validResponsive(rule));
    const QVariantMap original{{"columns",4},{"responsive",rule}};
    QCOMPARE(DesignSystem::responsive(original,220,720)["columns"].toInt(),2);
    QCOMPARE(DesignSystem::responsive(original,1280,720)["columns"].toInt(),4);
    QCOMPARE(original["columns"].toInt(),4);
    auto shrinkRule = rule;shrinkRule["policy"] = "shrink";
    const QVariantMap shrink{{"columns",4},{"contentWidth",400},{"responsive",shrinkRule}};
    QCOMPARE(DesignSystem::responsive(shrink,220,720,.5)["contentWidth"].toDouble(),400.0);
    QCOMPARE(DesignSystem::responsive(shrink,120,720,.5)["contentWidth"].toDouble(),240.0);
    QCOMPARE(shrink["contentWidth"].toInt(),400);
    rule["policy"]="execute";QVERIFY(!DesignSystem::validResponsive(rule));rule["policy"]="stack";rule["minimumWidth"]=-1;QVERIFY(!DesignSystem::validResponsive(rule));
    QVERIFY(!ComponentSchema::validPresentation({{"responsive",original["responsive"]}},false));
    QVERIFY(ComponentSchema::validPresentation(original,true));
    QTemporaryDir dir;AppRegistry registry;QVERIFY(registry.loadFromData(apps));HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));
    auto document=QJsonDocument::fromJson(home.committedDocument()).object();
    document["composition"]=QJsonObject{{"schemaVersion",1},{"enabled",true}};
    document["categories"]=QJsonArray{QJsonObject{{"id","flow"},{"name","Flow"},{"presentation",QJsonObject{{"columns",4},{"height",400}}}}};
    QJsonArray nodes;for(int i=0;i<3;++i)nodes.append(QJsonObject::fromVariantMap(ComponentSchema::component({{"id",QString("clock-%1").arg(i)},{"name","Clock"},{"kind","widget"},{"widgetType","clock"},{"categoryId","flow"}},true)));
    document["components"]=nodes;QVERIFY(home.applyStudioDraft(QJsonDocument(document).toJson()));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    rule=original["responsive"].toMap();
    auto proposal=session->previewReuse("responsive",{{"categoryId","flow"},{"rule",rule}});
    QVERIFY2(proposal["valid"].toBool(),qPrintable(session->errorString()));QVERIFY(session->acceptPreview());
    QCOMPARE(QJsonDocument::fromJson(session->draft()).object()["components"].toArray(),nodes);
    const auto baseline=session->draft();rule["minimumHeight"]=800;
    QVERIFY(session->previewReuse("responsive",{{"categoryId","flow"},{"rule",rule}}).isEmpty());QCOMPARE(session->draft(),baseline);

}
void StudioDesignTests::cachedPageStylesAndHomeTokensRefreshWithoutWrites()
{
    QTemporaryDir dir;AppRegistry registry;QVERIFY(registry.loadFromData(apps));HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("home.json")));QVERIFY(home.applySkin("builtin-compact-launcher"));
    const auto pageId=home.createPage("Styled destination","blank"),homeId=home.createPage("Home tokens","blank");
    QVERIFY(!pageId.isEmpty());QVERIFY(!homeId.isEmpty());const auto disk=read(dir.filePath("home.json"));
    auto *session=qobject_cast<StudioSession *>(home.beginStudio());QVERIFY(session);
    QVERIFY(session->execute("pagePatch",{{"scope","composition"},{"value",QVariantMap{{"grid",ComponentSchema::normalizedGrid({})}}}},pageId));
    QVERIFY(session->execute("patch",{{"id","main-apps"},{"value",QVariantMap{{"presentation",QVariantMap{{"labelColor","#abcdef"}}}}}}));
    QVERIFY(session->previewReuse("saveStyle",{{"id","main-apps"},{"name","Page ink"}})["valid"].toBool());QVERIFY(session->acceptPreview());
    const auto styleId=session->reusableChoices("styles")[0].toMap()["id"].toString();
    QVERIFY(session->previewReuse("useStyle",{{"id","main-apps"},{"libraryId",styleId}})["valid"].toBool());QVERIFY(session->acceptPreview());
    QVERIFY(session->previewReuse("saveComponent",{{"id","main-apps"},{"name","Styled row"}})["valid"].toBool());QVERIFY(session->acceptPreview());
    const auto symbolId=session->reusableChoices("components")[0].toMap()["id"].toString();
    QVERIFY(session->previewReuse("insert",{{"libraryId",symbolId},{"linked",false}},pageId)["valid"].toBool());QVERIFY(session->acceptPreview());
    auto *model=qobject_cast<HomeLayout *>(session->model());QVERIFY(model);auto *page=qobject_cast<PageModel *>(model->pageModel(pageId));QVERIFY(page);
    QCOMPARE(page->items()[0].toMap()["presentation"].toMap()["labelColor"].toString(),QString("#abcdef"));
    const auto authoredPage=page->document();QSignalSpy changes(page,&PageModel::placementsChanged);
    QVERIFY(session->previewReuse("editStyle",{{"libraryId",styleId},{"presentation",QVariantMap{{"labelColor","#fedcba"}}}})["valid"].toBool());QVERIFY(session->acceptPreview());
    QVERIFY(changes.count()>0);QCOMPARE(page->document(),authoredPage);
    QCOMPARE(page->items()[0].toMap()["presentation"].toMap()["labelColor"].toString(),QString("#fedcba"));
    QVERIFY(session->execute("setHomePage",{{"id",homeId}}));
    QVERIFY(session->execute("pagePatch",{{"scope","theme"},{"value",QVariantMap{{"tileRoundness",.42},{"backgroundStart","#111111"}}}},homeId));
    QCOMPARE(page->tileRoundness(),.42);QCOMPARE(page->backgroundStart(),QString("#111111"));
    QVERIFY(session->execute("pagePatch",{{"scope","theme"},{"value",QVariantMap{{"tileRoundness",.8}}}},pageId));QCOMPARE(page->tileRoundness(),.8);
    QVERIFY(session->execute("pagePatch",{{"scope","theme"},{"value",QVariantMap{{"tileRoundness",QVariant()}}}},pageId));QCOMPARE(page->tileRoundness(),.42);
    QCOMPARE(read(dir.filePath("home.json")),disk);
}
QTEST_GUILESS_MAIN(StudioDesignTests)
#include "tst_studio_designs.moc"

void StudioDesignTests::editorViewportDefaultsRejectInvalidDimensionsWithoutWrites()
{
    QTemporaryDir dir;
    AppRegistry registry;
    const auto defaults = dir.filePath("default-home-layout.json");
    QVERIFY(QFile::copy(QFINDTESTDATA("../config/default-home-layout.json"),defaults));
    const auto optionsPath = dir.filePath("studio-defaults.json");
    auto options = QJsonDocument::fromJson(read(QFINDTESTDATA("../config/studio-defaults.json"))).object();
    const auto baseline = options;
    HomeLayout home(&registry);
    const auto saved = dir.filePath("home.json");
    QVERIFY(home.load(defaults,saved));
    QVERIFY(write(optionsPath,QJsonDocument(baseline).toJson(QJsonDocument::Compact)));
    QVERIFY2(!home.studioOptions().isEmpty(),"Current packaged options must validate before testing older options");
    for (const auto &invalid : QList<QJsonObject>{
             {{"width",0},{"height",720}}, {{"width",1280},{"height",2161}},
             {{"width","1280"},{"height",720}}, {{"width",1280.5},{"height",720}},
             {{"width",1280},{"height",720},{"extra",true}}}) {
        options["referenceViewport"] = invalid;
        QVERIFY(write(optionsPath,QJsonDocument(options).toJson(QJsonDocument::Compact)));
        QVERIFY(home.studioOptions().isEmpty());
        QVERIFY(!QFile::exists(saved));
    }
    options = baseline;
    options.remove("referenceViewport");
    QVERIFY(write(optionsPath,QJsonDocument(options).toJson(QJsonDocument::Compact)));
    QCOMPARE(home.studioOptions()["referenceViewport"].toMap(),baseline["referenceViewport"].toObject().toVariantMap());
    QCOMPARE(read(optionsPath),QJsonDocument(options).toJson(QJsonDocument::Compact));
    QVERIFY(!QFile::exists(saved));
}

void StudioDesignTests::navigationOnlyReorderingAppearsInTheApplySummary()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto path = dir.filePath("home.json");
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"),path));
    auto *session = qobject_cast<StudioSession *>(home.beginStudio());
    QVERIFY(session);
    QVERIFY(session->execute("newPage",{{"name","First destination"}}));
    QVERIFY(session->execute("newPage",{{"name","Second destination"}}));
    QVERIFY(session->apply());
    const auto disk = read(path), baseline = session->draft();
    const auto first = session->document()["pages"].toList().first().toMap()["id"].toString();
    QVERIFY(session->execute("reorderPage",{{"id",first},{"offset",1}}));
    const auto changes = session->changeSummary();
    QCOMPARE(changes.size(),2);
    for (const auto &change : changes) {
        QCOMPARE(change.toMap()["change"].toString(),QString("Reordered"));
        QVERIFY(!change.toMap()["pageId"].toString().isEmpty());
    }
    QCOMPARE(read(path),disk);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(),baseline);
    QVERIFY(session->changeSummary().isEmpty());
}

void StudioDesignTests::appearancePasteAndIndependentArtworkCopiesKeepOwnership()
{
    QTemporaryDir dir;
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    const auto path = dir.filePath("home.json");
    HomeLayout home(&registry);
    QVERIFY(home.load(defaults,path));
    QVERIFY(home.applySkin("builtin-compact-launcher"));
    const auto pageId = home.createPage("Independent copies","blank");
    QVERIFY(!pageId.isEmpty());
    auto *session = qobject_cast<StudioSession *>(home.beginStudio());
    QVERIFY(session);
    QImage image(24,24,QImage::Format_RGB32);image.fill(Qt::blue);
    const auto imagePath = dir.filePath("source.png");
    QVERIFY(image.save(imagePath));
    const auto artwork = session->stageArtwork(imagePath);
    QVERIFY(!artwork.isEmpty());
    QVERIFY(session->execute("patch",{{"id","main-apps"},{"value",QVariantMap{
        {"customLogo",artwork},{"presentation",QVariantMap{{"labelColor","#123456"}}}}}}));
    QVERIFY(session->execute("pagePatch",{{"scope","composition"},{"value",QVariantMap{{"grid",ComponentSchema::normalizedGrid({})}}}},pageId));
    auto document = QJsonDocument::fromJson(session->draft()).object();
    auto pages = document["pages"].toArray();
    auto page = pages[0].toObject(), layout = page["layout"].toObject();
    auto target = document["components"].toArray()[0].toObject();
    target["id"] = "paste-target";target["name"] = "Independent content";target.remove("customLogo");
    target.remove("categoryId");target.remove("parentId");
    target["appCollection"] = QJsonObject{{"mode","selected"},{"ids",QJsonArray{"installed-app"}}};
    auto presentation = target["presentation"].toObject();presentation["labelColor"] = "#abcdef";target["presentation"] = presentation;
    layout["components"] = QJsonArray{target};page["layout"] = layout;pages[0] = page;document["pages"] = pages;
    QVERIFY2(session->pushEdit(QJsonDocument(document).toJson()),qPrintable(session->errorString()));
    const auto before = session->draft(), disk = read(path);
    const QVariantMap arguments{{"id","paste-target"},{"sourceId","main-apps"},{"sourcePage",""}};
    QVERIFY(session->previewReuse("pasteAppearance",arguments,pageId)["valid"].toBool());
    session->cancelPreview();QCOMPARE(session->draft(),before);QCOMPARE(read(path),disk);
    QVERIFY(session->previewReuse("pasteAppearance",arguments,pageId)["valid"].toBool());
    QVERIFY(session->acceptPreview());
    auto pasted = session->document()["pages"].toList()[0].toMap()["layout"].toMap()["components"].toList()[0].toMap();
    QCOMPARE(pasted["presentation"].toMap()["labelColor"].toString(),QString("#123456"));
    auto original = target.toVariantMap(), unchanged = pasted;original.remove("presentation");unchanged.remove("presentation");
    QCOMPARE(unchanged,original);
    QCOMPARE(pasted["presentation"].toMap()["layout"],target["presentation"].toObject()["layout"].toVariant());
    QVERIFY(session->undo());QCOMPARE(session->draft(),before);
    auto invalid = arguments;invalid["sourceId"] = "missing";
    QVERIFY(session->previewReuse("pasteAppearance",invalid,pageId).isEmpty());QCOMPARE(session->draft(),before);
    QVERIFY(session->previewReuse("saveComponent",{{"id","main-apps"},{"name","Artwork owner"}})["valid"].toBool());
    QVERIFY(session->acceptPreview());
    const auto libraryId = session->reusableChoices("components").first().toMap()["id"].toString();
    QVERIFY2(session->previewReuse("insert",{{"libraryId",libraryId},{"linked",false}},pageId)["valid"].toBool(),qPrintable(session->errorString()));
    QVERIFY(session->acceptPreview());
    const auto copies = session->document()["pages"].toList()[0].toMap()["layout"].toMap()["components"].toList();
    QCOMPARE(copies.size(),2);
    const auto copy = copies[1].toMap();
    QVERIFY(copy["id"].toString()!=QString("main-apps") && copy["id"]!=pasted["id"]);
    QVERIFY(!copy.contains("linkedDesign"));QCOMPARE(copy["customLogo"].toString(),artwork);
    QVERIFY(session->execute("remove",{{"id","main-apps"}}));
    QVERIFY2(session->apply(),qPrintable(session->errorString()));
    QCOMPARE(read(dir.filePath(artwork)),read(imagePath));
    HomeLayout reopened(&registry);QVERIFY(reopened.load(defaults,path));
    QCOMPARE(reopened.pages().first().toMap()["layout"].toMap()["components"].toList()[1].toMap()["customLogo"].toString(),artwork);
}
