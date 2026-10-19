#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
#include <functional>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

int main(int argc, char **argv)
{
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    if (app.arguments().size() == 1) {
        QTemporaryDir storage;
        if (!storage.isValid()) return 1;
        QTemporaryDir switchingStorage;
        QProcess switching;
        switching.setProcessChannelMode(QProcess::ForwardedChannels);
        switching.start(app.applicationFilePath(), {switchingStorage.path(), "cycle"});
        if (!switching.waitForFinished(45000) || switching.exitStatus() != QProcess::NormalExit || switching.exitCode() != 0) return 1;
        for (const auto &phase : {"A-write", "B-write", "A-read", "B-read"}) {
            QProcess process;
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            process.start(app.applicationFilePath(), {storage.path(), phase});
            if (!process.waitForFinished(45000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return 1;
        }
        return 0;
    }
    const auto phases = app.arguments().at(2) == "cycle" ? QStringList{"A-write", "B-write", "A-read", "B-read"} : QStringList{app.arguments().at(2)};
    app.setQuitOnLastWindowClosed(false);
    std::unique_ptr<QQmlApplicationEngine> engine;
    int index = 0;
    std::function<void()> next = [&] {
        engine.reset();
        if (index == phases.size()) { app.exit(0); return; }
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("testRoot", app.arguments().at(1));
        engine->rootContext()->setContextProperty("testPhase", phases.at(index++));
        QObject::connect(engine.get(), &QQmlApplicationEngine::exit, &app, [&](int code) {
            if (code) app.exit(code);
            else QTimer::singleShot(0, &app, next);
        });
        engine->load(QUrl::fromLocalFile(QStringLiteral(PROFILE_TEST_QML)));
        if (engine->rootObjects().isEmpty()) app.exit(1);
    };
    QTimer::singleShot(35000, &app, [] { qCritical("Browser isolation test timed out"); QCoreApplication::exit(1); });
    QTimer::singleShot(0, &app, next);
    return app.exec();
}
