#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "home/PageModel.h"
#include "home/StudioSession.h"

#include <QFile>
#include <QColor>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QPointer>
#include <memory>
#include <QtTest>

namespace {
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray fixture()
{
    return R"({"schemaVersion":2,"theme":{"surfaceColor":"#202126","selectedColor":"#353039"},
      "components":[],"categories":[],"homePageId":"home-page","pages":[
      {"id":"home-page","name":"Pastel Home","layout":{"schemaVersion":2,"composition":{"schemaVersion":1,"enabled":false},"components":[],"categories":[],
        "theme":{"surfaceColor":"#f5edf3","selectedColor":"#e9d8e3","textColor":"#241d2b","mutedColor":"#514b67","focusColor":"#6d3b69"}}},
      {"id":"films","name":"Films","layout":{"schemaVersion":2,"composition":{"schemaVersion":1,"enabled":false},"components":[],"categories":[],
        "theme":{"surfaceColor":"#102030","selectedColor":"#263850","textColor":"#ffffff","mutedColor":"#cccccc","focusColor":"#ffffff"}}}]})";
}

QVariantMap policy(const QString &surface, const QString &mode, const QVariantMap &tokens = {})
{
    return {{surface, QVariantMap{{"schemaVersion", 1}, {"mode", mode}, {"tokens", tokens}}}};
}

QVariantMap patch(const QVariantMap &policies)
{
    return {{"scope", "theme"}, {"value", QVariantMap{{"surfaceThemes", policies}}}};
}
}

class SurfaceAppearanceTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void policiesRejectUnsafeAndUnboundedData();
    void paletteHasReadableSemanticRolesWithoutMutatingInput();
    void resolverUsesHomeForSettingsAndRetainsOriginWithoutWrites();
    void overridesPreviewCancelUndoAndPersistAtomically();
    void reloadAndRejectedOverridesDoNotLeakContext();
    void installedThemeFixturesContainRenderablePages();
};

void SurfaceAppearanceTests::installedThemeFixturesContainRenderablePages()
{
    const auto data = read(QFINDTESTDATA("fixtures/shell-theme-routes.json"));
    const auto documents = QJsonDocument::fromJson(data).array();
    QCOMPARE(documents.size(), 3);
    for (const auto &entry : documents) {
        QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
        const auto path = dir.filePath("theme.json");
        QVERIFY(write(path, QJsonDocument(entry.toObject()).toJson()));
        QVERIFY2(home.load(path, dir.filePath("absent.json")), qPrintable(home.errorString()));
        QCOMPARE(home.items().size(), 1);
        auto *designated = qobject_cast<PageModel *>(home.pageModel("theme-home"));
        auto *origin = qobject_cast<PageModel *>(home.pageModel("theme-origin"));
        QVERIFY(designated); QVERIFY(origin);
        QCOMPARE(designated->items().size(), 1);
        QCOMPARE(origin->items().size(), 4);
        QCOMPARE(origin->placements().size(), 4);
    }
}

void SurfaceAppearanceTests::policiesRejectUnsafeAndUnboundedData()
{
    QVERIFY(ComponentSchema::validSurfaceThemes(policy("search", "origin")));
    QVERIFY(ComponentSchema::validSurfaceThemes(policy("settings", "home")));
    QVERIFY(!ComponentSchema::validSurfaceThemes(policy("settings", "origin")));
    QVERIFY(!ComponentSchema::validSurfaceThemes(policy("native-kodi", "custom")));
    QVERIFY(!ComponentSchema::validSurfaceThemes(policy("search", "code")));
    for (const auto &tokens : QList<QVariantMap>{{{"script", "run"}}, {{"textColor", "#00ffffff"}},
                                               {{"cornerRadius", 49}}, {{"typographyScale", "1"}},
                                               {{"scrimOpacity", -0.1}}, {{"motionEnabled", 1}},
                                               {{"highContrast", "true"}}, {{"spacing", 100}}})
        QVERIFY(!ComponentSchema::validSurfaceThemes(policy("search", "custom", tokens)));
    for (const auto &version : QVariantList{true, "1", 2, 1.5}) {
        auto item = policy("search", "home");
        auto value = item["search"].toMap(); value["schemaVersion"] = version; item["search"] = value;
        QVERIFY(!ComponentSchema::validSurfaceThemes(item));
    }
    QVERIFY(!ComponentSchema::validSurfaceThemes(policy("search", "home", {{"textColor", "#ffffff"}})));
    const QVariantMap dark{{"surfaceColor", "#000000"}};
    auto bad = dark; bad["surfaceThemes"] = policy("search", "custom", {{"textColor", "#000000"}});
    QVERIFY(!ComponentSchema::readableSurfaceThemes(dark, bad));
}

