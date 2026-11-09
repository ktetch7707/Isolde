#include "GameLibrary.h"
#include "GameEmulators.h"
#include <QCoreApplication>
#include "app/AppRegistry.h"
#include "app/AppCatalog.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
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
bool exactKeys(const QJsonObject &object,const QStringList &keys) {
    QStringList actual=object.keys();auto expected=keys;actual.sort();expected.sort();return actual==expected;
}
bool archiveInteger(const QJsonValue &value,qint64 *result) {
    if(!value.isDouble() || !std::isfinite(value.toDouble()) || std::floor(value.toDouble())!=value.toDouble()
        || value.toDouble()<0 || value.toDouble()>9007199254740991.0)return false;
    *result=qint64(value.toDouble());return true;
}
QVariantMap row(const QSqlQuery &query) {
    QStringList tags;
    const auto tagArray=QJsonDocument::fromJson(query.value(6).toByteArray()).array();
    for(const auto &tag:tagArray)if(tag.isString())tags.append(tag.toString());
    return {{"id",query.value(0)}, {"title",query.value(1)}, {"platform",query.value(2)},
        {"runtimeId",query.value(3)}, {"favorite",query.value(4).toBool()},
        {"mediaType","game"}, {"mediaId",query.value(0)},
        {"franchise",query.value(5).toString()},
        {"tags",tags}};
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
    if (exists && version != 1 && version != 2 && version != 3 && version != 4) { fail(tr("Unsupported games catalog version. Original data was preserved.")); return; }
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
    if(version==3 || version==4){
        if(!query.exec("SELECT game_id,last_played FROM game_activity LIMIT 0")){fail(tr("Invalid game activity catalog. Original data was preserved."));return;}
        m_hasActivity=true;
        if(!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='game_editions'")){fail(tr("Cannot inspect the editions catalog."));return;}
        if(query.next()){
            if(!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions LIMIT 0")){fail(tr("Invalid edition catalog. Original data was preserved."));return;}
            m_hasEditions=true;
        }
    }
    if(version==4){
        if(!query.exec("SELECT game_id,franchise,tags FROM game_metadata LIMIT 0")
            || !query.exec("SELECT id,name,kind,filter_type,filter_value FROM game_collections LIMIT 0")
            || !query.exec("SELECT collection_id,game_id,position FROM game_collection_members LIMIT 0")){
            fail(tr("Invalid Games organization catalog. Original data was preserved."));return;
        }
        m_hasMetadata=true;m_hasCollections=true;
    }
    m_ready = true;
}
GameLibrary::~GameLibrary() {
    cancelImport();if(m_steamScan.state()!=QProcess::NotRunning)m_steamScan.waitForFinished(1000);
    cancelEmulatorSetup();if(setupBusy())m_setup.waitForFinished(5000);
    m_db.close(); m_db = QSqlDatabase(); QSqlDatabase::removeDatabase(m_connection);
}
bool GameLibrary::fail(const QString &error) { m_error = error; emit errorChanged(); return false; }
void GameLibrary::invalidate() { cancelImport();cancelEmulatorSetup(); m_ready = false; m_db.close(); fail(tr("This games profile is no longer active.")); emit changed(); }
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
    const auto grantError = MediaGrants::validate(root);
    if (!grantError.isEmpty()) { fail(grantError); return {}; }
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.contains('\\') || relative.contains(QChar::Null)
        || relative.size()>4096 || QDir::cleanPath(relative)!=relative || relative.split('/').contains("..")) {
        fail(tr("Choose a file inside the approved game folder.")); return {};
    }
    const auto path = QDir(root).filePath(relative); const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable() || info.canonicalFilePath()!=path || !path.startsWith(root+'/')) {
        fail(tr("The game file is missing, unreadable or redirected. Reconnect its source and retry.")); return {};
    }
    return path;
}
QString GameLibrary::addGame(const QVariantMap &input) {
    if (!m_ready) { fail(tr("The games catalog is unavailable.")); return {}; }
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
        +" FROM games g";
    if(m_hasActivity)sql+=" LEFT JOIN game_activity a ON a.game_id=g.id";
    if(m_hasMetadata)sql+=" LEFT JOIN game_metadata m ON m.game_id=g.id";
    sql+=" WHERE (?=0 OR g.favorite=1) AND (?='' OR lower(g.platform)=lower(?)";
    if(m_hasEditions)sql+=" OR EXISTS(SELECT 1 FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(?))";
    sql+=") AND (instr(lower(g.title),lower(?))>0";
    if(m_hasMetadata)sql+=" OR instr(lower(m.franchise),lower(?))>0 OR instr(lower(m.tags),lower(?))>0";
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
    query.addBindValue(offset);
    if (!query.exec()) { fail(query.lastError().text()); return {}; }
    QVariantList result; while (query.next()) result.append(row(query)); return result;
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
        +" FROM games g"+QString(m_hasMetadata?" LEFT JOIN game_metadata m ON m.game_id=g.id":"")+" WHERE g.id=?");
    query.addBindValue(id);
    if (!query.exec() || !query.next()) return {};
    return row(query);
}
bool GameLibrary::setOrganization(const QString &id,const QString &franchise,const QStringList &inputTags) {
    if(!m_ready || details(id).isEmpty())return fail(tr("Game not found in this profile."));
    if(franchise.size()>160 || franchise.contains(QChar::Null) || franchise!=franchise.trimmed()
        || inputTags.size()>MaxTags)return fail(tr("Use a franchise name up to 160 characters and at most 32 tags."));
    QSet<QString> seen;
    QJsonArray tags;
    for(const auto &tag:inputTags){
        const auto normalized=tag.trimmed();
        if(normalized.isEmpty() || normalized.size()>64 || normalized.contains(QChar::Null) || normalized.contains(',')
            || normalized!=tag || seen.contains(normalized.toCaseFolded()))
            return fail(tr("Tags must be distinct, trimmed names of up to 64 characters."));
        seen.insert(normalized.toCaseFolded());tags.append(normalized);
    }
    if(!ensureOrganizationSchema())return false;
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO game_metadata(game_id,franchise,tags) VALUES(?,?,?) ON CONFLICT(game_id) DO UPDATE SET franchise=excluded.franchise,tags=excluded.tags");
    query.addBindValue(id);query.addBindValue(franchise);query.addBindValue(QJsonDocument(tags).toJson(QJsonDocument::Compact));
    if(!query.exec())return fail(tr("Could not save game organization. Previous metadata was preserved. %1").arg(query.lastError().text()));
    m_error.clear();emit errorChanged();emit changed();return true;
}
QVariantList GameLibrary::collections() {
    QVariantList result;
    if(!m_ready)return result;
    if(!m_hasCollections)return result;
    QSqlQuery query(m_db);
    if(!query.exec("SELECT c.id,c.name,c.kind,c.filter_type,c.filter_value,count(m.game_id) FROM game_collections c LEFT JOIN game_collection_members m ON m.collection_id=c.id GROUP BY c.id ORDER BY c.name COLLATE NOCASE,c.id")){
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
    QString sql="SELECT g.id,g.title,g.platform,g.runtime,g.favorite,COALESCE(m.franchise,''),COALESCE(m.tags,'[]') FROM games g LEFT JOIN game_metadata m ON m.game_id=g.id";
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
    QVariantList result;while(query.next())result.append(row(query));return result;
}
bool GameLibrary::exportCatalog(const QString &destination) {
    if(!m_ready || !QDir::isAbsolutePath(destination) || QDir::cleanPath(destination)!=destination
        || !destination.endsWith(".json",Qt::CaseInsensitive) || QFileInfo(destination).isSymLink())
        return fail(tr("Choose a JSON file in a specifically approved media folder."));
    const auto parent=QFileInfo(destination).absolutePath();
    const auto grant=MediaGrants::validate(parent);
    if(!grant.isEmpty())return fail(grant);
    if(QFileInfo::exists(destination) && (!QFileInfo(destination).isFile() || QFileInfo(destination).canonicalFilePath()!=destination))
        return fail(tr("The catalog export target is not a regular file. Nothing was replaced."));
    QJsonObject archive{{"schemaVersion",1}};
    QJsonArray games,editionsArray,collectionArray;
    QSqlQuery query(m_db);
    const QString gameSql="SELECT g.id,g.title,g.platform,g.runtime,g.root,g.target,g.favorite,g.added,"
        +(m_hasMetadata?QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')"):QString("'','[]'"))
        +(m_hasActivity?QString(",COALESCE(a.last_played,0)"):QString(",0"))
        +" FROM games g"
        +(m_hasMetadata?QString(" LEFT JOIN game_metadata m ON m.game_id=g.id"):QString())
        +(m_hasActivity?QString(" LEFT JOIN game_activity a ON a.game_id=g.id"):QString())
        +" ORDER BY g.id";
    if(!query.exec(gameSql))return fail(tr("Cannot read the games catalog for export."));
    while(query.next()){
        if(games.size()>=MaxGames)return fail(tr("Games catalog exceeds the export bound."));
        QJsonArray tags;
        const auto tagDocument=QJsonDocument::fromJson(query.value(9).toByteArray());
        if(!tagDocument.isArray())return fail(tr("Game organization metadata is corrupt. Export was canceled."));
        const auto tagList=tagDocument.array();
        for(const auto &tag:tagList)tags.append(tag);
        games.append(QJsonObject{{"id",query.value(0).toString()},{"title",query.value(1).toString()},
            {"platform",query.value(2).toString()},{"runtimeId",query.value(3).toString()},
            {"root",query.value(4).toString()},{"target",query.value(5).toString()},
            {"favorite",query.value(6).toBool()},{"added",query.value(7).toLongLong()},
            {"franchise",query.value(8).toString()},{"tags",tags},{"lastPlayed",query.value(10).toLongLong()}});
    }
    query.finish();
    if(m_hasEditions){
        if(!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions ORDER BY id"))
            return fail(tr("Cannot read game editions for export."));
        while(query.next())editionsArray.append(QJsonObject{{"id",query.value(0).toString()},{"gameId",query.value(1).toString()},
            {"title",query.value(2).toString()},{"platform",query.value(3).toString()},{"runtimeId",query.value(4).toString()},
            {"root",query.value(5).toString()},{"target",query.value(6).toString()}});
        query.finish();
    }
    if(m_hasCollections){
        if(!query.exec("SELECT id,name,kind,filter_type,filter_value FROM game_collections ORDER BY id"))
            return fail(tr("Cannot read game collections for export."));
        while(query.next()){
            const auto collectionId=query.value(0).toString();
            QJsonArray members;
            QSqlQuery memberQuery(m_db);memberQuery.prepare("SELECT game_id FROM game_collection_members WHERE collection_id=? ORDER BY position,game_id");memberQuery.addBindValue(collectionId);
            if(!memberQuery.exec())return fail(tr("Cannot read collection membership for export."));
            while(memberQuery.next())members.append(memberQuery.value(0).toString());
            collectionArray.append(QJsonObject{{"id",collectionId},{"name",query.value(1).toString()},
                {"kind",query.value(2).toString()},{"filterType",query.value(3).toString()},
                {"filterValue",query.value(4).toString()},{"members",members}});
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
    if(!m_ready || !confirmed || !QDir::isAbsolutePath(source) || QDir::cleanPath(source)!=source
        || QFileInfo(source).isSymLink() || !QFileInfo(source).isFile() || QFileInfo(source).canonicalFilePath()!=source
        || QFileInfo(source).size()<1 || QFileInfo(source).size()>MaxCatalogArchiveBytes)
        return fail(tr("Review and confirm a readable Games JSON backup from an approved folder."));
    const auto grant=MediaGrants::validate(QFileInfo(source).absolutePath());
    if(!grant.isEmpty())return fail(grant);
    QFile file(source);if(!file.open(QIODevice::ReadOnly))return fail(tr("Cannot read the selected Games backup."));
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);
    if(parse.error!=QJsonParseError::NoError || !document.isObject())return fail(tr("Invalid Games backup JSON."));
    const auto archive=document.object();
    if(!exactKeys(archive,{"schemaVersion","games","editions","collections"})
        || !archive.value("schemaVersion").isDouble() || archive.value("schemaVersion").toDouble()!=1
        || !archive.value("games").isArray() || archive.value("games").toArray().size()>MaxGames
        || !archive.value("editions").isArray() || archive.value("editions").toArray().size()>MaxGames
        || !archive.value("collections").isArray() || archive.value("collections").toArray().size()>MaxCollections)
        return fail(tr("Unsupported Games backup version or catalog bounds."));

    QList<QVariantMap> games,editionsData,collectionsData;
    QSet<QString> ids,gameIds,platforms,names,installations;
    QMap<QString,int> editionCounts;
    for(const auto &entry:archive.value("games").toArray()){
        if(!entry.isObject())return fail(tr("Invalid game record in backup."));
        const auto object=entry.toObject();
        if(!exactKeys(object,{"id","title","platform","runtimeId","root","target","favorite","added","franchise","tags","lastPlayed"})
            || !object.value("id").isString() || !ComponentSchema::safeId(object.value("id").toString())
            || ids.contains(object.value("id").toString()) || !object.value("favorite").isBool()
            || !object.value("franchise").isString() || object.value("franchise").toString().size()>160
            || object.value("franchise").toString().contains(QChar::Null)
            || !object.value("tags").isArray() || object.value("tags").toArray().size()>MaxTags)
            return fail(tr("Invalid or duplicate game identity or organization metadata in backup."));
        const auto gameId=object.value("id").toString(),platform=object.value("platform").toString();
        QVariantMap registration{{"schemaVersion",1},{"title",object.value("title").toString()},
            {"platform",platform},{"runtimeId",object.value("runtimeId").toString()},
            {"root",object.value("root").toString()},{"target",object.value("target").toString()}};
        QString schemaError;if(ComponentSchema::gameRegistration(registration,&schemaError).isEmpty())return fail(schemaError);
        const auto root=registration.value("root").toString();
        if(!root.isEmpty() && !MediaGrants::validate(root,false).isEmpty())return fail(tr("Backup refers to a game folder not authorized in this profile."));
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
        qint64 added=0,lastPlayed=0;
        if(!archiveInteger(object.value("added"),&added) || !archiveInteger(object.value("lastPlayed"),&lastPlayed))
            return fail(tr("Invalid game activity timestamp in backup."));
        auto value=registration;value["id"]=gameId;value["favorite"]=object.value("favorite").toBool();
        value["added"]=added;value["franchise"]=object.value("franchise").toString();
        QVariantList tagValues;for(const auto &tag:object.value("tags").toArray())tagValues.append(tag.toString());
        value["tags"]=tagValues;value["lastPlayed"]=lastPlayed;
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
        if(!root.isEmpty() && !MediaGrants::validate(root,false).isEmpty())return fail(tr("Backup edition refers to a game folder not authorized in this profile."));
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
    if(!ensureOrganizationSchema())return false;
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
        query.prepare("INSERT INTO game_metadata(game_id,franchise,tags) VALUES(?,?,?)");
        query.addBindValue(game.value("id"));query.addBindValue(game.value("franchise"));
        QJsonArray tags;for(const auto &tag:game.value("tags").toStringList())tags.append(tag);
        query.addBindValue(QJsonDocument(tags).toJson(QJsonDocument::Compact));ok=ok && query.exec();
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
    ok=ok && query.exec("PRAGMA user_version=4");
    if(!ok || !m_db.commit()){
        const auto reason=query.lastError().text();m_db.rollback();
        return fail(tr("Restore failed; the prior catalog remains intact. Recovery copy: %1. %2").arg(backupPath,reason));
    }
    m_hasActivity=true;m_hasMetadata=true;m_hasCollections=true;m_hasEditions=m_hasEditions || needEditionTable;
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
    QSqlQuery query(m_db); query.prepare("DELETE FROM games WHERE id=?"); query.addBindValue(id);
    if (!query.exec()) return fail(query.lastError().text());
    // Catalog-only operation: deliberately never deletes game/runtime/save files.
    m_error.clear(); emit changed(); return true;
}
QVariantMap GameLibrary::resolveLaunch(const QString &id,const QString &editionId) {
    m_error.clear();emit errorChanged();
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
    return {{"type","open-file"},{"openWith",runtime},{"target",target},{"sourceRoot",root}};
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
    m_preview.clear();m_steamScanOutput.clear();m_steamScanCanceled=false;m_importRuntime="steam";m_importRoot=QStringLiteral("");m_importReady=false;
    m_error.clear();emit errorChanged();m_importStatus=tr("Reading installed games in this profile's Steam library. Nothing is saved yet.");
    m_steamScan.start(python,{helper,"--discover","--home",home});m_steamScanDeadline.start();emit importChanged();return true;
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
