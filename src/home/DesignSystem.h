#pragma once
#include <QJsonObject>
#include <QVariantMap>

// Profile-local declarative library. Resolution never mutates authored data.
namespace DesignSystem {
bool valid(const QVariantMap &library);
bool validDocument(const QJsonObject &document, QString *error);
QVariantMap styleProperties(const QVariantMap &value);
bool validResponsive(const QVariantMap &rule);
QVariantMap presentation(const QVariantMap &authored, const QVariantMap &library);
QVariantMap responsive(const QVariantMap &authored, double width, double height, double unit = 1);
}
