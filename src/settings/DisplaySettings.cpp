#include "settings/DisplaySettings.h"

#include <QSettings>

DisplaySettings::DisplaySettings(QObject *parent) : QObject(parent)
{
    load();
}

int DisplaySettings::safeArea() const { return m_safeArea; }
int DisplaySettings::uiScale() const { return m_uiScale; }
bool DisplaySettings::reduceMotion() const { return m_reduceMotion; }

void DisplaySettings::adjustSafeArea(int delta)
{
    const auto next = clamp(m_safeArea + delta, 0, 12);
    if (next == m_safeArea)
        return;
    m_safeArea = next;
    save();
    emit changed();
}

void DisplaySettings::adjustUiScale(int delta)
{
    const auto next = clamp(m_uiScale + delta, 85, 125);
    if (next == m_uiScale)
        return;
    m_uiScale = next;
    save();
    emit changed();
}

void DisplaySettings::toggleReduceMotion()
{
    m_reduceMotion = !m_reduceMotion;
    save();
    emit changed();
}

void DisplaySettings::reset()
{
    m_safeArea = 4;
    m_uiScale = 100;
    m_reduceMotion = false;
    save();
    emit changed();
}

void DisplaySettings::load()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("display"));
    m_safeArea = clamp(settings.value(QStringLiteral("safeArea"), m_safeArea).toInt(), 0, 12);
    m_uiScale = clamp(settings.value(QStringLiteral("uiScale"), m_uiScale).toInt(), 85, 125);
    m_reduceMotion = settings.value(QStringLiteral("reduceMotion"), m_reduceMotion).toBool();
}

void DisplaySettings::save() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("display"));
    settings.setValue(QStringLiteral("safeArea"), m_safeArea);
    settings.setValue(QStringLiteral("uiScale"), m_uiScale);
    settings.setValue(QStringLiteral("reduceMotion"), m_reduceMotion);
}

int DisplaySettings::clamp(int value, int minimum, int maximum)
{
    return qBound(minimum, value, maximum);
}
