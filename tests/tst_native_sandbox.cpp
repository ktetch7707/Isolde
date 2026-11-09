#include "sessions/NativeSandbox.h"
#include "sessions/DesktopSession.h"
#include "sessions/SteamSessionOutput.h"
#include "sessions/GamepadTopology.h"
#include "media/MediaGrants.h"
#include "games/SteamLibraryGrants.h"
#include "games/ManagedGamePaths.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class NativeSandboxTests : public QObject {
    Q_OBJECT
private slots:
    void steamAdapterCannotAttachToHostGameProcesses();
    void steamProcessMarkerIsExactAndBounded();
    void profileAndAppStorageRemainSeparate();
    void explicitMediaIsReadOnlyAndPrivatePathsAreRefused();
    void reviewedGameBundleIsReadOnlyAndCannotEscapeItsRoot();
    void managedExecutableUsesSandboxPathAndRejectsChangedContent();
    void pendingManagedUninstallRecoveryPreservesConflicts();
    void processTreeDiesWithLauncher();
    void gracefulExitFlushesAppState();
    void steamHostAllowsDelayedGracefulExit();
    void steamHostReportsFailedGracefulExit();
    void normalExitRemovesDetachedChildren();
    void invalidIdentityAndSymlinkAreRefused();
    void rememberedKodiGrantsAreScopedReadOnlyAndRevocable();
    void externalSteamGrantsAreProfileScopedAndFailClosed();
    void gamepadCapabilityRecognitionIsBoundedAndExact();
    void gamepadDiscoveryFiltersKeyboardAndRefreshesSnapshot();
    void gamepadTopologyReportsAttachDetachAndReconnect();
    void gameSessionsAcrossSourcesRequestGamepadAccess();
};

static const QString alice = "11111111-1111-4111-8111-111111111111";
static const QString bob = "22222222-2222-4222-8222-222222222222";

