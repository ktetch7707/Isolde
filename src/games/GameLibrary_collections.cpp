#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include "GameLibrary.h"
#include "GameResourceLimits.h"
#include "media/MediaGrants.h"
#include "OpticalDiscIdentity.h"
#include "home/ComponentSchema.h"

QString sqlText(const QVariant& value);
bool approvedMediaDirectory(const QString& profile, const QString& path);
bool exactKeys(const QJsonObject& object, const QStringList& keys);
bool archiveInteger(const QJsonValue& value, qint64* result);
QByteArray normalizeArtwork(const QByteArray& source);
QString artworkHash(const QByteArray& bytes);
bool persistArtwork(const QString& databasePath, const QByteArray& bytes, QString* hash, QString* error);

namespace
{
constexpr int MaxGames = GamesResourceLimits::CatalogEntries;
constexpr int MaxPlatforms = GamesResourceLimits::Platforms;
constexpr int MaxCollections = GamesResourceLimits::Collections;
constexpr int MaxProvenanceRecords = GamesResourceLimits::ProvenanceRecords;
constexpr int MaxTags = 32;
constexpr qsizetype MaxCatalogArchiveBytes = GamesResourceLimits::CatalogArchiveBytes;
constexpr qint64 MaxManagedArtworkBytes = 12 * 1024 * 1024;
constexpr qint64 MaxRestoreArtworkBytes = GamesResourceLimits::ArtworkRestoreBytes;
constexpr int PageSize = GamesResourceLimits::PageEntries;
}

QVariantList
GameLibrary::collections()
{
    QVariantList result;
    if (!m_ready)
        return result;
    if (!m_hasCollections)
        return result;
    QSqlQuery query(m_db);
    const auto smartPlatform =
        m_hasEditions
            ? QString(
                  "(SELECT count(*) FROM games g WHERE lower(g.platform)=lower(c.filter_value) OR EXISTS(SELECT 1 "
                  "FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(c.filter_value)))")
            : QString("(SELECT count(*) FROM games g WHERE lower(g.platform)=lower(c.filter_value))");
    const auto countExpression =
        "CASE WHEN c.kind='manual' THEN count(m.game_id) "
        "WHEN c.filter_type='favorite' THEN (SELECT count(*) FROM games g WHERE g.favorite=1) "
        "WHEN c.filter_type='title' THEN (SELECT count(*) FROM games g WHERE "
        "instr(lower(g.title),lower(c.filter_value))>0) "
        "WHEN c.filter_type='platform' THEN " +
        smartPlatform +
        " "
        "WHEN c.filter_type='franchise' THEN (SELECT count(*) FROM games g JOIN game_metadata gm ON gm.game_id=g.id "
        "WHERE instr(lower(gm.franchise),lower(c.filter_value))>0) "
        "WHEN c.filter_type='tag' THEN (SELECT count(*) FROM games g,game_metadata gm,json_each(gm.tags) tag WHERE "
        "gm.game_id=g.id AND lower(tag.value)=lower(c.filter_value)) "
        "WHEN c.filter_type='recent' THEN (SELECT count(*) FROM games g JOIN game_activity ga ON ga.game_id=g.id) ELSE "
        "0 END";
    if (!query.exec("SELECT c.id,c.name,c.kind,c.filter_type,c.filter_value," + countExpression +
                    " FROM game_collections c LEFT JOIN game_collection_members m ON m.collection_id=c.id GROUP BY "
                    "c.id ORDER BY c.name COLLATE NOCASE,c.id"))
    {
        fail(query.lastError().text());
        return {};
    }
    while (query.next())
        result.append(QVariantMap{{"id", query.value(0)},
                                  {"name", query.value(1)},
                                  {"kind", query.value(2)},
                                  {"filterType", query.value(3)},
                                  {"filterValue", query.value(4)},
                                  {"count", query.value(5).toInt()}});
    return result;
}

QString
GameLibrary::createCollection(const QString& name,
                              const QString& kind,
                              const QString& filterType,
                              const QString& filterValue)
{
    const auto clean = name.trimmed();
    if (!createCollectionValidName(clean, name))
        return {};
    if (!createCollectionValidKind(kind, filterType, filterValue))
        return {};
    if (!createCollectionEnsureSchema(filterType))
        return {};
    if (!createCollectionCheckLimit())
        return {};
    return createCollectionInsert(clean, kind, filterType, filterValue);
}

bool
GameLibrary::createCollectionValidName(const QString& clean, const QString& name)
{
    if (!m_ready || clean.isEmpty() || clean != name || clean.size() > 80 || clean.contains(QChar::Null))
    {
        fail(tr("Choose a collection name of up to 80 characters."));
        return false;
    }
    return true;
}

bool
GameLibrary::createCollectionValidKind(const QString& kind, const QString& filterType,
                                       const QString& filterValue)
{
    if (kind == "manual")
        return createCollectionValidManual(filterType, filterValue);
    if (kind == "smart")
        return createCollectionValidSmart(filterType, filterValue);
    fail(tr("Choose a manual or smart collection."));
    return false;
}

bool
GameLibrary::createCollectionValidManual(const QString& filterType, const QString& filterValue)
{
    if (!filterType.isEmpty() || !filterValue.isEmpty())
    {
        fail(tr("Manual collections do not accept executable filters."));
        return false;
    }
    return true;
}

bool
GameLibrary::createCollectionValidSmart(const QString& filterType, const QString& filterValue)
{
    if (!QStringList{"platform", "favorite", "title", "franchise", "tag", "recent"}.contains(filterType) ||
        filterValue.size() > 80 || filterValue.contains(QChar::Null) ||
        (!QStringList{"favorite", "recent"}.contains(filterType) && filterValue.trimmed().isEmpty()) ||
        (QStringList{"favorite", "recent"}.contains(filterType) && !filterValue.isEmpty()))
    {
        fail(tr("Choose one supported declarative smart collection rule."));
        return false;
    }
    return true;
}

bool
GameLibrary::createCollectionEnsureSchema(const QString& filterType)
{
    if ((!m_hasCollections || (filterType == "recent" && !m_hasActivity)) && !ensureOrganizationSchema())
        return false;
    return true;
}

bool
GameLibrary::createCollectionCheckLimit()
{
    if (collections().size() >= MaxCollections)
    {
        fail(tr("This profile already has 256 game collections."));
        return false;
    }
    return true;
}

QString
GameLibrary::createCollectionInsert(const QString& clean, const QString& kind, const QString& filterType,
                                    const QString& filterValue)
{
    const auto id = "collection-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO game_collections(id,name,kind,filter_type,filter_value) VALUES(?,?,?,?,?)");
    query.addBindValue(id);
    query.addBindValue(clean);
    query.addBindValue(kind);
    query.addBindValue(filterType.isNull() ? QStringLiteral("") : filterType);
    query.addBindValue(filterValue.isNull() ? QStringLiteral("") : filterValue);
    if (!query.exec())
    {
        fail(tr("Collection was not created. Its name may already be in use. %1").arg(query.lastError().text()));
        return {};
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return id;
}

