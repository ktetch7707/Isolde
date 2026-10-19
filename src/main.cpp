#include "app/AppRegistry.h"
#include "app/AppCatalog.h"
#include "input/InputManager.h"
#include "home/HomeLayout.h"
#include "home/ActionDispatcher.h"
#include "home/PageData.h"
#include "settings/DisplaySettings.h"
#include "settings/AudioSettings.h"
#include "settings/RemoteSettings.h"
#include "settings/AmbientSettings.h"
#include "settings/NetworkSettings.h"
#include "settings/SearchSettings.h"
#include "settings/ProfileManager.h"
#include "settings/SetupState.h"
#include "settings/ComponentHealth.h"
#include "media/KodiMedia.h"
#include "media/KodiShelves.h"
#include "media/CaptureController.h"
#include "media/KodiArtwork.h"
#include "media/KodiService.h"
#include "media/KodiImporter.h"
#include "sessions/SessionManager.h"
#include "sources/SourceManager.h"
#include "theme/ArtPackCatalog.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QQuickWindow>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <memory>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#ifndef ISOLDE_VERSION
#define ISOLDE_VERSION "0.0.1.2"
#endif

// Declaration order keeps the engine alive only while its context objects exist.
struct ProfileRuntime {
    AppRegistry registry;
    SessionManager sessions{&registry};
    AppCatalog catalog{&registry};
    HomeLayout homeLayout{&registry};
    DisplaySettings displaySettings;
    AudioSettings &audioSettings;
    CaptureController &captureController;
    RemoteSettings remoteSettings;
    AmbientSettings ambientSettings;
    NetworkSettings networkSettings;
    SearchSettings searchSettings;
    SourceManager sourceManager{&registry};
    ArtPackCatalog artPacks;
    InputManager input;
    SetupState setup;
    ComponentHealth componentHealth;
    KodiMedia kodi{qApp->property("isoldeProfile").toString()};
    KodiMedia searchMovies{qApp->property("isoldeProfile").toString()};
    KodiMedia searchShows{qApp->property("isoldeProfile").toString()};
    KodiMedia searchEpisodes{qApp->property("isoldeProfile").toString()};
    KodiService kodiService{qApp->property("isoldeProfile").toString()};
    KodiImporter kodiImporter{qApp->property("isoldeProfile").toString()};
    KodiShelves kodiShelves{qApp->property("isoldeProfile").toString(), &kodi};
    KodiArtwork kodiArtwork{qApp->property("isoldeProfile").toString()};
    PageData pageData{qApp->property("isoldeProfile").toString(), &kodi};
    ActionDispatcher actions{&registry, &sessions, &kodi, &kodiService, &catalog, &captureController};
    QQmlApplicationEngine engine;