void NativeSandboxTests::gamepadCapabilityRecognitionIsBoundedAndExact()
{
    const auto bitmapFor=[](int code){
        QStringList words(5,QStringLiteral("0"));
        words[words.size()-1-code/64]=QString::number(quint64(1)<<(code%64),16);
        return words.join(' ');
    };
    QVERIFY(NativeSandbox::hasGamepadKeyCapabilities(bitmapFor(0x130)));
    QVERIFY(NativeSandbox::hasGamepadKeyCapabilities(bitmapFor(0x13f)));
    QVERIFY(!NativeSandbox::hasGamepadKeyCapabilities(bitmapFor(0x12f)));
    QVERIFY(!NativeSandbox::hasGamepadKeyCapabilities(bitmapFor(0x001e)));
    QVERIFY(!NativeSandbox::hasGamepadKeyCapabilities("0 0 invalid 0 0"));
    QVERIFY(!NativeSandbox::hasGamepadKeyCapabilities(QString(4097,'0')));
}
void NativeSandboxTests::gameSessionsAcrossSourcesRequestGamepadAccess()
{
    QVERIFY(!NativeSandbox::isGameSession({}, {}, {}, {}, {}));
    QVERIFY(NativeSandbox::isGameSession("ppsspp", {}, {}, {}, {}));
    QVERIFY(NativeSandbox::isGameSession({}, "42", {}, {}, {}));
    QVERIFY(NativeSandbox::isGameSession({}, {}, "/media/library", {}, {}));
    QVERIFY(NativeSandbox::isGameSession({}, {}, {}, "/private/install", {}));
    QVERIFY(NativeSandbox::isGameSession({}, {}, {}, {}, QString(64,'a')));

}
void NativeSandboxTests::gamepadDiscoveryFiltersKeyboardAndRefreshesSnapshot()
{
    QTemporaryDir root;QVERIFY(root.isValid());
    const auto sys=root.filePath("sys"),devices=root.filePath("dev");
    QVERIFY(QDir().mkpath(devices));
    const auto addNode=[&](const QString &name,const QString &bitmap){
        const auto capability=sys+"/"+name+"/device/capabilities/key";
        QVERIFY(QDir().mkpath(QFileInfo(capability).absolutePath()));
        QFile file(capability);QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bitmap.toLatin1()),qint64(bitmap.toLatin1().size()));file.close();
        QFile device(devices+"/"+name);QVERIFY(device.open(QIODevice::WriteOnly));device.close();
    };
    addNode("event0","0 0 0 0 1000000000000"); // BTN_GAMEPAD, not ordinary keys.
    addNode("event1","40000000 0 0 0 0"); // Keyboard key.
    addNode("js0","0 0 0 0 1000000000000");
    addNode("event1000","0 0 0 0 1000000000000"); // Outside the node grammar.
    QCOMPARE(NativeSandbox::gamepadDeviceNodes(sys,devices),
             QStringList({devices+"/event0",devices+"/js0"}));

    addNode("event2","0 0 0 0 1000000000000"); // Plugged in after the first snapshot.
    QCOMPARE(NativeSandbox::gamepadDeviceNodes(sys,devices),
             QStringList({devices+"/event0",devices+"/event2",devices+"/js0"}));
    QVERIFY(QFile::remove(devices+"/event0"));
    QCOMPARE(NativeSandbox::gamepadDeviceNodes(sys,devices),
             QStringList({devices+"/event2",devices+"/js0"}));
}
void NativeSandboxTests::gamepadTopologyReportsAttachDetachAndReconnect()
{
    const QSet<QString> launchBound{"event0"};
    auto status=GamepadTopology::summarize(QSet<QString>{"event0"},launchBound);
    QCOMPARE(status.value("connected").toInt(),1);
    QCOMPARE(status.value("available").toInt(),1);
    QCOMPARE(status.value("new").toInt(),0);
    QCOMPARE(status.value("missing").toInt(),0);

    status=GamepadTopology::summarize(QSet<QString>{"event0","event2"},launchBound);
    QCOMPARE(status.value("connected").toInt(),2);
    QCOMPARE(status.value("available").toInt(),1);
    QCOMPARE(status.value("new").toInt(),1);
    QCOMPARE(status.value("missing").toInt(),0);

    status=GamepadTopology::summarize(QSet<QString>{"event2"},launchBound);
    QCOMPARE(status.value("connected").toInt(),1);
    QCOMPARE(status.value("available").toInt(),0);
    QCOMPARE(status.value("new").toInt(),1);
    QCOMPARE(status.value("missing").toInt(),1);

    status=GamepadTopology::summarize(QSet<QString>{"event0"},launchBound);
    QCOMPARE(status.value("available").toInt(),1);
    QCOMPARE(status.value("new").toInt(),0);
    QCOMPARE(status.value("missing").toInt(),0);
}

void NativeSandboxTests::steamProcessMarkerIsExactAndBounded()
{
    SteamSessionOutput parser("42");
    QVERIFY(!parser.consume("STEAM_GAME_PROCESS_OBS"));
    QVERIFY(!parser.consume("ERVED 43\nSTEAM_GAME_PROCESS_OBSERVED 42"));
    QVERIFY(parser.consume("\n"));
    QVERIFY(!parser.consume("prefix STEAM_GAME_PROCESS_OBSERVED 42\n"));
    QVERIFY(!parser.consume(QByteArray(2100,'x')+"STEAM_GAME_PROCESS_OBSERVED 42\n"));
    QVERIFY(!parser.consume("STEAM_GAME_PROCESS_OBSERVED 42x\n"));
    QVERIFY(parser.consume("noise\nSTEAM_GAME_PROCESS_OBSERVED 42\n"));
    const QByteArray state=R"({"account":"unknown","appId":"42","client":"available","cloud_save":"unknown","connection":"unknown","error":"none","exit":"unknown","installation":"verified","local_save":"unknown","offline":"unknown","ownership":"unknown","phase":"ended","process":"gone","version":1})";
    QVERIFY(!parser.consume("STEAM_PROVIDER_STATE "+state.left(50)));
    QVERIFY(parser.takeState().isEmpty());
    QVERIFY(!parser.consume(state.mid(50)+"\n"));
    const auto parsed=parser.takeState();
    QCOMPARE(parsed.value("phase").toString(),QString("ended"));
    QCOMPARE(parsed.value("local_save").toString(),QString("unknown"));
    auto other=state;other.replace("\"appId\":\"42\"","\"appId\":\"43\"");
    parser.consume("STEAM_PROVIDER_STATE "+other+"\n");
    QVERIFY(parser.takeState().isEmpty());
    auto invalid=state;invalid.replace("\"exit\":\"unknown\"","\"exit\":\"guessed\"");
    parser.consume("STEAM_PROVIDER_STATE "+invalid+"\n");
    QVERIFY(parser.takeState().isEmpty());
    auto duplicate=state;duplicate.replace("\"exit\":\"unknown\"","\"exit\":\"unknown\",\"exit\":\"crashed\"");
    parser.consume("STEAM_PROVIDER_STATE "+duplicate+"\n");
    QVERIFY(parser.takeState().isEmpty());
    parser.consume("STEAM_PROVIDER_STATE "+QByteArray(2050,'x')+"\n");
    QVERIFY(parser.takeState().isEmpty());

}

