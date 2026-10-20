#pragma once
#include <QVariantMap>
#include <QVariantList>
class AppRegistry;

// Data contracts only. No process launching, networking or Kodi mutations.
namespace ComponentSchema {
bool validPresentation(const QVariantMap &value, bool section = false, bool container = false);
bool validChildLayout(const QVariantMap &value);
bool validComposition(const QVariantMap &value);
bool validGrid(const QVariantMap &value);
QVariantMap normalizedGrid(const QVariantMap &value);
bool validPlacement(const QVariantMap &value, int maxColumns, int maxRows);
bool placementsOverlap(const QVariantMap &a, const QVariantMap &b);
bool placementsFit(const QJsonArray &items, const QVariantMap &grid);
QVariantMap sectionGrid(const QVariantMap &composition, const QJsonArray &categories, const QString &categoryId);
bool layoutPlacementsFit(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories);
bool hierarchyFits(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories, QString *error = nullptr);
QVariantMap componentGrid(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories, const QString &parentId, const QString &categoryId);
QString effectiveCategory(const QJsonArray &items, const QString &id);
bool positionLocked(const QJsonArray &items, const QString &id);
bool safeId(const QString &id);
QVariantMap action(const QVariantMap &input, QString *error = nullptr);
QVariantMap component(const QVariantMap &input, bool legacy, QString *error = nullptr);
QString unavailable(const QVariantMap &action, const AppRegistry *apps, const QVariantList &collections);
}
