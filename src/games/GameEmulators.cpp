#include "GameEmulators.h"
#include "GameResourceLimits.h"
#include "media/MediaGrants.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
bool protectedPath(const QString &path) {
    if(!QDir::isAbsolutePath(path) || QDir::cleanPath(path)!=path)return false;
    auto info=QFileInfo(path);
    while(true){if(info.isSymLink())return false;if(info.absoluteFilePath()=="/")break;
        const auto parent=QFileInfo(info.absolutePath());if(parent.absoluteFilePath()==info.absoluteFilePath())break;info=parent;}
    return true;
}
bool save(const QString &path,const QByteArray &bytes,QString *error) {
    if(!protectedPath(path) || !QDir().mkpath(QFileInfo(path).absolutePath())){*error="Cannot prepare private emulator storage.";return false;}
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()){
        *error="Cannot save emulator setup. Previous settings were preserved. "+file.errorString();return false;}
    QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner);return true;
}
bool psxFirmware(const QByteArray &bytes) {
    static const QSet<QByteArray> hashes{
        "b05def971d8ec59f346f2d9ac21fb742e3eb6917",
        "0555c6fae8906f3f09baf5988f00e55f88e9f30b",
        "f6bc2d1f5eb6593de7d089c425ac681d6fffd3f0"};
    return bytes.size()==512*1024
        && hashes.contains(QCryptographicHash::hash(bytes,QCryptographicHash::Sha1).toHex());
}
bool prunePsxFirmware(const QString &root,const QString &keep,QString *error){
    const auto directory=root+"/firmware";
    if(!protectedPath(directory)){*error="Private firmware storage is redirected.";return false;}
    QDir dir(directory);if(!dir.exists())return true;
    const auto names=dir.entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System);
    if(names.size()>8){*error="Firmware storage contains too many entries. Review it before importing more.";return false;}
    for(const auto &name:names){
        if(name==keep || !QRegularExpression("^[a-f0-9]{64}\\.bin$").match(name).hasMatch())continue;
        const auto path=dir.filePath(name);const QFileInfo info(path);
        if(info.isSymLink() || !info.isFile() || info.size()!=512*1024)continue;
        QFile file(path);if(!file.open(QIODevice::ReadOnly))continue;
        const auto bytes=file.read(512*1024+1);
        if(!psxFirmware(bytes) || QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".bin"!=name)continue;
        if(!QFile::remove(path)){*error="A previous private firmware copy could not be retired.";return false;}
    }
    return true;
}
bool pruneFirmware(const QString &root,const QString &keep,QString *error){
    const auto directory=root+"/firmware";
    if(!protectedPath(directory)){*error="Private firmware storage is redirected.";return false;}
    QDir dir(directory);
    if(!dir.exists())return true;
    const auto names=dir.entryList(QDir::Files|QDir::NoDotAndDotDot);
    if(names.size()>8){*error="Firmware storage contains too many entries. Review it before importing more.";return false;}
    for(const auto &name:names){
        if(name==keep || !QRegularExpression("^[a-f0-9]{64}\\.bin$").match(name).hasMatch())continue;
        const auto path=dir.filePath(name);const QFileInfo info(path);
        if(info.isSymLink() || !info.isFile() || (info.size()!=4*1024*1024 && info.size()!=8*1024*1024))continue;
        QFile file(path);if(!file.open(QIODevice::ReadOnly))continue;
        const auto bytes=file.read(8*1024*1024+1);
        if(bytes.size()!=info.size() || !bytes.contains("ROMDIR") || !bytes.contains("ROMVER")
            || QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".bin"!=name)continue;
        if(!QFile::remove(path)){*error="A previous private firmware copy could not be retired.";return false;}
    }
    return true;
}
bool valid(const QVariantMap &value){
    const QSet<QString> keys{"schemaVersion","video","controller","firmware"};
    const auto actual=value.keys();
    const auto versionType=value.value("schemaVersion").metaType().id();
    return QSet<QString>(actual.begin(),actual.end())==keys && (versionType==QMetaType::Int || versionType==QMetaType::LongLong || versionType==QMetaType::Double)
        && value.value("video").metaType().id()==QMetaType::QString && value.value("controller").metaType().id()==QMetaType::QString && value.value("firmware").metaType().id()==QMetaType::QString
        && value.value("schemaVersion").toDouble()==1 && QStringList{"automatic","software"}.contains(value.value("video").toString())
        && QStringList{"xbox","xbox-wii-classic"}.contains(value.value("controller").toString()) && (value.value("firmware").toString().isEmpty()
            || QRegularExpression("^[a-f0-9]{64}\\.bin$").match(value.value("firmware").toString()).hasMatch());
}
}
QVariantList GameEmulators::definitions(){
    QFile file(":/config/game-runtimes.json");
    if(!file.exists())file.setFileName(QStringLiteral(ISOLDE_GAME_RUNTIME_SOURCE));
    if(!file.open(QIODevice::ReadOnly) || file.size()>65536)return {};
    const auto doc=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    const auto documentKeys=doc.keys();
    const QSet<QString> expectedKeys{"schemaVersion","target","generationCoverage","generations","runtimes"};
    const auto schemaType=doc.value("schemaVersion").metaType().id();
    if(documentKeys.size()!=expectedKeys.size() || !std::all_of(documentKeys.cbegin(),documentKeys.cend(),[&expectedKeys](const QString &key){return expectedKeys.contains(key);})
        || (schemaType!=QMetaType::Int && schemaType!=QMetaType::LongLong && schemaType!=QMetaType::Double)
        || doc.value("schemaVersion").toDouble()!=2)return {};
    const auto target=doc.value("target").toMap();
    if(target.value("os").toString()!="ubuntu" || target.value("version").toString()!="26.04"
        || target.value("architectures").toStringList()!=QStringList{"amd64"}
        || target.value("supportBoundary").toString().isEmpty())return {};
    const auto generations=doc.value("generations").toMap();
    if(generations.isEmpty())return {};
    static const QSet<QString> generationKeys{"architecture","installer","account","save","dependency","budget","firmwareRequired","unsupportedReason"};
    for(const auto &key:generations.keys()){
        const auto value=generations.value(key).toMap();
        if(!value.isEmpty()){
            const QSet<QString> actualKeys(value.keys());
            if(!std::all_of(actualKeys.cbegin(),actualKeys.cend(),[&generationKeys](const QString &k){return generationKeys.contains(k);}))return {};
            const auto budget=value.value("budget").toMap();
            if(!budget.isEmpty() && (!budget.value("diskMiB").isDouble() || !budget.value("ramMiB").isDouble()))return {};
        }
    }
    const auto runtimes=doc.value("runtimes").toList();
    const auto coverage=doc.value("generationCoverage").toMap();
    const auto integrated=coverage.value("integratedPlatforms").toStringList();
    const auto notIntegrated=coverage.value("notIntegrated").toStringList();
    QSet<QString> actualPlatforms,ids;
    for(const auto &entry:runtimes){
        const auto item=entry.toMap();
        const auto id=item.value("id").toString(),engine=item.value("engine").toString();
        const auto packages=item.value("packages").toStringList();
        const auto formats=item.value("formats").toStringList();
        const auto platforms=item.value("platforms").toStringList();
        static const QSet<QString> engines{"ppsspp","dolphin","pcsx2","retroarch","rpcs3","shadps4","vita3k","xemu","xenia","cemu"};
        if(item.isEmpty() || !QRegularExpression("^[a-z0-9-]{1,48}$").match(id).hasMatch()
            || ids.contains(id) || !engines.contains(engine) || item.value("binary").toString().isEmpty()
            || !QDir::isAbsolutePath(item.value("binary").toString()) || packages.isEmpty()
            || platforms.isEmpty() || formats.isEmpty() || item.value("firmwareRequired").metaType().id()!=QMetaType::Bool
            || (item.contains("core") && !QDir::isAbsolutePath(item.value("core").toString())))return {};
        const QSet<QString> packageSet(packages.cbegin(),packages.cend());
        if(engine=="retroarch"){
            int corePackages=0;
            for(const auto &package:packages)if(package.startsWith("libretro-"))++corePackages;
            if(!item.contains("core") || !packageSet.contains("retroarch") || corePackages!=1)return {};
        }else if(engine=="rpcs3"){
            if(item.contains("core") || packageSet!=QSet<QString>{"rpcs3"})return {};
        }else if(engine=="shadps4"){
            if(item.contains("core") || packageSet!=QSet<QString>{"shadps4"})return {};
        }else if(engine=="vita3k"){
            if(item.contains("core") || packageSet!=QSet<QString>{"vita3k"})return {};
        }else if(engine=="xemu"){
            if(item.contains("core") || packageSet!=QSet<QString>{"xemu"})return {};
        }else if(engine=="xenia"){
            if(item.contains("core") || packageSet!=QSet<QString>{"xenia"})return {};
        }else if(engine=="cemu"){
            if(item.contains("core") || packageSet!=QSet<QString>{"cemu"})return {};
        }else{
            const QSet<QString> expectedPackage=engine=="ppsspp"?QSet<QString>{"isolde-ppsspp"}
                :engine=="dolphin"?QSet<QString>{"dolphin-emu"}:QSet<QString>{"pcsx2"};
            if(item.contains("core") || packageSet!=expectedPackage)return {};
        }
        ids.insert(id);
        for(const auto &package:packages)if(!QRegularExpression("^[a-z0-9][a-z0-9+.-]{0,80}$").match(package).hasMatch())return {};
        for(const auto &format:formats)if(!QRegularExpression("^[a-z0-9]{1,8}$").match(format).hasMatch())return {};
        for(const auto &platform:platforms)actualPlatforms.insert(platform);
    }
    const QSet<QString> integratedSet(integrated.begin(),integrated.end());
    const QSet<QString> notIntegratedSet(notIntegrated.begin(),notIntegrated.end());
    if(runtimes.isEmpty() || integratedSet.size()!=integrated.size() || notIntegratedSet.size()!=notIntegrated.size()
        || actualPlatforms!=integratedSet)return {};
    for(const auto &platform:integratedSet)if(notIntegratedSet.contains(platform))return {};
    return runtimes;
}
QVariantMap GameEmulators::definition(const QString &id){for(const auto &item:definitions())if(item.toMap().value("id").toString()==id)return item.toMap();return {};}
QVariantMap GameEmulators::coverage(){
    QFile file(":/config/game-runtimes.json");
    if(!file.exists())file.setFileName(QStringLiteral(ISOLDE_GAME_RUNTIME_SOURCE));
    if(!file.open(QIODevice::ReadOnly) || file.size()>65536)return {};
    const auto doc=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    if(doc.value("schemaVersion").toInt()!=2 || definitions().isEmpty())return {};
    const auto target=doc.value("target").toMap();
    const QVariantMap budgets{
        {"catalogEntries",GamesResourceLimits::CatalogEntries},
        {"pageEntries",GamesResourceLimits::PageEntries},
        {"platforms",GamesResourceLimits::Platforms},
        {"collections",GamesResourceLimits::Collections},
        {"editionsPerGame",GamesResourceLimits::EditionsPerGame},
        {"provenanceRecords",GamesResourceLimits::ProvenanceRecords},
        {"catalogArchiveBytes",GamesResourceLimits::CatalogArchiveBytes},
        {"artworkRestoreBytes",GamesResourceLimits::ArtworkRestoreBytes},
        {"artworkStoreBytes",GamesResourceLimits::ArtworkStoreBytes}};
    return {{"target",target},{"budgets",budgets},
        {"integratedPlatforms",doc.value("generationCoverage").toMap().value("integratedPlatforms")},
        {"notIntegrated",doc.value("generationCoverage").toMap().value("notIntegrated")}};
}
QString GameEmulators::home(const QString &profile,const QString &id){
    if(definition(id).isEmpty() || (profile!="default" && (QUuid(profile).isNull() || QUuid(profile).toString(QUuid::WithoutBraces)!=profile)))return {};
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+profile+"/"+id+"/home";
}
QVariantMap GameEmulators::settings(const QString &profile,const QString &id,QString *error){
    const auto root=home(profile,id);if(root.isEmpty()){*error="Unknown emulator or profile.";return {};}
    const auto path=root+"/.config/isolde-emulator/settings.json";
    if(!protectedPath(path)){*error="Emulator settings are redirected. Repair storage before continuing.";return {};}
    if(!QFileInfo::exists(path))return {{"schemaVersion",1},{"video","automatic"},{"controller","xbox"},{"firmware",QStringLiteral("")}};
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>16384){*error="Cannot read emulator settings. Original data was preserved.";return {};}
    const auto result=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    if(!valid(result)){*error="Invalid emulator settings. Original data was preserved.";return {};}
    return result;
}
bool GameEmulators::playStationFirmwareReady(const QString &profile,QString *error){
    const auto value=settings(profile,"retroarch-psx",error);if(value.isEmpty())return false;
    const auto name=value.value("firmware").toString();
    if(name.isEmpty()){*error="Import your matching regional PlayStation firmware in Emulator setup before playing.";return false;}
    const auto path=home(profile,"retroarch-psx")+"/firmware/"+name;
    if(!protectedPath(path)){*error="PlayStation firmware storage is redirected.";return false;}
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size()!=512*1024){*error="PlayStation firmware is missing or invalid. Import it again.";return false;}
    const auto bytes=file.read(512*1024+1);
    if(!psxFirmware(bytes) || QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".bin"!=name){
        *error="PlayStation firmware changed or is invalid. Import it again.";return false;
    }
    return true;
}
bool GameEmulators::configure(const QString &profile,const QString &id,const QVariantMap &options,const QString &firmware,QString *error){
    auto value=settings(profile,id,error);if(value.isEmpty())return false;
    if(options.size()!=2 || !options.contains("video") || !options.contains("controller")){*error="Choose known emulator settings only.";return false;}
    value["video"]=options.value("video");value["controller"]=options.value("controller");
    if(!valid(value) || (id!="dolphin" && value.value("controller").toString()!="xbox")){*error="Choose a supported renderer and controller profile.";return false;}
    const auto root=home(profile,id);
    if(!firmware.isEmpty()){
        if(!definition(id).value("firmwareRequired").toBool()){*error="This runtime does not require imported firmware.";return false;}
        if(!(id=="retroarch-psx"?prunePsxFirmware(root,value.value("firmware").toString(),error)
                                 :pruneFirmware(root,value.value("firmware").toString(),error)))return false;
        const QFileInfo source(firmware);
        const auto grant=MediaGrants::validate(source.absolutePath());
        if(!grant.isEmpty() || !source.isFile() || !source.isReadable() || source.canonicalFilePath()!=firmware
            || (id=="retroarch-psx"?source.size()!=512*1024:(id=="vita3k"?source.size()!=4*1024*1024
            :(source.size()!=4*1024*1024 && source.size()!=8*1024*1024)))){
            if(id=="retroarch-psx"){*error="Choose your readable 512 KiB PlayStation firmware inside a game source folder.";return false;}
            if(id=="vita3k"){*error="Choose your readable 4 MiB PS Vita firmware file inside a game source folder.";return false;}
            *error=id=="xemu"||id=="xenia"?"Choose your readable 4 or 8 MiB Xbox firmware file inside a game source folder."
                :"Choose your readable 4 or 8 MiB PS2 firmware file inside a game source folder.";return false;}
        QFile file(firmware);if(!file.open(QIODevice::ReadOnly)){*error="Cannot read selected firmware.";return false;}
        const auto bytes=file.read(8*1024*1024+1);
        if(bytes.size()!=source.size() || (id=="retroarch-psx"?!psxFirmware(bytes):(!bytes.contains("ROMDIR") || !bytes.contains("ROMVER")))){
            *error=id=="retroarch-psx"?"The file is not a recognized SCPH-5500, 5501 or 5502 firmware image.":"The selected file is not recognized as PS2 firmware.";return false;}
        const auto name=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".bin";
        if(!save(root+"/firmware/"+name,bytes,error))return false;value["firmware"]=name;
    }
    if(!save(root+"/.config/isolde-emulator/settings.json",QJsonDocument::fromVariant(value).toJson(),error))return false;
    QString cleanupError;
    if(id=="retroarch-psx")prunePsxFirmware(root,value.value("firmware").toString(),&cleanupError);
    else pruneFirmware(root,value.value("firmware").toString(),&cleanupError);
    return true;
}
