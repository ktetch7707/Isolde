#include "settings/SearchSettings.h"

#include <QSettings>

SearchSettings::SearchSettings(QObject *parent) : QObject(parent)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("search"));
    m_webEnabled = settings.value(QStringLiteral("webEnabled"), false).toBool();
}

void SearchSettings::setWebEnabled(bool enabled)
{
    if (m_webEnabled == enabled)
        return;
    m_webEnabled = enabled;
    QSettings settings;
    settings.beginGroup(QStringLiteral("search"));
    settings.setValue(QStringLiteral("webEnabled"), enabled);
    settings.sync();
    emit changed();
}

void SearchSettings::toggleWebEnabled()
{
    setWebEnabled(!m_webEnabled);
}