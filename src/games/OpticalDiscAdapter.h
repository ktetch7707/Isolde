#pragma once

#include <QVariantList>
#include <QVariantMap>

class OpticalDiscAdapter {
public:
    static QVariantList mountedDiscs();
    static QVariantMap revalidate(const QString &observationId, const QString &device);
    static QVariantMap classifyRevalidation(const QString &observationId, const QString &device,
                                            const QVariantList &currentDiscs);
    static QVariantMap parseIsoVolumeDescriptor(const QByteArray &descriptor);
    static QVariantMap identityForDevice(const QString &device);
    static QVariantMap currentMedia(const QString &mediaId);
    static QVariantMap resolveRootReference(const QString &root);
    static QVariantMap classifyRootReference(const QString &root, const QVariantList &currentDiscs);
    static QString rootReference(const QVariantMap &disc);
    static bool isRootReference(const QString &root);
    static QString mediaIdFromRoot(const QString &root);
    static bool eject(const QString &device, QString *error = nullptr);
};
