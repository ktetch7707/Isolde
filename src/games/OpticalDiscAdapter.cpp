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
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QThreadPool>
#include <QMap>
#ifdef Q_OS_LINUX
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/stat.h>
#ifndef BLKGETDISKSEQ
#define BLKGETDISKSEQ _IOR(0x12,128,__u64)
#endif
#endif

namespace {
constexpr quint64 MaxOpticalBytes=16ULL*1024*1024*1024;
struct HashState {
    quint64 sequence=0;
    quint64 size=0;
    QString state;
    QByteArray digest;
    QVariantMap descriptor;
};
struct HashCache {
    QMutex mutex;
    QMap<QString,HashState> hashes;
    int running=0;
};
HashCache *hashCache()
{
    static auto *cache=new HashCache;
    return cache;
}

quint16 le16(const QByteArray &bytes,int offset)
{
    return quint8(bytes.at(offset)) | (quint16(quint8(bytes.at(offset+1)))<<8);
}
quint32 le32(const QByteArray &bytes,int offset)
{
    return quint32(quint8(bytes.at(offset))) | (quint32(quint8(bytes.at(offset+1)))<<8)
        | (quint32(quint8(bytes.at(offset+2)))<<16) | (quint32(quint8(bytes.at(offset+3)))<<24);
}

#ifdef Q_OS_LINUX
bool diskInfo(QFile &file,quint64 *sequence,quint64 *size)
{
    return file.open(QIODevice::ReadOnly)
        && ::ioctl(file.handle(),BLKGETDISKSEQ,sequence)==0
        && ::ioctl(file.handle(),BLKGETSIZE64,size)==0
        && *sequence && *size && *size<=MaxOpticalBytes;
}

QString mountInstance(const QString &root)
{
#if defined(SYS_statx) && defined(STATX_MNT_ID)
    const auto path=QFile::encodeName(root);
    struct statx info{};
#ifdef STATX_MNT_ID_UNIQUE
    if(::syscall(SYS_statx,AT_FDCWD,path.constData(),AT_SYMLINK_NOFOLLOW,
                 STATX_MNT_ID_UNIQUE,&info)==0 && (info.stx_mask & STATX_MNT_ID_UNIQUE) && info.stx_mnt_id)
        return "unique:"+QString::number(info.stx_mnt_id);
#endif
    info={};
    if(::syscall(SYS_statx,AT_FDCWD,path.constData(),AT_SYMLINK_NOFOLLOW,
                 STATX_MNT_ID,&info)==0 && (info.stx_mask & STATX_MNT_ID) && info.stx_mnt_id)
        return "mount:"+QString::number(info.stx_mnt_id);
#else
    Q_UNUSED(root);
#endif
    return {};
}
#endif

QVariantMap contentIdentity(const QString &device)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(device);
    return {{"hashState","unavailable"}};
#else
    QFile file(device);
    quint64 sequence=0,size=0;
    if(!diskInfo(file,&sequence,&size))return {{"hashState","unavailable"}};
    auto *cache=hashCache();
    QMutexLocker lock(&cache->mutex);
    auto &hashes=cache->hashes;
    auto existing=hashes.find(device);
    if(existing!=hashes.end() && existing->sequence==sequence && existing->size==size)
        return {{"hashState",existing->state},{"contentId",QString::fromLatin1(existing->digest.toHex())},
            {"diskSequence",QString::number(sequence)},{"descriptor",existing->descriptor}};
    if(cache->running>=4)return {{"hashState","busy"},{"diskSequence",QString::number(sequence)}};
    if(hashes.size()>=4 && existing==hashes.end())hashes.erase(hashes.begin());
    hashes.insert(device,{sequence,size,"hashing",{},{}});
    ++cache->running;
    QThreadPool::globalInstance()->start([device,sequence,size]{
        QFile input(device);quint64 before=0,bytes=0,after=0,afterSize=0;
        QByteArray digest;QVariantMap descriptor;
        if(diskInfo(input,&before,&bytes) && before==sequence && bytes==size){
            if(size>=17*2048 && input.seek(16*2048))
                descriptor=OpticalDiscAdapter::parseIsoVolumeDescriptor(input.read(2048));
            if(input.seek(0))digest=OpticalDiscAdapter::hashContent(&input,size);
            if(::ioctl(input.handle(),BLKGETDISKSEQ,&after)!=0
                || ::ioctl(input.handle(),BLKGETSIZE64,&afterSize)!=0
                || after!=sequence || afterSize!=size)digest.clear();
        }
        auto *cache=hashCache();
        QMutexLocker guard(&cache->mutex);
        --cache->running;
        auto entry=cache->hashes.find(device);
        if(entry!=cache->hashes.end() && entry->sequence==sequence && entry->size==size){
            entry->digest=digest;
            entry->descriptor=digest.isEmpty()?QVariantMap{}:descriptor;
            entry->state=digest.size()==32?"ready":"unreadable";
        }
    });
    return {{"hashState","hashing"},{"diskSequence",QString::number(sequence)}};
#endif
}
}

