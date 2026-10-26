#include "GameLibrary.h"
#include "app/AppRegistry.h"
#include "home/ComponentSchema.h"
#include "media/MediaGrants.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QRegularExpression>

namespace {
constexpr int MaxGames = 50000;
constexpr int PageSize = 50;
QVariantMap row(const QSqlQuery &query) {
    return {{"id",query.value(0)}, {"title",query.value(1)}, {"platform",query.value(2)},
        {"runtimeId",query.value(3)}, {"favorite",query.value(4).toBool()},
        {"mediaType","game"}, {"mediaId",query.value(0)}};
}
}

GameLibrary::GameLibrary(const QString &path, AppRegistry *apps, QObject *parent)
    : QObject(parent), m_connection("games-" + QUuid::createUuid().toString(QUuid::WithoutBraces)), m_apps(apps) {
    connect(apps,&AppRegistry::appsChanged,this,&GameLibrary::changed);
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
    if (exists && version != 1) { fail(tr("Unsupported games catalog version. Original data was preserved.")); return; }
    if (!query.exec("PRAGMA foreign_keys=ON") || !query.exec("PRAGMA synchronous=FULL")) { fail(query.lastError().text()); return; }
    if (!exists) {
        if (!m_db.transaction()) { fail(m_db.lastError().text()); return; }
        const bool created = query.exec("CREATE TABLE games (id TEXT PRIMARY KEY, title TEXT NOT NULL, platform TEXT NOT NULL, runtime TEXT NOT NULL, root TEXT NOT NULL, target TEXT NOT NULL, favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)), added INTEGER NOT NULL, UNIQUE(runtime,root,target))")
            && query.exec("CREATE INDEX games_title ON games(title COLLATE NOCASE)")
            && query.exec("PRAGMA user_version=1");
        if (!created || !m_db.commit()) { const auto error = query.lastError().text(); m_db.rollback(); fail(tr("Cannot initialize the games catalog: %1").arg(error)); return; }
    }
    if (!query.exec("SELECT id,title,platform,runtime,root,target,favorite,added FROM games LIMIT 0")) { fail(tr("Invalid games catalog. Original data was preserved.")); return; }
    m_ready = true;
}
GameLibrary::~GameLibrary() {
    m_db.close(); m_db = QSqlDatabase(); QSqlDatabase::removeDatabase(m_connection);
}
bool GameLibrary::fail(const QString &error) { m_error = error; emit errorChanged(); return false; }
void GameLibrary::invalidate() { cancelImport(); m_ready = false; m_db.close(); fail(tr("This games profile is no longer active.")); emit changed(); }
bool GameLibrary::compatibleRuntime(const QString &id, bool file) const {
    const auto *app = m_apps->find(id);
    if (!app || app->type != "desktop") return false;
    if (file) return QStringList{"game","game-hub"}.contains(app->presentationMode)
        && app->handledActions.contains("open-file") && app->launchConfiguration.value("actionHandlers").toMap().contains("open-file");
    return app->presentationMode == "game";
}
QVariantList GameLibrary::runtimes() const {
    QVariantList result;
    for (const auto &entry : m_apps->apps()) {
        const auto app = entry.toMap(); const auto id = app.value("id").toString();
        if (compatibleRuntime(id,true) || compatibleRuntime(id,false))
            result.append(QVariantMap{{"id",id},{"name",app.value("name")},{"file",compatibleRuntime(id,true)}});
    }
    return result;
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
    if (!compatibleRuntime(runtime,!root.isEmpty())) { fail(tr("Choose an installed compatible game runtime.")); return {}; }
    if (!root.isEmpty() && checkedTarget(root,target).isEmpty()) return {};
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
QVariantList GameLibrary::page(const QString &view, const QString &search, int offset) {
    if (!m_ready) return {};
    if (!QStringList{"all","recent","favorites","search"}.contains(view) || search.size()>160 || offset<0 || offset>=MaxGames || offset%PageSize) {
        fail(tr("Invalid games query.")); return {};
    }
    QSqlQuery query(m_db);
    // instr uses literal text, not LIKE wildcards or executable SQL predicates.
    query.prepare("SELECT id,title,platform,runtime,favorite FROM games WHERE (?=0 OR favorite=1) AND instr(lower(title),lower(?))>0 ORDER BY "
        + QString(view=="recent"?"added DESC,id":"title COLLATE NOCASE,id") + " LIMIT 50 OFFSET ?");
    query.addBindValue(view=="favorites"?1:0); query.addBindValue(search.isNull()?QStringLiteral(""):search); query.addBindValue(offset);
    if (!query.exec()) { fail(query.lastError().text()); return {}; }
    QVariantList result; while (query.next()) result.append(row(query)); return result;
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
    if(!compatibleRuntime(runtime,!root.isEmpty()))return fail(tr("Choose an installed compatible game runtime."));
    if(!root.isEmpty() && checkedTarget(root,target).isEmpty())return false;
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
    QSqlQuery query(m_db); query.prepare("SELECT id,title,platform,runtime,favorite FROM games WHERE id=?"); query.addBindValue(id);
    if (!query.exec() || !query.next()) return {};
    return row(query);
}
bool GameLibrary::setFavorite(const QString &id, bool favorite) {
    if (!m_ready || details(id).isEmpty()) return fail(tr("Game not found in this profile."));
    QSqlQuery query(m_db); query.prepare("UPDATE games SET favorite=? WHERE id=?"); query.addBindValue(favorite?1:0); query.addBindValue(id);
    if (!query.exec()) return fail(query.lastError().text());
    m_error.clear(); emit changed(); return true;
}
bool GameLibrary::removeGame(const QString &id) {
    if (!m_ready || details(id).isEmpty()) return fail(tr("Game not found in this profile."));
    QSqlQuery query(m_db); query.prepare("DELETE FROM games WHERE id=?"); query.addBindValue(id);
    if (!query.exec()) return fail(query.lastError().text());
    // Catalog-only operation: deliberately never deletes game/runtime/save files.
    m_error.clear(); emit changed(); return true;
}
QVariantMap GameLibrary::resolveLaunch(const QString &id) {
    if (!m_ready || !ComponentSchema::safeId(id)) { fail(tr("Game not found in this profile.")); return {}; }
    QSqlQuery query(m_db); query.prepare("SELECT runtime,root,target FROM games WHERE id=?"); query.addBindValue(id);
    if (!query.exec() || !query.next()) { fail(tr("Game not found in this profile.")); return {}; }
    const auto runtime=query.value(0).toString(), root=query.value(1).toString(), relative=query.value(2).toString();
    if (!compatibleRuntime(runtime,!root.isEmpty())) { fail(tr("The game runtime is missing or incompatible. Repair it and retry.")); return {}; }
    if (root.isEmpty()) return {{"type","launch-app"},{"appId",runtime}};
    const auto target=checkedTarget(root,relative); if (target.isEmpty()) return {};
    return {{"type","open-file"},{"openWith",runtime},{"target",target}};
}
void GameLibrary::cancelImport() {
    m_scanTimer.stop();m_scan.reset();m_preview.clear();m_importReady=false;
    m_importRoot.clear();m_importRuntime.clear();m_importPlatform.clear();m_importFormats.clear();
    m_scanEntries=0;emit importChanged();
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
    if(!compatibleRuntime(m_importRuntime,true))return fail(tr("The selected runtime is no longer installed."));
    if(!m_db.transaction())return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    if(!query.exec("SELECT count(*) FROM games") || !query.next()){m_db.rollback();return fail(tr("Cannot read the games catalog."));}
    int count=query.value(0).toInt();query.finish();
    for(const auto &entry:m_preview){
        const auto value=entry.toMap();QString error;
        if(ComponentSchema::gameRegistration(value,&error).isEmpty() || checkedTarget(m_importRoot,value.value("target").toString()).isEmpty()){
            m_db.rollback();return fail(error.isEmpty()?tr("A source changed after preview. Nothing was imported; review the folder again."):error);
        }
        query.prepare("SELECT id FROM games WHERE runtime=? AND root=? AND target=?");
        query.addBindValue(m_importRuntime);query.addBindValue(m_importRoot);query.addBindValue(value.value("target"));
        if(!query.exec()){m_db.rollback();return fail(query.lastError().text());}
        const bool existing=query.next();query.finish();if(existing)continue; // Preserve manual corrections.
        if(++count>MaxGames){m_db.rollback();return fail(tr("Import would exceed the catalog limit. Nothing was imported."));}
        query.prepare("INSERT INTO games(id,title,platform,runtime,root,target,added) VALUES(?,?,?,?,?,?,unixepoch())");
        query.addBindValue("game-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
        for(const auto &key:QStringList{"title","platform","runtimeId","root","target"})query.addBindValue(value.value(key));
        if(!query.exec()){const auto reason=query.lastError().text();m_db.rollback();return fail(reason);}
    }
    if(!m_db.commit()){m_db.rollback();return fail(tr("Import could not be saved. Previous data was preserved."));}
    cancelImport();m_error.clear();emit errorChanged();emit changed();return true;
}
