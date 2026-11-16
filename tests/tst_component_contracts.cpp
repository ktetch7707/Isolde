#include "../src/home/ActionDispatcher.h"
#include "../src/home/ComponentSchema.h"
#include "../src/home/HomeDesignData.h"
#include "../src/home/HomeLayout.h"
#include "../src/home/PageModel.h"
#include "../src/media/CaptureController.h"
#include "../src/media/KodiMedia.h"
#include "../src/media/KodiService.h"
#include "../src/sessions/SessionManager.h"
#include "tst_component_common.h"

class ComponentContractTests : public QObject {
    Q_OBJECT
  private slots:
    void validatesContracts();
    void guideActionsValidateIdentityAndAppearance();
    void captureAndAudioEditorActionsValidateAndDispatch();
    void densePlacementValidationPreservesBoundsAndTouchingEdges();
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    ComponentContractTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_component_contracts.moc"

void ComponentContractTests::validatesContracts() {
    for (const auto& value :
         QVariantList{QVariantMap{{"type", "shell"}, {"target", "touch /tmp/no"}},
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
    auto tile = ComponentSchema::component(
        {{"id", "movie-tile"},
         {"kind", "app"},
         {"appId", "movies"},
         {"actions", QVariantList{QVariantMap{{"type", "launch-app"}, {"appId", "movies"}}}}},
        true);
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
    const auto showTile = ComponentSchema::component(
        {{"id", "show-card"}, {"kind", "action"}, {"name", "Fixture show"}, {"actions", QVariantList{show}}}, true);
    QCOMPARE(showTile["source"].toMap()["kind"].toString(), QString("collection"));
    const auto inputAction = ComponentSchema::action({{"type", "switch-input"}, {"input", 4}});
    QCOMPARE(inputAction["input"].toInt(), 4);
    const auto inputTile = ComponentSchema::component(
        {{"id", "hdmi-four"}, {"kind", "action"}, {"name", "Console"}, {"actions", QVariantList{inputAction}}}, true);
    QCOMPARE(inputTile["source"].toMap()["provider"].toString(), QString("isolde"));
    QCOMPARE(inputTile["source"].toMap()["id"].toString(), QString("4"));
}

void ComponentContractTests::guideActionsValidateIdentityAndAppearance() {
    QString error;
    QVERIFY(!ComponentSchema::action({{"schemaVersion", 1}, {"type", "open-guide"}}, &error).isEmpty());
    QCOMPARE(error, QString());
    QVERIFY(!ComponentSchema::action({{"schemaVersion", 1}, {"type", "tune-channel"}, {"channelId", "local-station"}},
                                     &error)
                 .isEmpty());
    QCOMPARE(error, QString());
    QVERIFY(ComponentSchema::action({{"schemaVersion", 1}, {"type", "tune-channel"}, {"channelId", "../other"}}, &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(
        ComponentSchema::action(
            {{"schemaVersion", 1}, {"type", "tune-channel"}, {"channelId", "station"}, {"offsetSeconds", 9}}, &error)
            .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(ComponentSchema::action(
                {{"schemaVersion", 1}, {"type", "open-guide"}, {"target", "https://example.invalid"}}, &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(!ComponentSchema::validPresentation({{"guideRows", 2}}));
    QVERIFY(!ComponentSchema::validPresentation({{"guideMinutes", "120"}}));
    QVERIFY(ComponentSchema::validPresentation({{"guideRows", 5},
                                                {"guideMinutes", 120},
                                                {"guideChannelWidth", 220},
                                                {"guideLabels", true},
                                                {"guideSourceBadges", false}}));
    QVERIFY(!ComponentSchema::action({{"schemaVersion", 1},
                                      {"type", "open-guide"},
                                      {"focusChannelId", "local-station"},
                                      {"presentation", QVariantMap{{"guideSurface", "overlay"}}}},
                                     &error)
                 .isEmpty());
    for (const auto& id : QVariantList{true, 123, QString("../other")}) {
        QVERIFY(ComponentSchema::action({{"schemaVersion", 1}, {"type", "open-guide"}, {"focusChannelId", id}}, &error)
                    .isEmpty());
        QVERIFY(ComponentSchema::action({{"schemaVersion", 1}, {"type", "tune-channel"}, {"channelId", id}}, &error)
                    .isEmpty());
    }
    QVERIFY(
        ComponentSchema::action(
            {{"schemaVersion", 1}, {"type", "tune-channel"}, {"channelId", "station"}, {"focusChannelId", "station"}},
            &error)
            .isEmpty());
    for (const auto& position : QStringList{"top-left",
                                            "top-center",
                                            "top-right",
                                            "center-left",
                                            "center",
                                            "center-right",
                                            "bottom-left",
                                            "bottom-center",
                                            "bottom-right"})
        QVERIFY(ComponentSchema::validPresentation({{"guideSurface", "overlay"},
                                                    {"guideOverlayPosition", position},
                                                    {"guideOverlayWidth", 65},
                                                    {"guideOverlayHeight", 100},
                                                    {"guideOverlayRows", 3}}));
    for (const auto& invalid : QVariantList{QString("video-overlay"), 123, true})
        QVERIFY(!ComponentSchema::validPresentation({{"guideSurface", invalid}}));
    for (const auto& invalid : QVariantList{64, 101, 65.5, true, QString("95")})
        QVERIFY(!ComponentSchema::validPresentation({{"guideOverlayWidth", invalid}}));
    QVERIFY(!ComponentSchema::validPresentation({{"guideOverlayRows", 6}}));
    QVERIFY(!ComponentSchema::validPresentation({{"guideOverlayPosition", "offscreen"}}));
}

void ComponentContractTests::captureAndAudioEditorActionsValidateAndDispatch() {
    QVariantMap labels;
    for (int input = 1; input <= 8; ++input)
        labels.insert(QString::number(input), QString("HDMI %1").arg(input));
    const QVariantMap configuration{{"audioDeviceId", ""},
                                    {"baudRate", 9600},
                                    {"captureDeviceId", ""},
                                    {"commandTemplate", ""},
                                    {"inputLabels", labels},
                                    {"selectedInput", 1},
                                    {"serialPortName", ""},
                                    {"sessionReturn", "origin"},
                                    {"sessionTransition", "fade"},
                                    {"transitionDuration", 250},
                                    {"videoFormatKey", ""}};
    QString error;
    const auto apply =
        ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", configuration}}, &error);
    QVERIFY2(!apply.isEmpty(), qPrintable(error));
    QCOMPARE(apply.value("configuration").toMap().size(), 11);
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-preview-start"}}).contains("type"));
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-preview-draft"},
                                           {"captureDeviceId", "video"},
                                           {"videoFormatKey", ""},
                                           {"audioDeviceId", "audio"}})
                .contains("captureDeviceId"));
    QVERIFY(
        ComponentSchema::editorAction(
            {{"type", "capture-preview-draft"}, {"captureDeviceId", 42}, {"videoFormatKey", ""}, {"audioDeviceId", ""}})
            .isEmpty());
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-switch-input"}, {"input", 8}}).contains("input"));
    QVERIFY(ComponentSchema::editorAction({{"type", "audio-output-select"}, {"outputId", "alsa_output.usb-01"}})
                .contains("outputId"));
    QVERIFY(ComponentSchema::action({{"type", "capture-apply"}, {"configuration", configuration}}).isEmpty());

    auto invalid = configuration;
    invalid.insert("extra", "ignored?");
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", invalid}}, &error).isEmpty());
    QVERIFY(!error.isEmpty());
    invalid = configuration;
    invalid.insert("selectedInput", 1.5);
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", invalid}}).isEmpty());
    invalid = configuration;
    invalid.insert("sessionTransition", "spin");
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", invalid}}).isEmpty());
    invalid = configuration;
    invalid.insert("transitionDuration", 225);
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", invalid}}).isEmpty());
    invalid = configuration;
    invalid.insert("sessionReturn", "elsewhere");
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", invalid}}).isEmpty());
    invalid = configuration;
    labels.insert("9", "Out of range");
    invalid.insert("inputLabels", labels);
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-apply"}, {"configuration", invalid}}).isEmpty());
    QVERIFY(ComponentSchema::editorAction({{"type", "audio-output-select"}, {"outputId", "../speaker"}}).isEmpty());
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-preview-stop"}, {"ignored", true}}).isEmpty());
    const QVariantMap validArtwork{{"1", "artwork/studio-a.png"}, {"8", "artwork/logo-b.gif"}};
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-artwork-apply"}, {"artwork", validArtwork}})
                .contains("artwork"));
    QVERIFY(ComponentSchema::validComposition({{"schemaVersion", 1}, {"captureInputArtwork", validArtwork}}));
    QVERIFY(ComponentSchema::editorAction(
                {{"type", "capture-artwork-apply"}, {"artwork", QVariantMap{{"9", "artwork/a.png"}}}})
                .isEmpty());
    QVERIFY(
        ComponentSchema::editorAction({{"type", "capture-artwork-apply"}, {"artwork", QVariantMap{{"1", "../a.png"}}}})
            .isEmpty());
    QVERIFY(ComponentSchema::editorAction({{"type", "capture-artwork-apply"}, {"artwork", QVariantMap{{"1", 42}}}})
                .isEmpty());
    QVERIFY(ComponentSchema::action({{"type", "capture-artwork-apply"}, {"artwork", validArtwork}}).isEmpty());
    const QJsonObject artworkDocument{
        {"composition", QJsonObject{{"captureInputArtwork", QJsonObject{{"1", "artwork/a.png"}}}}}};
    const auto rewritten = HomeDesignData::artwork(artworkDocument, [](const QString&) {
                               return QString("artwork/imported.png");
                           }).toObject();
    QCOMPARE(rewritten.value("composition").toObject().value("captureInputArtwork").toObject().value("1").toString(),
             QString("artwork/imported.png"));

    AppRegistry registry;
    SessionManager sessions(&registry);
    KodiMedia media("editor-actions-test");
    KodiService service("editor-actions-test");
    CaptureController capture;
    ActionDispatcher dispatcher(&registry, &sessions, &media, &service, nullptr, &capture);
    QSignalSpy rejected(&dispatcher, &ActionDispatcher::rejected);
    QVERIFY(dispatcher.dispatchEditorAction({{"type", "capture-preview-stop"}}));
    QVERIFY(!dispatcher.dispatchEditorAction({{"type", "capture-apply"}, {"configuration", invalid}}));
    QCOMPARE(rejected.size(), 1);
    QVERIFY(!dispatcher.dispatchEditorAction({{"type", "audio-output-select"}, {"outputId", "alsa_output.usb-01"}}));
    QCOMPARE(rejected.size(), 2);
    const auto savedCapture = capture.captureDeviceId();
    const auto savedAudio = capture.audioDeviceId();
    QVERIFY(!dispatcher.dispatchEditorAction(
        {{"type", "capture-preview-draft"}, {"captureDeviceId", ""}, {"videoFormatKey", ""}, {"audioDeviceId", ""}}));
    QCOMPARE(capture.captureDeviceId(), savedCapture);
    QCOMPARE(capture.audioDeviceId(), savedAudio);
    QCOMPARE(rejected.size(), 3);
}

void ComponentContractTests::densePlacementValidationPreservesBoundsAndTouchingEdges() {
    QVariantMap grid{{"columns", 16}, {"rows", 10}, {"overlap", false}};
    QJsonArray items;
    for (int i = 0; i < 160; ++i)
        items.append(
            QJsonObject{{"placement", QJsonObject{{"x", i % 16}, {"y", i / 16}, {"width", 1}, {"height", 1}}}});
    QVERIFY(ComponentSchema::placementsFit(items, grid)); // All edges touch without overlap.
    items.append(QJsonObject{{"name", "Unplaced component"}});
    QVERIFY(ComponentSchema::placementsFit(items, grid));
    items.append(items.at(159)); // Last pair still detects a collision.
    QVERIFY(!ComponentSchema::placementsFit(items, grid));
    grid["overlap"] = true;
    QVERIFY(ComponentSchema::placementsFit(items, grid));
    items.append(QJsonObject{{"placement", QJsonObject{{"x", 16}, {"y", 0}, {"width", 1}, {"height", 1}}}});
    QVERIFY(!ComponentSchema::placementsFit(items, grid)); // Overlap never permits out-of-bounds data.
    for (const double coordinate : {-1e30, 1e30, 2147483647.0}) {
        QVERIFY(!ComponentSchema::validPlacement({{"x", coordinate}, {"y", 0}, {"width", 1}, {"height", 1}}, 16, 10));
        QVERIFY(!ComponentSchema::validPlacement({{"x", 0}, {"y", coordinate}, {"width", 1}, {"height", 1}}, 16, 10));
        QVERIFY(!ComponentSchema::validPlacement({{"x", 0}, {"y", 0}, {"width", coordinate}, {"height", 1}}, 16, 10));
    }
}