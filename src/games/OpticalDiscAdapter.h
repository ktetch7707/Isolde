#pragma once

#include <QVariantList>
#include <QVariantMap>
#include <QDBusMessage>
#include <functional>
class QIODevice;

class OpticalDiscAdapter {
public:
    using DbusCall = std::function<QDBusMessage(const QString &, const QString &, const QString &,
                                                 const QString &, const QVariantList &)>;
    static QVariantList mountedDiscs();
    static QVariantMap revalidate(const QString &observationId, const QString &device);
    static QVariantMap classifyRevalidation(const QString &observationId, const QString &device,
                                            const QVariantList &currentDiscs);
    static QVariantMap parseIsoVolumeDescriptor(const QByteArray &descriptor);
    static QVariantMap identityForDevice(const QString &device);
    static QByteArray hashContent(QIODevice *input, quint64 size);
    static QVariantMap identityForContent(QVariantMap descriptor, const QString &contentId);
    static void retryIdentity(const QString &device);
    static QVariantMap currentMedia(const QString &mediaId);
    static QVariantMap resolveRootReference(const QString &root);
    static QVariantMap classifyRootReference(const QString &root, const QVariantList &currentDiscs);
    static QVariantMap classifySession(const QString &reference, const QString &pinnedRoot,
                                       bool approved, const QVariantList &currentDiscs,
                                       const QString &pinnedObservation = {});
    static QVariantMap latchSession(const QVariantMap &previous, QVariantMap current);
    static QString rootReference(const QVariantMap &disc);
    static bool isRootReference(const QString &root);
    static QString mediaIdFromRoot(const QString &root);
    static bool mountReadOnly(const QString &device, QString *error = nullptr);
    static bool mountUsing(const QString &device, bool mediaAvailable, const DbusCall &call,
                           QString *mountPoint, QString *error = nullptr);
    static bool ejectUsing(const QString &device, bool mounted, const DbusCall &call,
                           QString *error = nullptr);
    static bool eject(const QString &device, QString *error = nullptr);
};
