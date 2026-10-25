#include "settings/AppVpn.h"
#include "app/AppRegistry.h"
#include "sessions/SessionManager.h"
#include "input/InputManager.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>

static const QString Connection="12345678-1234-1234-1234-123456789abc";
static QByteArray read(const QString &path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return file.readAll();}
static void write(const QString &path,const QByteArray &bytes){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),bytes.size());}
static QString helper(const QString &root){
    const auto path=root+"/helper.py";
    write(path,R"py(import json,sys,time,signal
from pathlib import Path
state=json.loads(Path(__file__).with_suffix('.json').read_text())
if sys.argv[1]=='status':
 print(json.dumps({'connections':[{'uuid':'12345678-1234-1234-1234-123456789abc','name':'Canada','country':'CA','provider':'surfshark'}]}))
else:
 if state.get('delay'):
  signal.signal(signal.SIGTERM,lambda *_:sys.exit(1))
  time.sleep(10)
 print(json.dumps({'error':'Country verification failed'} if state.get('fail') else {'country':'CA','verifiedAt':1}))
 sys.exit(1 if state.get('fail') else 0)
)py");
    write(root+"/helper.json","{}");return path;
}
class FakeVpnSettings final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy CONSTANT)
    Q_PROPERTY(QString message READ message CONSTANT)
    Q_PROPERTY(QVariantList providers READ providers CONSTANT)
    Q_PROPERTY(QVariantList connections READ connections CONSTANT)
    Q_PROPERTY(QVariantList downloads READ downloads CONSTANT)
    Q_PROPERTY(QVariantList countries READ countries CONSTANT)
public:
    bool busy() const{return false;}
    QString message() const{return {};}
    QVariantList providers() const{return {QVariantMap{{"id","surfshark"},{"name","Surfshark"},{"installed",true}}};}
    QVariantList connections() const{return {};}
    QVariantList downloads() const{return {QVariantMap{{"name","canada.conf"}}};}
    QVariantList countries() const{return {QVariantMap{{"code","CA"},{"name","Canada"}}};}
    Q_INVOKABLE void perform(const QString &,const QString & = {},const QString & = {},const QString & = {}){}
};
class VpnTests final:public QObject {
    Q_OBJECT
private slots:
    void rulesAtomicProfileScopedAndInvalidRead(){
        QTemporaryDir dir;const auto script=helper(dir.path());
        AppVpn a(dir.path()+"/a.json","default",script),b(dir.path()+"/b.json","default",script);
        QVERIFY(a.refresh());QTRY_VERIFY(!a.busy());QCOMPARE(a.connections().size(),1);
        QVERIFY(a.setRoute("demo",Connection));QVERIFY(b.routes().isEmpty());
        const auto saved=read(dir.path()+"/a.json");
        QVERIFY(QDir().mkdir(dir.path()+"/a.json.bak"));
        QVERIFY(!a.setRoute("demo",""));QCOMPARE(read(dir.path()+"/a.json"),saved);
        QCOMPARE(a.routes().value("demo").toString(),Connection);
        write(dir.path()+"/invalid.json",R"({"schemaVersion":99,"routes":{"demo":"invalid"}})");
        const auto invalid=read(dir.path()+"/invalid.json");
        AppVpn broken(dir.path()+"/invalid.json","default",script);QSignalSpy failed(&broken,&AppVpn::failed);
        QVERIFY(!broken.prepare("demo"));QCOMPARE(failed.size(),1);
        QVERIFY(!broken.setRoute("demo",""));QCOMPARE(read(dir.path()+"/invalid.json"),invalid);
    }
    void verifiedLaunchFailureAndCancellation(){
        QTemporaryDir dir;const auto script=helper(dir.path());
        AppVpn vpn(dir.path()+"/rules.json","default",script);
        QVERIFY(vpn.refresh());QTRY_VERIFY(!vpn.busy());QVERIFY(vpn.setRoute("demo",Connection));
        AppRegistry registry;QVERIFY(registry.loadFromData(R"([{"id":"demo","name":"Demo","type":"mock","section":"Apps","accent":"#556677"}])"));
        SessionManager sessions(&registry);
        sessions.setLaunchPreparation([&](const AppDefinition &definition){return vpn.prepare(definition.id);});
        connect(&vpn,&AppVpn::ready,&sessions,[&](const QString &id){sessions.launch(id);});
        QSignalSpy failed(&vpn,&AppVpn::failed);
        QVERIFY(sessions.launch("demo"));QVERIFY(!sessions.hasActiveSession());
        QTRY_VERIFY(sessions.hasActiveSession());sessions.goHome();
        write(dir.path()+"/helper.json",R"({"fail":true})");
        QVERIFY(sessions.launch("demo"));QTRY_COMPARE(failed.size(),1);QVERIFY(!sessions.hasActiveSession());
        write(dir.path()+"/helper.json",R"({"delay":true})");
        QVERIFY(sessions.launch("demo"));QTRY_VERIFY(vpn.busy());QTest::qWait(100);vpn.cancel();
        QTRY_VERIFY(!vpn.busy());QVERIFY(!sessions.hasActiveSession());QCOMPARE(failed.size(),2);
    }
    void remoteAssignAndBackDoNotWrite(){
        QTemporaryDir dir;const auto script=helper(dir.path());const auto path=dir.path()+"/rules.json";
        AppVpn vpn(path,"default",script);QVERIFY(vpn.refresh());QTRY_VERIFY(!vpn.busy());
        AppRegistry registry;QVERIFY(registry.loadFromData(R"([{"id":"demo","name":"Demo","type":"mock","section":"Apps","accent":"#556677"}])"));
        InputManager input;FakeVpnSettings settings;QQmlEngine engine;
        engine.rootContext()->setContextProperty("vpnSettings",&settings);
        engine.rootContext()->setContextProperty("appVpn",&vpn);
        engine.rootContext()->setContextProperty("appRegistry",&registry);
        engine.rootContext()->setContextProperty("inputManager",&input);
        QQmlComponent component(&engine,QUrl::fromLocalFile(VPN_QML));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root,qPrintable(component.errorString()));
        auto activate=[&](int index){QVERIFY(QMetaObject::invokeMethod(root.data(),"activate",Q_ARG(QVariant,index)));};
        activate(1);QCOMPARE(root->property("mode").toString(),"apps");
        activate(0);QCOMPARE(root->property("mode").toString(),"locations");
        QVERIFY(QMetaObject::invokeMethod(root.data(),"handleCommand",Q_ARG(QVariant,QVariant(input.back()))));
        QVERIFY(!QFileInfo::exists(path));
        activate(1);activate(0);activate(1);
        QCOMPARE(vpn.routes().value("demo").toString(),Connection);
        const auto bytes=read(path);
        root->setProperty("mode","main");
        root->setProperty("advanced",true);
        const auto rows=root->property("rows").toList();int download=-1;
        for(int i=0;i<rows.size();++i)if(rows[i].toMap().value("action")=="import")download=i;
        QVERIFY(download>=0);activate(download);activate(0);activate(0);
        QCOMPARE(root->property("mode").toString(),"import");
        QVERIFY(QMetaObject::invokeMethod(root.data(),"handleCommand",Q_ARG(QVariant,QVariant(input.back()))));
        QCOMPARE(read(path),bytes);
    }
};
QTEST_MAIN(VpnTests)
#include "tst_vpn.moc"
