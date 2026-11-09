#pragma once

#include <QVariantList>
#include <QVariantMap>

class OpticalDiscAdapter {
public:
    static QVariantList mountedDiscs();
    static QVariantMap revalidate(const QString &observationId, const QString &device);
    static QVariantMap classifyRevalidation(const QString &observationId, const QString &device,
                                            const QVariantList &currentDiscs);
};
