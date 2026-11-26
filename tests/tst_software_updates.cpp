#include "settings/SearchSettings.h"
#include "settings/SoftwareUpdates.h"

#include <QHostAddress>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class ReleaseServer : public QObject
{
    Q_OBJECT
  public:
    QTcpServer server;
    QByteArray response;
    QByteArray status = "200 OK";
    QByteArray extraHeaders;
    int requests = 0;
    bool hold = false;

    ReleaseServer()
    {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections())
            {
                auto* socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    if (!socket->readAll().contains("\r\n\r\n"))
                        return;
                    ++requests;
                    if (!hold)
                    {
                        socket->write("HTTP/1.1 " + status + "\r\n" + extraHeaders + "Content-Type: application/json\r\nContent-Length: " +
                                      QByteArray::number(response.size()) + "\r\nConnection: close\r\n\r\n" + response);
                        socket->disconnectFromHost();
                    }
                });
            }
        });
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/releases/latest").arg(server.serverPort())); }

    void release(const QString& version, bool compatible = true, bool prerelease = false,
                 const QString& platform = QStringLiteral("linux-amd64"))
    {
        QJsonArray assets{QJsonObject{{"name", "update-manifest.json"}},
                          QJsonObject{{"name", "update-manifest.sig"}}};
        if (compatible)
            assets.append(QJsonObject{{"name", platform.startsWith("desktop-")
                ? "isolde_" + version + "_" + platform.mid(8) + ".deb"
                : "isolde-" + version + "-" + platform + ".tar.gz"}});
        response = QJsonDocument(QJsonObject{{"tag_name", "v" + version}, {"draft", false},
                                             {"prerelease", prerelease}, {"assets", assets}}).toJson();
    }
};

class SoftwareUpdatesTests : public QObject
{
    Q_OBJECT
  private slots:
    void webGateAndDismissal();
    void checkRequestsWebEnableWhenWebDisabled();
    void incompatibleAndMalformedReleases();
    void cancelWhenWebDisabled();
    void rateLimitsManualRetry();
    void failedDismissalKeepsNotification();
    void desktopPackagesMatchArchitecture();
    void installFailsWhenNoUpdate();
    void installFailsWithoutPublicKey();
    void publicKeyPathIsReported();
    void savedVersionsReadsPinnedFile();
    void currentInstalledVersionReadsVersionFile();
    void restoreRefusesUnsavedVersion();
    void saveAndDropIgnoreEmptyVersion();
};

void SoftwareUpdatesTests::webGateAndDismissal()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    server.release("0.0.1.3");
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    const auto state = directory.filePath("updates.ini");
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(), state, "linux-amd64");
    updates.check();
    QTest::qWait(50);
    QCOMPARE(server.requests, 0);
    QVERIFY(!updates.available());

    web.setWebEnabled(true);
    QTRY_COMPARE(server.requests, 1);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(updates.available());
    QCOMPARE(updates.latestVersion(), QStringLiteral("0.0.1.3"));
    updates.dismiss();
    QVERIFY(!updates.available());

    SoftwareUpdates reloaded(&web, "0.0.1.2", nullptr, nullptr, server.url(), state, "linux-amd64");
    QTRY_COMPARE(server.requests, 2);
    QTRY_VERIFY(!reloaded.checking());
    QVERIFY(!reloaded.available());
    server.release("0.0.1.4");
    reloaded.check();
    QTRY_COMPARE(server.requests, 3);
    QTRY_VERIFY(!reloaded.checking());
    QVERIFY(reloaded.available());
    server.release("0.0.1.3");
    reloaded.dismiss();
    reloaded.check();
    QTRY_COMPARE(server.requests, 4);
    QTRY_VERIFY(!reloaded.checking());
    QVERIFY(!reloaded.available());
}

// A manual check while web is off must not hit the network; it asks the UI to
// enable web first instead.
void SoftwareUpdatesTests::checkRequestsWebEnableWhenWebDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    // SearchSettings defaults to web disabled.
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    QVERIFY(!web.webEnabled());
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("updates.ini"), "linux-amd64");
    bool requested = false;
    QObject::connect(&updates, &SoftwareUpdates::webEnableRequested, [&] { requested = true; });
    updates.check();
    QTest::qWait(50);
    QVERIFY(requested);
    QCOMPARE(server.requests, 0);
    QVERIFY(!updates.checking());
    QVERIFY(!updates.available());
}

