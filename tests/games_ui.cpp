#include "games/GameLibrary.h"
#include "app/AppRegistry.h"
#include "input/InputManager.h"
#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QJSValue>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QFile>

class GameDispatchProbe : public QObject {
    Q_OBJECT
public:
    QVariantMap lastAction;
    QString origin;
    Q_INVOKABLE bool dispatch(const QVariantMap &action,const QString &from) {lastAction=action;origin=from;return true;}
signals:
    void rejected(const QString &reason);
};
class GamesUiTests : public QObject {
    Q_OBJECT
private slots:
    void remoteDetailsFavoriteRemoveAndCancel();
    void remoteAddReviewWritesOnlyOnConfirm();
};
static AppDefinition runtime() {
    AppDefinition app;app.id="local-test";app.name="Local game";app.type="desktop";
    app.presentationMode="game";app.section="Games";app.accent=QColor("#556677");
    app.launchConfiguration={{"desktopId","isolde-test-game"}};return app;
}
static void command(QObject *widget,int value) {
    QVariant result;QVERIFY(QMetaObject::invokeMethod(widget,"handleCommand",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,value)));
}
void GamesUiTests::remoteDetailsFavoriteRemoveAndCancel() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);QVERIFY(library.ready());
    const auto id=library.addGame({{"schemaVersion",1},{"title","Playable fixture"},{"platform","PC"},{"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    QVERIFY2(!id.isEmpty(),qPrintable(library.errorString()));
    InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QTRY_VERIFY_WITH_TIMEOUT(component.status()!=QQmlComponent::Loading,5000);
    QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"originId","games-shelf"},{"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY(widget);
    const auto items=widget->property("items");
    QCOMPARE(items.metaType()==QMetaType::fromType<QJSValue>()?items.value<QJSValue>().toVariant().toList().size():items.toList().size(),1);
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("details"));
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(dispatch.lastAction.value("gameId").toString(),id);QCOMPARE(dispatch.origin,QString("games-shelf"));
    QCOMPARE(widget->property("selected").toInt(),0);
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    QVERIFY(library.details(id).value("favorite").toBool());
    command(widget.data(),InputManager::Back);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("remove"));
    command(widget.data(),InputManager::Confirm);QVERIFY(!library.details(id).isEmpty()); // default Cancel
    command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::NavigateRight);command(widget.data(),InputManager::Confirm);
    QVERIFY(library.details(id).isEmpty());
}
void GamesUiTests::remoteAddReviewWritesOnlyOnConfirm() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));
    QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("runtime"));
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("keyboard"));
    command(widget.data(),InputManager::Back);QVERIFY(library.page().isEmpty());
    // Enter the title using only the real on-screen keyboard's remote path.
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::Confirm); // first key = 1
    for(int i=0;i<5;++i)command(widget.data(),InputManager::NavigateDown);
    for(int i=0;i<9;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm); // Done: title
    QCOMPARE(widget->property("field").toInt(),1);
    for(int i=0;i<5;++i)command(widget.data(),InputManager::NavigateDown);
    for(int i=0;i<9;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm); // Done: default platform
    QCOMPARE(widget->property("mode").toString(),QString("review"));QVERIFY(library.page().isEmpty());
    command(widget.data(),InputManager::Confirm);QCOMPARE(library.page().size(),1);
    QCOMPARE(library.page()[0].toMap().value("title").toString(),QString("1"));
}
QTEST_MAIN(GamesUiTests)
#include "games_ui.moc"
