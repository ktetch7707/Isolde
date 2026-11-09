#include "GameLibrary.h"
#include "GameEmulators.h"
#include "SteamLibraryGrants.h"
#include "OpticalDiscAdapter.h"
#include "OpticalDiscGrants.h"
#include "ManagedGamePaths.h"
#include <QCoreApplication>
#include "app/AppRegistry.h"
#include "app/AppCatalog.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QImageReader>
#include <QBuffer>
#include <QCryptographicHash>
#include <QUrl>
#include <QFile>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif
#include "home/ComponentSchema.h"
#include "media/MediaGrants.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <QSaveFile>
#include <cmath>
#include <limits>

namespace {
constexpr int MaxGames = 50000;
constexpr int PageSize = 50;
constexpr int MaxPlatforms = 128;
constexpr int MaxCollections = 256;
constexpr int MaxTags = 32;
constexpr qsizetype MaxCatalogArchiveBytes = 32 * 1024 * 1024;
constexpr qsizetype MaxArtworkSourceBytes = 16 * 1024 * 1024;
constexpr qsizetype MaxManagedArtworkBytes = 12 * 1024 * 1024;
constexpr qint64 MaxArtworkPixels = 16 * 1024 * 1024;
constexpr qint64 MaxArtworkStoreBytes = 512LL * 1024 * 1024;
constexpr qint64 MaxRestoreArtworkBytes = 128LL * 1024 * 1024;
bool exactKeys(const QJsonObject &object,const QStringList &keys) {
    QStringList actual=object.keys();auto expected=keys;actual.sort();expected.sort();return actual==expected;
}
bool archiveInteger(const QJsonValue &value,qint64 *result) {
    if(!value.isDouble() || !std::isfinite(value.toDouble()) || std::floor(value.toDouble())!=value.toDouble()
        || value.toDouble()<0 || value.toDouble()>9007199254740991.0)return false;
    *result=qint64(value.toDouble());return true;
}
bool approvedMediaDirectory(const QString &profile,const QString &path) {
    if(!MediaGrants::validate(path).isEmpty())return false;
    for(const auto &grant:MediaGrants::folders(profile)){
        if(!MediaGrants::validate(grant,false).isEmpty())continue;
        if(path==grant || path.startsWith(grant+'/'))return true;
    }
    return false;
}
QByteArray normalizeArtwork(const QByteArray &source) {
    if(source.isEmpty() || source.size()>MaxArtworkSourceBytes)return {};
    QBuffer input;input.setData(source);if(!input.open(QIODevice::ReadOnly))return {};
    QImageReader reader(&input);reader.setAutoTransform(true);
    const auto format=reader.format().toLower();
    const auto size=reader.size();
    if((format!="png" && format!="jpg" && format!="jpeg")
        || size.width()<1 || size.height()<1 || size.width()>4096 || size.height()>4096
        || qint64(size.width())*size.height()>MaxArtworkPixels)return {};
    const auto image=reader.read();
    if(image.isNull() || image.width()>4096 || image.height()>4096
        || qint64(image.width())*image.height()>MaxArtworkPixels)return {};
    QByteArray normalized;QBuffer output(&normalized);
    if(!output.open(QIODevice::WriteOnly) || !image.save(&output,"PNG") || normalized.isEmpty()
        || normalized.size()>MaxManagedArtworkBytes)return {};
    return normalized;
}
QString artworkHash(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
bool persistArtwork(const QString &databasePath,const QByteArray &bytes,QString *hash,QString *error) {
    if(bytes.isEmpty() || bytes.size()>MaxManagedArtworkBytes){if(error)*error=QStringLiteral("Artwork exceeds the managed size limit.");return false;}
    const auto directory=databasePath+".artwork";
    if(!QDir().mkpath(directory) || QFileInfo(directory).isSymLink()
        || QFileInfo(directory).canonicalFilePath()!=QDir::cleanPath(directory)){
        if(error)*error=QStringLiteral("Cannot create a private artwork folder.");return false;
    }
    if(!QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)){
        if(error)*error=QStringLiteral("Cannot secure the private artwork folder.");return false;
    }
    const auto digest=artworkHash(bytes),path=directory+"/"+digest+".png";
    const auto info=QFileInfo(path);
    if(info.exists()){
        QFile existing(path);
        if(info.isSymLink() || !info.isFile() || info.canonicalFilePath()!=path
            || info.size()!=bytes.size() || !existing.open(QIODevice::ReadOnly) || existing.readAll()!=bytes){
            if(error)*error=QStringLiteral("Managed artwork content does not match its identity.");return false;
        }
    }else{
        qint64 used=0;
        const auto files=QDir(directory).entryInfoList({"*.png"},QDir::Files|QDir::NoSymLinks);
        for(const auto &file:files)used+=file.size();
        if(used+bytes.size()>MaxArtworkStoreBytes){if(error)*error=QStringLiteral("The profile artwork store is full.");return false;}
        QSaveFile file(path);file.setDirectWriteFallback(false);
        if(!file.open(QIODevice::WriteOnly)
            || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
            || file.write(bytes)!=bytes.size() || !file.commit()){
            file.cancelWriting();if(error)*error=QStringLiteral("Could not save managed artwork.");return false;
        }
    }
    if(hash)*hash=digest;return true;
}
QVariantMap row(const QSqlQuery &query,const QString &artworkDirectory) {
    QStringList tags;
    const auto tagArray=QJsonDocument::fromJson(query.value(6).toByteArray()).array();
    for(const auto &tag:tagArray)if(tag.isString())tags.append(tag.toString());
    QStringList aliases;
    const auto aliasArray=QJsonDocument::fromJson(query.value(7).toByteArray()).array();
    for(const auto &alias:aliasArray)if(alias.isString())aliases.append(alias.toString());
    const auto hash=query.value(9).toString();
    const auto artworkPath=artworkDirectory.isEmpty()?QString():artworkDirectory+"/"+hash+".png";
    const auto artworkInfo=QFileInfo(artworkPath);
    const auto artwork=QRegularExpression("^[0-9a-f]{64}$").match(hash).hasMatch()
        && artworkInfo.isFile() && !artworkInfo.isSymLink() && artworkInfo.canonicalFilePath()==artworkPath
        && artworkInfo.size()>0 && artworkInfo.size()<=MaxManagedArtworkBytes
        ?QUrl::fromLocalFile(artworkPath).toString():QString();
    return {{"id",query.value(0)}, {"title",query.value(1)}, {"platform",query.value(2)},
        {"runtimeId",query.value(3)}, {"favorite",query.value(4).toBool()},
        {"mediaType","game"}, {"mediaId",query.value(0)},
        {"franchise",query.value(5).toString()},
        {"tags",tags},{"aliases",aliases},{"summary",query.value(8).toString()},{"artwork",artwork}};
}
}