void SoftwareUpdatesTests::incompatibleAndMalformedReleases()
{
    QTemporaryDir directory;
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    server.release("0.0.1.3", false);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("updates.ini"), "linux-amd64");
    QTRY_COMPARE(server.requests, 1);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(!updates.available());
    QVERIFY(updates.status().contains("no update"));
    server.release("0.0.1.3", true, true);
    updates.check();
    QTRY_COMPARE(server.requests, 2);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(!updates.available());
    QVERIFY(updates.status().contains("invalid"));
    server.response = "{";
    updates.check();
    QTRY_COMPARE(server.requests, 3);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(!updates.available());
}

void SoftwareUpdatesTests::cancelWhenWebDisabled()
{
    QTemporaryDir directory;
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    server.hold = true;
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("updates.ini"), "linux-amd64");
    QTRY_COMPARE(server.requests, 1);
    QVERIFY(updates.checking());
    web.setWebEnabled(false);
    QVERIFY(!updates.checking());
    updates.check();
    QTest::qWait(50);
    QCOMPARE(server.requests, 1);
    QVERIFY(updates.status().contains("Web is off"));
}

void SoftwareUpdatesTests::rateLimitsManualRetry()
{
    QTemporaryDir directory;
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    server.status = "429 Too Many Requests";
    server.extraHeaders = "Retry-After: 120\r\n";
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("updates.ini"), "linux-amd64");
    QTRY_COMPARE(server.requests, 1);
    QTRY_VERIFY(!updates.checking());
    updates.check();
    QTest::qWait(50);
    QCOMPARE(server.requests, 1);
    QVERIFY(updates.status().contains("rate limited"));
}

void SoftwareUpdatesTests::failedDismissalKeepsNotification()
{
    QTemporaryDir directory;
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    server.release("0.0.1.3");
    QFile blocked(directory.filePath("not-a-directory"));
    QVERIFY(blocked.open(QIODevice::WriteOnly));
    blocked.close();
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("not-a-directory/updates.ini"), "linux-amd64");
    QTRY_COMPARE(server.requests, 1);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(updates.available());
    updates.dismiss();
    QVERIFY(updates.available());
    QVERIFY(updates.status().contains("Could not save"));
}

// Desktop candidates must be packages for this architecture, not appliance archives.
void SoftwareUpdatesTests::desktopPackagesMatchArchitecture()
{
    QTemporaryDir directory;
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    server.release("0.0.1.3", true, false, "linux-amd64");
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("updates.ini"), "desktop-amd64");
    QTRY_COMPARE(server.requests, 1);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(!updates.available());

    server.release("0.0.1.3", true, false, "desktop-arm64");
    updates.check();
    QTRY_COMPARE(server.requests, 2);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(!updates.available());

    server.release("0.0.1.3", true, false, "desktop-amd64");
    updates.check();
    QTRY_COMPARE(server.requests, 3);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(updates.available());
    updates.dismiss();
    QVERIFY(!updates.available());
}

// install() must refuse when no update is available.
void SoftwareUpdatesTests::installFailsWhenNoUpdate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, QUrl(),
                            directory.filePath("updates.ini"), "linux-amd64");
    QVERIFY(!updates.available());
    // install() is a slot; calling it directly tests the precondition guard.
    QMetaObject::invokeMethod(&updates, "install");
    QVERIFY(!updates.installing());
    QVERIFY(updates.status().contains("No update is available"));
}

// install() must refuse when no trusted public key is installed.
void SoftwareUpdatesTests::installFailsWithoutPublicKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ReleaseServer server;
    QVERIFY(server.server.listen(QHostAddress::LocalHost));
    server.release("0.0.1.3");
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, server.url(),
                            directory.filePath("updates.ini"), "linux-amd64", {});
    QTRY_COMPARE(server.requests, 1);
    QTRY_VERIFY(!updates.checking());
    QVERIFY(updates.available());
    QVERIFY(updates.publicKeyPath().isEmpty());
    QMetaObject::invokeMethod(&updates, "install");
    QVERIFY(!updates.installing());
    QVERIFY(updates.status().contains("No trusted signing key"));
}

