#include "../src/app/AppCatalog.h"
#include "../src/home/HomeLayout.h"
#include "../src/home/ComponentSchema.h"
#include "../src/home/ManagedGif.h"
#include "../src/home/PageModel.h"
#include "../src/home/StudioSession.h"
#include "../src/home/ThemeColors.h"
#include "tst_component_common.h"

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

class StudioSessionTests : public QObject
{
    Q_OBJECT
  private slots:
    void
    studioSessionDraftUndoBoundsAndNoWrite();
    void
    studioSessionCanonicalIdsAndRevisionConflict();
    void
    studioApplyAtomicFailureAndFocusPreserved();
    void
    studioCommandsAreAtomicAndProfileScoped();
    void
    studioCollisionProposalsAndParity();
    void
    studioSectionGridsAreIndependentAndAtomic();
    void
    studioHomeDesignationIsAtomicAndProtected();
    void
    studioArtworkUsesActiveProfileAndExpires();
    void
    studioPageLifecycleRepairsLinksAndPreservesRoot();
    void
    studioChromeRemainsReadableAndProfileLocal();
    void
    studioHiddenAndLockedStatesAreRecoverable();
    void
    studioScrollingPagesAreBoundedAndAtomic();
    void
    studioShiftCascadesAreBoundedAndAtomic();
    void
    studioSectionTransfersDuplicationAndOrdering();
    void
    studioNestedGroupsValidateAndReparentAtomically();
    void
    studioFiveColorThemeLinksOverridesAndInvalidRollback();
    void
    studioCatalogLinksInspectorAndStagedArtwork();
    void
    studioOrderedInsertionPreservesSlotsAndIsAtomic();
    void
    studioOverlayMovementKeepsNeighborsAndBounds();
    void
    studioHeaderValidationAndDraftRollback();
    void
    studioPageDefaultsAreDraftOnlyAndScoped();
    void
    studioShapesLabelsAndReplacementAreAtomic();
    void
    studioButtonCollectionsAreBoundedAndAtomic();
    void
    studioCollectionInstallDoesNotAuthorPlacements();
    void
    studioIconCleanupRetainsOriginalAndCancelsPrivately();
    void
    studioGifArtworkIsBoundedStagedAndProfileScoped();
    void
    studioPastelPresetUsesEditableButtons();
    void
    studioStartingStylesAreValidatedDraftData();
};

int
main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    StudioSessionTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_studio_session.moc"

void
StudioSessionTests::studioSessionDraftUndoBoundsAndNoWrite()
{
    QTemporaryDir dir;
    const auto user = dir.filePath("profile.json");
    writeFile(user, personalLayout);
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
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

    QVERIFY(session.pushEdit(edited));
    QByteArray huge = edited;
    huge += QByteArray(5 * 1024 * 1024, ' ');
    QVERIFY(!session.pushEdit(huge));
    QVERIFY(!session.errorString().isEmpty());
    QCOMPARE(session.draft(), edited);
    QCOMPARE(readFile(user), committed);

    QVERIFY(!session.pushEdit("not-json{{{"));
    QVERIFY(!session.errorString().isEmpty());
    QCOMPARE(session.draft(), edited);

    QJsonDocument vdoc = QJsonDocument::fromJson(edited);
    auto vobject = vdoc.object();
    vobject["schemaVersion"] = 99;
    QVERIFY(!session.pushEdit(QJsonDocument(vobject).toJson()));
    QCOMPARE(session.draft(), edited);
    QCOMPARE(readFile(user), committed);

    QJsonArray many;
    for (int i = 0; i < 1025; ++i)
        many.append(QJsonObject{{"id", QString("c%1").arg(i)}, {"kind", "widget"}, {"widgetType", "clock"}});
    vobject = QJsonDocument::fromJson(edited).object();
    vobject["components"] = many;
    QVERIFY(!session.pushEdit(QJsonDocument(vobject).toJson()));
    QCOMPARE(readFile(user), committed);

    session.revert();
    for (int i = 0; i < StudioSession::kMaxUndoSteps; ++i)
    {
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

    const QByteArray finalDraft = session.draft();
    session.markApplied();
    QVERIFY(!session.isDirty());
    QCOMPARE(session.committed(), finalDraft);
    QCOMPARE(readFile(user), committed);
}

void
StudioSessionTests::studioSessionCanonicalIdsAndRevisionConflict()
{
    QTemporaryDir dir;
    const auto user = dir.filePath("profile.json");
    writeFile(user, personalLayout);
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults, user));
    const QByteArray committed = readFile(user);

    StudioSession session;
    QVERIFY(session.open(committed));

    QCOMPARE(session.allocateId("brand-new-widget"), QString("brand-new-widget"));
    QCOMPARE(session.allocateId("clock"), QString("clock-copy"));
    QCOMPARE(session.allocateId("clock-copy"), QString("clock-copy"));
    QVERIFY(session.allocateId("has spaces!").isEmpty());
    QVERIFY(!session.errorString().isEmpty());

    const QByteArray a = R"({"schemaVersion":2,"theme":{"b":1,"a":2}})";
    const QByteArray b = R"({"theme":{"a":2,"b":1},"schemaVersion":2})";
    QByteArray aOut, bOut;
    QVERIFY(session.canonicalize(a, b, aOut, bOut));
    QCOMPARE(aOut, bOut);
    QVERIFY(!session.canonicalize("not json", b, aOut, bOut));

    QVERIFY(!session.isStaleAgainst(committed + " "));
    QJsonDocument doc = QJsonDocument::fromJson(committed);
    auto object = doc.object();
    auto changedTheme = object["theme"].toObject();
    changedTheme["backgroundStart"] = "#112233";
    object["theme"] = changedTheme;
    QVERIFY(session.pushEdit(QJsonDocument(object).toJson()));
    QVERIFY(session.isDirty());
    QVERIFY(!session.isStaleAgainst(committed));
    QJsonDocument other = QJsonDocument::fromJson(committed);
    auto otherObject = other.object();
    otherObject["theme"] = QJsonObject{{"backgroundStart", "#445566"}};
    QVERIFY(session.isStaleAgainst(QJsonDocument(otherObject).toJson()));
    QVERIFY(session.isStaleAgainst({}));

    const auto user2 = dir.filePath("profile2.json");
    writeFile(user2, personalLayout);
    const QByteArray committed2 = readFile(user2);
    StudioSession other2;
    QVERIFY(other2.open(committed2));
    QCOMPARE(other2.draft(), committed2);
    QVERIFY(session.draft() != other2.draft());
    QVERIFY(other2.pushEdit(QJsonDocument(object).toJson()));
    QCOMPARE(readFile(user), committed);
    QCOMPARE(readFile(user2), committed2);
    QVERIFY(!other2.isStaleAgainst(committed2));
    QCOMPARE(session.allocateId("clock"), QString("clock-copy"));
    other2.revert();
    QVERIFY(!other2.isDirty());
    QCOMPARE(readFile(user2), committed2);
}

void
StudioSessionTests::studioApplyAtomicFailureAndFocusPreserved()
{
    QTemporaryDir dir;
    const auto user = dir.filePath("profile.json");
    writeFile(user, personalLayout);
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults, user));
    const QByteArray committed = home.committedDocument();
    const QByteArray originalFile = readFile(user);
    const auto before = home.components();

    StudioSession session;
    QVERIFY(session.open(committed));
    home.setFocusOrigin(QStringLiteral("clock"));

    QJsonDocument doc = QJsonDocument::fromJson(committed);
    auto object = doc.object();
    auto entries = object.value(QStringLiteral("components")).toArray();
    QVERIFY(!entries.isEmpty());
    auto first = entries.first().toObject();
    first[QStringLiteral("name")] = QStringLiteral("Studio clock");
    entries[0] = first;
    object[QStringLiteral("components")] = entries;
    const QByteArray edited = QJsonDocument(object).toJson();
    QVERIFY(session.pushEdit(edited));

    QVERIFY(!home.applyStudioDraft("not-json{{{", QStringLiteral("clock")));
    QVERIFY(!home.errorString().isEmpty());
    QCOMPARE(readFile(user), originalFile);
    QCOMPARE(home.components(), before);
    QCOMPARE(session.draft(), edited);
    QCOMPARE(home.focusOrigin(), QStringLiteral("clock"));

    QVERIFY(QFile::rename(user, user + ".held"));
    QVERIFY(QDir().mkdir(user));
    QVERIFY(!home.applyStudioDraft(edited, QStringLiteral("my-folder")));
    QVERIFY(!home.errorString().isEmpty());
    QCOMPARE(home.components(), before);
    QCOMPARE(home.committedDocument(), committed);
    QCOMPARE(session.draft(), edited);
    QCOMPARE(home.focusOrigin(), QStringLiteral("clock"));
    QVERIFY(QDir().rmdir(user));
    QVERIFY(QFile::rename(user + ".held", user));

    QVERIFY(home.applyStudioDraft(edited, QStringLiteral("my-folder")));
    QCOMPARE(home.focusOrigin(), QStringLiteral("my-folder"));
    QVERIFY(readFile(user) != committed);
    session.markApplied();
    QVERIFY(!session.isDirty());
}

void
StudioSessionTests::studioCommandsAreAtomicAndProfileScoped()
{
    QTemporaryDir dir;
    AppRegistry registry;
    QVERIFY(registry.loadFromData(apps));
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    const auto user = dir.filePath("one.json");
    writeFile(user, personalLayout);
    HomeLayout home(&registry);
    QVERIFY(home.load(defaults, user));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    const auto file = readFile(user);
    const auto baseline = session->draft();
    QVERIFY(session->execute("rename", {{"id", "my-folder"}, {"name", "Draft name"}}));
    QCOMPARE(readFile(user), file);
    QVERIFY(home.items()[0].toMap()["name"] != QString("Draft name"));
    QVERIFY(session->model()->property("items").toList()[0].toMap()["name"] == QString("Draft name"));
    const auto renamed = session->draft();
    QVERIFY(!session->execute(
        "place",
        {{"placements",
          QVariantList{
              QVariantMap{{"id", "clock"}, {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}}},
              QVariantMap{{"id", "missing"},
                          {"placement", QVariantMap{{"x", 1}, {"y", 0}, {"width", 1}, {"height", 1}}}}}}}));
    QCOMPARE(session->draft(), renamed);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), baseline);
    QVERIFY(session->redo());
    QCOMPARE(session->draft(), renamed);
    QVERIFY(session->execute("newPage", {{"name", "Draft page"}}));
    QCOMPARE(home.pages().size(), 0);
    QCOMPARE(readFile(user), file);
    QVERIFY(session->apply("clock"));
    QCOMPARE(home.pages().size(), 1);
    QVERIFY(readFile(user) != file);
    QVERIFY(!session->isDirty());
    QVERIFY(session->execute("rename", {{"id", "my-folder"}, {"name", "Unapplied"}}));
    const auto saved = readFile(user), recoverable = session->draft();
    writeFile(user, saved + "\n");
    QVERIFY(!session->apply());
    QCOMPARE(readFile(user), saved + "\n");
    QCOMPARE(session->draft(), recoverable);
    writeFile(user, saved);
    QVERIFY(home.setItemName("my-folder", "Concurrent change"));
    QVERIFY(!session->apply());
    QVERIFY(session->errorString().contains("changed"));
    const auto second = dir.filePath("two.json");
    writeFile(second, personalLayout);
    bool teardownAttempted = false, teardownAccepted = true;
    connect(session,
            &StudioSession::changed,
            session,
            [&]
            {
                if (teardownAttempted)
                    return;
                teardownAttempted = true;
                teardownAccepted = session->execute("remove", {{"id", "clock"}});
            });
    QVERIFY(home.load(defaults, second));
    QVERIFY(teardownAttempted);
    QVERIFY(!teardownAccepted);
    QVERIFY(!session->active());
    QVERIFY(!session->apply());
    QVERIFY(!session->execute("remove", {{"id", "clock"}}));
    QCOMPARE(readFile(second), personalLayout);
}

void
StudioSessionTests::studioCollisionProposalsAndParity()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), dir.filePath("home.json")));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    QVERIFY(session->execute("convertGrid", {{"value", QVariantMap{{"columns", 8}, {"rows", 5}}}}));
    const auto start = session->draft();
    const auto history = session->changeCount();
    QVERIFY(session->previewCommand("convertGrid", {{"value", QVariantMap{{"columns", 4}, {"rows", 5}}}}));
    QCOMPARE(session->draft(), start);
    QCOMPARE(session->changeCount(), history);
    QVERIFY(session->proposalModel());
    session->cancelPreview();
    QCOMPARE(session->draft(), start);
    const QVariantMap p{{"x", 1}, {"y", 0}, {"width", 1}, {"height", 1}};
    QVERIFY(!session->placementProposal("clock", p, "cancel").value("valid").toBool());
    auto swap = session->placementProposal("clock", p, "swap");
    QVERIFY2(swap["valid"].toBool(), qPrintable(swap["reason"].toString()));
    QCOMPARE(swap["placements"].toList().size(), 2);
    QVERIFY2(session->execute("place", {{"placements", swap["placements"]}}), qPrintable(session->errorString()));
    const auto swapped = session->draft();
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    QVERIFY(session->execute("place", {{"placements", session->placementProposal("clock", p, "swap")["placements"]}}));
    QCOMPARE(session->draft(), swapped);
    QVERIFY(session->undo());
    auto shift = session->placementProposal("clock", p, "shift");
    QVERIFY2(shift["valid"].toBool(), qPrintable(shift["reason"].toString()));
    QVERIFY(session->execute("place", {{"placements", shift["placements"]}}));
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    QVERIFY(!session->placementProposal("clock", {{"x", 8}, {"y", 0}, {"width", 1}, {"height", 1}}, "shift")["valid"]
                 .toBool());
    QCOMPARE(session->draft(), start);
    const auto addition = session->additionProposal("clock", p, "shift");
    QVERIFY2(addition["valid"].toBool(), qPrintable(addition["reason"].toString()));
    const auto beforeAdd = session->changeCount();
    QVERIFY(session->execute("batch", {{"commands", addition["commands"]}}));
    QCOMPARE(session->changeCount(), beforeAdd + 1);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    const auto beforeBatch = session->changeCount();
    QVERIFY(session->execute(
        "batch",
        {{"commands",
          QVariantList{QVariantMap{{"operation", "rename"},
                                   {"arguments", QVariantMap{{"id", "clock"}, {"name", "Group rename"}}}},
                       QVariantMap{{"operation", "rename"},
                                   {"arguments", QVariantMap{{"id", "movies"}, {"name", "Library"}}}}}}}));
    QCOMPARE(session->changeCount(), beforeBatch + 1);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
}

