#pragma once
#include <QByteArray>
#include <QVariantMap>

// Shared import/runtime boundary. No URLs, executable data or persistent cache.
namespace ManagedGif {
bool matches(const QByteArray &data);
QVariantMap decode(const QByteArray &data, bool includeFrames, QString *error = nullptr);
}
