#include "sessions/AndroidSession.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

AndroidSession::~AndroidSession() { stop(); }

void AndroidSession::launch()
{
    if (!definition().isValid() || definition().type != "android") {
        setError(tr("This Android app has an invalid launch definition."));
        setState(State::Exited); return;
    }
    const auto base = QCoreApplication::applicationDirPath();
    QString helper;
    for (const auto &path : {base + "/../share/isolde/android-session.py", base + "/share/isolde/android-session.py"})
        if (QFileInfo(path).isFile()) { helper = QFileInfo(path).absoluteFilePath(); break; }
    const auto python = QStandardPaths::findExecutable("python3");
    if (helper.isEmpty() || python.isEmpty()) {
        setError(tr("Android support is missing. Run the Isolde installer with Android support."));
        setState(State::Exited); return;
    }
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_events += m_process.readAllStandardOutput();
        if (m_events.size() > 65536) {
            setError(tr("Android returned invalid session status.")); m_process.terminate(); return;
        }
        while (m_events.contains('\n')) {
            const auto split = m_events.indexOf('\n');
            const auto event = QJsonDocument::fromJson(m_events.left(split)).object();
            m_events.remove(0, split+1);
            if (event.value("event").toString() == "ready") { qInfo() << "Android app session ready" << definition().id; setState(State::Running); }
            if (event.value("event").toString() == "error") setError(event.value("message").toString().left(512));
        }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_stopping && error == QProcess::FailedToStart) {
            setError(tr("Android could not start. Check Android support in the Isolde installer."));
            setState(State::Exited);
        }
    });
    connect(&m_process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (m_stopping) return;
        if (errorString().isEmpty() && (code || status != QProcess::NormalExit))
            setError(tr("Android closed unexpectedly. Return Home and retry."));
        setState(State::Exited);
    });
    auto profile = qApp->property("isoldeProfile").toString();
    if (profile.isEmpty()) profile = "default";
    m_process.setProgram(python);
    QStringList arguments{helper,"--profile",profile,"--app",definition().launchConfiguration.value("runtimeApp").toString()};
    const auto version=definition().launchConfiguration.value("storeVersion").toString();
    if(!version.isEmpty())arguments << "--store-version" << version;
    m_process.setArguments(arguments);
    m_process.start();
}

void AndroidSession::stop()
{
    m_stopping = true;
    if (m_process.state() == QProcess::NotRunning) return;
    m_process.terminate();
    // The helper stops only its verified profile container and waits for release.
    if (!m_process.waitForFinished(15000)) {
        m_process.kill(); m_process.waitForFinished(3000);
    }
}
void AndroidSession::exit() { stop(); setState(State::Exited); }
void AndroidSession::suspend() { exit(); }
void AndroidSession::resume() { if (state() == State::Suspended) launch(); }