void
StudioSessionTests::studioSectionGridsAreIndependentAndAtomic()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto user = dir.filePath("home.json");
    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), user));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    const QVariantMap grid{{"columns", 8}, {"rows", 5}, {"mode", "sections"}};
    QVERIFY2(session->execute("convertGrid", {{"value", grid}, {"sectionHeight", 560}}),
             qPrintable(session->errorString()));
    const auto document = QJsonDocument::fromJson(session->draft()).object();
    const auto categories = document["categories"].toArray();
    QVERIFY(!categories.isEmpty());
    const auto category = categories.first().toObject()["id"].toString();
    QVERIFY(!category.isEmpty());
    const QVariantMap cell{{"x", 0}, {"y", 4}, {"width", 1}, {"height", 1}};
    QVERIFY2(session->execute("addPlaced", {{"type", "text"}, {"placement", cell}, {"categoryId", category}}),
             qPrintable(session->errorString()));
    QVERIFY2(session->execute("addPlaced", {{"type", "clock"}, {"placement", cell}}),
             qPrintable(session->errorString()));
    const auto before = session->draft();
    const auto count = session->changeCount();
    QVERIFY(!session->execute("addPlaced", {{"type", "text"}, {"placement", cell}, {"categoryId", category}}));
    QCOMPARE(session->draft(), before);
    QCOMPARE(session->changeCount(), count);
    QVERIFY(!session->execute("convertGrid", {{"value", grid}, {"sectionHeight", 59}}));
    QCOMPARE(session->draft(), before);
    QVERIFY(!QFile::exists(user));
    QVERIFY(session->undo());
    QVERIFY(session->redo());
    QCOMPARE(session->draft(), before);
    QVERIFY2(session->apply(), qPrintable(session->errorString()));
    HomeLayout restored(&registry);
    QVERIFY(restored.load(QFINDTESTDATA("../config/default-home-layout.json"), user));
    QCOMPARE(restored.committedDocument(), home.committedDocument());
    auto composition = document["composition"].toObject().toVariantMap();
    auto modified = categories;
    auto section = modified.first().toObject();
    section["presentation"] = QJsonObject{{"grid", QJsonObject{{"columns", 4}, {"rows", 3}}}};
    modified[0] = section;
    QCOMPARE(ComponentSchema::sectionGrid(composition, modified, category)["columns"].toInt(), 4);
    QVERIFY(!ComponentSchema::validPresentation({{"grid", QVariantMap{{"mode", "sections"}}}}, true));
}

void
StudioSessionTests::studioHomeDesignationIsAtomicAndProtected()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto user = dir.filePath("home.json"), defaults = QFINDTESTDATA("../config/default-home-layout.json");
    QVERIFY(home.load(defaults, user));
    QCOMPARE(home.homePageId(), QString());
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    QVERIFY2(session->execute("newPage", {{"name", "Library"}}), qPrintable(session->errorString()));
    const auto document = QJsonDocument::fromJson(session->draft()).object();
    const auto id = document["pages"].toArray().last().toObject()["id"].toString();
    QVERIFY(!id.isEmpty());
    QVERIFY(session->execute("appearance", {{"value", QVariantMap{{"focusColor", "#123456"}}}}, id));
    QVERIFY(session->execute("setHomePage", {{"id", id}}));
    const auto designated = session->draft();
    QVERIFY(!session->execute("removePage", {{"id", id}}));
    QCOMPARE(session->draft(), designated);
    QVERIFY(!session->execute("setHomePage", {{"id", "missing"}}));
    QCOMPARE(session->draft(), designated);
    QVERIFY(!QFile::exists(user));
    QCOMPARE(home.homePageId(), QString());
    QVERIFY(session->undo());
    QVERIFY(session->redo());
    QCOMPARE(session->draft(), designated);
    QVERIFY2(session->apply(), qPrintable(session->errorString()));
    QCOMPARE(home.homePageId(), id);
    QCOMPARE(home.homeAppearance()["focusColor"].toString(), QString("#123456"));
    HomeLayout restored(&registry);
    QVERIFY(restored.load(defaults, user));
    QCOMPARE(restored.homePageId(), id);
    QVERIFY2(session->execute(
                 "addPlaced",
                 {{"type", "clock"}, {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}}},
                 id),
             qPrintable(session->errorString()));
    QVERIFY(session->execute("setHomePage", {{"id", ""}}));
    QVERIFY(session->execute("removePage", {{"id", id}}));
    QVERIFY(session->undo());
    QCOMPARE(QJsonDocument::fromJson(session->draft()).object()["pages"].toArray().size(), 1);
}

void
StudioSessionTests::studioArtworkUsesActiveProfileAndExpires()
{
    QTemporaryDir dir;
    AppRegistry registry;
    auto document = QJsonDocument::fromJson(personalLayout).object();
    document["composition"] = QJsonObject{{"schemaVersion", 1}, {"enabled", true}, {"wallpaper", "artwork/scene.png"}};
    writeFile(dir.filePath("default.json"), QJsonDocument(document).toJson());
    HomeLayout home(&registry);
    QVERIFY(home.load(dir.filePath("default.json"), dir.filePath("home.json")));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    auto* model = qobject_cast<HomeLayout*>(session->model());
    QVERIFY(model);
    QCOMPARE(model->wallpaperSource(), home.wallpaperSource());
    QCOMPARE(model->components().first().toMap()["logoSource"], home.components().first().toMap()["logoSource"]);
    QVERIFY(!QFile::exists(dir.filePath("home.json")));
    session->invalidate();
    QVERIFY(model->wallpaperSource().isEmpty());
    QVERIFY(model->components().isEmpty());
}

void
StudioSessionTests::studioPageLifecycleRepairsLinksAndPreservesRoot()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"), user = dir.filePath("home.json");
    QVERIFY(home.load(defaults, user));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    QVERIFY2(session->execute("renamePage", {{"id", ""}, {"name", "Landing"}}), qPrintable(session->errorString()));
    QVERIFY2(session->execute("duplicatePage", {{"id", ""}, {"name", "Library"}}), qPrintable(session->errorString()));
    auto document = QJsonDocument::fromJson(session->draft()).object();
    const auto page = document["pages"].toArray().last().toObject();
    const auto id = page["id"].toString();
    QVERIFY(page["layout"].toObject()["components"].toArray().first().toObject()["id"] !=
            document["components"].toArray().first().toObject()["id"]);
    QVERIFY2(session->execute("reorderPage", {{"id", ""}, {"offset", 1}}), qPrintable(session->errorString()));
    auto* draft = qobject_cast<HomeLayout*>(session->model());
    QVERIFY(draft);
    QCOMPARE(draft->studioPages().last().toMap()["name"].toString(), QString("Landing"));
    QVERIFY2(session->execute("addButton", {}, id), qPrintable(session->errorString()));
    document = QJsonDocument::fromJson(session->draft()).object();
    const auto button = document["pages"]
                            .toArray()
                            .first()
                            .toObject()["layout"]
                            .toObject()["components"]
                            .toArray()
                            .last()
                            .toObject()["id"]
                            .toString();
    QVERIFY2(session->execute("action", {{"id", button}, {"value", QVariantMap{{"type", "root-page"}}}}, id),
             qPrintable(session->errorString()));
    QVERIFY(session->execute("setHomePage", {{"id", id}}));
    const auto before = session->draft();
    QVERIFY(!session->execute("removePage", {{"id", ""}}));
    QCOMPARE(session->draft(), before);
    QVERIFY2(session->execute("removePage", {{"id", ""}, {"replacementPageId", id}}),
             qPrintable(session->errorString()));
    const auto removed = session->draft();
    QVERIFY(!draft->rootPageVisible());
    QCOMPARE(draft->studioPages().size(), 1);
    document = QJsonDocument::fromJson(removed).object();
    QVERIFY(document["components"].toArray().isEmpty());
    const auto action = document["pages"]
                            .toArray()
                            .first()
                            .toObject()["layout"]
                            .toObject()["components"]
                            .toArray()
                            .last()
                            .toObject()["actions"]
                            .toArray()
                            .first()
                            .toObject();
    QCOMPARE(action["type"].toString(), QString("open-page"));
    QCOMPARE(action["pageId"].toString(), id);
    QVERIFY(!session->execute("setHomePage", {{"id", ""}}));
    QCOMPARE(session->draft(), removed);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), before);
    QVERIFY(session->redo());
    QCOMPARE(session->draft(), removed);
    QVERIFY(!QFile::exists(user));
    QVERIFY(session->apply());
    HomeLayout restored(&registry);
    QVERIFY(restored.load(defaults, user));
    QVERIFY(!restored.rootPageVisible());
    QCOMPARE(restored.homePageId(), id);
    QVERIFY2(restored.applySkin("builtin-default"), qPrintable(restored.errorString()));
    QVERIFY(!restored.rootPageVisible());
    QCOMPARE(restored.homePageId(), id);
    auto* homePage = qobject_cast<PageModel*>(restored.pageModel(id));
    QVERIFY(homePage);
    QVERIFY(!homePage->items().isEmpty());
    HomeLayout visibleRoot(&registry);
    QVERIFY(visibleRoot.load(defaults, dir.filePath("visible-root.json")));
    const auto another = visibleRoot.createPage("Another Home", "blank");
    QVERIFY(!another.isEmpty());
    auto* changeHome = qobject_cast<StudioSession*>(visibleRoot.beginStudio());
    QVERIFY(changeHome);
    QVERIFY(changeHome->execute("setHomePage", {{"id", another}}));
    QVERIFY(changeHome->apply());
    const auto rootComponents = QJsonDocument::fromJson(visibleRoot.committedDocument()).object().value("components");
    QVERIFY2(visibleRoot.applySkin("builtin-default"), qPrintable(visibleRoot.errorString()));
    QVERIFY(visibleRoot.rootPageVisible());
    QCOMPARE(QJsonDocument::fromJson(visibleRoot.committedDocument()).object().value("components"), rootComponents);
    QVERIFY(!qobject_cast<PageModel*>(visibleRoot.pageModel(another))->items().isEmpty());
}

void
StudioSessionTests::studioChromeRemainsReadableAndProfileLocal()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"), user = dir.filePath("home.json");
    QVERIFY(home.load(defaults, user));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    QVERIFY(session->execute("appearance",
                             {{"value", QVariantMap{{"surfaceColor", "#ffffff"}, {"textColor", "#ffffff"}}}}));
    auto* draft = qobject_cast<HomeLayout*>(session->model());
    QVERIFY(draft);
    QVERIFY(draft->studioAppearance()["surfaceColor"] != draft->studioAppearance()["textColor"]);
    const auto baseline = session->draft();
    QVERIFY(!session->execute("studioAppearance", {{"value", QVariantMap{{"textColor", "#202126"}}}}));
    QCOMPARE(session->draft(), baseline);
    QVERIFY2(session->execute("studioAppearance", {{"value", QVariantMap{{"focusColor", "#ffffff"}}}}),
             qPrintable(session->errorString()));
    QVERIFY(session->apply());
    QCOMPARE(home.studioAppearance()["focusColor"].toString(), QString("#ffffff"));
    const auto colorBaseline = session->draft();
    const auto colorCount = session->changeCount();
    const auto previewColor = [&](const QString& color)
    {
        return session->previewCommand("pagePatch",
                                       {{"scope", "theme"}, {"value", QVariantMap{{"backgroundStart", color}}}});
    };
    QVERIFY(previewColor("#abcdef"));
    QCOMPARE(session->draft(), colorBaseline);
    session->cancelPreview();
    QCOMPARE(session->draft(), colorBaseline);
    QVERIFY(previewColor("#abcdef"));
    QVERIFY(!session->acceptColorPreview("transparent"));
    QCOMPARE(session->draft(), colorBaseline);
    QVERIFY(session->acceptColorPreview("#ABCDEF"));
    QCOMPARE(session->changeCount(), colorCount + 1);
    QCOMPARE(session->document()["studioAppearance"].toMap()["recentColors"].toList(), QVariantList{"#abcdef"});
    QVERIFY(!home.studioAppearance().contains("recentColors"));
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), colorBaseline);
    QVERIFY(session->redo());
    for (int i = 0; i < 10; ++i)
    {
        const auto color = QString("#%1").arg(i, 6, 16, QChar('0'));
        QVERIFY(previewColor(color));
        QVERIFY(session->acceptColorPreview(color));
    }
    const auto recent = session->document()["studioAppearance"].toMap()["recentColors"].toList();
    QCOMPARE(recent.size(), 8);
    QCOMPARE(recent.first().toString(), QString("#000009"));
    QVERIFY(session->execute("studioAppearance", {{"value", QVariantMap{{"focusColor", "#ffffff"}}}}));
    QCOMPARE(session->document()["studioAppearance"].toMap()["recentColors"].toList(), recent);
    const auto valid = session->draft();
    for (const auto& bad : QVariantList{QVariantList{"red"},
                                        QVariantList{"#123456", "#123456"},
                                        QVariantList{"#abcdef00"},
                                        QVariantList{12},
                                        QVariantList{"#000000",
                                                     "#000001",
                                                     "#000002",
                                                     "#000003",
                                                     "#000004",
                                                     "#000005",
                                                     "#000006",
                                                     "#000007",
                                                     "#000008",
                                                     "#000009"}})
    {
        QVERIFY(!session->execute("studioAppearance", {{"value", QVariantMap{{"recentColors", bad}}}}));
        QCOMPARE(session->draft(), valid);
    }
    const auto applied = readFile(user);
    QVERIFY(QFile::rename(user, user + ".held"));
    QVERIFY(QDir().mkpath(user));
    QVERIFY(!session->apply());
    QCOMPARE(session->draft(), valid);
    QCOMPARE(readFile(user + ".held"), applied);
    QVERIFY(QDir().rmdir(user));
    QVERIFY(QFile::rename(user + ".held", user));
    QVERIFY(session->apply());
    HomeLayout reopened(&registry);
    QVERIFY(reopened.load(defaults, user));
    auto* reopenedSession = qobject_cast<StudioSession*>(reopened.beginStudio());
    QVERIFY(reopenedSession);
    QCOMPARE(reopenedSession->document()["studioAppearance"].toMap()["recentColors"].toList(), recent);
    QVERIFY(home.load(defaults, dir.filePath("other.json")));
    QVERIFY(!session->active());
    QVERIFY(home.studioAppearance()["focusColor"].toString() != QString("#ffffff"));
    QVERIFY(!session->acceptColorPreview("#123456"));
    QVERIFY(!home.committedDocument().contains("recentColors"));
}