static QByteArray run(const NativeSandboxPlan &plan, const QString &script, const QStringList &extra = {})
{
    if (!plan.error.isEmpty()) { qWarning() << plan.error; return "ERROR"; }
    QProcess process;
    process.setProcessEnvironment(plan.environment);
    process.start(plan.program, plan.arguments + QStringList{"-c", script, "fixture"} + extra);
    if (!process.waitForFinished(15000)) { process.kill(); process.waitForFinished(); return "TIMEOUT"; }
    if (process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit) {
        qWarning() << process.exitCode() << process.readAllStandardError(); return "ERROR";
    }
    return process.readAllStandardOutput();
}
void NativeSandboxTests::steamAdapterCannotAttachToHostGameProcesses(){
    QTemporaryDir storage;auto env=QProcessEnvironment::systemEnvironment();
    QProcess outside;auto outsideEnv=env;outsideEnv.insert("SteamAppId","42");outside.setProcessEnvironment(outsideEnv);
    outside.start("/usr/bin/python3",{"-c","import time; time.sleep(20)"});QVERIFY(outside.waitForStarted());
    const auto plan=NativeSandbox::prepare(alice,"steam","/usr/bin/python3",env,{},storage.path(),{},{},"42");QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));
    QVERIFY(plan.arguments.contains("--steam-session"));
    const auto otherPlan=NativeSandbox::prepare(bob,"steam","/usr/bin/python3",env,{},storage.path(),{},{},"42");
    QVERIFY(otherPlan.error.isEmpty());
    QVERIFY(otherPlan.homeDirectory!=plan.homeDirectory);
    QFile profileSave(plan.homeDirectory+"/steam-save");
    QVERIFY(profileSave.open(QIODevice::WriteOnly)); QCOMPARE(profileSave.write("private"),qint64(7)); profileSave.close();
    QCOMPARE(run(otherPlan,"import pathlib; print(pathlib.Path.home().joinpath('steam-save').exists())"),QByteArray("False\n"));
    QCOMPARE(run(plan,"import runpy; m=runpy.run_path('/run/steam-games.py'); print(m['game_processes']('42'))"),QByteArray("[]\n"));
    QCOMPARE(run(plan,"import subprocess; p=subprocess.run(['/usr/bin/python3','/run/steam-games.py','--launch','42'],capture_output=True); assert p.returncode==2 and b'Steam game:' in p.stderr; print('missing-game-refused')"),QByteArray("missing-game-refused\n"));
    QVERIFY(outside.state()!=QProcess::NotRunning);outside.terminate();QVERIFY(outside.waitForFinished());
    QVERIFY(!NativeSandbox::prepare(alice,"other","/usr/bin/python3",env,{},storage.path(),{},{},"42").error.isEmpty());
    QVERIFY(!NativeSandbox::prepare(alice,"steam","/usr/bin/python3",env,{},storage.path(),{},{},"42 -bad").error.isEmpty());
}
void NativeSandboxTests::externalSteamGrantsAreProfileScopedAndFailClosed()
{
    QTemporaryDir config, storage;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    const auto candidate=QStringLiteral("/media/isolde-test/SteamLibrary");
    QVERIFY(SteamLibraryGrants::validate(candidate,false).isEmpty());
    QVERIFY(!SteamLibraryGrants::validate("/media",false).isEmpty());
    QVERIFY(!SteamLibraryGrants::validate("/home/other/SteamLibrary",false).isEmpty());
    QVERIFY(!SteamLibraryGrants::validate("/etc/SteamLibrary",false).isEmpty());
    QVERIFY(SteamLibraryGrants::save(alice,{candidate}).isEmpty());

    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Isolde","MediaAccess");
    QCOMPARE(SteamLibraryGrants::folders(alice),QStringList{candidate});
    QVERIFY(SteamLibraryGrants::folders(bob).isEmpty());
    const auto env=QProcessEnvironment::systemEnvironment();
    const auto disconnected=NativeSandbox::prepare(alice,"steam","/usr/bin/python3",env,{},storage.path(),{},{},"42");
    QVERIFY(disconnected.error.isEmpty());
    QVERIFY(disconnected.steamLibraryRoots.isEmpty());
    settings.setValue("profiles/"+alice+"/steam/libraries",QStringList{"/etc/isolde-test"});
    const auto invalid=NativeSandbox::prepare(alice,"steam","/usr/bin/python3",env,{},storage.path(),{},{},"42");
    QVERIFY(!invalid.error.isEmpty());
    QSettings::setDefaultFormat(original);
}
void NativeSandboxTests::rememberedKodiGrantsAreScopedReadOnlyAndRevocable()
{
    QTemporaryDir config, storage, media;
    const auto original = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    QDir().mkpath(media.filePath("Nested"));
    const auto movie = media.filePath("Nested/movie.txt");
    QFile file(movie);
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write("preserve-me"); file.close();
    QVERIFY(MediaGrants::save(alice, {media.path()}).isEmpty());
    const auto env = QProcessEnvironment::systemEnvironment();
    auto plan = NativeSandbox::prepare(alice, "kodi", "/bin/sh", env, {}, storage.path());
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(run(plan, "if printf overwrite > \"$1\" 2>/dev/null; then exit 1; fi; cat \"$1\"", {movie}), QByteArray("preserve-me"));
    const auto denied = "if test -e \"$1\"; then exit 1; fi; printf private";
    QCOMPARE(run(NativeSandbox::prepare(bob, "kodi", "/bin/sh", env, {}, storage.path()), denied, {movie}), QByteArray("private"));
    QCOMPARE(run(NativeSandbox::prepare(alice, "other-app", "/bin/sh", env, {}, storage.path()), denied, {movie}), QByteArray("private"));
    QVERIFY(MediaGrants::save(alice, {}).isEmpty());
    QCOMPARE(run(NativeSandbox::prepare(alice, "kodi", "/bin/sh", env, {}, storage.path()), denied, {movie}), QByteArray("private"));
    QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("preserve-me")); file.close();
    const auto absent = media.filePath("Disconnected drive");
    QVERIFY(MediaGrants::save(alice, {absent}).isEmpty());
    QVERIFY(NativeSandbox::prepare(alice, "kodi", "/bin/sh", env, {}, storage.path()).error.isEmpty());
    QVERIFY(QFile::link(storage.path(), absent));
    QVERIFY(!NativeSandbox::prepare(alice, "kodi", "/bin/sh", env, {}, storage.path()).error.isEmpty());
    QSettings::setDefaultFormat(original);
}

