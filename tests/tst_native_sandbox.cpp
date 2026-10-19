#include "sessions/NativeSandbox.h"
#include "sessions/DesktopSession.h"
#include "media/MediaGrants.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class NativeSandboxTests : public QObject {
    Q_OBJECT
private slots:
    void profileAndAppStorageRemainSeparate();
    void explicitMediaIsReadOnlyAndPrivatePathsAreRefused();
    void processTreeDiesWithLauncher();
    void gracefulExitFlushesAppState();
    void normalExitRemovesDetachedChildren();
    void invalidIdentityAndSymlinkAreRefused();
    void rememberedKodiGrantsAreScopedReadOnlyAndRevocable();
};

static const QString alice = "11111111-1111-4111-8111-111111111111";
static const QString bob = "22222222-2222-4222-8222-222222222222";

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