void
StudioSessionTests::studioHiddenAndLockedStatesAreRecoverable()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"), user = dir.filePath("home.json");
    QVERIFY(home.load(defaults, user));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    QVERIFY(session->execute("convertGrid", {{"value", QVariantMap{{"columns", 8}, {"rows", 5}}}}));
    QVERIFY(session->execute("state", {{"id", "clock"}, {"key", "hidden"}, {"value", true}}));
    QVERIFY(session->execute("state", {{"id", "clock"}, {"key", "locked"}, {"value", true}}));
    const auto before = session->draft();
    const QVariantMap target{{"x", 0}, {"y", 3}, {"width", 1}, {"height", 1}};
    QVERIFY(!session->placementProposal("clock", target, "shift")["valid"].toBool());
    QVERIFY(!session->execute("place",
                              {{"placements", QVariantList{QVariantMap{{"id", "clock"}, {"placement", target}}}}}));
    QCOMPARE(session->draft(), before);
    QVERIFY(!session->execute("state", {{"id", "clock"}, {"key", "hidden"}, {"value", "yes"}}));
    QCOMPARE(session->draft(), before);
    auto* model = qobject_cast<HomeLayout*>(session->model());
    QVERIFY(model);
    QCOMPARE(model->items().size(), home.items().size());
    QVERIFY(session->apply());
    QVERIFY(!home.items().isEmpty());
    for (const auto& entry : home.items())
        QVERIFY(entry.toMap()["id"] != "clock");
    QVERIFY(session->execute("state", {{"id", "clock"}, {"key", "hidden"}, {"value", false}}));
    QVERIFY(session->execute("state", {{"id", "clock"}, {"key", "locked"}, {"value", false}}));
    QVERIFY(
        session->execute("place", {{"placements", QVariantList{QVariantMap{{"id", "clock"}, {"placement", target}}}}}));
    QVERIFY(session->apply());
    bool found = false;
    for (const auto& entry : home.items())
        if (entry.toMap()["id"] == "clock")
            found = true;
    QVERIFY(found);
}

void
StudioSessionTests::studioScrollingPagesAreBoundedAndAtomic()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"), user = dir.filePath("home.json");
    QVERIFY(home.load(defaults, user));
    auto document = QJsonDocument::fromJson(home.committedDocument()).object();
    document["categories"] = QJsonArray{};
    const QJsonObject composition{{"schemaVersion", 1},
                                  {"enabled", true},
                                  {"grid", QJsonObject{{"columns", 4}, {"rows", 3}, {"mode", "screen"}}}};
    document["composition"] = composition;
    document["components"] = QJsonArray{QJsonObject::fromVariantMap(
        ComponentSchema::component({{"id", "clock"},
                                    {"kind", "widget"},
                                    {"widgetType", "clock"},
                                    {"componentType", "clock"},
                                    {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}}},
                                   true))};
    writeFile(user, QJsonDocument(document).toJson());
    QVERIFY(home.load(defaults, user));
    const auto disk = readFile(user);
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    const auto start = session->draft();
    const QVariantMap below{{"x", 0}, {"y", 12}, {"width", 1}, {"height", 1}};
    QVERIFY(!session->placementProposal("clock", below, "shift")["valid"].toBool());
    QVERIFY(!session->execute("scrolling", {{"enabled", "yes"}}));
    QCOMPARE(session->draft(), start);
    QVERIFY(session->previewCommand("scrolling", {{"enabled", true}}));
    QCOMPARE(session->draft(), start);
    QCOMPARE(session->changeCount(), 0);
    session->cancelPreview();
    QCOMPARE(session->draft(), start);
    QVERIFY(session->execute("scrolling", {{"enabled", true}}));
    const auto enabled = session->draft();
    const auto proposal = session->placementProposal("clock", below, "shift");
    QVERIFY2(proposal["valid"].toBool(), qPrintable(proposal["reason"].toString()));
    QVERIFY2(session->execute("place", {{"placements", proposal["placements"]}}),
             qPrintable(session->errorString()));
    const auto longPage = session->draft();
    QVERIFY(!session->previewCommand("scrolling", {{"enabled", false}}));
    QCOMPARE(session->draft(), longPage);
    QVERIFY(!session->proposalModel());
    QVERIFY(!session
                 ->placementProposal("clock",
                                     {{"x", 0}, {"y", ComponentSchema::MaxScrollableRows}, {"width", 1}, {"height", 1}},
                                     "shift")["valid"]
                 .toBool());
    QVERIFY(!session->placementProposal("clock", {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 11}}, "shift")["valid"]
                 .toBool());
    QCOMPARE(readFile(user), disk);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), enabled);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    const auto addition = session->additionProposal("text", below, "shift", {}, {}, {}, {}, true);
    QVERIFY2(addition["valid"].toBool(), qPrintable(addition["reason"].toString()));
    QCOMPARE(session->draft(), start);
    QVERIFY(session->execute("batch", {{"commands", addition["commands"]}}));
    QCOMPARE(session->changeCount(), 1);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    QVERIFY(session->redo());
    QVERIFY(session->apply());
    HomeLayout reload(&registry);
    QVERIFY(reload.load(defaults, user));
    QVERIFY(reload.composition()["verticalScrolling"].toBool());
    QCOMPARE(reload.grid()["rows"].toInt(), 3);
    QCOMPARE(reload.components().size(), 2);
    QVERIFY(!session->canUndo());
    auto malformed = composition.toVariantMap();
    malformed["verticalScrolling"] = "true";
    QVERIFY(!ComponentSchema::validComposition(malformed));
    malformed["verticalScrolling"] = true;
    malformed["grid"] = QVariantMap{{"rows", 100}, {"columns", 4}, {"mode", "screen"}};
    QVERIFY(!ComponentSchema::validComposition(malformed));
}

void
StudioSessionTests::studioShiftCascadesAreBoundedAndAtomic()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"), user = dir.filePath("home.json");
    QVERIFY(home.load(defaults, user));
    auto document = QJsonDocument::fromJson(home.committedDocument()).object();
    document["categories"] = QJsonArray{};
    document["composition"] = QJsonObject{{"schemaVersion", 1},
                                          {"enabled", true},
                                          {"grid", QJsonObject{{"columns", 4}, {"rows", 3}, {"mode", "screen"}}}};
    const auto node = [](QString id, int x, int y, int width = 1)
    {
        return QJsonObject::fromVariantMap(ComponentSchema::component(
            QVariantMap{{"id", id},
                        {"kind", "widget"},
                        {"widgetType", "text"},
                        {"componentType", "text"},
                        {"name", id},
                        {"placement", QVariantMap{{"x", x}, {"y", y}, {"width", width}, {"height", 1}}}},
            true));
    };
    QJsonArray entries{node("moving", 0, 0), node("wide", 1, 0, 2)};
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 4; ++x)
            if (!(y == 0 && x < 3) && !(x == 3 && y == 2))
                entries.append(node(QString("cell-%1-%2").arg(y).arg(x), x, y));
    document["components"] = entries;
    writeFile(user, QJsonDocument(document).toJson());
    QVERIFY(home.load(defaults, user));
    QVERIFY2(home.errorString().isEmpty(), qPrintable(home.errorString()));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    const auto original = session->draft(), disk = readFile(user);
    const auto count = session->changeCount();
    const QVariantMap target{{"x", 1}, {"y", 0}, {"width", 2}, {"height", 1}};
    QVERIFY(!session->placementProposal("moving", target, "cancel")["valid"].toBool());
    QVERIFY(!session->placementProposal("moving", target, "swap")["valid"].toBool());
    QVERIFY(!session->placementProposal("moving", target, "unknown")["valid"].toBool());
    const auto shift = session->placementProposal("moving", target, "shift");
    QVERIFY2(shift["valid"].toBool(), qPrintable(shift["reason"].toString()));
    QVERIFY(shift["placements"].toList().size() > 2);
    QCOMPARE(session->placementProposal("moving", target, "shift"), shift);
    QCOMPARE(session->draft(), original);
    QCOMPARE(session->changeCount(), count);
    QVERIFY2(session->execute("place", {{"placements", shift["placements"]}}), qPrintable(session->errorString()));
    QCOMPARE(session->changeCount(), count + 1);
    QCOMPARE(readFile(user), disk);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), original);
    QVERIFY(!session->execute("duplicate", {{"id", "wide"}}));
    QCOMPARE(session->draft(), original);
    QCOMPARE(session->changeCount(), count);
    const auto fill = session->additionProposal("text", {{"x", 3}, {"y", 2}, {"width", 1}, {"height", 1}}, "cancel");
    QVERIFY2(fill["valid"].toBool(), qPrintable(fill["reason"].toString()));
    QVERIFY(session->execute("batch", {{"commands", fill["commands"]}}));
    const auto full = session->draft();
    const auto fullCount = session->changeCount();
    QVERIFY(!session->additionProposal("clock", {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}, "shift")["valid"]
                 .toBool());
    QCOMPARE(session->draft(), full);
    QCOMPARE(session->changeCount(), fullCount);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), original);
    QVERIFY(!session->placementProposal("moving", {{"x", 1}, {"y", 0}, {"width", 3}, {"height", 1}}, "shift")["valid"]
                 .toBool());
    QCOMPARE(session->draft(), original);
    QCOMPARE(session->changeCount(), count);
    QVERIFY(session->execute("state", {{"id", "wide"}, {"key", "locked"}, {"value", true}}));
    const auto locked = session->draft();
    QVERIFY(!session->placementProposal("moving", target, "shift")["valid"].toBool());
    QCOMPARE(session->draft(), locked);
    session->invalidate();
    document["composition"] = QJsonObject{{"schemaVersion", 1},
                                          {"enabled", true},
                                          {"grid", QJsonObject{{"columns", 16}, {"rows", 10}, {"mode", "screen"}}}};
    entries = QJsonArray{node("moving", 15, 9)};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            entries.append(node(QString("node-%1-%2").arg(y).arg(x), x, y));
    document["components"] = entries;
    writeFile(user, QJsonDocument(document).toJson());
    QVERIFY(home.load(defaults, user));
    session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    const auto bounded = session->draft();
    const auto tooMany =
        session->placementProposal("moving", {{"x", 0}, {"y", 0}, {"width", 8}, {"height", 8}}, "shift");
    QVERIFY(!tooMany["valid"].toBool());
    QVERIFY(tooMany["reason"].toString().contains("64"));
    QCOMPARE(session->draft(), bounded);
}

