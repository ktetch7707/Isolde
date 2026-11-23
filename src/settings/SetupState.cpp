#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCoreApplication>
#ifdef Q_OS_LINUX
#include <QProcess>
#endif
#include "SetupState.h"

SetupState::SetupState(const QString& path, QObject* parent)
    : QObject(parent),
      m_path(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/device-setup.json"
                            : path)
{
    QFile file(m_path);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly))
    {
        m_error = "Cannot read setup progress.";
        return;
    }
    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    const int saved = obj.value("step").toInt(-1);
    if (obj.value("version").toInt() != 1 || saved < 0 || saved > 7 || !obj.value("complete").isBool() ||
        (obj.value("complete").toBool() && saved != 6 && saved != 7))
    {
        m_error = "Setup progress could not be read. Your profiles and settings are unchanged.";
        return;
    }
    // Completed seven-step v1 setups stay complete after adding library setup.
    // Compatibility is in memory only; loading never rewrites the user's file.
    m_step = obj.value("complete").toBool() ? 7 : saved;
    m_complete = obj.value("complete").toBool();
    m_autostartEnabled = obj.value("autostart").toBool();
}

bool
SetupState::store(int step, bool complete)
{
    QSaveFile file(m_path);
    const QByteArray data = QJsonDocument(QJsonObject{{"version", 1}, {"step", step}, {"complete", complete},
                                                      {"autostart", m_autostartEnabled}}).toJson();
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()) || !file.open(QIODevice::WriteOnly) ||
        file.write(data) != data.size() || !file.commit())
    {
        m_error = "Could not save setup progress. Check that storage is writable and has free space.";
        emit changed();
        return false;
    }
    m_step = step;
    m_complete = complete;
    m_error.clear();
    emit changed();
    return true;
}
bool
SetupState::advance()
{
    return store(qMin(7, m_step + 1), m_step == 7);
}
bool
SetupState::back()
{
    return store(qMax(0, m_step - 1), false);
}
bool
SetupState::restart()
{
    return store(0, false);
}

#ifdef Q_OS_LINUX
void
SetupState::setAutostartEnabled(bool enabled)
{
    if (m_autostartEnabled == enabled)
        return;
    if (!writeAutostartFiles(enabled))
        return;
    m_autostartEnabled = enabled;
    emit autostartEnabledChanged();
    emit changed();
}

bool
SetupState::writeAutostartFiles(bool enabled)
{
    const QString exe = QCoreApplication::applicationFilePath();
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty())
    {
        m_error = "Cannot determine user config directory.";
        return false;
    }

    if (enabled)
    {
        // User-level autostart desktop entry (no root required).
        QDir autostartDir(configDir + "/autostart");
        if (!autostartDir.mkpath("."))
        {
            m_error = "Cannot create autostart directory.";
            return false;
        }
        QFile desktopFile(autostartDir.filePath("isolde.desktop"));
        if (!desktopFile.open(QIODevice::WriteOnly))
        {
            m_error = "Cannot write autostart desktop entry.";
            return false;
        }
        const QString entry = QStringLiteral(
            "[Desktop Entry]\nType=Application\nName=Isolde\n"
            "Comment=Media and home interface\n"
            "Exec=%1\nTerminal=false\n"
            "X-GNOME-Autostart-enabled=true\n"
            "OnlyShowIn=GNOME;Unity;XFCE;\n").arg(exe);
        if (desktopFile.write(entry.toUtf8()) == -1)
        {
            m_error = "Cannot write autostart desktop entry.";
            return false;
        }
        desktopFile.close();

        // User-level systemd service for crash recovery.
        QDir systemdDir(configDir + "/systemd/user");
        if (!systemdDir.mkpath("."))
        {
            m_error = "Cannot create systemd user directory.";
            return false;
        }
        QFile serviceFile(systemdDir.filePath("isolde.service"));
        if (!serviceFile.open(QIODevice::WriteOnly))
        {
            m_error = "Cannot write systemd user service.";
            return false;
        }
        const QString service = QStringLiteral(
            "[Unit]\nDescription=Isolde living-room interface\n"
            "PartOf=graphical-session.target\n"
            "StartLimitIntervalSec=60\nStartLimitBurst=3\n"
            "\n[Service]\nType=simple\n"
            "ExecStart=/usr/bin/python3 %1/share/isolde/wait-for-display.py --executable \"%2\"\n"
            "Restart=always\nRestartSec=5\nTimeoutStopSec=10\nKillMode=control-group\n"
            "\n[Install]\nWantedBy=graphical-session.target\n").
            arg(QFileInfo(exe).absolutePath() + "/../share/isolde", exe);
        if (serviceFile.write(service.toUtf8()) == -1)
        {
            m_error = "Cannot write systemd user service.";
            return false;
        }
        serviceFile.close();

        // Enable the service for future logins.
        m_systemctl = new QProcess(this);
        m_systemctl->start(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--user") << QStringLiteral("enable") << QStringLiteral("isolde.service"));
        if (!m_systemctl->waitForStarted(5000))
        {
            // Non-fatal: autostart desktop entry still works.
            m_systemctl->deleteLater();
            m_systemctl = nullptr;
        }
    }
    else
    {
        // Remove autostart files.
        QFile::remove(configDir + "/autostart/isolde.desktop");
        QFile::remove(configDir + "/systemd/user/isolde.service");
        m_systemctl = new QProcess(this);
        m_systemctl->start(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--user") << QStringLiteral("disable") << QStringLiteral("isolde.service"));
        if (!m_systemctl->waitForStarted(5000))
        {
            m_systemctl->deleteLater();
            m_systemctl = nullptr;
        }
    }
    return true;
}
#endif
