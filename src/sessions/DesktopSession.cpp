#include "sessions/DesktopSession.h"
#include "sessions/NativeSandbox.h"
#include <QCoreApplication>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonDocument>

namespace {

struct ResolvedDesktopLaunch {
    QString desktopPath;
    QString name;
    QString execLine;
    QString program;
    QStringList arguments;
    QString error;
};

QProcessEnvironment desktopEnvironment()
{
    auto environment = QProcessEnvironment::systemEnvironment();
    const QDir runtime(environment.value(QStringLiteral("XDG_RUNTIME_DIR"), QStringLiteral("/run/user/1000")));
    const auto authFiles = runtime.entryList({QStringLiteral(".mutter-Xwaylandauth.*")},
                                             QDir::Files | QDir::Hidden, QDir::Time);
    if (!authFiles.isEmpty())
        environment.insert(QStringLiteral("XAUTHORITY"), runtime.filePath(authFiles.first()));
    return environment;
}

QStringList splitSearchPath(const QString &value, const QStringList &fallback)
{
    const auto raw = value.trimmed().isEmpty() ? fallback : value.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    QStringList cleaned;
    for (const auto &path : raw) {
        const auto trimmed = path.trimmed();
        if (!trimmed.isEmpty() && !cleaned.contains(trimmed))
            cleaned.append(trimmed);
    }
    return cleaned;
}

QString desktopFileName(QString desktopId)
{
    desktopId = desktopId.trimmed();
    if (!desktopId.endsWith(QStringLiteral(".desktop")))
        desktopId += QStringLiteral(".desktop");
    return desktopId;
}

QString resolveDesktopEntryPath(const QString &desktopId, const QProcessEnvironment &environment)
{
    const auto fileName = desktopFileName(desktopId);
    QStringList applicationDirs;

    const auto dataHome = environment.value(QStringLiteral("XDG_DATA_HOME"),
                                            QDir::homePath() + QStringLiteral("/.local/share"));
    applicationDirs.append(QDir(dataHome).filePath(QStringLiteral("applications")));

    const auto dataDirs = splitSearchPath(environment.value(QStringLiteral("XDG_DATA_DIRS")),
                                          {QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")});
    for (const auto &dataDir : dataDirs)
        applicationDirs.append(QDir(dataDir).filePath(QStringLiteral("applications")));

    for (const auto &applicationDir : applicationDirs) {
        const QFileInfo candidate(QDir(applicationDir).filePath(fileName));
        if (candidate.isFile() && candidate.isReadable())
            return candidate.absoluteFilePath();
    }
    return {};
}

QString unescapeDesktopValue(QString value)
{
    value.replace(QStringLiteral("\\s"), QStringLiteral(" "));
    value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    value.replace(QStringLiteral("\\t"), QStringLiteral("\t"));
    value.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return value;
}

QStringList parseExecTokens(const QString &execLine, QString *error)
{
    QStringList tokens;
    QString token;
    bool inQuotes = false;
    bool escaping = false;

    for (const QChar ch : execLine) {
        if (escaping) {
            token.append(ch);
            escaping = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaping = true;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && ch.isSpace()) {
            if (!token.isEmpty()) {
                tokens.append(token);
                token.clear();
            }
            continue;
        }
        token.append(ch);
    }

    if (escaping)
        token.append(QLatin1Char('\\'));
    if (inQuotes) {
        if (error)
            *error = QStringLiteral("Desktop Exec line contains an unterminated quote.");
        return {};
    }
    if (!token.isEmpty())
        tokens.append(token);

    QStringList cleaned;
    for (auto item : tokens) {
        if (item == QStringLiteral("%%")) {
            cleaned.append(QStringLiteral("%"));
            continue;
        }
        if (item.startsWith(QLatin1Char('%'))) {
            if (item.size() == 2 && QStringLiteral("fFuUdDnNickvm").contains(item.at(1)))
                continue;
            if (error)
                *error = QStringLiteral("Desktop Exec line contains unsupported field code: %1").arg(item);
            return {};
        }
        item.replace(QStringLiteral("%%"), QStringLiteral("%"));
        cleaned.append(item);
    }

    if (cleaned.isEmpty() && error)
        *error = QStringLiteral("Desktop Exec line did not contain a launch command.");
    return cleaned;
}

bool tokenHasUnsafeShellSyntax(const QString &token)
{
    static const QString unsafe = QStringLiteral(";&|<>`");
    for (const QChar ch : unsafe) {
        if (token.contains(ch))
            return true;
    }
    return token.contains(QStringLiteral("$(")) || token.contains(QStringLiteral("${"));
}

bool hasControlCharacters(const QString &value)
{
    for (const auto ch : value) {
        if (ch.isNull() || (!ch.isSpace() && ch.category() == QChar::Other_Control))
            return true;
    }
    return false;
}

QString expandActionArgument(QString argument, const QString &target)
{
    argument.replace(QStringLiteral("{target}"), target);
    return argument;
}

bool appendActionArguments(ResolvedDesktopLaunch *launch, const QVariantMap &configuration, QString *error)
{
    const auto actionType = configuration.value(QStringLiteral("actionType")).toString().trimmed();
    const auto actionTarget = configuration.value(QStringLiteral("actionTarget")).toString().trimmed();
    if (actionType.isEmpty() && actionTarget.isEmpty())
        return true;
    if (actionType.isEmpty() || actionTarget.isEmpty()) {
        if (error)
            *error = QStringLiteral("Desktop action launch is missing type or target.");
        return false;
    }
    if (hasControlCharacters(actionTarget)) {
        if (error)
            *error = QStringLiteral("Desktop action target contains unsupported control characters.");
        return false;
    }

    const auto handlers = configuration.value(QStringLiteral("actionHandlers")).toMap();
    const auto handler = handlers.value(actionType).toMap();
    if (handler.isEmpty()) {
        if (error)
            *error = QStringLiteral("%1 declares %2 support but has no launch adapter for it.")
                .arg(configuration.value(QStringLiteral("desktopId")).toString().isEmpty()
                         ? QStringLiteral("This app")
                         : configuration.value(QStringLiteral("desktopId")).toString(),
                     actionType);
        return false;
    }

    const auto configuredArguments = handler.value(QStringLiteral("arguments")).toList();
    if (!configuredArguments.isEmpty()) {
        for (const auto &argumentValue : configuredArguments) {
            const auto argument = expandActionArgument(argumentValue.toString(), actionTarget);
            if (argument.trimmed().isEmpty() || tokenHasUnsafeShellSyntax(argument) || hasControlCharacters(argument)) {
                if (error)
                    *error = QStringLiteral("Desktop action adapter contains an unsafe argument.");
                return false;
            }
            launch->arguments.append(argument);
        }
        return true;
    }

    if (handler.value(QStringLiteral("appendTarget")).toBool()) {
        launch->arguments.append(actionTarget);
        return true;
    }

    if (error)
        *error = QStringLiteral("Desktop action adapter for %1 does not define how to pass the target.").arg(actionType);
    return false;
}

ResolvedDesktopLaunch resolveDesktopLaunch(const QString &desktopId, const QProcessEnvironment &environment)
{
    ResolvedDesktopLaunch resolved;
    resolved.desktopPath = resolveDesktopEntryPath(desktopId, environment);
    if (resolved.desktopPath.isEmpty()) {
        resolved.error = QStringLiteral("No readable desktop entry was found for \"%1\".").arg(desktopId);
        return resolved;
    }

    QFile desktopFile(resolved.desktopPath);
    if (!desktopFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        resolved.error = QStringLiteral("Desktop entry could not be opened: %1").arg(desktopFile.errorString());
        return resolved;
    }

    bool inDesktopEntry = false;
    QString type;
    QString terminal;
    QTextStream stream(&desktopFile);
    while (!stream.atEnd()) {
        const auto line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            const auto section = line.mid(1, line.size() - 2);
            if (inDesktopEntry && section != QStringLiteral("Desktop Entry"))
                break;
            inDesktopEntry = section == QStringLiteral("Desktop Entry");
            continue;
        }
        if (!inDesktopEntry)
            continue;
        const auto separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;
        const auto key = line.left(separator).trimmed();
        const auto value = unescapeDesktopValue(line.mid(separator + 1).trimmed());
        if (key == QStringLiteral("Name"))
            resolved.name = value;
        else if (key == QStringLiteral("Type"))
            type = value;
        else if (key == QStringLiteral("Terminal"))
            terminal = value;
        else if (key == QStringLiteral("Exec"))
            resolved.execLine = value;
    }

    if (type != QStringLiteral("Application")) {
        resolved.error = QStringLiteral("Desktop entry is not an application.");
        return resolved;
    }
    if (terminal.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        resolved.error = QStringLiteral("Terminal desktop entries are not supported in TV mode.");
        return resolved;
    }
    if (resolved.execLine.trimmed().isEmpty()) {
        resolved.error = QStringLiteral("Desktop entry has no Exec line.");
        return resolved;
    }

    QString parseError;
    auto tokens = parseExecTokens(resolved.execLine, &parseError);
    if (!parseError.isEmpty()) {
        resolved.error = parseError;
        return resolved;
    }
    for (const auto &token : tokens) {
        if (tokenHasUnsafeShellSyntax(token)) {
            resolved.error = QStringLiteral("Desktop Exec token contains shell syntax that Isolde will not run: %1").arg(token);
            return resolved;
        }
    }

    auto program = tokens.takeFirst();
    QString executable;
    if (QFileInfo(program).isAbsolute()) {
        const QFileInfo programInfo(program);
        if (programInfo.isFile() && programInfo.isExecutable())
            executable = programInfo.absoluteFilePath();
    } else {
        executable = QStandardPaths::findExecutable(program);
    }
    if (executable.isEmpty()) {
        resolved.error = QStringLiteral("Desktop Exec program is not available: %1").arg(program);
        return resolved;
    }

    resolved.program = executable;
    resolved.arguments = tokens;
    return resolved;
}

} // namespace

void DesktopSession::launch()
{
    auto environment = desktopEnvironment();
    const auto desktopId = definition().launchConfiguration.value(QStringLiteral("desktopId")).toString();
    const auto launch = resolveDesktopLaunch(desktopId, environment);
    if (!launch.error.isEmpty()) {
        setError(launch.error);
        qWarning() << "Desktop bridge refused to launch" << definition().id << launch.error;
        setState(State::Exited);
        return;
    }
    auto resolvedLaunch = launch;
    const auto sandbox = NativeSandbox::prepare(qApp->property("isoldeProfile").toString(), definition().id,
        resolvedLaunch.program, environment, definition().launchConfiguration.value("actionTarget").toString(), {},
        definition().launchConfiguration.value("emulatorRuntime").toString(),
        definition().launchConfiguration.value("gameSourceRoot").toString());
    if (!sandbox.error.isEmpty()) {
        setError(sandbox.error);
        setState(State::Exited);
        return;
    }
    auto configuration = definition().launchConfiguration;
    if (definition().id == "kodi") {
        m_kodiLock = std::make_unique<QLockFile>(sandbox.homeDirectory + "/.isolde-managed.lock");
        if (!m_kodiLock->tryLock()) {
            setError("Kodi is already using this profile. Exit it normally before opening another instance.");
            setState(State::Exited);
            return;
        }
    }
    if (configuration.contains("actionTarget")) configuration["actionTarget"] = sandbox.actionTarget;
    QString actionError;
    if (!appendActionArguments(&resolvedLaunch, configuration, &actionError)) {
        setError(actionError);
        qWarning() << "Desktop bridge refused action launch" << definition().id << actionError;
        setState(State::Exited);
        return;
    }

    const auto emulator=configuration.value("emulatorRuntime").toString();
    if(!emulator.isEmpty())resolvedLaunch.arguments={"/run/emulator-session.py","--runtime",emulator,sandbox.actionTarget};
    qInfo() << "Isolated native session launching" << definition().id << "profile" << qApp->property("isoldeProfile");
    m_launcher.setProcessEnvironment(sandbox.environment);
    m_launcher.setProgram(sandbox.program);
    auto launchArguments=sandbox.arguments;
    if(definition().presentationMode=="web-kiosk"){
        const auto appearance=qApp->property("isoldeRemoteAppearance").toMap();
        if(!appearance.contains("focusColor")){setError("The app's remote appearance is unavailable. Return Home and retry.");setState(State::Exited);return;}
        const auto index=launchArguments.lastIndexOf("--");
        launchArguments.insert(index,"--web-remote");launchArguments.insert(index+1,QString::fromLatin1(QJsonDocument::fromVariant(appearance).toJson(QJsonDocument::Compact).toBase64()));
    }
    m_launcher.setArguments(launchArguments + resolvedLaunch.arguments);
    connect(&m_launcher, &QProcess::readyReadStandardError, this, [this] {
        m_diagnostics = (m_diagnostics + m_launcher.readAllStandardError()).right(8192);
    });
    connect(&m_launcher, &QProcess::readyReadStandardOutput, this, [this] { m_launcher.readAllStandardOutput(); });
    connect(&m_launcher, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        qWarning() << "Desktop bridge process error for" << definition().id << error << m_launcher.errorString();
        setError(QStringLiteral("The isolated app process could not run: %1").arg(m_launcher.errorString()));
    });
    connect(&m_launcher, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                qInfo() << "Isolated native session finished" << definition().id << exitCode << exitStatus;
                m_kodiLock.reset();
                if (m_stopping) return;
                if ((exitStatus != QProcess::NormalExit || exitCode != 0) && definition().launchConfiguration.contains("emulatorRuntime") && m_diagnostics.contains("Emulator setup: ")) {
                    const auto reason=QString::fromUtf8(m_diagnostics).section("Emulator setup: ",-1).section('\n',0,0).left(320);
                    setError(reason);
                } else if (exitStatus != QProcess::NormalExit || exitCode != 0)
                    setError(QStringLiteral("%1 could not continue in its private profile (exit %2). No shared-account fallback was used.").arg(definition().name).arg(exitCode));
                // Defer teardown until QProcess has finished delivering its signal.
                QTimer::singleShot(0, this, [this] { setState(State::Exited); });
            });
    m_launcher.start();
    if (!m_launcher.waitForStarted(5000)) {
        setError(QStringLiteral("Could not start the native isolation service: %1").arg(m_launcher.errorString()));
        qWarning() << "Desktop bridge could not start" << definition().id << m_launcher.errorString();
        setState(State::Exited);
        return;
    }
    setState(State::Running);
    if (definition().launchConfiguration.value(QStringLiteral("containment")).toString() == QStringLiteral("fullscreen")) {
        QTimer::singleShot(1200, this, &DesktopSession::applyContainment);
        QTimer::singleShot(3000, this, &DesktopSession::applyContainment);
        QTimer::singleShot(6000, this, &DesktopSession::applyContainment);
        QTimer::singleShot(12000, this, &DesktopSession::applyContainment);
        QTimer::singleShot(20000, this, &DesktopSession::applyContainment);
    }
}