void
StudioSessionTests::studioSectionTransfersDuplicationAndOrdering()
{
    QTemporaryDir dir;
    AppRegistry registry;
    HomeLayout home(&registry);
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"), user = dir.filePath("home.json");
    QVERIFY(home.load(defaults, user));
    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
    QVERIFY(session);
    QVERIFY(session->execute(
        "convertGrid",
        {{"value", QVariantMap{{"columns", 8}, {"rows", 5}, {"mode", "sections"}}}, {"sectionHeight", 560}}));
    const auto beforeDuplicate = session->draft();
    const auto count = session->changeCount();
    QVERIFY2(session->execute("duplicate", {{"id", "clock"}}), qPrintable(session->errorString()));
    QCOMPARE(session->changeCount(), count + 1);
    const auto copy = QJsonDocument::fromJson(session->draft()).object()["components"].toArray().last().toObject();
    QCOMPARE(copy["categoryId"].toString(), QString("widgets"));
    QCOMPARE(copy["id"].toString(), QString("clock-copy"));
    QVERIFY(copy["placement"].isObject());
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), beforeDuplicate);
    const auto start = session->draft();
    const QVariantMap target{{"x", 0}, {"y", 4}, {"width", 1}, {"height", 1}};
    const auto transfer = session->placementProposal("clock", target, "cancel", {}, {{"categoryId", "inputs"}});
    QVERIFY2(transfer["valid"].toBool(), qPrintable(transfer["reason"].toString()));
    QCOMPARE(session->draft(), start);
    QVERIFY(session->execute("place", {{"placements", transfer["placements"]}}));
    const auto moved = QJsonDocument::fromJson(session->draft()).object()["components"].toArray().first().toObject();
    QCOMPARE(moved["categoryId"].toString(), QString("inputs"));
    QCOMPARE(moved["placement"].toObject()["y"].toInt(), 4);
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    const auto swap = session->placementProposal(
        "clock", {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}, "swap", {}, {{"categoryId", "inputs"}});
    QVERIFY2(swap["valid"].toBool(), qPrintable(swap["reason"].toString()));
    QVERIFY(session->execute("place", {{"placements", swap["placements"]}}));
    QVERIFY(session->undo());
    QCOMPARE(session->draft(), start);
    QVERIFY(!session->placementProposal("clock", target, "shift", {}, {{"categoryId", "missing"}})["valid"].toBool());
    QVERIFY(!session->execute(
        "place", {{"placements", QVariantList{QVariantMap{{"id", "clock"}, {"placement", target}, {"categoryId", "missing"}}}}}));
        QCOMPARE(session->draft(), start);
        QVERIFY(session->execute("orderRelative", {{"id", "movies"}, {"relativeId", "shows"}, {"before", false}}));
        const auto ordered = QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
        int movies = -1, shows = -1;
        for (int i = 0; i < ordered.size(); ++i)
        {
            const auto item = ordered[i].toObject();
            if (item["id"] == "movies")
                movies = i;
            if (item["id"] == "shows")
                shows = i;
        }
        QVERIFY(movies > shows);
        QCOMPARE(ordered[movies].toObject()["placement"].toObject()["x"].toInt(), 0);
        QVERIFY(session->undo());
        QCOMPARE(session->draft(), start);
        QVERIFY(!session->execute("orderRelative", {{"id", "clock"}, {"relativeId", "shows"}, {"before", true}}));
        QCOMPARE(session->draft(), start);
        QVERIFY(!QFile::exists(user));
        QVERIFY(session->execute("state", {{"id", "movies"}, {"key", "locked"}, {"value", true}}));
        const auto locked = session->draft();
        QVERIFY(!session->execute("orderRelative", {{"id", "movies"}, {"relativeId", "shows"}, {"before", false}}));
        QCOMPARE(session->draft(), locked);
}

void StudioSessionTests::studioNestedGroupsValidateAndReparentAtomically() {
        QTemporaryDir dir;
        AppRegistry registry;
        HomeLayout home(&registry);
        const auto user = dir.filePath("home.json");
        QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), user));
        auto* session = qobject_cast<StudioSession*>(home.beginStudio());
        QVERIFY(session);
        auto doc = QJsonDocument::fromJson(session->draft()).object();
        doc["components"] = QJsonArray{};
        doc["categories"] = QJsonArray{};
        doc["composition"] =
            QJsonObject{{"schemaVersion", 1},
                        {"enabled", true},
                        {"grid", QJsonObject::fromVariantMap(home.studioOptions()["defaultGrid"].toMap())}};
        QVERIFY2(session->pushEdit(QJsonDocument(doc).toJson()), qPrintable(session->errorString()));
        const auto empty = session->draft();
        const auto panel =
            session->additionProposal("input-panel", {{"x", 0}, {"y", 0}, {"width", 4}, {"height", 3}}, "cancel");
        QVERIFY2(panel["valid"].toBool(), qPrintable(panel["reason"].toString()));
        QCOMPARE(session->draft(), empty);
        QVERIFY(session->execute(
            "addResolved",
            {{"type", "input-panel"}, {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 4}, {"height", 3}}}}));
        const auto added = session->draft();
        const auto entries = QJsonDocument::fromJson(added).object()["components"].toArray();
        QCOMPARE(entries.size(), 9);
        QVERIFY2(session->execute("duplicatePage", {{"id", ""}, {"name", "Nested copy"}}),
                 qPrintable(session->errorString()));
        const auto copied = QJsonDocument::fromJson(session->draft())
                                .object()["pages"]
                                .toArray()
                                .last()
                                .toObject()["layout"]
                                .toObject()["components"]
                                .toArray();
        QCOMPARE(copied.size(), 9);
        QVERIFY(copied.first().toObject()["id"] != entries.first().toObject()["id"]);
        QCOMPARE(copied[1].toObject()["parentId"].toString(), copied[0].toObject()["id"].toString());
        QVERIFY(session->undo());
        QCOMPARE(session->draft(), added);
        const auto parent = entries.first().toObject()["id"].toString();
        const auto child = entries[1].toObject()["id"].toString();
        QCOMPARE(ComponentSchema::componentGrid(
                     entries, doc["composition"].toObject().toVariantMap(), {}, parent, {})["columns"]
                     .toInt(),
                 4);
        const auto transfer = session->placementProposal(
            child, {{"x", 5}, {"y", 0}, {"width", 1}, {"height", 1}}, "cancel", {}, {{"parentId", ""}});
        QVERIFY2(transfer["valid"].toBool(), qPrintable(transfer["reason"].toString()));
        QVERIFY2(session->execute("place", {{"placements", transfer["placements"]}}),
                 qPrintable(session->errorString()));
        QVERIFY(session->undo());
        QCOMPARE(session->draft(), added);
        QVERIFY(!session
                     ->placementProposal(parent,
                                         {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}},
                                         "cancel",
                                         {},
                                         {{"parentId", parent}})["valid"]
                     .toBool());
        QCOMPARE(session->draft(), added);
        auto broken = QJsonDocument::fromJson(added).object();
        auto badEntries = entries;
        auto root = badEntries[0].toObject();
        root["parentId"] = child;
        badEntries[0] = root;
        broken["components"] = badEntries;
        QVERIFY(!session->pushEdit(QJsonDocument(broken).toJson()));
        QCOMPARE(session->draft(), added);
        QVERIFY(session->execute("state", {{"id", parent}, {"key", "locked"}, {"value", true}}));
        const auto locked = session->draft();
        QVERIFY(!session->placementProposal(child, {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}, "shift")["valid"]
                     .toBool());
    QVERIFY(!session->execute(
        "place", {{"placements", QVariantList{QVariantMap{{"id", child},
                                                            {"placement", QVariantMap{{"x", 5}, {"y", 0}, {"width", 1}, {"height", 1}}},
                                                            {"parentId", ""}}}}}));
            QCOMPARE(session->draft(), locked);
            QVERIFY(session->undo());
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), empty);
            QVERIFY(!QFile::exists(user));
            QVERIFY(session->execute(
                "addResolved",
                {{"type", "text"}, {"placement", QVariantMap{{"x", 1}, {"y", 1}, {"width", 1}, {"height", 1}}}}));
            QVERIFY(session->execute(
                "addResolved",
                {{"type", "clock"}, {"placement", QVariantMap{{"x", 2}, {"y", 1}, {"width", 1}, {"height", 1}}}}));
            const auto beforeGroup = session->draft();
            const auto count = session->changeCount();
            QVERIFY(!session->execute("arrange", {{"ids", QVariantList{"text", "clock"}}, {"mode", "left"}}));
            QCOMPARE(session->draft(), beforeGroup);
            QVERIFY(!session->execute("group", {{"ids", QVariantList{"text", "text"}}}));
            QCOMPARE(session->draft(), beforeGroup);
            QVERIFY2(session->execute("group", {{"ids", QVariantList{"text", "clock"}}}),
                     qPrintable(session->errorString()));
            QCOMPARE(session->changeCount(), count + 1);
            const auto grouped = QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
            QCOMPARE(grouped.size(), 3);
            QCOMPARE(grouped[0].toObject()["componentType"].toString(), QString("container"));
            QCOMPARE(grouped[1].toObject()["parentId"].toString(), grouped[0].toObject()["id"].toString());
            QCOMPARE(grouped[1].toObject()["placement"].toObject()["x"].toInt(), 0);
            QCOMPARE(grouped[2].toObject()["placement"].toObject()["x"].toInt(), 1);
            const auto groupId = grouped[0].toObject()["id"].toString();
            const auto groupedBytes = session->draft();
            QVERIFY2(session->execute("duplicate", {{"id", groupId}}), qPrintable(session->errorString()));
            const auto duplicated = QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
            QCOMPARE(duplicated.size(), 6);
            QCOMPARE(duplicated[4].toObject()["parentId"].toString(), duplicated[3].toObject()["id"].toString());
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), groupedBytes);
            QVERIFY2(session->execute("remove", {{"id", groupId}}), qPrintable(session->errorString()));
            QVERIFY(QJsonDocument::fromJson(session->draft()).object()["components"].toArray().isEmpty());
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), groupedBytes);
            QVERIFY2(session->execute("ungroup", {{"id", groupId}}), qPrintable(session->errorString()));
            QCOMPARE(session->draft(), beforeGroup);
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), groupedBytes);
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), beforeGroup);
            QVERIFY(!QFile::exists(user));
            auto group = QJsonObject::fromVariantMap(session->componentPrototype("container")["root"].toMap());
            group["placement"] = QJsonObject{{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}};
            auto style = group["presentation"].toObject(), local = style["layout"].toObject();
            local["columns"] = 1;
            local["rows"] = 1;
            style["layout"] = local;
            group["presentation"] = style;
            QJsonArray deep;
            for (int level = 0; level < 4; ++level)
            {
                auto node = group;
                node["id"] = QString("depth-%1").arg(level);
                if (level)
                    node["parentId"] = QString("depth-%1").arg(level - 1);
                deep.append(node);
            }
            QString error;
            QVERIFY2(ComponentSchema::hierarchyFits(deep, doc["composition"].toObject().toVariantMap(), {}, &error),
                     qPrintable(error));
            auto fifth = group;
            fifth["id"] = "depth-4";
            fifth["parentId"] = "depth-3";
            deep.append(fifth);
            QVERIFY(!ComponentSchema::hierarchyFits(deep, doc["composition"].toObject().toVariantMap(), {}, &error));
            QVERIFY(error.contains("four"));
            auto invalid = local.toVariantMap();
            invalid["columns"] = 17;
            QVERIFY(!ComponentSchema::validChildLayout(invalid));
            invalid = local.toVariantMap();
            invalid["mode"] = "script";
            QVERIFY(!ComponentSchema::validChildLayout(invalid));
}