void NativeSandboxTests::profileAndAppStorageRemainSeparate()
{
    QTemporaryDir storage;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("SSH_AUTH_SOCK", "/host-secret-agent");
    environment.insert("DBUS_SESSION_BUS_ADDRESS", "unix:path=/host-secret-bus");
    const auto a = NativeSandbox::prepare(alice, "fixture", "/bin/sh", environment, {}, storage.path());
    const auto b = NativeSandbox::prepare(bob, "fixture", "/bin/sh", environment, {}, storage.path());
    const auto otherApp = NativeSandbox::prepare(alice, "other-app", "/bin/sh", environment, {}, storage.path());
    QVERIFY2(a.error.isEmpty(), qPrintable(a.error));
    QFile sentinel(storage.filePath("host-private"));
    QVERIFY(sentinel.open(QIODevice::WriteOnly)); sentinel.write("not-visible"); sentinel.close();
    const auto create = QStringLiteral(
        "test ! -e \"$1\" && test -z \"$SSH_AUTH_SOCK\" && "
        "test ! -e \"$XDG_RUNTIME_DIR/bus\" && test ! -e \"$HOME/.ssh\" && "
        "test -n \"$DBUS_SESSION_BUS_ADDRESS\" && "
        "gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.ListNames >/dev/null && "
        "test ! -e \"$HOME/account\" && printf '%s' \"$2\" > \"$HOME/account\" && "
        "printf '%s' \"$DBUS_SESSION_BUS_ADDRESS\"");
    const auto busA = run(a, create, {sentinel.fileName(), "Alice"});
    QVERIFY(busA.startsWith("unix:path="));
    const auto busB = run(b, create, {sentinel.fileName(), "Bob"});
    QVERIFY(busB.startsWith("unix:path="));
    QVERIFY(busA != busB);
    QVERIFY(run(otherApp, create, {sentinel.fileName(), "Other app"}).startsWith("unix:"));
    QCOMPARE(run(a, "cat \"$HOME/account\""), QByteArray("Alice"));
    QCOMPARE(run(b, "cat \"$HOME/account\""), QByteArray("Bob"));
    QCOMPARE(run(otherApp, "cat \"$HOME/account\""), QByteArray("Other app"));
    QVERIFY(a.homeDirectory != b.homeDirectory);
}

