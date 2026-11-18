from pathlib import Path

p = Path('src/media/KodiService.cpp')
content = p.read_text()

old_start = '''KodiService::KodiService(const QString& profile, QObject* parent)
    : QObject(parent), m_profile(profile), m_modeClient(profile)
{
    m_frontend = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                     .value("profiles/" + profile + "/kodi/frontend", "isolde")
                     .toString();
    if (m_frontend != "kodi")
        m_frontend = "isolde";
    m_modeDeadline.setSingleShot(true);
    m_modeDeadline.setInterval(20000);
    connect(&m_modeDeadline,
            &QTimer::timeout,
            this,
            [this]
            {
                if (m_pendingFrontend == "folders")
                    m_folderStatus =
                        "Kodi has not finished saving. Folder permissions are unchanged; stop playback/scanning "
                        "and press Menu to retry.";
                m_pendingFrontend.clear();
                m_status =
                    "Kodi has not exited cleanly. Mode unchanged; no forced shutdown was used. Exit Kodi normally, "
                    "then retry.";
                emit changed();
            });
    connect(&m_modeClient,
            &KodiMedia::quitReply,
            this,
            [this](bool accepted)
            {
                if (!switching() || accepted)
                    return;
                if (m_pendingFrontend == "folders")
                    m_folderStatus =
                        "Stop playback and wait for scans to finish, then press Menu to retry. Folder permissions "
                        "are unchanged.";
                m_modeDeadline.stop();
                m_pendingFrontend.clear();
                m_status = "Could not switch safely. Stop playback, wait for scans, and retry. Mode unchanged.";
                emit changed();
            });
    m_enabled = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                    .value("profiles/" + profile + "/kodi/autoStart", true)
                    .toBool();
    m_alAlwaysPlayLocally = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                                .value("profiles/" + profile + "/kodi/alwaysPlayLocally", true)
                                .toBool();
    if (!m_enabled)
        m_status = "Automatic startup is off.";
    m_probe.setInterval(1000);
    connect(&m_probe,
            &QTimer::timeout,
            this,
            [this]
            {
                if (++m_checks > 30)
                {
                    fail("Kodi did not become ready. Retry or open Kodi directly.");
                    return;
                }
                emit probeRequested();
            });
    connect(&m_process,
            &QProcess::readyReadStandardOutput,
            this,
            [this]()
            {
                m_process.readAllStandardOutput();
            });
    connect(&m_process,
            &QProcess::readyReadStandardError,
            this,
            [this]()
            {
                m_process.readAllStandardError();
            });
    connect(&m_process,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError error)
            {
                if (!m_stopping && error == QProcess::FailedToStart)
                    fail("Kodi could not start. Check the installation and retry.");
            });
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus exitStatus)
            {
                m_probe.stop();
                m_ready = false;
                if (switching())
                {
                    const auto requested = m_pendingFrontend;
                    m_pendingFrontend.clear();
                    m_modeDeadline.stop();
                    m_lock.reset();
                    m_overlay.reset();
                    emit unavailable("Kodi closed for frontend switching.");
                    if (requested == "folders")'''

new_start = '''KodiService::KodiService(const QString& profile, QObject* parent)
    : QObject(parent), m_profile(profile), m_modeClient(profile)
{
    m_frontend = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                     .value("profiles/" + profile + "/kodi/frontend", "isolde")
                     .toString();
    if (m_frontend != "kodi")
        m_frontend = "isolde";
    m_modeDeadline.setSingleShot(true);
    m_modeDeadline.setInterval(20000);
    connect(&m_modeDeadline, &QTimer::timeout, this, &KodiService::onModeDeadlineTimeout);
    connect(&m_modeClient, &KodiMedia::quitReply, this, &KodiService::onModeClientQuitReply);
    m_enabled = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                    .value("profiles/" + profile + "/kodi/autoStart", true)
                    .toBool();
    m_alAlwaysPlayLocally = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Kodi")
                                .value("profiles/" + profile + "/kodi/alwaysPlayLocally", true)
                                .toBool();
    if (!m_enabled)
        m_status = "Automatic startup is off.";
    m_probe.setInterval(1000);
    connect(&m_probe, &QTimer::timeout, this, &KodiService::onProbeTimeout);
    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            [this]() { m_process.readAllStandardOutput(); });
    connect(&m_process, &QProcess::readyReadStandardError, this,
            [this]() { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (!m_stopping && error == QProcess::FailedToStart)
                    fail("Kodi could not start. Check the installation and retry.");
            });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus exitStatus) {
                m_probe.stop();
                m_ready = false;
                if (switching()) {
                    const auto requested = m_pendingFrontend;
                    m_pendingFrontend.clear();
                    m_modeDeadline.stop();
                    m_lock.reset();
                    m_overlay.reset();
                    emit unavailable("Kodi closed for frontend switching.");
                    if (requested == "folders")'''

if old_start in content:
    content = content.replace(old_start, new_start, 1)
    p.write_text(content)
    print("Replaced KodiService::KodiService constructor")
else:
    print("Could not find old_start in KodiService.cpp")
    idx = content.find('KodiService::KodiService')
    if idx >= 0:
        print("Found at:", idx)
        print(repr(content[idx:idx+200]))