void StudioSessionTests::studioFiveColorThemeLinksOverridesAndInvalidRollback() {
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout live(&registry);
            const auto path = dir.filePath("layout.json");
            QVERIFY(live.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
            QVERIFY(ThemeColors::valid(live.appearance().value("colorTheme")));
            QCOMPARE(live.backgroundStart(), live.backgroundEnd());
            QCOMPARE(live.appearance().value("textColor"), live.appearance().value("mutedColor"));
            auto* session = qobject_cast<StudioSession*>(live.beginStudio());
            QVERIFY(session);
            const auto baseline = session->draft();
            const auto disk = readFile(path);
            const QVariantMap colors{{"background", "#101e2c"},
                                     {"cards", "#20384a"},
                                     {"text", "#f0f7ff"},
                                     {"highlight", "#89d6ff"},
                                     {"accent", "#76bddd"}};
            const QVariantMap palette{{"schemaVersion", 1}, {"colors", colors}};
            QVariantMap values{{"colorTheme", palette}};
            for (const auto& keys : ThemeColors::roles())
                for (const auto& key : keys)
                    values[key] = QVariant();
            QVERIFY(session->previewCommand("pagePatch", {{"scope", "theme"}, {"value", values}}));
            QCOMPARE(session->draft(), baseline);
            QCOMPARE(readFile(path), disk);
            const auto preview = qobject_cast<HomeLayout*>(session->proposalModel());
            QVERIFY(preview);
            QCOMPARE(preview->appearance().value("labelColor").toString(), QString("#f0f7ff"));
            session->cancelPreview();
            QCOMPARE(session->draft(), baseline);
            QVERIFY(session->execute("pagePatch", {{"scope", "theme"}, {"value", values}}));
            QVERIFY(session->execute("pagePatch",
                                     {{"scope", "theme"}, {"value", QVariantMap{{"headingColor", "#ffeedd"}}}}));
            auto changed = colors;
            changed["text"] = "#ffffff";
            QVERIFY(session->execute(
                "pagePatch",
                {{"scope", "theme"},
                 {"value", QVariantMap{{"colorTheme", QVariantMap{{"schemaVersion", 1}, {"colors", changed}}}}}}));
            auto model = qobject_cast<HomeLayout*>(session->model());
            QVERIFY(model);
            QCOMPARE(model->appearance().value("headingColor").toString(), QString("#ffeedd"));
            QCOMPARE(model->appearance().value("bodyColor").toString(), QString("#ffffff"));
            QVERIFY(session->execute("pagePatch",
                                     {{"scope", "theme"}, {"value", QVariantMap{{"headingColor", QVariant()}}}}));
            model = qobject_cast<HomeLayout*>(session->model());
            QCOMPARE(model->appearance().value("headingColor").toString(), QString("#ffffff"));
            const auto before = session->draft();
            auto invalid = palette;
            invalid["schemaVersion"] = 2;
            QVERIFY(
                !session->execute("pagePatch", {{"scope", "theme"}, {"value", QVariantMap{{"colorTheme", invalid}}}}));
            QCOMPARE(session->draft(), before);
            QVERIFY(!session->errorString().isEmpty());
            QCOMPARE(readFile(path), disk);
            invalid = palette;
            auto badColors = colors;
            badColors["script"] = "red";
            invalid["colors"] = badColors;
            QVERIFY(!ThemeColors::valid(invalid));
            badColors = colors;
            badColors["text"] = "#80ffffff";
            invalid["colors"] = badColors;
            QVERIFY(!ThemeColors::valid(invalid));
            while (session->canUndo())
                QVERIFY(session->undo());
            QCOMPARE(session->draft(), baseline);
}

void StudioSessionTests::studioCatalogLinksInspectorAndStagedArtwork() {
            QVERIFY(ComponentSchema::validPresentation(
                {{"libraryKind", "movies"}, {"libraryView", "playlist"}, {"libraryValue", "Family films.xsp"}}, true));
            QVERIFY(!ComponentSchema::validPresentation(
                {{"libraryView", "playlist"}, {"libraryValue", "../private.xsp"}}, true));
            QVERIFY(!ComponentSchema::validPresentation(
                {{"libraryView", "playlist"}, {"libraryValue", "https://private.xsp"}}, true));
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout home(&registry);
            const auto user = dir.filePath("profile/home.json");
            QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), user));
            const auto options = home.studioOptions();
            QVERIFY(options["catalog"].toList().size() >= 12);
            QVERIFY(!options["inspector"].toMap()["fields"].toList().isEmpty());
            auto* session = qobject_cast<StudioSession*>(home.beginStudio());
            QVERIFY(session);
            auto doc = QJsonDocument::fromJson(session->draft()).object();
            doc["components"] = QJsonArray{};
            doc["categories"] = QJsonArray{};
            doc["composition"] = QJsonObject{{"schemaVersion", 1},
                                             {"enabled", true},
                                             {"grid", QJsonObject::fromVariantMap(options["defaultGrid"].toMap())}};
            QVERIFY(session->pushEdit(QJsonDocument(doc).toJson()));
            const auto empty = session->draft();
            for (const auto& type : QStringList{"button",
                                                "image",
                                                "shape",
                                                "text",
                                                "library",
                                                "recently-viewed",
                                                "music-library",
                                                "tv-guide",
                                                "carousel",
                                                "app-bar",
                                                "page-menu"})
            {
                const auto prototype = session->componentPrototype(type);
                QVERIFY2(!prototype.isEmpty(), qPrintable(type + ": " + session->errorString()));
                QCOMPARE(session->draft(), empty);
            }
            const auto appPrototype = session->componentPrototype("app-bar");
            const auto historyPrototype = session->componentPrototype("recently-viewed");
            const auto historyRoot = historyPrototype["root"].toMap();
            QCOMPARE(historyRoot["family"].toString(), QString("recently-viewed"));
            QCOMPARE(historyRoot["source"].toMap()["kind"].toString(), QString("recently-viewed"));
            auto selectedHistory = historyRoot;
            auto selectedStyle = historyRoot["presentation"].toMap();
            selectedStyle["historyApps"] = QVariantList{"freetube", "plex"};
            selectedHistory["presentation"] = selectedStyle;
            QVERIFY(!ComponentSchema::component(selectedHistory, true).isEmpty());
            selectedStyle["historyApps"] = QVariantList{"plex", "plex"};
            selectedHistory["presentation"] = selectedStyle;
            QVERIFY(ComponentSchema::component(selectedHistory, true).isEmpty());
            selectedStyle["historyApps"] = QVariantList{"../private"};
            selectedHistory["presentation"] = selectedStyle;
            QVERIFY(ComponentSchema::component(selectedHistory, true).isEmpty());
            const auto musicRoot = session->componentPrototype("music-library")["root"].toMap();
            QCOMPARE(musicRoot["source"].toMap()["kind"].toString(), QString("music"));
            auto wrongMusic = musicRoot;
            auto musicStyle = musicRoot["presentation"].toMap();
            musicStyle["libraryView"] = "continue";
            wrongMusic["presentation"] = musicStyle;
            QVERIFY(ComponentSchema::component(wrongMusic, true).isEmpty());
            wrongMusic = musicRoot;
            wrongMusic.remove("family");
            QVERIFY(ComponentSchema::component(wrongMusic, true).isEmpty());
            auto wrongHistory = historyRoot;
            auto historyStyle = historyRoot["presentation"].toMap();
            historyStyle["libraryView"] = "recent";
            wrongHistory["presentation"] = historyStyle;
            QVERIFY(ComponentSchema::component(wrongHistory, true).isEmpty());
            historyStyle["libraryView"] = "watched";
            wrongHistory["presentation"] = historyStyle;
            wrongHistory.remove("family");
            QVERIFY(ComponentSchema::component(wrongHistory, true).isEmpty());
            QCOMPARE(appPrototype["components"].toList().size(), 1);
            auto appNode = appPrototype["root"].toMap();
            appNode["appCollection"] = QVariantMap{{"mode", "all"}, {"ids", QVariantList{}}, {"showStore", false}};
            QVERIFY(!ComponentSchema::component(appNode, true).isEmpty());
            appNode["appCollection"] = QVariantMap{{"mode", "all"}, {"ids", QVariantList{}}, {"showStore", "false"}};
            QVERIFY(ComponentSchema::component(appNode, true).isEmpty());
            appNode["appCollection"] = QVariantMap{{"mode", "all"}, {"ids", QVariantList{}}, {"section", "Games"}};
            QVERIFY(!ComponentSchema::component(appNode, true).isEmpty());
            appNode["appCollection"] = QVariantMap{{"mode", "all"}, {"ids", QVariantList{}}, {"section", ""}};
            QVERIFY(ComponentSchema::component(appNode, true).isEmpty());
            appNode = appPrototype["root"].toMap();
            QCOMPARE(appNode["appCollection"].toMap()["mode"].toString(), QString("all"));
            appNode["appCollection"] =
                QVariantMap{{"mode", "selected"}, {"ids", QVariantList{"duplicate", "duplicate"}}};
            QVERIFY(ComponentSchema::component(appNode, true).isEmpty());
            appNode["appCollection"] = QVariantMap{{"mode", "all"}, {"ids", QVariantList{"unsafe/identity"}}};
            QVERIFY(ComponentSchema::component(appNode, true).isEmpty());
            const QVariantMap box{{"x", 0}, {"y", 0}, {"width", 2}, {"height", 1}};
            QVERIFY(session->execute("addResolved", {{"type", "button"}, {"placement", box}}));
            QVERIFY(session->execute(
                "patch",
                {{"id", "button"},
                 {"value", QVariantMap{{"actions", QVariantList{QVariantMap{{"type", "root-page"}}}}}}}));
            const auto linked = session->draft();
            QVERIFY(!session->execute(
                "patch",
                {{"id", "button"},
                 {"value",
                  QVariantMap{{"actions", QVariantList{QVariantMap{{"type", "open-page"}, {"pageId", "absent"}}}}}}}));
            QCOMPARE(session->draft(), linked);
            QVERIFY(session->execute(
                "patch",
                {{"id", "button"}, {"value", QVariantMap{{"actions", QVariantList{}}}}}));
                    QVERIFY(!QJsonDocument::fromJson(session->draft())
                                 .object()["components"]
                                 .toArray()
                                 .first()
                                 .toObject()
                                 .contains("actions"));
                    QVERIFY(session->execute(
                        "patch",
                        {{"id", "button"},
                         {"value", QVariantMap{{"presentation", QVariantMap{{"surfaceColor", "#123456"}}}}}}));
                    QVERIFY(session->execute(
                        "patch",
                        {{"id", "button"},
                         {"value", QVariantMap{{"presentation", QVariantMap{{"surfaceColor", QVariant()}}}}}}));
                    QVERIFY(!QJsonDocument::fromJson(session->draft())
                                 .object()["components"]
                                 .toArray()
                                 .first()
                                 .toObject()["presentation"]
                                 .toObject()
                                 .contains("surfaceColor"));
                    QImage image(32, 24, QImage::Format_RGB32);
                    image.fill(QColor("#56789a"));
                    const auto source = dir.filePath("chosen.png");
                    QVERIFY(image.save(source));
                    const auto browseBaseline = session->draft();
                    const auto listing = session->browseArtwork(dir.path());
                    QVERIFY2(!listing.isEmpty(), qPrintable(session->errorString()));
                    QVERIFY(!listing["entries"].toList().isEmpty());
                    QCOMPARE(session->draft(), browseBaseline);
                    QVERIFY(session->browseArtwork("../unsafe").isEmpty());
                    QVERIFY(session->browseArtwork(dir.path(), 8).isEmpty());
                    QVERIFY(session->artworkPreview(source).startsWith("file:"));
                    QCOMPARE(session->draft(), browseBaseline);
                    QVERIFY(!QFile::exists(user));
                    QVERIFY(session->stageArtwork(dir.path()).isEmpty());
#ifdef Q_OS_UNIX
                    const auto pipe = dir.filePath("input-pipe.gif");
                    QVERIFY(::mkfifo(pipe.toLocal8Bit().constData(), 0600) == 0);
                    QVERIFY(session->stageArtwork(pipe).isEmpty());