void NativeSandboxTests::explicitMediaIsReadOnlyAndPrivatePathsAreRefused()
{
    QTemporaryDir storage, media;
    QFile file(media.filePath("movie.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly)); file.write("sample-media"); file.close();
    const auto environment = QProcessEnvironment::systemEnvironment();
    const auto plan = NativeSandbox::prepare(alice, "fixture", "/bin/sh", environment, file.fileName(), storage.path());
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QCOMPARE(run(plan, "if printf overwrite > \"$1\" 2>/dev/null; then exit 1; fi; cat \"$1\"", {plan.actionTarget}), QByteArray("sample-media"));
    QVERIFY(!NativeSandbox::prepare(alice, "fixture", "/bin/sh", environment, storage.path(), storage.path()).error.isEmpty());
    QVERIFY(!NativeSandbox::prepare(alice, "fixture", "/bin/sh", environment, QDir::homePath(), storage.path()).error.isEmpty());
    QVERIFY(!NativeSandbox::prepare(alice, "fixture", "/bin/sh", environment, "/proc", storage.path()).error.isEmpty());
}
void NativeSandboxTests::reviewedGameBundleIsReadOnlyAndCannotEscapeItsRoot(){
    QTemporaryDir storage,bundle,unrelated;
    QVERIFY(QDir().mkpath(bundle.filePath("game/models")));
    const auto target=bundle.filePath("game/EBOOT.PBP"),resource=bundle.filePath("game/models/mesh.txt");
    for(const auto &path:QStringList{target,resource}){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("preserve-bundle");}
    QFile secret(unrelated.filePath("private"));QVERIFY(secret.open(QIODevice::WriteOnly));secret.write("not-granted");secret.close();
    const auto env=QProcessEnvironment::systemEnvironment();
    const auto plan=NativeSandbox::prepare(alice,"fixture","/bin/sh",env,target,storage.path(),{},bundle.path());
    QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QCOMPARE(plan.actionTarget,QString("/media/games/game/EBOOT.PBP"));
    QCOMPARE(run(plan,"test ! -e \"$2\" && if printf overwrite > \"$1\" 2>/dev/null; then exit 1; fi; cat \"$1\"",{"/media/games/game/models/mesh.txt",secret.fileName()}),QByteArray("preserve-bundle"));
    QVERIFY(!NativeSandbox::prepare(alice,"fixture","/bin/sh",env,target,storage.path(),{},unrelated.path()).error.isEmpty());
    QVERIFY(!NativeSandbox::prepare(alice,"fixture","/bin/sh",env,target,storage.path(),{},QDir::homePath()).error.isEmpty());
    const auto redirected=bundle.filePath("redirect");QVERIFY(QFile::link(unrelated.path(),redirected));
    QVERIFY(!NativeSandbox::prepare(alice,"fixture","/bin/sh",env,target,storage.path(),{},redirected).error.isEmpty());
    QFile original(resource);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("preserve-bundle"));
}

void NativeSandboxTests::managedExecutableUsesSandboxPathAndRejectsChangedContent(){
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir storage;
    const auto parent=ManagedGamePaths::gamesRoot(alice)+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto root=parent+"/install", target=root+"/game";
    QVERIFY(QDir().mkpath(root));
    QFile marker(root+"/.isolde-install.json");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("{}");marker.close();
    QVERIFY(QFile::copy("/usr/bin/true",target));
    QVERIFY(QFile::setPermissions(target,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    const auto env=QProcessEnvironment::systemEnvironment();
    const auto plan=NativeSandbox::prepare(alice,"fixture",target,env,target,storage.path(),{},root,{},root);
    QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));
    QCOMPARE(plan.actionTarget,QString("/media/games/game"));
    QCOMPARE(plan.arguments.last(),plan.actionTarget);
    QVERIFY(plan.arguments.contains("--ro-bind"));
    QVERIFY(!NativeSandbox::prepare(alice,"fixture",target,env,target,storage.path(),{},storage.path(),{},root).error.isEmpty());
    QFile changed(target);QVERIFY(changed.open(QIODevice::WriteOnly|QIODevice::Truncate));changed.write("#!/bin/sh\n");changed.close();
    QVERIFY(!NativeSandbox::prepare(alice,"fixture",target,env,target,storage.path(),{},root,{},root).error.isEmpty());
    QVERIFY(!NativeSandbox::prepare(alice,"fixture","/usr/bin/true",env,{},storage.path(),{},root,{}, {},QString(64,'a')).error.isEmpty());
    QVERIFY(QDir(parent).removeRecursively());
    QStandardPaths::setTestModeEnabled(false);
}

void NativeSandboxTests::pendingManagedUninstallRecoveryPreservesConflicts(){
    QStandardPaths::setTestModeEnabled(true);
    const auto parent=ManagedGamePaths::gamesRoot(alice)+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto root=parent+"/install",pending=root+".uninstall-pending";
    QVERIFY(QDir().mkpath(pending));
    QFile marker(pending+"/.isolde-install.json");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("{}");marker.close();
    QVERIFY(ManagedGamePaths::recoverPendingUninstall(alice,root).isEmpty());
    QVERIFY(QFileInfo::exists(root+"/.isolde-install.json"));QVERIFY(!QFileInfo::exists(pending));
    QVERIFY(QDir().mkpath(pending));
    QVERIFY(!ManagedGamePaths::recoverPendingUninstall(alice,root).isEmpty());
    QVERIFY(QFileInfo::exists(root+"/.isolde-install.json"));QVERIFY(QFileInfo::exists(pending));
    QVERIFY(QDir(parent).removeRecursively());
    QStandardPaths::setTestModeEnabled(false);
}

void NativeSandboxTests::processTreeDiesWithLauncher()
{
    QTemporaryDir storage;
    const auto plan = NativeSandbox::prepare(alice, "fixture", "/bin/sh", QProcessEnvironment::systemEnvironment(), {}, storage.path());
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QProcess process;
    process.setProcessEnvironment(plan.environment);
    process.start(plan.program, plan.arguments + QStringList{"-c",
        "setsid sh -c 'while :; do printf x >> \"$HOME/heartbeat\"; sleep 0.05; done' & wait"});
    QVERIFY(process.waitForStarted());
    const auto heartbeat = plan.homeDirectory + "/heartbeat";
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(heartbeat).size() >= 3, 5000);
    process.terminate();
    QVERIFY(process.waitForFinished(5000));
    QTest::qWait(200);
    const auto stoppedSize = QFileInfo(heartbeat).size();
    QTest::qWait(300);
    QCOMPARE(QFileInfo(heartbeat).size(), stoppedSize);
}