bool
GameLibrary::updateCollection(const QString& id,
                              const QString& name,
                              const QString& filterType,
                              const QString& filterValue)
{
    const auto clean = name.trimmed();
    if (!updateCollectionValidArgs(id, clean, name))
        return fail(tr("Choose a collection name of up to 80 characters."));
    QSqlQuery current(m_db);
    current.prepare("SELECT kind FROM game_collections WHERE id=?");
    current.addBindValue(id);
    if (!current.exec() || !current.next())
        return fail(tr("Collection not found in this profile."));
    const auto kind = current.value(0).toString();
    current.finish();
    if (!updateCollectionValidKind(kind, filterType, filterValue))
        return false;
    if (!updateCollectionSave(id, clean, filterType, filterValue))
        return false;
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::updateCollectionValidArgs(const QString& id, const QString& clean, const QString& name)
{
    return m_ready && m_hasCollections && ComponentSchema::safeId(id) && !clean.isEmpty() && clean == name &&
           clean.size() <= 80 && !clean.contains(QChar::Null);
}

bool
GameLibrary::updateCollectionValidKind(const QString& kind, const QString& filterType,
                                       const QString& filterValue)
{
    if (kind == "manual")
        return updateCollectionValidManual(filterType, filterValue);
    if (kind == "smart")
        return updateCollectionValidSmart(filterType, filterValue);
    return fail(tr("Collection type is invalid."));
}

bool
GameLibrary::updateCollectionValidManual(const QString& filterType, const QString& filterValue)
{
    if (!filterType.isEmpty() || !filterValue.isEmpty())
        return fail(tr("Manual collections do not accept executable filters."));
    return true;
}

bool
GameLibrary::updateCollectionValidSmart(const QString& filterType, const QString& filterValue)
{
    if (!QStringList{"platform", "favorite", "title", "franchise", "tag", "recent"}.contains(filterType) ||
        filterValue.size() > 80 || filterValue.contains(QChar::Null) ||
        (!QStringList{"favorite", "recent"}.contains(filterType) && filterValue.trimmed().isEmpty()) ||
        (QStringList{"favorite", "recent"}.contains(filterType) && !filterValue.isEmpty()))
        return fail(tr("Choose one supported declarative smart collection rule."));
    return true;
}

bool
GameLibrary::updateCollectionSave(const QString& id, const QString& clean, const QString& filterType,
                                  const QString& filterValue)
{
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    query.prepare("UPDATE game_collections SET name=?,filter_type=?,filter_value=? WHERE id=?");
    query.addBindValue(clean);
    query.addBindValue(filterType.isNull() ? QStringLiteral("") : filterType);
    query.addBindValue(filterValue.isNull() ? QStringLiteral("") : filterValue);
    query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1 || !m_db.commit())
    {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Collection was not updated. Its previous settings were preserved. %1").arg(error));
    }
    return true;
}

