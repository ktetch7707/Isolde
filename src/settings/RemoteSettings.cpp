#include "settings/RemoteSettings.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

RemoteSettings::RemoteSettings(QObject *parent) : QObject(parent)
{
    load();
    refreshCec();
}

QString RemoteSettings::profile() const { return m_profile; }
bool RemoteSettings::setupMode() const { return m_setupMode; }
QString RemoteSettings::lastInput() const { return m_lastInput; }
QString RemoteSettings::setupStatus() const
{
    return m_setupMode ? QStringLiteral("Listening for remote buttons") : QStringLiteral("Ready for keyboard, remote, or controller input");
}
bool RemoteSettings::cecAvailable() const { return m_cecAvailable; }
QString RemoteSettings::cecStatus() const { return m_cecStatus; }
QString RemoteSettings::powerAction() const { return m_powerAction; }

QVariantList RemoteSettings::requiredButtons() const
{
    return {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Up")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Down")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Left")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Right")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("OK / Select")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Back")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Home")}, {QStringLiteral("mapped"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Volume + / -")}, {QStringLiteral("mapped"), false}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Mute")}, {QStringLiteral("mapped"), false}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Play / Pause")}, {QStringLiteral("mapped"), false}},
    };
}

void RemoteSettings::nextProfile()
{
    if (m_profile == QStringLiteral("simple"))
        m_profile = QStringLiteral("media");
    else if (m_profile == QStringLiteral("media"))
        m_profile = QStringLiteral("gamepad");
    else
        m_profile = QStringLiteral("simple");
    save();
    emit changed();
}

void RemoteSettings::toggleSetupMode()
{
    m_setupMode = !m_setupMode;
    save();
    emit changed();
}

void RemoteSettings::nextPowerAction()
{
    if (m_powerAction == QStringLiteral("ambient"))
        m_powerAction = QStringLiteral("tv-standby");
    else if (m_powerAction == QStringLiteral("tv-standby"))
        m_powerAction = QStringLiteral("tv-on");
    else
        m_powerAction = QStringLiteral("ambient");
    save();
    emit changed();
}

bool RemoteSettings::refreshCec()
{
    const auto cecClient = QStandardPaths::findExecutable(QStringLiteral("cec-client"));
    if (cecClient.isEmpty()) {
        m_cecTool.clear();
        m_cecAvailable = false;
        m_cecStatus = QStringLiteral("No cec-client command found; TV power control unavailable");
    } else {
        m_cecTool = QStringLiteral("cec-client");
        m_cecAvailable = true;
        m_cecStatus = QStringLiteral("CEC command available; adapter/device scan not run automatically");
    }
    emit changed();
    return m_cecAvailable;
}

bool RemoteSettings::sendTvPowerCommand()
{
    if (m_powerAction == QStringLiteral("tv-on"))
        return turnTvOn();
    if (m_powerAction == QStringLiteral("tv-standby"))
        return putTvInStandby();
    m_cecStatus = QStringLiteral("Power button is set to Ambient Mode, not TV CEC");
    emit changed();
    return false;
}

bool RemoteSettings::turnTvOn()
{
    return runCecCommand(QStringLiteral("on 0"));
}

bool RemoteSettings::putTvInStandby()
{
    return runCecCommand(QStringLiteral("standby 0"));
}

void RemoteSettings::recordTestInput(const QString &inputName)
{
    const auto cleaned = inputName.trimmed();
    m_lastInput = cleaned.isEmpty() ? QStringLiteral("Unknown input") : cleaned;
    save();
    emit changed();
}

void RemoteSettings::reset()
{
    m_profile = QStringLiteral("simple");
    m_setupMode = false;
    m_lastInput = QStringLiteral("No button tested yet");
    m_powerAction = QStringLiteral("ambient");
    save();
    emit changed();
}

void RemoteSettings::setCecEnabledForTests(bool enabled)
{
    m_cecEnabled = enabled;
}

void RemoteSettings::load()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("remote"));
    m_profile = cleanProfile(settings.value(QStringLiteral("profile"), m_profile).toString());
    m_setupMode = settings.value(QStringLiteral("setupMode"), m_setupMode).toBool();
    m_lastInput = settings.value(QStringLiteral("lastInput"), m_lastInput).toString().trimmed();
    m_powerAction = cleanPowerAction(settings.value(QStringLiteral("powerAction"), m_powerAction).toString());
    if (m_lastInput.isEmpty())
        m_lastInput = QStringLiteral("No button tested yet");
}

void RemoteSettings::save() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("remote"));
    settings.setValue(QStringLiteral("profile"), m_profile);
    settings.setValue(QStringLiteral("setupMode"), m_setupMode);
    settings.setValue(QStringLiteral("lastInput"), m_lastInput);
    settings.setValue(QStringLiteral("powerAction"), m_powerAction);
}

QString RemoteSettings::cleanProfile(const QString &profile)
{
    const auto normalized = profile.trimmed().toLower();
    if (normalized == QStringLiteral("media") || normalized == QStringLiteral("gamepad"))
        return normalized;
    return QStringLiteral("simple");
}

QString RemoteSettings::cleanPowerAction(const QString &action)
{
    const auto normalized = action.trimmed().toLower();
    if (normalized == QStringLiteral("tv-standby") || normalized == QStringLiteral("tv-on"))
        return normalized;
    return QStringLiteral("ambient");
}

bool RemoteSettings::runCecCommand(const QString &command)
{
    if (!m_cecEnabled) {
        m_cecStatus = QStringLiteral("CEC disabled for tests; command not sent");
        emit changed();
        return false;
    }
    if (!m_cecAvailable && !refreshCec())
        return false;
    QProcess process;
    process.start(m_cecTool, {QStringLiteral("-s"), QStringLiteral("-d"), QStringLiteral("1")});
    if (!process.waitForStarted(500)) {
        m_cecStatus = QStringLiteral("Could not start cec-client");
        emit changed();
        return false;
    }
    process.write((command + QStringLiteral("\n")).toUtf8());
    process.closeWriteChannel();
    if (!process.waitForFinished(2000)) {
        process.kill();
        m_cecStatus = QStringLiteral("cec-client did not finish; TV command may not have been sent");
        emit changed();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        m_cecStatus = error.isEmpty()
            ? QStringLiteral("cec-client rejected the TV command")
            : QStringLiteral("cec-client rejected the TV command: ") + error.left(120);
        emit changed();
        return false;
    }
    m_cecStatus = command == QStringLiteral("on 0")
        ? QStringLiteral("Sent TV on command through HDMI-CEC")
        : QStringLiteral("Sent TV standby command through HDMI-CEC");
    emit changed();
    return true;
}