    explicit ProfileRuntime(ProfileManager &profiles, bool initialLaunch, AudioSettings &audio, CaptureController &capture)
        : audioSettings(audio), captureController(capture) {
    if (initialLaunch && QCoreApplication::arguments().contains("--setup")) setup.restart();
    engine.rootContext()->setContextProperty(QStringLiteral("setupState"), &setup);
    engine.rootContext()->setContextProperty(QStringLiteral("componentHealth"), &componentHealth);
    engine.rootContext()->setContextProperty(QStringLiteral("searchPreferences"), &searchSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("captureController"), &captureController);
    engine.rootContext()->setContextProperty(QStringLiteral("kodiMedia"), &kodi);
    engine.rootContext()->setContextProperty(QStringLiteral("kodiImporter"), &kodiImporter);
    engine.rootContext()->setContextProperty(QStringLiteral("searchMovies"), &searchMovies);
    engine.rootContext()->setContextProperty(QStringLiteral("searchShows"), &searchShows);
    engine.rootContext()->setContextProperty(QStringLiteral("searchEpisodes"), &searchEpisodes);
    engine.rootContext()->setContextProperty(QStringLiteral("kodiService"), &kodiService);
    engine.rootContext()->setContextProperty(QStringLiteral("kodiShelves"), &kodiShelves);
    engine.rootContext()->setContextProperty(QStringLiteral("kodiArtwork"), &kodiArtwork);
    QObject::connect(&kodiService, &KodiService::unavailable, &kodiArtwork, [this](const QString &) { kodiArtwork.clear(); });
    engine.rootContext()->setContextProperty(QStringLiteral("actionDispatcher"), &actions);
    engine.rootContext()->setContextProperty(QStringLiteral("pageData"), &pageData);
    actions.setLayout(&homeLayout);
    QObject::connect(&kodiService,&KodiService::unavailable,&pageData,&PageData::invalidate);
    homeLayout.setCollections(kodi.collections());
    QObject::connect(&kodi, &KodiMedia::collectionsChanged, &homeLayout, [this] { homeLayout.setCollections(kodi.collections()); });
    homeLayout.setCaptureInputsReady(captureController.canSwitchInput(1));
    QObject::connect(&captureController, &CaptureController::changed, &homeLayout, [this] {
        homeLayout.setCaptureInputsReady(captureController.canSwitchInput(1));
    });
    QObject::connect(&kodiService, &KodiService::ready, &kodiShelves, [this] { kodiShelves.setAvailable(true); });
    QObject::connect(&kodiService, &KodiService::unavailable, &kodiShelves, [this](const QString &) { kodiShelves.setAvailable(false); });
    QObject::connect(&kodi, &KodiMedia::playbackReturned, &kodiShelves, [this](bool) {
        QTimer::singleShot(1500, &kodiShelves, [this] { kodiShelves.refresh(); });
    });
    sessions.setLaunchGuard([this](const QString &id) -> QString {
        if (id == "kodi" && !kodiService.canOpenApplication())
            return "Switch to Kodi application mode in Settings → System → Media Library first. This lets Kodi save your setup before opening its interface.";
        return {};
    });
    QObject::connect(&kodiService, &KodiService::probeRequested, &kodi, &KodiMedia::refresh);
    QObject::connect(&kodiService, &KodiService::unavailable, &kodi, &KodiMedia::invalidate);
    QObject::connect(&kodi, &KodiMedia::changed, &kodiService, [this] {
        if (kodi.connected()) kodiService.markReady();
    });
    QObject::connect(&sessions, &SessionManager::launching, &kodiService, [this](const QString &id) {
        if (id == "kodi") kodiService.suspend(true);
    });
    QObject::connect(&sessions, &SessionManager::returnedHome, &kodiService, [this](const QString &id) {
        if (id == "kodi") kodiService.suspend(false);
    });
    if (!registry.load())
        qFatal("%s", qPrintable(registry.errorString()));
    if (!catalog.load())
        qWarning("%s", qPrintable(catalog.errorString()));
    if (!homeLayout.load({}, profiles.layoutPath()))
        qFatal("%s", qPrintable(homeLayout.errorString()));
    QObject::connect(&catalog, &AppCatalog::packageInstalled, &homeLayout, &HomeLayout::addAppTile);
    // Keep user launchers repairable when a package disappears. Registry changes
    // update availability; reinstalling reconnects the same stable binding.
    if (!artPacks.load())
        qWarning("%s", qPrintable(artPacks.errorString()));
    engine.rootContext()->setContextProperty(QStringLiteral("profileManager"), &profiles);
    // Immutable for this engine's lifetime: never retarget a live browser profile.
    engine.rootContext()->setContextProperty(QStringLiteral("browserStorageName"), "isolde-" + profiles.activeId());
    engine.rootContext()->setContextProperty(QStringLiteral("appRegistry"), &registry);
    engine.rootContext()->setContextProperty(QStringLiteral("sessionManager"), &sessions);
    engine.rootContext()->setContextProperty(QStringLiteral("appCatalog"), &catalog);
    engine.rootContext()->setContextProperty(QStringLiteral("homeLayout"), &homeLayout);
    engine.rootContext()->setContextProperty(QStringLiteral("displaySettings"), &displaySettings);
    engine.rootContext()->setContextProperty(QStringLiteral("audioSettings"), &audioSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("remoteSettings"), &remoteSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("ambientSettings"), &ambientSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("networkSettings"), &networkSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("sourceManager"), &sourceManager);
    engine.rootContext()->setContextProperty(QStringLiteral("artPacks"), &artPacks);
    engine.rootContext()->setContextProperty(QStringLiteral("inputManager"), &input);
    engine.rootContext()->setContextProperty(QStringLiteral("isoldeVersion"), QCoreApplication::applicationVersion());
    engine.loadFromModule(QStringLiteral("Isolde"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        qFatal("Could not load the Isolde interface");
    const auto presentKodi = [this](bool show) {
        // Supported X11/XWayland host only. Require exactly one Kodi window;
        // never activate an arbitrary match or execute a shell command.
        auto *query = new QProcess(&engine);
        auto environment = QProcessEnvironment::systemEnvironment();
        const QDir runtime(environment.value("XDG_RUNTIME_DIR"));
        const auto auth = runtime.entryList({".mutter-Xwaylandauth.*"}, QDir::Files | QDir::Hidden, QDir::Time);
        if (!auth.isEmpty()) environment.insert("XAUTHORITY", runtime.filePath(auth.first()));
        query->setProcessEnvironment(environment);
        QObject::connect(query, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &engine,
            [this, query, show](int code, QProcess::ExitStatus) {
                query->deleteLater();
                if (show && code != 0 && kodi.playbackActive()) kodi.presentationFailed();
                if (!show && !kodi.playbackActive()) {
                    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
                        window->raise(); window->requestActivate();
                    }
                }
            });
        QObject::connect(query, &QProcess::errorOccurred, &engine, [this, query, show](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) { query->deleteLater(); if (show) kodi.presentationFailed(); }
        });
        auto helper = QCoreApplication::applicationDirPath() + "/../share/isolde/kodi-window.py";
        if (!QFileInfo::exists(helper)) helper = QCoreApplication::applicationDirPath() + "/share/isolde/kodi-window.py";
        query->start("python3", {helper, show ? "show" : "hide"});
        QTimer::singleShot(6000, query, [query] { if (query->state() != QProcess::NotRunning) query->kill(); });
    };
    QObject::connect(&kodi, &KodiMedia::playbackStarted, &engine, [presentKodi] { presentKodi(true); });
    QObject::connect(&kodi, &KodiMedia::playbackReturned, &engine, [this, presentKodi](bool) {
        if (kodi.playbackStatus() != "replaced") presentKodi(false);
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
            window->raise(); window->requestActivate();
        }
    });
    QObject::connect(&kodiService, &KodiService::ready, &engine, [this, presentKodi] {
        if (sessions.hasActiveSession()) return;
        presentKodi(false);
        QTimer::singleShot(3500, &engine, [this, presentKodi] { if (!kodi.playbackActive() && !sessions.hasActiveSession()) presentKodi(false); });
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
            window->raise();
            window->requestActivate();
        }
    });
    QObject::connect(&setup, &SetupState::changed, &kodiService, [this] { if (setup.complete()) kodiService.start(); });
    if (setup.complete()) QTimer::singleShot(1500, &kodiService, &KodiService::start);
    const QStringList arguments = QCoreApplication::arguments();
    for (const auto &argument : arguments) {
        if (initialLaunch && setup.complete() && argument.startsWith(QStringLiteral("--launch-app="))) {
            const auto appId = argument.mid(QStringLiteral("--launch-app=").size()).trimmed();
            if (!appId.isEmpty())
                QTimer::singleShot(500, &sessions, [this, appId] {
                    if (appId == "movies" || appId == "shows") actions.dispatch({{"type", "launch-app"}, {"appId", appId}});
                    else sessions.launch(appId);
                });
            break;
        }
    }
    }
    ~ProfileRuntime() {
        QObject::disconnect(&sessions, nullptr, &kodiService, nullptr);
        kodiService.stop();
        sessions.exitCurrent();
    }
};