void SurfaceAppearanceTests::paletteHasReadableSemanticRolesWithoutMutatingInput()
{
    const QVariantMap pastel{{"surfaceColor", "#ffffff"}, {"selectedColor", "#000000"},
                            {"textColor", "#ffffff"}, {"focusColor", "#ffffff"}, {"highContrast", true}};
    const auto before = pastel;
    const auto resolved = ComponentSchema::surfacePalette(pastel);
    QCOMPARE(pastel, before);
    QCOMPARE(resolved["textColor"].toString(), QString("#000000"));
    QCOMPARE(resolved["focusColor"].toString(), QString("#000000"));
    QCOMPARE(resolved["selectedTextColor"].toString(), QString("#ffffff"));
    QCOMPARE(resolved["selectedFocusColor"].toString(), QString("#ffffff"));
    QCOMPARE(resolved["inputTextColor"].toString(), QString("#ffffff"));
    for (const auto &role : QStringList{"elevatedColor", "outlineColor", "accentColor", "scrimColor",
                                       "successColor", "warningColor", "errorColor", "unavailableColor"})
        QVERIFY2(QColor(resolved[role].toString()).isValid(), qPrintable(role));
    const auto blended = ComponentSchema::surfacePalette({{"surfaceColor", "#ffffff"}, {"backgroundEnd", "#000000"}, {"panelOpacity", 0.0}});
    QCOMPARE(blended["surfaceColor"].toString(), QString("#000000"));
    QCOMPARE(blended["textColor"].toString(), QString("#ffffff"));
    AppRegistry registry; HomeLayout resolver(&registry);
    QCOMPARE(resolver.resolvedSurfacePalette(blended), blended);
    const QVariantMap translucent{{"surfaceColor", "#ffffff"}, {"backgroundEnd", "#000000"}, {"panelOpacity", 0.5}};
    const auto raw = translucent;
    const auto once = resolver.resolvedSurfacePalette(translucent);
    QCOMPARE(resolver.resolvedSurfacePalette(once), once); QCOMPARE(translucent, raw);

    const auto split = ComponentSchema::surfacePalette({{"surfaceColor", "#000000"},
        {"selectedColor", "#ffffff"}, {"elevatedColor", "#ffffff"}, {"backgroundEnd", "#ffffff"},
        {"inputColor", "#ffffff"}, {"textColor", "#ffffff"}, {"mutedColor", "#ffffff"},
        {"focusColor", "#ffffff"}, {"errorColor", "#ffffff"}, {"highContrast", true}});
    QCOMPARE(split["focusColor"].toString(), QString("#ffffff"));
    for (const auto &role : QStringList{"selectedFocusColor", "elevatedFocusColor", "backgroundFocusColor",
                                       "inputFocusColor", "inputTextColor", "inputMutedColor", "elevatedErrorColor", "backgroundErrorColor"})
        QCOMPARE(split[role].toString(), QString("#000000"));

}

