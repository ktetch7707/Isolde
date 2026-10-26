#pragma once
#include <QVariantMap>

namespace GuideRules {
// Evaluate already-validated data against a Kodi metadata snapshot. Missing
// metadata is never guessed; only the explicit missing operator matches it.
bool matches(const QVariantMap &rule, const QVariantMap &metadata);
}