int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Isolde"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ISOLDE_VERSION));
    app.setQuitOnLastWindowClosed(false);
    bool switching = false;
    QQuickWindow switchCover;
    switchCover.setColor(QColor("#0B1020"));
    switchCover.setFlags(Qt::Window | Qt::FramelessWindowHint);
    ProfileManager profiles;
    AudioSettings audio;
    CaptureController captureController;
    audio.initialize();
    auto runtime = std::make_unique<ProfileRuntime>(profiles, true, audio, captureController);
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, [&] {
        if (!switching) app.quit();
    });
    QObject::connect(&profiles, &ProfileManager::switchRequested, &app, [&] {
        if (switching) return;
        switching = true;
        // Cover the desktop while the outgoing browser and interface are destroyed.
        const auto onceQueued = Qt::ConnectionType(Qt::SingleShotConnection | Qt::QueuedConnection);
        QObject::connect(&switchCover, &QQuickWindow::frameSwapped, &app, [&, onceQueued] {
            runtime.reset();
            runtime = std::make_unique<ProfileRuntime>(profiles, false, audio, captureController);
            auto *window = qobject_cast<QQuickWindow *>(runtime->engine.rootObjects().first());
            QObject::connect(window, &QQuickWindow::frameSwapped, &app, [&] {
                switchCover.hide();
                switching = false;
            }, onceQueued);
            window->requestUpdate();
        }, onceQueued);
        switchCover.showFullScreen();
        switchCover.raise();
        switchCover.requestUpdate();
    });
    return app.exec();
}
