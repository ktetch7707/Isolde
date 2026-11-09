#include "OpticalDiscAdapter.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>

QVariantList OpticalDiscAdapter::mountedDiscs()
{
    QVariantList result;
    for (const auto &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || !storage.isReadOnly()) continue;
        const auto device = QString::fromLatin1(storage.device());
        if (!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch()) continue;
        const auto filesystem = QString::fromLatin1(storage.fileSystemType()).toLower();
        if (filesystem != "iso9660" && filesystem != "udf") continue;
        const auto root = storage.rootPath();
        const QFileInfo mount(root);
        if (root == "/" || !mount.isDir() || mount.canonicalFilePath() != root) continue;
        const auto observed = device + '\n' + filesystem + '\n' + storage.name() + '\n' + root;
        const auto observationId = QString::fromLatin1(
            QCryptographicHash::hash(observed.toUtf8(), QCryptographicHash::Sha256).toHex());
        result.append(QVariantMap{
            {"schemaVersion", 1},
            {"sourceType", "pc-optical-disc"},
            {"observationId", observationId},
            {"device", device},
            {"mountRoot", root},
            {"filesystem", filesystem},
            {"volumeLabel", storage.name()},
            {"readOnly", true},
            {"state", "mounted"}
        });
        if (result.size() >= 4) break;
    }
    return result;
}

QVariantMap OpticalDiscAdapter::revalidate(const QString &observationId, const QString &device)
{
    return classifyRevalidation(observationId,device,mountedDiscs());
}

QVariantMap OpticalDiscAdapter::classifyRevalidation(const QString &observationId, const QString &device,
                                                      const QVariantList &currentDiscs)
{
    for(const auto &item:currentDiscs){
        const auto disc=item.toMap();
        if(disc.value("observationId")==observationId)
            return {{"schemaVersion",1},{"state","ready"},{"disc",disc}};
    }
    for(const auto &item:currentDiscs){
        const auto disc=item.toMap();
        if(disc.value("device")==device)
            return {{"schemaVersion",1},{"state","replaced"},{"disc",disc}};
    }
    return {{"schemaVersion",1},{"state","missing"}};
}
