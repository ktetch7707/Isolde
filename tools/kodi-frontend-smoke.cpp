// Opt-in real-Kodi acceptance using a NEW disposable profile only.
// Native Kodi is exited through its fixture F12 keymap, never a forced mode handoff.
#include "media/KodiService.h"
#include "media/KodiMedia.h"
#include "sessions/DesktopSession.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    const auto home = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/native-profiles/" + profile + "/kodi/home";
    if (QFileInfo::exists(home)) { qCritical() << "Refusing existing profile"; return 2; }
    const auto userdata = home + "/.kodi/userdata/";
    QMap<QString, QByteArray> fixtures{
        {"favourites.xml", "<favourites><favourite name=\"Preservation fixture\">ActivateWindow(Videos)</favourite></favourites>"},
        {"sources.xml", "<sources><video><default pathversion=\"1\"></default><source><name>Preservation fixture</name><path pathversion=\"1\">/media/preservation-fixture/</path><allowsharing>false</allowsharing></source></video></sources>"},
        {"addon_data/isolde.preservation.fixture/settings.xml", "<settings version=\"2\"><setting id=\"preservation.fixture\">keep-me</setting></settings>"},
        {"keymaps/isolde-test.xml", "<keymap><global><keyboard><f12>Quit()</f12></keyboard></global></keymap>"}
    };
    const auto write = [&](const QString &name, const QByteArray &bytes) {
        QDir().mkpath(QFileInfo(userdata + name).absolutePath());
        QFile file(userdata + name);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    };
    for (auto it = fixtures.begin(); it != fixtures.end(); ++it) if (!write(it.key(), it.value())) return 2;
    if (!write("guisettings.xml", "<settings version=\"2\"><setting id=\"lookandfeel.skin\">skin.estuary</setting><setting id=\"lookandfeel.skincolors\">maroon.xml</setting></settings>")) return 2;
    app.setProperty("isoldeProfile", profile);
    KodiService service(profile);
    KodiMedia media(profile);
    AppDefinition definition;
    definition.id = "kodi"; definition.name = "Kodi"; definition.type = "desktop";
    definition.launchConfiguration.insert("desktopId", "kodi");
    DesktopSession native(definition);
    int phase = 0;
    QObject::connect(&service, &KodiService::probeRequested, &media, &KodiMedia::refresh);
    QObject::connect(&service, &KodiService::unavailable, &media, &KodiMedia::invalidate);
    QObject::connect(&media, &KodiMedia::changed, &service, [&] { if (media.connected()) service.markReady(); });
    QObject::connect(&service, &KodiService::ready, &app, [&] {
        QTimer::singleShot(2000, &app, [&] { if (phase == 2) phase = 3; service.setFrontend("kodi"); });
    });
    QObject::connect(&service, &KodiService::changed, &app, [&] {
        qInfo() << service.status();
        if (service.frontend() != "kodi" || service.switching()) return;
        if (phase == 0) {
            phase = 1;
            QTimer::singleShot(0, &app, [&] {
                service.suspend(true);
                native.launch();
                qInfo() << "NATIVE KODI READY: press F12 in the VM for a normal Kodi exit";
            });
        } else if (phase == 3) {
            phase = 4;
            QTimer::singleShot(0, &app, [&] {
                bool preserved = true;
                for (auto it = fixtures.begin(); it != fixtures.end(); ++it) {
                    QFile file(userdata + it.key());
                    preserved &= file.open(QIODevice::ReadOnly) && file.readAll() == it.value();
                }
                QFile settings(userdata + "guisettings.xml");
                const bool opened = settings.open(QIODevice::ReadOnly);
                const auto bytes = settings.readAll();
                preserved &= opened && bytes.contains(">skin.estuary</setting>") && bytes.contains(">maroon.xml</setting>");
                qInfo() << (preserved ? "FRONTEND ROUND TRIP PRESERVED CUSTOM COLOR AND FIXTURES" : "PRESERVATION FAILED");
                app.exit(preserved ? 0 : 1);
            });
        }
    });
    QObject::connect(&native, &AppSession::stateChanged, &app, [&](AppSession::State state) {
        if (state != AppSession::State::Exited || phase != 1) return;
        if (!native.errorString().isEmpty()) { qCritical() << native.errorString(); app.exit(1); return; }
        phase = 2;
        QTimer::singleShot(0, &app, [&] { service.suspend(false); service.setFrontend("isolde"); });
    });
    QTimer::singleShot(120000, &app, [&] { qCritical() << "Round trip timed out"; app.exit(1); });
    service.start();
    return app.exec();
}