#endif
                    QVERIFY(QDir().mkpath(dir.filePath("many")));
                    for (int i = 0; i < 513; ++i)
                    {
                        QFile f(dir.filePath("many/" + QString::number(i)));
                        QVERIFY(f.open(QIODevice::WriteOnly));
                    }
                    QVERIFY(session->browseArtwork(dir.filePath("many")).isEmpty());
                    QVERIFY(session->errorString().contains("512"));
                    for (const auto& field : QStringList{"bodySize", "bodyPadding", "bodyLines", "bodyLineHeight"})
                    {
                        QVERIFY(!ComponentSchema::validPresentation({{field, "3"}}));
                        QVERIFY(!ComponentSchema::validPresentation({{field, -1}}));
                        QVERIFY(!ComponentSchema::validPresentation({{field, 1000}}));
                    }
                    QVERIFY(ComponentSchema::validPresentation({{"bodySize", 32},
                                                                {"bodyBold", true},
                                                                {"bodyWrap", false},
                                                                {"bodyAlign", "right"},
                                                                {"bodyLines", 6},
                                                                {"bodyPadding", 12},
                                                                {"bodyLineHeight", 1.2},
                                                                {"bodyColor", "#123456"}}));
                    QVERIFY(!ComponentSchema::validPresentation({{"bodyAlign", "script"}}));
                    QVERIFY(!ComponentSchema::validPresentation({{"bodyBold", "true"}}));
                    const auto relative = session->stageArtwork(source);
                    QVERIFY2(relative.startsWith("artwork/studio-"), qPrintable(session->errorString()));
                    QVERIFY(!QFile::exists(dir.filePath("profile/" + relative)));
                    QVERIFY(!QFile::exists(user));
                    QVERIFY(session->execute("patch",
                                             {{"id", "button"}, {"value", QVariantMap{{"customLogo", relative}}}}));
                    auto* model = qobject_cast<HomeLayout*>(session->model());
                    QVERIFY(model);
                    const auto staged = QUrl(model->items().first().toMap()["logoSource"].toString()).toLocalFile();
                    QVERIFY(QFile::exists(staged));
                    QVERIFY(!staged.startsWith(dir.filePath("profile")));
                    QVERIFY(session->execute("pagePatch",
                                             {{"scope", "theme"}, {"value", QVariantMap{{"focusColor", QVariant()}}}}));
                    QVERIFY(
                        !QJsonDocument::fromJson(session->draft()).object()["theme"].toObject().contains("focusColor"));
                    QVERIFY(session->execute("rename", {{"id", "button"}, {"name", "Artwork button"}}));
                    QVERIFY(
                        !QJsonDocument::fromJson(session->draft()).object()["theme"].toObject().contains("focusColor"));
                    const auto beforeFailure = session->draft();
                    QVERIFY(QDir().mkpath(dir.filePath("profile")));
                    QFile blocked(dir.filePath("profile/artwork"));
                    QVERIFY(blocked.open(QIODevice::WriteOnly));
                    blocked.write("blocked");
                    blocked.close();
                    QVERIFY(!session->apply("button"));
                    QCOMPARE(session->draft(), beforeFailure);
                    QVERIFY(!QFile::exists(user));
                    QCOMPARE(home.focusOrigin(), QString());
                    QVERIFY(QFile::remove(blocked.fileName()));
                    QVERIFY(session->apply("button"));
                    QVERIFY(QFile::exists(dir.filePath("profile/" + relative)));
                    QCOMPARE(readFile(dir.filePath("profile/" + relative)), readFile(source));
                    QVERIFY(
                        !QJsonDocument::fromJson(readFile(user)).object()["theme"].toObject().contains("focusColor"));
                    QVERIFY(session->stageArtwork("https://example.com/image.png").isEmpty());
                    QVERIFY(session->stageArtwork(user).isEmpty());
                    session->invalidate();
                    QVERIFY(!QFile::exists(staged));
                    QVERIFY(QFile::exists(dir.filePath("profile/" + relative)));
                    auto invalid = options["componentPresets"].toMap()["image"].toMap();
                    invalid["actions"] = QVariantList{QVariantMap{{"type", "exec"}, {"target", "bad"}}};
                    QVERIFY(ComponentSchema::component(invalid, true).isEmpty());
                    invalid = options["componentPresets"].toMap()["library"].toMap();
                    auto style = invalid["presentation"].toMap();
                    style["libraryLimit"] = 51;
                    invalid["presentation"] = style;
                    QVERIFY(ComponentSchema::component(invalid, true).isEmpty());
                }

                void StudioSessionTests::studioOrderedInsertionPreservesSlotsAndIsAtomic() {
                    QTemporaryDir dir;
                    AppRegistry registry;
                    HomeLayout home(&registry);
                    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json"),
                               user = dir.filePath("ordered.json");
                    QVERIFY(home.load(defaults, user));
                    auto* setup = qobject_cast<StudioSession*>(home.beginStudio());
                    QVERIFY(setup);
                    auto group = QJsonObject::fromVariantMap(setup->componentPrototype("container")["root"].toMap());
                    group["id"] = "ordered";
                    group["placement"] = QJsonObject{{"x", 0}, {"y", 0}, {"width", 4}, {"height", 2}};
                    auto style = group["presentation"].toObject();
                    style["layout"] = QJsonObject{{"schemaVersion", 1},
                                                  {"mode", "row"},
                                                  {"columns", 6},
                                                  {"rows", 1},
                                                  {"visibleColumns", 2},
                                                  {"wrap", true},
                                                  {"gap", 12},
                                                  {"padding", 0}};
                    group["presentation"] = style;
                    auto doc = QJsonDocument::fromJson(home.committedDocument()).object();
                    doc["categories"] = QJsonArray{};
                    doc["composition"] =
                        QJsonObject{{"schemaVersion", 1},
                                    {"enabled", true},
                                    {"grid", QJsonObject{{"columns", 4}, {"rows", 3}, {"mode", "screen"}}}};
                    QJsonArray nodes{group};
                    for (int i = 0; i < 3; ++i)
                    {
                        auto node = QJsonObject::fromVariantMap(
                            ComponentSchema::component({{"id", QString("ordered-%1").arg(i)},
                                                        {"kind", "widget"},
                                                        {"widgetType", "text"},
                                                        {"componentType", "text"},
                                                        {"name", QString("Child %1").arg(i)},
                                                        {"parentId", "ordered"},
                                                        {"placement",
                                                         QVariantMap{{"x",
                                                                      i == 0   ? 0
                                                                      : i == 1 ? 3
                                                                               : 5},
                                                                     {"y", 0},
                                                                     {"width", i == 0 ? 2 : 1},
                                                                     {"height", 1}}}},
                                                       true));
                        QVERIFY(!node.isEmpty());
                        nodes.append(node);
                    }
                    auto outside = QJsonObject::fromVariantMap(ComponentSchema::component(
                        {{"id", "outside"},
                         {"kind", "widget"},
                         {"widgetType", "text"},
                         {"componentType", "text"},
                         {"name", "Outside"},
                         {"placement", QVariantMap{{"x", 0}, {"y", 2}, {"width", 1}, {"height", 1}}}},
                        true));
                    nodes.append(outside);
                    doc["components"] = nodes;
                    setup->invalidate();
                    writeFile(user, QJsonDocument(doc).toJson());
                    QVERIFY(home.load(defaults, user));
                    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
                    QVERIFY(session);
                    const auto before = session->draft(), disk = readFile(user);
                    const auto count = session->changeCount();
                    for (const auto& args : QVariantList{
                             QVariantMap{{"id", "ordered-0"}, {"relativeId", "outside"}, {"before", false}},
                             QVariantMap{{"id", "ordered-0"}, {"relativeId", "ordered-0"}, {"before", false}},
                             QVariantMap{{"id", "ordered-0"}, {"relativeId", "ordered-2"}, {"before", "false"}},
                             QVariantMap{{"id", "outside"}, {"relativeId", "ordered-2"}, {"before", false}}})
                    {
                        QVERIFY(!session->execute("insertRelative", args.toMap()));
                        QVERIFY(!session->errorString().isEmpty());
                        QCOMPARE(session->draft(), before);
                        QCOMPARE(session->changeCount(), count);
                    }
                    const QVariantMap insert{{"id", "ordered-0"}, {"relativeId", "ordered-2"}, {"before", false}};
                    QVERIFY2(session->previewCommand("insertRelative", insert), qPrintable(session->errorString()));
                    QCOMPARE(session->draft(), before);
                    QCOMPARE(session->changeCount(), count);
                    QVERIFY(session->acceptPreview());
                    QCOMPARE(session->changeCount(), count + 1);
                    const auto inserted = QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
                    QCOMPARE(inserted[0], nodes[0]);
                    QCOMPARE(inserted[4], nodes[4]);
                    for (int i = 0; i < 3; ++i)
                    {
                        auto expected = nodes[i + 1].toObject(), p = expected["placement"].toObject();
                        p["x"] = i == 0 ? 4 : i == 1 ? 0 : 2;
                        expected["placement"] = p;
                        QCOMPARE(inserted[i + 1].toObject(), expected);
                    }
                    QCOMPARE(readFile(user), disk);
                    QVERIFY(session->undo());
                    QCOMPARE(session->draft(), before);
                    QVERIFY(session->execute("state", {{"id", "ordered-1"}, {"key", "locked"}, {"value", true}}));
                    const auto locked = session->draft();
                    const auto lockedCount = session->changeCount();
                    QVERIFY(!session->execute("insertRelative", insert));
                    QCOMPARE(session->draft(), locked);
                    QCOMPARE(session->changeCount(), lockedCount);
                    QCOMPARE(readFile(user), disk);
                    QVERIFY(session->undo());
                    QCOMPARE(session->draft(), before);
                    QVERIFY(session->execute("orderRelative", insert));
                    for (const auto& entry : QJsonDocument::fromJson(session->draft()).object()["components"].toArray())
                        for (const auto& original : nodes)
                            if (entry.toObject()["id"] == original.toObject()["id"])
                                QCOMPARE(entry.toObject()["placement"], original.toObject()["placement"]);
                    QVERIFY(session->undo());
                    QCOMPARE(session->draft(), before);
                }

                void StudioSessionTests::studioOverlayMovementKeepsNeighborsAndBounds() {
                    QTemporaryDir dir;
                    AppRegistry registry;
                    HomeLayout home(&registry);
                    const auto user = dir.filePath("layers.json");
                    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), user));
                    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
                    QVERIFY(session);
                    QVERIFY(session->execute("convertGrid", {{"value", QVariantMap{{"columns", 8}, {"rows", 5}}}}));
                    QVERIFY(session->execute("grid", {{"value", QVariantMap{{"overlap", true}}}}));
                    QVERIFY(session->execute("state", {{"id", "movies"}, {"key", "locked"}, {"value", true}}));
                    const auto before = session->draft();
                    const auto count = session->changeCount();
                    const QVariantMap target{{"x", 1}, {"y", 0}, {"width", 1}, {"height", 1}};
                    for (const auto& strategy : QStringList{"shift", "cancel"})
                    {
                        const auto proposal = session->placementProposal("clock", target, strategy);
                        QVERIFY2(proposal["valid"].toBool(), qPrintable(proposal["reason"].toString()));
                        QCOMPARE(proposal["placements"].toList().size(), 1);
                        QCOMPARE(session->draft(), before);
                        QCOMPARE(session->changeCount(), count);
                        QVERIFY(session->execute("place", {{"placements", proposal["placements"]}}));
                        QCOMPARE(session->changeCount(), count + 1);
                        const auto original = QJsonDocument::fromJson(before).object()["components"].toArray(),
                                   moved = QJsonDocument::fromJson(session->draft()).object()["components"].toArray();
                        for (int i = 0; i < original.size(); ++i)
                            if (original[i].toObject()["id"] != "clock")
                                QCOMPARE(moved[i], original[i]);
                        const auto overlapping = session->draft();
                        QVERIFY(!session->execute("grid", {{"value", QVariantMap{{"overlap", false}}}}));
                        QCOMPARE(session->draft(), overlapping);
                        QCOMPARE(session->changeCount(), count + 1);
                        QVERIFY(session->undo());
                        QCOMPARE(session->draft(), before);
                    }
                    QVERIFY(!session->placementProposal("clock", target, "swap")["valid"].toBool());
                    QVERIFY(!session
                                 ->placementProposal(
                                     "clock", {{"x", 8}, {"y", 0}, {"width", 1}, {"height", 1}}, "shift")["valid"]
                                 .toBool());
                    QVERIFY(session->execute("state", {{"id", "clock"}, {"key", "locked"}, {"value", true}}));
                    const auto locked = session->draft();
                    QVERIFY(!session->placementProposal("clock", target, "shift")["valid"].toBool());
                    QCOMPARE(session->draft(), locked);
                    QVERIFY(!QFile::exists(user));
                }

                void StudioSessionTests::studioHeaderValidationAndDraftRollback() {
                    const QVariantMap style{{"schemaVersion", 1},
                                            {"height", 72},
                                            {"titleSize", 40},
                                            {"padding", 4},
                                            {"opacity", 0.4},
                                            {"showDate", true},
                                            {"controlDisplay", "both"},
                                            {"timeFormat", "HH:mm"},
                                            {"dateFormat", "yyyy-MM-dd"},
                                            {"order", QVariantList{"clock", "title", "controls"}},
                                            {"position", "bottom"}};
                    QVariantMap composition{
                        {"schemaVersion", 1}, {"headerMode", "identity"}, {"showClock", true}, {"header", style}};
                    QVERIFY(ComponentSchema::validComposition(composition));
                    const QVariantList malformed{QVariantMap{{"schemaVersion", 2}},
                                                 QVariantMap{{"height", "72"}},
                                                 QVariantMap{{"opacity", 1.1}},
                                                 QVariantMap{{"height", 42}, {"titleSize", 96}},
                                                 QVariantMap{{"order", QVariantList{"clock", "clock", "controls"}}},
                                                 QVariantMap{{"customLogo", "../private.png"}},
                                                 QVariantMap{{"customLogo", "https://example.invalid/logo.png"}},
                                                 QVariantMap{{"timeFormat", "arbitrary"}},
                                                 QVariantMap{{"position", "outside"}},
                                                 QVariantMap{{"backgroundColor", "transparent"}},
                                                 QVariantMap{{"script", "anything"}}};
                    for (const auto& entry : malformed)
                    {
                        auto invalid = composition;
                        invalid["header"] = entry;
                        QVERIFY(!ComponentSchema::validComposition(invalid));
                    }
                    QTemporaryDir dir;
                    AppRegistry registry;
                    QVERIFY(registry.loadFromData(apps));
                    HomeLayout home(&registry);
                    const auto defaultPath = QFINDTESTDATA("../config/default-home-layout.json");
                    const auto legacyPath = dir.filePath("legacy-home.json"), path = dir.filePath("home.json");
                    const auto legacyBytes = readFile(defaultPath);
                    QVERIFY(!legacyBytes.isEmpty());
                    writeFile(legacyPath, legacyBytes);
                    HomeLayout legacyHome(&registry);
                    QVERIFY(legacyHome.load(defaultPath, legacyPath));
                    QCOMPARE(readFile(legacyPath), legacyBytes);
                    QVERIFY(home.load(defaultPath, path));
                    const auto otherPath = dir.filePath("other-home.json");
                    HomeLayout other(&registry);
                    QVERIFY(other.load(defaultPath, otherPath));
                    const auto otherBytes = readFile(otherPath);
                    const auto otherHeader = other.composition().value("header");
                    QVERIFY(!home.studioOptions().value("headerDefaults").toMap().isEmpty());
                    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
                    QVERIFY(session);
                    const auto before = session->draft();
                    const QVariantMap gallery{{"layout", "list"},
                                              {"columns", 4},
                                              {"density", "spacious"},
                                              {"sort", "favorites"},
                                              {"showArtwork", false},
                                              {"showLabels", true},
                                              {"filterEnabled", true},
                                              {"favoritesEnabled", true},
                                              {"title", "Installed Apps"}};
                    const QVariantMap pageComposition{
                        {"headerMode", "identity"}, {"header", style}, {"appGallery", gallery}};
                    QVERIFY2(
                        session->previewCommand("pagePatch", {{"scope", "composition"}, {"value", pageComposition}}),
                        qPrintable(session->errorString()));
                    QCOMPARE(session->draft(), before);
                    QCOMPARE(session->changeCount(), 0);
                    QVERIFY(!QFile::exists(path));
                    session->cancelPreview();
                    QCOMPARE(session->draft(), before);
                    QVERIFY(session->execute("pagePatch", {{"scope", "composition"}, {"value", pageComposition}}));
                    QCOMPARE(session->changeCount(), 1);
                    const auto valid = session->draft();
                    QVERIFY(!session->execute("pagePatch",
                                              {{"scope", "composition"},
                                               {"value", QVariantMap{{"header", QVariantMap{{"schemaVersion", 2}}}}}}));
                    QVERIFY(!session->execute("pagePatch",
                                              {{"scope", "composition"},
                                               {"value", QVariantMap{{"appGallery", QVariantMap{{"columns", 1}}}}}}));
                    QCOMPARE(session->draft(), valid);
                    QCOMPARE(session->changeCount(), 1);
                    QVERIFY(!QFile::exists(path));
                    QVERIFY(session->undo());
                    QCOMPARE(session->draft(), before);
                    QVERIFY(session->execute("pagePatch", {{"scope", "composition"}, {"value", pageComposition}}));
                    QVERIFY(session->apply(QString()));
                    QCOMPARE(readFile(otherPath), otherBytes);
                    QCOMPARE(other.composition().value("header"), otherHeader);
                    HomeLayout reopened(&registry);
                    QVERIFY(reopened.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
                    QCOMPARE(reopened.composition().value("header").toMap(), style);
                    QCOMPARE(reopened.composition().value("appGallery").toMap(), gallery);
                    const auto skin = reopened.saveSkin("Bottom header");
                    QVERIFY(!skin.isEmpty());
                    auto changed = reopened.composition();
                    changed["header"] = QVariantMap{{"schemaVersion", 1}, {"position", "top"}};
                    QVERIFY(reopened.setComposition(changed));
                    QVERIFY(reopened.applySkin(skin));
                    QCOMPARE(reopened.composition().value("header").toMap(), style);
                    QCOMPARE(readFile(otherPath), otherBytes);
                    QCOMPARE(other.composition().value("header"), otherHeader);
                }

                void StudioSessionTests::studioPageDefaultsAreDraftOnlyAndScoped() {
                    QTemporaryDir dir;
                    AppRegistry registry;
                    QVERIFY(registry.loadFromData(apps));
                    HomeLayout home(&registry);
                    const auto path = dir.filePath("home.json");
                    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
                    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
                    QVERIFY(session);
                    QVERIFY(session->execute("newPage", {{"name", "Reading room"}}));
                    QVERIFY(session->execute(
                        "pagePatch", {{"scope", "theme"}, {"value", QVariantMap{{"backgroundStart", "#123456"}}}}));
                    const auto before = session->draft();
                    const auto beforeDoc = session->document();
                    const auto count = session->changeCount();
                    QVERIFY(!session->execute("restorePageDefaults", {{"scope", "invalid"}}));
                    QCOMPARE(session->draft(), before);
                    QVERIFY(!session->execute("restorePageDefaults", {}, "missing-page"));
                    QCOMPARE(session->draft(), before);
                    QVERIFY(session->execute("restorePageDefaults", {}));
                    QCOMPARE(session->document()["pages"], beforeDoc["pages"]);
                    QCOMPARE(session->changeCount(), count + 1);
                    QVERIFY(!QFile::exists(path));
                    QVERIFY(session->undo());
                    QCOMPARE(session->draft(), before);
                    const auto pages = beforeDoc["pages"].toList();
                    QVERIFY(!pages.isEmpty());
                    const auto id = pages.first().toMap()["id"].toString();
                    QVERIFY(session->execute("restorePageDefaults", {}, id));
                    auto original = beforeDoc, changed = session->document();
                    original.remove("pages");
                    changed.remove("pages");
                    QCOMPARE(changed, original);
                    QVERIFY(!QFile::exists(path));
                    QVERIFY(session->undo());
                    QCOMPARE(session->draft(), before);
                }

                void StudioSessionTests::studioShapesLabelsAndReplacementAreAtomic() {
                    QVERIFY(ComponentSchema::validPresentation({{"sizeAspectLocked", true}}));
                    QVERIFY(!ComponentSchema::validPresentation({{"sizeAspectLocked", QString("true")}}));
                    QVariantMap shape{
                        {"id", "decoration"},
                        {"kind", "widget"},
                        {"widgetType", "shape"},
                        {"componentType", "shape"},
                        {"name", "Decorative background"},
                        {"source", QVariantMap{{"schemaVersion", 1}, {"provider", "shell"}, {"kind", "shape"}}},
                        {"presentation",
                         QVariantMap{{"shape", "ellipse"},
                                     {"visibleLabel", "Caption"},
                                     {"fillMode", "gradient"},
                                     {"fillEndColor", "#123456"},
                                     {"fillDirection", "horizontal"}}}};
                    QVERIFY(!ComponentSchema::component(shape, false).isEmpty());
                    auto invalid = shape;
                    invalid["actions"] = QVariantList{QVariantMap{{"type", "settings"}, {"section", 1}}};
                    QVERIFY(ComponentSchema::component(invalid, false).isEmpty());
                    invalid = shape;
                    invalid["customLogo"] = "artwork/picture.png";
                    QVERIFY(ComponentSchema::component(invalid, false).isEmpty());
                    QVERIFY(!ComponentSchema::validPresentation({{"bodyText", QString(4097, 'x')}}));
                    QVERIFY(!ComponentSchema::validPresentation({{"visibleLabel", QString(161, 'x')}}));
                    QVERIFY(!ComponentSchema::validPresentation({{"shape", "script"}}));
                    QVERIFY(!ComponentSchema::validPresentation({{"fillDirection", "diagonal"}}));
                    QTemporaryDir dir;
                    AppRegistry registry;
                    QVERIFY(registry.loadFromData(apps));
                    HomeLayout home(&registry);
                    const auto path = dir.filePath("home.json");
                    QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
                    auto* session = qobject_cast<StudioSession*>(home.beginStudio());
                    QVERIFY(session);
                    auto document = QJsonDocument::fromJson(session->draft()).object();
                    document["components"] = QJsonArray{};
                    document["categories"] = QJsonArray{};
                    document["composition"] =
                        QJsonObject{{"schemaVersion", 1},
                                    {"enabled", true},
                                    {"grid", QJsonObject{{"columns", 8}, {"rows", 5}, {"mode", "screen"}}}};
                    QVERIFY(session->pushEdit(QJsonDocument(document).toJson()));
    QVERIFY(session->execute(
        "addPlaced",
        {{"type", "button"},
         {"placement", QVariantMap{{"x", 2}, {"y", 1}, {"width", 3}, {"height", 2}}},
         {"configuration", QVariantMap{{"name", "Library shortcut"},
                                        {"actions", QVariantList{QVariantMap{{"type", "settings"}, {"section", 1}}}}}},
         {"presentation", QVariantMap{{"visibleLabel", "My library"}}}}));
            const auto before = session->draft();
            const auto count = session->changeCount();
            const auto original = session->document()["components"].toList().first().toMap();
            const auto id = original["id"].toString();
            const auto image = session->replacementProposal(id, "image");
            QVERIFY2(image["valid"].toBool(), qPrintable(image["reason"].toString()));
            QCOMPARE(session->draft(), before);
            QVERIFY(session->previewCommand("replaceComponent", {{"id", id}, {"type", "image"}}));
            QCOMPARE(session->draft(), before);
            QCOMPARE(session->changeCount(), count);
            session->cancelPreview();
            QCOMPARE(session->draft(), before);
            QVERIFY(session->execute("replaceComponent", {{"id", id}, {"type", "image"}}));
            const auto replaced = session->document()["components"].toList().first().toMap();
            QCOMPARE(replaced["id"], original["id"]);
            QCOMPARE(replaced["placement"], original["placement"]);
            QCOMPARE(replaced["actions"], original["actions"]);
            QCOMPARE(replaced["presentation"].toMap()["visibleLabel"].toString(), QString("My library"));
            QCOMPARE(session->changeCount(), count + 1);
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), before);
            const auto text = session->replacementProposal(id, "text");
            QVERIFY(text["valid"].toBool());
            QVERIFY(text["losses"].toStringList().contains("Action link"));
            QVERIFY(!session->replacementProposal(id, "executable")["valid"].toBool());
            QCOMPARE(session->draft(), before);
            QVERIFY(!QFile::exists(path));
}

