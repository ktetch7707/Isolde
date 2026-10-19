// Manual VM smoke runner: launches an installed app using the production bridge,
// then asks it to exit. Does not change the user's selected Isolde profile.
#include "app/AppCatalog.h"
#include "app/AppRegistry.h"
#include "sessions/DesktopSession.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Isolde");
    app.setApplicationVersion("0.0.1.2");
    const auto args = app.arguments();
    if (args.size() != 4) { qCritical("Usage: isolde_native_smoke APP PROFILE SECONDS"); return 2; }
    bool validSeconds = false;
    const int seconds = args.at(3).toInt(&validSeconds);
    if (!validSeconds || seconds < 1 || seconds > 60) return 2;
    app.setProperty("isoldeProfile", args.at(2));
    AppRegistry registry;
    if (!registry.load()) { qCritical() << registry.errorString(); return 2; }
    AppCatalog catalog(&registry);
    if (!catalog.load()) { qCritical() << catalog.errorString(); return 2; }
    const auto *definition = registry.find(args.at(1));
    if (!definition || definition->type != "desktop") return 2;
    DesktopSession session(*definition);
    bool controlledStop = false;
    QObject::connect(&session, &AppSession::stateChanged, &app, [&](AppSession::State state) {
        qInfo() << "Native smoke state" << int(state);
        if (state == AppSession::State::Exited) {
            qInfo().noquote() << session.diagnostics();
            qInfo() << "Native smoke result" << (controlledStop && session.errorString().isEmpty() ? "PASS" : "FAIL") << session.errorString();
            QTimer::singleShot(0, &app, [&] { app.exit(controlledStop && session.errorString().isEmpty() ? 0 : 1); });
        }
    });
    QTimer::singleShot(0, &session, [&] { session.launch(); });
    QTimer::singleShot(seconds * 1000, &session, [&] { controlledStop = true; session.exit(); });
    return app.exec();
}
