#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <functional>

namespace HomeDesignData {
// These are the persisted managed-image fields used by layouts and snapshots.
inline QJsonValue artwork(QJsonValue value, const std::function<QString(const QString &)> &visit)
{
    if (value.isObject()) {
        auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if ((it.key() == "wallpaper" || it.key() == "customLogo") && it.value().isString() && !it.value().toString().isEmpty())
                it.value() = visit(it.value().toString());
            else it.value() = artwork(it.value(), visit);
        }
        return object;
    }
    if (value.isArray()) {
        auto array = value.toArray();
        for (qsizetype i = 0; i < array.size(); ++i) array[i] = artwork(array[i], visit);
        return array;
    }
    return value;
}
inline bool safeArtwork(const QString &path)
{
    return QRegularExpression("^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg|gif)$").match(path).hasMatch();
}
}