void StudioSessionTests::studioButtonCollectionsAreBoundedAndAtomic() {
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout home(&registry);
            const auto path = dir.filePath("home.json");
            QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
            auto* session = qobject_cast<StudioSession*>(home.beginStudio());
            QVERIFY(session);
            QVariantList buttons;
            for (int i = 0; i < 128; ++i)
                buttons.append(QVariantMap{
                    {"name", QString("Input %1").arg(i + 1)},
                    {"actions", QVariantList{QVariantMap{{"type", "switch-input"}, {"input", i % 8 + 1}}}}});
            const auto before = session->draft();
            const auto prototype = session->componentPrototype("carousel", {{"buttons", buttons}});
            QVERIFY2(!prototype.isEmpty(), qPrintable(session->errorString()));
            QCOMPARE(prototype["components"].toList().size(), 129);
            QCOMPARE(session->draft(), before);
            const auto root = prototype["root"].toMap();
            QCOMPARE(root["presentation"].toMap()["layout"].toMap()["columns"].toInt(), 128);
            auto excess = buttons;
            excess.append(buttons.first());
            QVERIFY(session->componentPrototype("carousel", {{"buttons", excess}}).isEmpty());
            QCOMPARE(session->draft(), before);
            const auto row = QVariantMap{{"schemaVersion", 1},
                                         {"mode", "row"},
                                         {"columns", 128},
                                         {"rows", 1},
                                         {"gap", 14},
                                         {"padding", 12},
                                         {"visibleColumns", 4}};
            QVERIFY(ComponentSchema::validChildLayout(row));
            auto invalid = row;
            invalid["columns"] = 129;
            QVERIFY(!ComponentSchema::validChildLayout(invalid));
            invalid = row;
            invalid["mode"] = "grid";
            QVERIFY(!ComponentSchema::validChildLayout(invalid));
            auto document = QJsonDocument::fromJson(before).object();
            document["components"] = QJsonArray::fromVariantList(prototype["components"].toList());
            document["categories"] = QJsonArray{};
            document["composition"] =
                QJsonObject{{"schemaVersion", 1},
                            {"enabled", true},
                            {"grid", QJsonObject{{"columns", 8}, {"rows", 5}, {"mode", "screen"}}}};
            QVERIFY2(session->pushEdit(QJsonDocument(document).toJson()), qPrintable(session->errorString()));
            QVERIFY(!QFile::exists(path));
            const auto withCollection = session->draft();
            const auto id = root["id"].toString();
            QVERIFY(session->execute("remove", {{"id", id}}));
            QVERIFY(session->document()["components"].toList().isEmpty());
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), withCollection);
            QVERIFY(!QFile::exists(path));
            auto appRoot = session->componentPrototype("app-bar")["root"].toMap();
            appRoot["appCollection"] = QVariantMap{{"mode", "selected"}, {"ids", QVariantList{"previously-installed"}}};
            document["components"] = QJsonArray{QJsonObject::fromVariantMap(appRoot)};
            QVERIFY(session->pushEdit(QJsonDocument(document).toJson()));
            const auto appId = appRoot["id"].toString();
            const auto missingBaseline = session->draft();
            QVERIFY(session->execute(
                "setApps", {{"id", appId}, {"mode", "selected"}, {"apps", QVariantList{"previously-installed"}}}));
            QVERIFY(!session->execute(
                "setApps", {{"id", appId}, {"mode", "selected"}, {"apps", QVariantList{"new-missing-app"}}}));
            QCOMPARE(session->draft(), missingBaseline);
            QVERIFY(session->execute("setApps", {{"id", appId}, {"mode", "selected"}, {"apps", QVariantList{}}}));
            QVERIFY(session->undo());
            QCOMPARE(session->draft(), missingBaseline);
            QVERIFY(!QFile::exists(path));
}

void StudioSessionTests::studioCollectionInstallDoesNotAuthorPlacements() {
            QCoreApplication::setApplicationVersion("0.0.1.2");
            for (const auto& mode : QStringList{"all", "selected"})
            {
                QTemporaryDir dir;
                AppRegistry registry;
                QVERIFY(registry.loadFromData(apps));
                AppCatalog catalog(&registry);
                QVERIFY(catalog.load(
                    QFINDTESTDATA("../config/catalog.json"), dir.filePath("installed.json"), dir.filePath("packages")));
                HomeLayout home(&registry);
                const auto path = dir.filePath("home.json");
                QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
                auto* session = qobject_cast<StudioSession*>(home.beginStudio());
                QVERIFY(session);
                auto root = session->componentPrototype("app-bar")["root"].toMap();
                root["appCollection"] = QVariantMap{{"mode", mode}, {"ids", QVariantList{}}};
                auto document = QJsonDocument::fromJson(home.committedDocument()).object();
                document["components"] = QJsonArray{QJsonObject::fromVariantMap(root)};
                document["categories"] = QJsonArray{};
                document["composition"] =
                    QJsonObject{{"schemaVersion", 1},
                                {"enabled", true},
                                {"grid", QJsonObject{{"columns", 8}, {"rows", 5}, {"mode", "screen"}}}};
                QVERIFY2(home.applyStudioDraft(QJsonDocument(document).toJson()), qPrintable(home.errorString()));
                QFile file(path);
                QVERIFY(file.open(QIODevice::ReadOnly));
                const auto saved = file.readAll();
                file.close();
                const auto canonical = home.committedDocument();
                QSignalSpy changes(&home, &HomeLayout::layoutChanged);
                connect(&catalog, &AppCatalog::packageInstalled, &home, &HomeLayout::addAppTile);
                QVERIFY2(catalog.install("freetube"), qPrintable(catalog.errorString()));
                QVERIFY(registry.find("freetube"));
                QCOMPARE(home.committedDocument(), canonical);
                QCOMPARE(changes.count(), 0);
                QVERIFY(catalog.uninstall("freetube"));
                QVERIFY(!registry.find("freetube"));
                QVERIFY(catalog.install("freetube"));
                QVERIFY(registry.find("freetube"));
                QCOMPARE(home.committedDocument(), canonical);
                QCOMPARE(changes.count(), 0);
                QVERIFY(file.open(QIODevice::ReadOnly));
                QCOMPARE(file.readAll(), saved);
            }
}

void StudioSessionTests::studioIconCleanupRetainsOriginalAndCancelsPrivately() {
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout home(&registry);
            const auto saved = dir.filePath("profile/home.json");
            QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), saved));
            auto* session = qobject_cast<StudioSession*>(home.beginStudio());
            QVERIFY(session);
            QImage sourceImage(64, 64, QImage::Format_ARGB32);
            sourceImage.fill(Qt::white);
            for (int y = 16; y < 48; ++y)
                for (int x = 16; x < 48; ++x)
                    sourceImage.setPixelColor(x, y, Qt::red);
            const auto source = dir.filePath("original.png");
            QVERIFY(sourceImage.save(source));
            const auto baseline = session->draft();
            const auto original = session->stageArtwork(source);
            QVERIFY(session->cleanIconArtwork(source, 3).isEmpty());
            QVERIFY(session->cleanIconArtwork(source, 1, "#80ffffff").isEmpty());
            const auto cleaned = session->cleanIconArtwork(source);
            QVERIFY2(cleaned["accepted"].toBool(), qPrintable(session->errorString()));
            QCOMPARE(session->draft(), baseline);
            QVERIFY(!QFile::exists(saved));
            QVERIFY(!QFile::exists(dir.filePath("profile/" + original)));
            const auto relative = session->stageArtwork(cleaned["source"].toString());
            QVERIFY(!relative.isEmpty());
            QVERIFY(relative != original);
            auto blank = QJsonDocument::fromJson(session->draft()).object();
            blank["components"] = QJsonArray{};
            blank["categories"] = QJsonArray{};
            blank["composition"] =
                QJsonObject{{"schemaVersion", 1},
                            {"enabled", true},
                            {"grid", QJsonObject::fromVariantMap(home.studioOptions()["defaultGrid"].toMap())}};
            QVERIFY(session->pushEdit(QJsonDocument(blank).toJson()));
            QVERIFY(session->execute(
                "addResolved",
                {{"type", "image"}, {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 2}, {"height", 2}}}}));
            QVERIFY(session->execute("patch", {{"id", "image"}, {"value", QVariantMap{{"customLogo", relative}}}}));
            QVERIFY2(session->apply("image"), qPrintable(session->errorString()));
            QVERIFY(QFile::exists(dir.filePath("profile/" + relative)));
            QVERIFY(QFile::exists(dir.filePath("profile/" + original)));
            QCOMPARE(QImage(dir.filePath("profile/" + original)), sourceImage);
            QCOMPARE(QImage(source), sourceImage);
            const auto count = session->artworkChoices().size();
            QVERIFY(count >= 2);
            session->invalidate();
            QVERIFY(session->cleanIconArtwork(source).isEmpty());
}

