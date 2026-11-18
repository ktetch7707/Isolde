#pragma once
#include <QString>
#include <QSet>
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
bool
validateEmulatorDocument(const QVariantMap& doc);
bool
validateEmulatorTarget(const QVariantMap& target);
bool
validateEmulatorGenerations(const QVariantMap& generations);
bool
validateEmulatorRuntimes(const QVariantList& runtimes, QSet<QString>& actualPlatforms);
    bool
    validateEmulatorRuntimeItem(const QVariantMap& item, QSet<QString>& ids, QSet<QString>& actualPlatforms);
    bool
    validateEmulatorRuntimeItemFields(const QVariantMap& item, QSet<QString>& ids);
    bool
    validateEmulatorRuntimeItem(const QVariantMap& item, QSet<QString>& ids, QSet<QString>& actualPlatforms);
    bool
    validateEmulatorRuntimeItemCore(const QVariantMap& item, QSet<QString>& ids);
    bool
    validateEmulatorRuntimeItemOptical(const QVariantMap& item);
    bool
    validateEmulatorRuntimePackages(const QString& engine, const QVariantMap& item, const QStringList& packages);
    bool
    validateEmulatorCoverage(const QVariantMap& coverage, const QSet<QString>& integratedSet);
    QSet<QString>
    expectedPackageSet(const QString& engine);
bool
validateEmulatorCoverage(const QVariantMap& coverage, const QSet<QString>& integratedSet);
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
    validateFirmwareFile(const QString& root, const QString& firmware, const QString& id,
                         const QString& existingFirmware, QString* error);
    bool
    importFirmwareWithoutSetup(const QString& root,
                           const QVariantMap& setup,
                           const QString& firmware,
                           const QString& id,
                           QVariantMap& value,
                           QString* error);
} // namespace GameEmulators
