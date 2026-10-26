#pragma once
#include <QVariantList>
#include <QVariantMap>
#include <QString>

// Packaged identities only. No executable, argv or configuration snippets may
// come from a layout, game record, imported document or settings control.
namespace GameEmulators {
QVariantList definitions();
QVariantMap definition(const QString &id);
QString home(const QString &profile, const QString &id);
QVariantMap settings(const QString &profile, const QString &id, QString *error);
bool configure(const QString &profile, const QString &id, const QVariantMap &options,
               const QString &firmware, QString *error);
}
