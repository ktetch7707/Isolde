#pragma once
#include <QVariantMap>

namespace GuideRules {
// Evaluate already-validated data against a Kodi metadata snapshot. Missing
// metadata is never guessed; only the explicit missing operator matches it.
bool matches(const QVariantMap &rule, const QVariantMap &metadata);
// Shared preview/store selection, including explicit Kodi watch-state policy.
bool matchesChannel(const QVariantMap &definition, const QVariantMap &metadata);
}
