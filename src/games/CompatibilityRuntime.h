#pragma once
#include <QVariantMap>

class CompatibilityRuntime {
public:
    static QVariantMap discover();
    static QVariantMap classify(bool present, bool trusted);
};
