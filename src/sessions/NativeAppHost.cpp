// Runs inside the native app namespace. The owning shell requests a graceful
// shutdown over stdin; namespace teardown remains the final containment boundary.
#include <QCoreApplication>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto arguments = app.arguments();
    if (arguments.size() < 3 || arguments.at(1) != "--") return 2;
    arguments.removeFirst(); arguments.removeFirst();
    const auto program = arguments.takeFirst();
    QProcess child;
    child.setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_UNIX
    child.setChildProcessModifier([] { ::setsid(); });
#endif
    bool stopping = false;
    qint64 childGroup = 0;
    const auto stop = [&] {
        if (stopping) return;
        stopping = true;
#ifdef Q_OS_UNIX
        // Let the main application coordinate its own worker shutdown first.
        if (child.state() != QProcess::NotRunning) ::kill(pid_t(child.processId()), SIGTERM);
#else
        child.terminate();
#endif
        QTimer::singleShot(2000, &app, [&] {
#ifdef Q_OS_UNIX
            if (child.state() != QProcess::NotRunning) ::kill(-pid_t(childGroup), SIGKILL);
#else
            child.kill();
#endif
        });
    };
    QObject::connect(&child, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &app,
        [&](int code, QProcess::ExitStatus status) {
            if (!stopping) { app.exit(status == QProcess::NormalExit ? code : 1); return; }
#ifdef Q_OS_UNIX
            // A launcher script may have exited before the real app underneath it.
            ::kill(-pid_t(childGroup), SIGTERM);
#endif
            QTimer::singleShot(1500, &app, [&] {
#ifdef Q_OS_UNIX
                ::kill(-pid_t(childGroup), SIGKILL);
#endif
                app.exit(0);
            });
        });
    child.start(program, arguments);
    if (!child.waitForStarted(5000)) return 127;
    childGroup = child.processId();
#ifdef Q_OS_UNIX
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
    QByteArray commands;
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&] {
        char bytes[64];
        const auto count = ::read(STDIN_FILENO, bytes, sizeof(bytes));
        if (count <= 0) { input.setEnabled(false); stop(); return; }
        commands.append(bytes, count);
        if (commands.contains("quit\n")) stop();
        if (commands.size() > 128) commands.clear();
    });
#endif
    return app.exec();
}
