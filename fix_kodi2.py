from pathlib import Path

p = Path('src/media/KodiService.cpp')
content = p.read_text()

# Find the constructor start
start_marker = 'KodiService::KodiService(const QString& profile, QObject* parent)'
start_idx = content.find(start_marker)

# Find opening brace
brace_start = content.find('{', start_idx)

# Find matching closing brace
depth = 0
brace_end = brace_start
for i in range(brace_start, len(content)):
    if content[i] == '{':
        depth += 1
    elif content[i] == '}':
        depth -= 1
        if depth == 0:
            brace_end = i + 1
            break

print(f"Constructor: lines from {start_idx} to {brace_end}")
print(f"Body length: {brace_end - brace_start - 1} chars")

new_constructor = '''KodiService::KodiService(const QString& profile, QObject* parent)
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
                    if (requested == "folders") {

'''

# Replace the constructor body
new_content = content[:start_idx] + new_constructor + content[brace_end:]

# Verify brace balance
new_open = new_content.count('{')
new_close = new_content.count('}')
print(f"New braces: open={new_open}, close={new_close}")
print(f"Balanced: {new_open == new_close}")

p.write_text(new_content)
print("Done!")
