#include "settings/SearchSettings.h"

#include <QSettings>
#include <QCoreApplication>
#include <QUuid>
#include <memory>

static std::unique_ptr<QSettings> searchStore(const QString &path)
{
    return path.isEmpty() ? std::make_unique<QSettings>() : std::make_unique<QSettings>(path,QSettings::IniFormat);
}

SearchSettings::SearchSettings(QObject *parent,const QString &settingsPath,const QString &profile) : QObject(parent),m_settingsPath(settingsPath)
{
    const auto id=profile.isEmpty()?QCoreApplication::instance()->property("isoldeProfile").toString():profile;
    if(!id.isEmpty() && id!="default" && (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces)!=id)){
        m_error="Invalid search profile.";return;
    }
    m_group=id.isEmpty() || id=="default"?"search":"profiles/"+id+"/search";
    auto settings=searchStore(m_settingsPath);
    settings->beginGroup(m_group);
    m_webEnabled = settings->value(QStringLiteral("webEnabled"), false).toBool();
}

void SearchSettings::setWebEnabled(bool enabled)
{
    if (m_group.isEmpty() || m_webEnabled == enabled)
        return;
    auto settings=searchStore(m_settingsPath);
    settings->setAtomicSyncRequired(true);
    settings->beginGroup(m_group);
    settings->setValue(QStringLiteral("webEnabled"), enabled);
    settings->sync();
    if(settings->status()!=QSettings::NoError){
        settings->setValue(QStringLiteral("webEnabled"),m_webEnabled);
        settings->sync();
        m_error="Could not save Web search. Your previous setting is unchanged. Check available storage and retry.";
        emit changed();return;
    }
    m_webEnabled = enabled;
    m_error.clear();
    emit changed();
}

void SearchSettings::toggleWebEnabled()
{
    setWebEnabled(!m_webEnabled);
}