GameLibrary::GameLibrary(const QString &path, AppRegistry *apps, QObject *parent)
    : QObject(parent), m_connection("games-" + QUuid::createUuid().toString(QUuid::WithoutBraces)), m_apps(apps) {
    connect(apps,&AppRegistry::appsChanged,this,&GameLibrary::changed);
    m_emulatorProfile=qApp->property("isoldeProfile").toString();if(m_emulatorProfile.isEmpty())m_emulatorProfile="default";
    m_steamScanDeadline.setSingleShot(true);m_steamScanDeadline.setInterval(30000);
    connect(&m_steamScanDeadline,&QTimer::timeout,this,[this]{fail(tr("Steam discovery timed out. Nothing was imported; retry when storage is available."));cancelImport();});
    connect(&m_steamScan,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart && !m_steamScanCanceled){fail(tr("Steam discovery could not start. Repair the Isolde Steam adapter and retry."));cancelImport();}
    });
    connect(&m_steamScan,&QProcess::readyReadStandardOutput,this,[this]{
        m_steamScanOutput+=m_steamScan.readAllStandardOutput();
        if(m_steamScanOutput.size()>2*1024*1024){fail(tr("Steam preview exceeds its size limit. Nothing was imported."));cancelImport();}
    });
    connect(&m_steamScan,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        m_steamScanDeadline.stop();
        if(m_steamScanCanceled || !m_ready)return;
        if(code!=0 || status!=QProcess::NormalExit){
            const auto error=QString::fromUtf8(m_steamScan.readAllStandardError()).right(1000);
            fail(tr("Steam discovery failed. Nothing was imported. %1").arg(error));cancelImport();return;
        }
        m_steamScanOutput+=m_steamScan.readAllStandardOutput();
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(m_steamScanOutput,&parse).toVariant().toMap();
        const auto games=document.value("games").toList();
        if(parse.error!=QJsonParseError::NoError || document.value("games").metaType()!=QMetaType::fromType<QVariantList>() || games.size()>5000 || m_steamScanOutput.size()>2*1024*1024){fail(tr("Invalid Steam discovery response."));cancelImport();return;}
        QSet<QString> seen;
        for(const auto &entry:games){
            const auto game=entry.toMap();
            if(ComponentSchema::gameRegistration(game).isEmpty() || game.value("runtimeId")!="steam" || !game.value("root").toString().isEmpty() || seen.contains(game.value("target").toString())){
                fail(tr("Invalid or duplicate Steam title. Nothing was imported."));cancelImport();return;
            }
            seen.insert(game.value("target").toString());
        }
        m_preview=games;m_importReady=true;
        m_importStatus=games.isEmpty()?tr("No fully installed games were found in this profile. Open Steam once to sign in and install games, then retry."):
            tr("%1 installed Steam games. Review before adding. Steam owns licensing, installation and saves.").arg(games.size());
        const auto warnings=document.value("warnings").toStringList();if(!warnings.isEmpty())m_importStatus+=' '+warnings.join(' ');
        emit importChanged();
    });
    m_setupDeadline.setSingleShot(true);m_setupDeadline.setInterval(2400000);
    connect(&m_setupDeadline,&QTimer::timeout,this,&GameLibrary::cancelEmulatorSetup);
    connect(&m_setup,&QProcess::readyReadStandardOutput,this,[this]{m_setupOutput=(m_setupOutput+m_setup.readAllStandardOutput()).right(8192);});
    connect(&m_setup,&QProcess::readyReadStandardError,this,[this]{m_setupOutput=(m_setupOutput+m_setup.readAllStandardError()).right(8192);});
    connect(&m_setup,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){m_setupStatus="Emulator setup could not start. "+m_setup.errorString();emit setupChanged();});
    connect(&m_setup,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        m_setupDeadline.stop();
        if(!m_ready || m_setupCanceled){m_setupStatus="Setup stopped. Existing games and saves were preserved. Review runtime installation before retrying.";emit setupChanged();return;}
        QFile receipt(m_setupReceipt);QVariantMap result;
        if(receipt.open(QIODevice::ReadOnly) && receipt.size()<65536)result=QJsonDocument::fromJson(receipt.readAll()).toVariant().toMap();
        if(code!=0 || status!=QProcess::NormalExit || !result.value("ready").toBool()){
            m_setupStatus="Emulator installation failed. Existing games and saves were preserved. "+QString::fromUtf8(m_setupOutput).right(1200);emit setupChanged();return;}
        bool activated=true;
        for(const auto &item:emulators()){
            const auto runtime=item.toMap();
            if(!runtime.value("installed").toBool() || !m_catalog || !m_catalog->install(runtime.value("id").toString())){activated=false;break;}
        }
        m_setupStatus=activated?"Emulators installed and available in Add game. Your games and firmware were not downloaded.":"Runtime files installed, but activation needs repair. "+(m_catalog?m_catalog->errorString():QString("Store unavailable."));
        emit changed();emit setupChanged();
    });
    m_portableInstallDeadline.setSingleShot(true);m_portableInstallDeadline.setInterval(900000);
    connect(&m_portableInstallDeadline,&QTimer::timeout,this,&GameLibrary::cancelPortableInstall);
    connect(&m_portableInstall,&QProcess::readyReadStandardOutput,this,[this]{
        m_portableInstallOutput+=m_portableInstall.readAllStandardOutput();
        if(m_portableInstallOutput.size()>65536)cancelPortableInstall();
    });
    connect(&m_portableInstall,&QProcess::readyReadStandardError,this,[this]{
        m_portableInstallError=(m_portableInstallError+m_portableInstall.readAllStandardError()).right(8192);
    });
    connect(&m_portableInstall,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){m_portableInstallStatus="Portable game helper could not start.";m_portablePhase.clear();emit portableInstallChanged();}
    });
    connect(&m_portableInstall,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        m_portableInstallDeadline.stop();
        if(!m_ready || m_portablePhase.isEmpty())return;
        if(code!=0 || status!=QProcess::NormalExit){
            const auto reason=QString::fromUtf8(m_portableInstallError).right(1000);
            const auto stage=m_portableDestination+".staging";
            if(m_portablePhase=="install" && QFileInfo(stage).isDir() && QFileInfo(stage).canonicalFilePath()==stage)QDir(stage).removeRecursively();
            const auto snapshot=m_portableDestination+".staging-source";
            if(m_portablePhase=="install" && QFileInfo(snapshot).isFile() && !QFileInfo(snapshot).isSymLink())QFile::remove(snapshot);
            m_portableInstallStatus=m_portablePhase=="uninstall"
                ?"Safe uninstall stopped or failed. The catalog entry was preserved; inspect the managed folder before retrying. "+reason
                :m_portableCanceled?"Install canceled. Existing files and saves were preserved.":
                    "Portable game preflight/install failed. Nothing was added to the catalog. "+reason;
            m_portablePhase.clear();emit portableInstallChanged();return;
        }
        m_portableInstallOutput+=m_portableInstall.readAllStandardOutput();
        QJsonParseError parse;const auto metadata=QJsonDocument::fromJson(m_portableInstallOutput,&parse).toVariant().toMap();
        if(m_portablePhase=="uninstall"){
            if(parse.error!=QJsonParseError::NoError || metadata.value("schemaVersion").toInt()!=1
                || !metadata.value("removed").toBool()){
                m_portableInstallStatus="The ownership verification response was invalid. The catalog entry was preserved.";
            }else if(removeGame(m_portableGameId)){
                m_portableInstallStatus="Verified Isolde-owned files removed. Runtime profile and saves were preserved.";
            }else{
                m_portableInstallStatus="Owned game files were removed, but the catalog could not be updated. Restore the profile catalog backup or retry.";
            }
            m_portableGameId.clear();m_portablePhase.clear();emit portableInstallChanged();return;
        }
        if(parse.error!=QJsonParseError::NoError || metadata.value("schemaVersion").toInt()!=1
            || metadata.value("title").toString().isEmpty() || metadata.value("platform").toString().isEmpty()){
            m_portableInstallStatus="Invalid portable game adapter response. Nothing was added to Games.";
            m_portablePhase.clear();emit portableInstallChanged();return;
        }
        if(m_portablePhase=="inspect"){
            const auto *runtime=m_apps->find(m_portableRuntime);
            if(!runtime || !compatibleRuntime(m_portableRuntime,true)){
                m_portableInstallStatus="The selected runtime is no longer available. Choose a runtime and inspect the bundle again.";
                m_portablePhase.clear();emit portableInstallChanged();return;
            }
            m_portableInstallPreview=metadata;
            m_portableInstallPreview["destination"]=m_portableDestination;
            m_portableInstallPreview["runtimeName"]=runtime->name;
            m_portablePhase="review";
            m_portableInstallStatus="Preflight complete. Review runtime, destination and side effects before Install.";
            emit portableInstallChanged();return;
        }
        if(m_portablePhase=="install"){
            if(metadata.value("archiveSha256")!=m_portableInstallPreview.value("archiveSha256")
                || metadata.value("title")!=m_portableInstallPreview.value("title")
                || metadata.value("platform")!=m_portableInstallPreview.value("platform")
                || metadata.value("entrypoint")!=m_portableInstallPreview.value("entrypoint")){
                m_portableInstallStatus="The selected package changed after preflight. Nothing was added to Games.";
                m_portablePhase.clear();emit portableInstallChanged();return;
            }
            const auto root=m_portableDestination;
            if(!ManagedGamePaths::isInstallRoot(m_emulatorProfile,root)){
                m_portableInstallStatus="Installed package failed private destination verification; it was not added to Games.";
                m_portablePhase.clear();emit portableInstallChanged();return;
            }
            const auto id=addGame({{"schemaVersion",1},{"title",metadata.value("title")},
                {"platform",metadata.value("platform")},{"runtimeId",m_portableRuntime},
                {"root",root},{"target",metadata.value("entrypoint")}});
            m_portableInstallStatus=id.isEmpty()?"Package installed but catalog registration failed. Preserve the private recovery folder and retry from Games.":"Installed, verified and added to Games. Launch uses an isolated session; setup scripts are never executed.";
            m_portablePhase.clear();m_portableInstallPreview.clear();emit portableInstallChanged();return;
        }
    });
    m_opticalRefreshTimer.setInterval(2500);
    connect(&m_opticalRefreshTimer,&QTimer::timeout,this,[this]{
        if(!m_ready)return;
        const auto discs=OpticalDiscAdapter::mountedDiscs();
        QStringList ids;
        for(const auto &item:discs)ids.append(item.toMap().value("observationId").toString());
        ids.sort();
        if(ids==m_lastOpticalObservationIds)return;
        bool inserted=false,removed=false;
        for(const auto &id:ids)if(!m_lastOpticalObservationIds.contains(id))inserted=true;
        for(const auto &id:m_lastOpticalObservationIds)if(!ids.contains(id))removed=true;
        m_lastOpticalObservationIds=ids;
        const auto change=inserted&&removed?QStringLiteral("changed"):
            (inserted?QStringLiteral("inserted"):QStringLiteral("removed"));
        emit opticalDiscsChanged(discs,change);
    });
    m_scanTimer.setInterval(0);
    connect(&m_scanTimer,&QTimer::timeout,this,&GameLibrary::scanStep);
    if (!QDir::isAbsolutePath(path) || QFileInfo(path).isSymLink()) { fail(tr("Choose a private games catalog path.")); return; }
    const bool exists = QFileInfo::exists(path);
    if (!exists && !QDir().mkpath(QFileInfo(path).absolutePath())) { fail(tr("Cannot create the games catalog folder.")); return; }
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(path);
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
    if (!m_db.open()) { fail(tr("Cannot open the games catalog: %1").arg(m_db.lastError().text())); return; }
    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA user_version") || !query.next()) { fail(tr("Cannot read the games catalog version.")); return; }
    const int version = query.value(0).toInt(); query.finish();
    // Never migrate, replace or repair an unknown existing catalog on read.
    if (exists && version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != 6) { fail(tr("Unsupported games catalog version. Original data was preserved.")); return; }
    if (!query.exec("PRAGMA foreign_keys=ON") || !query.exec("PRAGMA synchronous=FULL")) { fail(query.lastError().text()); return; }
    if (!exists) {
        if (!m_db.transaction()) { fail(m_db.lastError().text()); return; }
        const bool created = query.exec("CREATE TABLE games (id TEXT PRIMARY KEY, title TEXT NOT NULL, platform TEXT NOT NULL, runtime TEXT NOT NULL, root TEXT NOT NULL, target TEXT NOT NULL, favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)), added INTEGER NOT NULL, UNIQUE(runtime,root,target))")
            && query.exec("CREATE INDEX games_title ON games(title COLLATE NOCASE)")
            && query.exec("PRAGMA user_version=1");
        if (!created || !m_db.commit()) { const auto error = query.lastError().text(); m_db.rollback(); fail(tr("Cannot initialize the games catalog: %1").arg(error)); return; }
    }
    if (!query.exec("SELECT id,title,platform,runtime,root,target,favorite,added FROM games LIMIT 0")) { fail(tr("Invalid games catalog. Original data was preserved.")); return; }
    if(version==2){
        if(!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions LIMIT 0")){fail(tr("Invalid edition catalog. Original data was preserved."));return;}
        m_hasEditions=true;
    }
    if(version==3 || version==4 || version==5 || version==6){
        if(!query.exec("SELECT game_id,last_played FROM game_activity LIMIT 0")){fail(tr("Invalid game activity catalog. Original data was preserved."));return;}
        m_hasActivity=true;
        if(!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='game_editions'")){fail(tr("Cannot inspect the editions catalog."));return;}
        if(query.next()){
            if(!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions LIMIT 0")){fail(tr("Invalid edition catalog. Original data was preserved."));return;}
            m_hasEditions=true;
        }
    }
    if(version==4 || version==5 || version==6){
        if(!query.exec("SELECT game_id,franchise,tags FROM game_metadata LIMIT 0")
            || !query.exec("SELECT id,name,kind,filter_type,filter_value FROM game_collections LIMIT 0")
            || !query.exec("SELECT collection_id,game_id,position FROM game_collection_members LIMIT 0")){
            fail(tr("Invalid Games organization catalog. Original data was preserved."));return;
        }
        m_hasMetadata=true;m_hasCollections=true;
    }
    if(version==5 || version==6){
        if(!query.exec("SELECT aliases,summary FROM game_metadata LIMIT 0")){
            fail(tr("Invalid rich game metadata catalog. Original data was preserved."));return;
        }
        m_hasRichMetadata=true;
    }
    if(version==6){
        if(!query.exec("SELECT artwork_hash FROM game_metadata LIMIT 0")){
            fail(tr("Invalid managed artwork catalog. Original data was preserved."));return;
        }
        m_hasArtwork=true;
    }
    m_ready = true;
    for(const auto &disc:OpticalDiscAdapter::mountedDiscs())
        m_lastOpticalObservationIds.append(disc.toMap().value("observationId").toString());
    m_lastOpticalObservationIds.sort();
    m_opticalRefreshTimer.start();
}
GameLibrary::~GameLibrary() {
    cancelImport();if(m_steamScan.state()!=QProcess::NotRunning)m_steamScan.waitForFinished(1000);
    cancelPortableInstall();
    cancelEmulatorSetup();if(setupBusy())m_setup.waitForFinished(5000);
    m_db.close(); m_db = QSqlDatabase(); QSqlDatabase::removeDatabase(m_connection);
}
bool GameLibrary::fail(const QString &error) { m_error = error; emit errorChanged(); return false; }
void GameLibrary::invalidate() { cancelImport();cancelEmulatorSetup();cancelPortableInstall(); m_ready = false; m_db.close(); fail(tr("This games profile is no longer active.")); emit changed(); }
bool GameLibrary::compatibleRuntime(const QString &id, bool file) const {
    const auto *app = m_apps->find(id);
    if (!app || app->type != "desktop") return false;
    if(id=="steam" && !file)return app->presentationMode=="game-hub" && app->launchConfiguration.value("desktopId")=="isolde-steam";
    if (file) return QStringList{"game","game-hub"}.contains(app->presentationMode)
        && app->handledActions.contains("open-file") && app->launchConfiguration.value("actionHandlers").toMap().contains("open-file");
    return app->presentationMode == "game";
}
QVariantList GameLibrary::runtimes() const {
    QVariantList result;
    for (const auto &entry : m_apps->apps()) {
        const auto app = entry.toMap(); const auto id = app.value("id").toString();
        if (compatibleRuntime(id,true) || compatibleRuntime(id,false))
            result.append(QVariantMap{{"id",id},{"name",app.value("name")},{"file",compatibleRuntime(id,true)},{"provider",id=="steam"?"steam":"local"}});
    }
    return result;
}
QVariantList GameLibrary::emulators() const {
    QVariantList result;
    if(!m_ready)return result;
    for(const auto &item:GameEmulators::definitions()){
        auto value=item.toMap();const auto binary=value.value("binary").toString();
        const auto core=value.value("core").toString();
        value["installed"]=QFileInfo(binary).isExecutable() && (core.isEmpty() || QFileInfo(core).isFile());
        value["registered"]=compatibleRuntime(value.value("id").toString(),true);result.append(value);
    }return result;
}
QVariantMap GameLibrary::emulatorSettings(const QString &id){
    if(!m_ready){fail(tr("This games profile is no longer active."));return {};}
    QString error;const auto value=GameEmulators::settings(m_emulatorProfile,id,&error);if(value.isEmpty())fail(error);return value;
}
bool GameLibrary::configureEmulator(const QString &id,const QVariantMap &options,const QString &firmware){
    if(!m_ready)return fail(tr("This games profile is no longer active."));
    QString error;if(!GameEmulators::configure(m_emulatorProfile,id,options,firmware,&error))return fail(error);
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::installEmulators(bool consent){
    if(!m_ready || !consent || setupBusy() || !m_catalog)return fail(tr("Review and confirm emulator installation before continuing."));
#ifndef Q_OS_LINUX
    return fail(tr("Managed emulator installation currently requires the Linux appliance."));
#else
    auto root=QCoreApplication::applicationDirPath()+"/share/isolde";
    if(!QFileInfo(root+"/provision-games.py").isReadable())root=QCoreApplication::applicationDirPath()+"/../share/isolde";
    const auto program=QStandardPaths::findExecutable("python3");
    if(program.isEmpty() || !QFileInfo(root+"/provision-games.py").isReadable())return fail(tr("Repair the Isolde emulator installer."));
    const auto state=GameEmulators::home(m_emulatorProfile,"ppsspp")+"/.local/state";
    for(auto ancestor=QFileInfo(state);;ancestor=QFileInfo(ancestor.absolutePath())){
        if(ancestor.isSymLink())return fail(tr("Private emulator storage is redirected."));
        if(ancestor.absolutePath()==ancestor.absoluteFilePath())break;
    }
    if(!QDir().mkpath(state) || QFileInfo(state).canonicalFilePath()!=state)return fail(tr("Cannot prepare private installation evidence."));
    m_setupReceipt=state+"/runtime-install-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".json";
    m_setupCanceled=false;m_setupOutput.clear();m_setupStatus="Installing the verified runtimes and console cores. First-time PSP preparation may take several minutes. No game or firmware downloads.";
    m_setup.setChildProcessModifier([]{::setsid();});
    m_setup.start(program,{root+"/provision-games.py","--install","--receipt",m_setupReceipt});
    m_setupDeadline.start();emit setupChanged();return true;
#endif
}
void GameLibrary::cancelEmulatorSetup(){
    if(!setupBusy())return;m_setupCanceled=true;
#ifdef Q_OS_UNIX
    if(m_setup.processId()>0)::kill(-pid_t(m_setup.processId()),SIGINT);
#else
    m_setup.terminate();
#endif
    m_setupStatus="Stopping emulator setup safely. Wait before retrying.";emit setupChanged();
}
QString GameLibrary::checkedTarget(const QString &root, const QString &relative) {
    QString validatedRoot=root;
    const bool optical=OpticalDiscAdapter::isRootReference(root);
    if(optical){
        const auto mediaId=OpticalDiscAdapter::mediaIdFromRoot(root);
        if(!OpticalDiscGrants::approved(m_emulatorProfile,mediaId)){
            fail(tr("This profile has not approved that optical identity. Insert the disc and explicitly approve it in Games."));
            return {};
        }
        const auto media=OpticalDiscAdapter::resolveRootReference(root);
        const auto disc=media.value("disc").toMap();
        if(media.value("state")!="ready"){
            const auto number=media.value("expectedDisc").toInt();
            fail(media.value("state")=="wrong-disc"
                ?tr("Wrong optical disc. Insert disc %1 of %2, then retry.").arg(number).arg(media.value("discCount").toInt())
                :tr("Insert optical disc %1 of %2, then retry.").arg(number).arg(media.value("discCount").toInt()));
            return {};
        }
        validatedRoot=disc.value("mountRoot").toString();
    }
    const auto grantError = MediaGrants::validate(root);
    const bool managed=ManagedGamePaths::isInstallRoot(m_emulatorProfile,root);
    if (!optical && !grantError.isEmpty() && !managed) { fail(grantError); return {}; }
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.contains('\\') || relative.contains(QChar::Null)
        || relative.size()>4096 || QDir::cleanPath(relative)!=relative || relative.split('/').contains("..")) {
        fail(tr("Choose a file inside the approved game folder.")); return {};
    }
    const auto path = QDir(validatedRoot).filePath(relative); const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable() || info.canonicalFilePath()!=path || !path.startsWith(validatedRoot+'/')) {
        fail(tr("The game file is missing, unreadable or redirected. Reconnect its source and retry.")); return {};
    }
    return path;
}
QString GameLibrary::addGame(const QVariantMap &input) {
    if (!m_ready) { fail(tr("The games catalog is unavailable.")); return {}; }
    if(m_portablePhase=="uninstall"){fail(tr("Wait for the managed uninstall to finish before changing game sources."));return {};}
    QString error; const auto registration = ComponentSchema::gameRegistration(input,&error);
    if (registration.isEmpty()) { fail(error); return {}; }
    const auto runtime = registration.value("runtimeId").toString();
    const auto root = registration.value("root").toString(), target = registration.value("target").toString();
    if (!platformAvailable(registration.value("platform").toString())) {
        fail(tr("The Games catalog supports up to 128 distinct platforms. Existing entries were preserved."));
        return {};
    }
    if (!compatibleRuntime(runtime,!root.isEmpty())) { fail(tr("Choose an installed compatible game runtime.")); return {}; }
    if (!root.isEmpty() && checkedTarget(root,target).isEmpty()) return {};
    if(m_hasEditions && installationExists(runtime,root,target)){fail(tr("This installation already belongs to a game or edition."));return {};}
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?");
    query.addBindValue(runtime); query.addBindValue(root); query.addBindValue(target);
    if (!query.exec()) { fail(query.lastError().text()); return {}; }
    if (query.next()) { fail(tr("This installation is already in Games. Its metadata was preserved.")); return {}; }
    query.finish();
    if (!m_db.transaction()) { fail(m_db.lastError().text()); return {}; }
    if (!query.exec("SELECT count(*) FROM games") || !query.next() || query.value(0).toInt()>=MaxGames) {
        m_db.rollback(); fail(tr("The games catalog is full or unavailable.")); return {};
    }
    query.finish();
    const auto id = "game-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,added) VALUES(?,?,?,?,?,?,unixepoch())");
    for (const auto &value : QStringList{id,registration.value("title").toString(),registration.value("platform").toString(),runtime,root,target}) query.addBindValue(value);
    if (!query.exec() || !m_db.commit()) { const auto error = query.lastError().text(); m_db.rollback(); fail(tr("Could not save the game: %1").arg(error)); return {}; }
    m_error.clear(); emit changed(); return id;
}
QVariantList GameLibrary::page(const QString &view, const QString &search, int offset, const QString &platform) {
    if (!m_ready) return {};
    if (!QStringList{"all","recent","favorites","search","platform","continue"}.contains(view)
        || search.size()>160 || offset<0 || offset>=MaxGames || offset%PageSize
        || platform.size()>80 || platform.contains(QChar::Null)
        || (view=="platform" && (platform.trimmed().isEmpty() || platform!=platform.trimmed()))) {
        fail(tr("Invalid games query.")); return {};
    }
    if(view=="continue" && !m_hasActivity)return {};
    QSqlQuery query(m_db);
    // instr uses literal text, not LIKE wildcards or executable SQL predicates.
    QString sql="SELECT g.id,g.title,g.platform,g.runtime,g.favorite,"
        +(m_hasMetadata?QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')"):QString("'', '[]'"))
        +(m_hasRichMetadata?QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')"):QString(",'[]',''"))
        +(m_hasArtwork?QString(",COALESCE(m.artwork_hash,'')"):QString(",'')"))
        +" FROM games g";
    if(m_hasActivity)sql+=" LEFT JOIN game_activity a ON a.game_id=g.id";
    if(m_hasMetadata)sql+=" LEFT JOIN game_metadata m ON m.game_id=g.id";
    sql+=" WHERE (?=0 OR g.favorite=1) AND (?='' OR lower(g.platform)=lower(?)";
    if(m_hasEditions)sql+=" OR EXISTS(SELECT 1 FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(?))";
    sql+=") AND (instr(lower(g.title),lower(?))>0";
    if(m_hasMetadata)sql+=" OR instr(lower(m.franchise),lower(?))>0 OR instr(lower(m.tags),lower(?))>0";
    if(m_hasRichMetadata)sql+=" OR instr(lower(m.aliases),lower(?))>0 OR instr(lower(m.summary),lower(?))>0";
    sql+=")";
    if(view=="continue")sql+=" AND a.game_id IS NOT NULL";
    sql+=" ORDER BY "+QString(view=="recent"?"g.added DESC,g.id":view=="continue"?"a.last_played DESC,g.id":"g.title COLLATE NOCASE,g.id")+" LIMIT 50 OFFSET ?";
    query.prepare(sql);
    query.addBindValue(view=="favorites"?1:0);
    query.addBindValue(view=="platform"?platform:QStringLiteral(""));
    query.addBindValue(view=="platform"?platform:QStringLiteral(""));
    if(m_hasEditions)query.addBindValue(view=="platform"?platform:QStringLiteral(""));
    query.addBindValue(search.isNull()?QStringLiteral(""):search);
    if(m_hasMetadata){query.addBindValue(search.isNull()?QStringLiteral(""):search);query.addBindValue(search.isNull()?QStringLiteral(""):search);}
    if(m_hasRichMetadata){query.addBindValue(search.isNull()?QStringLiteral(""):search);query.addBindValue(search.isNull()?QStringLiteral(""):search);}
    query.addBindValue(offset);
    if (!query.exec()) { fail(query.lastError().text()); return {}; }
    QVariantList result; while (query.next()) result.append(row(query,artworkDirectory())); return result;
}
bool GameLibrary::ensureOrganizationSchema() {
    if(m_hasMetadata && m_hasCollections)return true;
    if(!m_db.transaction())return fail(tr("Could not begin the Games organization update."));
    QSqlQuery query(m_db);
    const bool created=(!m_hasActivity
            ? query.exec("CREATE TABLE game_activity(game_id TEXT PRIMARY KEY REFERENCES games(id) ON DELETE CASCADE,last_played INTEGER NOT NULL)")
                && query.exec("CREATE INDEX game_activity_recent ON game_activity(last_played DESC)")
            : true)
        && query.exec("CREATE TABLE game_metadata(game_id TEXT PRIMARY KEY REFERENCES games(id) ON DELETE CASCADE,franchise TEXT NOT NULL DEFAULT '',tags TEXT NOT NULL DEFAULT '[]')")
        && query.exec("CREATE TABLE game_collections(id TEXT PRIMARY KEY,name TEXT NOT NULL COLLATE NOCASE UNIQUE,kind TEXT NOT NULL CHECK(kind IN ('manual','smart')),filter_type TEXT NOT NULL DEFAULT '',filter_value TEXT NOT NULL DEFAULT '')")
        && query.exec("CREATE TABLE game_collection_members(collection_id TEXT NOT NULL REFERENCES game_collections(id) ON DELETE CASCADE,game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,position INTEGER NOT NULL,PRIMARY KEY(collection_id,game_id))")
        && query.exec("CREATE INDEX game_collection_members_order ON game_collection_members(collection_id,position,game_id)")
        && query.exec("PRAGMA user_version=4");
    if(!created || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();
        return fail(tr("Games organization could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasActivity=true;m_hasMetadata=true;m_hasCollections=true;
    return true;
}
bool GameLibrary::ensureRichMetadataSchema() {
    if(m_hasRichMetadata)return true;
    if(!ensureOrganizationSchema())return false;
    if(!m_db.transaction())return fail(tr("Could not begin the rich game metadata update."));
    QSqlQuery query(m_db);
    const bool migrated=query.exec("ALTER TABLE game_metadata ADD COLUMN aliases TEXT NOT NULL DEFAULT '[]'")
        && query.exec("ALTER TABLE game_metadata ADD COLUMN summary TEXT NOT NULL DEFAULT ''")
        && query.exec("PRAGMA user_version=5");
    if(!migrated || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();
        return fail(tr("Rich game metadata could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasRichMetadata=true;return true;
}
QString GameLibrary::artworkDirectory() const { return m_db.databaseName()+".artwork"; }
bool GameLibrary::ensureArtworkSchema() {
    if(m_hasArtwork)return true;
    if(!ensureRichMetadataSchema())return false;
    if(!m_db.transaction())return fail(tr("Could not begin the managed artwork update."));
    QSqlQuery query(m_db);
    const bool migrated=query.exec("ALTER TABLE game_metadata ADD COLUMN artwork_hash TEXT NOT NULL DEFAULT ''")
        && query.exec("PRAGMA user_version=6");
    if(!migrated || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();
        return fail(tr("Managed artwork could not be initialized. Existing catalog data was preserved. %1").arg(error));
    }
    m_hasArtwork=true;return true;
}
bool GameLibrary::setArtwork(const QString &id,const QString &source) {
    if(!m_ready || details(id).isEmpty())return fail(tr("Game not found in this profile."));
    if(!QDir::isAbsolutePath(source) || QDir::cleanPath(source)!=source
        || QFileInfo(source).isSymLink() || !QFileInfo(source).isFile()
        || QFileInfo(source).canonicalFilePath()!=source
        || QFileInfo(source).size()<1 || QFileInfo(source).size()>MaxArtworkSourceBytes)
        return fail(tr("Choose a readable PNG or JPEG artwork file in an approved media folder."));
    const auto parent=QFileInfo(source).absolutePath();
    const auto grantError=MediaGrants::validate(parent);
    if(!grantError.isEmpty())return fail(grantError);
    if(!approvedMediaDirectory(m_emulatorProfile,parent))
        return fail(tr("Approve this media folder for the active profile before importing artwork."));
    QFile input(source);if(!input.open(QIODevice::ReadOnly))return fail(tr("Cannot read the selected artwork."));
    const auto sourceBytes=input.read(MaxArtworkSourceBytes+1);
    if(sourceBytes.size()>MaxArtworkSourceBytes)return fail(tr("Artwork exceeds the 16 MiB source limit."));
    const auto bytes=normalizeArtwork(sourceBytes);
    if(bytes.isEmpty())return fail(tr("Artwork must be a readable PNG or JPEG no larger than 4096 by 4096 pixels and 12 MiB when managed."));
    if(!ensureArtworkSchema())return false;
    QString digest,error;
    if(!persistArtwork(m_db.databaseName(),bytes,&digest,&error))return fail(tr("%1 Nothing was changed in the game catalog.").arg(error));
    QSqlQuery oldQuery(m_db);oldQuery.prepare("SELECT artwork_hash FROM game_metadata WHERE game_id=?");oldQuery.addBindValue(id);
    if(!oldQuery.exec())return fail(oldQuery.lastError().text());
    const QString oldHash=oldQuery.next()?oldQuery.value(0).toString():QString();
    oldQuery.finish();
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);query.prepare("INSERT INTO game_metadata(game_id,artwork_hash) VALUES(?,?) ON CONFLICT(game_id) DO UPDATE SET artwork_hash=excluded.artwork_hash");
    query.addBindValue(id);query.addBindValue(digest);
    if(!query.exec() || !m_db.commit()){
        const auto reason=query.lastError().text();m_db.rollback();
        return fail(tr("Artwork could not be assigned. Previous artwork was preserved. %1").arg(reason));
    }
    if(!oldHash.isEmpty() && oldHash!=digest){
        QSqlQuery references(m_db);references.prepare("SELECT count(*) FROM game_metadata WHERE artwork_hash=?");references.addBindValue(oldHash);
        const auto oldPath=artworkDirectory()+"/"+oldHash+".png";
        if(references.exec() && references.next() && references.value(0).toInt()==0
            && QRegularExpression("^[0-9a-f]{64}$").match(oldHash).hasMatch())QFile::remove(oldPath);
    }
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::clearArtwork(const QString &id) {
    if(!m_ready || details(id).isEmpty())return fail(tr("Game not found in this profile."));
    if(!m_hasArtwork){m_error.clear();emit errorChanged();return true;}
    QSqlQuery oldQuery(m_db);oldQuery.prepare("SELECT artwork_hash FROM game_metadata WHERE game_id=?");oldQuery.addBindValue(id);
    if(!oldQuery.exec())return fail(oldQuery.lastError().text());
    const QString oldHash=oldQuery.next()?oldQuery.value(0).toString():QString();oldQuery.finish();
    if(oldHash.isEmpty()){m_error.clear();emit errorChanged();return true;}
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);query.prepare("UPDATE game_metadata SET artwork_hash='' WHERE game_id=?");query.addBindValue(id);
    if(!query.exec() || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();return fail(tr("Artwork reference was not removed. %1").arg(error));
    }
    QSqlQuery references(m_db);references.prepare("SELECT count(*) FROM game_metadata WHERE artwork_hash=?");references.addBindValue(oldHash);
    const auto oldPath=artworkDirectory()+"/"+oldHash+".png";
    if(references.exec() && references.next() && references.value(0).toInt()==0
        && QRegularExpression("^[0-9a-f]{64}$").match(oldHash).hasMatch())QFile::remove(oldPath);
    m_error.clear();emit errorChanged();emit changed();return true;
}
QVariantList GameLibrary::platforms() {
    QVariantList result;
    if (!m_ready) return result;
    QSqlQuery query(m_db);
    const auto sql="SELECT platform FROM (SELECT platform FROM games"
        +QString(m_hasEditions?" UNION ALL SELECT platform FROM game_editions":"")
        +") GROUP BY platform COLLATE NOCASE ORDER BY platform COLLATE NOCASE LIMIT 129";
    if (!query.exec(sql)) {
        fail(query.lastError().text());
        return {};
    }
    while (query.next()) result.append(query.value(0).toString());
    if (result.size()>MaxPlatforms) {
        fail(tr("The Games catalog has more platform categories than supported."));
        return {};
    }
    return result;
}
bool GameLibrary::recordCompletedSession(const QString &id) {
    if(!m_ready || !ComponentSchema::safeId(id) || details(id).isEmpty())return fail(tr("Game not found in this profile."));
    if(m_hasMetadata && m_hasCollections) {
        QSqlQuery query(m_db);query.prepare("INSERT INTO game_activity(game_id,last_played) VALUES(?,unixepoch()) ON CONFLICT(game_id) DO UPDATE SET last_played=excluded.last_played");query.addBindValue(id);
        if(!query.exec())return fail(tr("Could not record the completed game session. %1").arg(query.lastError().text()));
        m_error.clear();emit errorChanged();emit changed();return true;
    }
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    if(!m_hasActivity && (!query.exec("CREATE TABLE game_activity(game_id TEXT PRIMARY KEY REFERENCES games(id) ON DELETE CASCADE,last_played INTEGER NOT NULL)")
        || !query.exec("CREATE INDEX game_activity_recent ON game_activity(last_played DESC)"))){
        const auto error=query.lastError().text();m_db.rollback();return fail(tr("Could not initialize game activity. Previous catalog data was preserved. %1").arg(error));
    }
    query.prepare("INSERT INTO game_activity(game_id,last_played) VALUES(?,unixepoch()) ON CONFLICT(game_id) DO UPDATE SET last_played=excluded.last_played");
    query.addBindValue(id);
    if(!query.exec() || (!m_hasActivity && !query.exec("PRAGMA user_version=3")) || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();return fail(tr("Could not record the completed game session. Previous activity was preserved. %1").arg(error));
    }
    m_hasActivity=true;m_error.clear();emit errorChanged();emit changed();return true;
}
QVariantMap GameLibrary::registration(const QString &id) {
    if(!m_ready || !ComponentSchema::safeId(id))return {};
    QSqlQuery query(m_db);
    query.prepare("SELECT title,platform,runtime,root,target FROM games WHERE id=?");query.addBindValue(id);
    if(!query.exec() || !query.next())return {};
    return {{"schemaVersion",1},{"title",query.value(0)},{"platform",query.value(1)},
        {"runtimeId",query.value(2)},{"root",query.value(3)},{"target",query.value(4)}};
}
bool GameLibrary::updateGame(const QString &id,const QVariantMap &input) {
    if(m_portablePhase=="uninstall")return fail(tr("Wait for the managed uninstall to finish before changing game sources."));
    if(!m_ready || registration(id).isEmpty())return fail(tr("Game not found in this profile."));
    QString error;const auto value=ComponentSchema::gameRegistration(input,&error);
    if(value.isEmpty())return fail(error);
    const auto runtime=value.value("runtimeId").toString(),root=value.value("root").toString(),target=value.value("target").toString();
    if(!platformAvailable(value.value("platform").toString()))return fail(tr("The Games catalog supports up to 128 distinct platforms. Previous metadata was preserved."));
    if(!compatibleRuntime(runtime,!root.isEmpty()))return fail(tr("Choose an installed compatible game runtime."));
    if(!root.isEmpty() && checkedTarget(root,target).isEmpty())return false;
    if(m_hasEditions){
        QSqlQuery duplicate(m_db);duplicate.prepare("SELECT id FROM game_editions WHERE runtime=? AND root=? AND target=?");
        duplicate.addBindValue(runtime);duplicate.addBindValue(root);duplicate.addBindValue(target);
        if(!duplicate.exec() || duplicate.next())return fail(tr("This installation already belongs to an edition. Previous metadata was preserved."));
    }
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    query.prepare("UPDATE games SET title=?,platform=?,runtime=?,root=?,target=? WHERE id=?");
    for(const auto &key:QStringList{"title","platform","runtimeId","root","target"})query.addBindValue(value.value(key));
    query.addBindValue(id);
    if(!query.exec() || query.numRowsAffected()!=1 || !m_db.commit()){
        const auto reason=query.lastError().text();m_db.rollback();return fail(tr("Could not update this game. Previous metadata and source were preserved. %1").arg(reason));
    }
    m_error.clear();emit errorChanged();emit changed();return true;
}
QVariantMap GameLibrary::details(const QString &id) {
    if (!m_ready || !ComponentSchema::safeId(id)) return {};
    QSqlQuery query(m_db);
    query.prepare("SELECT g.id,g.title,g.platform,g.runtime,g.favorite,"
        +(m_hasMetadata?QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')"):QString("'', '[]'"))
        +(m_hasRichMetadata?QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')"):QString(",'[]',''"))
        +(m_hasArtwork?QString(",COALESCE(m.artwork_hash,'')"):QString(",'')"))
        +" FROM games g"+QString(m_hasMetadata?" LEFT JOIN game_metadata m ON m.game_id=g.id":"")+" WHERE g.id=?");
    query.addBindValue(id);
    if (!query.exec() || !query.next()) return {};
    return row(query,artworkDirectory());
}
bool GameLibrary::setOrganization(const QString &id,const QString &franchise,const QStringList &inputTags,
                                  const QStringList &inputAliases,const QString &summary) {
    if(!m_ready || details(id).isEmpty())return fail(tr("Game not found in this profile."));
    if(franchise.size()>160 || franchise.contains(QChar::Null) || franchise!=franchise.trimmed()
        || inputTags.size()>MaxTags || inputAliases.size()>MaxTags || summary.size()>4096 || summary.contains(QChar::Null))
        return fail(tr("Use a franchise name up to 160 characters, at most 32 tags and aliases, and a summary up to 4096 characters."));
    QSet<QString> seen;
    QJsonArray tags;
    for(const auto &tag:inputTags){
        const auto normalized=tag.trimmed();
        if(normalized.isEmpty() || normalized.size()>64 || normalized.contains(QChar::Null) || normalized.contains(',')
            || normalized!=tag || seen.contains(normalized.toCaseFolded()))
            return fail(tr("Tags must be distinct, trimmed names of up to 64 characters."));
        seen.insert(normalized.toCaseFolded());tags.append(normalized);
    }
    QSet<QString> aliasSet;
    QJsonArray aliases;
    for(const auto &alias:inputAliases){
        if(alias.isEmpty() || alias.size()>160 || alias!=alias.trimmed() || alias.contains(';') || alias.contains(QChar::Null)
            || aliasSet.contains(alias.toCaseFolded()))
            return fail(tr("Aliases must be distinct, trimmed names of up to 160 characters."));
        aliasSet.insert(alias.toCaseFolded());aliases.append(alias);
    }
    if(!ensureRichMetadataSchema())return false;
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO game_metadata(game_id,franchise,tags,aliases,summary) VALUES(?,?,?,?,?) ON CONFLICT(game_id) DO UPDATE SET franchise=excluded.franchise,tags=excluded.tags,aliases=excluded.aliases,summary=excluded.summary");
    query.addBindValue(id);query.addBindValue(franchise);query.addBindValue(QJsonDocument(tags).toJson(QJsonDocument::Compact));
    query.addBindValue(QJsonDocument(aliases).toJson(QJsonDocument::Compact));query.addBindValue(summary);
    if(!query.exec())return fail(tr("Could not save game organization. Previous metadata was preserved. %1").arg(query.lastError().text()));
    m_error.clear();emit errorChanged();emit changed();return true;
}
QVariantList GameLibrary::collections() {
    QVariantList result;
    if(!m_ready)return result;
    if(!m_hasCollections)return result;
    QSqlQuery query(m_db);
    const auto smartPlatform=m_hasEditions
        ?QString("(SELECT count(*) FROM games g WHERE lower(g.platform)=lower(c.filter_value) OR EXISTS(SELECT 1 FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(c.filter_value)))")
        :QString("(SELECT count(*) FROM games g WHERE lower(g.platform)=lower(c.filter_value))");
    const auto countExpression="CASE WHEN c.kind='manual' THEN count(m.game_id) "
        "WHEN c.filter_type='favorite' THEN (SELECT count(*) FROM games g WHERE g.favorite=1) "
        "WHEN c.filter_type='title' THEN (SELECT count(*) FROM games g WHERE instr(lower(g.title),lower(c.filter_value))>0) "
        "WHEN c.filter_type='platform' THEN "+smartPlatform+" ELSE 0 END";
    if(!query.exec("SELECT c.id,c.name,c.kind,c.filter_type,c.filter_value,"+countExpression
        +" FROM game_collections c LEFT JOIN game_collection_members m ON m.collection_id=c.id GROUP BY c.id ORDER BY c.name COLLATE NOCASE,c.id")){
        fail(query.lastError().text());return {};
    }
    while(query.next())result.append(QVariantMap{{"id",query.value(0)},{"name",query.value(1)},
        {"kind",query.value(2)},{"filterType",query.value(3)},{"filterValue",query.value(4)},
        {"count",query.value(5).toInt()}});
    return result;
}
QString GameLibrary::createCollection(const QString &name,const QString &kind,const QString &filterType,const QString &filterValue) {
    const auto clean=name.trimmed();
    if(!m_ready || clean.isEmpty() || clean!=name || clean.size()>80 || clean.contains(QChar::Null))
        {fail(tr("Choose a collection name of up to 80 characters."));return {};}
    if(kind=="manual"){
        if(!filterType.isEmpty() || !filterValue.isEmpty()){fail(tr("Manual collections do not accept executable filters."));return {};}
    }else if(kind=="smart"){
        if(!QStringList{"platform","favorite","title"}.contains(filterType)
            || filterValue.size()>80 || filterValue.contains(QChar::Null)
            || (filterType!="favorite" && filterValue.trimmed().isEmpty())
            || (filterType=="favorite" && !filterValue.isEmpty())){
            fail(tr("Choose one supported declarative smart collection rule."));return {};
        }
    }else{fail(tr("Choose a manual or smart collection."));return {};}
    if(!m_hasCollections && !ensureOrganizationSchema())return {};
    if(collections().size()>=MaxCollections){fail(tr("This profile already has 256 game collections."));return {};}
    const auto id="collection-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery query(m_db);query.prepare("INSERT INTO game_collections(id,name,kind,filter_type,filter_value) VALUES(?,?,?,?,?)");
    query.addBindValue(id);query.addBindValue(clean);query.addBindValue(kind);query.addBindValue(filterType);query.addBindValue(filterValue);
    if(!query.exec()){fail(tr("Collection was not created. Its name may already be in use. %1").arg(query.lastError().text()));return {};}
    m_error.clear();emit errorChanged();emit changed();return id;
}
bool GameLibrary::updateCollection(const QString &id,const QString &name,const QString &filterType,const QString &filterValue) {
    const auto clean=name.trimmed();
    if(!m_ready || !m_hasCollections || !ComponentSchema::safeId(id)
        || clean.isEmpty() || clean!=name || clean.size()>80 || clean.contains(QChar::Null))
        return fail(tr("Choose a collection name of up to 80 characters."));
    QSqlQuery current(m_db);current.prepare("SELECT kind FROM game_collections WHERE id=?");current.addBindValue(id);
    if(!current.exec() || !current.next())return fail(tr("Collection not found in this profile."));
    const auto kind=current.value(0).toString();
    current.finish();
    if(kind=="manual"){
        if(!filterType.isEmpty() || !filterValue.isEmpty())return fail(tr("Manual collections do not accept executable filters."));
    }else if(kind=="smart"){
        if(!QStringList{"platform","favorite","title"}.contains(filterType)
            || filterValue.size()>80 || filterValue.contains(QChar::Null)
            || (filterType!="favorite" && filterValue.trimmed().isEmpty())
            || (filterType=="favorite" && !filterValue.isEmpty()))
            return fail(tr("Choose one supported declarative smart collection rule."));
    }else return fail(tr("Collection type is invalid."));
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);query.prepare("UPDATE game_collections SET name=?,filter_type=?,filter_value=? WHERE id=?");
    query.addBindValue(clean);query.addBindValue(filterType);query.addBindValue(filterValue);query.addBindValue(id);
    if(!query.exec() || query.numRowsAffected()!=1 || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();
        return fail(tr("Collection was not updated. Its previous settings were preserved. %1").arg(error));
    }
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::removeCollection(const QString &id) {
    if(!m_ready || !m_hasCollections || !ComponentSchema::safeId(id))return fail(tr("Collection not found in this profile."));
    QSqlQuery query(m_db);query.prepare("DELETE FROM game_collections WHERE id=?");query.addBindValue(id);
    if(!query.exec() || query.numRowsAffected()!=1)return fail(tr("Collection could not be removed."));
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::setCollectionMember(const QString &collectionId,const QString &gameId,bool member) {
    if(!m_ready || !m_hasCollections || !ComponentSchema::safeId(collectionId) || details(gameId).isEmpty())
        return fail(tr("Choose a game and collection from this profile."));
    QSqlQuery query(m_db);query.prepare("SELECT kind FROM game_collections WHERE id=?");query.addBindValue(collectionId);
    if(!query.exec() || !query.next())return fail(tr("Collection not found in this profile."));
    if(query.value(0).toString()!="manual")return fail(tr("Smart collection membership is determined by its saved rule."));
    if(!m_db.transaction())return fail(m_db.lastError().text());
    if(member){
        query.prepare("SELECT count(*) FROM game_collection_members WHERE collection_id=?");query.addBindValue(collectionId);
        if(!query.exec() || !query.next() || query.value(0).toInt()>=MaxGames){m_db.rollback();return fail(tr("Manual collection is full."));}
        query.prepare("INSERT OR IGNORE INTO game_collection_members(collection_id,game_id,position) SELECT ?,?,COALESCE(MAX(position),-1)+1 FROM game_collection_members WHERE collection_id=?");
        query.addBindValue(collectionId);query.addBindValue(gameId);query.addBindValue(collectionId);
    }else{
        query.prepare("DELETE FROM game_collection_members WHERE collection_id=? AND game_id=?");
        query.addBindValue(collectionId);query.addBindValue(gameId);
    }
    if(!query.exec() || !m_db.commit()){const auto error=query.lastError().text();m_db.rollback();return fail(tr("Collection membership was not changed. %1").arg(error));}
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::moveCollectionMember(const QString &collectionId,const QString &gameId,int direction) {
    if(!m_ready || !m_hasCollections || !ComponentSchema::safeId(collectionId)
        || !ComponentSchema::safeId(gameId) || (direction!=-1 && direction!=1))
        return fail(tr("Choose a valid manual collection and game."));
    QSqlQuery kind(m_db);kind.prepare("SELECT kind FROM game_collections WHERE id=?");kind.addBindValue(collectionId);
    if(!kind.exec() || !kind.next() || kind.value(0).toString()!="manual")
        return fail(tr("Only manual collection order can be changed."));
    kind.finish();
    QSqlQuery query(m_db);query.prepare("SELECT game_id,position FROM game_collection_members WHERE collection_id=? ORDER BY position,game_id");
    query.addBindValue(collectionId);if(!query.exec())return fail(query.lastError().text());
    QList<QPair<QString,int>> members;
    while(query.next())members.append({query.value(0).toString(),query.value(1).toInt()});
    query.finish();
    auto index=-1;
    for(qsizetype i=0;i<members.size();++i)if(members.at(i).first==gameId){index=int(i);break;}
    const auto target=qsizetype(index)+direction;
    if(index<0)return fail(tr("Game is not in this collection."));
    if(target<0 || target>=members.size()){m_error.clear();emit errorChanged();return true;}
    if(!m_db.transaction())return fail(m_db.lastError().text());
    query.prepare("UPDATE game_collection_members SET position=-1 WHERE collection_id=? AND game_id=?");
    query.addBindValue(collectionId);query.addBindValue(gameId);
    bool ok=query.exec();
    query.prepare("UPDATE game_collection_members SET position=? WHERE collection_id=? AND game_id=?");
    query.addBindValue(members.at(index).second);query.addBindValue(collectionId);query.addBindValue(members.at(target).first);
    ok=ok && query.exec();
    query.prepare("UPDATE game_collection_members SET position=? WHERE collection_id=? AND game_id=?");
    query.addBindValue(members.at(target).second);query.addBindValue(collectionId);query.addBindValue(gameId);
    ok=ok && query.exec();
    if(!ok || !m_db.commit()){
        const auto error=query.lastError().text();m_db.rollback();
        return fail(tr("Collection order was not changed. %1").arg(error));
    }
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::collectionContains(const QString &collectionId,const QString &gameId) {
    if(!m_ready || !m_hasCollections || !ComponentSchema::safeId(collectionId) || !ComponentSchema::safeId(gameId))return false;
    QSqlQuery query(m_db);query.prepare("SELECT 1 FROM game_collection_members WHERE collection_id=? AND game_id=?");
    query.addBindValue(collectionId);query.addBindValue(gameId);return query.exec() && query.next();
}
QVariantList GameLibrary::pageCollection(const QString &id,int offset) {
    if(!m_ready || !m_hasCollections || !ComponentSchema::safeId(id) || offset<0 || offset>=MaxGames || offset%PageSize)
        {fail(tr("Invalid game collection query."));return {};}
    QSqlQuery collection(m_db);collection.prepare("SELECT kind,filter_type,filter_value FROM game_collections WHERE id=?");collection.addBindValue(id);
    if(!collection.exec() || !collection.next()){fail(tr("Collection not found in this profile."));return {};}
    const auto kind=collection.value(0).toString(),filter=collection.value(1).toString(),filterValue=collection.value(2).toString();
    QString sql="SELECT g.id,g.title,g.platform,g.runtime,g.favorite,COALESCE(m.franchise,''),COALESCE(m.tags,'[]')"
        +(m_hasRichMetadata?QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')"):QString(",'[]',''"))
        +(m_hasArtwork?QString(",COALESCE(m.artwork_hash,'')"):QString(",'')"))
        +" FROM games g LEFT JOIN game_metadata m ON m.game_id=g.id";
    if(kind=="manual")sql+=" JOIN game_collection_members cm ON cm.game_id=g.id";
    sql+=" WHERE ";
    if(kind=="manual")sql+="cm.collection_id=? ORDER BY cm.position, g.id";
    else if(filter=="platform"){
        sql+="lower(g.platform)=lower(?)";
        if(m_hasEditions)sql+=" OR EXISTS(SELECT 1 FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(?))";
        sql+=" ORDER BY g.title COLLATE NOCASE,g.id";
    }else if(filter=="favorite")sql+="g.favorite=1 ORDER BY g.title COLLATE NOCASE,g.id";
    else if(filter=="title")sql+="instr(lower(g.title),lower(?))>0 ORDER BY g.title COLLATE NOCASE,g.id";
    else{fail(tr("Collection rule is invalid. No results were changed."));return {};}
    sql+=" LIMIT 50 OFFSET ?";
    QSqlQuery query(m_db);query.prepare(sql);
    if(kind=="manual")query.addBindValue(id);
    else if(filter=="platform"){query.addBindValue(filterValue);if(m_hasEditions)query.addBindValue(filterValue);}
    else if(filter=="title")query.addBindValue(filterValue);
    query.addBindValue(offset);
    if(!query.exec()){fail(query.lastError().text());return {};}
    QVariantList result;while(query.next())result.append(row(query,artworkDirectory()));return result;
}
bool GameLibrary::exportCatalog(const QString &destination) {
    if(!m_ready || !QDir::isAbsolutePath(destination) || QDir::cleanPath(destination)!=destination
        || !destination.endsWith(".json",Qt::CaseInsensitive) || QFileInfo(destination).isSymLink())
        return fail(tr("Choose a JSON file in a specifically approved media folder."));
    const auto parent=QFileInfo(destination).absolutePath();
    const auto grant=MediaGrants::validate(parent);
    if(!grant.isEmpty())return fail(grant);
    if(!approvedMediaDirectory(m_emulatorProfile,parent))
        return fail(tr("Approve this media folder for the active profile before exporting."));
    if(QFileInfo::exists(destination) && (!QFileInfo(destination).isFile() || QFileInfo(destination).canonicalFilePath()!=destination))
        return fail(tr("The catalog export target is not a regular file. Nothing was replaced."));
    QJsonObject archive{{"schemaVersion",3}};
    QJsonArray games,editionsArray,collectionArray;
    qsizetype estimatedArchiveBytes=256;
    QSqlQuery query(m_db);
    const QString gameSql="SELECT g.id,g.title,g.platform,g.runtime,g.root,g.target,g.favorite,g.added,"
        +(m_hasMetadata?QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')"):QString("'','[]'"))
        +(m_hasRichMetadata?QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')"):QString(",'[]',''"))
        +(m_hasActivity?QString(",COALESCE(a.last_played,0)"):QString(",0"))
        +(m_hasArtwork?QString(",COALESCE(m.artwork_hash,'')"):QString(",'')"))
        +" FROM games g"
        +(m_hasMetadata?QString(" LEFT JOIN game_metadata m ON m.game_id=g.id"):QString())
        +(m_hasActivity?QString(" LEFT JOIN game_activity a ON a.game_id=g.id"):QString())
        +" ORDER BY g.id";
    if(!query.exec(gameSql))return fail(tr("Cannot read the games catalog for export."));
    while(query.next()){
        if(games.size()>=MaxGames)return fail(tr("Games catalog exceeds the export bound."));
        QJsonArray tags;
        const auto tagDocument=QJsonDocument::fromJson(query.value(9).toByteArray());
        if(!tagDocument.isArray() || tagDocument.array().size()>MaxTags)return fail(tr("Game organization metadata is corrupt. Export was canceled."));
        const auto tagList=tagDocument.array();
        for(const auto &tag:tagList){
            if(!tag.isString() || tag.toString().isEmpty() || tag.toString()!=tag.toString().trimmed()
                || tag.toString().size()>64 || tag.toString().contains(',') || tag.toString().contains(QChar::Null))
                return fail(tr("Game tags are corrupt. Export was canceled."));
            tags.append(tag);
        }
        const auto aliasDocument=QJsonDocument::fromJson(query.value(10).toByteArray());
        if(!aliasDocument.isArray() || aliasDocument.array().size()>MaxTags)return fail(tr("Game aliases are corrupt. Export was canceled."));
        const auto aliasList=aliasDocument.array();
        QJsonArray aliases;
        for(const auto &alias:aliasList){
            if(!alias.isString() || alias.toString().isEmpty() || alias.toString()!=alias.toString().trimmed()
                || alias.toString().size()>160 || alias.toString().contains(';') || alias.toString().contains(QChar::Null))
                return fail(tr("Game aliases are corrupt. Export was canceled."));
            aliases.append(alias);
        }
        if(query.value(8).toString().size()>160 || query.value(8).toString()!=query.value(8).toString().trimmed()
            || query.value(8).toString().contains(QChar::Null)
            || query.value(11).toString().size()>4096 || query.value(11).toString().contains(QChar::Null))
            return fail(tr("Game summary is corrupt. Export was canceled."));
        QString encodedArtwork;
        const auto hash=query.value(13).toString();
        if(!hash.isEmpty()){
            if(!m_hasArtwork || !QRegularExpression("^[0-9a-f]{64}$").match(hash).hasMatch())
                return fail(tr("Managed artwork reference is invalid. Export was canceled."));
            const auto path=artworkDirectory()+"/"+hash+".png";QFile artwork(path);
            if(QFileInfo(path).isSymLink() || !QFileInfo(path).isFile() || QFileInfo(path).canonicalFilePath()!=path
                || QFileInfo(path).size()<1 || QFileInfo(path).size()>MaxManagedArtworkBytes || !artwork.open(QIODevice::ReadOnly))
                return fail(tr("Managed artwork is unavailable. Export was canceled."));
            const auto artworkBytes=artwork.readAll();
            if(artworkHash(artworkBytes)!=hash || normalizeArtwork(artworkBytes).isEmpty())
                return fail(tr("Managed artwork failed integrity validation. Export was canceled."));
            encodedArtwork=QString::fromLatin1(artworkBytes.toBase64());
        }
        const QJsonObject gameRecord{{"id",query.value(0).toString()},{"title",query.value(1).toString()},
            {"platform",query.value(2).toString()},{"runtimeId",query.value(3).toString()},
            {"root",query.value(4).toString()},{"target",query.value(5).toString()},
            {"favorite",query.value(6).toBool()},{"added",query.value(7).toLongLong()},
            {"franchise",query.value(8).toString()},{"tags",tags},{"aliases",aliases},
            {"summary",query.value(11).toString()},{"lastPlayed",query.value(12).toLongLong()},
            {"artwork",encodedArtwork}};
        estimatedArchiveBytes+=QJsonDocument(gameRecord).toJson(QJsonDocument::Compact).size()+1;
        if(estimatedArchiveBytes>MaxCatalogArchiveBytes)return fail(tr("Catalog export exceeds the 32 MiB size limit."));
        games.append(gameRecord);
    }
    query.finish();
    if(m_hasEditions){
        if(!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions ORDER BY id"))
            return fail(tr("Cannot read game editions for export."));
        while(query.next()){
            if(editionsArray.size()>=MaxGames)return fail(tr("Game editions exceed the export bound."));
            const QJsonObject editionRecord{{"id",query.value(0).toString()},{"gameId",query.value(1).toString()},
            {"title",query.value(2).toString()},{"platform",query.value(3).toString()},{"runtimeId",query.value(4).toString()},
            {"root",query.value(5).toString()},{"target",query.value(6).toString()}};
            estimatedArchiveBytes+=QJsonDocument(editionRecord).toJson(QJsonDocument::Compact).size()+1;
            if(estimatedArchiveBytes>MaxCatalogArchiveBytes)return fail(tr("Catalog export exceeds the 32 MiB size limit."));
            editionsArray.append(editionRecord);
        }
        query.finish();
    }
    if(m_hasCollections){
        if(!query.exec("SELECT id,name,kind,filter_type,filter_value FROM game_collections ORDER BY id"))
            return fail(tr("Cannot read game collections for export."));
        while(query.next()){
            if(collectionArray.size()>=MaxCollections)return fail(tr("Game collections exceed the export bound."));
            const auto collectionId=query.value(0).toString();
            QJsonArray members;
            QSqlQuery memberQuery(m_db);memberQuery.prepare("SELECT game_id FROM game_collection_members WHERE collection_id=? ORDER BY position,game_id");memberQuery.addBindValue(collectionId);
            if(!memberQuery.exec())return fail(tr("Cannot read collection membership for export."));
            while(memberQuery.next()){
                if(members.size()>=MaxGames)return fail(tr("Manual collection exceeds the export bound."));
                members.append(memberQuery.value(0).toString());
            }
            const QJsonObject collectionRecord{{"id",collectionId},{"name",query.value(1).toString()},
                {"kind",query.value(2).toString()},{"filterType",query.value(3).toString()},
                {"filterValue",query.value(4).toString()},{"members",members}};
            estimatedArchiveBytes+=QJsonDocument(collectionRecord).toJson(QJsonDocument::Compact).size()+1;
            if(estimatedArchiveBytes>MaxCatalogArchiveBytes)return fail(tr("Catalog export exceeds the 32 MiB size limit."));
            collectionArray.append(collectionRecord);
        }
    }
    archive.insert("games",games);archive.insert("editions",editionsArray);archive.insert("collections",collectionArray);
    const auto bytes=QJsonDocument(archive).toJson(QJsonDocument::Compact);
    if(bytes.size()>MaxCatalogArchiveBytes)return fail(tr("Catalog export exceeds the 32 MiB size limit."));
    QSaveFile file(destination);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
        || file.write(bytes)!=bytes.size() || !file.commit()){
        file.cancelWriting();return fail(tr("Catalog export failed. The previous file was preserved. %1").arg(file.errorString()));
    }
    m_error.clear();emit errorChanged();return true;
}
bool GameLibrary::restoreCatalog(const QString &source,bool confirmed) {
    if(m_portablePhase=="uninstall")return fail(tr("Wait for the managed uninstall to finish before restoring the catalog."));
    if(!m_ready || !confirmed || !QDir::isAbsolutePath(source) || QDir::cleanPath(source)!=source
        || QFileInfo(source).isSymLink() || !QFileInfo(source).isFile() || QFileInfo(source).canonicalFilePath()!=source
        || QFileInfo(source).size()<1 || QFileInfo(source).size()>MaxCatalogArchiveBytes)
        return fail(tr("Review and confirm a readable Games JSON backup from an approved folder."));
    const auto grant=MediaGrants::validate(QFileInfo(source).absolutePath());
    if(!grant.isEmpty())return fail(grant);
    if(!approvedMediaDirectory(m_emulatorProfile,QFileInfo(source).absolutePath()))
        return fail(tr("Approve this media folder for the active profile before restoring."));
    QFile file(source);if(!file.open(QIODevice::ReadOnly))return fail(tr("Cannot read the selected Games backup."));
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);
    if(parse.error!=QJsonParseError::NoError || !document.isObject())return fail(tr("Invalid Games backup JSON."));
    const auto archive=document.object();
    const auto archiveVersion=archive.value("schemaVersion").toInt(-1);
    if(!exactKeys(archive,{"schemaVersion","games","editions","collections"})
        || !archive.value("schemaVersion").isDouble() || archive.value("schemaVersion").toDouble()!=archiveVersion
        || (archiveVersion!=1 && archiveVersion!=2 && archiveVersion!=3)
        || !archive.value("games").isArray() || archive.value("games").toArray().size()>MaxGames
        || !archive.value("editions").isArray() || archive.value("editions").toArray().size()>MaxGames
        || !archive.value("collections").isArray() || archive.value("collections").toArray().size()>MaxCollections)
        return fail(tr("Unsupported Games backup version or catalog bounds."));

    QList<QVariantMap> games,editionsData,collectionsData;
    QSet<QString> ids,gameIds,platforms,names,installations;
    QMap<QString,int> editionCounts;
    qint64 restoreArtworkBytes=0;
    for(const auto &entry:archive.value("games").toArray()){
        if(!entry.isObject())return fail(tr("Invalid game record in backup."));
        const auto object=entry.toObject();
        const auto gameKeys=archiveVersion==1
            ?QStringList{"id","title","platform","runtimeId","root","target","favorite","added","franchise","tags","lastPlayed"}
            :archiveVersion==2
                ?QStringList{"id","title","platform","runtimeId","root","target","favorite","added","franchise","tags","aliases","summary","lastPlayed"}
                :QStringList{"id","title","platform","runtimeId","root","target","favorite","added","franchise","tags","aliases","summary","lastPlayed","artwork"};
        if(!exactKeys(object,gameKeys)
            || !object.value("id").isString() || !ComponentSchema::safeId(object.value("id").toString())
            || ids.contains(object.value("id").toString()) || !object.value("favorite").isBool()
            || !object.value("franchise").isString() || object.value("franchise").toString().size()>160
            || object.value("franchise").toString().contains(QChar::Null)
            || !object.value("tags").isArray() || object.value("tags").toArray().size()>MaxTags
            || (archiveVersion>=2 && (!object.value("aliases").isArray() || object.value("aliases").toArray().size()>MaxTags
                || !object.value("summary").isString() || object.value("summary").toString().size()>4096
                || object.value("summary").toString().contains(QChar::Null))))
            return fail(tr("Invalid or duplicate game identity or organization metadata in backup."));
        if(archiveVersion==3 && (!object.value("artwork").isString()
            || object.value("artwork").toString().size()>((MaxManagedArtworkBytes+2)/3)*4))
            return fail(tr("Invalid or oversized artwork in backup."));
        const auto gameId=object.value("id").toString(),platform=object.value("platform").toString();
        QVariantMap registration{{"schemaVersion",1},{"title",object.value("title").toString()},
            {"platform",platform},{"runtimeId",object.value("runtimeId").toString()},
            {"root",object.value("root").toString()},{"target",object.value("target").toString()}};
        QString schemaError;if(ComponentSchema::gameRegistration(registration,&schemaError).isEmpty())return fail(schemaError);
        const auto root=registration.value("root").toString();
        if(!root.isEmpty() && !OpticalDiscAdapter::isRootReference(root) && !MediaGrants::validate(root,false).isEmpty())return fail(tr("Backup refers to a game folder not authorized in this profile."));
        if(object.value("franchise").toString()!=object.value("franchise").toString().trimmed())return fail(tr("Franchise names in the backup must be trimmed."));
        const auto installIdentity=registration.value("runtimeId").toString()+"\n"+root+"\n"+registration.value("target").toString();
        if(installations.contains(installIdentity))return fail(tr("Backup contains duplicate installation identities."));
        installations.insert(installIdentity);
        if(platforms.size()>=MaxPlatforms && !platforms.contains(platform.toCaseFolded()))return fail(tr("Backup contains more than 128 platforms."));
        platforms.insert(platform.toCaseFolded());ids.insert(gameId);gameIds.insert(gameId);
        QSet<QString> tagSet;
        for(const auto &tag:object.value("tags").toArray()){
            if(!tag.isString() || tag.toString().isEmpty() || tag.toString()!=tag.toString().trimmed()
                || tag.toString().size()>64 || tag.toString().contains(',') || tag.toString().contains(QChar::Null)
                || tagSet.contains(tag.toString().toCaseFolded()))return fail(tr("Invalid or duplicate tag in backup."));
            tagSet.insert(tag.toString().toCaseFolded());
        }
        const auto archiveAliases=archiveVersion>=2?object.value("aliases").toArray():QJsonArray{};
        QSet<QString> aliasSet;
        for(const auto &alias:archiveAliases){
            if(!alias.isString() || alias.toString().isEmpty() || alias.toString()!=alias.toString().trimmed()
                || alias.toString().size()>160 || alias.toString().contains(';') || alias.toString().contains(QChar::Null)
                || aliasSet.contains(alias.toString().toCaseFolded()))
                return fail(tr("Invalid or duplicate alias in backup."));
            aliasSet.insert(alias.toString().toCaseFolded());
        }
        qint64 added=0,lastPlayed=0;
        if(!archiveInteger(object.value("added"),&added) || !archiveInteger(object.value("lastPlayed"),&lastPlayed))
            return fail(tr("Invalid game activity timestamp in backup."));
        auto value=registration;value["id"]=gameId;value["favorite"]=object.value("favorite").toBool();
        value["added"]=added;value["franchise"]=object.value("franchise").toString();
        QVariantList tagValues;for(const auto &tag:object.value("tags").toArray())tagValues.append(tag.toString());
        value["tags"]=tagValues;value["lastPlayed"]=lastPlayed;
        QVariantList archiveAliasValues;for(const auto &alias:archiveAliases)archiveAliasValues.append(alias.toString());
        value["aliases"]=archiveAliasValues;value["summary"]=archiveVersion>=2?object.value("summary").toString():QString();
        QByteArray artworkBytes;
        if(archiveVersion==3){
            const auto encoded=object.value("artwork").toString().toLatin1();
            if(!encoded.isEmpty()){
                const auto decoded=QByteArray::fromBase64(encoded,QByteArray::AbortOnBase64DecodingErrors);
                if(decoded.isEmpty() || decoded.size()>MaxManagedArtworkBytes || decoded.toBase64()!=encoded)
                    return fail(tr("Invalid encoded artwork in backup."));
                artworkBytes=normalizeArtwork(decoded);
                if(artworkBytes.isEmpty())return fail(tr("Backup artwork is not a supported bounded PNG or JPEG."));
                restoreArtworkBytes+=artworkBytes.size();
                if(restoreArtworkBytes>MaxRestoreArtworkBytes)
                    return fail(tr("Backup artwork exceeds the 128 MiB restore budget."));
            }
        }
        value["artworkBytes"]=artworkBytes;
        games.append(value);
    }
    for(const auto &entry:archive.value("editions").toArray()){
        if(!entry.isObject())return fail(tr("Invalid edition record in backup."));
        const auto object=entry.toObject();
        if(!exactKeys(object,{"id","gameId","title","platform","runtimeId","root","target"})
            || !object.value("id").isString() || !object.value("gameId").isString()
            || !ComponentSchema::safeId(object.value("id").toString()) || !gameIds.contains(object.value("gameId").toString())
            || ids.contains(object.value("id").toString()))return fail(tr("Invalid or duplicate edition identity in backup."));
        QVariantMap registration{{"schemaVersion",1},{"title",object.value("title").toString()},
            {"platform",object.value("platform").toString()},{"runtimeId",object.value("runtimeId").toString()},
            {"root",object.value("root").toString()},{"target",object.value("target").toString()}};
        QString schemaError;if(ComponentSchema::gameRegistration(registration,&schemaError).isEmpty())return fail(schemaError);
        const auto root=registration.value("root").toString();
        if(!root.isEmpty() && !OpticalDiscAdapter::isRootReference(root) && !MediaGrants::validate(root,false).isEmpty())return fail(tr("Backup edition refers to a game folder not authorized in this profile."));
        const auto folded=registration.value("platform").toString().toCaseFolded();
        if(platforms.size()>=MaxPlatforms && !platforms.contains(folded))return fail(tr("Backup contains more than 128 platforms."));
        platforms.insert(folded);
        const auto identity=registration.value("runtimeId").toString()+"\n"+root+"\n"+registration.value("target").toString();
        if(installations.contains(identity))return fail(tr("Two backup editions refer to the same installation."));
        installations.insert(identity);
        const auto gameId=object.value("gameId").toString();
        if(++editionCounts[gameId]>=32)return fail(tr("A backup game has more than 31 additional editions."));
        ids.insert(object.value("id").toString());
        auto value=registration;value["id"]=object.value("id").toString();value["gameId"]=gameId;editionsData.append(value);
    }
    for(const auto &entry:archive.value("collections").toArray()){
        if(!entry.isObject())return fail(tr("Invalid collection record in backup."));
        const auto object=entry.toObject();const auto kind=object.value("kind").toString();
        if(!exactKeys(object,{"id","name","kind","filterType","filterValue","members"})
            || !object.value("id").isString() || !ComponentSchema::safeId(object.value("id").toString())
            || ids.contains(object.value("id").toString()) || !object.value("name").isString()
            || !object.value("kind").isString() || !object.value("filterType").isString() || !object.value("filterValue").isString()
            || !object.value("members").isArray() || object.value("members").toArray().size()>MaxGames)
            return fail(tr("Invalid collection identity or membership in backup."));
        const auto name=object.value("name").toString();
        const auto foldedName=name.toCaseFolded();
        if(name.isEmpty() || name!=name.trimmed() || name.size()>80 || name.contains(QChar::Null) || names.contains(foldedName))
            return fail(tr("Invalid or duplicate collection name in backup."));
        names.insert(foldedName);
        const auto filterType=object.value("filterType").toString(),filterValue=object.value("filterValue").toString();
        if(kind=="manual"){
            if(!filterType.isEmpty() || !filterValue.isEmpty())return fail(tr("Manual collections cannot contain filter expressions."));
        }else if(kind=="smart"){
            if(!QStringList{"platform","favorite","title"}.contains(filterType) || filterValue.size()>80
                || filterValue.contains(QChar::Null) || (filterType!="favorite" && filterValue.trimmed().isEmpty())
                || (filterType=="favorite" && !filterValue.isEmpty()) || !object.value("members").toArray().isEmpty())
                return fail(tr("Invalid smart collection rule in backup."));
        }else return fail(tr("Unknown collection type in backup."));
        QSet<QString> memberSet;
        for(const auto &member:object.value("members").toArray()){
            if(!member.isString() || !gameIds.contains(member.toString()) || memberSet.contains(member.toString()))
                return fail(tr("Collection refers to an unknown or duplicate game."));
            memberSet.insert(member.toString());
        }
        ids.insert(object.value("id").toString());
        collectionsData.append({{"id",object.value("id").toString()},{"name",name},{"kind",kind},
            {"filterType",filterType},{"filterValue",filterValue},{"members",object.value("members").toArray().toVariantList()}});
    }
    if(!ensureArtworkSchema())return false;
    for(auto &game:games){
        const auto bytes=game.value("artworkBytes").toByteArray();
        if(bytes.isEmpty())continue;
        QString hash,error;
        if(!persistArtwork(m_db.databaseName(),bytes,&hash,&error))
            return fail(tr("Backup artwork could not be staged. Nothing in the catalog was replaced. %1").arg(error));
        game["artworkHash"]=hash;
    }
    const auto backupPath=m_db.databaseName()+".pre-restore-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".sqlite";
    if(QFileInfo::exists(backupPath) || QFileInfo(backupPath).isSymLink())return fail(tr("Cannot create a private recovery backup before restore."));
    QSqlQuery backup(m_db);backup.prepare("VACUUM main INTO ?");backup.addBindValue(backupPath);
    if(!backup.exec())return fail(tr("Could not preserve the current catalog before restore. Nothing was replaced."));
    if(!QFile::setPermissions(backupPath,QFile::ReadOwner|QFile::WriteOwner)){
        QFile::remove(backupPath);
        return fail(tr("Could not secure the private recovery copy. Nothing was replaced."));
    }
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    bool ok=query.exec("DELETE FROM game_collection_members") && query.exec("DELETE FROM game_collections")
        && query.exec("DELETE FROM game_activity") && query.exec("DELETE FROM game_metadata");
    if(m_hasEditions)ok=ok && query.exec("DELETE FROM game_editions");
    ok=ok && query.exec("DELETE FROM games");
    const bool needEditionTable=!editionsData.isEmpty() && !m_hasEditions;
    if(ok && needEditionTable)ok=query.exec("CREATE TABLE game_editions(id TEXT PRIMARY KEY,game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,title TEXT NOT NULL,platform TEXT NOT NULL,runtime TEXT NOT NULL,root TEXT NOT NULL,target TEXT NOT NULL,UNIQUE(runtime,root,target))")
        && query.exec("CREATE INDEX game_editions_owner ON game_editions(game_id)");
    for(const auto &game:games){
        query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,favorite,added) VALUES(?,?,?,?,?,?,?,?)");
        for(const auto &key:QStringList{"id","title","platform","runtimeId","root","target","favorite","added"})
            query.addBindValue(game.value(key));
        ok=ok && query.exec();
        query.prepare("INSERT INTO game_metadata(game_id,franchise,tags,aliases,summary,artwork_hash) VALUES(?,?,?,?,?,?)");
        query.addBindValue(game.value("id"));query.addBindValue(game.value("franchise"));
        QJsonArray tags;for(const auto &tag:game.value("tags").toStringList())tags.append(tag);
        QJsonArray aliases;for(const auto &alias:game.value("aliases").toStringList())aliases.append(alias);
        query.addBindValue(QJsonDocument(tags).toJson(QJsonDocument::Compact));
        query.addBindValue(QJsonDocument(aliases).toJson(QJsonDocument::Compact));
        query.addBindValue(game.value("summary"));query.addBindValue(game.value("artworkHash"));ok=ok && query.exec();
        if(game.value("lastPlayed").toLongLong()>0){
            query.prepare("INSERT INTO game_activity(game_id,last_played) VALUES(?,?)");
            query.addBindValue(game.value("id"));query.addBindValue(game.value("lastPlayed"));ok=ok && query.exec();
        }
        if(!ok)break;
    }
    for(const auto &edition:editionsData){
        if(!ok)break;
        query.prepare("INSERT INTO game_editions(id,game_id,title,platform,runtime,root,target) VALUES(?,?,?,?,?,?,?)");
        for(const auto &key:QStringList{"id","gameId","title","platform","runtimeId","root","target"})query.addBindValue(edition.value(key));
        ok=query.exec();
    }
    for(const auto &collection:collectionsData){
        if(!ok)break;
        query.prepare("INSERT INTO game_collections(id,name,kind,filter_type,filter_value) VALUES(?,?,?,?,?)");
        for(const auto &key:QStringList{"id","name","kind","filterType","filterValue"})query.addBindValue(collection.value(key));
        ok=query.exec();
        const auto members=collection.value("members").toList();
        for(qsizetype position=0;position<members.size();++position){
            if(!ok)break;
            query.prepare("INSERT INTO game_collection_members(collection_id,game_id,position) VALUES(?,?,?)");
            query.addBindValue(collection.value("id"));query.addBindValue(members.at(position));query.addBindValue(position);
            ok=query.exec();
        }
    }
    ok=ok && query.exec("PRAGMA user_version=6");
    if(!ok || !m_db.commit()){
        const auto reason=query.lastError().text();m_db.rollback();
        return fail(tr("Restore failed; the prior catalog remains intact. Recovery copy: %1. %2").arg(backupPath,reason));
    }
    m_hasActivity=true;m_hasMetadata=true;m_hasCollections=true;m_hasRichMetadata=true;m_hasArtwork=true;m_hasEditions=m_hasEditions || needEditionTable;
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::setFavorite(const QString &id, bool favorite) {
    if (!m_ready || details(id).isEmpty()) return fail(tr("Game not found in this profile."));
    QSqlQuery query(m_db); query.prepare("UPDATE games SET favorite=? WHERE id=?"); query.addBindValue(favorite?1:0); query.addBindValue(id);
    if (!query.exec()) return fail(query.lastError().text());
    m_error.clear(); emit changed(); return true;
}
bool GameLibrary::platformAvailable(const QString &platform) {
    if(!m_ready || platform.trimmed().isEmpty() || platform.size()>80)return false;
    QSqlQuery query(m_db);
    query.prepare("SELECT count(DISTINCT lower(platform)) FROM (SELECT platform FROM games"
        +QString(m_hasEditions?" UNION ALL SELECT platform FROM game_editions":"")+")");
    if(!query.exec() || !query.next())return false;
    const int count=query.value(0).toInt();query.finish();
    if(count<MaxPlatforms)return true;
    query.prepare("SELECT EXISTS(SELECT 1 FROM games WHERE lower(platform)=lower(?))"
        +QString(m_hasEditions?" OR EXISTS(SELECT 1 FROM game_editions WHERE lower(platform)=lower(?))":""));
    query.addBindValue(platform);
    if(m_hasEditions)query.addBindValue(platform);
    return query.exec() && query.next() && query.value(0).toBool();
}
bool GameLibrary::removeGame(const QString &id) {
    if (!m_ready || details(id).isEmpty()) return fail(tr("Game not found in this profile."));
    QString oldHash;
    if(m_hasArtwork){
        QSqlQuery artwork(m_db);artwork.prepare("SELECT artwork_hash FROM game_metadata WHERE game_id=?");artwork.addBindValue(id);
        if(!artwork.exec())return fail(artwork.lastError().text());
        if(artwork.next())oldHash=artwork.value(0).toString();
        artwork.finish();
    }
    QSqlQuery query(m_db); query.prepare("DELETE FROM games WHERE id=?"); query.addBindValue(id);
    if (!query.exec()) return fail(query.lastError().text());
    if(!oldHash.isEmpty() && m_hasArtwork){
        QSqlQuery references(m_db);references.prepare("SELECT count(*) FROM game_metadata WHERE artwork_hash=?");references.addBindValue(oldHash);
        const auto path=artworkDirectory()+"/"+oldHash+".png";
        if(references.exec() && references.next() && references.value(0).toInt()==0
            && QRegularExpression("^[0-9a-f]{64}$").match(oldHash).hasMatch())QFile::remove(path);
    }
    // Catalog-only operation: deliberately never deletes game/runtime/save files.
    m_error.clear(); emit changed(); return true;
}
bool GameLibrary::canUninstallManagedGame(const QString &id) const {
    if(!m_ready || !ComponentSchema::safeId(id))return false;
    QSqlQuery query(m_db);query.prepare("SELECT root,target FROM games WHERE id=?");query.addBindValue(id);
    if(!query.exec() || !query.next())return false;
    const auto root=query.value(0).toString(),target=query.value(1).toString();
    if(!ManagedGamePaths::isInstallRoot(m_emulatorProfile,root)
        || target.isEmpty() || QDir::isAbsolutePath(target)
        || QDir::cleanPath(target)!=target || target.split('/').contains("..")
        || !QFileInfo(root+'/'+target).isFile())return false;
    if(m_hasEditions){
        QSqlQuery editionsQuery(m_db);editionsQuery.prepare("SELECT count(*) FROM game_editions WHERE game_id=? OR root=?");
        editionsQuery.addBindValue(id);editionsQuery.addBindValue(root);
        if(!editionsQuery.exec() || !editionsQuery.next() || editionsQuery.value(0).toInt()!=0)return false;
    }
    QSqlQuery gamesQuery(m_db);gamesQuery.prepare("SELECT count(*) FROM games WHERE root=? AND id<>?");
    gamesQuery.addBindValue(root);gamesQuery.addBindValue(id);
    if(!gamesQuery.exec() || !gamesQuery.next() || gamesQuery.value(0).toInt()!=0)return false;
    return true;
}
bool GameLibrary::uninstallManagedGame(const QString &id,bool confirmed) {
    if(m_sessionActiveGuard && m_sessionActiveGuard())
        return fail(tr("A session is active or starting. Return Home before uninstalling a game."));
    if(!m_ready || !confirmed || portableInstallBusy() || !m_portablePhase.isEmpty()
        || !canUninstallManagedGame(id))
        return fail(tr("Only a separately confirmed, verified Isolde-managed install can be uninstalled."));
    auto helper=QCoreApplication::applicationDirPath()+"/share/isolde/portable-game-installer.py";
    if(!QFileInfo(helper).isReadable())helper=QCoreApplication::applicationDirPath()+"/../share/isolde/portable-game-installer.py";
    const auto python=QStandardPaths::findExecutable("python3");
    if(python.isEmpty() || !QFileInfo(helper).isReadable())return fail(tr("Repair the Isolde managed-game verifier."));
    const auto record=registration(id),root=record.value("root").toString();
    if(!ManagedGamePaths::isInstallRoot(m_emulatorProfile,root))return fail(tr("The managed installation changed; no files were removed."));
    m_portableGameId=id;m_portableRuntime.clear();m_portableDestination=root;
    m_portableInstallOutput.clear();m_portableInstallError.clear();m_portableCanceled=false;
    m_portablePhase="uninstall";m_portableInstallStatus="Verifying the complete owned-file manifest before removing this install. Profile saves are outside the install folder.";
    m_portableInstall.start(python,{helper,"--uninstall","--root",root});
    m_portableInstallDeadline.start();m_error.clear();emit errorChanged();emit portableInstallChanged();return true;
}
QVariantMap GameLibrary::resolveLaunch(const QString &id,const QString &editionId) {
    m_error.clear();emit errorChanged();
    if(m_portablePhase=="uninstall"){fail(tr("Wait for the managed uninstall to finish before launching a game."));return {};}
    if (!m_ready || !ComponentSchema::safeId(id)) { fail(tr("Game not found in this profile.")); return {}; }
    if(editionId.isEmpty() && editions(id).size()>1){fail(tr("Choose which edition to play."));return {};}
    if(!editionId.isEmpty() && !ComponentSchema::safeId(editionId)){fail(tr("Invalid edition identity."));return {};}
    QSqlQuery query(m_db);
    if(editionId.isEmpty() || editionId==id){query.prepare("SELECT runtime,root,target FROM games WHERE id=?");query.addBindValue(id);}
    else if(m_hasEditions){query.prepare("SELECT runtime,root,target FROM game_editions WHERE game_id=? AND id=?");query.addBindValue(id);query.addBindValue(editionId);}
    else{fail(tr("Edition not found in this profile."));return {};}
    if (!query.exec() || !query.next()) { fail(tr("Game not found in this profile.")); return {}; }
    const auto runtime=query.value(0).toString(), root=query.value(1).toString(), relative=query.value(2).toString();
    if (!compatibleRuntime(runtime,!root.isEmpty())) { fail(tr("The game runtime is missing or incompatible. Repair it and retry.")); return {}; }
    if(runtime=="steam"){
        if(ComponentSchema::gameRegistration({{"schemaVersion",1},{"title","Steam game"},{"platform","Steam"},{"runtimeId",runtime},{"root",root},{"target",relative}}).isEmpty()){
            fail(tr("Invalid Steam identity. Remove the entry and import it again."));return {};
        }
        return {{"type","steam-game"},{"steamAppId",relative}};
    }
    if (root.isEmpty()) return {{"type","launch-app"},{"appId",runtime}};
    const auto target=checkedTarget(root,relative); if (target.isEmpty()) return {};
    const bool optical=OpticalDiscAdapter::isRootReference(root);
    const auto media=optical?OpticalDiscAdapter::resolveRootReference(root).value("disc").toMap():QVariantMap{};
    const auto sourceRoot=optical?media.value("mountRoot").toString():root;
    QVariantMap action{{"type","open-file"},{"openWith",runtime},{"target",target},{"sourceRoot",sourceRoot}};
    if(optical)action["opticalMediaId"]=media.value("mediaId");
    return action;
}
bool GameLibrary::installationExists(const QString &runtime,const QString &root,const QString &target) {
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?"+QString(m_hasEditions?" UNION ALL SELECT id FROM game_editions WHERE runtime=? AND root=? AND target=?":""));
    for(int i=0;i<(m_hasEditions?2:1);++i){query.addBindValue(runtime);query.addBindValue(root);query.addBindValue(target);}
    if(!query.exec()){fail(query.lastError().text());return true;}
    return query.next();
}
QVariantList GameLibrary::editions(const QString &id) {
    const auto base=registration(id);if(base.isEmpty())return {};
    QVariantList result{QVariantMap{{"id",id},{"title",base.value("title")},{"platform",base.value("platform")},{"runtimeId",base.value("runtimeId")}}};
    if(!m_hasEditions)return result;
    QSqlQuery query(m_db);query.prepare("SELECT id,title,platform,runtime FROM game_editions WHERE game_id=? ORDER BY id LIMIT 31");query.addBindValue(id);
    if(!query.exec()){fail(query.lastError().text());return {};}
    while(query.next())result.append(QVariantMap{{"id",query.value(0)},{"title",query.value(1)},{"platform",query.value(2)},{"runtimeId",query.value(3)}});
    return result;
}
QString GameLibrary::addEdition(const QString &id,const QVariantMap &input) {
    if(m_portablePhase=="uninstall"){fail(tr("Wait for the managed uninstall to finish before changing game sources."));return {};}
    if(!m_ready || registration(id).isEmpty()){fail(tr("Game not found in this profile."));return {};}
    QString error;const auto value=ComponentSchema::gameRegistration(input,&error);
    if(value.isEmpty()){fail(error);return {};}
    const auto runtime=value.value("runtimeId").toString(),root=value.value("root").toString(),target=value.value("target").toString();
    if(!platformAvailable(value.value("platform").toString())){fail(tr("The Games catalog supports up to 128 distinct platforms."));return {};}
    if(!compatibleRuntime(runtime,!root.isEmpty())){fail(tr("Choose a compatible edition runtime."));return {};}
    if(!root.isEmpty() && checkedTarget(root,target).isEmpty())return {};
    if(editions(id).size()>=32){fail(tr("A game supports up to 32 editions."));return {};}
    if(installationExists(runtime,root,target)){fail(tr("This installation already belongs to a game or edition."));return {};}
    QSqlQuery query(m_db);
    if(!m_hasEditions){
        // User-confirmed first edition edit is the upgrade boundary, never startup.
        const auto backup=m_db.databaseName()+".v1-backup.sqlite";
        if(QFileInfo::exists(backup) || QFileInfo(backup).isSymLink()){fail(tr("A pre-edition backup already exists. Preserve it before retrying the catalog upgrade."));return {};}
        query.prepare("VACUUM main INTO ?");query.addBindValue(backup);
        if(!query.exec()){fail(tr("Cannot back up the catalog before adding editions."));return {};}
    }
    if(!m_db.transaction()){fail(m_db.lastError().text());return {};}
    if(!m_hasEditions && (!query.exec("CREATE TABLE game_editions(id TEXT PRIMARY KEY,game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,title TEXT NOT NULL,platform TEXT NOT NULL,runtime TEXT NOT NULL,root TEXT NOT NULL,target TEXT NOT NULL,UNIQUE(runtime,root,target))")
        || !query.exec("CREATE INDEX game_editions_owner ON game_editions(game_id)")
        || (!m_hasActivity && !query.exec("PRAGMA user_version=2")))){m_db.rollback();fail(query.lastError().text());return {};}
    const auto edition="edition-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.prepare("INSERT INTO game_editions(id,game_id,title,platform,runtime,root,target) VALUES(?,?,?,?,?,?,?)");query.addBindValue(edition);query.addBindValue(id);
    for(const auto &key:QStringList{"title","platform","runtimeId","root","target"})query.addBindValue(value.value(key));
    if(!query.exec() || !m_db.commit()){m_db.rollback();fail(tr("Edition could not be saved; the previous catalog was preserved."));return {};}
    m_hasEditions=true;m_error.clear();emit errorChanged();emit changed();return edition;
}
void GameLibrary::cancelImport() {
    m_steamScanCanceled=true;m_steamScanDeadline.stop();
    if(m_steamScan.state()!=QProcess::NotRunning)m_steamScan.kill();
    m_steamScanOutput.clear();m_importStatus.clear();
    m_scanTimer.stop();m_scan.reset();m_preview.clear();m_importReady=false;
    m_importRoot.clear();m_importRuntime.clear();m_importPlatform.clear();m_importFormats.clear();
    m_scanEntries=0;emit importChanged();
}
bool GameLibrary::beginSteamImport(){
    if(!m_ready || scanning() || m_importReady)return fail(tr("Finish or cancel the current import first."));
    if(!compatibleRuntime("steam",false))return fail(tr("Install Steam from the Isolde Store first."));
    if(m_emulatorProfile!="default" && (QUuid(m_emulatorProfile).isNull() || QUuid(m_emulatorProfile).toString(QUuid::WithoutBraces)!=m_emulatorProfile))return fail(tr("Invalid active profile."));
    auto helper=QCoreApplication::applicationDirPath()+"/share/isolde/steam-games.py";
    if(!QFileInfo(helper).isReadable())helper=QCoreApplication::applicationDirPath()+"/../share/isolde/steam-games.py";
    const auto python=QStandardPaths::findExecutable("python3");
    if(python.isEmpty() || !QFileInfo(helper).isReadable())return fail(tr("Repair the Isolde Steam adapter."));
    const auto home=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+m_emulatorProfile+"/steam/home";
    const auto libraries=SteamLibraryGrants::folders(m_emulatorProfile);
    const auto grantsError=SteamLibraryGrants::validateList(libraries,false);
    if(!grantsError.isEmpty())return fail(tr("Review external Steam library access in Games settings. %1").arg(grantsError));
    QStringList arguments{helper,"--discover","--home",home};
    for(const auto &library:libraries){
        if(QFileInfo(library).isDir())arguments << "--library" << library;
    }
    m_preview.clear();m_steamScanOutput.clear();m_steamScanCanceled=false;m_importRuntime="steam";m_importRoot=QStringLiteral("");m_importReady=false;
    m_error.clear();emit errorChanged();m_importStatus=tr("Reading installed games in this profile's approved Steam libraries. Nothing is saved yet.");
    m_steamScan.start(python,arguments);m_steamScanDeadline.start();emit importChanged();return true;
}
QStringList GameLibrary::steamLibraries() const {
    return SteamLibraryGrants::folders(m_emulatorProfile);
}
bool GameLibrary::addSteamLibrary(const QString &path) {
    if(!m_ready)return fail(tr("Games is unavailable in this profile."));
    auto libraries=steamLibraries();
    if(libraries.contains(path))return fail(tr("That external Steam library is already approved."));
    const auto validation=SteamLibraryGrants::validate(path);
    if(!validation.isEmpty())return fail(validation);
    libraries.append(path);
    const auto error=SteamLibraryGrants::save(m_emulatorProfile,libraries);
    if(!error.isEmpty())return fail(error);
    m_error.clear();emit errorChanged();emit changed();return true;
}
bool GameLibrary::removeSteamLibrary(const QString &path) {
    if(!m_ready)return fail(tr("Games is unavailable in this profile."));
    auto libraries=steamLibraries();
    if(!libraries.removeOne(path))return fail(tr("That external Steam library is not approved in this profile."));
    const auto error=SteamLibraryGrants::save(m_emulatorProfile,libraries);
    if(!error.isEmpty())return fail(error);
    m_error.clear();emit errorChanged();emit changed();return true;
}
QVariantList GameLibrary::mountedOpticalDiscs() const {
    return m_ready ? OpticalDiscAdapter::mountedDiscs() : QVariantList{};
}
QVariantMap GameLibrary::revalidateOpticalDisc(const QString &observationId,const QString &device) const {
    if(!m_ready || !QRegularExpression("^[0-9a-f]{64}$").match(observationId).hasMatch()
        || !QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch())
        return {{"schemaVersion",1},{"state","invalid"}};
    return OpticalDiscAdapter::revalidate(observationId,device);
}
QString GameLibrary::registerOpticalGame(const QString &title,const QString &runtimeId,
                                         const QString &relativeExecutable,const QString &observationId,
                                         const QString &device){
    if(!m_ready || !compatibleRuntime(runtimeId,true)){fail(tr("Choose an installed file-based game runtime."));return {};}
    if(title.trimmed().isEmpty() || title!=title.trimmed() || title.size()>256){fail(tr("Enter a game title."));return {};}
    const auto media=revalidateOpticalDisc(observationId,device);
    if(media.value("state")!="ready"){fail(tr("Optical media changed. Refresh disc details and review it again."));return {};}
    const auto disc=media.value("disc").toMap();
    const auto root=OpticalDiscAdapter::rootReference(disc);
    if(root.isEmpty() || !disc.value("directPlaySupported").toBool()){
        fail(tr("This disc has no readable ISO identity. Direct Play is supported only for identified ISO9660 discs."));return {};
    }
    const auto grantError=OpticalDiscGrants::authorize(m_emulatorProfile,disc.value("mediaId").toString());
    if(!grantError.isEmpty()){fail(grantError);return {};}
    const auto target=checkedTarget(root,relativeExecutable);
    if(target.isEmpty())return {};
    QFile executable(target);
    if(!QFileInfo(target).isExecutable() || !executable.open(QIODevice::ReadOnly)
        || executable.read(20).left(4)!=QByteArray("\x7f""ELF",4)){
        fail(tr("Choose a native Linux ELF executable on the selected disc. Installers and scripts are not run."));return {};
    }
    const QVariantMap registration{{"schemaVersion",1},{"title",title},{"platform","PC Optical"},
        {"runtimeId",runtimeId},{"root",root},{"target",relativeExecutable}};
    QSqlQuery related(m_db);
    related.prepare("SELECT id FROM games WHERE title=? AND instr(root,?)>0");
    related.addBindValue(title);
    related.addBindValue("/"+disc.value("mediaSetId").toString()+"/");
    if(!related.exec()){fail(related.lastError().text());return {};}
    if(related.next()){
        const auto gameId=related.value(0).toString();related.finish();
        return addEdition(gameId,registration).isEmpty()?QString():gameId;
    }
    return addGame(registration);
}
bool GameLibrary::approveOpticalMedia(const QString &observationId,const QString &device){
    if(!m_ready)return fail(tr("Games is unavailable."));
    const auto media=revalidateOpticalDisc(observationId,device);
    const auto disc=media.value("disc").toMap();
    if(media.value("state")!="ready" || !disc.value("directPlaySupported").toBool())
        return fail(tr("Only a currently mounted ISO9660 disc with a readable identity can be approved."));
    const auto error=OpticalDiscGrants::authorize(m_emulatorProfile,disc.value("mediaId").toString());
    if(!error.isEmpty())return fail(error);
    m_error.clear();emit errorChanged();return true;
}
bool GameLibrary::revokeOpticalMedia(const QString &observationId,const QString &device){
    if(!m_ready)return fail(tr("Games is unavailable."));
    const auto media=revalidateOpticalDisc(observationId,device);
    const auto disc=media.value("disc").toMap();
    if(media.value("state")!="ready")return fail(tr("The disc changed. Refresh optical details before revoking its access."));
    const auto error=OpticalDiscGrants::revoke(m_emulatorProfile,disc.value("mediaId").toString());
    if(!error.isEmpty())return fail(error);
    m_error.clear();emit errorChanged();return true;
}
bool GameLibrary::mountOpticalDisc(const QString &device){
    if(!m_ready)return fail(tr("Games is unavailable."));
    QString error;
    if(!OpticalDiscAdapter::mountReadOnly(device,&error))return fail(error);
    m_error.clear();emit errorChanged();return true;
}
bool GameLibrary::ejectOpticalDisc(const QString &device){
    if(!m_ready)return fail(tr("Games is unavailable."));
    if(m_sessionActiveGuard && m_sessionActiveGuard())
        return fail(tr("A session is active. Return Home before safely ejecting optical media."));
    if(portableInstallBusy() && m_portableOpticalDevice==device)
        return fail(tr("The disc is being used by the game installer. Finish or cancel it before ejecting."));
    QString error;
    if(!OpticalDiscAdapter::eject(device,&error))return fail(error);
    m_error.clear();emit errorChanged();return true;
}
bool GameLibrary::beginPortableInstall(const QString &source,const QString &runtimeId,
                                       const QString &opticalObservationId,const QString &opticalDevice){
    if(!m_ready || portableInstallBusy() || !m_portablePhase.isEmpty())return fail(tr("Finish or cancel the current game installation first."));
    if(!compatibleRuntime(runtimeId,true))return fail(tr("Choose an installed Linux game runtime."));
    const QFileInfo file(source);
    if(opticalObservationId.isEmpty()!=opticalDevice.isEmpty())
        return fail(tr("Optical media identity is incomplete."));
    if(!file.isFile() || file.isSymLink() || file.canonicalFilePath()!=file.absoluteFilePath()
        || file.size()<1 || file.size()>512LL*1024*1024
        || (opticalObservationId.isEmpty() && !MediaGrants::validate(file.absolutePath()).isEmpty()))
        return fail(tr("Choose a regular ZIP, USTAR or gzip-compressed USTAR bundle inside an approved media folder."));
    if(!opticalObservationId.isEmpty()){
        const auto media=revalidateOpticalDisc(opticalObservationId,opticalDevice);
        const auto disc=media.value("disc").toMap();
        if(media.value("state")!="ready" || !file.canonicalFilePath().startsWith(disc.value("mountRoot").toString()+'/'))
            return fail(tr("The selected optical disc changed. Refresh disc details and choose its package again."));
    }
    auto helper=QCoreApplication::applicationDirPath()+"/share/isolde/portable-game-installer.py";
    if(!QFileInfo(helper).isReadable())helper=QCoreApplication::applicationDirPath()+"/../share/isolde/portable-game-installer.py";
    const auto python=QStandardPaths::findExecutable("python3");
    if(python.isEmpty() || !QFileInfo(helper).isReadable())return fail(tr("Repair the Isolde portable-game installer."));
    m_portableSource=file.canonicalFilePath();m_portableRuntime=runtimeId;
    m_portableOpticalObservationId=opticalObservationId;m_portableOpticalDevice=opticalDevice;
    m_portableDestination=ManagedGamePaths::gamesRoot(m_emulatorProfile)+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"/install";
    m_portableInstallOutput.clear();m_portableInstallError.clear();m_portableInstallPreview.clear();
    m_portableCanceled=false;m_portablePhase="inspect";m_portableInstallStatus="Checking bundle contents; nothing will be executed or installed during preflight.";
    m_portableInstall.start(python,{helper,"--inspect","--source",m_portableSource});
    m_portableInstallDeadline.start();m_error.clear();emit errorChanged();emit portableInstallChanged();return true;
}
bool GameLibrary::confirmPortableInstall(){
    if(!m_ready || m_portablePhase!="review" || portableInstallBusy())return fail(tr("No portable game package is awaiting confirmation."));
    if(!compatibleRuntime(m_portableRuntime,true)
        || !QFileInfo(m_portableSource).isFile()
        || QFileInfo(m_portableSource).canonicalFilePath()!=m_portableSource
        || (m_portableOpticalObservationId.isEmpty()
            && !MediaGrants::validate(QFileInfo(m_portableSource).absolutePath()).isEmpty()))
        return fail(tr("The package source or selected runtime changed. Review preflight again."));
    if(!m_portableOpticalObservationId.isEmpty()){
        const auto media=revalidateOpticalDisc(m_portableOpticalObservationId,m_portableOpticalDevice);
        const auto disc=media.value("disc").toMap();
        if(media.value("state")!="ready" || !m_portableSource.startsWith(disc.value("mountRoot").toString()+'/'))
            return fail(tr("The selected optical disc changed or was removed. Refresh its details before installing."));
    }
    const auto parent=QFileInfo(m_portableDestination).absolutePath();
    const auto gamesRoot=ManagedGamePaths::gamesRoot(m_emulatorProfile);
    if(gamesRoot.isEmpty() || !QDir().mkpath(parent)
        || QFileInfo(gamesRoot).canonicalFilePath()!=gamesRoot
        || QFileInfo(parent).canonicalFilePath()!=parent
        || QFileInfo(parent).isSymLink()
        || QFileInfo::exists(m_portableDestination)
        || QFileInfo::exists(m_portableDestination+".staging")){
        return fail(tr("Cannot prepare a new private game destination; existing files were preserved."));
    }
    if(!QFile::setPermissions(gamesRoot,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)
        || !QFile::setPermissions(parent,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))
        return fail(tr("Cannot secure the private game install destination."));
    auto helper=QCoreApplication::applicationDirPath()+"/share/isolde/portable-game-installer.py";
    if(!QFileInfo(helper).isReadable())helper=QCoreApplication::applicationDirPath()+"/../share/isolde/portable-game-installer.py";
    const auto python=QStandardPaths::findExecutable("python3");
    if(python.isEmpty() || !QFileInfo(helper).isReadable())return fail(tr("Repair the Isolde portable-game installer."));
    m_portableInstallOutput.clear();m_portableInstallError.clear();m_portableCanceled=false;
    m_portablePhase="install";m_portableInstallStatus="Installing into a new private profile folder. Existing installs and saves will not be overwritten.";
    m_portableInstall.start(python,{helper,"--install","--source",m_portableSource,"--destination",m_portableDestination,
        "--expected-sha256",m_portableInstallPreview.value("archiveSha256").toString()});
    m_portableInstallDeadline.start();emit portableInstallChanged();return true;
}
void GameLibrary::cancelPortableInstall(){
    m_portableInstallDeadline.stop();
    if(m_portablePhase.isEmpty())return;
    const auto phase=m_portablePhase;
    m_portableCanceled=true;
    if(m_portableInstall.state()!=QProcess::NotRunning){
        m_portableInstall.terminate();
        if(!m_portableInstall.waitForFinished(2000)){m_portableInstall.kill();m_portableInstall.waitForFinished(2000);}
    }
    const auto stage=m_portableDestination+".staging";
    if(m_portablePhase=="install" && QFileInfo(stage).isDir() && QFileInfo(stage).canonicalFilePath()==stage)QDir(stage).removeRecursively();
    const auto snapshot=m_portableDestination+".staging-source";
    if(m_portablePhase=="install" && QFileInfo(snapshot).isFile() && !QFileInfo(snapshot).isSymLink())QFile::remove(snapshot);
    m_portablePhase.clear();m_portableInstallPreview.clear();m_portableGameId.clear();
    m_portableInstallStatus=phase=="uninstall"
        ?"Uninstall canceled. The catalog entry was preserved; inspect the managed folder before retrying."
        :"Installation canceled. Existing installations and saves were preserved.";
    emit portableInstallChanged();
}
bool GameLibrary::beginImport(const QString &root,const QString &runtime,const QString &platform) {
    if(!m_ready || scanning() || m_importReady)return fail(tr("Finish or cancel the current import first."));
    const auto error=MediaGrants::validate(root);if(!error.isEmpty())return fail(error);
    if(!compatibleRuntime(runtime,true) || platform.trimmed().isEmpty() || platform.size()>80)return fail(tr("Choose a compatible runtime and platform."));
    const auto formats=m_apps->find(runtime)->launchConfiguration.value("gameFormats").toList();
    if(formats.isEmpty() || formats.size()>32)return fail(tr("This runtime has no verified import formats. Add individual games instead."));
    QStringList extensions;
    for(const auto &format:formats){
        const auto text=format.toString();
        if(format.metaType().id()!=QMetaType::QString || !QRegularExpression("^[a-z0-9]{1,8}$").match(text).hasMatch())return fail(tr("Invalid runtime import format."));
        extensions.append(text);
    }
    m_importRoot=root;m_importRuntime=runtime;m_importPlatform=platform;m_importFormats=extensions;
    m_preview.clear();m_scanEntries=0;m_error.clear();emit errorChanged();
    m_scan=std::make_unique<QDirIterator>(root,QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDirIterator::Subdirectories);
    m_scanTimer.start();emit importChanged();return true;
}
void GameLibrary::scanStep() {
    if(!m_ready || !m_scan)return;
    // Yield to input between bounded batches. No file contents are read.
    for(int batch=0;batch<32 && m_scan->hasNext();++batch){
        const auto path=m_scan->next();const auto info=m_scan->fileInfo();
        if(++m_scanEntries>10000){cancelImport();fail(tr("This folder exceeds the 10,000-entry import limit. Choose a smaller folder; nothing was imported."));return;}
        if(!info.isFile() || info.isSymLink() || info.canonicalFilePath()!=path || !path.startsWith(m_importRoot+'/') || !info.isReadable())continue;
        if(!m_importFormats.contains(info.suffix().toLower()))continue;
        const auto title=info.completeBaseName();
        if(title.trimmed()!=title || title.isEmpty() || title.size()>256){cancelImport();fail(tr("An import title is invalid. Add it manually with a corrected title."));return;}
        m_preview.append(QVariantMap{{"schemaVersion",1},{"title",title},{"platform",m_importPlatform},
            {"runtimeId",m_importRuntime},{"root",m_importRoot},{"target",QDir(m_importRoot).relativeFilePath(path)}});
    }
    if(!m_scan->hasNext()){m_scanTimer.stop();m_scan.reset();m_importReady=true;}
    emit importChanged();
}
QVariantList GameLibrary::importPage(int offset) const {
    if(offset<0 || offset%PageSize || offset>=10000)return {};
    return m_preview.mid(offset,PageSize);
}
bool GameLibrary::commitImport() {
    if(m_portablePhase=="uninstall")return fail(tr("Wait for the managed uninstall to finish before importing games."));
    if(!m_ready || scanning() || !m_importReady)return fail(tr("Wait for the import preview before confirming."));
    const bool steam=m_importRuntime=="steam";
    if(!compatibleRuntime(m_importRuntime,!steam))return fail(tr("The selected runtime is no longer installed."));
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    if(!query.exec("SELECT count(*) FROM games") || !query.next()){m_db.rollback();return fail(tr("Cannot read the games catalog."));}
    int count=query.value(0).toInt();query.finish();
    bool checkedPlatform=false;
    for(const auto &entry:m_preview){
        const auto value=entry.toMap();QString error;
        if(ComponentSchema::gameRegistration(value,&error).isEmpty() || (!steam && checkedTarget(m_importRoot,value.value("target").toString()).isEmpty())){
            m_db.rollback();return fail(error.isEmpty()?tr("A source changed after preview. Nothing was imported; review the folder again."):error);
        }
        query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?"+QString(m_hasEditions?" UNION ALL SELECT id FROM game_editions WHERE runtime=? AND root=? AND target=?":""));
        query.addBindValue(m_importRuntime);query.addBindValue(m_importRoot);query.addBindValue(value.value("target"));
        if(m_hasEditions){query.addBindValue(m_importRuntime);query.addBindValue(m_importRoot);query.addBindValue(value.value("target"));}
        if(!query.exec()){m_db.rollback();return fail(query.lastError().text());}
        const bool existing=query.next();query.finish();if(existing)continue; // Preserve manual corrections.
        if(!checkedPlatform){
            checkedPlatform=true;
            if(!platformAvailable(value.value("platform").toString())){m_db.rollback();return fail(tr("The Games catalog supports up to 128 distinct platforms. Nothing was imported."));}
        }
        if(++count>MaxGames){m_db.rollback();return fail(tr("Import would exceed the catalog limit. Nothing was imported."));}
        query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,added) VALUES(?,?,?,?,?,?,unixepoch())");
        query.addBindValue("game-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
        for(const auto &key:QStringList{"title","platform","runtimeId","root","target"})query.addBindValue(value.value(key));
        if(!query.exec()){const auto reason=query.lastError().text();m_db.rollback();return fail(reason);}
    }
    if(!m_db.commit()){m_db.rollback();return fail(tr("Import could not be saved. Previous data was preserved."));}
    cancelImport();m_error.clear();emit errorChanged();emit changed();return true;
}