void NativeSandboxTests::invalidIdentityAndSymlinkAreRefused()
{
    QTemporaryDir storage, outside;
    const auto environment = QProcessEnvironment::systemEnvironment();
    QVERIFY(!NativeSandbox::prepare("../escape", "fixture", "/bin/sh", environment, {}, storage.path()).error.isEmpty());
    QVERIFY(!NativeSandbox::prepare(alice, "../escape", "/bin/sh", environment, {}, storage.path()).error.isEmpty());
    QVERIFY(QFile::link(outside.path(), storage.filePath(alice)));
    QVERIFY(!NativeSandbox::prepare(alice, "fixture", "/bin/sh", environment, {}, storage.path()).error.isEmpty());
    QVERIFY(!QFileInfo::exists(outside.filePath("fixture")));
    QVERIFY(!NativeSandbox::prepare(alice, "fixture", "/usr/bin/flatpak", environment, {}, outside.path()).error.isEmpty());
}

void NativeSandboxTests::gracefulExitFlushesAppState()
{
    QTemporaryDir storage;
    const auto plan = NativeSandbox::prepare(alice, "fixture", "/bin/sh", QProcessEnvironment::systemEnvironment(), {}, storage.path());
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QProcess process;
    process.setProcessEnvironment(plan.environment);
    process.start(plan.program, plan.arguments + QStringList{"-c",
        "trap 'printf saved > \"$HOME/saved\"; exit 0' TERM; printf ready > \"$HOME/ready\"; while :; do sleep 0.1; done"});
    QVERIFY(process.waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(plan.homeDirectory + "/ready"), 5000);
    process.write("quit\n");
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitCode(), 0);
    QFile saved(plan.homeDirectory + "/saved");
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), QByteArray("saved"));
}

