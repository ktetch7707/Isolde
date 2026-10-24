#include "sessions/DesktopMode.h"
#include "input/InputManager.h"
#include <QtTest>
#include <QDBusVirtualObject>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QProcess>
#include <QScopeGuard>
#include <QUuid>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>

class Portal final : public QDBusVirtualObject {
public:
    bool deny=false,empty=false,badSession=false,validBind=false;
    int delay=0,registrationDelay=0,registrations=0,creates=0;
    bool registered=false,rejectRegistration=false,objectPathResult=false;
    QString session;
    QStringList closed;
    QString introspect(const QString &) const override{return {};}
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &bus) override{
        if(message.member()=="Register"){
            ++registrations;
            if(registered || rejectRegistration || message.signature()!="sa{sv}" || message.arguments().first().toString()!=DesktopMode::ApplicationId){bus.send(message.createErrorReply("org.freedesktop.portal.Error.NotAllowed","Invalid or duplicate identity"));return true;}
            registered=true;
            QTimer::singleShot(registrationDelay,this,[=]{bus.send(message.createReply());});return true;
        }
        if(message.member()=="Close"){closed.append(message.path());bus.send(message.createReply());return true;}
        const bool create=message.member()=="CreateSession";
        if(!create && message.member()!="BindShortcuts")return false;
        if(!registered){bus.send(message.createErrorReply("org.freedesktop.portal.Error.NotAllowed","An app id is required"));return true;}
        if(create)++creates;
        const auto options=qdbus_cast<QVariantMap>(message.arguments().last());
        auto sender=message.service();sender.remove(':');sender.replace('.','_');
        const auto handle="/org/freedesktop/portal/desktop/request/"+sender+"/"+options.value("handle_token").toString();
        QVariantMap result;
        if(create){session="/org/freedesktop/portal/desktop/session/"+sender+"/"+options.value("session_handle_token").toString();result["session_handle"]=objectPathResult?QVariant::fromValue(QDBusObjectPath(session)):QVariant(badSession?"/wrong/session":session);}
        else {
            const auto entries=qdbus_cast<QList<DesktopShortcut>>(message.arguments()[1]);
            validBind=entries.size()==1 && entries.first().id=="return-home" && entries.first().properties.value("preferred_trigger")=="Home";
            const QList<DesktopShortcut> granted=empty?QList<DesktopShortcut>{}:QList<DesktopShortcut>{{"return-home",{{"trigger_description","Home"}}}};
            result["shortcuts"]=QVariant::fromValue(granted);
        }
        bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath(handle))));
        const auto code=deny?uint(1):uint(0);
        QTimer::singleShot(delay,this,[=]{auto response=QDBusMessage::createSignal(handle,"org.freedesktop.portal.Request","Response");response<<code<<result;bus.send(response);});
        return true;
    }
    void activate(const QDBusConnection &bus,const QString &target={}){
        auto signal=QDBusMessage::createSignal("/org/freedesktop/portal/desktop","org.freedesktop.portal.GlobalShortcuts","Activated");
        signal<<QVariant::fromValue(QDBusObjectPath(target.isEmpty()?session:target))<<QString("return-home")<<qulonglong(0)<<QVariantMap{};bus.send(signal);
    }
};
class DesktopTests final : public QObject {
    Q_OBJECT
private:
    QProcess daemon;
    QString address,serverName,clientName;
    std::unique_ptr<QDBusConnection> server,client;
    std::unique_ptr<Portal> portal;
    QVariantMap options;
private slots:
    void init(){
        QFile defaults(QStringLiteral(STUDIO_DEFAULTS));QVERIFY(defaults.open(QIODevice::ReadOnly));options=QJsonDocument::fromJson(defaults.readAll()).object()["desktop"].toObject().toVariantMap();
        daemon.start("dbus-daemon",{"--session","--nofork","--print-address=1","--nopidfile"});QVERIFY(daemon.waitForStarted(3000));QVERIFY(daemon.waitForReadyRead(3000));address=QString::fromUtf8(daemon.readLine()).trimmed();QVERIFY(!address.isEmpty());
        serverName="desktop-server-"+QUuid::createUuid().toString(QUuid::Id128);clientName="desktop-client-"+QUuid::createUuid().toString(QUuid::Id128);
        server=std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address,serverName));client=std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address,clientName));QVERIFY(server->isConnected());QVERIFY(client->isConnected());
        QVERIFY(server->registerService("org.freedesktop.portal.Desktop"));portal=std::make_unique<Portal>();
        QVERIFY(server->registerVirtualObject("/org/freedesktop/portal/desktop",portal.get(),QDBusConnection::SubPath));
    }
    void cleanup(){
        portal.reset();server.reset();client.reset();QDBusConnection::disconnectFromBus(serverName);QDBusConnection::disconnectFromBus(clientName);
        daemon.terminate();if(!daemon.waitForFinished(1000)){daemon.kill();daemon.waitForFinished(1000);}
    }
    void grantedShortcutReturnsExactOrigin(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);QSignalSpy returned(&mode,&DesktopMode::returned);
        mode.start("page.origin");QVERIFY(mode.busy());QVERIFY(!mode.active());QTRY_VERIFY(mode.active());QVERIFY(portal->validBind);QVERIFY(!mode.busy());QCOMPARE(mode.shortcutDescription(),QString("Home"));QCOMPARE(portal->registrations,1);
        portal->activate(*server,"/wrong/session");QTest::qWait(30);QVERIFY(mode.active());
        portal->activate(*server);QTRY_VERIFY(!mode.active());QCOMPARE(returned.count(),1);QCOMPARE(returned.first().first().toString(),QString("page.origin"));QTRY_VERIFY(!portal->closed.isEmpty());
    }
    void identityRegistrationCancelAndRetryRemainSingle(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);portal->registrationDelay=80;
        mode.start("old");QTest::qWait(20);mode.cancel();mode.start("new");QTRY_VERIFY(mode.active());QCOMPARE(portal->registrations,1);
        mode.returnHome();portal->objectPathResult=true;mode.start("again");QTRY_VERIFY(mode.active());QCOMPARE(portal->registrations,1);
    }
    void nativeReturnRequiresGrantAndPreservesOrigin(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);QSignalSpy returned(&mode,&DesktopMode::returned);
        mode.prepareNative("freetube");QVERIFY(mode.nativeMode());QVERIFY(!mode.active());QTRY_VERIFY(mode.active());
        portal->activate(*server,"/wrong/session");QTest::qWait(30);QVERIFY(mode.active());
        portal->activate(*server);QTRY_VERIFY(!mode.active());QCOMPARE(returned.count(),1);QCOMPARE(returned.first().first().toString(),QString("freetube"));
        portal->deny=true;mode.prepareNative("cbc-gem");QTRY_VERIFY(!mode.busy());QVERIFY(!mode.active());QVERIFY(!mode.errorString().isEmpty());
    }
    void identityRejectionNeverCreatesSession(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);portal->rejectRegistration=true;mode.start("origin");QTRY_VERIFY(!mode.busy());QVERIFY(!mode.active());QCOMPARE(portal->creates,0);QVERIFY(!mode.errorString().isEmpty());
    }
    void portalRestartReturnsAndReregisters(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);mode.start("before");QTRY_VERIFY(mode.active());
        QVERIFY(server->unregisterService("org.freedesktop.portal.Desktop"));QTRY_VERIFY(!mode.active());QVERIFY(!mode.errorString().isEmpty());
        portal->registered=false;QVERIFY(server->registerService("org.freedesktop.portal.Desktop"));QTest::qWait(20);
        mode.start("after");QTRY_VERIFY(mode.active());QCOMPARE(portal->registrations,2);
    }
    void deniedOrInvalidPermissionNeverHidesShell_data(){QTest::addColumn<int>("kind");QTest::newRow("denied")<<0;QTest::newRow("no-shortcut")<<1;QTest::newRow("foreign-session")<<2;}
    void deniedOrInvalidPermissionNeverHidesShell(){
        QFETCH(int,kind);portal->deny=kind==0;portal->empty=kind==1;portal->badSession=kind==2;
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);QSignalSpy returned(&mode,&DesktopMode::returned);mode.start("origin");QTRY_VERIFY(!mode.busy());QVERIFY(!mode.active());QVERIFY(!mode.errorString().isEmpty());QCOMPARE(returned.count(),1);
    }
    void cancelIgnoresLateGrant(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);portal->delay=80;mode.start("old-origin");QTest::qWait(30);mode.cancel();QVERIFY(!mode.busy());QVERIFY(!mode.active());
        portal->delay=0;mode.start("new-origin");QTRY_VERIFY(mode.active());QTest::qWait(160);QVERIFY(mode.active());mode.returnHome();QVERIFY(!mode.active());
    }
    void returnWindowBackWorksAndPortalClosureRecovers(){
        DesktopMode mode(nullptr,*client);mode.setPresentation(options);mode.start("button");QTRY_VERIFY(mode.active());
        InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
        QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(DESKTOP_RETURN_QML)));QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        std::unique_ptr<QObject> root(component.createWithInitialProperties({{"controller",QVariant::fromValue(&mode)},{"appearance",QVariantMap{{"surfaceColor","#28262f"},{"selectedColor","#353039"},{"textColor","#f5f1f3"},{"mutedColor","#aaa5af"},{"focusColor","#c994a5"}}}}));QVERIFY2(root,qPrintable(component.errorString()));
        auto *window=qobject_cast<QQuickWindow *>(root.get());QVERIFY(window);QTRY_VERIFY(window->isVisible());QVERIFY(!window->transientParent());
        auto *button=root->findChild<QQuickItem *>("desktopReturnButton");QVERIFY(button);QTRY_VERIFY(button->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!mode.active());QTRY_VERIFY(!window->isVisible());
        mode.start("again");QTRY_VERIFY(mode.active());auto closed=QDBusMessage::createSignal(portal->session,"org.freedesktop.portal.Session","Closed");server->send(closed);QTRY_VERIFY(!mode.active());QVERIFY(!mode.errorString().isEmpty());
    }
};
QTEST_MAIN(DesktopTests)
#include "tst_desktop_mode.moc"
