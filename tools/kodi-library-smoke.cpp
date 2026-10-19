// Opt-in acceptance. Creates a NEW test profile and tiny synthetic videos only.
// Configure the fixture source in Kodi's UI, then F12 quits normally. The second
// movie is created afterward, so only the Isolde scan can discover that addition.
#include "media/KodiService.h"
#include "media/KodiMedia.h"
#include "sessions/DesktopSession.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QMap>
#include <QTimer>
#include <QUuid>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Isolde");
    if (app.arguments().size() != 2) return 2;
    const auto profile = app.arguments().at(1);
    if (QUuid(profile).isNull()) return 2;
    const auto userdata = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/native-profiles/" + profile + "/kodi/home/.kodi/userdata/";
    const auto collection = QDir::homePath() + "/Videos/Isolde-library-test-" + profile;
    if (QFileInfo::exists(QFileInfo(userdata).absolutePath()) || QFileInfo::exists(collection)) return 2;
    QMap<QString, QByteArray> hashes;
    const auto hash = [](const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray();
        return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
    };
    const auto write = [](const QString &path, const QByteArray &bytes) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(bytes) == bytes.size();
    };
    const auto movie = [&](const QString &name) {
        const auto base = collection + "/Nested/" + name + '/' + name;
        QDir().mkpath(QFileInfo(base).absolutePath());
        QProcess generator;
        generator.start("/usr/bin/gst-launch-1.0", {"-q", "videotestsrc", "num-buffers=15", "!",
            "video/x-raw,width=160,height=120,framerate=15/1", "!", "theoraenc", "!", "oggmux", "!", "filesink", "location=" + base + ".ogv"});
        if (!generator.waitForFinished(15000) || generator.exitCode() != 0 || generator.exitStatus() != QProcess::NormalExit) {
            qCritical() << generator.readAllStandardError(); generator.kill(); generator.waitForFinished(); return false;
        }
        if (!write(base + ".nfo", ("<movie><title>" + name + "</title><year>2026</year><plot>Synthetic Isolde acceptance fixture.</plot></movie>").toUtf8())) return false;
        hashes[base + ".ogv"] = hash(base + ".ogv");
        hashes[base + ".nfo"] = hash(base + ".nfo");
        return !hashes[base + ".ogv"].isEmpty();
    };
    if (!movie("Isolde Existing Fixture")) return 2;
    if (!write(userdata + "sources.xml", ("<sources><video><default pathversion=\"1\"></default><source><name>Isolde Library Test</name><path pathversion=\"1\">" + collection.toHtmlEscaped() + "/</path><allowsharing>false</allowsharing></source></video></sources>").toUtf8())) return 2;
    if (!write(userdata + "keymaps/isolde-test.xml", "<keymap><global><keyboard><f8>ActivateWindow(Videos,sources://video/,return)</f8><f12>Quit()</f12></keyboard></global></keymap>")) return 2;
    app.setProperty("isoldeProfile", profile);
    KodiService service(profile);
    KodiMedia media(profile);
    service.setEnabled(false);
    service.setFrontend("kodi");
    service.addLibraryFolder(QUrl::fromLocalFile(collection));
    if (service.libraryFolders() != QStringList{collection}) { qCritical() << service.folderStatus(); return 2; }
    AppDefinition definition;
    definition.id = "kodi"; definition.name = "Kodi"; definition.type = "desktop";
    definition.launchConfiguration.insert("desktopId", "kodi");
    DesktopSession native(definition);
    int stage = 0;
    bool success = false;
    QObject::connect(&service, &KodiService::probeRequested, &media, &KodiMedia::refresh);
    QObject::connect(&service, &KodiService::unavailable, &media, &KodiMedia::invalidate);
    QObject::connect(&media, &KodiMedia::changed, &app, [&] {
        if (media.connected()) service.markReady();
        if (stage != 2 || media.busy() || media.scanStatus().isEmpty()) return;
        QStringList titles;
        for (const auto &entry : media.movies()) titles.append(entry.toMap().value("title").toString());
        success = media.connected() && titles.contains("Isolde Existing Fixture") && titles.contains("Isolde Newly Added Fixture");
        for (auto it = hashes.begin(); it != hashes.end(); ++it) success &= hash(it.key()) == it.value();
        qInfo() << "SCAN RESULT" << titles << media.scanStatus() << "MEDIA UNCHANGED AND BOTH MOVIES FOUND:" << success;
        stage = 3;
        QTimer::singleShot(0, &app, [&] { service.setFrontend("kodi"); });
    });
    QObject::connect(&service, &KodiService::ready, &app, [&] {
        if (stage != 1) return;
        // A missing new title before Scan proves this was not an automatic startup scan.
        for (const auto &entry : media.movies()) if (entry.toMap().value("title") == "Isolde Newly Added Fixture") {
            qCritical() << "New movie was already scanned; fixture cannot prove manual scan"; app.exit(1); return;
        }
        stage = 2;
        QTimer::singleShot(0, &app, [&] { media.scanMovies(); });
    });
    QObject::connect(&service, &KodiService::changed, &app, [&] {
        qInfo() << service.status();
        if (stage == 3 && service.frontend() == "kodi" && !service.switching()) app.exit(success ? 0 : 1);
    });
    QObject::connect(&native, &AppSession::stateChanged, &app, [&](AppSession::State state) {
        if (state != AppSession::State::Exited || stage != 0) return;
        if (!native.errorString().isEmpty()) { qCritical() << native.errorString(); app.exit(1); return; }
        QTimer::singleShot(0, &app, [&] {
            if (!movie("Isolde Newly Added Fixture")) { app.exit(1); return; }
            stage = 1; service.setEnabled(true); service.setFrontend("isolde");
        });
    });
    QTimer::singleShot(300000, &app, [&] { qCritical() << "Acceptance timed out"; app.exit(1); });
    native.launch();
    qInfo() << "Configure test source: F8, select Isolde Library Test, context menu Set content, Movies, Local information only, Scan recursively. Confirm scan, then F12 to exit.";
    return app.exec();
}
