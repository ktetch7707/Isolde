// Runs inside the native app namespace. The owning shell requests a graceful
// shutdown over stdin; namespace teardown remains the final containment boundary.
#include <QCoreApplication>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include "sessions/ChromiumRemotePipe.h"
#include <memory>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    #ifdef Q_OS_LINUX
    ::signal(SIGPIPE,SIG_IGN);
    #endif
    auto arguments = app.arguments();
    QVariantMap remoteAppearance;
    if(arguments.size()>3 && arguments.at(1)=="--web-remote"){
        if(arguments.at(2).size()>16384)return 2;
        remoteAppearance=QJsonDocument::fromJson(QByteArray::fromBase64(arguments.at(2).toLatin1())).toVariant().toMap();
        arguments.removeAt(1);arguments.removeAt(1);
        if(remoteAppearance.isEmpty())return 2;
    }
    if (arguments.size() < 3 || arguments.at(1) != "--") return 2;
    arguments.removeFirst(); arguments.removeFirst();
    const auto program = arguments.takeFirst();
    QProcess child;
    child.setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_UNIX
    child.setChildProcessModifier([] { ::setsid(); });
#endif
    bool stopping = false;
    bool remoteFailed=false;
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
                app.exit(remoteFailed?1:0);
            });
        });
    #ifdef Q_OS_LINUX
    std::unique_ptr<ChromiumRemotePipe> remote;
    if(!remoteAppearance.isEmpty()){
        const auto name=QFileInfo(program).fileName();
        if(!QStringList{"google-chrome-stable","google-chrome","chrome","chromium","chromium-browser"}.contains(name))return 2;
        if(qEnvironmentVariable("ISOLDE_NATIVE_NAMESPACE")!="1")return 2;
        // A separate app-private browser profile. Existing Chrome profiles are
        // never inspected, copied, renamed, or supplied to the control bridge.
        const auto data=QDir::homePath()+"/.config/isolde-remote-web";
        if(QFileInfo(data).isSymLink() || QFileInfo(QFileInfo(data).absolutePath()).isSymLink())return 2;
        if(!QDir().mkpath(data))return 2;
        arguments<<"--remote-debugging-pipe"<<"--user-data-dir="+data;
        remote=std::make_unique<ChromiumRemotePipe>(app,child,remoteAppearance,[&]{remoteFailed=true;stop();});
        if(!remote->valid)return 2;
    }
    #endif
    child.start(program, arguments);
    if (!child.waitForStarted(5000)) return 127;
    childGroup = child.processId();
    #ifdef Q_OS_LINUX
    if(remote)remote->started();
    #endif
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