void NativeSandboxTests::steamHostAllowsDelayedGracefulExit()
{
    QTemporaryDir storage;
    const auto ready=storage.filePath("ready");
    const auto saved=storage.filePath("saved");
    QProcess process;
    process.start(QCoreApplication::applicationDirPath()+"/isolde-native-host",
        {"--steam-session","--","/usr/bin/python3","-c",
         "import pathlib,signal,sys,time\n"
         "def stop(*args):\n time.sleep(2.5); pathlib.Path(sys.argv[2]).write_text('saved'); sys.exit(0)\n"
         "signal.signal(signal.SIGTERM,stop)\n"
         "pathlib.Path(sys.argv[1]).write_text('ready')\n"
         "while True: time.sleep(.1)\n",ready,saved});
    QVERIFY(process.waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready), 5000);
    process.write("quit\n");
    QVERIFY(process.waitForFinished(7000));
    QCOMPARE(process.exitCode(),0);
    QFile file(saved); QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(),QByteArray("saved"));
}

void NativeSandboxTests::steamHostReportsFailedGracefulExit()
{
    QTemporaryDir storage;
    const auto ready=storage.filePath("ready");
    QProcess process;
    process.start(QCoreApplication::applicationDirPath()+"/isolde-native-host",
        {"--steam-session","--","/usr/bin/python3","-c",
         "import pathlib,signal,sys,time\n"
         "signal.signal(signal.SIGTERM,lambda *_: sys.exit(2))\n"
         "pathlib.Path(sys.argv[1]).write_text('ready')\n"
         "while True: time.sleep(.1)\n",ready});
    QVERIFY(process.waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready), 5000);
    process.write("quit\n");
    QVERIFY(process.waitForFinished(7000));
    QCOMPARE(process.exitCode(),1);
}

void NativeSandboxTests::normalExitRemovesDetachedChildren()
{
    QTemporaryDir storage;
    const auto plan = NativeSandbox::prepare(alice, "fixture", "/bin/sh", QProcessEnvironment::systemEnvironment(), {}, storage.path());
    QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
    QProcess process;
    process.setProcessEnvironment(plan.environment);
    process.start(plan.program, plan.arguments + QStringList{"-c",
        "setsid sh -c 'while :; do printf x >> \"$HOME/heartbeat\"; sleep 0.05; done' & sleep 0.3; exit 0"});
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitCode(), 0);
    const auto heartbeat = plan.homeDirectory + "/heartbeat";
    QTest::qWait(200);
    const auto stoppedSize = QFileInfo(heartbeat).size();
    QVERIFY(stoppedSize > 0);
    QTest::qWait(300);
    QCOMPARE(QFileInfo(heartbeat).size(), stoppedSize);
}

QTEST_GUILESS_MAIN(NativeSandboxTests)
#include "tst_native_sandbox.moc"