QString OpticalDiscAdapter::mountObservationId(const QString &device,const QString &filesystem,
                                              const QString &label,const QString &root,
                                              const QString &sequence,const QString &hashState,
                                              const QString &mediaId,const QString &mountInstance)
{
    const auto fields=QStringList{device,filesystem,label,root,sequence,hashState,mediaId,mountInstance};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto &field:fields){
        const auto bytes=field.toUtf8();
        hash.addData(QByteArray::number(bytes.size()));
        hash.addData(":",1);
        hash.addData(bytes);
    }
    return QString::fromLatin1(hash.result().toHex());
}

QByteArray OpticalDiscAdapter::hashContent(QIODevice *input,quint64 size)
{
    if(!input || !input->isReadable() || !size || size>MaxOpticalBytes)return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    quint64 remaining=size;
    while(remaining){
        const auto chunk=input->read(qint64(qMin<quint64>(remaining,1024*1024)));
        if(chunk.isEmpty())return {};
        hash.addData(chunk);
        remaining-=quint64(chunk.size());
    }
    return hash.result();
}

void OpticalDiscAdapter::retryIdentity(const QString &device)
{
    auto *cache=hashCache();
    QMutexLocker lock(&cache->mutex);
    auto entry=cache->hashes.find(device);
    if(entry!=cache->hashes.end() && entry->state=="unreadable")cache->hashes.erase(entry);
}

QVariantMap OpticalDiscAdapter::identityForContent(QVariantMap descriptor,const QString &contentId)
{
    if(!QRegularExpression("^[0-9a-f]{64}$").match(contentId).hasMatch())return {};
    descriptor["mediaId"]=contentId;
    if(descriptor.value("mediaSetId").toString().isEmpty()){
        descriptor["mediaSetId"]=contentId;
        descriptor["discNumber"]=1;descriptor["discCount"]=1;
    }
    return rootReference(descriptor).isEmpty()?QVariantMap{}:descriptor;
}

