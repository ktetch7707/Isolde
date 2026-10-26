#include "GuideStore.h"
#include "GuideRules.h"
#include "home/ComponentSchema.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
constexpr int ApplicationId=0x49534744;
constexpr int MaxChannels=1024, MaxCandidates=10000, MaxSlotsPerChannel=100000;
constexpr qint64 Day=86400, MaxTime=253402214400LL;
QByteArray json(const QVariant &v) { return QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact); }
QVariantMap object(const QVariant &v) { return QJsonDocument::fromJson(v.toByteArray()).toVariant().toMap(); }
QVariantList array(const QVariant &v) { return QJsonDocument::fromJson(v.toByteArray()).toVariant().toList(); }
bool instant(qint64 v) { return v>=0 && v<=MaxTime; }
bool number(const QVariant &v, qint64 low, qint64 high) {
    const auto type=v.metaType().id();
    if (type!=QMetaType::Int && type!=QMetaType::LongLong && type!=QMetaType::UInt
        && type!=QMetaType::ULongLong && type!=QMetaType::Double) return false;
    bool ok=false; const auto n=v.toDouble(&ok);
    return ok && std::isfinite(n) && n==std::floor(n) && n>=low && n<=high;
}
QVariantMap channelRow(const QSqlQuery &q) {
    auto v=object(q.value(0));
    v.insert("revision",q.value(1)); v.insert("hidden",q.value(2).toBool());
    v.insert("favorite",q.value(3).toBool()); v.insert("position",q.value(4));
    v.insert("anchorUtc",q.value(5)); v.insert("clockUtc",q.value(6)); v.insert("active",q.value(7).toBool());
    return v;
}
const QString ChannelSelect="SELECT definition,revision,hidden,favorite,position,anchor_utc,clock_utc,active FROM channels";
QString mediaKey(const QVariantMap &item) { return item.value("mediaType").toString()+":"+QString::number(item.value("mediaId").toInt()); }
}

