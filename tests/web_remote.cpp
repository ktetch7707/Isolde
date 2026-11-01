#include "input/InputManager.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

class RemoteReport final : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void result(const QVariant &value){emit reported(value);}
    Q_INVOKABLE void ready(){emit loaded();}
signals:
    void reported(const QVariant &value);
    void loaded();
};
class WebRemoteTest final : public QObject {
    Q_OBJECT
private slots:
    void directionalActivationKeyboardCancelAndPageActions(){
        InputManager input;RemoteReport report;QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("inputManager",&input);engine.rootContext()->setContextProperty("remoteTest",&report);
        QSignalSpy loaded(&report,&RemoteReport::loaded),result(&report,&RemoteReport::reported);
        engine.load(QUrl::fromLocalFile(WEB_REMOTE_TEST_QML));QVERIFY(!engine.rootObjects().isEmpty());
        auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(loaded.count() || loaded.wait(15000));
        auto value=[&](const QString &script,int world=0){
            result.clear();QMetaObject::invokeMethod(window,"evaluate",Q_ARG(QVariant,script),Q_ARG(QVariant,world));
            if(result.isEmpty() && !result.wait(5000))return QVariant();return result.first().first();
        };
        auto key=[&](Qt::Key code){QTest::keyClick(window,code);QTest::qWait(80);};
        auto capture=[&](const QString &name){const auto dir=qEnvironmentVariable("ISOLDE_TEST_ARTIFACTS");if(!dir.isEmpty())window->grabWindow().save(dir+"/studio-test-web-remote-"+name+".png");};
        key(Qt::Key_Right);QCOMPARE(value("document.activeElement.id").toString(),QString("one"));
        key(Qt::Key_Right);QCOMPARE(value("document.activeElement.id").toString(),QString("two"));key(Qt::Key_Return);
        QCOMPARE(value("document.getElementById('two').dataset.clicked").toString(),QString("yes"));capture("focus");
        key(Qt::Key_Down);QCOMPARE(value("document.activeElement.id").toString(),QString("entry"));key(Qt::Key_Return);
        QVERIFY(value("__isoldeRemote.state().keyboardOpen",1).toBool());key(Qt::Key_Return);capture("keyboard");key(Qt::Key_Escape);
        QCOMPARE(value("document.getElementById('entry').value").toString(),QString());
        key(Qt::Key_Return);key(Qt::Key_Return);for(int i=0;i<4;i++)key(Qt::Key_Down);for(int i=0;i<4;i++)key(Qt::Key_Right);key(Qt::Key_Return);
        QCOMPARE(value("document.getElementById('entry').value").toString(),QString("1"));
        QVERIFY(!value("__isoldeRemote.state().keyboardOpen",1).toBool());
        key(Qt::Key_Menu);QVERIFY(value("__isoldeRemote.state().pageActionsOpen",1).toBool());capture("page-actions");
        key(Qt::Key_Down);key(Qt::Key_Escape);QVERIFY(!value("__isoldeRemote.state().pageActionsOpen",1).toBool());
        QCOMPARE(value("__isoldeRemote.state().mode",1).toString(),QString("focus"));
        key(Qt::Key_Down);key(Qt::Key_Return);QVERIFY(value("document.getElementById('choice').checked").toBool());
        // Replacing controls and opening a modal must not strand remote focus.
        value("document.body.insertAdjacentHTML('beforeend', '<dialog id=modal><button id=modalCancel>Cancel</button><button id=modalAccept>Accept</button></dialog>');document.getElementById('modal').showModal()");
        QTest::qWait(150);
        key(Qt::Key_Right);QCOMPARE(value("document.activeElement.id").toString(),QString("modalAccept"));capture("dialog");
        QVERIFY(value("document.getElementById('modal').querySelectorAll('div').length >= 2").toBool());
        value("document.getElementById('modal').insertAdjacentHTML('beforeend','<input id=modalText aria-label=Text>');document.getElementById('modalText').focus()");QTest::qWait(100);
        key(Qt::Key_Return);QVERIFY(value("__isoldeRemote.state().keyboardOpen",1).toBool());capture("dialog-keyboard");key(Qt::Key_Escape);
        value("document.getElementById('modal').remove()");QTest::qWait(150);
        QCOMPARE(value("document.activeElement.id").toString(),QString("one"));
        // A semantic card in a scrolling shelf needs no coordinate targeting.
        value("document.body.insertAdjacentHTML('beforeend', '<div id=shelf style=\"position:absolute;top:380px;left:40px;width:400px;height:100px;overflow:auto\"><div style=\"height:300px\"></div><button id=last style=\"height:60px;width:160px\" onclick=\"this.dataset.selected=1\">Last card</button></div>')");
        key(Qt::Key_Down);key(Qt::Key_Down);key(Qt::Key_Down);
        QCOMPARE(value("document.activeElement.id").toString(),QString("last"));
        QVERIFY(value("document.getElementById('shelf').scrollTop > 0").toBool());
        key(Qt::Key_Return);QCOMPARE(value("document.getElementById('last').dataset.selected").toString(),QString("1"));capture("shelf");
    }
};
int main(int argc,char **argv){QtWebEngineQuick::initialize();QGuiApplication app(argc,argv);WebRemoteTest test;return QTest::qExec(&test,argc,argv);}
#include "web_remote.moc"