bool
GameLibrary::removeCollection(const QString& id)
{
    if (!m_ready || !m_hasCollections || !ComponentSchema::safeId(id))
        return fail(tr("Collection not found in this profile."));
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM game_collections WHERE id=?");
    query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1)
        return fail(tr("Collection could not be removed."));
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::setCollectionMember(const QString& collectionId, const QString& gameId, bool member)
{
    if (!m_ready || !m_hasCollections || !ComponentSchema::safeId(collectionId) || details(gameId).isEmpty())
        return fail(tr("Choose a game and collection from this profile."));
    QSqlQuery query(m_db);
    query.prepare("SELECT kind FROM game_collections WHERE id=?");
    query.addBindValue(collectionId);
    if (!query.exec() || !query.next())
        return fail(tr("Collection not found in this profile."));
    if (query.value(0).toString() != "manual")
        return fail(tr("Smart collection membership is determined by its saved rule."));
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    if (member)
    {
        query.prepare("SELECT count(*) FROM game_collection_members WHERE collection_id=?");
        query.addBindValue(collectionId);
        if (!query.exec() || !query.next() || query.value(0).toInt() >= MaxGames)
        {
            m_db.rollback();
            return fail(tr("Manual collection is full."));
        }
        query.prepare(
            "INSERT OR IGNORE INTO game_collection_members(collection_id,game_id,position) SELECT "
            "?,?,COALESCE(MAX(position),-1)+1 FROM game_collection_members WHERE collection_id=?");
        query.addBindValue(collectionId);
        query.addBindValue(gameId);
        query.addBindValue(collectionId);
    }
    else
    {
        query.prepare("DELETE FROM game_collection_members WHERE collection_id=? AND game_id=?");
        query.addBindValue(collectionId);
        query.addBindValue(gameId);
    }
    if (!query.exec() || !m_db.commit())
    {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Collection membership was not changed. %1").arg(error));
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::moveCollectionMember(const QString& collectionId, const QString& gameId, int direction)
{
    if (!moveCollectionValidArgs(collectionId, gameId, direction))
        return fail(tr("Choose a valid manual collection and game."));
    QSqlQuery kind(m_db);
    kind.prepare("SELECT kind FROM game_collections WHERE id=?");
    kind.addBindValue(collectionId);
    if (!kind.exec() || !kind.next() || kind.value(0).toString() != "manual")
        return fail(tr("Only manual collection order can be changed."));
    kind.finish();
    QList<QPair<QString, int>> members;
    if (!moveCollectionLoadMembers(collectionId, members))
        return fail(m_db.lastError().text());
    auto index = -1;
    for (qsizetype i = 0; i < members.size(); ++i)
        if (members.at(i).first == gameId)
        {
            index = int(i);
            break;
        }
    if (index < 0)
        return fail(tr("Game is not in this collection."));
    const auto target = qsizetype(index) + direction;
    if (target < 0 || target >= members.size())
    {
        m_error.clear();
        emit errorChanged();
        return true;
    }
    return moveCollectionSwap(collectionId, gameId, members, index, target);
}

bool
GameLibrary::moveCollectionValidArgs(const QString& collectionId, const QString& gameId, int direction)
{
    return m_ready && m_hasCollections && ComponentSchema::safeId(collectionId) &&
           ComponentSchema::safeId(gameId) && (direction == -1 || direction == 1);
}

bool
GameLibrary::moveCollectionLoadMembers(const QString& collectionId, QList<QPair<QString, int>>& members)
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT game_id,position FROM game_collection_members WHERE collection_id=? ORDER BY position,game_id");
    query.addBindValue(collectionId);
    if (!query.exec())
        return false;
    while (query.next())
        members.append({query.value(0).toString(), query.value(1).toInt()});
    query.finish();
    return true;
}

bool
GameLibrary::moveCollectionSwap(const QString& collectionId, const QString& gameId,
                                const QList<QPair<QString, int>>& members, int index, qsizetype target)
{
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    query.prepare("UPDATE game_collection_members SET position=-1 WHERE collection_id=? AND game_id=?");
    query.addBindValue(collectionId);
    query.addBindValue(gameId);
    bool ok = query.exec();
    query.prepare("UPDATE game_collection_members SET position=? WHERE collection_id=? AND game_id=?");
    query.addBindValue(members.at(index).second);
    query.addBindValue(collectionId);
    query.addBindValue(members.at(target).first);
    ok = ok && query.exec();
    query.prepare("UPDATE game_collection_members SET position=? WHERE collection_id=? AND game_id=?");
    query.addBindValue(members.at(target).second);
    query.addBindValue(collectionId);
    query.addBindValue(gameId);
    ok = ok && query.exec();
    if (!ok || !m_db.commit())
    {
        const auto error = query.lastError().text();
        m_db.rollback();
        return fail(tr("Collection order was not changed. %1").arg(error));
    }
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::collectionContains(const QString& collectionId, const QString& gameId)
{
    if (!m_ready || !m_hasCollections || !ComponentSchema::safeId(collectionId) || !ComponentSchema::safeId(gameId))
        return false;
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM game_collection_members WHERE collection_id=? AND game_id=?");
    query.addBindValue(collectionId);
    query.addBindValue(gameId);
    return query.exec() && query.next();
}

QVariantList
GameLibrary::pageCollection(const QString& id, int offset)
{
    if (!pageCollectionValidArgs(id, offset))
    {
        fail(tr("Invalid game collection query."));
        return {};
    }
    QSqlQuery collection(m_db);
    collection.prepare("SELECT kind,filter_type,filter_value FROM game_collections WHERE id=?");
    collection.addBindValue(id);
    if (!collection.exec() || !collection.next())
    {
        fail(tr("Collection not found in this profile."));
        return {};
    }
    const auto kind = collection.value(0).toString(), filter = collection.value(1).toString(),
               filterValue = collection.value(2).toString();
    QString sql = pageCollectionBaseSql(m_hasRichMetadata, m_hasArtwork);
    if (!pageCollectionBuildWhere(kind, filter, filterValue, m_hasEditions, sql))
    {
        fail(tr("Collection rule is invalid. No results were changed."));
        return {};
    }
    sql += " LIMIT 50 OFFSET ?";
    QSqlQuery query(m_db);
    query.prepare(sql);
    pageCollectionBindValues(kind, filter, filterValue, m_hasEditions, id, offset, query);
    if (!query.exec())
    {
        fail(query.lastError().text());
        return {};
    }
    QVariantList result;
    while (query.next())
    {
        QVariantMap entry;
        entry["id"] = query.value(0).toString();
        entry["title"] = query.value(1).toString();
        entry["platform"] = query.value(2).toString();
        entry["runtimeId"] = query.value(3).toString();
        entry["favorite"] = query.value(4).toBool();
        entry["franchise"] = query.value(5).toString();
        entry["tags"] = QJsonDocument::fromJson(query.value(6).toByteArray()).array().toVariantList();
        entry["aliases"] = m_hasRichMetadata
                               ? QJsonDocument::fromJson(query.value(7).toByteArray()).array().toVariantList()
                               : QVariantList{};
        entry["summary"] = m_hasRichMetadata ? query.value(8).toString() : QString();
        entry["artworkHash"] = m_hasArtwork ? query.value(m_hasRichMetadata ? 9 : 7).toString() : QString();
        result.append(entry);
    }
    return result;
}

bool
GameLibrary::pageCollectionValidArgs(const QString& id, int offset)
{
    return m_ready && m_hasCollections && ComponentSchema::safeId(id) && offset >= 0 && offset < MaxGames &&
           offset % PageSize == 0;
}

QString
GameLibrary::pageCollectionBaseSql(bool hasRichMetadata, bool hasArtwork)
{
    QString sql =
        "SELECT g.id,g.title,g.platform,g.runtime,g.favorite,COALESCE(m.franchise,''),COALESCE(m.tags,'[]')" +
        (hasRichMetadata ? QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')") : QString(",'[]',''")) +
        (hasArtwork ? QString(",COALESCE(m.artwork_hash,'')") : QString(",''")) +
        " FROM games g LEFT JOIN game_metadata m ON m.game_id=g.id";
    return sql;
}

bool
GameLibrary::pageCollectionBuildWhere(const QString& kind, const QString& filter, const QString& filterValue,
                                      bool hasEditions, QString& sql)
{
    Q_UNUSED(filterValue);
    if (kind == "manual")
    {
        sql += " JOIN game_collection_members cm ON cm.game_id=g.id WHERE cm.collection_id=? ORDER BY cm.position, "
               "g.id";
        return true;
    }
    if (filter == "platform")
    {
        sql += " WHERE lower(g.platform)=lower(?)";
        if (hasEditions)
            sql += " OR EXISTS(SELECT 1 FROM game_editions e WHERE e.game_id=g.id AND lower(e.platform)=lower(?))";
        sql += " ORDER BY g.title COLLATE NOCASE,g.id";
        return true;
    }
    if (filter == "favorite")
    {
        sql += " WHERE g.favorite=1 ORDER BY g.title COLLATE NOCASE,g.id";
        return true;
    }
    if (filter == "title")
    {
        sql += " WHERE instr(lower(g.title),lower(?))>0 ORDER BY g.title COLLATE NOCASE,g.id";
        return true;
    }
    if (filter == "franchise")
    {
        sql += " WHERE instr(lower(COALESCE(m.franchise,'')),lower(?))>0 ORDER BY g.title COLLATE NOCASE,g.id";
        return true;
    }
    if (filter == "tag")
    {
        sql +=
            " WHERE EXISTS(SELECT 1 FROM json_each(COALESCE(m.tags,'[]')) tag WHERE lower(tag.value)=lower(?)) ORDER "
            "BY g.title COLLATE NOCASE,g.id";
        return true;
    }
    if (filter == "recent" && hasEditions)
    {
        sql +=
            " WHERE EXISTS(SELECT 1 FROM game_activity a WHERE a.game_id=g.id) ORDER BY (SELECT a.last_played FROM "
            "game_activity a WHERE a.game_id=g.id) DESC,g.id";
        return true;
    }
    return false;
}

void
GameLibrary::pageCollectionBindValues(const QString& kind, const QString& filter, const QString& filterValue,
                                      bool hasEditions, const QString& id, int offset, QSqlQuery& query)
{
    if (kind == "manual")
        query.addBindValue(id);
    else if (filter == "platform")
    {
        query.addBindValue(filterValue);
        if (hasEditions)
            query.addBindValue(filterValue);
    }
    else if (filter == "title" || filter == "franchise" || filter == "tag")
        query.addBindValue(filterValue);
    query.addBindValue(offset);
}

bool
GameLibrary::exportCatalog(const QString& destination)
{
    if (!exportCatalogValidateDestination(destination))
        return fail(tr("Choose a JSON file in a specifically approved media folder."));
    QJsonObject archive{{"schemaVersion", 4}};
    QJsonArray games, editionsArray, collectionArray, provenanceArray;
    qsizetype estimatedArchiveBytes = 256;
    QSqlQuery query(m_db);
    if (!exportCatalogReadGames(query, games, estimatedArchiveBytes))
        return fail(tr("Cannot read the games catalog for export."));
    if (m_hasEditions)
    {
        if (!query.exec("SELECT id,game_id,title,platform,runtime,root,target FROM game_editions ORDER BY id"))
            return fail(tr("Cannot read game editions for export."));
        while (query.next())
        {
            if (editionsArray.size() >= MaxGames)
                return fail(tr("Game editions exceed the export bound."));
            editionsArray.append(
                QJsonObject{{"id", query.value(0).toString()},
                            {"gameId", query.value(1).toString()},
                            {"title", query.value(2).toString()},
                            {"platform", query.value(3).toString()},
                            {"runtimeId", query.value(4).toString()},
                            {"root", query.value(5).toString()},
                            {"target", query.value(6).toString()}});
            estimatedArchiveBytes += QJsonDocument(editionsArray.last().toObject()).toJson(QJsonDocument::Compact).size() + 1;
            if (estimatedArchiveBytes > MaxCatalogArchiveBytes)
                return fail(tr("Catalog export exceeds the 32 MiB size limit."));
        }
        query.finish();
    }
    if (m_hasCollections)
    {
        if (!exportCatalogReadCollections(query, collectionArray, estimatedArchiveBytes))
            return fail(tr("Cannot read game collections for export."));
    }
    if (m_hasProvenance)
    {
        if (!exportCatalogReadProvenance(query, provenanceArray, estimatedArchiveBytes))
            return fail(tr("Cannot read immutable acquisition provenance for export."));
    }
    archive.insert("games", games);
    archive.insert("editions", editionsArray);
    archive.insert("collections", collectionArray);
    archive.insert("provenance", provenanceArray);
    const auto bytes = QJsonDocument(archive).toJson(QJsonDocument::Compact);
    if (bytes.size() > MaxCatalogArchiveBytes)
        return fail(tr("Catalog export exceeds the 32 MiB size limit."));
    return exportCatalogWriteFile(destination, bytes);
}

bool
GameLibrary::exportCatalogValidateDestination(const QString& destination)
{
    if (!m_ready || !QDir::isAbsolutePath(destination) || QDir::cleanPath(destination) != destination ||
        !destination.endsWith(".json", Qt::CaseInsensitive) || QFileInfo(destination).isSymLink())
        return false;
    const auto parent = QFileInfo(destination).absolutePath();
    const auto grant = MediaGrants::validate(parent);
    if (!grant.isEmpty())
        return fail(grant);
    if (!approvedMediaDirectory(m_emulatorProfile, parent))
        return fail(tr("Approve this media folder for the active profile before exporting."));
    if (QFileInfo::exists(destination) &&
        (!QFileInfo(destination).isFile() || QFileInfo(destination).canonicalFilePath() != destination))
        return fail(tr("The catalog export target is not a regular file. Nothing was replaced."));
    return true;
}

bool
GameLibrary::exportCatalogReadGames(QSqlQuery& query, QJsonArray& games, qsizetype& estimatedArchiveBytes)
{
    const QString gameSql =
        "SELECT g.id,g.title,g.platform,g.runtime,g.root,g.target,g.favorite,g.added," +
        (m_hasMetadata ? QString("COALESCE(m.franchise,''),COALESCE(m.tags,'[]')") : QString("'','[]'")) +
        (m_hasRichMetadata ? QString(",COALESCE(m.aliases,'[]'),COALESCE(m.summary,'')") : QString(",'[]',''")) +
        (m_hasActivity ? QString(",COALESCE(a.last_played,0)") : QString(",0")) +
        (m_hasArtwork ? QString(",COALESCE(m.artwork_hash,'')") : QString(",''")) + " FROM games g" +
        (m_hasMetadata ? QString(" LEFT JOIN game_metadata m ON m.game_id=g.id") : QString()) +
        (m_hasActivity ? QString(" LEFT JOIN game_activity a ON a.game_id=g.id") : QString()) + " ORDER BY g.id";
    if (!query.exec(gameSql))
        return false;
    while (query.next())
    {
        if (games.size() >= MaxGames)
            return fail(tr("Games catalog exceeds the export bound."));
        QJsonArray tags;
        if (!exportCatalogReadGameTags(query.value(9).toByteArray(), tags))
            return false;
        QJsonArray aliases;
        if (!exportCatalogReadGameAliases(query.value(10).toByteArray(), aliases))
            return false;
        if (!exportCatalogValidateGameSummary(query.value(8), query.value(11)))
            return false;
        QString encodedArtwork;
        if (!exportCatalogReadGameArtwork(query.value(13).toString(), encodedArtwork))
            return false;
        const QJsonObject gameRecord{{"id", query.value(0).toString()},
                                     {"title", query.value(1).toString()},
                                     {"platform", query.value(2).toString()},
                                     {"runtimeId", query.value(3).toString()},
                                     {"root", query.value(4).toString()},
                                     {"target", query.value(5).toString()},
                                     {"favorite", query.value(6).toBool()},
                                     {"added", query.value(7).toLongLong()},
                                     {"franchise", query.value(8).toString()},
                                     {"tags", tags},
                                     {"aliases", aliases},
                                     {"summary", query.value(11).toString()},
                                     {"lastPlayed", query.value(12).toLongLong()},
                                     {"artwork", encodedArtwork}};
        estimatedArchiveBytes += QJsonDocument(gameRecord).toJson(QJsonDocument::Compact).size() + 1;
        if (estimatedArchiveBytes > MaxCatalogArchiveBytes)
            return fail(tr("Catalog export exceeds the 32 MiB size limit."));
        games.append(gameRecord);
    }
    query.finish();
    return true;
}

bool
GameLibrary::exportCatalogReadGameTags(const QByteArray& data, QJsonArray& tags)
{
    const auto tagDocument = QJsonDocument::fromJson(data);
    if (!tagDocument.isArray() || tagDocument.array().size() > MaxTags)
        return fail(tr("Game organization metadata is corrupt. Export was canceled."));
    const auto tagList = tagDocument.array();
    for (const auto& tag : tagList)
    {
        if (!tag.isString() || tag.toString().isEmpty() || tag.toString() != tag.toString().trimmed() ||
            tag.toString().size() > 64 || tag.toString().contains(',') || tag.toString().contains(QChar::Null))
            return fail(tr("Game tags are corrupt. Export was canceled."));
        tags.append(tag);
    }
    return true;
}

bool
GameLibrary::exportCatalogReadGameAliases(const QByteArray& data, QJsonArray& aliases)
{
    const auto aliasDocument = QJsonDocument::fromJson(data);
    if (!aliasDocument.isArray() || aliasDocument.array().size() > MaxTags)
        return fail(tr("Game aliases are corrupt. Export was canceled."));
    const auto aliasList = aliasDocument.array();
    for (const auto& alias : aliasList)
    {
        if (!alias.isString() || alias.toString().isEmpty() || alias.toString() != alias.toString().trimmed() ||
            alias.toString().size() > 160 || alias.toString().contains(';') ||
            alias.toString().contains(QChar::Null))
            return fail(tr("Game aliases are corrupt. Export was canceled."));
        aliases.append(alias);
    }
    return true;
}

bool
GameLibrary::exportCatalogValidateGameSummary(const QVariant& franchise, const QVariant& summary)
{
    if (franchise.toString().size() > 160 || franchise.toString() != franchise.toString().trimmed() ||
        franchise.toString().contains(QChar::Null) || summary.toString().size() > 4096 ||
        summary.toString().contains(QChar::Null))
        return fail(tr("Game summary is corrupt. Export was canceled."));
    return true;
}

bool
GameLibrary::exportCatalogReadGameArtwork(const QString& hash, QString& encodedArtwork)
{
    if (hash.isEmpty())
        return true;
    if (!m_hasArtwork || !QRegularExpression("^[0-9a-f]{64}$").match(hash).hasMatch())
        return fail(tr("Managed artwork reference is invalid. Export was canceled."));
    const auto path = artworkDirectory() + "/" + hash + ".png";
    QFile artwork(path);
    if (QFileInfo(path).isSymLink() || !QFileInfo(path).isFile() ||
        QFileInfo(path).canonicalFilePath() != path || QFileInfo(path).size() < 1 ||
        QFileInfo(path).size() > MaxManagedArtworkBytes || !artwork.open(QIODevice::ReadOnly))
        return fail(tr("Managed artwork is unavailable. Export was canceled."));
    const auto artworkBytes = artwork.readAll();
    if (artworkHash(artworkBytes) != hash || normalizeArtwork(artworkBytes).isEmpty())
        return fail(tr("Managed artwork failed integrity validation. Export was canceled."));
    encodedArtwork = QString::fromLatin1(artworkBytes.toBase64());
    return true;
}

bool
GameLibrary::exportCatalogReadCollections(QSqlQuery& query, QJsonArray& collectionArray,
                                          qsizetype& estimatedArchiveBytes)
{
    if (!query.exec("SELECT id,name,kind,filter_type,filter_value FROM game_collections ORDER BY id"))
        return fail(tr("Cannot read game collections for export."));
    while (query.next())
    {
        if (collectionArray.size() >= MaxCollections)
            return fail(tr("Game collections exceed the export bound."));
        const auto collectionId = query.value(0).toString();
        QJsonArray members;
        QSqlQuery memberQuery(m_db);
        memberQuery.prepare(
            "SELECT game_id FROM game_collection_members WHERE collection_id=? ORDER BY position,game_id");
        memberQuery.addBindValue(collectionId);
        if (!memberQuery.exec())
            return fail(tr("Cannot read collection membership for export."));
        while (memberQuery.next())
        {
            if (members.size() >= MaxGames)
                return fail(tr("Manual collection exceeds the export bound."));
            members.append(memberQuery.value(0).toString());
        }
        const QJsonObject collectionRecord{{"id", collectionId},
                                           {"name", query.value(1).toString()},
                                           {"kind", query.value(2).toString()},
                                           {"filterType", query.value(3).toString()},
                                           {"filterValue", query.value(4).toString()},
                                           {"members", members}};
        estimatedArchiveBytes += QJsonDocument(collectionRecord).toJson(QJsonDocument::Compact).size() + 1;
        if (estimatedArchiveBytes > MaxCatalogArchiveBytes)
            return fail(tr("Catalog export exceeds the 32 MiB size limit."));
        collectionArray.append(collectionRecord);
    }
    return true;
}

bool
GameLibrary::exportCatalogReadProvenance(QSqlQuery& query, QJsonArray& provenanceArray,
                                         qsizetype& estimatedArchiveBytes)
{
    if (!query.exec(
            "SELECT entity_id,entity_type,source_kind,origin,source_reference,source_fingerprint,recorded_at FROM "
            "game_provenance ORDER BY entity_id"))
        return fail(tr("Cannot read immutable acquisition provenance for export."));
    while (query.next())
    {
        if (provenanceArray.size() >= MaxProvenanceRecords)
            return fail(tr("Acquisition provenance exceeds the export record bound."));
        const QJsonObject record{{"entityId", query.value(0).toString()},
                                 {"entityType", query.value(1).toString()},
                                 {"sourceKind", query.value(2).toString()},
                                 {"origin", query.value(3).toString()},
                                 {"sourceReference", query.value(4).toString()},
                                 {"sourceFingerprint", query.value(5).toString()},
                                 {"recordedAt", query.value(6).toLongLong()}};
        estimatedArchiveBytes += QJsonDocument(record).toJson(QJsonDocument::Compact).size() + 1;
        if (estimatedArchiveBytes > MaxCatalogArchiveBytes)
            return fail(tr("Catalog export exceeds the 32 MiB size limit."));
        provenanceArray.append(record);
    }
    query.finish();
    return true;
}

bool
GameLibrary::exportCatalogWriteFile(const QString& destination, const QByteArray& bytes)
{
    QSaveFile file(destination);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit())
    {
        file.cancelWriting();
        return fail(tr("Catalog export failed. The previous file was preserved. %1").arg(file.errorString()));
    }
    m_error.clear();
    emit errorChanged();
    return true;
}

// Restrict restore to an explicitly confirmed, bounded regular file at its canonical path.
bool
GameLibrary::restoreCatalogValidateArgs(const QString& source, bool confirmed)
{
    const QFileInfo file(source);
    return m_ready && confirmed && QDir::isAbsolutePath(source) && QDir::cleanPath(source) == source &&
           !file.isSymLink() && file.isFile() && file.canonicalFilePath() == source &&
           file.size() > 0 && file.size() <= MaxCatalogArchiveBytes;
}

// Check the archive envelope before parsing records or touching the database.
bool
GameLibrary::restoreCatalogValidateArchive(const QJsonObject& archive)
{
    const int version = archive.value("schemaVersion").toInt(-1);
    const auto keys = version == 4 ? QStringList{"schemaVersion", "games", "editions", "collections", "provenance"}
                                   : QStringList{"schemaVersion", "games", "editions", "collections"};
    if (!exactKeys(archive, keys) || !archive.value("schemaVersion").isDouble() ||
        archive.value("schemaVersion").toDouble() != version || version < 1 || version > 4 ||
        !archive.value("games").isArray() || archive.value("games").toArray().size() > MaxGames ||
        !archive.value("editions").isArray() || archive.value("editions").toArray().size() > MaxGames ||
        !archive.value("collections").isArray() || archive.value("collections").toArray().size() > MaxCollections ||
        (version == 4 && (!archive.value("provenance").isArray() ||
                          archive.value("provenance").toArray().size() > MaxProvenanceRecords)))
        return fail(tr("Unsupported Games backup version or catalog bounds."));
    return true;
}

bool
GameLibrary::restoreCatalog(const QString& source, bool confirmed)
{
    if (m_portablePhase.startsWith("uninstall"))
        return fail(tr("Wait for the managed uninstall to finish before restoring the catalog."));
    if (!restoreCatalogValidateArgs(source, confirmed))
        return fail(tr("Review and confirm a readable Games JSON backup from an approved folder."));
    const auto grant = MediaGrants::validate(QFileInfo(source).absolutePath());
    if (!grant.isEmpty())
        return fail(grant);
    if (!approvedMediaDirectory(m_emulatorProfile, QFileInfo(source).absolutePath()))
        return fail(tr("Approve this media folder for the active profile before restoring."));
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly))
        return fail(tr("Cannot read the selected Games backup."));
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject())
        return fail(tr("Invalid Games backup JSON."));
    const auto archive = document.object();
    const int archiveVersion = archive.value("schemaVersion").toInt();
    QList<QVariantMap> games, editionsData, collectionsData, provenanceData;
    QSet<QString> ids, gameIds, editionIds, platforms, names, installations;
    QMap<QString, int> editionCounts;
    qint64 restoreArtworkBytes = 0;
    if (!restoreCatalogValidateArchive(archive))
        return false;
    for (const auto& entry : archive.value("games").toArray())
    {
        if (!entry.isObject())
            return fail(tr("Invalid game record in backup."));
        const auto object = entry.toObject();
        const auto gameKeys = archiveVersion == 1   ? QStringList{"id",
                                                                  "title",
                                                                  "platform",
                                                                  "runtimeId",
                                                                  "root",
                                                                  "target",
                                                                  "favorite",
                                                                  "added",
                                                                  "franchise",
                                                                  "tags",
                                                                  "lastPlayed"}
                              : archiveVersion == 2 ? QStringList{"id",
                                                                  "title",
                                                                  "platform",
                                                                  "runtimeId",
                                                                  "root",
                                                                  "target",
                                                                  "favorite",
                                                                  "added",
                                                                  "franchise",
                                                                  "tags",
                                                                  "aliases",
                                                                  "summary",
                                                                  "lastPlayed"}
                                                    : QStringList{"id",
                                                                  "title",
                                                                  "platform",
                                                                  "runtimeId",
                                                                  "root",
                                                                  "target",
                                                                  "favorite",
                                                                  "added",
                                                                  "franchise",
                                                                  "tags",
                                                                  "aliases",
                                                                  "summary",
                                                                  "lastPlayed",
                                                                  "artwork"};
        if (!exactKeys(object, gameKeys) || !object.value("id").isString() ||
            !ComponentSchema::safeId(object.value("id").toString()) || ids.contains(object.value("id").toString()) ||
            !object.value("favorite").isBool() || !object.value("franchise").isString() ||
            object.value("franchise").toString().size() > 160 ||
            object.value("franchise").toString().contains(QChar::Null) || !object.value("tags").isArray() ||
            object.value("tags").toArray().size() > MaxTags ||
            (archiveVersion >= 2 &&
             (!object.value("aliases").isArray() || object.value("aliases").toArray().size() > MaxTags ||
              !object.value("summary").isString() || object.value("summary").toString().size() > 4096 ||
              object.value("summary").toString().contains(QChar::Null))))
            return fail(tr("Invalid or duplicate game identity or organization metadata in backup."));
        if (archiveVersion >= 3 && (!object.value("artwork").isString() ||
                                    object.value("artwork").toString().size() > ((MaxManagedArtworkBytes + 2) / 3) * 4))
            return fail(tr("Invalid or oversized artwork in backup."));
        const auto gameId = object.value("id").toString(), platform = object.value("platform").toString();
        QVariantMap registration{{"schemaVersion", 1},
                                 {"title", object.value("title").toString()},
                                 {"platform", platform},
                                 {"runtimeId", object.value("runtimeId").toString()},
                                 {"root", object.value("root").toString()},
                                 {"target", object.value("target").toString()}};
        QString schemaError;
        if (ComponentSchema::gameRegistration(registration, &schemaError).isEmpty())
            return fail(schemaError);
        const auto root = registration.value("root").toString();
        if (!root.isEmpty() && !OpticalDiscIdentify::isRootReference(root) &&
            !MediaGrants::validate(root, false).isEmpty())
            return fail(tr("Backup refers to a game folder not authorized in this profile."));
        if (object.value("franchise").toString() != object.value("franchise").toString().trimmed())
            return fail(tr("Franchise names in the backup must be trimmed."));
        const auto installIdentity =
            registration.value("runtimeId").toString() + "\n" + root + "\n" + registration.value("target").toString();
        if (installations.contains(installIdentity))
            return fail(tr("Backup contains duplicate installation identities."));
        installations.insert(installIdentity);
        if (platforms.size() >= MaxPlatforms && !platforms.contains(platform.toCaseFolded()))
            return fail(tr("Backup contains more than 128 platforms."));
        platforms.insert(platform.toCaseFolded());
        ids.insert(gameId);
        gameIds.insert(gameId);
        QSet<QString> tagSet;
        for (const auto& tag : object.value("tags").toArray())
        {
            if (!tag.isString() || tag.toString().isEmpty() || tag.toString() != tag.toString().trimmed() ||
                tag.toString().size() > 64 || tag.toString().contains(',') || tag.toString().contains(QChar::Null) ||
                tagSet.contains(tag.toString().toCaseFolded()))
                return fail(tr("Invalid or duplicate tag in backup."));
            tagSet.insert(tag.toString().toCaseFolded());
        }
        const auto archiveAliases = archiveVersion >= 2 ? object.value("aliases").toArray() : QJsonArray{};
        QSet<QString> aliasSet;
        for (const auto& alias : archiveAliases)
        {
            if (!alias.isString() || alias.toString().isEmpty() || alias.toString() != alias.toString().trimmed() ||
                alias.toString().size() > 160 || alias.toString().contains(';') ||
                alias.toString().contains(QChar::Null) ||
                aliasSet.contains(alias.toString().toCaseFolded()))
                return fail(tr("Invalid or duplicate alias in backup."));
            aliasSet.insert(alias.toString().toCaseFolded());
        }
        qint64 added = 0, lastPlayed = 0;
        if (!archiveInteger(object.value("added"), &added) || !archiveInteger(object.value("lastPlayed"), &lastPlayed))
            return fail(tr("Invalid game activity timestamp in backup."));
        auto value = registration;
        value["id"] = gameId;
        value["favorite"] = object.value("favorite").toBool();
        value["added"] = added;
        value["franchise"] = object.value("franchise").toString();
        QVariantList tagValues;
        for (const auto& tag : object.value("tags").toArray())
            tagValues.append(tag.toString());
        value["tags"] = tagValues;
        value["lastPlayed"] = lastPlayed;
        QVariantList archiveAliasValues;
        for (const auto& alias : archiveAliases)
            archiveAliasValues.append(alias.toString());
        value["aliases"] = archiveAliasValues;
        value["summary"] = archiveVersion >= 2 ? object.value("summary").toString() : QString();
        QByteArray artworkBytes;
        if (archiveVersion >= 3)
        {
            const auto encoded = object.value("artwork").toString().toLatin1();
            if (!encoded.isEmpty())
            {
                const auto decoded = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
                if (decoded.isEmpty() || decoded.size() > MaxManagedArtworkBytes || decoded.toBase64() != encoded)
                    return fail(tr("Invalid encoded artwork in backup."));
                artworkBytes = normalizeArtwork(decoded);
                if (artworkBytes.isEmpty())
                    return fail(tr("Backup artwork is not a supported bounded PNG or JPEG."));
                restoreArtworkBytes += artworkBytes.size();
                if (restoreArtworkBytes > MaxRestoreArtworkBytes)
                    return fail(tr("Backup artwork exceeds the 128 MiB restore budget."));
            }
        }
        value["artworkBytes"] = artworkBytes;
        games.append(value);
    }
    for (const auto& entry : archive.value("editions").toArray())
    {
        if (!entry.isObject())
            return fail(tr("Invalid edition record in backup."));
        const auto object = entry.toObject();
        if (!exactKeys(object, {"id", "gameId", "title", "platform", "runtimeId", "root", "target"}) ||
            !object.value("id").isString() || !object.value("gameId").isString() ||
            !ComponentSchema::safeId(object.value("id").toString()) ||
            !gameIds.contains(object.value("gameId").toString()) || ids.contains(object.value("id").toString()))
            return fail(tr("Invalid or duplicate edition identity in backup."));
        QVariantMap registration{{"schemaVersion", 1},
                                 {"title", object.value("title").toString()},
                                 {"platform", object.value("platform").toString()},
                                 {"runtimeId", object.value("runtimeId").toString()},
                                 {"root", object.value("root").toString()},
                                 {"target", object.value("target").toString()}};
        QString schemaError;
        if (ComponentSchema::gameRegistration(registration, &schemaError).isEmpty())
            return fail(schemaError);
        const auto root = registration.value("root").toString();
        if (!root.isEmpty() && !OpticalDiscIdentify::isRootReference(root) &&
            !MediaGrants::validate(root, false).isEmpty())
            return fail(tr("Backup edition refers to a game folder not authorized in this profile."));
        const auto folded = registration.value("platform").toString().toCaseFolded();
        if (platforms.size() >= MaxPlatforms && !platforms.contains(folded))
            return fail(tr("Backup contains more than 128 platforms."));
        platforms.insert(folded);
        const auto identity =
            registration.value("runtimeId").toString() + "\n" + root + "\n" + registration.value("target").toString();
        if (installations.contains(identity))
            return fail(tr("Two backup editions refer to the same installation."));
        installations.insert(identity);
        const auto gameId = object.value("gameId").toString();
        if (++editionCounts[gameId] >= 32)
            return fail(tr("A backup game has more than 31 additional editions."));
        ids.insert(object.value("id").toString());
        editionIds.insert(object.value("id").toString());
        auto value = registration;
        value["id"] = object.value("id").toString();
        value["gameId"] = gameId;
        editionsData.append(value);
    }
    for (const auto& entry : archive.value("collections").toArray())
    {
        if (!entry.isObject())
            return fail(tr("Invalid collection record in backup."));
        const auto object = entry.toObject();
        const auto kind = object.value("kind").toString();
        if (!exactKeys(object, {"id", "name", "kind", "filterType", "filterValue", "members"}) ||
            !object.value("id").isString() || !ComponentSchema::safeId(object.value("id").toString()) ||
            ids.contains(object.value("id").toString()) || !object.value("name").isString() ||
            !object.value("kind").isString() || !object.value("filterType").isString() ||
            !object.value("filterValue").isString() || !object.value("members").isArray() ||
            object.value("members").toArray().size() > MaxGames)
            return fail(tr("Invalid collection identity or membership in backup."));
        const auto name = object.value("name").toString();
        const auto foldedName = name.toCaseFolded();
        if (name.isEmpty() || name != name.trimmed() || name.size() > 80 || name.contains(QChar::Null) ||
            names.contains(foldedName))
            return fail(tr("Invalid or duplicate collection name in backup."));
        names.insert(foldedName);
        const auto filterType = object.value("filterType").toString(),
                   filterValue = object.value("filterValue").toString();
        if (kind == "manual")
        {
            if (!filterType.isEmpty() || !filterValue.isEmpty())
                return fail(tr("Manual collections cannot contain filter expressions."));
        }
        else if (kind == "smart")
        {
            if (!QStringList{"platform", "favorite", "title", "franchise", "tag", "recent"}.contains(filterType) ||
                filterValue.size() > 80 || filterValue.contains(QChar::Null) ||
                (!QStringList{"favorite", "recent"}.contains(filterType) && filterValue.trimmed().isEmpty()) ||
                (QStringList{"favorite", "recent"}.contains(filterType) && !filterValue.isEmpty()) ||
                !object.value("members").toArray().isEmpty())
                return fail(tr("Invalid smart collection rule in backup."));
        }
        else
            return fail(tr("Unknown collection type in backup."));
        QSet<QString> memberSet;
        for (const auto& member : object.value("members").toArray())
        {
            if (!member.isString() || !gameIds.contains(member.toString()) || memberSet.contains(member.toString()))
                return fail(tr("Collection refers to an unknown or duplicate game."));
            memberSet.insert(member.toString());
        }
        ids.insert(object.value("id").toString());
        collectionsData.append({{"id", object.value("id").toString()},
                                {"name", name},
                                {"kind", kind},
                                {"filterType", filterType},
                                {"filterValue", filterValue},
                                {"members", object.value("members").toArray().toVariantList()}});
    }
    if (archiveVersion == 4)
    {
        QSet<QString> provenanceIds;
        const QSet<QString> sourceKinds{
            "steam-app", "optical-media", "isolde-managed-install", "external-folder", "runtime-entry"};
        const QSet<QString> origins{
            "steam-import", "folder-import", "optical-registration", "portable-install", "manual-registration"};
        for (const auto& entry : archive.value("provenance").toArray())
        {
            if (!entry.isObject())
                return fail(tr("Invalid acquisition provenance record in backup."));
            const auto object = entry.toObject();
            if (!exactKeys(object,
                           {"entityId",
                            "entityType",
                            "sourceKind",
                            "origin",
                            "sourceReference",
                            "sourceFingerprint",
                            "recordedAt"}) ||
                !object.value("entityId").isString() || !ComponentSchema::safeId(object.value("entityId").toString()) ||
                !object.value("entityType").isString() ||
                !QStringList{"game", "edition"}.contains(object.value("entityType").toString()) ||
                !object.value("sourceKind").isString() ||
                !sourceKinds.contains(object.value("sourceKind").toString()) || !object.value("origin").isString() ||
                !origins.contains(object.value("origin").toString()) || !object.value("sourceReference").isString() ||
                object.value("sourceReference").toString().size() > 4096 ||
                object.value("sourceReference").toString().contains(QChar::Null) ||
                !object.value("sourceFingerprint").isString() ||
                !QRegularExpression("^[0-9a-f]{64}$").match(object.value("sourceFingerprint").toString()).hasMatch())
                return fail(tr("Invalid acquisition provenance identity in backup."));
            const auto kind = object.value("sourceKind").toString(), origin = object.value("origin").toString();
            const auto expectedOrigin = kind == "steam-app"                ? QString("steam-import")
                                        : kind == "optical-media"          ? QString("optical-registration")
                                        : kind == "isolde-managed-install" ? QString("portable-install")
                                        : kind == "external-folder"        ? QString("folder-import")
                                                                           : QString("manual-registration");
            const auto entityId = object.value("entityId").toString();
            const auto entityType = object.value("entityType").toString();
            const bool knownGame = gameIds.contains(entityId);
            const bool knownEdition = editionIds.contains(entityId);
            qint64 recordedAt = 0;
            if ((origin != "manual-registration" && origin != expectedOrigin &&
                 !(origin == "folder-import" && kind == "isolde-managed-install")) ||
                !archiveInteger(object.value("recordedAt"), &recordedAt) || recordedAt == 0 ||
                provenanceIds.contains(entityId) || (knownGame && entityType != "game") ||
                (knownEdition && entityType != "edition"))
                return fail(tr("Acquisition provenance is inconsistent or duplicated in backup."));
            provenanceIds.insert(entityId);
            provenanceData.append({{"entityId", entityId},
                                   {"entityType", entityType},
                                   {"sourceKind", kind},
                                   {"origin", origin},
                                   {"sourceReference", object.value("sourceReference").toString()},
                                   {"sourceFingerprint", object.value("sourceFingerprint").toString()},
                                   {"recordedAt", recordedAt}});
        }
        if (!ensureProvenanceSchema())
            return false;
        for (const auto& record : provenanceData)
        {
            QSqlQuery existing(m_db);
            existing.prepare(
                "SELECT entity_type,source_kind,origin,source_reference,source_fingerprint,recorded_at "
                "FROM game_provenance WHERE entity_id=?");
            existing.addBindValue(record.value("entityId"));
            if (!existing.exec())
                return fail(tr("Could not verify existing immutable provenance."));
            if (existing.next())
            {
                for (int index = 0; index < 6; ++index)
                {
                    const auto key =
                        QStringList{
                            "entityType", "sourceKind", "origin", "sourceReference", "sourceFingerprint", "recordedAt"}
                            .at(index);
                    if (existing.value(index).toString() != record.value(key).toString())
                        return fail(tr(
                            "Backup conflicts with immutable acquisition provenance. No catalog data was replaced."));
                }
            }
        }
    }
    if (!ensureArtworkSchema())
        return false;
    for (auto& game : games)
    {
        const auto bytes = game.value("artworkBytes").toByteArray();
        if (bytes.isEmpty())
            continue;
        QString hash, error;
        if (!persistArtwork(m_db.databaseName(), bytes, &hash, &error))
            return fail(tr("Backup artwork could not be staged. Nothing in the catalog was replaced. %1").arg(error));
        game["artworkHash"] = hash;
    }
    const auto backupPath =
        m_db.databaseName() + ".pre-restore-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite";
    if (QFileInfo::exists(backupPath) || QFileInfo(backupPath).isSymLink())
        return fail(tr("Cannot create a private recovery backup before restore."));
    QSqlQuery backup(m_db);
    backup.prepare("VACUUM main INTO ?");
    backup.addBindValue(backupPath);
    if (!backup.exec())
        return fail(tr("Could not preserve the current catalog before restore. Nothing was replaced."));
    if (!QFile::setPermissions(backupPath, QFile::ReadOwner | QFile::WriteOwner))
    {
        QFile::remove(backupPath);
        return fail(tr("Could not secure the private recovery copy. Nothing was replaced."));
    }
    if (!m_db.transaction())
        return fail(m_db.lastError().text());
    QSqlQuery query(m_db);
    bool ok = query.exec("DELETE FROM game_collection_members") && query.exec("DELETE FROM game_collections") &&
              query.exec("DELETE FROM game_activity") && query.exec("DELETE FROM game_metadata");
    if (m_hasEditions)
        ok = ok && query.exec("DELETE FROM game_editions");
    ok = ok && query.exec("DELETE FROM games");
    const bool needEditionTable = !editionsData.isEmpty() && !m_hasEditions;
    if (ok && needEditionTable)
        ok = query.exec(
                 "CREATE TABLE game_editions(id TEXT PRIMARY KEY,game_id TEXT NOT NULL REFERENCES games(id) ON "
                 "DELETE CASCADE,title TEXT NOT NULL,platform TEXT NOT NULL,runtime TEXT NOT NULL,root TEXT NOT "
                 "NULL,target TEXT NOT NULL,UNIQUE(runtime,root,target))") &&
             query.exec("CREATE INDEX game_editions_owner ON game_editions(game_id)");
    for (const auto& game : games)
    {
        if (!ok)
            break;
        ok = restoreCatalogApplyGame(query, game);
    }
    for (const auto& edition : editionsData)
    {
        if (!ok)
            break;
        ok = restoreCatalogApplyEdition(query, edition);
    }
    for (const auto& collection : collectionsData)
    {
        if (!ok)
            break;
        ok = restoreCatalogApplyCollection(query, collection);
    }
    for (const auto& record : provenanceData)
    {
        if (!ok)
            break;
        ok = restoreCatalogApplyProvenanceRecord(query, record);
    }
    ok = restoreCatalogApplyFinal(query, ok, m_hasProvenance, needEditionTable, backupPath);
    if (!ok || !m_db.commit())
    {
        const auto reason = query.lastError().text();
        m_db.rollback();
        return fail(
            tr("Restore failed; the prior catalog remains intact. Recovery copy: %1. %2").arg(backupPath, reason));
    }
    m_hasActivity = true;
    m_hasMetadata = true;
    m_hasCollections = true;
    m_hasRichMetadata = true;
    m_hasArtwork = true;
    m_hasEditions = m_hasEditions || needEditionTable;
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool
GameLibrary::restoreCatalogApplyGame(QSqlQuery& query, const QVariantMap& game)
{
    query.prepare(
        "INSERT INTO games(id,title,platform,runtime,root,target,favorite,added) VALUES(?,?,?,?,?,?,?,?)");
    for (const auto& key :
         QStringList{"id", "title", "platform", "runtimeId", "root", "target", "favorite", "added"})
        query.addBindValue(game.value(key));
    if (!query.exec())
        return false;
    query.prepare(
        "INSERT INTO game_metadata(game_id,franchise,tags,aliases,summary,artwork_hash) VALUES(?,?,?,?,?,?)");
    query.addBindValue(game.value("id"));
    QJsonArray tags;
    for (const auto& tag : game.value("tags").toStringList())
        tags.append(tag);
    QJsonArray aliases;
    for (const auto& alias : game.value("aliases").toStringList())
        aliases.append(alias);
    query.addBindValue(QJsonDocument(tags).toJson(QJsonDocument::Compact));
    query.addBindValue(QJsonDocument(aliases).toJson(QJsonDocument::Compact));
    query.addBindValue(sqlText(game.value("summary")));
    query.addBindValue(sqlText(game.value("artworkHash")));
    if (!query.exec())
        return false;
    if (game.value("lastPlayed").toLongLong() > 0)
    {
        query.prepare("INSERT INTO game_activity(game_id,last_played) VALUES(?,?)");
        query.addBindValue(game.value("id"));
        query.addBindValue(game.value("lastPlayed"));
        if (!query.exec())
            return false;
    }
    return true;
}

