#include "SetupState.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

SetupState::SetupState(const QString &path, QObject *parent) : QObject(parent),
    m_path(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/device-setup.json" : path)
{
    QFile file(m_path);
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) { m_error = "Cannot read setup progress."; return; }
    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    const int saved = obj.value("step").toInt(-1);
    if (obj.value("version").toInt() != 1 || saved < 0 || saved > 6
        || !obj.value("complete").isBool() || (obj.value("complete").toBool() && saved != 6)) {
        m_error = "Setup progress could not be read. Your profiles and settings are unchanged.";
        return;
    }
    m_step = saved;
    m_complete = obj.value("complete").toBool();
}

bool SetupState::store(int step, bool complete)
{
    QSaveFile file(m_path);
    const QByteArray data = QJsonDocument(QJsonObject{{"version", 1}, {"step", step}, {"complete", complete}}).toJson();
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()) || !file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size() || !file.commit()) {
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
bool SetupState::advance() { return store(qMin(6, m_step + 1), m_step == 6); }
bool SetupState::back() { return store(qMax(0, m_step - 1), false); }
bool SetupState::restart() { return store(0, false); }
