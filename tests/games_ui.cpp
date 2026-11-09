#include "games/GameLibrary.h"
#include "games/GameEmulators.h"
#include "games/OpticalDiscAdapter.h"
#include "games/OpticalDiscGrants.h"
#include "games/ManagedGamePaths.h"
#include "games/CompatibilityRuntime.h"
#include <QStandardPaths>
#include <QUuid>
#include "app/AppRegistry.h"
#include "input/InputManager.h"
#include "media/MediaGrants.h"
#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QJSValue>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QFile>
#include <QScopeGuard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QBuffer>
#include <QCryptographicHash>

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
    void remoteSteamImportCanCancelReviewAndPlay();
    void remoteExternalSteamAccessIsExplicitAndCancelable();
    void mountedOpticalDiscoveryUsesTypedReadOnlyObservations();
    void opticalRevalidationClassifiesReadyReplacementAndRemoval();
    void opticalIdentityAndMultiDiscReferencesAreDurable();
    void fullOpticalContentIdentityRejectsTruncationAndChanges();
    void opticalSessionContractNeverClaimsHotSwap();
    void opticalApprovalsAreProfileScopedAndRevocable();
    void readOnlyOpticalMountIsBoundedAndVerifiedByCaller();
    void ejectRefusesWhileSessionIsActive();
    void catalogRecoversStagedUninstallOnReopen();
    void orphanedUninstallRecoveryPreservesRemainingFiles();
    void compatibilityCandidatesNeverClaimInstallerSupport();
    void safeEjectRefusesBusyUnmountAndNeverForces();
    void remoteOpticalInspectionNeverStartsMedia();
    void remoteDetailsFavoriteRemoveAndCancel();
    void remoteAddReviewWritesOnlyOnConfirm();
    void remoteEditCanCancelAndApplyWithoutMouse();
    void remoteEditionChooserDispatchesOwnedVersion();
    void remoteEmulatorSetupCancelsAndSavesWithoutMouse();
    void platformShelvesFilterCanonicalGamesRemotely();
    void fiveThousandGameCatalogStaysPagedAndSearchable();
    void catalogCapDisablesNextFromLastFullPage();
    void organizationAndManualSmartCollectionsStayProfileLocal();
    void remoteSmartCollectionRulesAreDpadSelectable();
    void catalogExportRestoreIsBoundedAndTransactional();
    void catalogProvenanceIsImmutableAndConflictSafe();
};
static AppDefinition runtime() {
    AppDefinition app;app.id="local-test";app.name="Local game";app.type="desktop";
    app.presentationMode="game";app.section="Games";app.accent=QColor("#556677");
    app.launchConfiguration={{"desktopId","isolde-test-game"}};return app;
}
void GamesUiTests::opticalSessionContractNeverClaimsHotSwap(){
    const QVariantMap disc{{"mediaId",QString(64,'a')},{"mediaSetId",QString(64,'b')},
        {"discNumber",1},{"discCount",2},{"mountRoot","/media/first"},{"state","mounted"},{"observationId",QString(64,'e')}};
    const auto reference=OpticalDiscAdapter::rootReference(disc);
    const auto pinned=OpticalDiscAdapter::classifySession(reference,"/media/first",true,{disc});
    QCOMPARE(pinned.value("state").toString(),QString("pinned"));
    QVERIFY(!pinned.value("hotSwapSupported").toBool());
    auto reinserted=disc;reinserted["observationId"]=QString(64,'f');
    QCOMPARE(OpticalDiscAdapter::classifySession(reference,"/media/first",true,{reinserted},QString(64,'e')).value("state").toString(),QString("interrupted"));
    auto other=disc;other["mediaId"]=QString(64,'c');other["discNumber"]=2;
    QCOMPARE(OpticalDiscAdapter::classifySession(reference,"/media/first",true,{other}).value("state").toString(),QString("wrong-disc"));
    auto relocated=disc;relocated["mountRoot"]="/media/second";
    QCOMPARE(OpticalDiscAdapter::classifySession(reference,"/media/first",true,{relocated}).value("state").toString(),QString("relocated"));
    QCOMPARE(OpticalDiscAdapter::classifySession(reference,"/media/first",true,{}).value("state").toString(),QString("missing"));
    other["mediaSetId"]=QString(64,'d');
    QCOMPARE(OpticalDiscAdapter::classifySession(reference,"/media/first",true,{other}).value("state").toString(),QString("replaced"));
    QCOMPARE(OpticalDiscAdapter::classifySession(reference,"/media/first",false,{disc}).value("state").toString(),QString("revoked"));
    const auto missing=OpticalDiscAdapter::classifySession(reference,"/media/first",true,{});
    QVERIFY(!missing.value("recovery").toString().isEmpty());
    const auto interrupted=OpticalDiscAdapter::latchSession(missing,pinned);
    QCOMPARE(interrupted.value("state").toString(),QString("interrupted"));
    QCOMPARE(OpticalDiscAdapter::latchSession(interrupted,pinned).value("state").toString(),QString("interrupted"));
    QCOMPARE(OpticalDiscAdapter::latchSession({},pinned).value("state").toString(),QString("pinned"));
}
void GamesUiTests::compatibilityCandidatesNeverClaimInstallerSupport(){
    const auto missing=CompatibilityRuntime::classify(false,false);
    const auto untrusted=CompatibilityRuntime::classify(true,false);
    const auto candidate=CompatibilityRuntime::classify(true,true);
    QCOMPARE(missing.value("state").toString(),QString("missing"));
    QCOMPARE(untrusted.value("state").toString(),QString("unsafe"));
    QCOMPARE(candidate.value("state").toString(),QString("unverified"));
    for(const auto &status:{missing,untrusted,candidate}){
        QVERIFY(!status.value("supported").toBool());
        QVERIFY(status.value("requirements").toString().contains("verified"));
    }
}
void GamesUiTests::orphanedUninstallRecoveryPreservesRemainingFiles(){
    QStandardPaths::setTestModeEnabled(true);
    const auto previous=qApp->property("isoldeProfile");
    const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    qApp->setProperty("isoldeProfile",profile);
    const auto identity=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto parent=ManagedGamePaths::gamesRoot(profile)+"/"+identity;
    auto cleanup=qScopeGuard([&]{QDir(parent).removeRecursively();qApp->setProperty("isoldeProfile",previous);QStandardPaths::setTestModeEnabled(false);});
    const auto root=parent+"/install",pending=root+".uninstall-pending";
    QVERIFY(QDir().mkpath(pending));
    QFile payload(pending+"/remaining-save");QVERIFY(payload.open(QIODevice::WriteOnly));payload.write("keep");payload.close();
    QTemporaryDir dbdir;AppRegistry apps;GameLibrary library(dbdir.filePath("games.sqlite"),&apps);QVERIFY(library.ready());
    QCOMPARE(library.pendingUninstallRecovery().size(),1);
    QVERIFY(!library.recoverOrphanedUninstall(identity,false));
    QVERIFY(QDir().mkpath(root));QVERIFY(!library.recoverOrphanedUninstall(identity,true));
    QVERIFY(QDir().rmdir(root));
    QVERIFY(library.recoverOrphanedUninstall(identity,true));
    QFile restored(root+"/remaining-save");QVERIFY(restored.open(QIODevice::ReadOnly));QCOMPARE(restored.readAll(),QByteArray("keep"));
    QVERIFY(library.pendingUninstallRecovery().isEmpty());
}
void GamesUiTests::fullOpticalContentIdentityRejectsTruncationAndChanges(){
    QByteArray bytes(3*1024*1024,'x');
    QBuffer source(&bytes);QVERIFY(source.open(QIODevice::ReadOnly));
    const auto digest=OpticalDiscAdapter::hashContent(&source,bytes.size());
    QCOMPARE(digest,QCryptographicHash::hash(bytes,QCryptographicHash::Sha256));
    bytes[bytes.size()-1]='y';QVERIFY(source.seek(0));
    const auto changed=OpticalDiscAdapter::hashContent(&source,bytes.size());
    QVERIFY(changed!=digest);
    QVERIFY(source.seek(0));QVERIFY(OpticalDiscAdapter::hashContent(&source,bytes.size()+1).isEmpty());
    QVERIFY(source.seek(0));QVERIFY(OpticalDiscAdapter::hashContent(&source,0).isEmpty());
    QVERIFY(OpticalDiscAdapter::hashContent(&source,16ULL*1024*1024*1024+1).isEmpty());
    const auto id=QString::fromLatin1(digest.toHex());
    const auto udf=OpticalDiscAdapter::identityForContent({},id);
    QCOMPARE(udf.value("discNumber").toInt(),1);QCOMPARE(udf.value("discCount").toInt(),1);
    QCOMPARE(udf.value("mediaSetId").toString(),id);
    QVERIFY(OpticalDiscAdapter::isRootReference(OpticalDiscAdapter::rootReference(udf)));
    QVERIFY(OpticalDiscAdapter::identityForContent({},"bad").isEmpty());
    const QVariantMap set{{"mediaSetId",QString(64,'a')},{"discNumber",2},{"discCount",2}};
    const auto iso=OpticalDiscAdapter::identityForContent(set,id);
    QCOMPARE(iso.value("mediaId").toString(),id);
    QCOMPARE(iso.value("mediaSetId").toString(),QString(64,'a'));
    QCOMPARE(iso.value("discNumber").toInt(),2);
}
void GamesUiTests::catalogRecoversStagedUninstallOnReopen(){
    QStandardPaths::setTestModeEnabled(true);
    const auto previous=qApp->property("isoldeProfile");
    const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    qApp->setProperty("isoldeProfile",profile);
    const auto parent=ManagedGamePaths::gamesRoot(profile)+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto cleanup=qScopeGuard([&]{QDir(parent).removeRecursively();qApp->setProperty("isoldeProfile",previous);QStandardPaths::setTestModeEnabled(false);});
    const auto root=parent+"/install",pending=root+".uninstall-pending";
    QVERIFY(QDir().mkpath(pending));
    QFile marker(pending+"/.isolde-install.json");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("{}");marker.close();
    QFile payload(pending+"/game");QVERIFY(payload.open(QIODevice::WriteOnly));payload.write("keep");payload.close();
    QTemporaryDir dbdir;AppRegistry apps;const auto path=dbdir.filePath("games.sqlite");
    {GameLibrary initial(path,&apps);QVERIFY(initial.ready());}
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE","uninstall-recovery-test");db.setDatabaseName(path);QVERIFY(db.open());
        QSqlQuery query(db);query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,added) VALUES(?,?,?,?,?,?,0)");
        for(const auto &value:QStringList{"game-test","Fixture","PC","local-test",root,"game"})query.addBindValue(value);
        QVERIFY(query.exec());db.close();
    }
    QSqlDatabase::removeDatabase("uninstall-recovery-test");
    {GameLibrary reopened(path,&apps);QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));}
    QVERIFY(QFileInfo::exists(root+"/game"));QVERIFY(!QFileInfo::exists(pending));
    QFile restored(root+"/game");QVERIFY(restored.open(QIODevice::ReadOnly));QCOMPARE(restored.readAll(),QByteArray("keep"));
}
void GamesUiTests::opticalRevalidationClassifiesReadyReplacementAndRemoval(){
    const QVariantMap old{{"schemaVersion",1},{"observationId",QString(64,'a')},{"device","/dev/sr0"},{"mountRoot","/media/disc-a"},{"state","mounted"}};
    const QVariantMap replacement{{"schemaVersion",1},{"observationId",QString(64,'b')},{"device","/dev/sr0"},{"mountRoot","/media/disc-b"},{"state","mounted"}};
    QCOMPARE(OpticalDiscAdapter::classifyRevalidation(old.value("observationId").toString(),"/dev/sr0",{old}).value("state").toString(),QString("ready"));
    const auto changed=OpticalDiscAdapter::classifyRevalidation(old.value("observationId").toString(),"/dev/sr0",{replacement});
    QCOMPARE(changed.value("state").toString(),QString("replaced"));
    QCOMPARE(changed.value("disc").toMap().value("observationId").toString(),replacement.value("observationId").toString());
    QCOMPARE(OpticalDiscAdapter::classifyRevalidation(old.value("observationId").toString(),"/dev/sr0",{}).value("state").toString(),QString("missing"));
}
void GamesUiTests::opticalIdentityAndMultiDiscReferencesAreDurable(){
    auto descriptor=QByteArray(2048,'\0');
    descriptor[0]=1;descriptor.replace(1,5,"CD001");descriptor[6]=1;
    descriptor.replace(40,4,"DISC");
    descriptor[80]=100;descriptor[120]=2;descriptor[124]=1;descriptor[128]=0;descriptor[129]=8;
    descriptor.replace(190,8,"SET-1234");
    const auto first=OpticalDiscAdapter::parseIsoVolumeDescriptor(descriptor);
    QVERIFY(!first.isEmpty());QCOMPARE(first.value("discNumber").toInt(),1);QCOMPARE(first.value("discCount").toInt(),2);
    const auto root=OpticalDiscAdapter::rootReference(first);
    QVERIFY(OpticalDiscAdapter::isRootReference(root));
    QCOMPARE(OpticalDiscAdapter::mediaIdFromRoot(root),first.value("mediaId").toString());
    auto secondDescriptor=descriptor;secondDescriptor[124]=2;
    const auto second=OpticalDiscAdapter::parseIsoVolumeDescriptor(secondDescriptor);
    QCOMPARE(first.value("mediaSetId").toString(),second.value("mediaSetId").toString());
    QVERIFY(first.value("mediaId").toString()!=second.value("mediaId").toString());
    const auto secondRoot=OpticalDiscAdapter::rootReference(second);
    auto ready=first;ready["state"]="mounted";ready["mountRoot"]="/media/test-disc-1";
    QCOMPARE(OpticalDiscAdapter::classifyRootReference(root,{ready}).value("state").toString(),QString("ready"));
    QCOMPARE(OpticalDiscAdapter::classifyRootReference(root,{}).value("state").toString(),QString("missing"));
    const QVariantMap wrong{{"mediaId",second.value("mediaId")},{"mediaSetId",second.value("mediaSetId")},
        {"discNumber",2},{"discCount",2},{"mountRoot","/media/test-disc-2"},{"state","mounted"}};
    const auto state=OpticalDiscAdapter::classifyRootReference(root,{wrong});
    QCOMPARE(state.value("state").toString(),QString("wrong-disc"));
    QCOMPARE(state.value("expectedDisc").toInt(),1);
    QCOMPARE(OpticalDiscAdapter::classifyRootReference(root,{wrong,ready}).value("state").toString(),QString("ready"));
    auto inconsistent=ready;inconsistent["discNumber"]=2;
    QCOMPARE(OpticalDiscAdapter::classifyRootReference(root,{inconsistent}).value("state").toString(),QString("wrong-disc"));
    auto unmounted=wrong;unmounted["observationId"]=QString(64,'c');unmounted["state"]="unmounted";unmounted["mountRoot"]="";
    QCOMPARE(OpticalDiscAdapter::classifyRootReference(root,{unmounted}).value("state").toString(),QString("missing"));
    QCOMPARE(OpticalDiscAdapter::classifyRevalidation(QString(64,'a'),"/dev/sr0",{unmounted}).value("state").toString(),QString("replaced"));
    const auto selectedUnmounted=OpticalDiscAdapter::classifyRevalidation(
        unmounted.value("observationId").toString(),"/dev/sr0",{unmounted});
    QCOMPARE(selectedUnmounted.value("state").toString(),QString("unmounted"));
    auto noSet=descriptor;noSet.fill('\0');noSet[0]=1;noSet.replace(1,5,"CD001");noSet[6]=1;
    noSet.replace(40,4,"DISC");noSet[80]=100;noSet[120]=2;noSet[124]=1;noSet[128]=0;noSet[129]=8;
    QVERIFY(OpticalDiscAdapter::parseIsoVolumeDescriptor(noSet).isEmpty());
    QVERIFY(!OpticalDiscAdapter::parseIsoVolumeDescriptor(QByteArray(2048,'x')).size());
    QVERIFY(!OpticalDiscAdapter::isRootReference(secondRoot+"/../bad"));
}
void GamesUiTests::opticalApprovalsAreProfileScopedAndRevocable(){
    QStandardPaths::setTestModeEnabled(true);
    const auto alice=QUuid::createUuid().toString(QUuid::WithoutBraces),bob=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto identity=QString(64,'a');
    QVERIFY(OpticalDiscGrants::authorize(alice,identity).isEmpty());
    QVERIFY(OpticalDiscGrants::approved(alice,identity));QVERIFY(!OpticalDiscGrants::approved(bob,identity));
    QVERIFY(OpticalDiscGrants::revoke(alice,identity).isEmpty());QVERIFY(!OpticalDiscGrants::approved(alice,identity));
    QVERIFY(!OpticalDiscGrants::authorize(alice,"not-an-identity").isEmpty());
}
void GamesUiTests::ejectRefusesWhileSessionIsActive(){
    QTemporaryDir dir;AppRegistry apps;GameLibrary library(dir.filePath("games.sqlite"),&apps);
    QVERIFY(library.ready());
    bool active=true;
    library.setSessionActiveGuard([&active]{return active;});
    QVERIFY(!library.ejectOpticalDisc("/dev/sr0"));
    QVERIFY(library.errorString().contains("session is active"));
    QVERIFY(!library.uninstallManagedGame("game-test",true));
    QVERIFY(library.errorString().contains("session is active"));
    active=false;
    QVERIFY(!library.ejectOpticalDisc("/dev/sr9999"));
    QCOMPARE(library.errorString(),QString("Invalid optical device."));
}
void GamesUiTests::safeEjectRefusesBusyUnmountAndNeverForces(){
    QStringList calls;
    const OpticalDiscAdapter::DbusCall busy=[&calls](const QString &service,const QString &path,
        const QString &interface,const QString &method,const QVariantList &){
        calls.append(method);return QDBusMessage::createMethodCall(service,path,interface,method)
            .createErrorReply("org.freedesktop.DBus.Error.Busy","busy");
    };
    QString error;
    QVERIFY(!OpticalDiscAdapter::ejectUsing("/dev/sr0",true,busy,&error));
    QCOMPARE(calls,QStringList{"Unmount"});
    QVERIFY(error.contains("in use"));
    calls.clear();
    const OpticalDiscAdapter::DbusCall success=[&calls](const QString &service,const QString &path,
        const QString &interface,const QString &method,const QVariantList &){
        calls.append(method);
        const auto request=QDBusMessage::createMethodCall(service,path,interface,method);
        if(method=="Get")return request.createReply({QVariant::fromValue(
            QDBusVariant(QVariant::fromValue(QDBusObjectPath("/org/freedesktop/UDisks2/drives/drive0"))))});
        return request.createReply();
    };
    QVERIFY(OpticalDiscAdapter::ejectUsing("/dev/sr0",true,success,&error));
    QCOMPARE(calls,QStringList({"Unmount","Get","Eject"}));
    QVERIFY(!calls.contains("ForceEject"));
    QVERIFY(!OpticalDiscAdapter::ejectUsing("/dev/sr9999",false,success,&error));
    QCOMPARE(error,QString("Invalid optical device."));
}
void GamesUiTests::readOnlyOpticalMountIsBoundedAndVerifiedByCaller(){
    int calls=0;QString error,mountPoint,serviceSeen,pathSeen,interfaceSeen,methodSeen;QVariantList argumentsSeen;
    const OpticalDiscAdapter::DbusCall mount=[&](const QString &service,const QString &path,
        const QString &interface,const QString &method,const QVariantList &arguments){
        ++calls;serviceSeen=service;pathSeen=path;interfaceSeen=interface;
        methodSeen=method;argumentsSeen=arguments;
        return QDBusMessage::createMethodCall(service,path,interface,method).createReply({"/media/disc"});
    };
    QVERIFY(OpticalDiscAdapter::mountUsing("/dev/sr0",true,mount,&mountPoint,&error));
    QCOMPARE(calls,1);QCOMPARE(mountPoint,QString("/media/disc"));QVERIFY(error.isEmpty());
    QCOMPARE(serviceSeen,QString("org.freedesktop.UDisks2"));
    QCOMPARE(pathSeen,QString("/org/freedesktop/UDisks2/block_devices/sr0"));
    QCOMPARE(interfaceSeen,QString("org.freedesktop.UDisks2.Filesystem"));
    QCOMPARE(methodSeen,QString("Mount"));
    QCOMPARE(argumentsSeen.size(),1);
    QCOMPARE(argumentsSeen.first().toMap().value("options").toString(),QString("ro"));
    QVERIFY(!OpticalDiscAdapter::mountUsing("/dev/sr0",false,mount,&mountPoint,&error));
    QCOMPARE(calls,1);QVERIFY(error.contains("No unmounted optical media"));
    QVERIFY(!OpticalDiscAdapter::mountUsing("/dev/sr9999",true,mount,&mountPoint,&error));
    QCOMPARE(calls,1);QCOMPARE(error,QString("Invalid optical device."));
    const OpticalDiscAdapter::DbusCall failed=[](const QString &service,const QString &path,
        const QString &interface,const QString &method,const QVariantList &){
        return QDBusMessage::createMethodCall(service,path,interface,method)
            .createErrorReply("org.freedesktop.DBus.Error.Failed","unreadable");
    };
    QVERIFY(!OpticalDiscAdapter::mountUsing("/dev/sr0",true,failed,&mountPoint,&error));
    QVERIFY(error.contains("could not be mounted read-only"));
    const OpticalDiscAdapter::DbusCall invalidPoint=[](const QString &service,const QString &path,
        const QString &interface,const QString &method,const QVariantList &){
        return QDBusMessage::createMethodCall(service,path,interface,method).createReply({"relative"});
    };
    QVERIFY(!OpticalDiscAdapter::mountUsing("/dev/sr0",true,invalidPoint,&mountPoint,&error));
    QVERIFY(error.contains("invalid mountpoint"));
}
static void command(QObject *widget,int value) {
    QVariant result;QVERIFY(QMetaObject::invokeMethod(widget,"handleCommand",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,value)));
}
static void doneOnKeyboard(QObject *widget) {
    for(int i=0;i<5;++i)command(widget,InputManager::NavigateDown);
    for(int i=0;i<9;++i)command(widget,InputManager::NavigateRight);
    command(widget,InputManager::Confirm);
}
void GamesUiTests::remoteExternalSteamAccessIsExplicitAndCancelable(){
    QStandardPaths::setTestModeEnabled(true);const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);qApp->setProperty("isoldeProfile",profile);
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("games.sqlite"),&apps);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    for(int i=0;i<12;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("catalogTools"));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::NavigateDown);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("steamLibraries"));
    QVERIFY(library.steamLibraries().isEmpty());
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("keyboard"));
    QVERIFY(library.steamLibraries().isEmpty());
    command(widget.data(),InputManager::Back);QCOMPARE(widget->property("mode").toString(),QString("steamLibraries"));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("catalogTools"));QVERIFY(library.steamLibraries().isEmpty());
    qApp->setProperty("isoldeProfile",QVariant());
}
void GamesUiTests::mountedOpticalDiscoveryUsesTypedReadOnlyObservations(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("games.sqlite"),&apps);
    const auto discs=library.mountedOpticalDiscs();QVERIFY(discs.size()<=4);
    for(const auto &value:discs){
        const auto disc=value.toMap();
        QCOMPARE(disc.value("schemaVersion").toInt(),1);
        QCOMPARE(disc.value("sourceType").toString(),QString("pc-optical-disc"));
        QVERIFY(QRegularExpression("^[0-9a-f]{64}$").match(disc.value("observationId").toString()).hasMatch());
        QVERIFY(disc.value("device").toString().startsWith("/dev/sr"));
        if(disc.value("state")=="mounted")QVERIFY(disc.value("mountRoot").toString().startsWith("/"));
        else{QCOMPARE(disc.value("state").toString(),QString("unmounted"));QVERIFY(disc.value("mountRoot").toString().isEmpty());}
        QVERIFY(disc.value("readOnly").toBool());
    }
}
void GamesUiTests::remoteOpticalInspectionNeverStartsMedia(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("games.sqlite"),&apps);
    InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    for(int i=0;i<12;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("catalogTools"));
    for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateDown);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("opticalChooser"));
    QVERIFY(dispatch.lastAction.isEmpty());
    const auto discs=library.mountedOpticalDiscs();
    if(discs.isEmpty()){
        command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("opticalChooser"));
    }else{
        command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("opticalDetails"));
        for(int i=0;i<5;++i)command(widget.data(),InputManager::NavigateDown);
        command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("opticalChooser"));
    }
    QVERIFY(dispatch.lastAction.isEmpty());
}
void GamesUiTests::remoteSteamImportCanCancelReviewAndPlay(){
    QStandardPaths::setTestModeEnabled(true);const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);qApp->setProperty("isoldeProfile",profile);
    const auto parent=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+profile;
    const auto path=parent+"/steam/home/.local/share/Steam/steamapps";QVERIFY(QDir().mkpath(path+"/common/Fixture"));
    QFile manifest(path+"/appmanifest_42.acf");QVERIFY(manifest.open(QIODevice::WriteOnly));manifest.write("\"AppState\" { \"appid\" \"42\" \"name\" \"Steam fixture\" \"StateFlags\" \"4\" \"installdir\" \"Fixture\" }");manifest.close();
    QTemporaryDir dir;AppRegistry apps;auto steam=runtime();steam.id="steam";steam.name="Steam";steam.presentationMode="game-hub";steam.launchConfiguration={{"desktopId","isolde-steam"}};QVERIFY(apps.addApp(steam));
    GameLibrary library(dir.filePath("games.sqlite"),&apps);InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"originId","steam-origin"},{"width",1000},{"height",600},{"engaged",true}}));QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QTRY_COMPARE_WITH_TIMEOUT(widget->property("mode").toString(),QString("importPreview"),10000);QVERIFY(library.page().isEmpty());
    command(widget.data(),InputManager::Confirm);QVERIFY(library.page().isEmpty()); // Default Cancel.
    QVERIFY(manifest.open(QIODevice::ReadOnly));const auto validManifest=manifest.readAll();manifest.close();
    QVERIFY(manifest.open(QIODevice::WriteOnly|QIODevice::Truncate));manifest.write("invalid Steam metadata");manifest.close();
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QTRY_COMPARE_WITH_TIMEOUT(widget->property("mode").toString(),QString("browse"),10000);
    QVERIFY(widget->property("message").toString().contains("Steam discovery failed"));QVERIFY(library.page().isEmpty());
    QVERIFY(manifest.open(QIODevice::WriteOnly|QIODevice::Truncate));manifest.write(validManifest);manifest.close();
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QTRY_COMPARE_WITH_TIMEOUT(widget->property("mode").toString(),QString("importPreview"),10000);
    command(widget.data(),InputManager::NavigateUp);command(widget.data(),InputManager::Confirm);QCOMPARE(library.page().size(),1);
    const auto steamGame=library.page().first().toMap().value("id").toString();
    QCOMPARE(library.details(steamGame).value("sourceKind").toString(),QString("steam-app"));
    QCOMPARE(library.details(steamGame).value("sourceReference").toString(),QString("42"));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QCOMPARE(dispatch.lastAction.value("type").toString(),QString("launch-game"));QCOMPARE(dispatch.origin,QString("steam-origin"));
    QCOMPARE(library.resolveLaunch(dispatch.lastAction.value("gameId").toString()).value("steamAppId").toString(),QString("42"));
    qApp->setProperty("isoldeProfile",QVariant());QVERIFY(QDir(parent).removeRecursively());
}
void GamesUiTests::remoteEmulatorSetupCancelsAndSavesWithoutMouse(){
    QStandardPaths::setTestModeEnabled(true);const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);
    qApp->setProperty("isoldeProfile",profile);
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("games.sqlite"),&apps);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    for(int i=0;i<8;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("emulators"));
    QVERIFY(library.emulatorCoverage().value("integratedPlatforms").toList().contains("PSP"));
    QVERIFY(library.emulatorCoverage().value("notIntegrated").toList().contains("Xbox 360"));
    QVERIFY(widget->property("message").toString().contains("Xbox 360"));
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("emulatorSettings"));
    const auto file=GameEmulators::home(profile,"ppsspp")+"/.config/isolde-emulator/settings.json";
    command(widget.data(),InputManager::Confirm);QVERIFY(!QFileInfo::exists(file)); // default Cancel
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    for(int i=0;i<4;++i)command(widget.data(),InputManager::NavigateUp);
    command(widget.data(),InputManager::Confirm); // automatic -> software, draft only
    QVERIFY(!QFileInfo::exists(file));
    for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateDown);
    command(widget.data(),InputManager::Confirm);QCOMPARE(library.emulatorSettings("ppsspp").value("video").toString(),QString("software"));
    QVERIFY(!library.installEmulators(false));QVERIFY(!library.setupBusy());
    QVERIFY(QDir(QFileInfo(GameEmulators::home(profile,"ppsspp")).absolutePath()).removeRecursively());
    qApp->setProperty("isoldeProfile",QVariant());
}
void GamesUiTests::platformShelvesFilterCanonicalGamesRemotely(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);QVERIFY(library.ready());
    const auto add=[&library](const QString &title,const QString &platform){
        return library.addGame({{"schemaVersion",1},{"title",title},{"platform",platform},
            {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    };
    const auto pc=add("Desk fixture","PC"),psp=add("Handheld fixture","PSP");
    QVERIFY(!pc.isEmpty());QVERIFY(!psp.isEmpty());
    QCOMPARE(library.details(pc).value("sourceKind").toString(),QString("runtime-entry"));
    QVERIFY(library.setFavorite(psp,true));
    QCOMPARE(library.platforms().size(),2);
    QCOMPARE(library.page("platform",{},0,"psp").size(),1);
    QCOMPARE(library.page("platform",{},0,"PSP").first().toMap().value("id").toString(),psp);
    QCOMPARE(library.page("favorites").size(),1);
    QVERIFY(library.page("platform",{},0,QString()).isEmpty());
    QVERIFY(library.page("continue").isEmpty());
    QVERIFY(!library.recordCompletedSession("missing-game"));
    QVERIFY(library.recordCompletedSession(psp));
    QCOMPARE(library.page("continue").size(),1);
    GameLibrary reopened(dir.filePath("profile/games.sqlite"),&apps);
    QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.page("continue").size(),1); // v3 activity without an editions table

    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({
        {"library",QVariant::fromValue(&library)},{"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY2(widget,qPrintable(component.errorString()));
    for(int i=0;i<4;++i)command(widget.data(),InputManager::NavigateRight); // Platforms
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("platformChooser"));
    command(widget.data(),InputManager::NavigateDown);
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("browse"));
    QCOMPARE(widget->property("queryView").toString(),QString("platform"));
    QCOMPARE(widget->property("selectedPlatform").toString(),QString("PSP"));
    const auto items=widget->property("items");
    const auto listed=items.metaType()==QMetaType::fromType<QJSValue>()?items.value<QJSValue>().toVariant().toList():items.toList();
    QCOMPARE(listed.size(),1);QCOMPARE(listed.first().toMap().value("id").toString(),psp);

    QScopedPointer<QObject> continueWidget(component.createWithInitialProperties({
        {"library",QVariant::fromValue(&library)},{"style",QVariantMap{{"libraryView","continue"}}},
        {"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY2(continueWidget,qPrintable(component.errorString()));
    QCOMPARE(continueWidget->property("queryView").toString(),QString("continue"));
    const auto continued=continueWidget->property("items");
    const auto continueItems=continued.metaType()==QMetaType::fromType<QJSValue>()?continued.value<QJSValue>().toVariant().toList():continued.toList();
    QCOMPARE(continueItems.size(),1);QCOMPARE(continueItems.first().toMap().value("id").toString(),psp);
}
void GamesUiTests::catalogCapDisablesNextFromLastFullPage(){
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto path=dir.filePath("catalog-cap.sqlite");
    const auto connection="games-cap-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto seed=QSqlDatabase::addDatabase("QSQLITE",connection);seed.setDatabaseName(path);QVERIFY(seed.open());
    QSqlQuery query(seed);
    QVERIFY(query.exec("CREATE TABLE games(id TEXT PRIMARY KEY,title TEXT NOT NULL,platform TEXT NOT NULL,runtime TEXT NOT NULL,root TEXT NOT NULL,target TEXT NOT NULL,favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)),added INTEGER NOT NULL,UNIQUE(runtime,root,target))"));
    QVERIFY(query.exec("CREATE INDEX games_title ON games(title COLLATE NOCASE)"));
    QVERIFY(query.exec("PRAGMA user_version=1"));
    QVERIFY(query.exec("WITH RECURSIVE n(x) AS (VALUES(0) UNION ALL SELECT x+1 FROM n WHERE x<49999) "
        "INSERT INTO games(id,title,platform,runtime,root,target,added) "
        "SELECT printf('game-%05d',x),printf('Fixture %05d',x),CASE WHEN x<50 THEN 'Small' ELSE 'PC' END,printf('runtime-%05d',x),'','',x FROM n"));
    query=QSqlQuery();seed.close();seed=QSqlDatabase();QSqlDatabase::removeDatabase(connection);
    AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(path,&apps);QVERIFY2(library.ready(),qPrintable(library.errorString()));
    QCOMPARE(library.page("all",{},49950).size(),50);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({
        {"library",QVariant::fromValue(&library)},{"width",1000},{"height",600},{"engaged",true}}));
    QVERIFY2(widget,qPrintable(component.errorString()));
    QCOMPARE(widget->property("catalogLimit").toInt(),50000);
    QVERIFY(widget->property("canNextPage").toBool());
    auto labelsValue=widget->property("tools");
    auto labels=labelsValue.metaType()==QMetaType::fromType<QJSValue>()?labelsValue.value<QJSValue>().toVariant().toList():labelsValue.toList();
    QCOMPARE(labels.at(7).toString(),QString("Next"));
    widget->setProperty("toolbar",7);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("offset").toInt(),50);
    QVERIFY(widget->setProperty("offset",49950));
    QVERIFY(QMetaObject::invokeMethod(widget.data(),"reload"));
    QCOMPARE(widget->property("canNextPage").toBool(),false);
    labelsValue=widget->property("tools");
    labels=labelsValue.metaType()==QMetaType::fromType<QJSValue>()?labelsValue.value<QJSValue>().toVariant().toList():labelsValue.toList();
    QCOMPARE(labels.at(7).toString(),QString("End"));
    auto listed=widget->property("items");
    auto page=listed.metaType()==QMetaType::fromType<QJSValue>()?listed.value<QJSValue>().toVariant().toList():listed.toList();
    QCOMPARE(page.size(),50);
    widget->setProperty("toolbar",7);
    command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("offset").toInt(),49950);
    listed=widget->property("items");
    page=listed.metaType()==QMetaType::fromType<QJSValue>()?listed.value<QJSValue>().toVariant().toList():listed.toList();
    QCOMPARE(page.size(),50);
    QVERIFY(library.errorString().isEmpty());
    widget->setProperty("selectedPlatform","Small");
    widget->setProperty("queryView","platform");
    listed=widget->property("items");
    page=listed.metaType()==QMetaType::fromType<QJSValue>()?listed.value<QJSValue>().toVariant().toList():listed.toList();
    QCOMPARE(page.size(),50);
    QCOMPARE(widget->property("canNextPage").toBool(),false);
    QVERIFY(library.errorString().isEmpty());
}
void GamesUiTests::organizationAndManualSmartCollectionsStayProfileLocal(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));auto secondRuntime=runtime();secondRuntime.id="other-test";QVERIFY(apps.addApp(secondRuntime));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    const auto id=library.addGame({{"schemaVersion",1},{"title","Example title"},{"platform","PC"},
        {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    QVERIFY(!id.isEmpty());
    const auto second=library.addGame({{"schemaVersion",1},{"title","Second title"},{"platform","PC"},
        {"runtimeId","other-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    QVERIFY(!second.isEmpty());
    QVERIFY(library.setOrganization(id,"Example Saga",{"Puzzle","Co-op"},{"Alternate title"},"A searchable summary."));
    QCOMPARE(library.details(id).value("franchise").toString(),QString("Example Saga"));
    QCOMPARE(library.details(id).value("tags").toStringList(),QStringList({"Puzzle","Co-op"}));
    QCOMPARE(library.details(id).value("aliases").toStringList(),QStringList({"Alternate title"}));
    QCOMPARE(library.details(id).value("summary").toString(),QString("A searchable summary."));
    QCOMPARE(library.page("search","saga").size(),1);
    QCOMPARE(library.page("search","co-op").size(),1);
    QCOMPARE(library.page("search","alternate title").size(),1);
    QCOMPARE(library.page("search","searchable summary").size(),1);
    QVERIFY(!library.setOrganization(id,"",{"same","SAME"}));
    QVERIFY(!library.setOrganization(id,"",{},{"duplicate","DUPLICATE"}));
    QVERIFY(!library.setOrganization(id,"",{}, {},QString(4097,'x')));
    QCOMPARE(library.details(id).value("franchise").toString(),QString("Example Saga"));
    QVERIFY(library.recordCompletedSession(id));

    const auto manual=library.createCollection("Favorites to play","manual");
    const auto smart=library.createCollection("Puzzle games","smart","title","example");
    const auto favorites=library.createCollection("My favorites","smart","favorite");
    const auto platform=library.createCollection("PC games","smart","platform","PSP");
    const auto franchise=library.createCollection("Saga shelf","smart","franchise","saga");
    const auto tag=library.createCollection("Puzzle tag","smart","tag","puzzle");
    const auto recent=library.createCollection("Recently played","smart","recent");
    const auto literal=library.createCollection("Literal query","smart","title","x' OR 1=1 --");
    QVERIFY(!manual.isEmpty());QVERIFY(!smart.isEmpty());QVERIFY(!favorites.isEmpty());QVERIFY(!platform.isEmpty());
    QVERIFY(!franchise.isEmpty());QVERIFY(!tag.isEmpty());QVERIFY(!recent.isEmpty());QVERIFY(!literal.isEmpty());
    QVERIFY(library.createCollection("Invalid recent rule","smart","recent","now").isEmpty());
    QVERIFY(library.createCollection("Invalid blank tag","smart","tag","").isEmpty());
    const auto collectionById=[&library](const QString &wanted){
        for(const auto &entry:library.collections())if(entry.toMap().value("id").toString()==wanted)return entry.toMap();
        return QVariantMap{};
    };
    QVERIFY(library.setCollectionMember(manual,id,true));
    QVERIFY(library.setCollectionMember(manual,second,true));
    QVERIFY(library.collectionContains(manual,id));
    QCOMPARE(library.pageCollection(manual).size(),2);
    QCOMPARE(library.pageCollection(manual,0).at(0).toMap().value("id").toString(),id);
    QVERIFY(library.moveCollectionMember(manual,second,-1));
    QCOMPARE(library.pageCollection(manual,0).at(0).toMap().value("id").toString(),second);
    QCOMPARE(library.pageCollection(smart).size(),1);
    QCOMPARE(collectionById(smart).value("count").toInt(),1);
    QVERIFY(library.updateCollection(platform,"PC games","platform","pc"));
    QCOMPARE(library.pageCollection(platform).size(),2);
    QCOMPARE(collectionById(platform).value("count").toInt(),2);
    QCOMPARE(library.pageCollection(franchise).size(),1);
    QCOMPARE(collectionById(franchise).value("count").toInt(),1);
    QCOMPARE(library.pageCollection(tag).size(),1);
    QVERIFY(library.updateCollection(tag,"Exact tag","tag","coop"));
    QCOMPARE(library.pageCollection(tag).size(),0);
    QCOMPARE(library.pageCollection(recent).size(),1);
    QCOMPARE(collectionById(recent).value("count").toInt(),1);
    QVERIFY(library.updateCollection(manual,"My ordered games"));
    QVERIFY(!library.updateCollection(manual,"Puzzle games"));
    QCOMPARE(collectionById(manual).value("name").toString(),QString("My ordered games"));
    QVERIFY(library.updateCollection(smart,"Example matches","title","second"));
    QCOMPARE(library.pageCollection(smart).size(),1);
    QVERIFY(library.setFavorite(id,true));
    QCOMPARE(library.pageCollection(favorites).size(),1);
    QCOMPARE(library.pageCollection(literal).size(),0);
    QVERIFY(!library.setCollectionMember(smart,id,true));
    QVERIFY(library.setCollectionMember(manual,id,false));
    QVERIFY(!library.collectionContains(manual,id));
    QVERIFY(library.setCollectionMember(manual,id,true));
    QVERIFY(library.removeGame(id));
    QCOMPARE(library.pageCollection(manual).size(),1);
    QCOMPARE(library.collections().size(),8);
    GameLibrary otherProfile(dir.filePath("other-profile/games.sqlite"),&apps);
    const auto otherGame=otherProfile.addGame({{"schemaVersion",1},{"title","Example title"},{"platform","PC"},
        {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});
    QVERIFY(!otherGame.isEmpty());
    const auto otherShelf=otherProfile.createCollection("Other profile matches","smart","title","example");
    QVERIFY(!otherShelf.isEmpty());
    QCOMPARE(otherProfile.pageCollection(otherShelf).size(),1);
    QCOMPARE(library.collections().size(),8);

    GameLibrary reopened(dir.filePath("profile/games.sqlite"),&apps);
    QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.collections().size(),8);
    QCOMPARE(reopened.pageCollection(smart).size(),1);
    QCOMPARE(reopened.pageCollection(recent).size(),0);
    QCOMPARE(reopened.pageCollection(franchise).size(),0);
    QCOMPARE(reopened.pageCollection(tag).size(),0);
    QVERIFY(reopened.removeCollection(manual));
    QCOMPARE(reopened.collections().size(),7);
}
void GamesUiTests::remoteSmartCollectionRulesAreDpadSelectable(){
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    for(int ruleIndex=4;ruleIndex<=6;++ruleIndex){
        QScopedPointer<QObject> widget(component.createWithInitialProperties({
            {"library",QVariant::fromValue(&library)},{"width",1000},{"height",600},{"engaged",true}}));
        QVERIFY2(widget,qPrintable(component.errorString()));
        widget->setProperty("toolbar",11);command(widget.data(),InputManager::Confirm);
        QCOMPARE(widget->property("mode").toString(),QString("collectionChooser"));
        for(int i=0;i<ruleIndex;++i)command(widget.data(),InputManager::NavigateDown);
        command(widget.data(),InputManager::Confirm);
        QCOMPARE(widget->property("mode").toString(),QString("keyboard"));
        QCOMPARE(widget->property("collectionCreateType").toString(),
            QStringList({"manual","title","favorite","platform","franchise","tag","recent"}).at(ruleIndex));
    }
    QVERIFY(library.collections().isEmpty());
}
void GamesUiTests::fiveThousandGameCatalogStaysPagedAndSearchable(){
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto path=dir.filePath("scale.sqlite");
    const auto connection="games-scale-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto seed=QSqlDatabase::addDatabase("QSQLITE",connection);seed.setDatabaseName(path);QVERIFY(seed.open());
    QSqlQuery query(seed);QVERIFY(query.exec("CREATE TABLE games(id TEXT PRIMARY KEY,title TEXT NOT NULL,platform TEXT NOT NULL,runtime TEXT NOT NULL,root TEXT NOT NULL,target TEXT NOT NULL,favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)),added INTEGER NOT NULL,UNIQUE(runtime,root,target))"));
    QVERIFY(query.exec("CREATE INDEX games_title ON games(title COLLATE NOCASE)"));
    QVERIFY(query.exec("PRAGMA user_version=1"));QVERIFY(seed.transaction());
    query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,favorite,added) VALUES(:id,:title,:platform,:runtime,:root,:target,:favorite,:added)");
    for(int i=0;i<5000;++i){
        const auto suffix=QString("%1").arg(i,5,10,QLatin1Char('0'));
        query.bindValue(":id","game-"+suffix);query.bindValue(":title","Fixture "+suffix);
        query.bindValue(":platform",i%2?"PC":"PSP");query.bindValue(":runtime","runtime-"+suffix);
        query.bindValue(":root","");query.bindValue(":target","");query.bindValue(":favorite",i%7==0?1:0);query.bindValue(":added",i);
        QVERIFY2(query.exec(),qPrintable(query.lastError().text()));query.finish();
    }
    QVERIFY(seed.commit());query=QSqlQuery();seed.close();seed=QSqlDatabase();QSqlDatabase::removeDatabase(connection);
    AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(path,&apps);QVERIFY2(library.ready(),qPrintable(library.errorString()));
    QCOMPARE(library.page("all",{},0).size(),50);
    QCOMPARE(library.page("all",{},4950).size(),50);
    QCOMPARE(library.page("search","Fixture 04999",0).size(),1);
    QCOMPARE(library.page("platform",{},0,"PSP").size(),50);
    QVERIFY(library.page("all",{},5000).isEmpty());
}
void GamesUiTests::catalogExportRestoreIsBoundedAndTransactional(){
    QTemporaryDir dir;QVERIFY(dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    const auto previousProfile=qApp->property("isoldeProfile");
    const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);qApp->setProperty("isoldeProfile",profile);
    auto cleanup=qScopeGuard([&]{qApp->setProperty("isoldeProfile",previousProfile);MediaGrants::save(profile,{});});
    QVERIFY2(MediaGrants::save(profile,{dir.path()}).isEmpty(),"Test export folder grant could not be prepared.");
    AppRegistry apps;QVERIFY(apps.addApp(runtime()));auto second=runtime();second.id="other-test";QVERIFY(apps.addApp(second));
    const auto database=dir.filePath("profile/games.sqlite");
    GameLibrary library(database,&apps);
    QVariantMap registration{{"schemaVersion",1},{"title","Portable fixture"},{"platform","PC"},
        {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}};
    const auto game=library.addGame(registration);QVERIFY(!game.isEmpty());
    registration["title"]="Alternate edition";registration["platform"]="PSP";registration["runtimeId"]="other-test";
    const auto edition=library.addEdition(game,registration);QVERIFY(!edition.isEmpty());
    QVERIFY(library.setOrganization(game,"Fixture series",{"Puzzle"},{"Portable fixture alt"},"Archive summary"));
    QVERIFY(library.setFavorite(game,true));QVERIFY(library.recordCompletedSession(game));
    const auto collection=library.createCollection("Export set","manual");QVERIFY(!collection.isEmpty());
    const auto tagCollection=library.createCollection("Puzzle tag shelf","smart","tag","puzzle");
    const auto recentCollection=library.createCollection("Recent shelf","smart","recent");
    QVERIFY(!tagCollection.isEmpty());QVERIFY(!recentCollection.isEmpty());
    QVERIFY(library.setCollectionMember(collection,game,true));
    const auto coverPath=dir.filePath("cover.png");QImage cover(48,64,QImage::Format_ARGB32);cover.fill(QColor("#3467a1"));
    QVERIFY(cover.save(coverPath,"PNG"));
    QVERIFY2(library.setArtwork(game,coverPath),qPrintable(library.errorString()));
    QVERIFY(!library.details(game).value("artwork").toString().isEmpty());
    QVERIFY(library.clearArtwork(game));QVERIFY(library.details(game).value("artwork").toString().isEmpty());
    QVERIFY2(library.setArtwork(game,coverPath),qPrintable(library.errorString()));
    QTemporaryDir ungrantedArtwork;QVERIFY(ungrantedArtwork.isValid());
    const auto ungrantedCover=ungrantedArtwork.filePath("cover.png");QVERIFY(cover.save(ungrantedCover,"PNG"));
    QVERIFY(!library.setArtwork(game,ungrantedCover));
    const auto invalidCover=dir.filePath("invalid.png");QFile invalidImage(invalidCover);
    QVERIFY(invalidImage.open(QIODevice::WriteOnly));QVERIFY(invalidImage.write("not a PNG")>0);invalidImage.close();
    QVERIFY(!library.setArtwork(game,invalidCover));
    QTemporaryDir ungranted;QVERIFY(ungranted.isValid());
    QVERIFY(!library.exportCatalog(ungranted.filePath("unapproved.json")));
    const auto backup=dir.filePath("games-backup.json");
    QVERIFY2(library.exportCatalog(backup),qPrintable(library.errorString()));
    QVERIFY(QFileInfo(backup).size()>0);
    QVERIFY(!library.restoreCatalog(backup,false));
    const auto invalidBackup=dir.filePath("invalid-backup.json");
    QFile invalid(invalidBackup);QVERIFY(invalid.open(QIODevice::WriteOnly));
    QVERIFY(invalid.write("{\"schemaVersion\":1}")>0);invalid.close();
    QVERIFY(!library.restoreCatalog(invalidBackup,true));
    QCOMPARE(library.page().size(),1);
    const auto triggerConnection="games-restore-injection-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto triggerDb=QSqlDatabase::addDatabase("QSQLITE",triggerConnection);triggerDb.setDatabaseName(database);
    QVERIFY(triggerDb.open());
    QSqlQuery trigger(triggerDb);
    QVERIFY(trigger.exec("CREATE TRIGGER reject_restore BEFORE INSERT ON games WHEN NEW.title='Portable fixture' BEGIN SELECT RAISE(ABORT,'injected restore failure'); END"));
    trigger=QSqlQuery();triggerDb.close();triggerDb=QSqlDatabase();QSqlDatabase::removeDatabase(triggerConnection);
    QVERIFY(!library.restoreCatalog(backup,true));
    QCOMPARE(library.page().size(),1);
    QCOMPARE(library.details(game).value("title").toString(),QString("Portable fixture"));
    const auto dropConnection="games-restore-cleanup-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto cleanupDb=QSqlDatabase::addDatabase("QSQLITE",dropConnection);cleanupDb.setDatabaseName(database);
    QVERIFY(cleanupDb.open());QSqlQuery cleanupQuery(cleanupDb);
    QVERIFY(cleanupQuery.exec("DROP TRIGGER reject_restore"));
    cleanupQuery=QSqlQuery();cleanupDb.close();cleanupDb=QSqlDatabase();QSqlDatabase::removeDatabase(dropConnection);
    QVERIFY(library.removeGame(game));QVERIFY(library.page().isEmpty());
    QVERIFY2(library.restoreCatalog(backup,true),qPrintable(library.errorString()));
    QCOMPARE(library.details(game).value("sourceKind").toString(),QString("runtime-entry"));
    QCOMPARE(library.details(game).value("franchise").toString(),QString("Fixture series"));
    QCOMPARE(library.details(game).value("tags").toStringList(),QStringList({"Puzzle"}));
    QCOMPARE(library.details(game).value("aliases").toStringList(),QStringList({"Portable fixture alt"}));
    QCOMPARE(library.details(game).value("summary").toString(),QString("Archive summary"));
    QVERIFY(!library.details(game).value("artwork").toString().isEmpty());
    QCOMPARE(library.editions(game).size(),2);QCOMPARE(library.page("continue").size(),1);
    QCOMPARE(library.pageCollection(collection).size(),1);QVERIFY(library.collectionContains(collection,game));
    QCOMPARE(library.pageCollection(tagCollection).size(),1);
    QCOMPARE(library.pageCollection(recentCollection).size(),1);
    QFile exported(backup);QVERIFY(exported.open(QIODevice::ReadOnly));
    QJsonParseError parse;const auto exportedObject=QJsonDocument::fromJson(exported.readAll(),&parse).object();exported.close();
    QCOMPARE(parse.error,QJsonParseError::NoError);
    auto versionTwo=exportedObject;versionTwo["schemaVersion"]=2;
    versionTwo.remove("provenance");
    QJsonArray versionTwoGames;
    for(const auto &entry:versionTwo.value("games").toArray()){
        auto gameObject=entry.toObject();gameObject.remove("artwork");versionTwoGames.append(gameObject);
    }
    versionTwo["games"]=versionTwoGames;
    const auto versionTwoPath=dir.filePath("version-two-backup.json");QFile versionTwoFile(versionTwoPath);
    QVERIFY(versionTwoFile.open(QIODevice::WriteOnly));QVERIFY(versionTwoFile.write(QJsonDocument(versionTwo).toJson(QJsonDocument::Compact))>0);versionTwoFile.close();
    QVERIFY2(library.restoreCatalog(versionTwoPath,true),qPrintable(library.errorString()));
    QCOMPARE(library.details(game).value("aliases").toStringList(),QStringList({"Portable fixture alt"}));
    QCOMPARE(library.details(game).value("summary").toString(),QString("Archive summary"));
    QVERIFY(library.details(game).value("artwork").toString().isEmpty());
    auto legacy=exportedObject;legacy["schemaVersion"]=1;
    legacy.remove("provenance");
    QJsonArray legacyGames;
    for(const auto &entry:legacy.value("games").toArray()){
        auto gameObject=entry.toObject();gameObject.remove("aliases");gameObject.remove("summary");gameObject.remove("artwork");legacyGames.append(gameObject);
    }
    legacy["games"]=legacyGames;
    const auto legacyPath=dir.filePath("legacy-backup.json");QFile legacyFile(legacyPath);
    QVERIFY(legacyFile.open(QIODevice::WriteOnly));QVERIFY(legacyFile.write(QJsonDocument(legacy).toJson(QJsonDocument::Compact))>0);legacyFile.close();
    QVERIFY2(library.restoreCatalog(legacyPath,true),qPrintable(library.errorString()));
    QVERIFY(library.details(game).value("aliases").toStringList().isEmpty());
    QVERIFY(library.details(game).value("summary").toString().isEmpty());
    QVERIFY(library.details(game).value("artwork").toString().isEmpty());
    QCOMPARE(QDir(QFileInfo(database).absolutePath()).entryList({"games.sqlite.pre-restore-*.sqlite"},QDir::Files).size(),4);
    GameLibrary reopened(database,&apps);QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.pageCollection(collection).size(),1);QCOMPARE(reopened.editions(game).size(),2);
    QCOMPARE(reopened.pageCollection(tagCollection).size(),1);
    QCOMPARE(reopened.pageCollection(recentCollection).size(),1);
    QCOMPARE(reopened.details(game).value("sourceReference").toString(),QString());
}
void GamesUiTests::catalogProvenanceIsImmutableAndConflictSafe() {
    QTemporaryDir dir;QVERIFY(dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    const auto previousProfile=qApp->property("isoldeProfile");
    const auto profile=QUuid::createUuid().toString(QUuid::WithoutBraces);qApp->setProperty("isoldeProfile",profile);
    auto cleanup=qScopeGuard([&]{qApp->setProperty("isoldeProfile",previousProfile);MediaGrants::save(profile,{});});
    QVERIFY2(MediaGrants::save(profile,{dir.path()}).isEmpty(),"Test export folder grant could not be prepared.");
    AppRegistry apps;QVERIFY(apps.addApp(runtime()));
    const auto database=dir.filePath("profile/games.sqlite");
    GameLibrary library(database,&apps);
    QVariantMap registration{{"schemaVersion",1},{"title","Source fixture"},{"platform","PC"},
        {"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}};
    const auto game=library.addGame(registration);QVERIFY2(!game.isEmpty(),qPrintable(library.errorString()));
    auto provenance=library.details(game).value("acquisitionProvenance").toMap();
    QVERIFY(provenance.value("recorded").toBool());
    QCOMPARE(provenance.value("sourceKind").toString(),QString("runtime-entry"));
    QCOMPARE(provenance.value("origin").toString(),QString("manual-registration"));
    QCOMPARE(provenance.value("sourceFingerprint").toString().size(),64);
    registration["title"]="Corrected metadata";
    QVERIFY2(library.updateGame(game,registration),qPrintable(library.errorString()));
    QCOMPARE(library.details(game).value("acquisitionProvenance").toMap(),provenance);

    const auto connection="games-provenance-immutability-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(database);QVERIFY(db.open());
    QSqlQuery query(db);query.prepare("UPDATE game_provenance SET origin='steam-import' WHERE entity_id=?");
    query.addBindValue(game);QVERIFY(!query.exec());
    query.prepare("DELETE FROM game_provenance WHERE entity_id=?");query.addBindValue(game);QVERIFY(!query.exec());
    query=QSqlQuery();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase(connection);

    const auto backup=dir.filePath("provenance-backup.json");
    QVERIFY2(library.exportCatalog(backup),qPrintable(library.errorString()));
    QFile input(backup);QVERIFY(input.open(QIODevice::ReadOnly));
    auto archive=QJsonDocument::fromJson(input.readAll()).object();input.close();
    auto records=archive.value("provenance").toArray();QCOMPARE(records.size(),1);
    auto tampered=records.at(0).toObject();tampered["sourceFingerprint"]=QString(64,'f');
    records[0]=tampered;archive["provenance"]=records;
    const auto conflictPath=dir.filePath("provenance-conflict.json");QFile conflict(conflictPath);
    QVERIFY(conflict.open(QIODevice::WriteOnly));
    QVERIFY(conflict.write(QJsonDocument(archive).toJson(QJsonDocument::Compact))>0);conflict.close();
    QVERIFY(!library.restoreCatalog(conflictPath,true));
    QCOMPARE(library.details(game).value("acquisitionProvenance").toMap(),provenance);
    QVERIFY2(library.restoreCatalog(backup,true),qPrintable(library.errorString()));
    QCOMPARE(library.details(game).value("acquisitionProvenance").toMap(),provenance);
}
void GamesUiTests::remoteEditionChooserDispatchesOwnedVersion() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));auto secondRuntime=runtime();secondRuntime.id="other-test";QVERIFY(apps.addApp(secondRuntime));
    GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    QVariantMap registration{{"schemaVersion",1},{"title","Shared title"},{"platform","PC"},{"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}};
    const auto game=library.addGame(registration);QVERIFY(!game.isEmpty());registration["runtimeId"]="other-test";registration["platform"]="PSP";registration["title"]="Alternate";
    const auto edition=library.addEdition(game,registration);QVERIFY2(!edition.isEmpty(),qPrintable(library.errorString()));
    QCOMPARE(library.platforms().size(),2);QCOMPARE(library.page("platform",{},0,"psp").size(),1);
    InputManager input;GameDispatchProbe dispatch;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"dispatcher",QVariant::fromValue(&dispatch)},{"originId","game-origin"},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    QCOMPARE(widget->property("mode").toString(),QString("editions"));QVERIFY(dispatch.lastAction.isEmpty());
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    QCOMPARE(dispatch.lastAction.value("gameId").toString(),game);QCOMPARE(dispatch.lastAction.value("editionId").toString(),edition);
    QCOMPARE(dispatch.origin,QString("game-origin"));QCOMPARE(library.page().size(),1);
    QVERIFY(library.recordCompletedSession(game));
    GameLibrary reopened(dir.filePath("profile/games.sqlite"),&apps);QVERIFY2(reopened.ready(),qPrintable(reopened.errorString()));
    QCOMPARE(reopened.editions(game).size(),2);QCOMPARE(reopened.page("continue").size(),1);
}
void GamesUiTests::remoteEditCanCancelAndApplyWithoutMouse() {
    QTemporaryDir dir;AppRegistry apps;QVERIFY(apps.addApp(runtime()));GameLibrary library(dir.filePath("profile/games.sqlite"),&apps);
    const auto id=library.addGame({{"schemaVersion",1},{"title","Original"},{"platform","PC"},{"runtimeId","local-test"},{"root",QStringLiteral("")},{"target",QStringLiteral("")}});QVERIFY(!id.isEmpty());
    InputManager input;QQmlEngine engine;engine.rootContext()->setContextProperty("inputManager",&input);
    QQmlComponent component(&engine,QUrl::fromLocalFile(GAMES_WIDGET_QML));
    QScopedPointer<QObject> widget(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"width",1000},{"height",600}}));QVERIFY2(widget,qPrintable(component.errorString()));
    command(widget.data(),InputManager::NavigateDown);command(widget.data(),InputManager::Confirm);
    for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);QCOMPARE(widget->property("mode").toString(),QString("runtime"));
    command(widget.data(),InputManager::Back);QCOMPARE(library.details(id).value("title").toString(),QString("Original"));
    command(widget.data(),InputManager::Confirm);for(int i=0;i<3;++i)command(widget.data(),InputManager::NavigateRight);
    command(widget.data(),InputManager::Confirm);command(widget.data(),InputManager::Confirm);
    command(widget.data(),InputManager::Confirm); // append digit 1 to existing title
    doneOnKeyboard(widget.data());doneOnKeyboard(widget.data());
    QCOMPARE(widget->property("mode").toString(),QString("review"));QCOMPARE(library.details(id).value("title").toString(),QString("Original"));
    command(widget.data(),InputManager::Confirm);QCOMPARE(library.details(id).value("title").toString(),QString("Original1"));
    QCOMPARE(library.page().size(),1);
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
