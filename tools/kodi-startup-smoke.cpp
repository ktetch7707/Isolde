#include "media/KodiService.h"
#include "media/KodiMedia.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Isolde");
    if (app.arguments().size() < 2 || app.arguments().size() > 3) return 2;
    const bool scan = app.arguments().size() == 3 && app.arguments().at(2) == "--scan";
    bool scanStarted = false;
    const auto profile = app.arguments().at(1);
    KodiService service(profile);
    KodiMedia media(profile);
    QObject::connect(&service, &KodiService::changed, &app, [&] { qInfo() << service.status(); });
    QObject::connect(&service, &KodiService::probeRequested, &media, &KodiMedia::refresh);
    QObject::connect(&service, &KodiService::unavailable, &media, &KodiMedia::invalidate);
    QObject::connect(&media, &KodiMedia::changed, &app, [&] {
        if (!media.busy()) qInfo() << media.status();
        if (media.connected()) service.markReady();
        if (scan && scanStarted && !media.busy() && !media.scanStatus().isEmpty()) {
            qInfo() << media.scanStatus();
            const bool success = media.connected() && (media.scanStatus().contains("accepted") || media.scanStatus().contains("finished scanning"));
            // Defer stop: it emits changed() through invalidation.
            scanStarted = false;
            QTimer::singleShot(0, &app, [&, success] { service.stop(); app.exit(success ? 0 : 1); });
        }
    });
    QObject::connect(&service, &KodiService::ready, &app, [&] {
        if (scan) {
            QTimer::singleShot(0, &app, [&] { scanStarted = true; media.scanMovies(); });
            return;
        }
        QTimer::singleShot(5000, &app, [&] {
            service.stop();
            if (media.connected() || media.busy() || !media.movies().isEmpty()) { app.exit(1); return; }
            qInfo() << "AUTOSTART AND STOP INVALIDATION PASS";
            app.exit(0);
        });
    });
    QTimer::singleShot(35000, &app, [&] { service.stop(); app.exit(1); });
    service.start();
    return app.exec();
}
