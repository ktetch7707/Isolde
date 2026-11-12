// Manual VM smoke runner: launches an installed app using the production bridge,
// then asks it to exit. Does not change the user's selected Isolde profile.
#include "app/AppCatalog.h"
#include "app/AppRegistry.h"
#include "sessions/DesktopSession.h"
#include "sessions/NativeSandbox.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QUrl>
#include <unistd.h>
#include <QTimer>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Isolde");
    app.setApplicationVersion("0.0.1.2");
    const auto args = app.arguments();
    if (args.size()==4 && args.at(1)=="--remote-fixture") {
        const QUrl url(args.at(2));
        if(url.scheme()!="http" || url.host()!="127.0.0.1" || url.port()<1024 || !url.userInfo().isEmpty())return 2;
        const auto plan=NativeSandbox::prepare(args.at(3),"remote-fixture","/usr/bin/google-chrome-stable",QProcessEnvironment::systemEnvironment());
        if(!plan.error.isEmpty()){qCritical().noquote()<<plan.error;return 2;}
        auto launch=plan.arguments;const auto index=launch.lastIndexOf("--");
        if(index<0)return 2;
        const QVariantMap appearance{{"focusColor","#a9326e"},{"surfaceColor","#f7edf4"},{"textColor","#332c44"},{"cornerRadius",12},{"typographyScale",1}};
        launch.insert(index,"--web-remote");launch.insert(index+1,QString::fromLatin1(QJsonDocument::fromVariant(appearance).toJson(QJsonDocument::Compact).toBase64()));
        launch<<"--ozone-platform=x11"<<"--no-first-run"<<"--no-default-browser-check"<<"--disable-background-networking"<<"--disable-component-update"<<"--window-size=1000,720"<<"--app="+url.toString();
        QProcess child;child.setProcessChannelMode(QProcess::ForwardedChannels);child.setProcessEnvironment(plan.environment);
        child.start(plan.program,launch);if(!child.waitForStarted(5000))return 2;
        bool stopping=false,expectReturn=false;QByteArray input;
        QSocketNotifier reader(STDIN_FILENO,QSocketNotifier::Read);
        QObject::connect(&reader,&QSocketNotifier::activated,&app,[&]{char bytes[32];const auto n=::read(STDIN_FILENO,bytes,sizeof(bytes));if(n<=0){reader.setEnabled(false);return;}input.append(bytes,n);if(input.size()>128){child.kill();app.exit(2);}if(input.contains("expect-return\n"))expectReturn=true;if(input.contains("quit\n")&&!stopping){stopping=true;child.write("quit\n");}});
        QObject::connect(&child,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&app,[&](int code,QProcess::ExitStatus status){app.exit((stopping||expectReturn)&&status==QProcess::NormalExit&&code==0?0:1);});
        QTimer::singleShot(45000,&app,[&]{child.kill();app.exit(1);});
        const auto result=app.exec();if(child.state()!=QProcess::NotRunning){child.kill();child.waitForFinished(5000);}return result;
    }
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
