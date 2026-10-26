#include "GameEmulators.h"
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
    return doc.value("schemaVersion").toInt()==1?doc.value("runtimes").toList():QVariantList{};
}
QVariantMap GameEmulators::definition(const QString &id){for(const auto &item:definitions())if(item.toMap().value("id").toString()==id)return item.toMap();return {};}
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
bool GameEmulators::configure(const QString &profile,const QString &id,const QVariantMap &options,const QString &firmware,QString *error){
    auto value=settings(profile,id,error);if(value.isEmpty())return false;
    if(options.size()!=2 || !options.contains("video") || !options.contains("controller")){*error="Choose known emulator settings only.";return false;}
    value["video"]=options.value("video");value["controller"]=options.value("controller");
    if(!valid(value) || (id!="dolphin" && value.value("controller").toString()!="xbox")){*error="Choose a supported renderer and controller profile.";return false;}
    const auto root=home(profile,id);
    if(!firmware.isEmpty()){
        if(!definition(id).value("firmwareRequired").toBool()){*error="This runtime does not require imported firmware.";return false;}
        const QFileInfo source(firmware);
        const auto grant=MediaGrants::validate(source.absolutePath());
        if(!grant.isEmpty() || !source.isFile() || !source.isReadable() || source.canonicalFilePath()!=firmware
            || (source.size()!=4*1024*1024 && source.size()!=8*1024*1024)){
            *error="Choose your readable 4 or 8 MiB PS2 firmware file inside a game source folder.";return false;}
        QFile file(firmware);if(!file.open(QIODevice::ReadOnly)){*error="Cannot read selected firmware.";return false;}
        const auto bytes=file.read(8*1024*1024+1);
        if(bytes.size()!=source.size() || !bytes.contains("ROMDIR") || !bytes.contains("ROMVER")){*error="The selected file is not recognized as PS2 firmware.";return false;}
        const auto name=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".bin";
        if(!save(root+"/firmware/"+name,bytes,error))return false;value["firmware"]=name;
    }
    return save(root+"/.config/isolde-emulator/settings.json",QJsonDocument::fromVariant(value).toJson(),error);
}