void DesktopSession::suspend() { if (state() == State::Running) setState(State::Suspended); }
void DesktopSession::resume() { if (state() == State::Suspended) setState(State::Running); }

DesktopSession::~DesktopSession() { stopProcess(); }

void DesktopSession::stopProcess()
{
    m_stopping = true;
    if (m_launcher.state() != QProcess::NotRunning) {
        m_launcher.write("quit\n");
        m_launcher.waitForBytesWritten(500);
        if (!m_launcher.waitForFinished(4000)) {
            m_launcher.kill();
            m_launcher.waitForFinished(3000);
        }
    }
}

void DesktopSession::exit()
{
    stopProcess();
    setState(State::Exited);
}

bool DesktopSession::applyContainment()
{
    if (state() != State::Running)
        return false;
    const auto windowClass = definition().launchConfiguration.value(QStringLiteral("windowClass")).toString().trimmed();
    if (windowClass.isEmpty()) {
        qWarning() << "Desktop bridge containment skipped for" << definition().id << "because no windowClass is declared.";
        return false;
    }
    const auto wmctrl = QStandardPaths::findExecutable(QStringLiteral("wmctrl"));
    if (wmctrl.isEmpty()) {
        qWarning() << "Desktop bridge containment skipped for" << definition().id << "because wmctrl is unavailable.";
        return false;
    }
    auto environment = desktopEnvironment();

    QProcess windows;
    windows.setProcessEnvironment(environment);
    windows.start(wmctrl, {QStringLiteral("-lx")});
    if (!windows.waitForFinished(1200) || windows.exitCode() != 0) {
        qWarning() << "Desktop bridge containment could not list windows for" << definition().id;
        return false;
    }
    QString windowId;
    const auto lines = QString::fromUtf8(windows.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        if (line.contains(windowClass)) {
            windowId = line.section(QLatin1Char(' '), 0, 0).trimmed();
            break;
        }
    }
    if (windowId.isEmpty()) {
        qWarning() << "Desktop bridge containment could not find window class" << windowClass << "for" << definition().id;
        if (m_launcher.state() == QProcess::NotRunning) {
            qWarning() << "Desktop bridge launcher is no longer running and no contained window was found for" << definition().id;
            setState(State::Exited);
        }
        return false;
    }
    if (!m_containmentMatched) {
        qInfo() << "Desktop bridge contained" << definition().id << "window" << windowId << "class" << windowClass;
        m_containmentMatched = true;
    }

    const auto xprop = QStandardPaths::findExecutable(QStringLiteral("xprop"));
    if (!xprop.isEmpty()) {
        QProcess undecorate;
        undecorate.setProcessEnvironment(environment);
        undecorate.start(xprop, {QStringLiteral("-id"), windowId, QStringLiteral("-f"), QStringLiteral("_MOTIF_WM_HINTS"),
                                 QStringLiteral("32c"), QStringLiteral("-set"), QStringLiteral("_MOTIF_WM_HINTS"),
                                 QStringLiteral("2, 0, 0, 0, 0")});
        undecorate.waitForFinished(1200);
    }

    QProcess activate;
    activate.setProcessEnvironment(environment);
    activate.start(wmctrl, {QStringLiteral("-i"), QStringLiteral("-a"), windowId});
    activate.waitForFinished(1200);

    QProcess maximize;
    maximize.setProcessEnvironment(environment);
    maximize.start(wmctrl, {QStringLiteral("-i"), QStringLiteral("-r"), windowId,
                            QStringLiteral("-b"), QStringLiteral("add,maximized_vert,maximized_horz")});
    maximize.waitForFinished(1200);

    QProcess fullscreen;
    fullscreen.setProcessEnvironment(environment);
    fullscreen.start(wmctrl, {QStringLiteral("-i"), QStringLiteral("-r"), windowId,
                              QStringLiteral("-b"), QStringLiteral("add,fullscreen")});
    if (!fullscreen.waitForFinished(1200) || fullscreen.exitCode() != 0) {
        qWarning() << "Desktop bridge fullscreen containment failed for" << definition().id
                   << QString::fromUtf8(fullscreen.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}
