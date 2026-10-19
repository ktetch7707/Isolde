#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

// Deliberately independent of profiles, app catalog, native sessions and Chromium.
// A broken profile or browser startup must not prevent recovery from opening.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("Isolde Recovery");
    const auto args = app.arguments();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("previousAvailable", args.contains("--previous"));
    engine.rootContext()->setContextProperty("recoveryError", args.contains("--error")
        ? QStringLiteral("That action could not be completed. Your profile data has not been reset. Try again or contact support.") : QString());
    QObject::connect(&engine, &QQmlApplicationEngine::exit, &app, [](int code) { QCoreApplication::exit(code); });
    engine.load(QUrl("qrc:/recovery/Recovery.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