// The public key path set at construction is reported through the property.
void SoftwareUpdatesTests::publicKeyPathIsReported()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString keyPath = directory.filePath("isolde-signing-key.pub");
    QFile keyFile(keyPath);
    QVERIFY(keyFile.open(QIODevice::WriteOnly));
    keyFile.write("-----BEGIN PUBLIC KEY-----\ndummy\n-----END PUBLIC KEY-----\n");
    keyFile.close();
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    web.setWebEnabled(true);
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, QUrl(),
                            directory.filePath("updates.ini"), "linux-amd64", keyPath);
    QCOMPARE(updates.publicKeyPath(), keyPath);
}

// Saved versions are read from the appliance prefix pinned file and tagged with
// whether they are the currently installed release.
void SoftwareUpdatesTests::savedVersionsReadsPinnedFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString prefix = directory.path();
    QFile pinned(prefix + QStringLiteral("/pinned"));
    QVERIFY(pinned.open(QIODevice::WriteOnly));
    pinned.write("[\"0.0.1.2\", \"0.0.1.3\"]");
    pinned.close();
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, QUrl(),
                            directory.filePath("updates.ini"), "linux-amd64", {}, prefix);
    const auto saved = updates.savedVersions();
    QCOMPARE(saved.size(), 2);
    QCOMPARE(saved[0].toMap()["version"].toString(), QStringLiteral("0.0.1.2"));
    QCOMPARE(saved[0].toMap()["current"].toBool(), false);
    QCOMPARE(saved[1].toMap()["version"].toString(), QStringLiteral("0.0.1.3"));
    QCOMPARE(saved[1].toMap()["current"].toBool(), false);
}

// The current installed version is read from the current -> release symlink and
// the release's version file.
void SoftwareUpdatesTests::currentInstalledVersionReadsVersionFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString prefix = directory.path();
    const auto releaseDir = QDir(prefix).filePath(QStringLiteral("releases/0.0.1.3"));
    QVERIFY(QDir().mkpath(releaseDir));
    QFile versionFile(releaseDir + QStringLiteral("/version"));
    QVERIFY(versionFile.open(QIODevice::WriteOnly));
    versionFile.write("0.0.1.3\n");
    versionFile.close();
    QVERIFY(QProcess::execute("ln", QStringList{"-s", releaseDir, prefix + QStringLiteral("/current")}) == 0);
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, QUrl(),
                            directory.filePath("updates.ini"), "linux-amd64", {}, prefix);
    QCOMPARE(updates.currentInstalledVersion(), QStringLiteral("0.0.1.3"));
}

// Restoring a version that is not saved fails synchronously without touching the
// installation.
void SoftwareUpdatesTests::restoreRefusesUnsavedVersion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString prefix = directory.path();
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, QUrl(),
                            directory.filePath("updates.ini"), "linux-amd64", {}, prefix);
    QMetaObject::invokeMethod(&updates, "restoreVersion", Q_ARG(QString, QStringLiteral("0.0.1.3")));
    QTest::qWait(50);
    QVERIFY(updates.status().contains("not saved"));
    QVERIFY(!updates.installing());
}

// Empty versions are ignored by save and drop.
void SoftwareUpdatesTests::saveAndDropIgnoreEmptyVersion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString prefix = directory.path();
    SearchSettings web(nullptr, directory.filePath("search.ini"), "default");
    SoftwareUpdates updates(&web, "0.0.1.2", nullptr, nullptr, QUrl(),
                            directory.filePath("updates.ini"), "linux-amd64", {}, prefix);
    QMetaObject::invokeMethod(&updates, "saveVersion", Q_ARG(QString, QString{}));
    QMetaObject::invokeMethod(&updates, "dropVersion", Q_ARG(QString, QString{}));
    QTest::qWait(50);
    QVERIFY(updates.savedVersions().isEmpty());
}

QTEST_GUILESS_MAIN(SoftwareUpdatesTests)
#include "tst_software_updates.moc"
