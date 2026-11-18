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

    virtual QVariantMap
    parseIsoVolumeDescriptor(const QByteArray& descriptor) = 0;
    virtual QVariantMap
    parseUdfVolumeDescriptors(const QByteArray& anchorDescriptor,
                              const QByteArray& volumeDescriptors,
                              quint32 sequenceLocation) = 0;
    virtual QVariantMap
    descriptorForFilesystem(const QString& filesystem,
                            const QVariantMap& iso9660Descriptor,
                            const QVariantMap& udfDescriptor) = 0;
};

class OpticalDiscIdentify
{
  public:
    virtual ~OpticalDiscIdentify() = default;

    virtual QVariantMap
    identityForDevice(const QString& device) = 0;
    virtual QVariantMap
    identityForContent(QVariantMap descriptor, const QString& contentId) = 0;
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

    virtual QVariantMap
    currentMedia(const QString& mediaId) = 0;
    static QVariantMap
    resolveRootReference(const QString& root);
    virtual QVariantMap
    classifyRootReference(const QString& root, const QVariantList& currentDiscs) = 0;
    virtual QVariantMap
    classifySession(const QString& reference,
                    const QString& pinnedRoot,
                    bool approved,
                    const QVariantList& currentDiscs,
                    const QString& pinnedObservation = {}) = 0;
};

class OpticalDiscHash
{
  public:
    virtual ~OpticalDiscHash() = default;

    static QVariantList
    mountedDiscs();
    static QVariantMap
    contentIdentity(const QString& device);
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

    virtual QVariantMap
    classifyRevalidation(const QString& observationId, const QString& device, const QVariantList& currentDiscs) = 0;
    virtual QVariantMap
    revalidate(const QString& observationId, const QString& device) = 0;
    virtual void
    retryIdentity(const QString& device) = 0;
    virtual QVariantMap
    deviceDiagnostics(const QString& device) = 0;
};
