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

class ComponentContractTests : public QObject
{
    Q_OBJECT
  private slots:
    void
    validatesContracts();
    void
    guideActionsValidateIdentityAndAppearance();
    void
    mediaActionsValidateIdentityAndResume();
    void
    slotValidationPermitsAndRejectsComponentKinds();
    void
    captureAndAudioEditorActionsValidateAndDispatch();
    void
    densePlacementValidationPreservesBoundsAndTouchingEdges();
    void
    versionedFieldRoleSlotActionDefinitionsValidate();
};

int
main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    ComponentContractTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_component_contracts.moc"

void
ComponentContractTests::validatesContracts()
{
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
                      QVariantMap{{"type", "settings"}, {"section", 100}}})
    {
        QString error;
        QVERIFY(ComponentSchema::action(value.toMap(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    auto tile = ComponentSchema::component(
        {{"id", "movie-tile"},
         {"kind", "app"},
         {"componentType", "launcher"},
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
        {{"id", "show-card"}, {"kind", "action"}, {"componentType", "launcher"},
         {"name", "Fixture show"}, {"actions", QVariantList{show}}}, true);
    QCOMPARE(showTile["source"].toMap()["kind"].toString(), QString("collection"));
    const auto inputAction = ComponentSchema::action({{"type", "switch-input"}, {"input", 4}});
    QCOMPARE(inputAction["input"].toInt(), 4);
    const auto inputTile = ComponentSchema::component(
        {{"id", "hdmi-four"}, {"kind", "action"}, {"componentType", "launcher"},
         {"name", "Console"}, {"actions", QVariantList{inputAction}}}, true);
    QCOMPARE(inputTile["source"].toMap()["provider"].toString(), QString("isolde"));
    QCOMPARE(inputTile["source"].toMap()["id"].toString(), QString("4"));
}

void
ComponentContractTests::mediaActionsValidateIdentityAndResume()
{
    QString error;
    const auto play = ComponentSchema::action(
        {{"schemaVersion", 1}, {"type", "media.play"}, {"mediaKind", "movie"}, {"mediaId", 12}}, &error);
    QVERIFY(!play.isEmpty());
    QCOMPARE(play["type"].toString(), QString("media.play"));
    QCOMPARE(play["mediaKind"].toString(), QString("movie"));
    QCOMPARE(play["mediaId"].toInt(), 12);
    QCOMPARE(play["resume"].toBool(), false);
    QCOMPARE(error, QString());

    const auto resume = ComponentSchema::action(
        {{"schemaVersion", 1}, {"type", "media.resume"}, {"mediaKind", "episode"}, {"mediaId", 5}}, &error);
    QVERIFY(!resume.isEmpty());
    QCOMPARE(resume["type"].toString(), QString("media.resume"));
    QCOMPARE(resume["mediaKind"].toString(), QString("episode"));
    QCOMPARE(resume["mediaId"].toInt(), 5);
    QCOMPARE(resume["resume"].toBool(), true);

    const auto markWatched = ComponentSchema::action(
        {{"schemaVersion", 1}, {"type", "media.markWatched"}, {"mediaKind", "movie"}, {"mediaId", 12},
         {"watched", true}}, &error);
    QVERIFY(!markWatched.isEmpty());
    QCOMPARE(markWatched["type"].toString(), QString("media.markWatched"));
    QCOMPARE(markWatched["mediaKind"].toString(), QString("movie"));
    QCOMPARE(markWatched["mediaId"].toInt(), 12);
    QCOMPARE(markWatched["watched"].toBool(), true);

    const auto markUnwatched = ComponentSchema::action(
        {{"schemaVersion", 1}, {"type", "media.markWatched"}, {"mediaKind", "episode"}, {"mediaId", 7}},
        &error);
    QVERIFY(!markUnwatched.isEmpty());
    QCOMPARE(markUnwatched["watched"].toBool(), true);

    for (const auto& value :
         QVariantList{QVariantMap{{"type", "media.play"}, {"mediaKind", "book"}, {"mediaId", 12}},
                      QVariantMap{{"type", "media.resume"}, {"mediaKind", "movie"}, {"mediaId", 1.5}},
                      QVariantMap{{"type", "media.play"}, {"mediaKind", "movie"}, {"mediaId", 0}},
                      QVariantMap{{"type", "media.resume"}, {"mediaKind", "song"}, {"mediaId", -3}},
                      QVariantMap{{"type", "media.markWatched"}, {"mediaKind", "song"}, {"mediaId", 1}},
                      QVariantMap{{"type", "media.markWatched"}, {"mediaKind", "movie"}, {"mediaId", "not-an-id"}}})
    {
        QVERIFY(ComponentSchema::action(value.toMap(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
}

void
ComponentContractTests::slotValidationPermitsAndRejectsComponentKinds()
{
    QString error;

    // Empty permitted list accepts any component kind.
    QVERIFY(ComponentSchema::validateSlotComponent({}, "button", &error));
    QCOMPARE(error, QString());

    // Non-empty permitted list accepts matching kinds.
    QVERIFY(ComponentSchema::validateSlotComponent(
                QVariantList{QVariant("button"), QVariant("image")}, "image", &error));
    QCOMPARE(error, QString());

    // Non-empty permitted list rejects non-matching kinds.
    QVERIFY(!ComponentSchema::validateSlotComponent(
                QVariantList{QVariant("button"), QVariant("image")}, "container", &error));
    QVERIFY(!error.isEmpty());

    // contentSlot validation accepts a valid permitted list.
    QVariantMap slotWithPermitted{
        {"schemaVersion", 1}, {"id", "slot-1"}, {"label", "Media actions"}, {"kind", "media"},
        {"permitted", QVariantList{QVariant("button"), QVariant("image")}}};
    QVERIFY(!ComponentSchema::component(
                QVariantMap{{"schemaVersion", 1}, {"id", "comp-1"}, {"componentType", "button"},
                            {"kind", "app"}, {"size", "standard"}, {"contentSlot", slotWithPermitted}},
                false, &error)
                .isEmpty());
    QCOMPARE(error, QString());

    // contentSlot validation rejects a permitted kind that is too long.
    QVariantMap slotWithLongKind{
        {"schemaVersion", 1}, {"id", "slot-2"}, {"label", "Media actions"}, {"kind", "media"},
        {"permitted", QVariantList{QVariant("this-kind-is-way-too-long-for-a-component-name")}}};
    QVERIFY(ComponentSchema::component(
                QVariantMap{{"schemaVersion", 1}, {"id", "comp-2"}, {"componentType", "button"},
                            {"kind", "app"}, {"size", "standard"}, {"contentSlot", slotWithLongKind}},
                false, &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());

    // contentSlot with permitted size 5 is accepted (was previously rejected as size != 4).
    QVariantMap slotSize5{
        {"schemaVersion", 1}, {"id", "slot-3"}, {"label", "Buttons"}, {"kind", "buttons"},
        {"permitted", QVariantList{QVariant("button")}}};
    QVERIFY(!ComponentSchema::component(
                QVariantMap{{"schemaVersion", 1}, {"id", "comp-3"}, {"componentType", "button"},
                            {"kind", "app"}, {"size", "standard"}, {"contentSlot", slotSize5}},
                false, &error)
                .isEmpty());
    QCOMPARE(error, QString());
}

void
ComponentContractTests::guideActionsValidateIdentityAndAppearance()
{
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
    for (const auto& id : QVariantList{true, 123, QString("../other")})
    {
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

void
ComponentContractTests::captureAndAudioEditorActionsValidateAndDispatch()
{
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
    const auto rewritten = HomeDesignData::artwork(artworkDocument,
                                                   [](const QString&)
                                                   {
                                                       return QString("artwork/imported.png");
                                                   })
                               .toObject();
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

void
ComponentContractTests::densePlacementValidationPreservesBoundsAndTouchingEdges()
{
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
    for (const double coordinate : {-1e30, 1e30, 2147483647.0})
    {
        QVERIFY(!ComponentSchema::validPlacement({{"x", coordinate}, {"y", 0}, {"width", 1}, {"height", 1}}, 16, 10));
        QVERIFY(!ComponentSchema::validPlacement({{"x", 0}, {"y", coordinate}, {"width", 1}, {"height", 1}}, 16, 10));
        QVERIFY(!ComponentSchema::validPlacement({{"x", 0}, {"y", 0}, {"width", coordinate}, {"height", 1}}, 16, 10));
    }
}

void
ComponentContractTests::versionedFieldRoleSlotActionDefinitionsValidate()
{
    QString error;

    // Inspector fields: valid field passes.
    QVERIFY(ComponentSchema::validateInspectorField(
                QVariantMap{{"schemaVersion", 1}, {"tab", "Layout"}, {"scope", "presentation"},
                            {"key", "testKey"}, {"label", "Test Label"}, {"type", "number"}, {"min", 1}, {"max", 10}},
                &error));
    QCOMPARE(error, QString());

    // Inspector field missing schemaVersion fails.
    QVERIFY(!ComponentSchema::validateInspectorField(
                QVariantMap{{"tab", "Layout"}, {"scope", "presentation"}, {"key", "testKey"},
                            {"label", "Test Label"}, {"type", "number"}},
                &error));
    QVERIFY(!error.isEmpty());

    // Inspector field with unknown key fails.
    QVERIFY(!ComponentSchema::validateInspectorField(
                QVariantMap{{"schemaVersion", 1}, {"tab", "Layout"}, {"scope", "presentation"},
                            {"key", "testKey"}, {"label", "Test Label"}, {"type", "number"}, {"bogus", true}},
                &error));
    QVERIFY(!error.isEmpty());

    // Inspector fields list: valid passes, duplicate key fails.
    QVERIFY(ComponentSchema::validateInspectorFields(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"key", "field1"}, {"label", "One"}, {"type", "text"}},
                             QVariantMap{{"schemaVersion", 1}, {"key", "field2"}, {"label", "Two"}, {"type", "text"}}},
                &error));
    QCOMPARE(error, QString());
    QVERIFY(!ComponentSchema::validateInspectorFields(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"key", "field1"}, {"label", "One"}, {"type", "text"}},
                             QVariantMap{{"schemaVersion", 1}, {"key", "field1"}, {"label", "One again"}, {"type", "text"}}},
                &error));
    QVERIFY(!error.isEmpty());

    // Theme role: valid passes.
    QVERIFY(ComponentSchema::validateRoleDefinition(
                QVariantMap{{"schemaVersion", 1}, {"key", "surfaceColor"}, {"label", "Surface"}}, &error));
    QCOMPARE(error, QString());

    // Theme role missing schemaVersion fails.
    QVERIFY(!ComponentSchema::validateRoleDefinition(
                QVariantMap{{"key", "surfaceColor"}, {"label", "Surface"}}, &error));
    QVERIFY(!error.isEmpty());

    // Theme roles list: valid passes, duplicate key fails.
    QVERIFY(ComponentSchema::validateRoleDefinitions(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"key", "role1"}, {"label", "Role One"}},
                             QVariantMap{{"schemaVersion", 1}, {"key", "role2"}, {"label", "Role Two"}}},
                &error));
    QCOMPARE(error, QString());
    QVERIFY(!ComponentSchema::validateRoleDefinitions(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"key", "role1"}, {"label", "Role One"}},
                             QVariantMap{{"schemaVersion", 1}, {"key", "role1"}, {"label", "Role One again"}}},
                &error));
    QVERIFY(!error.isEmpty());

    // Action definition: valid passes.
    QVERIFY(ComponentSchema::validateActionDefinition(
                QVariantMap{{"schemaVersion", 1}, {"name", "Play Media"}, {"action", "media.play"},
                            {"aliases", QVariantList{QVariant("play"), QVariant("start")}}},
                &error));
    QCOMPARE(error, QString());

    // Action definition missing schemaVersion fails.
    QVERIFY(!ComponentSchema::validateActionDefinition(
                QVariantMap{{"name", "Play Media"}, {"action", "media.play"}}, &error));
    QVERIFY(!error.isEmpty());

    // Action definitions list: valid passes, duplicate id fails.
    QVERIFY(ComponentSchema::validateActionDefinitions(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"name", "Play"}, {"action", "media.play"}},
                             QVariantMap{{"schemaVersion", 1}, {"name", "Resume"}, {"action", "media.resume"}}},
                &error));
    QCOMPARE(error, QString());
    QVERIFY(!ComponentSchema::validateActionDefinitions(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"name", "Play"}, {"action", "media.play"}},
                             QVariantMap{{"schemaVersion", 1}, {"name", "Play again"}, {"action", "media.play"}}},
                &error));
    QVERIFY(!error.isEmpty());

    // Action category: valid passes.
    QVERIFY(ComponentSchema::validateActionCategoryDefinition(
                QVariantMap{{"schemaVersion", 1}, {"name", "Media"}, {"types", QVariantList{QVariant("media.play"), QVariant("media.resume")}}},
                &error));
    QCOMPARE(error, QString());

    // Action category missing types fails.
    QVERIFY(!ComponentSchema::validateActionCategoryDefinition(
                QVariantMap{{"schemaVersion", 1}, {"name", "Media"}}, &error));
    QVERIFY(!error.isEmpty());

    // Action categories list: valid passes.
    QVERIFY(ComponentSchema::validateActionCategoryDefinitions(
                QVariantList{QVariantMap{{"schemaVersion", 1}, {"name", "Media"},
                                        {"types", QVariantList{QVariant("media.play")}}}},
                &error));
    QCOMPARE(error, QString());

    // Slot with schemaVersion and permitted passes.
    QVERIFY(ComponentSchema::validateSlotComponent(
                QVariantList{QVariant("button"), QVariant("image")}, "button", &error));
    QCOMPARE(error, QString());

    // Slot with empty permitted accepts any kind.
    QVERIFY(ComponentSchema::validateSlotComponent(QVariantList(), "anything", &error));
    QCOMPARE(error, QString());
}

