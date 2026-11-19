#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class OpticalDiscParse
{
  public:
    virtual ~OpticalDiscParse() = default;

    static QVariantMap
    parseIsoVolumeDescriptor(const QByteArray& descriptor);
    static QVariantMap
    parseUdfVolumeDescriptors(const QByteArray& anchorDescriptor,
                              const QByteArray& volumeDescriptors,
                              quint32 sequenceLocation);
    static QVariantMap
    descriptorForFilesystem(const QString& filesystem,
                            const QVariantMap& iso9660Descriptor,
                            const QVariantMap& udfDescriptor);
};

class OpticalDiscIdentify
{
  public:
    virtual ~OpticalDiscIdentify() = default;

    static QVariantMap
    identityForDevice(const QString& device);
    static QVariantMap
    identityForContent(QVariantMap descriptor, const QString& contentId);
    static QString
    rootReference(const QVariantMap& disc);
    static bool
    isRootReference(const QString& root);
    static QString
    mediaIdFromRoot(const QString& root);
};

class OpticalDiscClassify
{
  public:
    virtual ~OpticalDiscClassify() = default;

    static QVariantMap
    currentMedia(const QString& mediaId);
    static QVariantMap
    resolveRootReference(const QString& root);
    static QVariantMap
    classifyRootReference(const QString& root, const QVariantList& currentDiscs);
    static QVariantMap
    classifySession(const QString& reference,
                    const QString& pinnedRoot,
                    bool approved,
                    const QVariantList& currentDiscs,
                    const QString& pinnedObservation = {});
};

class OpticalDiscHash
{
  public:
    virtual ~OpticalDiscHash() = default;

    static QVariantList
    mountedDiscs();
    static QByteArray
    hashContent(QIODevice* input, quint64 size);
    static QString
    mountObservationId(const QString& device,
                       const QString& filesystem,
                       const QString& label,
                       const QString& root,
                       const QString& sequence,
                       const QString& hashState,
                       const QString& mediaId,
                       const QString& mountInstance);
};

class OpticalDiscProbe
{
  public:
    virtual ~OpticalDiscProbe() = default;

    static QVariantMap
    classifyRevalidation(const QString& observationId, const QString& device, const QVariantList& currentDiscs);
    static QVariantMap
    revalidate(const QString& observationId, const QString& device);
    static void
    retryIdentity(const QString& device);
    static QVariantMap
    deviceDiagnostics(const QString& device);
};