GuideStore::GuideStore(const QString &path, QObject *parent)
    : QObject(parent), m_connection("guide-"+QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    if (!QDir::isAbsolutePath(path) || QFileInfo(path).isSymLink()) { fail(tr("Choose a private absolute guide path.")); return; }
    // Refuse ancestor symlinks too: a profile path must not escape through a
    // redirect into another profile or a mounted personal data directory.
    for (auto dir=QFileInfo(path).absoluteDir();;) {
        if (QFileInfo(dir.absolutePath()).isSymLink()) { fail(tr("The guide folder must not contain symbolic links.")); return; }
        if (dir.isRoot()) break;
        if (!dir.cdUp()) break;
    }
    const bool exists=QFileInfo::exists(path);
    if (!exists && !QDir().mkpath(QFileInfo(path).absolutePath())) { fail(tr("Cannot create the guide folder.")); return; }
    m_db=QSqlDatabase::addDatabase("QSQLITE",m_connection);
    m_db.setDatabaseName(path); m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
    if (!m_db.open()) { fail(tr("Cannot open this profile's guide: %1").arg(m_db.lastError().text())); return; }
    QSqlQuery q(m_db);
    int version=-1, owner=-1;
    if (q.exec("PRAGMA user_version") && q.next()) version=q.value(0).toInt(); q.finish();
    if (q.exec("PRAGMA application_id") && q.next()) owner=q.value(0).toInt(); q.finish();
    // Check identity before any journal/schema mutation. Unknown documents
    // are not repaired, migrated, replaced or silently reset on read.
    if (exists && (version!=1 || owner!=ApplicationId)) { fail(tr("Unsupported guide database. Original data was preserved.")); return; }
    if (!q.exec("PRAGMA foreign_keys=ON") || !q.exec("PRAGMA synchronous=FULL")
        || !q.exec("PRAGMA journal_mode=WAL")) { fail(tr("Cannot enable durable guide storage.")); return; }
    q.finish();
    if (!exists) {
        if (!m_db.transaction()) { fail(m_db.lastError().text()); return; }
        const QStringList schema{
            "CREATE TABLE channels (id TEXT PRIMARY KEY, number INTEGER NOT NULL UNIQUE CHECK(number BETWEEN 1 AND 99999), definition BLOB NOT NULL, candidates BLOB NOT NULL, cursor INTEGER NOT NULL DEFAULT 0 CHECK(cursor>=0), revision INTEGER NOT NULL DEFAULT 1, hidden INTEGER NOT NULL DEFAULT 0 CHECK(hidden IN(0,1)), favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN(0,1)), position INTEGER NOT NULL, anchor_utc INTEGER NOT NULL, clock_utc INTEGER NOT NULL, active INTEGER NOT NULL CHECK(active IN(0,1)))",
            "CREATE TABLE slots (id TEXT PRIMARY KEY, channel TEXT NOT NULL REFERENCES channels(id) ON DELETE CASCADE, media_type TEXT NOT NULL, media_id INTEGER NOT NULL CHECK(media_id>0), title TEXT NOT NULL, start_utc INTEGER NOT NULL, end_utc INTEGER NOT NULL CHECK(end_utc>start_utc), status TEXT NOT NULL DEFAULT 'ready' CHECK(status IN('ready','unavailable')), UNIQUE(channel,start_utc))",
            "CREATE INDEX slot_window ON slots(channel,end_utc,start_utc)",
            "CREATE TRIGGER slot_insert_overlap BEFORE INSERT ON slots WHEN EXISTS(SELECT 1 FROM slots WHERE channel=NEW.channel AND start_utc<NEW.end_utc AND end_utc>NEW.start_utc) BEGIN SELECT RAISE(ABORT,'overlapping schedule interval'); END",
            "CREATE TRIGGER slot_update_overlap BEFORE UPDATE OF channel,start_utc,end_utc ON slots WHEN EXISTS(SELECT 1 FROM slots WHERE channel=NEW.channel AND id<>OLD.id AND start_utc<NEW.end_utc AND end_utc>NEW.start_utc) BEGIN SELECT RAISE(ABORT,'overlapping schedule interval'); END",
            QString("PRAGMA application_id=%1").arg(ApplicationId), "PRAGMA user_version=1"};
        for (const auto &statement:schema) if (!q.exec(statement)) {
            m_db.rollback(); fail(tr("Cannot initialize guide storage: %1").arg(q.lastError().text())); return;
        }
        if (!m_db.commit()) { m_db.rollback(); fail(tr("Cannot commit guide initialization.")); return; }
    }
    if (!q.exec(ChannelSelect+" LIMIT 0") || !q.exec("SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots LIMIT 0")) {
        fail(tr("Invalid guide database. Original data was preserved.")); return;
    }
    m_ready=true;
}
GuideStore::~GuideStore() {
    m_db.close(); m_db=QSqlDatabase(); QSqlDatabase::removeDatabase(m_connection);
}
bool GuideStore::fail(const QString &message) { m_error=message; emit errorChanged(); return false; }
bool GuideStore::commit() {
    if (m_cancelled && m_cancelled()) { m_db.rollback(); return fail(tr("Cancelled. Previous channels and schedules were preserved.")); }
    if (m_failCommit) {
        m_failCommit=false; m_db.rollback(); return fail(tr("Guide save failed. Previous channels and schedules were preserved."));
    }
    if (!m_db.commit()) { m_db.rollback(); return fail(tr("Guide save failed: %1").arg(m_db.lastError().text())); }
    m_error.clear(); emit errorChanged(); emit changed(); return true;
}
void GuideStore::invalidate() { m_ready=false; m_db.close(); fail(tr("This guide profile is no longer active.")); emit changed(); }
QVariantList GuideStore::channels() const {
    QVariantList result; if (!m_ready) return result;
    QSqlQuery q(m_db); if (!q.exec(ChannelSelect+" ORDER BY position,id LIMIT 1024")) return result;
    while (q.next()) result.append(channelRow(q));
    return result;
}
QVariantMap GuideStore::channel(const QString &id) const {
    if (!m_ready || !ComponentSchema::safeId(id)) return {};
    QSqlQuery q(m_db); q.prepare(ChannelSelect+" WHERE id=?"); q.addBindValue(id);
    return q.exec() && q.next() ? channelRow(q) : QVariantMap();
}
QVariantList GuideStore::eligible(const QVariantMap &definition, const QVariantList &candidates, QString *error) const {
    QVariantList result;
    if (candidates.isEmpty() || candidates.size()>MaxCandidates || json(candidates).size()>4*1024*1024) {
        *error=tr("Choose between 1 and 10,000 Kodi items within the 4 MiB channel snapshot limit."); return {};
    }
    QSet<QString> seen;
    const QSet<QString> fields{"mediaType","mediaId","title","duration","showtitle","studio","network","genre","tag","year","rating",
        "playcount","language","actor","director","source","path","season","episode"};
    for (const auto &v:candidates) {
        if (v.metaType().id()!=QMetaType::QVariantMap) { *error=tr("Invalid Kodi channel item."); return {}; }
        const auto item=v.toMap(); const auto kind=item.value("mediaType").toString();
        for (auto it=item.cbegin();it!=item.cend();++it) if (!fields.contains(it.key())) { *error=tr("Unknown channel metadata field."); return {}; }
        if (!QStringList{"movie","episode","musicvideo","song"}.contains(kind) || !number(item.value("mediaId"),1,2147483647)
            || !number(item.value("duration"),1,Day) || item.value("title").metaType().id()!=QMetaType::QString
            || item.value("title").toString().trimmed().isEmpty() || item.value("title").toString().size()>512
            || json(item).size()>16384 || seen.contains(mediaKey(item))) {
            *error=tr("Each channel item needs a unique Kodi identity, title and known positive duration (at most one day)."); return {};
        }
        seen.insert(mediaKey(item));
        if ((!definition.contains("mediaKinds") || definition.value("mediaKinds").toList().contains(kind))
            && GuideRules::matches(definition.value("rule").toMap(),item)) result.append(item);
    }
    if (result.isEmpty()) *error=tr("No media matches these channel rules. Adjust the rules before saving.");
    return result;
}
bool GuideStore::createLocal(const QVariantMap &definition, const QVariantList &candidates, qint64 now, int days) {
    if (!m_ready) return fail(tr("This guide profile is unavailable."));
    QString error; const auto valid=ComponentSchema::guideChannel(definition,&error);
    if (valid.isEmpty()) return fail(error);
    if (!instant(now) || days<1 || days>31 || !instant(now+days*Day)) return fail(tr("Choose a guide horizon of 1–31 days and a valid clock."));
    const auto selected=eligible(valid,candidates,&error); if (selected.isEmpty()) return fail(error);
    if (!m_db.transaction()) return fail(tr("The guide is busy. Retry the save."));
    QSqlQuery q(m_db);
    if (!q.exec("SELECT COUNT(*) FROM channels") || !q.next() || q.value(0).toInt()>=MaxChannels) {
        m_db.rollback(); return fail(tr("The guide supports at most 1,024 channels."));
    }
    q.prepare("INSERT INTO channels(id,number,definition,candidates,position,anchor_utc,clock_utc,active) VALUES(?,?,?,?,(SELECT COALESCE(MAX(position)+1,0) FROM channels),?,?,1)");
    q.addBindValue(valid.value("id")); q.addBindValue(valid.value("number")); q.addBindValue(json(valid)); q.addBindValue(json(selected));
    q.addBindValue(now); q.addBindValue(now);
    if (!q.exec()) { m_db.rollback(); return fail(tr("Channel ID or number is already in use, or storage is unavailable. Choose another number or retry.")); }
    if (!appendSlots(valid.value("id").toString(),now+days*Day)) { m_db.rollback(); return false; }
    return commit();
}
bool GuideStore::appendSlots(const QString &id, qint64 until) {
    QSqlQuery q(m_db); q.prepare("SELECT definition,candidates,cursor,clock_utc FROM channels WHERE id=?"); q.addBindValue(id);
    if (!q.exec() || !q.next()) return fail(tr("Channel no longer exists."));
    const auto def=object(q.value(0)); auto items=array(q.value(1)); qint64 cursor=q.value(2).toLongLong(), start=q.value(3).toLongLong(); q.finish();
    if (ComponentSchema::guideChannel(def).isEmpty() || items.isEmpty()) return fail(tr("Channel data needs repair before extending its schedule."));
    q.prepare("SELECT COUNT(*),MAX(end_utc) FROM slots WHERE channel=?"); q.addBindValue(id);
    if (!q.exec() || !q.next()) return fail(tr("Cannot read the committed schedule."));
    int count=q.value(0).toInt(); if (!q.value(1).isNull()) start=q.value(1).toLongLong(); q.finish();
    if (until<=start) return true;
    if (until-start>31*Day) return fail(tr("Extend the schedule by at most 31 days at a time."));
    // Sort each cycle by a keyed hash, independent of platform PRNGs and
    // QVariant/hash iteration order. Already committed slots never change.
    const auto sequence=def.value("sequence").toString();
    std::sort(items.begin(),items.end(),[](const QVariant &a,const QVariant &b) {
        const auto x=a.toMap(),y=b.toMap();
        const auto key=[](const QVariantMap &i) {
            return QStringList{i.value("showtitle").toString(),QString::number(i.value("season").toInt()).rightJustified(8,'0'),
                QString::number(i.value("episode").toInt()).rightJustified(8,'0'),mediaKey(i)}.join(QChar(0x1f));
        };
        return key(x)<key(y);
    });
    QVariantList ordered; qint64 currentCycle=-1;
    QSqlQuery insert(m_db); insert.prepare("INSERT INTO slots(id,channel,media_type,media_id,title,start_utc,end_utc) VALUES(?,?,?,?,?,?,?)");
    while (start<until) {
        if (m_cancelled && m_cancelled()) return fail(tr("Cancelled. Previous channels and schedules were preserved."));
        if (++count>MaxSlotsPerChannel || cursor>=1000000000LL) return fail(tr("The channel schedule limit was reached. Shorten the horizon or remove expired history."));
        const qint64 cycle=cursor/items.size();
        if (currentCycle!=cycle) {
            currentCycle=cycle; ordered=items;
            if (sequence=="seeded-shuffle") {
                const auto salt=QByteArray::number(def.value("seed").toInt())+":"+QByteArray::number(cycle)+":";
                std::sort(ordered.begin(),ordered.end(),[salt](const QVariant &a,const QVariant &b) {
                    const auto digest=[salt](const QVariant &v) { return QCryptographicHash::hash(salt+mediaKey(v.toMap()).toUtf8(),QCryptographicHash::Sha256); };
                    const auto x=digest(a),y=digest(b); return x==y ? mediaKey(a.toMap())<mediaKey(b.toMap()) : x<y;
                });
            } else if (sequence=="rotation") {
                // Round-robin across shows; each show's episodes retain order.
                QMap<QString,QVariantList> shows;
                for (const auto &v:items) { const auto m=v.toMap(); const auto show=m.value("showtitle").toString(); shows[show.isEmpty()?mediaKey(m):show].append(v); }
                ordered.clear();
                for (int index=0;ordered.size()<items.size();++index)
                    for (auto it=shows.cbegin();it!=shows.cend();++it) if (index<it.value().size()) ordered.append(it.value()[index]);
            }
        }
        const auto item=ordered[cursor%ordered.size()].toMap(); const auto duration=item.value("duration").toLongLong();
        if (duration<=0 || duration>Day || !instant(start+duration)) return fail(tr("Invalid scheduled duration."));
        const auto slotId=QString::fromLatin1(QCryptographicHash::hash(id.toUtf8()+":"+QByteArray::number(start)+":"+QByteArray::number(cursor),QCryptographicHash::Sha256).toHex());
        insert.bindValue(0,slotId); insert.bindValue(1,id); insert.bindValue(2,item.value("mediaType")); insert.bindValue(3,item.value("mediaId"));
        insert.bindValue(4,item.value("title")); insert.bindValue(5,start); insert.bindValue(6,start+duration);
        if (!insert.exec()) return fail(tr("Cannot extend the guide: %1").arg(insert.lastError().text()));
        start+=duration; ++cursor;
    }
    q.prepare("UPDATE channels SET cursor=? WHERE id=?"); q.addBindValue(cursor); q.addBindValue(id);
    return q.exec() || fail(tr("Cannot save schedule continuity."));
}
bool GuideStore::extend(const QString &id, qint64 until, int revision) {
    if (!m_ready || !instant(until) || !ComponentSchema::safeId(id)) return fail(tr("Invalid guide extension."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    const auto state=channel(id);
    if (state.isEmpty() || state.value("revision").toInt()!=revision) { m_db.rollback(); return fail(tr("Channel changed. Refresh before extending its schedule.")); }
    if (!appendSlots(id,until)) { m_db.rollback(); return false; }
    return commit();
}
QVariantList GuideStore::programs(const QString &id, qint64 from, qint64 to, int limit) const {
    QVariantList result;
    if (!m_ready || !ComponentSchema::safeId(id) || !instant(from) || !instant(to) || to<=from || to-from>31*Day || limit<1 || limit>1000) return result;
    QSqlQuery q(m_db); q.prepare("SELECT id,media_type,media_id,title,start_utc,end_utc,status FROM slots WHERE channel=? AND start_utc<? AND end_utc>? ORDER BY start_utc LIMIT ?");
    q.addBindValue(id); q.addBindValue(to); q.addBindValue(from); q.addBindValue(limit);
    if (q.exec()) while (q.next()) result.append(QVariantMap{{"id",q.value(0)},{"channelId",id},{"mediaType",q.value(1)},
        {"mediaId",q.value(2)},{"title",q.value(3)},{"startUtc",q.value(4)},{"endUtc",q.value(5)},{"status",q.value(6)}});
    return result;
}
qint64 GuideStore::clockTime(const QVariantMap &state, qint64 now) const {
    if (state.value("clock")=="real-time") return now;
    return state.value("clockUtc").toLongLong()+(state.value("active").toBool()?std::max<qint64>(0,now-state.value("anchorUtc").toLongLong()):0);
}
QVariantMap GuideStore::tuneTarget(const QString &id, qint64 now) const {
    const auto state=channel(id); if (state.isEmpty() || !instant(now) || state.value("hidden").toBool()) return {};
    const auto time=clockTime(state,now); const auto list=programs(id,time,time+1,1);
    if (list.isEmpty()) return {};
    auto target=list.first().toMap(); if (target.value("status")!="ready") return {};
    target.insert("offsetSeconds",time-target.value("startUtc").toLongLong());
    target.insert("clockUtc",time); target.insert("launchKind","kodi_media"); return target;
}
bool GuideStore::updateRules(const QVariantMap &definition, const QVariantList &candidates, int revision) {
    if (!m_ready) return fail(tr("This guide profile is unavailable."));
    QString error; const auto def=ComponentSchema::guideChannel(definition,&error); if (def.isEmpty()) return fail(error);
    const auto selected=eligible(def,candidates,&error); if (selected.isEmpty()) return fail(error);
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    const auto state=channel(def.value("id").toString());
    if (state.isEmpty() || state.value("revision").toInt()!=revision || state.value("clock")!=def.value("clock") || state.value("timezone")!=def.value("timezone")) {
        m_db.rollback(); return fail(tr("Channel changed, or its broadcast clock needs a separate preview. Refresh before saving."));
    }
    QSqlQuery q(m_db); q.prepare("UPDATE channels SET number=?,definition=?,candidates=?,revision=revision+1,cursor=0 WHERE id=? AND revision=?");
    q.addBindValue(def.value("number")); q.addBindValue(json(def)); q.addBindValue(json(selected)); q.addBindValue(def.value("id")); q.addBindValue(revision);
    if (!q.exec() || q.numRowsAffected()!=1) { m_db.rollback(); return fail(tr("Channel number conflicts or the channel changed. Existing slots were preserved.")); }
    return commit();
}
bool GuideStore::setActive(bool active, qint64 now) {
    if (!m_ready || !instant(now)) return fail(tr("Invalid channel clock checkpoint."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    const auto all=channels();
    QSqlQuery q(m_db); q.prepare("UPDATE channels SET anchor_utc=?,clock_utc=?,active=? WHERE id=?");
    for (const auto &v:all) {
        const auto state=v.toMap(); if (state.value("clock")!="pause-when-off") continue;
        // Reject a backward jump rather than silently freezing/rewinding time.
        if (now<state.value("anchorUtc").toLongLong()) { m_db.rollback(); return fail(tr("The system clock moved backwards. Correct the clock before resuming channels.")); }
        q.bindValue(0,now); q.bindValue(1,clockTime(state,now)); q.bindValue(2,active?1:0); q.bindValue(3,state.value("id"));
        if (!q.exec()) { m_db.rollback(); return fail(tr("Cannot save the broadcast clock.")); }
    }
    return commit();
}
bool GuideStore::setPreferences(const QString &id, const QVariantMap &prefs, int revision) {
    if (!m_ready || !ComponentSchema::safeId(id) || prefs.isEmpty()) return fail(tr("Invalid channel preferences."));
    for (auto it=prefs.cbegin();it!=prefs.cend();++it)
        if ((it.key()!="hidden" && it.key()!="favorite") || it.value().metaType().id()!=QMetaType::Bool) return fail(tr("Unknown channel preference."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    const auto state=channel(id); if (state.isEmpty() || state.value("revision").toInt()!=revision) { m_db.rollback(); return fail(tr("Channel changed. Refresh before saving.")); }
    QSqlQuery q(m_db); q.prepare("UPDATE channels SET hidden=?,favorite=?,revision=revision+1 WHERE id=? AND revision=?");
    q.addBindValue(prefs.value("hidden",state.value("hidden")).toBool()?1:0); q.addBindValue(prefs.value("favorite",state.value("favorite")).toBool()?1:0);
    q.addBindValue(id); q.addBindValue(revision);
    if (!q.exec() || q.numRowsAffected()!=1) { m_db.rollback(); return fail(tr("Cannot save channel preferences.")); }
    return commit();
}
bool GuideStore::setMetadata(const QString &id,const QVariantMap &metadata,int revision) {
    if(!m_ready || !ComponentSchema::safeId(id) || metadata.isEmpty())return fail(tr("Invalid channel metadata."));
    for(auto it=metadata.cbegin();it!=metadata.cend();++it)if(it.key()!="name" && it.key()!="number")return fail(tr("Unknown channel metadata."));
    if(!m_db.transaction())return fail(tr("The guide is busy."));
    const auto state=channel(id);if(state.isEmpty() || state.value("revision").toInt()!=revision){m_db.rollback();return fail(tr("Channel changed. Refresh before saving."));}
    QSqlQuery q(m_db);q.prepare("SELECT definition FROM channels WHERE id=?");q.addBindValue(id);
    if(!q.exec() || !q.next()){m_db.rollback();return fail(tr("Cannot read channel metadata."));}
    auto definition=object(q.value(0));q.finish();for(auto it=metadata.cbegin();it!=metadata.cend();++it)definition[it.key()]=it.value();
    QString error;if(ComponentSchema::guideChannel(definition,&error).isEmpty()){m_db.rollback();return fail(error);}
    q.prepare("UPDATE channels SET number=?,definition=?,revision=revision+1 WHERE id=? AND revision=?");q.addBindValue(definition.value("number"));q.addBindValue(json(definition));q.addBindValue(id);q.addBindValue(revision);
    if(!q.exec() || q.numRowsAffected()!=1){m_db.rollback();return fail(tr("Channel number is already in use or storage is unavailable. Choose another number; nothing was changed."));}
    return commit();
}
bool GuideStore::reorder(const QStringList &ids, const QVariantMap &revisions) {
    if (!m_ready || ids.isEmpty() || ids.size()>MaxChannels) return fail(tr("Invalid channel order."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    QSet<QString> expected;
    for (const auto &v:channels()) {
        const auto state=v.toMap(); const auto id=state.value("id").toString(); expected.insert(id);
        if (!number(revisions.value(id),1,2147483647) || revisions.value(id).toInt()!=state.value("revision").toInt()) {
            m_db.rollback(); return fail(tr("Channel lineup changed. Refresh the move preview."));
        }
    }
    QSet<QString> supplied; for (const auto &id:ids) supplied.insert(id);
    if (expected!=supplied || supplied.size()!=ids.size() || revisions.size()!=ids.size()) { m_db.rollback(); return fail(tr("Channel lineup changed. Refresh the move preview.")); }
    QSqlQuery q(m_db); q.prepare("UPDATE channels SET position=?,revision=revision+1 WHERE id=?");
    for (int i=0;i<ids.size();++i) { q.bindValue(0,i); q.bindValue(1,ids[i]); if (!q.exec()) { m_db.rollback(); return fail(tr("Cannot save channel order.")); } }
    return commit();
}
qint64 GuideStore::broadcastTime(const QString &id, qint64 now) const {
    const auto state=channel(id); return state.isEmpty() || !instant(now) ? -1 : clockTime(state,now);
}
bool GuideStore::resumeClocks(qint64 now) {
    if (!m_ready || !instant(now)) return fail(tr("Invalid broadcast clock."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    // On restart, a virtual channel resumes from its last durable checkpoint.
    // A previously active flag indicates an interrupted shutdown, not a reason
    // to advance a pause-when-off channel through the whole outage.
    QSqlQuery q(m_db); q.prepare("UPDATE channels SET anchor_utc=?,active=1 WHERE id=?");
    for (const auto &v:channels()) {
        const auto channel=v.toMap();if(channel.value("clock")!="pause-when-off")continue;
        q.bindValue(0,now);q.bindValue(1,channel.value("id"));
        if (!q.exec()) { m_db.rollback(); return fail(tr("Cannot resume the saved broadcast clocks.")); }
    }
    return commit();
}
bool GuideStore::remove(const QString &id, int revision) {
    if (!m_ready || !ComponentSchema::safeId(id)) return fail(tr("Invalid channel."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    QSqlQuery q(m_db); q.prepare("DELETE FROM channels WHERE id=? AND revision=?"); q.addBindValue(id); q.addBindValue(revision);
    if (!q.exec() || q.numRowsAffected()!=1) { m_db.rollback(); return fail(tr("Channel changed. Refresh before removing.")); }
    return commit();
}
bool GuideStore::markUnavailable(const QString &kind, int mediaId) {
    if (!m_ready || !QStringList{"movie","episode","musicvideo","song"}.contains(kind) || mediaId<1) return fail(tr("Invalid missing-media identity."));
    if (!m_db.transaction()) return fail(tr("The guide is busy."));
    QSqlQuery q(m_db); q.prepare("UPDATE slots SET status='unavailable' WHERE media_type=? AND media_id=?"); q.addBindValue(kind); q.addBindValue(mediaId);
    if (!q.exec()) { m_db.rollback(); return fail(tr("Cannot mark the affected slots unavailable.")); }
    q.finish();
    // Invalidate only matching candidate snapshots as well. Otherwise the
    // next horizon extension would schedule the removed episode again.
    QVariantList updates;
    if (!q.exec("SELECT id,candidates FROM channels")) { m_db.rollback(); return fail(tr("Cannot update affected channel candidates.")); }
    while (q.next()) {
        const auto original=array(q.value(1)); QVariantList retained;
        for (const auto &v:original) {
            const auto item=v.toMap();
            if (item.value("mediaType")!=kind || item.value("mediaId").toInt()!=mediaId) retained.append(v);
        }
        if (retained.size()!=original.size()) updates.append(QVariantMap{{"id",q.value(0)},{"candidates",retained}});
    }
    q.finish(); q.prepare("UPDATE channels SET candidates=?,revision=revision+1,cursor=0 WHERE id=?");
    for (const auto &v:updates) {
        const auto update=v.toMap(); q.bindValue(0,json(update.value("candidates"))); q.bindValue(1,update.value("id"));
        if (!q.exec()) { m_db.rollback(); return fail(tr("Cannot save the affected channel candidates.")); }
    }
    return commit();
}
