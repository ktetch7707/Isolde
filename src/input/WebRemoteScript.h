#pragma once
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

inline QString webRemoteScript(const QVariantMap &appearance)
{
    if(!appearance.contains("focusColor") || !appearance.contains("surfaceColor") || !appearance.contains("textColor"))return {};
    QFile script(":/isolde/web-remote.js");if(!script.open(QIODevice::ReadOnly))return {};
    return "("+QString::fromUtf8(script.readAll())+")("+QString::fromUtf8(QJsonDocument::fromVariant(appearance).toJson(QJsonDocument::Compact))+");";
}
