#include "settings/SearchSettings.h"

#include <QSettings>
#include <QCoreApplication>
#include <QUuid>
#include <memory>
#include <QRegularExpression>

static std::unique_ptr<QSettings> searchStore(const QString &path)
{
    // Retain the existing profile preference file. The application has no
    // organization name, so default QSettings reports AccessError even when
    // its fallback file is writable. Explicit file access avoids that error
    // without relocating or rewriting existing preferences on load.
    const auto file = path.isEmpty() ? QSettings().fileName() : path;
    return std::make_unique<QSettings>(file,QSettings::IniFormat);
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
    const auto region=settings->value(QStringLiteral("discoveryRegion"),QStringLiteral("US")).toString().trimmed().toUpper();
    static const QRegularExpression regionPattern(QStringLiteral("^[A-Z]{2}$"));
    if(regionPattern.match(region).hasMatch())m_discoveryRegion=region;
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

void SearchSettings::setDiscoveryRegion(const QString &region)
{
    static const QRegularExpression regionPattern(QStringLiteral("^[A-Z]{2}$"));
    const auto normalized=region.trimmed().toUpper();
    if(normalized==m_discoveryRegion)return;
    if(m_group.isEmpty() || !regionPattern.match(normalized).hasMatch()){
        m_error=m_group.isEmpty()?"Could not save discovery region for this profile.":"Enter a two-letter ISO region code.";
        emit changed();return;
    }
    auto settings=searchStore(m_settingsPath);
    settings->setAtomicSyncRequired(true);
    settings->beginGroup(m_group);
    settings->setValue(QStringLiteral("discoveryRegion"),normalized);
    settings->sync();
    if(settings->status()!=QSettings::NoError){
        settings->setValue(QStringLiteral("discoveryRegion"),m_discoveryRegion);
        settings->sync();
        m_error="Could not save discovery region. Your previous setting is unchanged.";
        emit changed();return;
    }
    m_discoveryRegion=normalized;
    m_error.clear();
    emit changed();
}
