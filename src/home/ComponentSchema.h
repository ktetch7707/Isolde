#pragma once
#include <QVariantMap>
#include <QVariantList>
class AppRegistry;

// Data contracts only. No process launching, networking or Kodi mutations.
namespace ComponentSchema {
bool validPresentation(const QVariantMap &value, bool section = false);
bool validComposition(const QVariantMap &value);
bool validGrid(const QVariantMap &value);
QVariantMap normalizedGrid(const QVariantMap &value);
bool validPlacement(const QVariantMap &value, int maxColumns, int maxRows);
bool placementsOverlap(const QVariantMap &a, const QVariantMap &b);
bool placementsFit(const QJsonArray &items, const QVariantMap &grid);
QVariantMap sectionGrid(const QVariantMap &composition, const QJsonArray &categories, const QString &categoryId);
bool layoutPlacementsFit(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories);
bool safeId(const QString &id);
QVariantMap action(const QVariantMap &input, QString *error = nullptr);
QVariantMap component(const QVariantMap &input, bool legacy, QString *error = nullptr);
QString unavailable(const QVariantMap &action, const AppRegistry *apps, const QVariantList &collections);
}
