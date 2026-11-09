#include "OpticalDiscAdapter.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QFile>
#include <QDir>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QSet>

namespace {
quint16 le16(const QByteArray &bytes,int offset)
{
    return quint8(bytes.at(offset)) | (quint16(quint8(bytes.at(offset+1)))<<8);
}
quint32 le32(const QByteArray &bytes,int offset)
{
    return quint32(quint8(bytes.at(offset))) | (quint32(quint8(bytes.at(offset+1)))<<8)
        | (quint32(quint8(bytes.at(offset+2)))<<16) | (quint32(quint8(bytes.at(offset+3)))<<24);
}
}

QVariantMap OpticalDiscAdapter::parseIsoVolumeDescriptor(const QByteArray &descriptor)
{
    if(descriptor.size()!=2048 || quint8(descriptor[0])!=1
        || descriptor.mid(1,5)!="CD001" || quint8(descriptor[6])!=1)return {};
    const auto volumeId=descriptor.mid(40,32).trimmed();
    const auto volumeSet=descriptor.mid(190,128).trimmed();
    const auto blocks=le32(descriptor,80), blockSize=le16(descriptor,128);
    const auto discNumber=le16(descriptor,124), discCount=le16(descriptor,120);
    if(volumeId.isEmpty() || (discCount>1 && volumeSet.isEmpty()) || blocks==0 || blockSize!=2048 || discCount==0
        || discNumber==0 || discNumber>discCount || discCount>99)return {};
    const auto identity=descriptor.mid(40,32)+descriptor.mid(80,4)+descriptor.mid(120,2)
        +descriptor.mid(124,2)+descriptor.mid(190,128);
    const auto setIdentity=volumeSet.isEmpty()?identity:volumeSet+descriptor.mid(120,2);
    return {{"mediaId",QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex())},
        {"mediaSetId",QString::fromLatin1(QCryptographicHash::hash(setIdentity,QCryptographicHash::Sha256).toHex())},
        {"discNumber",discNumber},{"discCount",discCount},{"volumeIdentifier",QString::fromLatin1(volumeId)}};
}

QVariantMap OpticalDiscAdapter::identityForDevice(const QString &device)
{
    if(!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch())return {};
    QFile block(device);
    if(!block.open(QIODevice::ReadOnly) || !block.seek(16*2048))return {};
    return parseIsoVolumeDescriptor(block.read(2048));
}

QString OpticalDiscAdapter::rootReference(const QVariantMap &disc)
{
    const auto id=disc.value("mediaId").toString();
    const auto set=disc.value("mediaSetId").toString();
    const int number=disc.value("discNumber").toInt(),count=disc.value("discCount").toInt();
    if(!QRegularExpression("^[0-9a-f]{64}$").match(id).hasMatch()
        || !QRegularExpression("^[0-9a-f]{64}$").match(set).hasMatch()
        || number<1 || count<number || count>99)return {};
    return "/__isolde_optical__/"+id+"/"+set+"/"+QString::number(number)+"/"+QString::number(count);
}

bool OpticalDiscAdapter::isRootReference(const QString &root)
{
    static const QRegularExpression pattern("^/__isolde_optical__/([0-9a-f]{64})/([0-9a-f]{64})/([1-9][0-9]?)/([1-9][0-9]?)$");
    const auto match=pattern.match(root);
    if(!match.hasMatch())return false;
    bool numberOk=false,countOk=false;
    const int number=match.captured(3).toInt(&numberOk),count=match.captured(4).toInt(&countOk);
    return numberOk && countOk && number<=count && count<=99 && QDir::cleanPath(root)==root;
}

QString OpticalDiscAdapter::mediaIdFromRoot(const QString &root)
{
    if(!isRootReference(root))return {};
    return root.split('/').at(2);
}

QVariantMap OpticalDiscAdapter::currentMedia(const QString &mediaId)
{
    if(!QRegularExpression("^[0-9a-f]{64}$").match(mediaId).hasMatch())
        return {{"state","invalid"}};
    for(const auto &item:mountedDiscs()){
        const auto disc=item.toMap();
        if(disc.value("state")=="mounted" && disc.value("mediaId")==mediaId)
            return {{"state","ready"},{"disc",disc}};
    }
    return {{"state","missing"}};
}

QVariantMap OpticalDiscAdapter::resolveRootReference(const QString &root)
{
    return classifyRootReference(root,mountedDiscs());
}

