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
    void nestedRuleEditorDoesNotDiscardChildren();
    void remoteReorderPreviewCancelAndApply();
    void backgroundRefreshQueuesConfirmedEditsWithoutPrematureIdle();
    void configuredCommandPaletteGuideEntryNeedsNoLayoutRewrite();
    void rangeAndSetRulesUseRemoteEditableIndividualValues();
};
namespace {
void serve(QLocalServer &server) {
    QObject::connect(&server,&QLocalServer::newConnection,&server,[&server]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        QObject::connect(socket,&QLocalSocket::readyRead,socket,[socket,&server]{
            auto input=socket->property("input").toByteArray()+socket->readAll();socket->setProperty("input",input);if(!input.endsWith('\n'))return;
            const auto request=QJsonDocument::fromJson(input).object();server.setProperty("requests",server.property("requests").toInt()+1);
            const auto params=request.value("params").toObject();const auto kind=params.value("kind").toString();const int start=params.value("start").toInt(),end=qMin(101,start+50);
            QJsonArray items;for(int i=start;i<end;++i)items.append(QJsonObject{{"mediaType",kind},{"mediaId",i+1},{"title",QString("Cartoon %1").arg(i+1)},{"duration",1800},{"source","kodi"},{"studio",QJsonArray{"Cartoon Network"}}});
            const QJsonObject result{{"items",items},{"limits",QJsonObject{{"start",start},{"end",end},{"total",101}}},{"skipped",0}};
            socket->write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",request.value("id")},{"result",result}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
        });
    }});
}
void command(QObject *object,int value) {QVERIFY(QMetaObject::invokeMethod(object,"handleCommand",Q_ARG(QVariant,value)));}
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
        controller.invalidate();QVERIFY(!controller.ready());QVERIFY(!controller.previewLocal(definition,"episode"));
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
    command(view.data(),InputManager::NavigateUp);QVERIFY(view->property("headerFocused").toBool());
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
QTEST_MAIN(GuideUiTests)
#include "guide_ui.moc"
