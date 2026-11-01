#pragma once
#include <QVariantMap>

namespace GuideRules {
// Evaluate already-validated data against a Kodi metadata snapshot. Missing
// metadata is never guessed; only the explicit missing operator matches it.
bool matches(const QVariantMap &rule, const QVariantMap &metadata);
// Shared preview/store selection, including explicit Kodi watch-state policy.
bool matchesChannel(const QVariantMap &definition, const QVariantMap &metadata);
// UTC boundaries derived from civil time, including both DST folds. The
// returned rule applies until nextUtc; callers never cross that boundary.
QVariantMap calendarWindow(const QVariantMap &definition, qint64 utc);
}