QVariantMap OpticalDiscAdapter::classifyRootReference(const QString &root,const QVariantList &currentDiscs)
{
    if(!isRootReference(root))return {{"state","invalid"}};
    const auto parts=root.split('/');
    const auto mediaId=parts.at(2),setId=parts.at(3);
    const int expectedNumber=parts.at(4).toInt(),expectedCount=parts.at(5).toInt();
    QVariantMap wrongDisc;
    for(const auto &item:currentDiscs){
        const auto disc=item.toMap();
        if(disc.value("state")!="mounted")continue;
        if(disc.value("mediaId")==mediaId && disc.value("mediaSetId")==setId
            && disc.value("discNumber").toInt()==expectedNumber
            && disc.value("discCount").toInt()==expectedCount)
            return {{"state","ready"},{"disc",disc}};
        if(disc.value("mediaSetId")==setId && wrongDisc.isEmpty())wrongDisc=disc;
    }
    if(!wrongDisc.isEmpty())return {{"state","wrong-disc"},{"disc",wrongDisc},
        {"expectedDisc",expectedNumber},{"discCount",expectedCount}};
    return {{"state","missing"},{"expectedDisc",expectedNumber},{"discCount",expectedCount}};
}

QVariantList OpticalDiscAdapter::mountedDiscs()
{
    QVariantList result;
    QSet<QString> mountedDevices;
    for (const auto &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || !storage.isReadOnly()) continue;
        const auto device = QString::fromLatin1(storage.device());
        if (!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch()) continue;
        const auto filesystem = QString::fromLatin1(storage.fileSystemType()).toLower();
        if (filesystem != "iso9660" && filesystem != "udf") continue;
        const auto root = storage.rootPath();
        const QFileInfo mount(root);
        if (root == "/" || !mount.isDir() || mount.canonicalFilePath() != root) continue;
        mountedDevices.insert(device);
        const auto identity=filesystem=="iso9660"?identityForDevice(device):QVariantMap{};
        const auto observed = device + '\n' + filesystem + '\n' + storage.name() + '\n' + root
            + '\n' + identity.value("mediaId").toString();
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
            {"mediaId",identity.value("mediaId")},
            {"mediaSetId",identity.value("mediaSetId")},
            {"discNumber",identity.value("discNumber")},
            {"discCount",identity.value("discCount")},
            {"volumeIdentifier",identity.value("volumeIdentifier")},
            {"directPlaySupported",!identity.isEmpty()},
            {"readOnly", true},
            {"state", "mounted"}
        });
        if (result.size() >= 4) break;
    }
    if(result.size()<4){
        const QDir blockDevices("/sys/class/block");
        for(const auto &name:blockDevices.entryList({"sr[0-9]*"},QDir::Dirs|QDir::NoDotAndDotDot)){
            const auto device="/dev/"+name;
            if(mountedDevices.contains(device) || !QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch())
                continue;
            QFile sizeFile("/sys/class/block/"+name+"/size");
            if(!sizeFile.open(QIODevice::ReadOnly))continue;
            bool sizeOk=false;
            const auto sectors=sizeFile.read(64).trimmed().toULongLong(&sizeOk);
            if(!sizeOk || sectors==0)continue;
            const auto identity=identityForDevice(device);
            const auto observed=device+"\nunmounted\n"+identity.value("mediaId").toString();
            result.append(QVariantMap{
                {"schemaVersion",1},{"sourceType","pc-optical-disc"},
                {"observationId",QString::fromLatin1(QCryptographicHash::hash(observed.toUtf8(),QCryptographicHash::Sha256).toHex())},
                {"device",device},{"mountRoot",QString()},{"filesystem","unknown"},
                {"volumeLabel",QString()},{"mediaId",identity.value("mediaId")},
                {"mediaSetId",identity.value("mediaSetId")},{"discNumber",identity.value("discNumber")},
                {"discCount",identity.value("discCount")},{"volumeIdentifier",identity.value("volumeIdentifier")},
                {"directPlaySupported",!identity.isEmpty()},{"readOnly",true},{"state","unmounted"}
            });
            if(result.size()>=4)break;
        }
    }
    return result;
}

QVariantMap OpticalDiscAdapter::revalidate(const QString &observationId, const QString &device)
{
    return classifyRevalidation(observationId,device,mountedDiscs());
}

bool OpticalDiscAdapter::mountReadOnly(const QString &device,QString *error)
{
    bool available=false;
    for(const auto &item:mountedDiscs()){
        const auto disc=item.toMap();
        if(disc.value("device")==device && disc.value("state")=="unmounted")available=true;
    }
    const auto call=[](const QString &service,const QString &path,const QString &interface,
                       const QString &method,const QVariantList &arguments){
        QDBusInterface object(service,path,interface,QDBusConnection::systemBus());
        return object.callWithArgumentList(QDBus::Block,method,arguments);
    };
    QString mountPoint;
    if(!mountUsing(device,available,call,&mountPoint,error))return false;
    QStorageInfo mounted(mountPoint);
    mounted.refresh();
    if(!mounted.isValid() || !mounted.isReady() || !mounted.isReadOnly()
        || QString::fromLatin1(mounted.device())!=device){
        if(error)*error="A mount was requested, but Isolde could not verify the same optical device is mounted read-only. Do not use it until refreshed.";
        return false;
    }
    return true;
}