bool
GameLibrary::restoreCatalogApplyEdition(QSqlQuery& query, const QVariantMap& edition)
{
    query.prepare("INSERT INTO game_editions(id,game_id,title,platform,runtime,root,target) VALUES(?,?,?,?,?,?,?)");
    for (const auto& key : QStringList{"id", "gameId", "title", "platform", "runtimeId", "root", "target"})
        query.addBindValue(edition.value(key));
    return query.exec();
}

bool
GameLibrary::restoreCatalogApplyCollection(QSqlQuery& query, const QVariantMap& collection)
{
    query.prepare("INSERT INTO game_collections(id,name,kind,filter_type,filter_value) VALUES(?,?,?,?,?)");
    for (const auto& key : QStringList{"id", "name", "kind", "filterType", "filterValue"})
        query.addBindValue(collection.value(key));
    if (!query.exec())
        return false;
    const auto members = collection.value("members").toList();
    for (qsizetype position = 0; position < members.size(); ++position)
    {
        query.prepare("INSERT INTO game_collection_members(collection_id,game_id,position) VALUES(?,?,?)");
        query.addBindValue(collection.value("id"));
        query.addBindValue(members.at(position));
        query.addBindValue(position);
        if (!query.exec())
            return false;
    }
    return true;
}

bool
GameLibrary::restoreCatalogApplyProvenanceRecord(QSqlQuery& query, const QVariantMap& record)
{
    query.prepare(
        "INSERT OR IGNORE INTO "
        "game_provenance(entity_id,entity_type,source_kind,origin,source_reference,source_fingerprint,"
        "recorded_at) VALUES(?,?,?,?,?,?,?)");
    for (const auto& key : QStringList{"entityId",
                                       "entityType",
                                       "sourceKind",
                                       "origin",
                                       "sourceReference",
                                       "sourceFingerprint",
                                       "recordedAt"})
        query.addBindValue(record.value(key));
    return query.exec();
}

bool
GameLibrary::restoreCatalogApplyFinal(QSqlQuery& query, bool ok, bool hasProvenance, bool needEditionTable,
                                      const QString& backupPath)
{
    if (ok && hasProvenance)
    {
        if (!query.exec("SELECT count(*) FROM game_provenance") || !query.next() ||
            query.value(0).toInt() > MaxProvenanceRecords)
            ok = false;
    }
    ok = ok && query.exec("PRAGMA user_version=6");
    if (!ok || !query.exec("COMMIT"))
    {
        const auto reason = query.lastError().text();
        query.exec("ROLLBACK");
        return fail(
            tr("Restore failed; the prior catalog remains intact. Recovery copy: %1. %2").arg(backupPath, reason));
    }
    m_hasActivity = true;
    m_hasMetadata = true;
    m_hasCollections = true;
    m_hasRichMetadata = true;
    m_hasArtwork = true;
    m_hasEditions = m_hasEditions || needEditionTable;
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}