void StudioSessionTests::studioGifArtworkIsBoundedStagedAndProfileScoped() {
            const auto gif = [](int count, int delay = 10)
            {
                auto bytes = QByteArray::fromHex("47494638396101000100800000000000ffffff");
                for (int i = 0; i < count; ++i)
                {
                    bytes += QByteArray::fromHex("21f90400");
                    bytes += char(delay & 255);
                    bytes += char((delay >> 8) & 255);
                    bytes += QByteArray::fromHex("00002c0000000001000100000202440100");
                }
                bytes += char(0x3b);
                return bytes;
            };
            QString error;
            const auto valid = gif(2);
            const auto decoded = ManagedGif::decode(valid, true, &error);
            QVERIFY2(!decoded.isEmpty(), qPrintable(error));
            QCOMPARE(decoded["frameCount"].toInt(), 2);
            QCOMPARE(decoded["frames"].toList().size(), 2);
            QVERIFY(decoded["frames"].toList().first().toString().startsWith("data:image/png;base64,"));
            QVERIFY(ManagedGif::decode(gif(61), false, &error).isEmpty());
            QVERIFY(!error.isEmpty());
            QVERIFY(ManagedGif::decode(gif(2, 101), false, &error).isEmpty());
            QVERIFY(ManagedGif::decode(gif(31, 100), false, &error).isEmpty());
            auto huge = valid;
            huge[6] = char(0);
            huge[7] = char(8);
            QVERIFY(ManagedGif::decode(huge, false, &error).isEmpty());
            QVERIFY(ManagedGif::decode(QByteArray("GIF89a"), false, &error).isEmpty());
            QVERIFY(ManagedGif::decode(valid + QByteArray(2 * 1024 * 1024, 'x'), false, &error).isEmpty());
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout home(&registry);
            const auto path = dir.filePath("profile/home.json");
            QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
            auto* session = qobject_cast<StudioSession*>(home.beginStudio());
            QVERIFY(session);
            const auto baseline = session->draft();
            const auto source = dir.filePath("source.gif");
            QFile file(source);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(valid), qint64(valid.size()));
            file.close();
            const auto relative = session->stageArtwork(QUrl::fromLocalFile(source).toString());
            QVERIFY2(relative.endsWith(".gif"), qPrintable(session->errorString()));
            QCOMPARE(session->draft(), baseline);
            QVERIFY(!QFile::exists(path));
            auto* draft = qobject_cast<HomeLayout*>(session->model());
            QVERIFY(draft);
            for (int i = 0; i < 8; ++i)
                QVERIFY(draft->artworkAnimation(relative, QString("owner-%1").arg(i)).contains("frames"));
            QVERIFY(draft->artworkAnimation(relative, "ninth-owner").contains("error"));
            draft->releaseArtworkAnimation("owner-0");
            QVERIFY(draft->artworkAnimation(relative, "ninth-owner").contains("frames"));
            QVERIFY(draft->artworkAnimation("../source.gif", "unsafe").contains("error"));
            QVERIFY(home.artworkAnimation(relative, "other-model").contains("error"));
            QVERIFY(draft->resolveArtwork(relative).startsWith("data:image/png;base64,"));
            session->invalidate();
            QVERIFY(draft->resolveArtwork(relative).isEmpty());
            QVERIFY(!QFile::exists(path));
            QVERIFY(ComponentSchema::validPresentation({{"animateArtwork", true}}));
            QVERIFY(!ComponentSchema::validPresentation({{"animateArtwork", "true"}}));
            HomeLayout live(&registry);
            const auto saved = dir.filePath("saved/home.json");
            QVERIFY(live.load(QFINDTESTDATA("../config/default-home-layout.json"), saved));
            auto* editing = qobject_cast<StudioSession*>(live.beginStudio());
            QVERIFY(editing);
            auto blank = QJsonDocument::fromJson(editing->draft()).object();
            blank["components"] = QJsonArray{};
            blank["categories"] = QJsonArray{};
            blank["composition"] =
                QJsonObject{{"schemaVersion", 1},
                            {"enabled", true},
                            {"grid", QJsonObject::fromVariantMap(live.studioOptions()["defaultGrid"].toMap())}};
            QVERIFY(editing->pushEdit(QJsonDocument(blank).toJson()));
            QVERIFY(editing->execute(
                "addResolved",
                {{"type", "image"}, {"placement", QVariantMap{{"x", 0}, {"y", 0}, {"width", 3}, {"height", 2}}}}));
            const auto imported = editing->stageArtwork(source);
            QVERIFY(imported.endsWith(".gif"));
            QVariantMap rootPageAction{{"type", "root-page"}};
            QVariantList rootPageActions{rootPageAction};
            QVariantMap valueMap{{"customLogo", imported},
                                 {"actions", rootPageActions},
                                 {"presentation",
                                  QVariantMap{{"animateArtwork", true},
                                              {"visibleLabel", "Animation caption"},
                                              {"labelPosition", "below"}}}};
            QVERIFY(editing->execute("patch", {{"id", "image"}, {"value", valueMap}}));
            const auto beforeApply = editing->draft();
            QVERIFY(QDir().mkpath(dir.filePath("saved")));
            QFile blocked(dir.filePath("saved/artwork"));
            QVERIFY(blocked.open(QIODevice::WriteOnly));
            blocked.write("blocked");
            blocked.close();
            QVERIFY(!editing->apply("image"));
            QCOMPARE(editing->draft(), beforeApply);
            QVERIFY(!QFile::exists(saved));
            QVERIFY(blocked.remove());
            QVERIFY2(editing->apply("image"), qPrintable(editing->errorString()));
            QCOMPARE(readFile(dir.filePath("saved/" + imported)), valid);
            const auto skin = live.saveSkin("Portable animation");
            QVERIFY2(!skin.isEmpty(), qPrintable(live.errorString()));
            editing->invalidate();
            HomeLayout reloaded(&registry);
            QVERIFY(reloaded.load(QFINDTESTDATA("../config/default-home-layout.json"), saved));
            QVERIFY(reloaded.resolveArtwork(imported).startsWith("data:image/png;base64,"));
            QVERIFY(reloaded.resetToDefaults());
            QVERIFY(QFile::remove(dir.filePath("saved/" + imported)));
            QVERIFY2(reloaded.applySkin(skin), qPrintable(reloaded.errorString()));
            const auto restored = reloaded.items().first().toMap();
            QVERIFY(restored["customLogo"].toString().endsWith(".gif"));
            QVERIFY(reloaded.resolveArtwork(restored["customLogo"].toString()).startsWith("data:image/png;base64,"));
            QVERIFY(reloaded.artworkAnimation(restored["customLogo"].toString(), "restored").contains("frames"));
}

void StudioSessionTests::studioPastelPresetUsesEditableButtons() {
            QFile packaged(QFINDTESTDATA("../config/skins/pastel.json")),
                fixture(QFINDTESTDATA("visual/home-pastel-reference/recreation-config.json"));
            QVERIFY(packaged.open(QIODevice::ReadOnly));
            QVERIFY(fixture.open(QIODevice::ReadOnly));
            const auto layout = QJsonDocument::fromJson(packaged.readAll()).object()["layout"].toObject();
            QCOMPARE(layout, QJsonDocument::fromJson(fixture.readAll()).object());
            int buttons = 0, appCollections = 0, histories = 0;
            for (const auto& value : layout["components"].toArray())
            {
                const auto item = value.toObject();
                QVERIFY(item["componentType"] != "launcher");
                if (item["componentType"] == "button")
                {
                    ++buttons;
                    QCOMPARE(item["kind"].toString(), QString("widget"));
                    QCOMPARE(item["widgetType"].toString(), QString("button"));
                    const auto normalized = ComponentSchema::component(item.toVariantMap(), false);
                    QVERIFY(!normalized.isEmpty());
                    QCOMPARE(normalized["source"].toMap()["kind"].toString(), QString("button"));
                    QVERIFY(!normalized["actions"].toList().first().toMap().isEmpty());
                }
                if (item.contains("appCollection"))
                    ++appCollections;
                if (item["family"] == "recently-viewed")
                {
                    ++histories;
                    QCOMPARE(item["source"].toObject()["libraryKind"].toString(), QString("history"));
                }
            }
            QCOMPARE(buttons, 10);
            QCOMPARE(appCollections, 1);
            QCOMPARE(histories, 1);
            const auto categories = layout["categories"].toArray();
            QJsonObject historyAction;
            for (const auto& value : categories)
                if (value.toObject()["id"] == "recent")
                {
                    const auto style = value.toObject()["presentation"].toObject();
                    QCOMPARE(style["trailingLabel"].toString(), QString("See All"));
                    historyAction = style["trailingAction"].toObject();
                }
            QCOMPARE(historyAction["type"].toString(), QString("open-page"));
            bool foundHistoryPage = false;
            for (const auto& value : layout["pages"].toArray())
                if (value.toObject()["id"] == historyAction["pageId"])
                {
                    const auto items = value.toObject()["layout"].toObject()["components"].toArray();
                    QCOMPARE(items.size(), 1);
                    const auto item = items.first().toObject();
                    QCOMPARE(item["source"], layout["components"].toArray().last().toObject()["source"]);
                    QCOMPARE(item["presentation"].toObject()["libraryLayout"].toString(), QString("grid"));
                    QCOMPARE(item["presentation"].toObject()["libraryLimit"].toInt(), 50);
                    foundHistoryPage = true;
                }
            QVERIFY(foundHistoryPage);
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout home(&registry);
            const auto path = dir.filePath("home.json"), defaults = dir.filePath("pastel-defaults.json");
            const auto data = QJsonDocument(layout).toJson();
            {
                QFile output(defaults);
                QVERIFY(output.open(QIODevice::WriteOnly));
                QCOMPARE(output.write(data), data.size());
            }
            QVERIFY2(home.load(defaults, path), qPrintable(home.errorString()));
            QVERIFY(!QFile::exists(path));
}

void StudioSessionTests::studioStartingStylesAreValidatedDraftData() {
            QTemporaryDir dir;
            AppRegistry registry;
            QVERIFY(registry.loadFromData(apps));
            HomeLayout home(&registry);
            const auto path = dir.filePath("home.json");
            QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), path));
            auto* session = qobject_cast<StudioSession*>(home.beginStudio());
            QVERIFY(session);
            const auto before = session->draft();
            const auto variants = home.studioOptions()["componentVariants"].toMap();
            QCOMPARE(variants.size(), 3);
            for (auto family = variants.cbegin(); family != variants.cend(); ++family)
                for (const auto& value : family.value().toList())
                {
                    const auto variant = value.toMap();
                    const auto prototype = session->componentPrototype(family.key(), {{"variant", variant["id"]}});
                    QVERIFY2(!prototype.isEmpty(), qPrintable(session->errorString()));
                    const auto root = prototype["root"].toMap();
                    QCOMPARE(root["componentType"].toString(), family.key());
                    QVERIFY(!root.contains("variant"));
                    const auto span = variant["span"].toMap();
                    QCOMPARE(root["placement"].toMap()["width"], span["width"]);
                    QCOMPARE(root["placement"].toMap()["height"], span["height"]);
                    const auto presentation = variant["presentation"].toMap();
                    for (auto it = presentation.cbegin(); it != presentation.cend(); ++it)
                        QCOMPARE(root["presentation"].toMap()[it.key()], it.value());
                    QCOMPARE(session->draft(), before);
                    QCOMPARE(session->changeCount(), 0);
                }
            QVERIFY(session->componentPrototype("button", {{"variant", "unknown"}}).isEmpty());
            QVERIFY(session->componentPrototype("button", {{"variant", 42}}).isEmpty());
            QVERIFY(session->componentPrototype("app-bar", {{"variant", "round"}}).isEmpty());
            QCOMPARE(session->draft(), before);
            QVERIFY(!QFile::exists(path));
            const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
            const auto localDefaults = dir.filePath("default-home-layout.json");
            QVERIFY(QFile::copy(defaults, localDefaults));
            auto options = QJsonDocument::fromJson(readFile(QFINDTESTDATA("../config/studio-defaults.json"))).object();
            auto familyOptions = options["componentVariants"].toObject();
            auto buttonOptions = familyOptions["button"].toArray();
            auto invalid = buttonOptions.first().toObject();
            invalid["span"] = QJsonObject{{"width", 17}, {"height", 1}};
            buttonOptions[0] = invalid;
            familyOptions["button"] = buttonOptions;
            options["componentVariants"] = familyOptions;
            const auto localOptions = dir.filePath("studio-defaults.json");
            writeFile(localOptions, QJsonDocument(options).toJson(QJsonDocument::Compact));
            HomeLayout refused(&registry);
            QVERIFY(refused.load(localDefaults, dir.filePath("refused.json")));
            QVERIFY(refused.studioOptions().isEmpty());
            const auto invalidBytes = QJsonDocument(options).toJson(QJsonDocument::Compact);
            invalid["span"] = QJsonObject{{"width", 16}, {"height", 1}};
            buttonOptions[0] = invalid;
            familyOptions["button"] = buttonOptions;
            options["componentVariants"] = familyOptions;
            const auto validBytes = QJsonDocument(options).toJson(QJsonDocument::Compact);
            QCOMPARE(validBytes.size(), invalidBytes.size());
            writeFile(localOptions, validBytes);
            QVERIFY(!refused.studioOptions().isEmpty());
            writeFile(localOptions, invalidBytes);
            QVERIFY(refused.studioOptions().isEmpty());
}