bool OpticalDiscAdapter::mountUsing(const QString &device,bool mediaAvailable,const DbusCall &call,
                                    QString *mountPoint,QString *error)
{
    if(error)error->clear();
    if(mountPoint)mountPoint->clear();
    if(!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch()){
        if(error)*error="Invalid optical device.";
        return false;
    }
    if(!mediaAvailable){
        if(error)*error="No unmounted optical media is available in this drive. Refresh and retry.";
        return false;
    }
    if(!call){
        if(error)*error="Read-only mount is unavailable; the optical drive service is not responding.";
        return false;
    }
    const auto object="/org/freedesktop/UDisks2/block_devices/"+device.mid(QString("/dev/").size());
    const auto reply=call("org.freedesktop.UDisks2",object,"org.freedesktop.UDisks2.Filesystem",
                          "Mount",{QVariantMap{{"options","ro"}}});
    if(reply.type()==QDBusMessage::ErrorMessage){
        if(error)*error="The disc could not be mounted read-only. Check that media is present and readable, then refresh; no installer or disc content was started.";
        return false;
    }
    if(reply.arguments().isEmpty()){
        if(error)*error="The drive did not confirm a mountpoint. Refresh optical media before continuing.";
        return false;
    }
    const auto path=reply.arguments().first().toString();
    if(!QDir::isAbsolutePath(path) || QDir::cleanPath(path)!=path){
        if(error)*error="The drive returned an invalid mountpoint. Refresh optical media before continuing.";
        return false;
    }
    if(mountPoint)*mountPoint=path;
    return true;
}

bool OpticalDiscAdapter::eject(const QString &device,QString *error)
{
    bool mounted=false;
    for(const auto &volume:QStorageInfo::mountedVolumes())
        mounted|=volume.isValid() && volume.isReady() && QString::fromLatin1(volume.device())==device;
    const auto call=[](const QString &service,const QString &path,const QString &interface,
                       const QString &method,const QVariantList &arguments){
        QDBusInterface object(service,path,interface,QDBusConnection::systemBus());
        return object.callWithArgumentList(QDBus::Block,method,arguments);
    };
    return ejectUsing(device,mounted,call,error);
}

bool OpticalDiscAdapter::ejectUsing(const QString &device,bool mounted,const DbusCall &call,QString *error)
{
    if(error)error->clear();
    if(!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch()){
        if(error)*error="Invalid optical device.";
        return false;
    }
    if(!call){
        if(error)*error="Safe eject is unavailable; the optical drive service is not responding.";
        return false;
    }
    const auto name=device.mid(QString("/dev/").size());
    const auto object="/org/freedesktop/UDisks2/block_devices/"+name;
    if(mounted){
        const auto reply=call("org.freedesktop.UDisks2",object,"org.freedesktop.UDisks2.Filesystem",
                              "Unmount",{QVariantMap{}});
        if(reply.type()==QDBusMessage::ErrorMessage){
            if(error)*error="The disc is in use and could not be safely unmounted. Close the game or operation and retry.";
            return false;
        }
    }
    const auto driveReply=call("org.freedesktop.UDisks2",object,"org.freedesktop.DBus.Properties",
                               "Get",{QString("org.freedesktop.UDisks2.Block"),QString("Drive")});
    if(driveReply.type()==QDBusMessage::ErrorMessage || driveReply.arguments().isEmpty()){
        if(error)*error="Safe eject is unavailable; the optical drive service is not responding.";
        return false;
    }
    const auto drivePath=driveReply.arguments().at(0).value<QDBusVariant>().variant().value<QDBusObjectPath>().path();
    if(drivePath.isEmpty() || drivePath=="/"){
        if(error)*error="Safe eject is unavailable; the optical drive identity is incomplete.";
        return false;
    }
    const auto reply=call("org.freedesktop.UDisks2",drivePath,"org.freedesktop.UDisks2.Drive","Eject",{QVariantMap{}});
    if(reply.type()==QDBusMessage::ErrorMessage){
        if(error)*error="The drive refused safe eject because media is busy or unavailable. Nothing was force-ejected.";
        return false;
    }
    return true;
}

QVariantMap OpticalDiscAdapter::classifyRevalidation(const QString &observationId, const QString &device,
                                                      const QVariantList &currentDiscs)
{
    for(const auto &item:currentDiscs){
        const auto disc=item.toMap();
        if(disc.value("observationId")==observationId)
            return {{"schemaVersion",1},{"state",disc.value("state")=="mounted"?"ready":"unmounted"},{"disc",disc}};
    }
    for(const auto &item:currentDiscs){
        const auto disc=item.toMap();
        if(disc.value("device")==device)
            return {{"schemaVersion",1},{"state","replaced"},{"disc",disc}};
    }
    return {{"schemaVersion",1},{"state","missing"}};
}