QVariantMap OpticalDiscAdapter::parseIsoVolumeDescriptor(const QByteArray &descriptor)
{
    if(descriptor.size()!=2048 || quint8(descriptor[0])!=1
        || descriptor.mid(1,5)!="CD001" || quint8(descriptor[6])!=1)return {};
    const auto volumeId=descriptor.mid(40,32).trimmed();
    const auto volumeSet=descriptor.mid(190,128).trimmed();
    const auto blocks=le32(descriptor,80);
    const auto blockSize=le16(descriptor,128);
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

QVariantMap OpticalDiscAdapter::classifySession(const QString &reference,const QString &pinnedRoot,
                                               bool approved,const QVariantList &currentDiscs,
                                               const QString &pinnedObservation)
{
    auto classified=classifyRootReference(reference,currentDiscs);
    for(const auto &item:currentDiscs){
        const auto disc=item.toMap();
        if(disc.value("mountRoot").toString()==pinnedRoot
            && classifyRootReference(reference,{disc}).value("state")=="ready"){
            classified={{"state","ready"},{"disc",disc}};break;
        }
    }
    auto state=classified.value("state").toString();
    if(!approved)state="revoked";
    else if(state=="ready" && classified.value("disc").toMap().value("mountRoot").toString()!=pinnedRoot)
        state="relocated";
    else if(state=="ready" && !pinnedObservation.isEmpty()
        && classified.value("disc").toMap().value("observationId").toString()!=pinnedObservation)
        state="interrupted";
    else if(state=="ready")state="pinned";
    else if(state=="missing"){
        for(const auto &item:currentDiscs)
            if(item.toMap().value("mountRoot").toString()==pinnedRoot){state="replaced";break;}
    }
    return {{"schemaVersion",1},{"state",state},{"expectedDisc",reference.split('/').value(4).toInt()},
        {"discCount",reference.split('/').value(5).toInt()},{"hotSwapSupported",false},
        {"recovery",state=="pinned"?"":state=="revoked"
            ?"This profile's disc approval was revoked. Return Home and review access."
            :"The session uses a pinned read-only disc mount. Return Home, safely eject or reconnect the expected disc, then retry Play."}};
}

QVariantMap OpticalDiscAdapter::latchSession(const QVariantMap &previous,QVariantMap current)
{
    const auto previousState=previous.value("state").toString();
    if(current.value("state")=="pinned" && !previousState.isEmpty() && previousState!="pinned"){
        current["state"]="interrupted";
        current["recovery"]="The original session mount cannot be restored by reinsertion. Return Home and retry Play with the expected disc.";
    }
    return current;
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
        const auto content=contentIdentity(device);
        const auto contentId=content.value("contentId").toString();
        const auto identity=content.value("hashState")=="ready"
            ?identityForContent(filesystem=="iso9660"?content.value("descriptor").toMap():QVariantMap{},contentId):QVariantMap{};
#ifdef Q_OS_LINUX
        const auto instance=mountInstance(root);
#else
        const QString instance;
#endif
        const auto observationId=mountObservationId(device,filesystem,storage.name(),root,
            content.value("diskSequence").toString(),content.value("hashState").toString(),
            identity.value("mediaId").toString(),instance);
        result.append(QVariantMap{
            {"schemaVersion", 1},
            {"sourceType", "pc-optical-disc"},
            {"observationId", observationId},
            {"mountTracking",instance.isEmpty()?"unavailable":instance.startsWith("unique:")?"unique":"available"},
            {"device", device},
            {"mountRoot", root},
            {"filesystem", filesystem},
            {"volumeLabel", storage.name()},
            {"mediaId",identity.value("mediaId")},
            {"mediaSetId",identity.value("mediaSetId")},
            {"discNumber",identity.value("discNumber")},
            {"discCount",identity.value("discCount")},
            {"volumeIdentifier",identity.value("volumeIdentifier")},
            {"hashState",content.value("hashState")},
            {"directPlaySupported",!instance.isEmpty() && !contentId.isEmpty() && !rootReference(identity).isEmpty()},
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
            const auto observed=device+"\nunmounted";
            result.append(QVariantMap{
                {"schemaVersion",1},{"sourceType","pc-optical-disc"},
                {"observationId",QString::fromLatin1(QCryptographicHash::hash(observed.toUtf8(),QCryptographicHash::Sha256).toHex())},
                {"device",device},{"mountRoot",QString()},{"filesystem","unknown"},
                {"volumeLabel",QString()},{"mediaId",QString()},
                {"mediaSetId",QString()},{"discNumber",0},
                {"discCount",0},{"volumeIdentifier",QString()},{"hashState","unmounted"},
                {"directPlaySupported",false},{"readOnly",true},{"state","unmounted"}
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
