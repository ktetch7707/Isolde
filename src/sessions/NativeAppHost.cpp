// Runs inside the native app namespace. The owning shell requests a graceful
// shutdown over stdin; namespace teardown remains the final containment boundary.
#include <QCoreApplication>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include "sessions/ChromiumRemotePipe.h"
#include "sessions/GamepadTopology.h"
#include <memory>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace {
bool gamepadKeyBitmap(const QByteArray &bitmap)
{
    const auto words=QString::fromLatin1(bitmap).trimmed().split(' ',Qt::SkipEmptyParts);
    if(words.isEmpty() || words.size()>512)return false;
    for(int code=0x120;code<0x140;++code){
        const auto word=words.size()-1-code/64;
        if(word<0)continue;
        bool ok=false;
        const auto bits=words.at(word).toULongLong(&ok,16);
        if(ok && (bits&(quint64(1)<<(code%64))))return true;
    }
    return false;
}

QSet<QString> connectedGamepadEvents()
{
    QSet<QString> result;
    const QDir input("/sys/class/input");
    for(const auto &name:input.entryList({"event*"},QDir::Dirs|QDir::System|QDir::NoDotAndDotDot,QDir::Name)){
        if(!QRegularExpression("^event[0-9]{1,3}$").match(name).hasMatch())continue;
        QFile capabilities(input.filePath(name+"/device/capabilities/key"));
        if(!capabilities.open(QIODevice::ReadOnly))continue;
        const auto bitmap=capabilities.read(4097);
        if(bitmap.size()<=4096 && gamepadKeyBitmap(bitmap)){
            result.insert(name);
            if(result.size()>=32)break;
        }
    }
    return result;
}

void reportGamepadStatus(const QVariantMap &status)
{
#ifdef Q_OS_UNIX
    const auto json=QJsonDocument::fromVariant(status).toJson(QJsonDocument::Compact);
    std::fprintf(stderr,"ISOLDE_GAMEPAD_STATUS %s\n",json.constData());
    std::fflush(stderr);
#else
    Q_UNUSED(status);
#endif
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    #ifdef Q_OS_LINUX
    ::signal(SIGPIPE,SIG_IGN);
    #endif
    auto arguments = app.arguments();
    const bool steamSession = arguments.size()>1 && arguments.at(1)=="--steam-session";
    if(steamSession) arguments.removeAt(1);
    const bool gameSession=arguments.size()>1 && arguments.at(1)=="--game-session";
    if(gameSession)arguments.removeAt(1);
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
    QTimer gamepadTimer;
    if(gameSession){
        QSet<QString> boundAtLaunch;
        const auto bound=qEnvironmentVariable("ISOLDE_GAMEPAD_BOUND_EVENTS").split(',',Qt::SkipEmptyParts);
        for(const auto &name:bound)
            if(QRegularExpression("^event[0-9]{1,3}$").match(name).hasMatch())boundAtLaunch.insert(name);
        QVariantMap lastStatus;
        const auto updateGamepads=[&]{
            const auto status=GamepadTopology::summarize(connectedGamepadEvents(),boundAtLaunch);
            if(status!=lastStatus){lastStatus=status;reportGamepadStatus(status);}
        };
        gamepadTimer.setInterval(1000);
        QObject::connect(&gamepadTimer,&QTimer::timeout,&app,updateGamepads);
        updateGamepads();
        gamepadTimer.start();
    }
    const auto stop = [&] {
        if (stopping) return;
        stopping = true;
#ifdef Q_OS_UNIX
        // Let the main application coordinate its own worker shutdown first.
        if (child.state() != QProcess::NotRunning) ::kill(pid_t(child.processId()), SIGTERM);
#else
        child.terminate();
#endif
        QTimer::singleShot(steamSession ? 14000 : 2000, &app, [&] {
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
            QTimer::singleShot(1500, &app, [&,code,status] {
#ifdef Q_OS_UNIX
                ::kill(-pid_t(childGroup), SIGKILL);
#endif
                app.exit(remoteFailed || (steamSession && (status!=QProcess::NormalExit || code!=0)) ? 1 : 0);
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