void SurfaceAppearanceTests::resolverUsesHomeForSettingsAndRetainsOriginWithoutWrites()
{
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
    const auto defaults = dir.filePath("defaults.json"), user = dir.filePath("user.json");
    QVERIFY(write(defaults, fixture())); QVERIFY(write(user, fixture()));
    QVERIFY2(home.load(defaults, user), qPrintable(home.errorString()));
    const auto saved = read(user);
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(home.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#f5edf3"));
        QCOMPARE(home.surfaceAppearance("settings", "films")["themeSource"].toString(), QString("Pastel Home"));
        QCOMPARE(home.surfaceAppearance("search", "films")["surfaceColor"].toString(), QString("#102030"));
        QCOMPARE(home.surfaceAppearance("kodi", "films")["surfaceColor"].toString(), QString("#102030"));
        QCOMPARE(home.surfaceAppearance("apps", "removed-page")["surfaceColor"].toString(), QString("#f5edf3"));
        QCOMPARE(home.surfaceAppearance("search", "")["surfaceColor"].toString(), QString("#202126"));
    }
    QVERIFY(home.surfaceAppearance("native-kodi", "films").isEmpty());
    QCOMPARE(read(user), saved);
    StudioSession preview; QVERIFY(preview.bind(&home));
    QVERIFY(preview.previewCommand("pagePatch", patch(policy("apps", "home")), "films"));
    auto *proposal = qobject_cast<HomeLayout *>(preview.proposalModel()); QVERIFY(proposal);
    QCOMPARE(proposal->surfaceAppearance("apps", "films")["surfaceColor"].toString(), QString("#f5edf3"));
    QCOMPARE(home.surfaceAppearance("apps", "films")["surfaceColor"].toString(), QString("#102030"));
    preview.cancelPreview();
    QVERIFY(preview.previewCommand("pagePatch", patch(policy("apps", "custom", {{"surfaceColor", "#ffffff"}, {"textColor", "#000000"}})), "films"));
    proposal = qobject_cast<HomeLayout *>(preview.proposalModel()); QVERIFY(proposal);
    QCOMPARE(proposal->surfaceAppearance("apps", "films")["surfaceColor"].toString(), QString("#ffffff"));
    preview.cancelPreview(); QCOMPARE(read(user), saved);
    HomeLayout other(&registry);
    auto second = fixture(); second.replace("#f5edf3", "#fafafa");
    const auto otherUser = dir.filePath("other.json"); QVERIFY(write(otherUser, second));
    QVERIFY(other.load(defaults, otherUser));
    QCOMPARE(other.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#fafafa"));
    QCOMPARE(home.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#f5edf3"));
}

void SurfaceAppearanceTests::overridesPreviewCancelUndoAndPersistAtomically()
{
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
    const auto defaults = dir.filePath("defaults.json"), user = dir.filePath("user.json");
    QVERIFY(write(defaults, fixture())); QVERIFY(write(user, fixture())); QVERIFY(home.load(defaults, user));
    StudioSession session; QVERIFY(session.bind(&home));
    const auto saved = read(user), initial = session.draft();
    const auto custom = patch(policy("settings", "custom", {{"surfaceColor", "#ffffff"}, {"textColor", "#000000"}}));
    QVERIFY2(session.previewCommand("pagePatch", custom, "home-page"), qPrintable(session.errorString()));
    auto *proposal = qobject_cast<HomeLayout *>(session.proposalModel()); QVERIFY(proposal);
    QCOMPARE(proposal->surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#ffffff"));
    QCOMPARE(home.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#f5edf3"));
    QCOMPARE(session.draft(), initial); QCOMPARE(read(user), saved);
    session.cancelPreview(); QCOMPARE(session.draft(), initial);
    QVERIFY(session.execute("pagePatch", custom, "home-page"));
    QVERIFY(session.undo()); QCOMPARE(session.draft(), initial); QVERIFY(session.redo());
    const auto accepted = session.draft();
    QVERIFY(!session.execute("pagePatch", patch(policy("settings", "origin")), "home-page"));
    QCOMPARE(session.draft(), accepted); QCOMPARE(read(user), saved);
    QVERIFY(session.apply());
    QCOMPARE(home.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#ffffff"));
    HomeLayout loaded(&registry); QVERIFY(loaded.load(defaults, user));
    QCOMPARE(loaded.surfaceAppearance("settings", "films"), home.surfaceAppearance("settings", "films"));
    // A write failure keeps the applied palette and disk bytes intact.
    StudioSession retry; QVERIFY(retry.bind(&home));
    QVERIFY(retry.execute("pagePatch", patch(policy("search", "home")), "films"));
    const auto disk = read(user);
    const auto applied = home.surfaceAppearance("search", "films");
    QVERIFY(QFile::rename(user, user + ".saved")); QVERIFY(QDir().mkdir(user));
    QVERIFY(!retry.apply()); QCOMPARE(home.surfaceAppearance("search", "films"), applied);
    QVERIFY(QDir().rmdir(user)); QVERIFY(QFile::rename(user + ".saved", user));
    QCOMPARE(read(user), disk); QVERIFY(retry.isDirty());
}

void SurfaceAppearanceTests::reloadAndRejectedOverridesDoNotLeakContext()
{
    QTemporaryDir dir; AppRegistry registry; HomeLayout home(&registry);
    const auto defaults = dir.filePath("defaults.json"), first = dir.filePath("first.json"), second = dir.filePath("second.json");
    QVERIFY(write(defaults, fixture())); QVERIFY(write(first, fixture()));
    auto other = fixture(); other.replace("#f5edf3", "#ddeecc"); other.replace("#102030", "#443322");
    QVERIFY(write(second, other)); QVERIFY(home.load(defaults, first));
    QPointer<QObject> previous(home.pageModel("films")); QVERIFY(previous);
    QCOMPARE(home.surfaceAppearance("search", "films")["surfaceColor"].toString(), QString("#102030"));
    QVERIFY(home.load(defaults, second));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(previous); // Same-owner reload preserves live page bindings.
    QCOMPARE(previous->property("appearance").toMap()["surfaceColor"].toString(), QString("#443322"));
    auto outgoing = std::make_unique<HomeLayout>(&registry); QVERIFY(outgoing->load(defaults, first));
    QPointer<QObject> outgoingPage(outgoing->pageModel("films")); QVERIFY(outgoingPage);
    outgoing.reset(); QVERIFY(outgoingPage.isNull());
    QCOMPARE(home.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#ddeecc"));
    QCOMPARE(home.surfaceAppearance("search", "films")["surfaceColor"].toString(), QString("#443322"));
    QCOMPARE(read(first), fixture()); QCOMPARE(read(second), other);
    // Invalid persisted surface policies use packaged defaults and preserve the
    // rejected bytes. The previously loaded profile palette must not survive.
    auto invalid = QJsonDocument::fromJson(other).object();
    auto theme = invalid["theme"].toObject();
    theme["surfaceThemes"] = QJsonObject{{"search", QJsonObject{{"schemaVersion", 2}, {"mode", "custom"}}}};
    invalid["theme"] = theme;
    const auto rejected = QJsonDocument(invalid).toJson(); QVERIFY(write(second, rejected));
    QVERIFY(home.load(defaults, second)); QCOMPARE(read(second), rejected);
    QCOMPARE(home.surfaceAppearance("settings", "films")["surfaceColor"].toString(), QString("#f5edf3"));
    QCOMPARE(home.surfaceAppearance("search", "removed-page")["surfaceColor"].toString(), QString("#f5edf3"));
}

QTEST_GUILESS_MAIN(SurfaceAppearanceTests)
#include "tst_surface_appearance.moc"
