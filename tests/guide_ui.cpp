#include "media/GuideController.h"
#include "media/GuideStore.h"
#include "home/ComponentSchema.h"
#include "input/InputManager.h"
#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QTemporaryDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QJSValue>
#include <QTimeZone>
#include <QQmlExpression>
#include <QDir>
#include <QQuickItem>

class GuideDispatchProbe final:public QObject {
    Q_OBJECT
public:
    QVariantMap last;
    Q_INVOKABLE bool dispatch(const QVariantMap &action,const QString &){last=ComponentSchema::action(action);return !last.isEmpty();}
};
class GuideUiTests final:public QObject {
    Q_OBJECT
private slots:
    void asyncPreviewCreateCancelAndProfileTeardown();
    void remoteCreatePreviewSaveAndFutureSelection();
    void remoteNextUnwatchedPreviewAndSaveUseTheSameKodiSnapshot();
    void unavailableIdentityOnlyChangesMatchingCachedPrograms();
    void remoteClockAdjustmentRequiresReviewAndPreservesPrograms();
    void remoteClockDraftPolicySwitchShowsAndClearsOffset();
    void remoteAdvancedScheduleDraftCancelWritesNothing();
    void compactPvrProgressAndAccessiblePressUseTheSameActions();
    void remoteSelectedSlotRepairRequiresReviewAndHonorsCancelAndStaleRevision();
    void overlayAndFocusedEntryPreserveGuideStateWithoutSaving();
    void nestedRuleEditorDoesNotDiscardChildren();
    void remoteReorderPreviewCancelAndApply();
    void backgroundRefreshQueuesConfirmedEditsWithoutPrematureIdle();
    void configuredCommandPaletteGuideEntryNeedsNoLayoutRewrite();
    void rangeAndSetRulesUseRemoteEditableIndividualValues();
    void startupMaintainsExpiredSchedulesWithoutKodiReads();
    void pvrPreviewMappingImportAndFailedRefreshPreserveCache();
    void pvrUtcParsingAndCacheIgnoreCivilDstGaps();
    void remoteRuleEditDuplicateAndRegenerationAreReviewedTransactions();
};
namespace {
void serve(QLocalServer &server) {
    QObject::connect(&server,&QLocalServer::newConnection,&server,[&server]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        QObject::connect(socket,&QLocalSocket::readyRead,socket,[socket,&server]{
            auto input=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",input);if(!input.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(input).object();server.setProperty("requests",server.property("requests").toInt()+1);
            const auto params=request.value("params").toObject();const auto kind=params.value("kind").toString();const int start=params.value("start").toInt(),end=qMin(101,start+50);
            QJsonArray items;int total=101,actualEnd=end;
            if(server.property("watchFixture").isValid()){const auto fixture=server.property("watchFixture").toList();total=fixture.size();actualEnd=qMin(total,start+50);for(int i=start;i<actualEnd;++i){auto item=fixture[i].toMap();item["mediaType"]=kind;items.append(QJsonObject::fromVariantMap(item));}}
            else for(int i=start;i<end;++i)items.append(QJsonObject{{"mediaType",kind},{"mediaId",i+1},{"title",QString("Cartoon %1").arg(i+1)},{"duration",1800},{"source","kodi"},{"studio",QJsonArray{"Cartoon Network"}}});
            const QJsonObject result{{"items",items},{"limits",QJsonObject{{"start",start},{"end",actualEnd},{"total",total}}},{"skipped",0}};
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request.value("id")},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });
    }});
}
void command(QObject *object,int value) {QVERIFY(QMetaObject::invokeMethod(object,"handleCommand",Q_ARG(QVariant,value)));}
QQuickItem *visualItem(QQuickItem *root,const QString &name) {if(root->objectName()==name)return root;for(auto *child:root->childItems())if(auto *found=visualItem(child,name))return found;return nullptr;}
QVariantMap options() {QFile file(GUIDE_DEFAULTS);if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll()).object().value("guide").toObject().toVariantMap();}
QVariantMap appearance() {return {{"surfaceColor","#202126"},{"selectedColor","#353039"},{"textColor","#f5f1f3"},{"mutedColor","#aaa5af"},{"focusColor","#c994a5"}};}
}
void GuideUiTests::asyncPreviewCreateCancelAndProfileTeardown() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    const auto path=dir.filePath("a/guide.sqlite");
    {
        GuideController controller(path,"a",nullptr,dir.path());QTRY_VERIFY(controller.ready() && !controller.busy());
        auto definition=controller.newDefinition();definition["name"]="Cartoon Network";definition["rule"]=QVariantMap{{"field","studio"},{"op","contains"},{"value","Cartoon Network"}};
        QVERIFY(controller.previewLocal(definition,"episode"));QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.previewCount(),101);QCOMPARE(server.property("requests").toInt(),3);QVERIFY(controller.channels().isEmpty());
        auto changed=definition;changed["mediaKinds"]=QVariantList{QString("movie")};QVERIFY(!controller.createLocal(changed));QVERIFY(controller.channels().isEmpty());
        controller.cancel();QVERIFY(!controller.previewReady());QVERIFY(!controller.createLocal(definition));QVERIFY(controller.channels().isEmpty());
        definition["mediaKinds"]=QVariantList{QString("movie"),QString("episode")};
        QVERIFY(controller.previewLocal(definition,"mixed"));QTRY_VERIFY(!controller.busy());QCOMPARE(controller.previewCount(),202);
        QVERIFY(controller.createLocal(definition));QTRY_VERIFY(!controller.busy());QCOMPARE(controller.channels().size(),1);
        const auto before=server.property("requests").toInt();controller.refresh();QTRY_VERIFY(!controller.busy());QCOMPARE(server.property("requests").toInt(),before);
        QVERIFY(!controller.compactRows().isEmpty());QVERIFY(!controller.rows().isEmpty());
        QVERIFY(controller.previewLocal(definition,"mixed"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.previewReady());
        const auto savedId=definition.value("id").toString();const auto savedSlot=controller.rows().first().toMap().value("programs").toList().first().toMap().value("id").toString();
        QVERIFY(controller.previewRepair(savedId,savedSlot,1));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.repairReview().isEmpty());
        QVERIFY(controller.previewRegeneration(savedId));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.regenerationReview().isEmpty());
        controller.invalidate();QVERIFY(!controller.ready());QVERIFY(!controller.previewReady());QVERIFY(controller.previewDefinition().isEmpty());QVERIFY(controller.repairReview().isEmpty());QVERIFY(controller.regenerationReview().isEmpty());QVERIFY(!controller.previewLocal(definition,"episode"));
    }
    GuideStore reopened(path);QVERIFY(reopened.ready());QCOMPARE(reopened.channels().size(),1);
    GuideController other(dir.filePath("b/guide.sqlite"),"b",nullptr,dir.path());QTRY_VERIFY(other.ready() && !other.busy());QVERIFY(other.channels().isEmpty());
}
void GuideUiTests::remoteCreatePreviewSaveAndFutureSelection() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    GuideController controller(dir.filePath("guide.sqlite"),"ui",nullptr,dir.path());QTRY_VERIFY(controller.ready() && !controller.busy());
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));
    QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},
        {"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("guide-tile")),Q_ARG(QVariant,QVariantMap())));
    command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("add"));
    command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("create"));
    command(view.data(),InputManager::Back);QCOMPARE(view->property("mode").toString(),QString("discard"));
    command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("create"));QVERIFY(controller.channels().isEmpty());
    for(int i=0;i<6;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(controller.previewReady());QVERIFY(controller.channels().isEmpty());
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy() && controller.channels().size()==1);
    QTRY_COMPARE(view->property("mode").toString(),QString());QTRY_VERIFY(!controller.busy());
    command(view.data(),InputManager::Confirm);QTRY_COMPARE(dispatch.last.value("type").toString(),QString("tune-channel"));
    const auto selected=view->property("selectedChannelId");dispatch.last.clear();
    command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("details"));QVERIFY(dispatch.last.isEmpty());
    command(view.data(),InputManager::Back);QCOMPARE(view->property("selectedChannelId"),selected);
    view->setProperty("anchorUtc",QDateTime::currentSecsSinceEpoch()+29*86400);
    command(view.data(),InputManager::Confirm);QVERIFY(dispatch.last.isEmpty()); // future gaps never launch the currently airing item
    view->setProperty("channelQuery","no matching station");QVERIFY(QMetaObject::invokeMethod(view.data(),"applyFilter"));
    QVERIFY(view->property("headerFocused").toBool());
    view->setProperty("channelQuery",QString());view->setProperty("favoritesOnly",true);QVERIFY(QMetaObject::invokeMethod(view.data(),"applyFilter"));
    QVERIFY(view->property("headerFocused").toBool()); // empty filtered lineup retains recovery navigation
    view->setProperty("favoritesOnly",false);QVERIFY(QMetaObject::invokeMethod(view.data(),"applyFilter"));
    const auto beforeJump=view->property("anchorUtc");QVariant jumpResult;
    QVERIFY(QMetaObject::invokeMethod(view.data(),"jumpTo",Q_RETURN_ARG(QVariant,jumpResult),Q_ARG(QVariant,QDateTime::currentSecsSinceEpoch()+31*86400)));
    QVERIFY(!jumpResult.toBool());QCOMPARE(view->property("anchorUtc"),beforeJump);QVERIFY(!view->property("navigationError").toString().isEmpty());
    command(view.data(),InputManager::NavigateUp);QVERIFY(view->property("headerFocused").toBool());
}
void GuideUiTests::remoteNextUnwatchedPreviewAndSaveUseTheSameKodiSnapshot() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));
    QVariantList fixture;const QVariantList states{0,1,QVariant(),false,QString("0")};
    for(int i=0;i<states.size();++i){QVariantMap item{{"source","kodi"},{"mediaId",i+1},{"title",QString("Episode %1").arg(i+1)},{"duration",1800},{"studio",QStringList{"Cartoon Network"}},{"showtitle","Series"},{"season",1},{"episode",i+1}};if(states[i].isValid())item["playcount"]=states[i];fixture.append(item);}
    server.setProperty("watchFixture",fixture);serve(server);
    const auto path=dir.filePath("guide.sqlite");GuideController controller(path,"unwatched",nullptr,dir.path());QTRY_VERIFY(controller.ready()&&!controller.busy());
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("guide-tile")),Q_ARG(QVariant,QVariantMap())));
    command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);command(view.data(),InputManager::Confirm);
    for(int i=0;i<4;++i)command(view.data(),InputManager::NavigateDown);
    for(int i=0;i<3;++i)command(view.data(),InputManager::Confirm);
    QCOMPARE(view->property("draft").toMap().value("sequence").toString(),QString("next-unwatched"));
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);
    QTRY_VERIFY(!controller.busy());QVERIFY2(controller.previewReady(),qPrintable(controller.errorString()));QCOMPARE(controller.previewCount(),1);QCOMPARE(controller.previewItems().first().toMap().value("mediaId").toInt(),1);QVERIFY(controller.channels().isEmpty());
    const auto definition=controller.previewDefinition();const auto id=definition.value("id").toString();
    // Kodi changes after preview cannot silently change the reviewed save.
    auto changed=fixture;auto item=changed[0].toMap();item["playcount"]=1;changed[0]=item;server.setProperty("watchFixture",changed);
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy()&&controller.channels().size()==1);
    GuideStore reader(path);const auto wall=QDateTime::currentSecsSinceEpoch();const auto programs=reader.programs(id,wall,wall+86400,1000);QVERIFY(!programs.isEmpty());for(const auto &program:programs)QCOMPARE(program.toMap().value("mediaId").toInt(),1);
    QVERIFY(controller.previewLocal(definition,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.previewReady());QCOMPARE(controller.previewCount(),0);QCOMPARE(reader.programs(id,wall,wall+86400,1000),programs);
}
void GuideUiTests::unavailableIdentityOnlyChangesMatchingCachedPrograms() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    const auto path=dir.filePath("guide.sqlite");GuideController controller(path,"missing",nullptr,dir.path());QTRY_VERIFY(controller.ready()&&!controller.busy());
    auto episode=controller.newDefinition();episode["sequence"]="episode-order";QVERIFY(controller.previewLocal(episode,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(episode));QTRY_VERIFY(!controller.busy());
    auto movie=controller.newDefinition();movie["mediaKinds"]=QVariantList{QString("movie")};movie["sequence"]="episode-order";QVERIFY(controller.previewLocal(movie,"movie"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(movie));QTRY_VERIFY(!controller.busy());
    GuideStore reader(path);const auto episodeId=episode.value("id").toString(),movieId=movie.value("id").toString();const auto wall=QDateTime::currentSecsSinceEpoch();
    const auto before=reader.programs(episodeId,wall,wall+7*86400,1000),other=reader.programs(movieId,wall,wall+7*86400,1000);const auto otherCandidates=reader.candidateSnapshot(movieId);QVERIFY(!before.isEmpty());
    const auto missingId=before.first().toMap().value("mediaId").toInt();QVERIFY(controller.markUnavailable("episode",missingId));QTRY_VERIFY(!controller.busy());
    const auto after=reader.programs(episodeId,wall,wall+7*86400,1000);QCOMPARE(after.size(),before.size());
    for(int i=0;i<before.size();++i){auto expected=before[i].toMap();if(expected.value("mediaId").toInt()==missingId)expected["status"]="unavailable";QCOMPARE(after[i].toMap(),expected);}
    QCOMPARE(reader.programs(movieId,wall,wall+7*86400,1000),other);QCOMPARE(reader.candidateSnapshot(movieId),otherCandidates);
    for(const auto &item:reader.candidateSnapshot(episodeId))QVERIFY(item.toMap().value("mediaId").toInt()!=missingId);
    QVERIFY(reader.tuneTarget(episodeId,wall).isEmpty());
    QVariantMap cached;for(const auto &row:controller.compactRows())if(row.toMap().value("id")==episodeId)cached=row.toMap().value("current").toMap();
    QCOMPARE(cached.value("id"),before.first().toMap().value("id"));QCOMPARE(cached.value("title"),before.first().toMap().value("title"));QCOMPARE(cached.value("status").toString(),QString("unavailable"));
    QCOMPARE(cached.value("startUtc"),before.first().toMap().value("startUtc"));QCOMPARE(cached.value("endUtc"),before.first().toMap().value("endUtc"));QVERIFY(cached.value("clockUtc").toLongLong()>=cached.value("startUtc").toLongLong());
    for(const auto &row:controller.rows())if(row.toMap().value("id")==episodeId)QCOMPARE(row.toMap().value("current").toMap().value("id"),cached.value("id"));
    controller.invalidate();QVERIFY(!controller.markUnavailable("episode",missingId));QCOMPARE(reader.programs(movieId,wall,wall+7*86400,1000),other);
}
void GuideUiTests::remoteClockDraftPolicySwitchShowsAndClearsOffset() {
    QTemporaryDir dir;GuideController controller(dir.filePath("guide.sqlite"),"clock-draft");QTRY_VERIFY(controller.ready()&&!controller.busy());
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("clock-origin")),Q_ARG(QVariant,QVariantMap())));command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);command(view.data(),InputManager::Confirm);
    auto draft=controller.newDefinition();draft["clockOffsetSeconds"]=3600;view->setProperty("draft",draft);
    QQmlExpression label(QQmlEngine::contextForObject(view.data()),view.data(),"createFields[5]");QVERIFY(label.evaluate().toString().contains("Offset 60 minutes"));
    for(int i=0;i<5;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);
    const auto changed=view->property("draft").toMap();QCOMPARE(changed.value("clock").toString(),QString("pause-when-off"));QVERIFY(!changed.contains("clockOffsetSeconds"));QVERIFY(!ComponentSchema::guideChannel(changed).isEmpty());QVERIFY(controller.channels().isEmpty());
}
void GuideUiTests::remoteClockAdjustmentRequiresReviewAndPreservesPrograms() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    const auto path=dir.filePath("guide.sqlite");GuideController controller(path,"clock",nullptr,dir.path());QTRY_VERIFY(controller.ready()&&!controller.busy());
    const auto def=controller.newDefinition();QVERIFY(controller.previewLocal(def,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(def));QTRY_VERIFY(!controller.busy());
    GuideStore reader(path);const auto id=def.value("id").toString(),origin=QString("guide-clock-origin");const auto wall=QDateTime::currentSecsSinceEpoch();const auto programs=reader.programs(id,wall,wall+7*86400,1000),candidates=reader.candidateSnapshot(id),channels=reader.channels();const auto requests=server.property("requests").toInt();
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,origin),Q_ARG(QVariant,QVariantMap())));QTRY_VERIFY(!controller.busy());command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Menu);
    for(int i=0;i<13;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("clock"));
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("clockDeltaSeconds").toInt(),900);
    for(int i=0;i<3;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(!controller.clockReview().isEmpty());QCOMPARE(reader.channels(),channels);
    QCOMPARE(view->property("mode").toString(),QString("clockReview"));QCOMPARE(view->property("menuIndex").toInt(),0);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("clock"));QVERIFY(controller.clockReview().isEmpty());QCOMPARE(reader.channels(),channels);
    command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(!controller.clockReview().isEmpty());command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QTRY_COMPARE(view->property("mode").toString(),QString("context"));
    QCOMPARE(reader.channel(id).value("clockOffsetSeconds").toInt(),900);QCOMPARE(reader.programs(id,wall,wall+7*86400,1000),programs);QCOMPARE(reader.candidateSnapshot(id),candidates);QCOMPARE(server.property("requests").toInt(),requests);QCOMPARE(view->property("selectedChannelId").toString(),id);
    QVERIFY(controller.previewClockAdjustment(id,900,2));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.clockReview().isEmpty());QVERIFY(!controller.previewClockAdjustment(id,true,2));QVERIFY(controller.clockReview().isEmpty());
    QVERIFY(controller.previewClockAdjustment(id,900,2));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.clockReview().isEmpty());controller.invalidate();QVERIFY(controller.clockReview().isEmpty());QVERIFY(!controller.adjustClock());
}
void GuideUiTests::remoteAdvancedScheduleDraftCancelWritesNothing() {
    QTemporaryDir dir;GuideController controller(dir.filePath("guide.sqlite"),"policy-ui",nullptr,dir.path());QTRY_VERIFY(controller.ready()&&!controller.busy());
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("guide")),Q_ARG(QVariant,QVariantMap())));QTRY_VERIFY(!controller.busy());
    command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);command(view.data(),InputManager::Confirm);
    for(int i=0;i<8;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("schedule"));
    command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::NavigateLeft);
    const auto value=view->property("draft");const auto draft=value.metaType().id()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toMap():value.toMap();QCOMPARE(draft.value("horizonDays").toInt(),8);QCOMPARE(draft.value("retentionDays").toInt(),1);
    command(view.data(),InputManager::Back);QCOMPARE(view->property("mode").toString(),QString("create"));command(view.data(),InputManager::Back);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);
    QVERIFY(controller.channels().isEmpty());GuideStore reader(dir.filePath("guide.sqlite"));QVERIFY(reader.channels().isEmpty());
}
void GuideUiTests::compactPvrProgressAndAccessiblePressUseTheSameActions() {
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    const QVariantMap current{{"launchKind","kodi_pvr"},{"startUtc",1000},{"endUtc",2000},{"clockUtc",1250},{"offsetSeconds",0},{"title","Current broadcast"}};
    const QVariantMap row{{"id","pvr-station"},{"number",901},{"name","PVR station"},{"sourceName","Kodi PVR"},{"current",current}};
    QQmlComponent component(&engine,QUrl::fromLocalFile(QFileInfo(GUIDE_VIEW_QML).dir().filePath("GuideWidget.qml")));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"controller",QVariantMap{{"compactRows",QVariantList{row}}}},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"originId","home-guide"},{"engaged",true},{"width",640},{"height",360}}));QVERIFY2(widget,qPrintable(component.errorString()));
    QVariant progress;QVERIFY(QMetaObject::invokeMethod(widget.data(),"progressFor",Q_RETURN_ARG(QVariant,progress),Q_ARG(QVariant,current)));QCOMPARE(progress.toDouble(),0.25);
    const QVariantMap noListings{{"launchKind","kodi_pvr"}};QVariant label;QVERIFY(QMetaObject::invokeMethod(widget.data(),"programLabel",Q_RETURN_ARG(QVariant,label),Q_ARG(QVariant,noListings)));QVERIFY(label.toString().contains("Watch live"));
    auto *header=widget->findChild<QObject*>("guideWidgetHeader");QVERIFY(header);
    QQmlExpression pressHeader(QQmlEngine::contextForObject(header),header,"Accessible.pressAction()");pressHeader.evaluate();QVERIFY2(!pressHeader.hasError(),qPrintable(pressHeader.error().toString()));QCOMPARE(dispatch.last.value("type").toString(),QString("open-guide"));
    QQmlExpression rowCount(QQmlEngine::contextForObject(widget.data()),widget.data(),"rows.length");QCOMPARE(rowCount.evaluate().toInt(),1);
    QQuickItem *channel=nullptr;QTRY_VERIFY((channel=visualItem(qobject_cast<QQuickItem*>(widget.data()),"guideWidgetChannel-pvr-station")));
    QQmlExpression pressChannel(QQmlEngine::contextForObject(channel),channel,"Accessible.pressAction()");pressChannel.evaluate();QVERIFY2(!pressChannel.hasError(),qPrintable(pressChannel.error().toString()));QCOMPARE(dispatch.last.value("channelId").toString(),QString("pvr-station"));
    QQmlExpression description(QQmlEngine::contextForObject(channel),channel,"Accessible.description");QVERIFY(description.evaluate().toString().contains("Kodi PVR"));
    QQuickItem *sourceLabel=nullptr;QTRY_VERIFY((sourceLabel=visualItem(qobject_cast<QQuickItem*>(widget.data()),"guideWidgetLabel-pvr-station")));
    widget->setProperty("style",QVariantMap{{"guideSourceBadges",true}});QTRY_VERIFY(sourceLabel->property("text").toString().contains("Kodi PVR"));
    widget->setProperty("style",QVariantMap{{"guideSourceBadges",false}});QTRY_VERIFY(!sourceLabel->property("text").toString().contains("Kodi PVR"));
    QVERIFY(description.evaluate().toString().contains("Kodi PVR"));
    dispatch.last.clear();command(widget.data(),InputManager::Menu);QCOMPARE(dispatch.last.value("type").toString(),QString("open-guide"));QCOMPARE(dispatch.last.value("focusChannelId").toString(),QString("pvr-station"));QCOMPARE(dispatch.last.value("presentation").toMap().value("guideSurface").toString(),QString("overlay"));
    dispatch.last.clear();widget->setProperty("readOnly",true);pressChannel.evaluate();QVERIFY(dispatch.last.isEmpty());
    command(widget.data(),InputManager::Menu);QVERIFY(dispatch.last.isEmpty());
    auto unavailable=current;unavailable["status"]="unavailable";auto unavailableRow=row;unavailableRow["current"]=unavailable;
    widget->setProperty("controller",QVariantMap{{"compactRows",QVariantList{unavailableRow}}});
    QTRY_VERIFY((sourceLabel=visualItem(qobject_cast<QQuickItem*>(widget.data()),"guideWidgetLabel-pvr-station"))&&sourceLabel->property("text").toString().contains("Current broadcast · Unavailable"));
    channel=visualItem(qobject_cast<QQuickItem*>(widget.data()),"guideWidgetChannel-pvr-station");QVERIFY(channel);
    QQmlExpression unavailableDescription(QQmlEngine::contextForObject(channel),channel,"Accessible.description");QVERIFY(unavailableDescription.evaluate().toString().contains("Unavailable"));
    QVariant unavailableProgress;QVERIFY(QMetaObject::invokeMethod(widget.data(),"progressFor",Q_RETURN_ARG(QVariant,unavailableProgress),Q_ARG(QVariant,unavailable)));QCOMPARE(unavailableProgress.toDouble(),0.25);
}
void GuideUiTests::overlayAndFocusedEntryPreserveGuideStateWithoutSaving() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    const auto path=dir.filePath("guide.sqlite");GuideController setup(path,"overlay",nullptr,dir.path());QTRY_VERIFY(setup.ready()&&!setup.busy());
    const auto def=setup.newDefinition();QVERIFY(setup.previewLocal(def,"episode"));QTRY_VERIFY(!setup.busy());QVERIFY(setup.createLocal(def));QTRY_VERIFY(!setup.busy());
    GuideController controller(path,"overlay",nullptr,dir.path());QVERIFY(!controller.ready());
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    const QVariantMap overlay{{"guideSurface","overlay"},{"guideOverlayWidth",80},{"guideOverlayHeight",70},{"guideOverlayRows",3},{"guideOverlayPosition","top-right"}};const auto id=def.value("id").toString();
    QVERIFY(QMetaObject::invokeMethod(view.data(),"showFocused",Q_ARG(QVariant,QString("guide-tile")),Q_ARG(QVariant,overlay),Q_ARG(QVariant,id)));
    QTRY_VERIFY(controller.ready()&&!controller.busy());QTRY_COMPARE(view->property("selectedChannelId").toString(),id);QVERIFY(!view->property("headerFocused").toBool());QVERIFY(view->property("inOverlay").toBool());QCOMPARE(view->property("rowCount").toInt(),3);
    QCOMPARE(view->property("overlayWidthFraction").toDouble(),0.8);QCOMPARE(view->property("overlayHorizontal").toDouble(),1.0);QCOMPARE(view->property("overlayVertical").toDouble(),0.0);
    GuideStore reader(path);const auto channels=reader.channels();const auto wall=QDateTime::currentSecsSinceEpoch();const auto programs=reader.programs(id,wall,wall+86400,1000);
    view->setProperty("channelQuery",def.value("name"));view->setProperty("anchorUtc",wall+3600);const auto anchor=view->property("anchorUtc");
    command(view.data(),InputManager::NavigateUp);command(view.data(),InputManager::Menu);QVERIFY(!view->property("overlay").toBool());QCOMPARE(view->property("anchorUtc"),anchor);QCOMPARE(view->property("selectedChannelId").toString(),id);QCOMPARE(view->property("channelQuery").toString(),def.value("name").toString());
    command(view.data(),InputManager::Menu);QVERIFY(view->property("inOverlay").toBool());command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("add"));QVERIFY(!view->property("inOverlay").toBool());command(view.data(),InputManager::Back);QVERIFY(view->property("inOverlay").toBool());
    view->setProperty("channelQuery","unmatched filter");QVERIFY(QMetaObject::invokeMethod(view.data(),"showFocused",Q_ARG(QVariant,QString("guide-tile")),Q_ARG(QVariant,overlay),Q_ARG(QVariant,id)));QTRY_COMPARE(view->property("selectedChannelId").toString(),id);QCOMPARE(view->property("channelQuery").toString(),QString());
    QVERIFY(QMetaObject::invokeMethod(view.data(),"showFocused",Q_ARG(QVariant,QString("guide-tile")),Q_ARG(QVariant,overlay),Q_ARG(QVariant,QString("missing-channel"))));QTRY_VERIFY(!controller.busy()&&!view->property("navigationError").toString().isEmpty());QVERIFY(view->property("headerFocused").toBool());
    QCOMPARE(reader.channels(),channels);QCOMPARE(reader.programs(id,wall,wall+86400,1000),programs);QVERIFY(dispatch.last.isEmpty());
}
void GuideUiTests::remoteSelectedSlotRepairRequiresReviewAndHonorsCancelAndStaleRevision() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    const auto path=dir.filePath("guide.sqlite");GuideController controller(path,"repair-ui",nullptr,dir.path());QTRY_VERIFY(controller.ready()&&!controller.busy());
    const auto def=controller.newDefinition();QVERIFY(controller.previewLocal(def,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(def));QTRY_VERIFY(!controller.busy());
    const auto id=def.value("id").toString();const auto wall=QDateTime::currentSecsSinceEpoch();GuideStore reader(path);const auto before=reader.programs(id,wall,wall+86400,1000);
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("guide")),Q_ARG(QVariant,QVariantMap())));QTRY_VERIFY(!controller.busy());command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Menu);
    for(int i=0;i<12;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QCOMPARE(view->property("mode").toString(),QString("repair"));
    QCOMPARE(controller.repairReview().value("alternatives").toList().size(),50);QCOMPARE(controller.repairReview().value("total").toInt(),101);
    command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("repairConfirm"));command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("repair"));QCOMPARE(reader.programs(id,wall,wall+86400,1000),before);
    command(view.data(),InputManager::Back);QCOMPARE(view->property("mode").toString(),QString("context"));QCOMPARE(reader.channel(id).value("revision").toInt(),1);
    command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());command(view.data(),InputManager::Confirm);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QCOMPARE(reader.channel(id).value("revision").toInt(),2);
    const auto repaired=reader.programs(id,wall,wall+86400,1000);QCOMPARE(repaired.size(),before.size());for(int i=0;i<before.size();++i)for(const auto &key:QStringList{"id","startUtc","endUtc"})QCOMPARE(repaired[i].toMap().value(key),before[i].toMap().value(key));
    command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(!controller.repairReview().isEmpty());
    QVERIFY(reader.setPreferences(id,{{"favorite",true}},2));command(view.data(),InputManager::Confirm);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(controller.errorString().contains("changed"));QCOMPARE(reader.programs(id,wall,wall+86400,1000),repaired);QCOMPARE(reader.channel(id).value("revision").toInt(),3);
}
void GuideUiTests::nestedRuleEditorDoesNotDiscardChildren() {
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_RULE_QML));
    QScopedPointer<QObject> editor(component.createWithInitialProperties({{"options",options()},{"appearance",appearance()},{"width",1000},{"height",650}}));QVERIFY2(editor,qPrintable(component.errorString()));
    const QVariantMap a{{"field","studio"},{"op","contains"},{"value","Cartoon Network"}},b{{"field","year"},{"op","after"},{"value",2000}};
    const QVariantMap tree{{"op","all"},{"rules",QVariantList{a,b}}};
    QVERIFY(QMetaObject::invokeMethod(editor.data(),"begin",Q_ARG(QVariant,tree)));
    command(editor.data(),InputManager::Confirm);QCOMPARE(editor->property("tree").toMap().value("rules").toList().size(),2);
    command(editor.data(),InputManager::Confirm);QVERIFY(!editor->property("message").toString().isEmpty());
    QCOMPARE(editor->property("tree").toMap().value("rules").toList().size(),2);
}
void GuideUiTests::remoteReorderPreviewCancelAndApply() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    GuideController controller(dir.filePath("guide.sqlite"),"move",nullptr,dir.path());QTRY_VERIFY(controller.ready() && !controller.busy());
    QStringList original;
    for(int i=0;i<2;++i){auto def=controller.newDefinition();original.append(def.value("id").toString());QVERIFY(controller.previewLocal(def,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(def));QTRY_VERIFY(!controller.busy());}
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("guide")),Q_ARG(QVariant,QVariantMap())));QTRY_VERIFY(!controller.busy());
    command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);
    QCOMPARE(view->property("mode").toString(),QString("manage"));command(view.data(),InputManager::Confirm);
    for(int i=0;i<3;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);
    QCOMPARE(view->property("mode").toString(),QString("move"));QCOMPARE(controller.channels().first().toMap().value("id").toString(),original.first());
    command(view.data(),InputManager::Back);QCOMPARE(view->property("mode").toString(),QString("context"));
    QCOMPARE(controller.channels().first().toMap().value("id").toString(),original.first());
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QCOMPARE(view->property("mode").toString(),QString("move"));
    command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QCOMPARE(controller.channels().first().toMap().value("id").toString(),original.last());
}
void GuideUiTests::backgroundRefreshQueuesConfirmedEditsWithoutPrematureIdle() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    GuideController controller(dir.filePath("guide.sqlite"),"queue",nullptr,dir.path());QTRY_VERIFY(controller.ready() && !controller.busy());
    const auto definition=controller.newDefinition();QVERIFY(controller.previewLocal(definition,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(definition));QTRY_VERIFY(!controller.busy());
    const auto channel=controller.channels().first().toMap();bool prematureIdle=false;
    const auto connection=connect(&controller,&GuideController::changed,this,[&]{if(!controller.busy() && controller.channels().first().toMap().value("name")!="Queued edit")prematureIdle=true;});
    controller.refresh();QVERIFY(controller.backgroundBusy());
    QVERIFY(controller.setMetadata(channel.value("id").toString(),{{"name","Queued edit"}},channel.value("revision").toInt()));
    QVERIFY(controller.busy());QVERIFY(!controller.backgroundBusy());
    QTRY_VERIFY(!controller.busy());disconnect(connection);QVERIFY(!prematureIdle);QCOMPARE(controller.channels().first().toMap().value("name").toString(),QString("Queued edit"));
    const auto updated=controller.channels().first().toMap();controller.refresh();
    QVERIFY(controller.setMetadata(updated.value("id").toString(),{{"name","Cancelled edit"}},updated.value("revision").toInt()));controller.cancel();
    QTRY_VERIFY(!controller.busy());QCOMPARE(controller.channels().first().toMap().value("name").toString(),QString("Queued edit"));
}
void GuideUiTests::configuredCommandPaletteGuideEntryNeedsNoLayoutRewrite() {
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QFile config(GUIDE_DEFAULTS);QVERIFY(config.open(QIODevice::ReadOnly));const auto actions=QJsonDocument::fromJson(config.readAll()).object().value("actions").toArray().toVariantList();
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_COMMAND_CENTER_QML));
    QScopedPointer<QObject> palette(component.createWithInitialProperties({{"apps",QVariantList()},{"packages",QVariantList()},{"shellActions",actions},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(palette,qPrintable(component.errorString()));
    palette->setProperty("open",true);palette->setProperty("query","guide");palette->setProperty("focusArea","results");
    QSignalSpy requested(palette.data(),SIGNAL(shellActionRequested(QVariant)));QVERIFY(requested.isValid());
    command(palette.data(),InputManager::Confirm);QCOMPARE(requested.size(),1);
    QCOMPARE(requested.first().first().toMap().value("type").toString(),QString("open-guide"));
}
void GuideUiTests::rangeAndSetRulesUseRemoteEditableIndividualValues() {
    const auto row=[](QObject *object){const auto value=object->property("editingRow");return value.metaType().id()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toMap():value.toMap();};
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_RULE_QML));
    QScopedPointer<QObject> editor(component.createWithInitialProperties({{"options",options()},{"appearance",appearance()},{"width",1000},{"height",650}}));QVERIFY2(editor,qPrintable(component.errorString()));
    const QVariantMap range{{"field","year"},{"op","in-range"},{"value",QVariantList{1990,2020}}};
    QVERIFY(QMetaObject::invokeMethod(editor.data(),"begin",Q_ARG(QVariant,range)));
    for(int i=0;i<3;++i)command(editor.data(),InputManager::NavigateDown);command(editor.data(),InputManager::Confirm);
    QVERIFY(editor->property("typing").toBool());QCOMPARE(row(editor.data()).value("valueIndex").toInt(),0);
    command(editor.data(),InputManager::Back);QVERIFY(!editor->property("typing").toBool());
    command(editor.data(),InputManager::NavigateDown);command(editor.data(),InputManager::Confirm);QVERIFY(editor->property("typing").toBool());
    QCOMPARE(row(editor.data()).value("valueIndex").toInt(),1);
    command(editor.data(),InputManager::Back);
    const QVariantMap set{{"field","tag"},{"op","one-of"},{"value",QVariantList{QString("Family, classics"),QString("Favorites")}}};
    QVERIFY(QMetaObject::invokeMethod(editor.data(),"begin",Q_ARG(QVariant,set)));
    for(int i=0;i<4;++i)command(editor.data(),InputManager::NavigateDown);command(editor.data(),InputManager::Confirm); // explicit Remove value 1
    const auto values=editor->property("tree").toMap().value("value").toList();QCOMPARE(values,QVariantList{QString("Favorites")});
    command(editor.data(),InputManager::Confirm);QVERIFY(editor->property("typing").toBool()); // Add value
    command(editor.data(),InputManager::Back);QCOMPARE(editor->property("tree").toMap().value("value").toList(),values);
}
void GuideUiTests::startupMaintainsExpiredSchedulesWithoutKodiReads() {
    QTemporaryDir dir;const auto path=dir.filePath("guide.sqlite");const auto wall=QDateTime::currentSecsSinceEpoch();
    QVariantMap def{{"schemaVersion",1},{"id","offline"},{"name","Offline channel"},{"number",1},{"clock","real-time"},
        {"sequence","seeded-shuffle"},{"seed",42},{"timezone","UTC"},{"rule",QVariantMap{{"field","source"},{"op","is"},{"value","kodi"}}}};
    const QVariantList items{QVariantMap{{"mediaType","episode"},{"mediaId",1},{"title","Cached episode"},{"duration",1800},{"source","kodi"}}};
    {GuideStore store(path);QVERIFY(store.createLocal(def,items,wall-400*86400,1));}
    // No bridge server exists: maintenance must use saved candidates only.
    GuideController controller(path,"offline",nullptr,dir.filePath("no-bridge"));
    QTRY_VERIFY(controller.ready() && !controller.busy());
    QTRY_VERIFY(!controller.channels().isEmpty() && controller.channels().first().toMap().value("generatedThroughUtc").toLongLong()>wall+6*86400);
    QVERIFY(controller.errorString().isEmpty());QVERIFY(!controller.scheduleStatus().isEmpty());
    QVERIFY(!controller.compactRows().first().toMap().value("current").toMap().isEmpty());
    GuideStore persisted(path);QVERIFY(persisted.ready());
    const auto channels=persisted.channels(),programs=persisted.programs("offline",wall-2*86400,wall+8*86400,1000),candidates=persisted.candidateSnapshot("offline");
    auto before=controller.rows();QVERIFY(!before.isEmpty());auto oldRow=before.first().toMap();
    const auto oldCurrent=oldRow.take("current").toMap();
    QCOMPARE(oldCurrent,persisted.tuneTarget("offline",oldCurrent.value("clockUtc").toLongLong()));before[0]=oldRow;
    // Live progress must advance across refresh; it is not persisted schedule data.
    QTRY_VERIFY(QDateTime::currentSecsSinceEpoch()>oldCurrent.value("clockUtc").toLongLong());
    controller.refresh();QTRY_VERIFY(!controller.busy());auto after=controller.rows();QCOMPARE(after.size(),before.size());
    auto newRow=after.first().toMap();const auto current=newRow.take("current").toMap();
    QVERIFY(current.value("clockUtc").toLongLong()>oldCurrent.value("clockUtc").toLongLong());
    QCOMPARE(current,persisted.tuneTarget("offline",current.value("clockUtc").toLongLong()));after[0]=newRow;
    QCOMPARE(after,before);QCOMPARE(persisted.channels(),channels);
    QCOMPARE(persisted.programs("offline",wall-2*86400,wall+8*86400,1000),programs);
    QCOMPARE(persisted.candidateSnapshot("offline"),candidates);QVERIFY(controller.errorString().isEmpty());
}
void GuideUiTests::pvrPreviewMappingImportAndFailedRefreshPreserveCache() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));
    connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{auto raw=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",raw);if(!raw.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(raw).object();const auto method=request.value("method").toString();QJsonObject reply{{"jsonrpc","2.0"},{"id",request.value("id")}};
            if(server.property("fail").toBool() && method=="PVR.GetBroadcasts")reply["error"]=QJsonObject{{"code",-32100}};
            else if(method=="PVR.GetChannels"){
                const auto properties=request.value("params").toObject().value("properties").toArray();server.setProperty("channelProperties",properties.toVariantList());
                QJsonArray channels{
                QJsonObject{{"channelid",7},{"clientid",9},{"uniqueid",101},{"label","PVR station"},{"channelnumber",1},{"locked",false},{"hidden",false}},
                QJsonObject{{"channelid",8},{"clientid",9},{"uniqueid",102},{"label","No EPG station"},{"channelnumber",1},{"locked",false},{"hidden",false}}};
                if(!properties.contains("uniqueid") || server.property("omitUniqueId").toBool())for(int i=0;i<channels.size();++i){auto item=channels[i].toObject();item.remove("uniqueid");channels[i]=item;}
                reply["result"]=QJsonObject{{"channels",channels},
                {"limits",QJsonObject{{"start",0},{"end",2},{"total",2}}}};
            }
            else if(method=="PVR.GetBroadcasts"){
                QJsonArray programs;if(request.value("params").toObject().value("channelid").toInt()==7){const auto time=QDateTime::currentDateTimeUtc();programs.append(QJsonObject{{"broadcastid",77},{"title","PVR current program"},{"starttime",time.addSecs(-600).toString("yyyy-MM-dd HH:mm:ss")},{"endtime",time.addSecs(1200).toString("yyyy-MM-dd HH:mm:ss")}});}
                reply["result"]=QJsonObject{{"broadcasts",programs},{"limits",QJsonObject{{"start",0},{"end",programs.size()},{"total",programs.size()}}}};
            }else QFAIL("Unexpected PVR preview method");
            socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });
    }});
    GuideController controller(dir.filePath("guide.sqlite"),"pvr",nullptr,dir.path());QTRY_VERIFY(controller.ready() && !controller.busy());
    QVERIFY(controller.previewPvr());QTRY_VERIFY(!controller.busy());QVERIFY2(controller.sourcePreviewReady(),qPrintable(controller.errorString()));
    QCOMPARE(controller.sourcePreview().size(),2);QVERIFY(controller.channels().isEmpty());
    QVERIFY(server.property("channelProperties").toList().contains(QString("uniqueid")));QVERIFY(server.property("channelProperties").toList().contains(QString("clientid")));
    QCOMPARE(controller.sourcePreview()[1].toMap().value("definition").toMap().value("number").toInt(),2); // explicit reviewed mapping, not silent overwrite
    QVERIFY(!controller.setSourceNumber(1,1));QVERIFY(controller.setSourceNumber(1,10));
    const auto reviewed=controller.sourcePreview();
    QVERIFY(!controller.setSourceNumber(1,10.5));QVERIFY(!controller.setSourceNumber(1,true));
    QVERIFY(!controller.setSourceNumber(1,QString("10.5")));QCOMPARE(controller.sourcePreview(),reviewed);
    QVERIFY(controller.importPvr());QTRY_VERIFY(!controller.busy());QCOMPARE(controller.channels().size(),2);
    QSignalSpy tune(&controller,&GuideController::targetReady);const auto id=controller.channels()[1].toMap().value("id").toString();
    QVERIFY(controller.tune(id,"guide"));QTRY_COMPARE(tune.size(),1);QTRY_VERIFY(!controller.busy());QCOMPARE(tune.first().first().toMap().value("launchKind").toString(),QString("kodi_pvr"));
    const auto cached=controller.rows();server.setProperty("fail",true);QVERIFY(controller.previewPvr());QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.sourcePreviewReady());QVERIFY(!controller.errorString().isEmpty());QCOMPARE(controller.rows(),cached);
    server.setProperty("fail",false);server.setProperty("omitUniqueId",true);QVERIFY(controller.previewPvr());QTRY_VERIFY(!controller.busy());QVERIFY(!controller.sourcePreviewReady());QVERIFY(!controller.errorString().isEmpty());QCOMPARE(controller.rows(),cached);QCOMPARE(controller.channels().size(),2);
    server.setProperty("omitUniqueId",false);QVERIFY(controller.previewPvr());controller.cancel();QVERIFY(!controller.sourcePreviewReady());QCOMPARE(controller.channels().size(),2);QTRY_VERIFY(!controller.busy());
    {
        GuideStore reader(dir.filePath("guide.sqlite"));const auto saved=reader.channels();
        auto configured=options();auto sources=configured.value("sources").toList();auto second=sources.first().toMap();second["id"]="second-pvr";second["name"]="Second configured backend";configured["sources"]=QVariantList{sources.first(),second};
        InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
        QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",configured},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("source-origin")),Q_ARG(QVariant,QVariantMap())));QTRY_VERIFY(!controller.busy());
        for(int i=0;i<3;++i)command(view.data(),InputManager::NavigateRight);command(view.data(),InputManager::Confirm);command(view.data(),InputManager::NavigateDown);server.setProperty("omitUniqueId",true);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(!controller.sourcePreviewReady());
        QQmlExpression firstChoice(QQmlEngine::contextForObject(view.data()),view.data(),"menuRows[0]");QCOMPARE(firstChoice.evaluate().toString(),QString("Retry source preview"));QCOMPARE(reader.channels(),saved);
        server.setProperty("omitUniqueId",false);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY2(controller.sourcePreviewReady(),qPrintable(controller.errorString()));QCOMPARE(view->property("mode").toString(),QString("sourcePreview"));
        command(view.data(),InputManager::Back);QCOMPARE(view->property("mode").toString(),QString("sources"));QCOMPARE(view->property("menuIndex").toInt(),1);QCOMPARE(reader.channels(),saved);
    }
    QVERIFY(controller.previewPvr());QTRY_VERIFY(!controller.busy());QVERIFY(controller.sourcePreviewReady());controller.invalidate();QVERIFY(!controller.sourcePreviewReady());QVERIFY(controller.sourcePreview().isEmpty());
}
void GuideUiTests::pvrUtcParsingAndCacheIgnoreCivilDstGaps() {
    const QTimeZone losAngeles("America/Los_Angeles");QVERIFY(losAngeles.isValid());
    const QDate date(2026,3,8);const QTime time(2,30);
    QVERIFY(!QDateTime(date,time,losAngeles,QDateTime::TransitionResolution::Reject).isValid());
    QCOMPARE(GuideController::pvrUtc(QString("2026-03-08 02:30:00")),QDateTime(date,time,Qt::UTC).toSecsSinceEpoch());
    for(const auto &bad:QVariantList{QString("2026-02-30 02:30:00"),QString("2026-03-08 25:00:00"),QString("2026-03-08T02:30:00Z"),true,123})QCOMPARE(GuideController::pvrUtc(bad),qint64(-1));
    // The actual adapter/cache uses a recent UTC listing within its bounded refresh window.
    const auto utc=QDateTime(QDateTime::currentDateTimeUtc().date().addDays(-1),QTime(2,30),Qt::UTC);const auto text=utc.toString("yyyy-MM-dd HH:mm:ss");
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));
    connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{auto raw=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",raw);if(!raw.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(raw).object();QJsonObject result;
            if(request.value("method").toString()=="PVR.GetChannels"){
                QJsonObject channel{{"channelid",7},{"clientid",9},{"label","UTC gap station"},{"channelnumber",1},{"locked",false},{"hidden",false}};
                if(request.value("params").toObject().value("properties").toArray().contains("uniqueid"))channel["uniqueid"]=101;
                result={{"channels",QJsonArray{channel}},{"limits",QJsonObject{{"start",0},{"end",1},{"total",1}}}};
            }
            else result={{"broadcasts",QJsonArray{QJsonObject{{"broadcastid",77},{"title","UTC gap program"},{"starttime",text},{"endtime",utc.addSecs(1800).toString("yyyy-MM-dd HH:mm:ss")}}}},{"limits",QJsonObject{{"start",0},{"end",1},{"total",1}}}};
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request.value("id")},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });
    }});
    const auto path=dir.filePath("guide.sqlite");GuideController controller(path,"utc-gap",nullptr,dir.path());QTRY_VERIFY(controller.ready()&&!controller.busy());
    QVERIFY(controller.previewPvr());QTRY_VERIFY(!controller.busy());QVERIFY2(controller.sourcePreviewReady(),qPrintable(controller.errorString()));
    const auto entry=controller.sourcePreview().first().toMap();const auto programs=entry.value("programs").toList();QCOMPARE(programs.size(),1);QCOMPARE(programs.first().toMap().value("startUtc").toLongLong(),utc.toSecsSinceEpoch());
    QVERIFY(controller.importPvr());QTRY_VERIFY(!controller.busy());GuideStore reader(path);
    const auto cached=reader.programs(entry.value("definition").toMap().value("id").toString(),utc.toSecsSinceEpoch(),utc.toSecsSinceEpoch()+1800);
    QCOMPARE(cached.size(),1);QCOMPARE(cached.first().toMap().value("startUtc").toLongLong(),utc.toSecsSinceEpoch());
}
void GuideUiTests::remoteRuleEditDuplicateAndRegenerationAreReviewedTransactions() {
    QTemporaryDir dir;QLocalServer server;QVERIFY(server.listen(dir.filePath("rpc.sock")));serve(server);
    const auto path=dir.filePath("guide.sqlite");GuideController controller(path,"edits",nullptr,dir.path());QTRY_VERIFY(controller.ready() && !controller.busy());
    const auto def=controller.newDefinition();QVERIFY(controller.previewLocal(def,"episode"));QTRY_VERIFY(!controller.busy());QVERIFY(controller.createLocal(def));QTRY_VERIFY(!controller.busy());
    const auto id=def.value("id").toString();GuideStore reader(path);const auto wall=QDateTime::currentSecsSinceEpoch();const auto committed=reader.programs(id,wall,wall+86400,1000);
    InputManager input;GuideDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GUIDE_VIEW_QML));QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller",QVariant::fromValue(&controller)},{"dispatcher",QVariant::fromValue(&dispatch)},{"options",options()},{"appearance",appearance()},{"width",1280},{"height",720}}));QVERIFY2(view,qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(view.data(),"show",Q_ARG(QVariant,QString("guide")),Q_ARG(QVariant,QVariantMap())));QTRY_VERIFY(!controller.busy());command(view.data(),InputManager::NavigateDown);
    command(view.data(),InputManager::Menu);for(int i=0;i<7;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);
    QCOMPARE(view->property("mode").toString(),QString("create"));QCOMPARE(view->property("editRevision").toInt(),1);
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::NavigateRight); // sequence changes only in draft
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());
    command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QCOMPARE(reader.channel(id).value("revision").toInt(),2);QCOMPARE(reader.programs(id,wall,wall+86400,1000),committed);
    const auto duplicate=[&]{command(view.data(),InputManager::Menu);for(int i=0;i<9;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);};
    duplicate();QTRY_COMPARE(view->property("mode").toString(),QString("create"));QCOMPARE(controller.channels().size(),1);const auto draftValue=view->property("draft");const auto draftMap=draftValue.metaType().id()==qMetaTypeId<QJSValue>()?draftValue.value<QJSValue>().toVariant().toMap():draftValue.toMap();QVERIFY(draftMap.value("id").toString()!=id);
    command(view.data(),InputManager::Back);command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QCOMPARE(controller.channels().size(),1);
    duplicate();QTRY_COMPARE(view->property("mode").toString(),QString("create"));for(int i=0;i<7;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QCOMPARE(controller.channels().size(),2);
    const auto copy=view->property("selectedChannelId").toString();QVERIFY(copy!=id);const auto baseline=reader.programs(copy,wall,wall+86400,1000);
    command(view.data(),InputManager::Menu);for(int i=0;i<8;++i)command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QVERIFY(!controller.regenerationReview().isEmpty());
    command(view.data(),InputManager::Confirm);QCOMPARE(reader.programs(copy,wall,wall+86400,1000),baseline); // Cancel-first review
    command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());command(view.data(),InputManager::NavigateDown);command(view.data(),InputManager::Confirm);QTRY_VERIFY(!controller.busy());QCOMPARE(reader.channel(copy).value("revision").toInt(),2);
    const auto copyStart=baseline.first().toMap().value("startUtc").toLongLong();QCOMPARE(reader.programs(copy,copyStart,copyStart+1,1).first(),baseline.first());QCOMPARE(reader.programs(id,wall,wall+86400,1000),committed);
}
QTEST_MAIN(GuideUiTests)
#include "guide_ui.moc"
