#pragma once
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Packaged identities only. No executable, argv or configuration snippets may
// come from a layout, game record, imported document or settings control.
namespace GameEmulators
{
QVariantList
definitions();
QVariantMap
definition(const QString& id);
QVariantMap
coverage();
QString
home(const QString& profile, const QString& id);
QVariantMap
settings(const QString& profile, const QString& id, QString* error);
bool
configure(
    const QString& profile, const QString& id, const QVariantMap& options, const QString& firmware, QString* error);
bool
playStationFirmwareReady(const QString& profile, QString* error);
bool
setupAssetReady(const QString& profile, const QString& id, QString* error);
bool
validateConfigureOptions(const QVariantMap& options, QString* error);
bool
saveEmulatorSettings(const QString& root, const QVariantMap& value, QString* error);
bool
importFirmwareWithSetup(
    const QString& root, const QVariantMap& setup, const QString& firmware, QVariantMap& value, QString* error);
bool
importFirmwareWithoutSetup(const QString& root,
                           const QVariantMap& setup,
                           const QString& firmware,
                           const QString& id,
                           QVariantMap& value,
                           QString* error);
} // namespace GameEmulators
