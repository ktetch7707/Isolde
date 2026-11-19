// Runs inside the native app namespace. The owning shell requests a graceful
// shutdown over stdin; namespace teardown remains the final containment boundary.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSocketNotifier>
#include <QTimer>
#include <memory>
#include <functional>
#include "sessions/ChromiumRemotePipe.h"
#include "sessions/GamepadTopology.h"
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#endif
#ifdef Q_OS_LINUX
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <cerrno>
#endif

namespace
{
bool
verifiedGamepadNode(const QDir& input, const QString& name)
{
    QFile capabilities(input.filePath(name + "/device/capabilities/key"));
    QFile device(input.filePath(name + "/dev"));
    if (!capabilities.open(QIODevice::ReadOnly) || !device.open(QIODevice::ReadOnly))
        return false;
    const auto bitmap = capabilities.read(4097), number = device.read(32);
    return bitmap.size() <= 4096 && number.size() < 32 &&
           GamepadTopology::verifiedGamepadNode(
               name, QString::fromLatin1(bitmap).trimmed(), QString::fromLatin1(number).trimmed());
}

QSet<QString>
connectedGamepadEvents()
{
    QSet<QString> result;
    const QDir input("/sys/class/input");
    for (const auto& name : input.entryList({"event*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot, QDir::Name))
    {
        if (verifiedGamepadNode(input, name))
        {
            result.insert(name);
            if (result.size() >= 32)
                break;
        }
    }
    return result;
}

QSet<QString>
connectedGamepadNodes()
{
    QSet<QString> result;
    const QDir input("/sys/class/input");
    for (const auto& name :
         input.entryList({"event*", "js*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot, QDir::Name))
    {
        if (verifiedGamepadNode(input, name))
            result.insert(name);
        if (result.size() >= 32)
            break;
    }
    return result;
}

QSet<QString>
attachGamepadNodes(const QSet<QString>& connected,
                   const QSet<QString>& boundAtLaunch,
                   QMap<QString, quint64>* createdNodes);
bool
attachGamepadNode(const QSet<QString>& connected,
                  const QSet<QString>& boundAtLaunch,
                  QMap<QString, quint64>* createdNodes,
                  QSet<QString>& dynamicEvents,
                  const QDir& sysInput,
                  const QString& name);
bool
attachExistingGamepadNode(QMap<QString, quint64>::iterator& it,
                          QMap<QString, quint64>* createdNodes,
                          const QSet<QString>& connected,
                          QSet<QString>& dynamicEvents);

QSet<QString>
attachGamepadNodes(const QSet<QString>& connected,
                   const QSet<QString>& boundAtLaunch,
                   QMap<QString, quint64>* createdNodes)
{
    QSet<QString> dynamicEvents;
#ifdef Q_OS_LINUX
    const QDir sysInput("/sys/class/input");
    for (auto it = createdNodes->begin(); it != createdNodes->end();)
    {
        if (attachExistingGamepadNode(it, createdNodes, connected, dynamicEvents))
            ++it;
        else
            it = createdNodes->erase(it);
    }
    for (const auto& name : connected)
    {
        attachGamepadNode(connected, boundAtLaunch, createdNodes, dynamicEvents, sysInput, name);
    }
#else
    Q_UNUSED(connected);
    Q_UNUSED(boundAtLaunch);
    Q_UNUSED(createdNodes);
#endif
    return dynamicEvents;
}

bool
attachGamepadNode(const QSet<QString>& connected,
                  const QSet<QString>& boundAtLaunch,
                  QMap<QString, quint64>* createdNodes,
                  QSet<QString>& dynamicEvents,
                  const QDir& sysInput,
                  const QString& name)
{
    if (boundAtLaunch.contains(name) || createdNodes->contains(name))
        return false;
    QFile deviceInfo(sysInput.filePath(name + "/dev"));
    if (!deviceInfo.open(QIODevice::ReadOnly))
        return false;
    uint major = 0, minor = 0;
    if (!GamepadTopology::deviceNumber(name, QString::fromLatin1(deviceInfo.read(32)).trimmed(), &major, &minor))
        return false;
    const auto path = QString("/dev/input/") + name;
    const auto encodedPath = path.toLocal8Bit();
    struct stat info{};
    if (::lstat(encodedPath.constData(), &info) == 0 || errno != ENOENT)
        return false;
    const auto number = ::makedev(major, minor);
    if (::mknod(encodedPath.constData(), S_IFCHR | S_IRUSR | S_IWUSR, number) != 0)
        return false;
    if (::lstat(encodedPath.constData(), &info) == 0 && S_ISCHR(info.st_mode) && info.st_rdev == number)
    {
        createdNodes->insert(name, quint64(number));
        if (name.startsWith("event"))
            dynamicEvents.insert(name);
        return true;
    }
    ::unlink(encodedPath.constData());
    return false;
}

bool
attachExistingGamepadNode(QMap<QString, quint64>::iterator& it,
                          QMap<QString, quint64>* createdNodes,
                          const QSet<QString>& connected,
                          QSet<QString>& dynamicEvents)
{
    const auto path = QString("/dev/input/") + it.key();
    struct stat info{};
    const auto encodedPath = path.toLocal8Bit();
    const bool sameDevice =
        ::lstat(encodedPath.constData(), &info) == 0 && S_ISCHR(info.st_mode) && info.st_rdev == dev_t(it.value());
    if (connected.contains(it.key()))
    {
        if (sameDevice && it.key().startsWith("event"))
            dynamicEvents.insert(it.key());
        if (sameDevice)
        {
            ++it;
            return true;
        }
    }
    else if (sameDevice)
    {
        ::unlink(encodedPath.constData());
    }
    return false;
}

void
reportGamepadStatus(const QVariantMap& status)
{
#ifdef Q_OS_UNIX
    const auto json = QJsonDocument::fromVariant(status).toJson(QJsonDocument::Compact);
    std::fprintf(stderr, "ISOLDE_GAMEPAD_STATUS %s\n", json.constData());
    std::fflush(stderr);
#else
    Q_UNUSED(status);
#endif
}
} // namespace

struct
LaunchConfig
{
    bool steamSession = false;
    bool gameSession = false;
    QVariantMap remoteAppearance;
    QString program;
    QStringList arguments;
};

bool
validateLaunchPreconditions(bool gameSession, const QStringList& arguments);
LaunchConfig
parseLaunchArguments(QCoreApplication& app);

QVariantMap
parseWebRemoteAppearance(const QStringList& arguments);

void
configureChildProcess(QProcess& child, bool gameSession);

void
setupGamepadMonitoring(QCoreApplication& app, QTimer& gamepadTimer, bool gameSession,
                       QSet<QString>& boundAtLaunch, QVariantMap& lastStatus,
                       QMap<QString, quint64>& createdNodes);

// Keep the session mode in the callback so deferred shutdown uses a live value.
auto
makeStopHandler(QProcess& child, QCoreApplication& app, qint64& childGroup,
                 bool& stopping, bool steamSession)
{
    return [&, steamSession]
    {
        if (stopping)
            return;
        stopping = true;
#ifdef Q_OS_UNIX
        if (child.state() != QProcess::NotRunning)
            ::kill(pid_t(child.processId()), SIGTERM);
#else
        child.terminate();
#endif
        QTimer::singleShot(steamSession ? 14000 : 2000,
                           &app,
                           [&]
                           {
#ifdef Q_OS_UNIX
                               if (child.state() != QProcess::NotRunning)
                                   ::kill(-pid_t(childGroup), SIGKILL);
#else
                               child.kill();
#endif
                           });
    };
}

// Preserve the child result and session mode until delayed process-group cleanup finishes.
auto
makeChildFinishedHandler(QProcess& child, QCoreApplication& app, qint64& childGroup,
                         bool& stopping, bool& remoteFailed, bool steamSession)
{
    return [&, steamSession](int code, QProcess::ExitStatus status)
    {
        if (!stopping)
        {
            app.exit(status == QProcess::NormalExit ? code : 1);
            return;
        }
#ifdef Q_OS_UNIX
        ::kill(-pid_t(childGroup), SIGTERM);
#endif
        QTimer::singleShot(
            1500,
            &app,
            [&, code, status, steamSession]
            {
#ifdef Q_OS_UNIX
                ::kill(-pid_t(childGroup), SIGKILL);
#endif
                app.exit(remoteFailed || (steamSession && (status != QProcess::NormalExit || code != 0)) ? 1 : 0);
            });
    };
}

#ifdef Q_OS_LINUX
bool
setupChromiumRemote(QCoreApplication& app, QProcess& child, const QVariantMap& remoteAppearance,
                    const QString& program, QStringList& arguments, std::unique_ptr<ChromiumRemotePipe>& remote,
                    bool& remoteFailed, const std::function<void()>& stop);
#endif

int
main(int argc, char** argv)
{    QCoreApplication app(argc, argv);
#ifdef Q_OS_LINUX
    ::signal(SIGPIPE, SIG_IGN);
#endif
    auto config = parseLaunchArguments(app);
    bool& steamSession = config.steamSession;
    bool& gameSession = config.gameSession;
    auto& remoteAppearance = config.remoteAppearance;
    auto& program = config.program;
    auto& arguments = config.arguments;
if (!validateLaunchPreconditions(gameSession, arguments))
        return 2;
    arguments.removeFirst();
    arguments.removeFirst();
    program = arguments.takeFirst();
    QProcess child;
    configureChildProcess(child, gameSession);
    bool stopping = false;
    bool remoteFailed = false;
    qint64 childGroup = 0;
    QTimer gamepadTimer;
    QSet<QString> boundAtLaunch;
    QVariantMap lastStatus;
    QMap<QString, quint64> createdNodes;
    setupGamepadMonitoring(app, gamepadTimer, gameSession, boundAtLaunch, lastStatus, createdNodes);
    auto stop = makeStopHandler(child, app, childGroup, stopping, steamSession);
    QObject::connect(
        &child,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        &app,
        makeChildFinishedHandler(child, app, childGroup, stopping, remoteFailed, steamSession));
#ifdef Q_OS_LINUX
    std::unique_ptr<ChromiumRemotePipe> remote;
    if (!remoteAppearance.isEmpty() && !setupChromiumRemote(app, child, remoteAppearance, program, arguments, remote,
                                                            remoteFailed, stop))
        return 2;
#endif
    child.start(program, arguments);
    if (!child.waitForStarted(5000))
        return 127;
    childGroup = child.processId();
#ifdef Q_OS_LINUX
    if (remote)
        remote->started();
#endif
    #ifdef Q_OS_UNIX
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
    QByteArray commands;
    QObject::connect(&input,
                     &QSocketNotifier::activated,
                     &app,
                     [&]
                     {
                         char bytes[64];
                         const auto count = ::read(STDIN_FILENO, bytes, sizeof(bytes));
                         if (count <= 0)
                         {
                             input.setEnabled(false);
                             stop();
                             return;
                         }
                         commands.append(bytes, count);
                         if (commands.contains("quit\n"))
                             stop();
                         if (commands.size() > 128)
                             commands.clear();
                     });
#endif
    return app.exec();
}

bool
validateLaunchPreconditions(bool gameSession, const QStringList& arguments)
{
#ifdef Q_OS_LINUX
    if (gameSession && ::prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0)
        return false;
#endif
    if (arguments.size() < 3 || arguments.at(1) != "--")
        return false;
    return true;
}

LaunchConfig
parseLaunchArguments(QCoreApplication& app)
{
    LaunchConfig config;
    auto arguments = app.arguments();
    bool steamSession = false, gameSession = false;
    while (arguments.size() > 1)
    {
        const auto flag = arguments.at(1);
        if (flag == "--steam-session")
        {
            if (steamSession)
            {
                config.steamSession = true;
                return config;
            }
            steamSession = true;
        }
        else if (flag == "--game-session")
        {
            if (gameSession)
            {
                config.gameSession = true;
                return config;
            }
            gameSession = true;
        }
        else
            break;
        arguments.removeAt(1);
    }
    config.steamSession = steamSession;
    config.gameSession = gameSession;
    config.arguments = arguments;
    return config;
}

QVariantMap
parseWebRemoteAppearance(const QStringList& arguments)
{
    QVariantMap remoteAppearance;
    if (arguments.size() > 3 && arguments.at(1) == "--web-remote")
    {
        if (arguments.at(2).size() > 16384)
            return {};
        remoteAppearance =
            QJsonDocument::fromJson(QByteArray::fromBase64(arguments.at(2).toLatin1())).toVariant().toMap();
        if (remoteAppearance.isEmpty())
            return {};
    }
    return remoteAppearance;
}

void
configureChildProcess(QProcess& child, bool gameSession)
{
    child.setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_UNIX
    child.setChildProcessModifier(
        [gameSession]
        {
#ifdef Q_OS_LINUX
            if (gameSession)
            {
                if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
                    ::_exit(126);
                __user_cap_header_struct header{_LINUX_CAPABILITY_VERSION_3, 0};
                __user_cap_data_struct capabilities[2]{};
                if (::syscall(SYS_capset, &header, capabilities) != 0)
                    ::_exit(126);
            }
#else
            Q_UNUSED(gameSession);
#endif
            ::setsid();
        });
#endif
}

void
setupGamepadMonitoring(QCoreApplication& app, QTimer& gamepadTimer, bool gameSession,
                       QSet<QString>& boundAtLaunch, QVariantMap& lastStatus,
                       QMap<QString, quint64>& createdNodes)
{
    if (!gameSession)
        return;
    const auto bound = qEnvironmentVariable("ISOLDE_GAMEPAD_BOUND_EVENTS").split(',', Qt::SkipEmptyParts);
    for (const auto& name : bound)
        if (QRegularExpression("^event[0-9]{1,3}$").match(name).hasMatch())
            boundAtLaunch.insert(name);
    const auto updateGamepads = [&]
    {
        const auto connected = connectedGamepadEvents();
        const auto dynamicEvents = attachGamepadNodes(connectedGamepadNodes(), boundAtLaunch, &createdNodes);
        const auto status = GamepadTopology::summarize(connected, boundAtLaunch, dynamicEvents);
        if (status != lastStatus)
        {
            lastStatus = status;
            reportGamepadStatus(status);
        }
    };
    gamepadTimer.setInterval(1000);
    QObject::connect(&gamepadTimer, &QTimer::timeout, &app, updateGamepads);
    updateGamepads();
    gamepadTimer.start();
}

#ifdef Q_OS_LINUX
bool
setupChromiumRemote(QCoreApplication& app, QProcess& child, const QVariantMap& remoteAppearance,
                    const QString& program, QStringList& arguments, std::unique_ptr<ChromiumRemotePipe>& remote,
                    bool& remoteFailed, const std::function<void()>& stop)
{
    if (remoteAppearance.isEmpty())
        return true;
    const auto name = QFileInfo(program).fileName();
    if (!QStringList{"google-chrome-stable", "google-chrome", "chrome", "chromium", "chromium-browser"}.contains(
            name))
        return false;
    if (qEnvironmentVariable("ISOLDE_NATIVE_NAMESPACE") != "1")
        return false;
    const auto data = QDir::homePath() + "/.config/isolde-remote-web";
    if (QFileInfo(data).isSymLink() || QFileInfo(QFileInfo(data).absolutePath()).isSymLink())
        return false;
    if (!QDir().mkpath(data))
        return false;
    arguments << "--remote-debugging-pipe" << "--user-data-dir=" + data;
    remote = std::make_unique<ChromiumRemotePipe>(
        app,
        child,
        remoteAppearance,
        [&]
        {
            remoteFailed = true;
            stop();
        },
        stop);
    return remote->valid;
}
#endif

