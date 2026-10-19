#include "ComponentHealth.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ComponentHealth::ComponentHealth(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(25000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { m_process.kill(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_timeout.stop(); m_busy = false;
        m_status = "Component inspection could not start. Reinstall Isolde support files.";
        emit changed();
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus exit) {
        m_timeout.stop(); m_busy = false;
        const auto result = QJsonDocument::fromJson(m_process.readAllStandardOutput()).object();
        m_process.readAllStandardError();
        if (code == 0 && exit == QProcess::NormalExit && result.value("components").isArray()) {
            m_components = result.value("components").toArray().toVariantList();
            m_status = "Offline inspection complete. Installed runtime is not proof of working capture hardware.";
        } else m_status = "Inspection failed or timed out. No settings were changed.";
        emit changed();
    });
}
ComponentHealth::~ComponentHealth() { m_process.kill(); m_process.waitForFinished(1000); }
void ComponentHealth::refresh()
{
    if (m_busy) return;
    const auto base = QCoreApplication::applicationDirPath();
    auto script = base + "/../share/isolde/component-health.py";
    if (!QFileInfo::exists(script)) script = base + "/share/isolde/component-health.py";
    m_components.clear();
    m_busy = true;
    m_status = "Checking local versions and capture runtime elements…";
    emit changed();
    m_timeout.start();
    m_process.start("/usr/bin/python3", {script});
}
