#include "GuideController.h"
#include "GuideStore.h"
#include "GuideRules.h"
#include "GuideFeeds.h"
#include "GuideAssets.h"
#include "GuideClock.h"
#include "GuidePlaylists.h"
#include "GuideAtomicFile.h"
#include "home/ComponentSchema.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QTimeZone>
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QDebug>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif

class GuideWorker final : public QObject {
    Q_OBJECT
public:
    explicit GuideWorker(QString path):path(std::move(path)) {}
    QString path;
    std::unique_ptr<GuideStore> store;
signals:
    void completed(quint64 id,const QVariantMap &result);
};
namespace {
qint64 now() { return QDateTime::currentSecsSinceEpoch(); }
bool integer(const QVariant &value,qint64 minimum,qint64 maximum) {
    const auto type=value.metaType().id();
    if(type!=QMetaType::Int && type!=QMetaType::UInt && type!=QMetaType::LongLong && type!=QMetaType::ULongLong && type!=QMetaType::Double)return false;
    bool ok=false;const auto number=value.toDouble(&ok);
    return ok && std::isfinite(number) && std::floor(number)==number && number>=minimum && number<=maximum;
}
QVariantMap outcome(bool ok,GuideStore &store) { return {{"ok",ok},{"error",ok?QString():store.errorString()}}; }
bool portablePath(const QString &path) {
    for(const auto c:path)if(c.unicode()<32 || c.unicode()==127)return false;
    if(!QDir::isAbsolutePath(path) || path.size()>4096 || (!path.endsWith(".json",Qt::CaseInsensitive) && !path.endsWith(".xsp",Qt::CaseInsensitive)) || QFileInfo(path).isSymLink())return false;
    for(auto dir=QFileInfo(path).absoluteDir();;){if(QFileInfo(dir.absolutePath()).isSymLink())return false;if(dir.isRoot())break;if(!dir.cdUp())return false;}
    return true;
}
QVariantMap cachedCurrent(GuideStore &store,const QString &id,qint64 wall) {
    auto current=store.tuneTarget(id,wall);if(!current.isEmpty())return current;
    // Presentation retains an unavailable listing even when execution cannot
    // produce a tune target. This is a bounded cached read, never a media scan.
    const auto clock=store.broadcastTime(id,wall);const auto programs=store.programs(id,clock,clock+1,1);
    if(programs.isEmpty())return {};
    current=programs.first().toMap();current["clockUtc"]=clock;current["offsetSeconds"]=clock-current.value("startUtc").toLongLong();
    current["displayStartUtc"]=current.value("startUtc").toLongLong()-clock+wall;
    current["displayEndUtc"]=current.value("endUtc").toLongLong()-clock+wall;
    current["status"]="unavailable";return current;
}
QVariantMap snapshot(GuideStore &store,qint64 from,qint64 to,int first) {
    QVariantList rows; const auto channels=store.channels(); int visible=0;
    for (const auto &v:channels) {
        auto channel=v.toMap(); if (channel.value("hidden").toBool()) continue;
        if (visible++<first) continue;
        if (rows.size()>=50) break;
        const auto id=channel.value("id").toString(); const auto wall=now(),clock=store.broadcastTime(id,wall);
        const auto delta=clock-wall;
        auto programs=store.programs(id,from+delta,to+delta,1000);
        channel["listingsTruncated"]=programs.size()==1000 && programs.last().toMap().value("endUtc").toLongLong()<to+delta;
        for (auto &entry:programs) {
            auto slot=entry.toMap(); slot["displayStartUtc"]=slot.value("startUtc").toLongLong()-delta;
            slot["displayEndUtc"]=slot.value("endUtc").toLongLong()-delta; entry=slot;
        }
        channel["programs"]=programs; channel["current"]=cachedCurrent(store,id,wall); channel["sourceName"]=channel.value("owner")=="kodi-pvr"?"Kodi PVR":channel.value("owner")=="guide-feed"?"Stream · "+channel.value("source").toMap().value("instance").toString():"Local"; rows.append(channel);
    }
    QVariantList compact;
    for(const auto &v:channels) {
        auto channel=v.toMap();if(channel.value("hidden").toBool())continue;
        const auto id=channel.value("id").toString();const auto wall=now(),clock=store.broadcastTime(id,wall);channel["current"]=cachedCurrent(store,id,wall);
        const auto current=channel.value("current").toMap();const auto end=current.value("endUtc",clock).toLongLong();
        auto next=store.programs(id,end,end+86400,3);for(auto &entry:next){auto slot=entry.toMap();slot["displayStartUtc"]=slot.value("startUtc").toLongLong()-clock+wall;slot["displayEndUtc"]=slot.value("endUtc").toLongLong()-clock+wall;entry=slot;}
        channel["upcoming"]=next;channel["sourceName"]=channel.value("owner")=="kodi-pvr"?"Kodi PVR":channel.value("owner")=="guide-feed"?"Stream · "+channel.value("source").toMap().value("instance").toString():"Local";compact.append(channel);
    }
    return {{"channels",channels},{"rows",rows},{"compactRows",compact},{"ready",store.ready()}};
}
}
GuideController::GuideController(const QString &path,const QString &profile,QObject *parent,const QString &bridgeDirectory)
    :QObject(parent),m_directory(bridgeDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        +"/native-profiles/"+profile+"/kodi/home/.kodi/userdata/isolde":bridgeDirectory) {
    m_profileDirectory=QFileInfo(path).absolutePath();m_from=now(); m_to=m_from+7200;
    m_worker=new GuideWorker(path); m_worker->moveToThread(&m_thread);
    connect(&m_thread,&QThread::finished,m_worker,&QObject::deleteLater);
    connect(m_worker,&GuideWorker::completed,this,[this](quint64 id,const QVariantMap &result) {
        if (!m_valid || id!=m_jobId) return;
        const bool background=m_background;
        m_busy=m_background=false; m_ready=result.value("ready").toBool(); m_channels=result.value("channels").toList();m_rows=result.value("rows").toList();m_compactRows=result.value("compactRows").toList();
        if(!background || !result.value("error").toString().isEmpty())m_error=result.value("error").toString();
        m_status=m_error.isEmpty()?tr("Saved guide ready"):m_error;
        auto completed=std::move(m_completion); m_completion={}; emit changed(); if (completed) completed(result);
        if(!m_busy && !m_pendingDefinition.isEmpty()) {const auto definition=m_pendingDefinition;const auto kind=m_pendingKind;m_pendingDefinition.clear();m_pendingKind.clear();previewLocal(definition,kind);return;}
        if(!m_busy && m_pendingJob) {auto job=std::move(m_pendingJob);auto completion=std::move(m_pendingCompletion);m_pendingJob={};m_pendingCompletion={};submit(std::move(job),std::move(completion));return;}
        if (m_refreshPending && !m_busy) {m_refreshPending=false;refresh();}
    });
    m_thread.start();
    const auto profileDirectory=m_profileDirectory;
    submit([profileDirectory](GuideStore &store){auto result=outcome(store.resumeClocks(now()),store);result["clockWarning"]=GuideClock::warning(profileDirectory,GuideClock::observation());return result;},[this](const QVariantMap &result){m_clockWarning=result.value("clockWarning").toString();emit changed();QTimer::singleShot(0,this,&GuideController::maintainSchedules);},true);
    m_maintenance.setInterval(60000);
    connect(&m_maintenance,&QTimer::timeout,this,[this]{m_maintenanceAttempted.clear();maintainSchedules();});
    m_maintenance.start();
    m_checkpoint.setInterval(30000);
    connect(&m_checkpoint,&QTimer::timeout,this,[this]{if(m_valid && m_ready && !m_busy && m_clockWarning.isEmpty()){const auto profile=m_profileDirectory;submit([profile](GuideStore &store){auto result=outcome(store.setActive(true,now()),store);const auto observation=GuideClock::observation();const auto warning=GuideClock::warning(profile,observation);result["clockWarning"]=warning;if(warning.isEmpty() && result.value("ok").toBool() && !GuideClock::save(profile,observation))result={{"ok",false},{"error","Cannot save this profile's clock observation."}};return result;},[this](const QVariantMap &result){if(!result.value("clockWarning").toString().isEmpty()){m_clockWarning=result.value("clockWarning").toString();emit changed();}},true);}});
    m_checkpoint.start();
    m_socket.setReadBufferSize(4*1024*1024+1);
    connect(&m_socket,&QLocalSocket::connected,this,[this]{m_socket.write(m_request);});
    connect(&m_socket,&QLocalSocket::readyRead,this,&GuideController::consume);
    connect(&m_socket,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError error){
        if(error==QLocalSocket::PeerClosedError && m_input.endsWith('\n'))return;
        if(m_reading)readFailure(tr("This profile's Kodi bridge is unavailable. Start Kodi, then preview again."));
    });
    m_requestDeadline.setSingleShot(true);m_requestDeadline.setInterval(5000);
    m_catalogDeadline.setSingleShot(true);m_catalogDeadline.setInterval(120000);
    connect(&m_requestDeadline,&QTimer::timeout,this,[this]{readFailure(tr("Kodi media preview timed out. Your channel draft was not saved."));});
    connect(&m_catalogDeadline,&QTimer::timeout,this,[this]{readFailure(tr("The media preview exceeded two minutes. Narrow the media selection and retry."));});
}
GuideController::~GuideController() { invalidate(); }
void GuideController::invalidate() {
    if(!m_valid)return;
    m_valid=false;++m_jobId;m_pendingJob={};m_pendingCompletion={};m_pendingDefinition.clear();if(m_cancel)m_cancel->store(true);m_checkpoint.stop();m_maintenance.stop();m_catalogDeadline.stop();m_reading=false;closeBridge();
    QMetaObject::invokeMethod(m_worker,[worker=m_worker]{if(worker->store){worker->store->setCancellationCheck({});worker->store->setActive(false,now());worker->store.reset();}},Qt::BlockingQueuedConnection);
    m_thread.quit();m_thread.wait();m_busy=false;m_ready=false;m_ruleTemplates.clear();m_clockWarning.clear();m_diagnostics.clear();m_channels.clear();m_rows.clear();m_compactRows.clear();m_candidates.clear();
    m_previewReady=m_sourcePreviewReady=false;m_previewDefinition.clear();m_sourcePreview.clear();m_sourceRevisions.clear();m_regenerationReview.clear();m_repairReview.clear();m_clockReview.clear();m_importPackage.clear();m_importRemapping.clear();m_feedListings.clear();
    emit changed();
}
bool GuideController::fail(const QString &message) {m_error=message;emit changed();return false;}
bool GuideController::submit(Job job,std::function<void(const QVariantMap &)> completion,bool background) {
    if(!m_valid)return fail(tr("This guide profile is no longer active."));
    if(m_busy) {
        if(m_background && !background && !m_pendingJob && m_pendingDefinition.isEmpty()) {m_pendingJob=std::move(job);m_pendingCompletion=std::move(completion);m_status=tr("Waiting for the saved guide refresh…");emit changed();return true;}
        return fail(tr("Finish or cancel the current guide operation first."));
    }
    m_busy=true;m_background=background;if(!background)m_error.clear();m_completion=std::move(completion);const auto id=++m_jobId;
    const auto from=m_from,to=m_to;const auto first=m_first;const auto cancelled=m_cancel=std::make_shared<std::atomic_bool>(false);
    QMetaObject::invokeMethod(m_worker,[worker=m_worker,id,from,to,first,cancelled,job=std::move(job)]{
        if(!worker->store)worker->store=std::make_unique<GuideStore>(worker->path);
        auto &store=*worker->store;store.setCancellationCheck([cancelled]{return cancelled->load();});
        auto result=store.ready()?job(store):outcome(false,store);
        const auto view=snapshot(store,from,to,first);for(auto it=view.cbegin();it!=view.cend();++it)result[it.key()]=it.value();
        store.setCancellationCheck({});emit worker->completed(id,result);
    },Qt::QueuedConnection);emit changed();return true;
}
void GuideController::refresh() {if(!m_valid)return;if(m_busy){m_refreshPending=true;return;}submit([](GuideStore &store){return outcome(true,store);},{},true);}
void GuideController::maintainSchedules() {
    if(!m_valid || !m_ready || busy() || !m_clockWarning.isEmpty())return;
    const auto wall=now();
    for(const auto &v:m_channels){
        const auto channel=v.toMap();const auto id=channel.value("id").toString();
        if(channel.contains("owner"))continue;
        const auto clock=channel.value("clock")=="real-time"?wall+channel.value("clockOffsetSeconds",0).toLongLong():channel.value("clockUtc").toLongLong()
            +(channel.value("active").toBool()?std::max<qint64>(0,wall-channel.value("anchorUtc").toLongLong()):0);
        const auto horizon=channel.value("horizonDays",7).toInt(),retention=channel.value("retentionDays",2).toInt();
        const bool horizonDue=channel.value("generatedThroughUtc").toLongLong()<clock+horizon*86400-6*3600;
        const bool historyDue=!channel.value("earliestEndUtc").isNull() && channel.value("earliestEndUtc").toLongLong()<=clock-retention*86400;
        if((!horizonDue && !historyDue) || m_maintenanceAttempted.contains(id))continue;
        m_maintenanceAttempted.insert(id);const auto revision=channel.value("revision").toInt();
        submit([id,revision,wall](GuideStore &store){return outcome(store.maintain(id,wall,revision),store);},[this,id](const QVariantMap &result){
            m_scheduleStatus=result.value("ok").toBool()?tr("Rolling schedule updated · %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                :tr("Schedule maintenance needs attention for %1: %2").arg(id,result.value("error").toString());
            emit changed();QTimer::singleShot(0,this,&GuideController::maintainSchedules);
        },true);return;
    }
}
void GuideController::setWindow(qint64 from,qint64 to,int first) {
    if(from<86400 || to<=from || to-from>86400 || first<0 || first>1024){fail(tr("Invalid guide time window."));return;}
    m_from=from;m_to=to;m_first=first;refresh();
}
QVariantMap GuideController::newDefinition() const {
    QSet<int> used;for(const auto &v:m_channels)used.insert(v.toMap().value("number").toInt());int number=1;while(used.contains(number))++number;
    return {{"schemaVersion",1},{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"name",tr("Local channel")},{"number",number},
        {"clock","real-time"},{"sequence","seeded-shuffle"},{"seed",int(QRandomGenerator::global()->bounded(2147483647u))},
        {"timezone",QString::fromUtf8(QTimeZone::systemTimeZoneId())},{"mediaKinds",QVariantList{QString("episode")}},{"rule",QVariantMap{{"field","source"},{"op","is"},{"value","kodi"}}}};
}
bool GuideController::previewLocal(const QVariantMap &definition,const QString &kind) {
    if(!m_valid)return fail(tr("This guide profile is no longer active."));
    QString error;const auto validated=ComponentSchema::guideChannel(definition,&error);if(validated.isEmpty())return fail(error);
    if(!QStringList{"movie","episode","musicvideo","song","mixed","all","custom"}.contains(kind))return fail(tr("Choose movies, TV episodes, music videos, songs or mixed media."));
    QStringList requested=kind=="mixed"?QStringList{"movie","episode"}:kind=="all"?QStringList{"movie","episode","musicvideo","song"}:QStringList{kind};
    if(kind=="custom"){requested.clear();for(const auto &v:validated.value("mediaKinds").toList())requested.append(v.toString());}
    QSet<QString> selected;
    for(const auto &v:validated.value("mediaKinds").toList())selected.insert(v.toString());
    if(selected!=QSet<QString>(requested.cbegin(),requested.cend()))return fail(tr("The media selection changed. Preview the updated channel settings."));
    if(m_busy) {
        if(m_background && !m_pendingJob && m_pendingDefinition.isEmpty()) {m_pendingDefinition=validated;m_pendingKind=kind;m_status=tr("Waiting for the saved guide refresh…");emit changed();return true;}
        return fail(tr("Finish the current guide operation first."));
    }
    ++m_catalogGeneration;m_pvrReading=false;m_previewDefinition=validated;m_candidates.clear();m_previewReady=false;m_skipped=m_page=m_kindIndex=m_snapshotBytes=0;
    m_importRemapping.clear();
    m_kinds=requested;
    m_busy=m_reading=true;m_background=false;m_error.clear();m_status=tr("Reading this profile's Kodi metadata…");m_catalogDeadline.start();requestPage();emit changed();return true;
}
void GuideController::closeBridge() {
    m_requestDeadline.stop();m_socket.abort();
#ifdef Q_OS_LINUX
    if(m_descriptor>=0)::close(m_descriptor);
#endif
    m_descriptor=-1;
}
void GuideController::readFailure(const QString &message) {
    ++m_catalogGeneration;m_reading=false;m_busy=false;m_previewReady=m_sourcePreviewReady=false;m_catalogDeadline.stop();closeBridge();m_candidates.clear();m_sourcePreview.clear();fail(message);
}
void GuideController::requestPage() {
    closeBridge();m_input.clear();
    m_request=QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",++m_requestId},{"method","Isolde.GuideCandidates"},
        {"params",QJsonObject{{"kind",m_pvrReading?QString():m_kinds[m_kindIndex]},{"start",m_page},{"end",m_page+50}}}}).toJson(QJsonDocument::Compact)+'\n';
    if(m_pvrReading){
        const auto params=m_sourceIndex<0?QVariantMap{{"channelgroupid","alltv"},{"properties",QVariantList{"channelnumber","locked","hidden","clientid","uniqueid"}},{"limits",QVariantMap{{"start",m_page},{"end",m_page+50}}}}
            :QVariantMap{{"channelid",m_sourcePreview[m_sourceIndex].toMap().value("definition").toMap().value("source").toMap().value("channelId")},
                {"properties",QVariantList{"title","starttime","endtime"}},{"limits",QVariantMap{{"start",m_page},{"end",m_page+50}}}};
        m_request=QJsonDocument::fromVariant(QVariantMap{{"jsonrpc","2.0"},{"id",m_requestId},{"method",m_sourceIndex<0?"PVR.GetChannels":"PVR.GetBroadcasts"},{"params",params}}).toJson(QJsonDocument::Compact)+'\n';
    }
#ifdef Q_OS_LINUX
    m_descriptor=::open(m_directory.toLocal8Bit().constData(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(m_descriptor<0){readFailure(tr("Kodi's local bridge is not set up for this profile. Start Kodi, then retry."));return;}
    m_requestDeadline.start();m_socket.connectToServer(QString("/proc/self/fd/%1/rpc.sock").arg(m_descriptor));
#else
    readFailure(tr("The local Kodi bridge requires Linux."));
#endif
}
void GuideController::consume() {
    if(!m_valid || !m_reading)return;
    m_input+=m_socket.readAll();if(m_input.size()>4*1024*1024){readFailure(tr("Kodi returned too much preview data."));return;}
    if(!m_input.endsWith('\n'))return;
    const auto reply=QJsonDocument::fromJson(m_input).object();
    if(reply.value("jsonrpc")!="2.0" || reply.value("id").toInt(-1)!=m_requestId || reply.contains("error") || !reply.value("result").isObject()){
        readFailure(tr("Kodi could not provide guide metadata. Update/enable the Isolde bridge, check your library and retry."));return;
    }
    const auto result=reply.value("result").toObject();
    if(m_pvrReading){consumePvr(result.toVariantMap());return;}
    const auto limits=result.value("limits").toObject();const auto items=result.value("items").toArray();
    const int start=limits.value("start").toInt(-1),end=limits.value("end").toInt(-1),total=limits.value("total").toInt(-1),skipped=result.value("skipped").toInt(-1);
    if(!result.value("items").isArray() || start!=m_page || end<start || end>start+50 || total<end || total>100000 || (end==start && total>end)
        || skipped<0 || items.size()+skipped!=end-start){readFailure(tr("Kodi returned an invalid guide metadata page."));return;}
    for(const auto &v:items) {
        const auto item=v.toObject().toVariantMap();if(!GuideRules::matchesChannel(m_previewDefinition,item))continue;
        m_snapshotBytes+=QJsonDocument::fromVariant(item).toJson(QJsonDocument::Compact).size()+1;
        if(m_candidates.size()>=10000 || m_snapshotBytes>4*1024*1024-2){readFailure(tr("More media matches than one channel can hold. Narrow the rules before saving."));return;}
        m_candidates.append(item);
    }
    m_skipped+=skipped;closeBridge();
    if(end<total)m_page=end;else{++m_kindIndex;m_page=0;}
    if(m_kindIndex<m_kinds.size()){m_status=tr("Previewing local media · %1 matches").arg(m_candidates.size());emit changed();const auto generation=m_catalogGeneration;QTimer::singleShot(0,this,[this,generation]{if(m_valid && m_reading && generation==m_catalogGeneration)requestPage();});return;}
    m_catalogDeadline.stop();m_reading=m_busy=false;m_previewReady=!m_candidates.isEmpty();
    m_status=tr("%1 matching items · %2 items skipped (remote or unknown duration)").arg(m_candidates.size()).arg(m_skipped);
    if(m_previewDefinition.value("sequence")=="next-unwatched")m_status+=tr(" · Unwatched in Kodi at preview; repeats in episode order. Refresh rules to update watch state; committed programs stay unchanged.");
    if(m_importPackage.value("draftId")==m_previewDefinition.value("id") && !m_importPackage.value("snapshot").toMap().isEmpty())m_status+=tr(" · Review imported media mappings before Save. Imported UTC intervals stay unchanged; edited matching rules apply beyond the snapshot.");
    if(!m_previewReady)m_error=tr("No playable local media matches. Adjust the rules or update Kodi's metadata.");emit changed();
}
bool GuideController::previewClockAdjustment(const QString &id,const QVariant &delta,int revision) {
    m_clockReview.clear();
    if(!ComponentSchema::safeId(id) || !integer(delta,-31*86400,31*86400) || delta.toLongLong()==0)return fail(tr("Choose a nonzero clock adjustment of at most 31 days."));
    return submit([id,delta,revision](GuideStore &store){const auto review=store.clockAdjustmentPreview(id,delta.toLongLong(),revision,now());return QVariantMap{{"ok",!review.isEmpty()},{"review",review},{"error",review.isEmpty()?QString("Review an unchanged local channel and choose a position inside its cached schedule."):QString()}};},
        [this](const QVariantMap &result){if(result.value("ok").toBool()){m_clockReview=result.value("review").toMap();emit changed();}});
}
bool GuideController::adjustClock() {
    if(m_clockReview.isEmpty())return fail(tr("Preview the broadcast-clock adjustment before applying it."));
    const auto review=m_clockReview;
    return submit([review](GuideStore &store){return outcome(store.adjustClock(review,now()),store);},[this,review](const QVariantMap &result){if(result.value("ok").toBool()){m_clockReview.clear();m_maintenanceAttempted.remove(review.value("channelId").toString());emit changed();QTimer::singleShot(0,this,&GuideController::maintainSchedules);}});
}
bool GuideController::createLocal(const QVariantMap &definition) {
    if(!m_previewReady || definition!=m_previewDefinition)return fail(tr("Preview these exact channel settings before saving."));
    const auto candidates=m_candidates;const auto id=definition.value("id").toString();
    const auto package=m_importPackage.value("draftId")==id?m_importPackage.value("snapshot").toMap():QVariantMap();
    const auto assets=m_importPackage.value("draftId")==id?m_importPackage.value("assets").toMap():QVariantMap();const auto profile=m_profileDirectory;const auto remapping=m_importRemapping;
    return submit([definition,candidates,package,assets,profile,remapping](GuideStore &store){
        for(auto it=assets.cbegin();it!=assets.cend();++it)if(!GuideAssets::write(profile,it.key(),QByteArray::fromBase64(it.value().toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors)))return QVariantMap{{"ok",false},{"error","Cannot save the imported station artwork. No channel was created."}};
        return outcome(store.createLocal(definition,candidates,now(),-1,package,remapping),store);
    },[this,id](const QVariantMap &r){if(r.value("ok").toBool()){m_previewReady=false;m_candidates.clear();m_importPackage.clear();emit channelCreated(id);emit changed();}});
}
bool GuideController::exportChannel(const QString &id,const QString &path,bool includeSchedule) {
    if(!portablePath(path))return fail(tr("Choose an absolute .json file path without symbolic links."));
    const auto profile=m_profileDirectory;
    return submit([id,path,includeSchedule,profile](GuideStore &store){
        if(path.endsWith(".xsp",Qt::CaseInsensitive)) {
            if(includeSchedule)return QVariantMap{{"ok",false},{"error","Kodi smart playlists cannot contain cached broadcast schedules. Choose JSON or turn off the snapshot."}};
            QString error;const auto bytes=GuidePlaylists::encode(store.definition(id),&error);if(bytes.isEmpty())return QVariantMap{{"ok",false},{"error",error}};
            if(!portablePath(path) || QFileInfo::exists(path))return QVariantMap{{"ok",false},{"error","Choose a new playlist filename. Existing files are preserved."}};
            if(!GuideAtomicFile::publish(path,bytes,[&store]{return !store.cancellationRequested();}))return QVariantMap{{"ok",false},{"error","Cannot atomically export this new playlist. Existing files were preserved."}};return outcome(true,store);
        }
        auto package=store.portableChannel(id,includeSchedule);if(package.isEmpty())return outcome(false,store);
        const auto logo=package.value("definition").toMap().value("logo").toString();
        if(!logo.isEmpty()) {
            QFile asset(GuideAssets::path(profile,logo));if(!asset.open(QIODevice::ReadOnly) || asset.size()>20*1024*1024)return QVariantMap{{"ok",false},{"error","The managed station logo is unavailable. Restore or remove it before exporting."}};
            const auto data=asset.read(20*1024*1024+1);if(GuideAssets::name(data)!=logo)return QVariantMap{{"ok",false},{"error","The station logo failed its content identity check."}};
            package["assets"]=QVariantMap{{logo,QString::fromLatin1(data.toBase64())}};
        }
        if(!portablePath(path) || QFileInfo::exists(path))return QVariantMap{{"ok",false},{"error","Choose a new export filename. Existing files are never overwritten."}};
        const auto bytes=QJsonDocument::fromVariant(package).toJson(QJsonDocument::Compact);
        QString error;if(GuideStore::portableDefinition(package,&error).isEmpty())return QVariantMap{{"ok",false},{"error",error}};
        if(!GuideAtomicFile::publish(path,bytes,[&store]{return !store.cancellationRequested();}))return QVariantMap{{"ok",false},{"error","Cannot atomically save this new channel package. Choose a writable folder and retry. Existing files were preserved."}};
        return outcome(true,store);
    },[this](const QVariantMap &result){if(result.value("ok").toBool()){m_status=tr("Channel exported. No media files, account credentials or Kodi configuration were included.");emit changed();}});
}
bool GuideController::previewImport(const QString &path,bool restoreSchedule) {
    if(!portablePath(path))return fail(tr("Choose an absolute .json channel-package path without symbolic links."));
    m_importRemapping.clear();const auto fresh=newDefinition();
    return submit([path,restoreSchedule,fresh](GuideStore &store){
        if(!portablePath(path))return QVariantMap{{"ok",false},{"error","The import path changed. Choose the package again."}};
        QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>32*1024*1024)return QVariantMap{{"ok",false},{"error","Cannot read this package within the 32 MiB limit."}};
        if(path.endsWith(".xsp",Qt::CaseInsensitive)) {
            if(restoreSchedule)return QVariantMap{{"ok",false},{"error","Kodi smart playlists have no broadcast snapshot. Turn off snapshot restoration."}};
            QString error;const auto definition=GuidePlaylists::decode(file.read(256*1024+1),fresh,&error);if(definition.isEmpty())return QVariantMap{{"ok",false},{"error",error}};
            auto result=outcome(true,store);result["definition"]=definition;return result;
        }
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.read(32*1024*1024+1),&parse);QString error;
        auto package=document.toVariant().toMap();auto definition=parse.error==QJsonParseError::NoError?GuideStore::portableDefinition(package,&error):QVariantMap();
        if(definition.isEmpty())return QVariantMap{{"ok",false},{"error",error.isEmpty()?QString("Invalid channel package JSON."):error}};
        if(definition.contains("logo") && !package.contains("assets"))return QVariantMap{{"ok",false},{"error","The package references a logo but contains no portable artwork."}};
        if(restoreSchedule && !package.contains("slots"))return QVariantMap{{"ok",false},{"error","This package contains no schedule snapshot. Choose definition-only import."}};
        definition["id"]=fresh.value("id");definition["number"]=fresh.value("number");
        if(!definition.contains("mediaKinds"))definition["mediaKinds"]=QVariantList{"movie","episode","musicvideo","song"};
        if(restoreSchedule && !package.value("definition").toMap().contains("mediaKinds")){auto original=package.value("definition").toMap();original["mediaKinds"]=definition.value("mediaKinds");package["definition"]=original;}
        auto result=outcome(true,store);result["definition"]=definition;result["assets"]=package.value("assets");if(restoreSchedule)result["snapshot"]=package;return result;
    },[this](const QVariantMap &result){
        if(!result.value("ok").toBool())return;
        const auto definition=result.value("definition").toMap();m_importPackage={{"draftId",definition.value("id")},{"snapshot",result.value("snapshot")},{"assets",result.value("assets")}};
        m_previewReady=false;m_candidates.clear();m_previewDefinition.clear();emit duplicatePrepared(definition);emit changed();
    });
}
QVariantList GuideController::importReview() const {
    QVariantList result;if(!m_valid || !m_previewReady || m_importPackage.value("draftId")!=m_previewDefinition.value("id"))return result;
    const auto items=m_importPackage.value("snapshot").toMap().value("items").toList();
    QMap<QString,int> counts;QMap<QString,QVariantMap> byId;
    const auto signature=[](const QVariantMap &item){return item.value("mediaType").toString()+QChar(0x1f)+item.value("title").toString()+QChar(0x1f)+QString::number(item.value("duration").toLongLong());};
    for(const auto &v:m_candidates){const auto item=v.toMap();++counts[signature(item)];byId[item.value("mediaType").toString()+":"+item.value("mediaId").toString()]=item;}
    for(const auto &v:items){auto old=v.toMap();const auto key=old.value("mediaType").toString()+":"+old.value("mediaId").toString();int matches=0;QString title;
        matches=counts.value(signature(old));if(matches==1)title=old.value("title").toString();
        if(m_importRemapping.contains(key)){const auto chosen=m_importRemapping.value(key).toMap();const auto item=byId.value(chosen.value("mediaType").toString()+":"+chosen.value("mediaId").toString());if(!item.isEmpty()){title=item.value("title").toString();matches=1;}}
        old["mapped"]=matches==1;old["mappingLabel"]=matches==1?tr("Use %1").arg(title):matches?tr("Ambiguous · Choose replacement"):tr("Missing · Choose replacement");result.append(old);
    }return result;
}
QVariantList GuideController::importChoices(int item,int first) const {
    const auto review=importReview();QVariantList result;if(item<0 || item>=review.size() || first<0 || first>=m_candidates.size())return result;
    const auto duration=review[item].toMap().value("duration").toLongLong();
    for(int i=first;i<std::min(first+50,int(m_candidates.size()));++i){auto candidate=m_candidates[i].toMap();candidate["choice"]=i;candidate["fits"]=candidate.value("duration").toLongLong()==duration;result.append(candidate);}return result;
}
bool GuideController::mapImportedItem(int item,int candidate) {
    const auto review=importReview();if(!m_valid || busy() || item<0 || item>=review.size() || candidate<0 || candidate>=m_candidates.size())return fail(tr("Preview current Kodi media before remapping."));
    const auto old=review[item].toMap(),choice=m_candidates[candidate].toMap();if(choice.value("duration").toLongLong()!=old.value("duration").toLongLong())return fail(tr("Choose a replacement with the same indexed duration. Import definition only when none fits; slot times stay unchanged."));
    m_importRemapping[old.value("mediaType").toString()+":"+old.value("mediaId").toString()]=QVariantMap{{"mediaType",choice.value("mediaType")},{"mediaId",choice.value("mediaId")}};emit changed();return true;
}
QVariantMap GuideController::editableDefinition(const QString &id) const {
    if(!m_valid || !ComponentSchema::safeId(id))return {};
    for(const auto &v:m_channels){auto c=v.toMap();if(c.value("id")!=id || c.contains("owner"))continue;
        for(const auto &key:QStringList{"revision","hidden","favorite","position","anchorUtc","clockUtc","active","generatedThroughUtc","earliestEndUtc"})c.remove(key);
        if(!c.contains("mediaKinds"))c["mediaKinds"]=QVariantList{"movie","episode","musicvideo","song"};
        return ComponentSchema::guideChannel(c);
    }
    return {};
}
bool GuideController::updateLocal(const QVariantMap &definition,int revision) {
    if(!m_previewReady || definition!=m_previewDefinition)return fail(tr("Preview these exact updated rules before saving."));
    const auto candidates=m_candidates;const auto id=definition.value("id").toString();
    return submit([definition,candidates,revision](GuideStore &s){return outcome(s.updateRules(definition,candidates,revision,now()),s);},[this,id](const QVariantMap &r){if(r.value("ok").toBool()){m_previewReady=false;m_candidates.clear();emit channelCreated(id);emit changed();}});
}
bool GuideController::previewRegeneration(const QString &id) {
    if(!ComponentSchema::safeId(id))return fail(tr("Invalid local channel."));m_regenerationReview.clear();
    return submit([id](GuideStore &s){const auto review=s.regenerationPreview(id,now());return QVariantMap{{"ok",!review.isEmpty()},{"review",review},{"error",review.isEmpty()?QString("No future local schedule is available to replace. Check maintenance and retry."):QString()}};},
        [this](const QVariantMap &r){m_regenerationReview=r.value("review").toMap();emit changed();});
}
bool GuideController::regenerate() {
    if(m_regenerationReview.isEmpty())return fail(tr("Review the future schedule boundary before regenerating."));
    const auto review=m_regenerationReview;const auto id=review.value("channelId").toString();
    return submit([id,review](GuideStore &s){return outcome(s.regenerate(id,review,now()),s);},[this](const QVariantMap &r){if(r.value("ok").toBool()){m_regenerationReview.clear();emit changed();}});
}
bool GuideController::previewDuplicate(const QString &id,int revision) {
    const auto fresh=newDefinition();const auto newId=fresh.value("id").toString();const auto number=fresh.value("number");
    return submit([id,revision,newId,number](GuideStore &s){
        auto def=s.definition(id);const auto items=s.candidateSnapshot(id);
        if(def.isEmpty() || def.contains("owner") || s.channel(id).value("revision").toInt()!=revision || items.isEmpty())return QVariantMap{{"ok",false},{"error","The local channel changed or its media needs repair. Refresh before duplicating."}};
        def["id"]=newId;def["number"]=number;const auto name=def.value("name").toString()+" copy";def["name"]=name.size()<=160?name:QString("Channel copy");
        return QVariantMap{{"ok",true},{"definition",def},{"candidates",items}};
    },[this](const QVariantMap &r){if(!r.value("ok").toBool())return;m_previewDefinition=r.value("definition").toMap();m_candidates=r.value("candidates").toList();m_previewReady=true;emit changed();emit duplicatePrepared(m_previewDefinition);});
}
bool GuideController::previewRepair(const QString &id,const QString &slotId,int revision,int first,bool fitInterval) {
    return submit([id,slotId,revision,first,fitInterval](GuideStore &s){const auto review=s.repairPreview(id,slotId,revision,now(),first,fitInterval);return QVariantMap{{"ok",!review.isEmpty()},{"review",review},{"error",review.isEmpty()?QString("This local program changed or its channel needs repair. Refresh the selection and retry."):QString()}};},
        [this](const QVariantMap &r){m_repairReview=r.value("review").toMap();emit changed();});
}
bool GuideController::repair(const QVariant &choice) {
    const auto alternatives=m_repairReview.value("alternatives").toList();
    if(m_repairReview.isEmpty() || !integer(choice,-1,alternatives.size()-1))return fail(tr("Review an available repair choice before applying it."));
    const auto item=choice.toInt()==-1?QVariantMap():alternatives[choice.toInt()].toMap();
    const auto id=m_repairReview.value("channelId").toString(),slot=m_repairReview.value("slotId").toString();const auto revision=m_repairReview.value("revision").toInt();
    const bool fitInterval=m_repairReview.value("fitInterval").toBool();
    return submit([id,slot,revision,item,fitInterval](GuideStore &s){return outcome(s.repairSlot(id,slot,item.value("mediaType").toString(),item.value("mediaId").toInt(),revision,fitInterval),s);},
        [this](const QVariantMap &r){if(r.value("ok").toBool()){m_repairReview.clear();emit changed();}});
}
bool GuideController::previewPvr() {
    if(!m_valid)return fail(tr("This guide profile is no longer active."));
    if(m_busy){
        if(m_background)return submit([](GuideStore &s){return outcome(true,s);},[this](const QVariantMap &){previewPvr();});
        return fail(tr("Finish or cancel the current guide operation first."));
    }
    ++m_catalogGeneration;m_sourceInstance="kodi-pvr";m_feedListings.clear();m_sourcePreview.clear();m_sourceRevisions.clear();
    for(const auto &v:m_channels){const auto c=v.toMap();m_sourceRevisions[c.value("id").toString()]=c.value("revision");}
    m_sourceRefreshed=now();m_sourceIndex=-1;m_page=m_snapshotBytes=m_skipped=0;m_sourcePreviewReady=false;
    m_pvrReading=m_reading=m_busy=true;m_background=false;m_error.clear();m_status=tr("Previewing this profile's configured Kodi PVR channels…");
    m_catalogDeadline.start();requestPage();emit changed();return true;
}
qint64 GuideController::pvrUtc(const QVariant &value) {
    if(value.metaType().id()!=QMetaType::QString)return -1;
    const auto d=QDateTime::fromString(QString(value.toString()).replace(' ','T')+"Z",Qt::ISODate);
    if(!d.isValid() || d.toUTC().toString("yyyy-MM-dd HH:mm:ss")!=value.toString())return -1;
    return d.toSecsSinceEpoch();
}
void GuideController::consumePvr(const QVariantMap &result) {
    const auto limits=result.value("limits").toMap();const auto key=m_sourceIndex<0?QString("channels"):QString("broadcasts");
    const auto items=result.value(key).toList();const int start=limits.value("start",-1).toInt(),end=limits.value("end",-1).toInt(),total=limits.value("total",-1).toInt();
    if(!integer(limits.value("start"),0,1024) || !integer(limits.value("end"),0,1024) || !integer(limits.value("total"),0,1024)
        || result.value(key).metaType().id()!=QMetaType::QVariantList || start!=m_page || end!=start+items.size() || items.size()>50 || total<end
        || total>(m_sourceIndex<0?1024:1000) || (end==start && total>end)){
        readFailure(tr("Kodi returned an invalid or oversized PVR page. Cached listings were preserved."));return;
    }
    if(m_sourceIndex<0){
        QSet<int> used;for(const auto &v:m_channels)used.insert(v.toMap().value("number").toInt());
        for(const auto &v:m_sourcePreview)used.insert(v.toMap().value("definition").toMap().value("number").toInt());
        for(const auto &v:items){
            const auto item=v.toMap();if(item.value("locked").metaType().id()!=QMetaType::Bool || item.value("hidden").metaType().id()!=QMetaType::Bool){readFailure(tr("Kodi did not report PVR availability safely."));return;}
            if(item.value("locked").toBool() || item.value("hidden").toBool()){++m_skipped;continue;}
            const auto source=QVariantMap{{"instance","kodi-pvr"},{"channelId",item.value("channelid")},{"clientId",item.value("clientid")},{"uniqueId",item.value("uniqueid")},{"number",item.value("channelnumber")}};
            const auto id="pvr-"+QString::fromLatin1(QCryptographicHash::hash(QByteArray("kodi-pvr:")+QByteArray::number(source.value("clientId").toInt())+":"+QByteArray::number(source.value("uniqueId").toInt()),QCryptographicHash::Sha256).toHex());
            int number=item.value("channelnumber").toInt();const auto existing=std::find_if(m_channels.cbegin(),m_channels.cend(),[&id](const QVariant &c){return c.toMap().value("id")==id;});
            if(existing!=m_channels.cend())number=existing->toMap().value("number").toInt();
            else if(number<1 || used.contains(number)){number=1;while(used.contains(number) && number<99999)++number;}
            QVariantMap definition{{"schemaVersion",1},{"id",id},{"owner","kodi-pvr"},{"source",source},{"number",number},{"name",existing==m_channels.cend()?item.value("label"):existing->toMap().value("name")},{"sourceAvailable",true},{"refreshedUtc",m_sourceRefreshed}};
            QString error;if(ComponentSchema::guideChannel(definition,&error).isEmpty() || std::any_of(m_sourcePreview.cbegin(),m_sourcePreview.cend(),[&id](const QVariant &c){return c.toMap().value("definition").toMap().value("id")==id;})){
                readFailure(tr("Invalid or ambiguous PVR channel identity. Cached listings were preserved."));return;
            }
            used.insert(number);m_sourcePreview.append(QVariantMap{{"definition",definition},{"programs",QVariantList()}});
            m_snapshotBytes+=QJsonDocument::fromVariant(definition).toJson(QJsonDocument::Compact).size()+40;
        }
    }else{
        auto entry=m_sourcePreview[m_sourceIndex].toMap();auto programs=entry.value("programs").toList();
        for(const auto &v:items){const auto p=v.toMap();
            const auto from=pvrUtc(p.value("starttime")),to=pvrUtc(p.value("endtime"));
            if(from<0 || to<=from || to-from>7*86400 || !integer(p.value("broadcastid"),1,2147483647) || p.value("title").metaType().id()!=QMetaType::QString || p.value("title").toString().trimmed().isEmpty() || p.value("title").toString().size()>512){readFailure(tr("Invalid PVR broadcast interval or identity. Cached listings were preserved."));return;}
            if(to<m_sourceRefreshed-7*86400 || from>m_sourceRefreshed+30*86400)continue;
            QVariantMap program{{"broadcastId",p.value("broadcastid").toString()},{"title",p.value("title")},{"startUtc",from},{"endUtc",to}};
            programs.append(program);m_snapshotBytes+=QJsonDocument::fromVariant(program).toJson(QJsonDocument::Compact).size()+1;
        }
        std::sort(programs.begin(),programs.end(),[](const QVariant &a,const QVariant &b){return a.toMap().value("startUtc").toLongLong()<b.toMap().value("startUtc").toLongLong();});
        entry["programs"]=programs;m_sourcePreview[m_sourceIndex]=entry;
    }
    closeBridge();if(m_snapshotBytes>4*1024*1024-4096){readFailure(tr("PVR preview exceeds the 4 MiB cache generation limit."));return;}
    if(end<total)m_page=end;
    else {m_page=0;++m_sourceIndex;if(m_sourceIndex==0 && m_sourcePreview.isEmpty()){readFailure(tr("No unlocked, visible PVR channels are configured in this Kodi profile. Set up a PVR source, then retry."));return;}}
    if(m_sourceIndex>=0 && m_sourceIndex>=m_sourcePreview.size()){
        m_catalogDeadline.stop();m_reading=m_busy=m_pvrReading=false;m_sourcePreviewReady=true;
        m_status=tr("%1 PVR channels ready for mapping review · %2 hidden/locked channels skipped. Nothing imported yet.").arg(m_sourcePreview.size()).arg(m_skipped);emit changed();return;
    }
    m_status=tr("Reading PVR listings · channel %1 of %2").arg(std::max(0,m_sourceIndex)+1).arg(m_sourcePreview.size());emit changed();
    const auto generation=m_catalogGeneration;QTimer::singleShot(0,this,[this,generation]{if(m_valid && m_reading && generation==m_catalogGeneration)requestPage();});
}
bool GuideController::importPvr() {
    if(!m_sourcePreviewReady)return fail(tr("Preview the complete PVR source before importing."));
    const auto entries=m_sourcePreview;const auto revisions=m_sourceRevisions;const auto refreshed=m_sourceRefreshed;
    const auto instance=m_sourceInstance;
    return submit([entries,revisions,refreshed,instance](GuideStore &store){return outcome(store.importPvr(instance,entries,refreshed,revisions),store);},[this](const QVariantMap &r){if(r.value("ok").toBool()){m_sourcePreviewReady=false;emit changed();}});
}
bool GuideController::previewFeed(const QString &instance,const QString &playlistPath,const QString &xmltvPath,const QString &directUrl,const QString &directName) {
    if(!ComponentSchema::safeId(instance) || instance=="kodi-pvr")return fail(tr("Choose a unique source ID containing letters, numbers, dots, underscores or hyphens."));
    if(!directUrl.isEmpty() && (!GuideFeeds::streamUrl(directUrl) || directName.trimmed().isEmpty() || directName.size()>160 || directName.contains('\n') || directName.contains('\r')))return fail(tr("Choose a station name and compatible stream URL without credentials or player options."));
    m_sourcePreviewReady=false;m_sourcePreview.clear();m_feedListings.clear();m_sourceInstance=instance;m_sourceRefreshed=now();m_sourceRevisions.clear();
    for(const auto &v:m_channels){const auto channel=v.toMap();m_sourceRevisions[channel.value("id").toString()]=channel.value("revision");}
    const auto refreshed=m_sourceRefreshed;
    return submit([instance,playlistPath,xmltvPath,directUrl,directName,refreshed](GuideStore &store){
        const auto read=[](const QString &path,qint64 maximum,QByteArray &bytes){
            for(const auto c:path)if(c.unicode()<32 || c.unicode()==127)return false;
            if(!QDir::isAbsolutePath(path) || QFileInfo(path).isSymLink() || !QFileInfo(path).isFile())return false;
            QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>maximum)return false;bytes=file.read(maximum+1);return bytes.size()<=maximum && file.atEnd();
        };
        QByteArray playlist,xml;
        if(directUrl.isEmpty()){if(!read(playlistPath,4*1024*1024,playlist))return QVariantMap{{"ok",false},{"error","Cannot read a bounded local playlist file."}};}
        else playlist="#EXTM3U\n#EXTINF:-1,"+directName.toUtf8()+"\n"+directUrl.toUtf8()+"\n";
        if(!xmltvPath.isEmpty() && !read(xmltvPath,16*1024*1024,xml))return QVariantMap{{"ok",false},{"error","Cannot read a bounded local XMLTV file. Cached listings were preserved."}};
        auto result=GuideFeeds::parse(playlist,xml,instance,refreshed,[&store]{return store.cancellationRequested();});if(!result.value("ok").toBool())return result;
        auto entries=result.value("entries").toList();const auto listings=result.value("listings").toMap();QSet<int> used;
        for(const auto &v:store.channels())used.insert(v.toMap().value("number").toInt());
        for(auto &v:entries){auto entry=v.toMap();auto definition=entry.value("definition").toMap();const auto old=store.channel(definition.value("id").toString());
            int number=old.isEmpty()?definition.value("number").toInt():old.value("number").toInt();if(old.isEmpty() && used.contains(number)){number=1;while(used.contains(number) && number<99999)++number;}
            if(!old.isEmpty()) {
                auto source=definition.value("source").toMap();const auto epg=old.value("source").toMap().value("epgId").toString();
                if(!epg.isEmpty() && !xml.isEmpty() && !listings.contains(epg))return QVariantMap{{"ok",false},{"error","A previously mapped XMLTV channel is missing from this refresh. Cached listings were preserved. Review a complete source file before retrying."}};
                source["epgId"]=epg;definition["source"]=source;entry["programs"]=listings.value(epg,QVariantList{});
            }
            definition["number"]=number;used.insert(number);QString error;
            if(ComponentSchema::guideChannel(definition,&error).isEmpty())return QVariantMap{{"ok",false},{"error",error}};
            entry["definition"]=definition;v=entry;
        }
        if(QJsonDocument::fromVariant(entries).toJson(QJsonDocument::Compact).size()>4*1024*1024)return QVariantMap{{"ok",false},{"error","The reviewed source generation exceeds the 4 MiB cache limit. Use a smaller station/EPG file; cached listings were preserved."}};
        result["entries"]=entries;return result;
    },[this](const QVariantMap &result){if(result.value("ok").toBool()){m_sourcePreview=result.value("entries").toList();m_feedListings=result.value("listings").toMap();m_sourcePreviewReady=true;m_status=tr("Review station numbers and EPG mappings. Refresh is manual; browsing makes no network requests. No listings still permits live tuning.");emit changed();}});
}
bool GuideController::setSourceEpg(int index,const QString &epgId) {
    if(!m_valid || busy() || !m_sourcePreviewReady || index<0 || index>=m_sourcePreview.size())return fail(tr("Preview the source before editing its EPG mapping."));
    auto entry=m_sourcePreview[index].toMap();auto definition=entry.value("definition").toMap();
    if(definition.value("owner")!="guide-feed" || (!epgId.isEmpty() && !m_feedListings.contains(epgId)))return fail(tr("Choose an XMLTV channel ID present in this preview, or empty for no listings."));
    auto source=definition.value("source").toMap();source["epgId"]=epgId;definition["source"]=source;entry["definition"]=definition;entry["programs"]=m_feedListings.value(epgId,QVariantList{});auto preview=m_sourcePreview;preview[index]=entry;
    if(QJsonDocument::fromVariant(preview).toJson(QJsonDocument::Compact).size()>4*1024*1024)return fail(tr("This mapping exceeds the 4 MiB source generation limit. The previous mapping was preserved."));
    m_sourcePreview=preview;emit changed();return true;
}
bool GuideController::setSourceNumber(int index,const QVariant &value) {
    if(!m_valid || busy() || !m_sourcePreviewReady || index<0 || index>=m_sourcePreview.size() || !integer(value,1,99999))return fail(tr("Choose a whole preview channel number from 1 to 99999."));
    const int number=value.toInt();
    auto entry=m_sourcePreview[index].toMap();auto def=entry.value("definition").toMap();const auto id=def.value("id").toString();
    for(const auto &v:m_channels){const auto c=v.toMap();if(c.value("id")==id)return fail(tr("This channel already exists. Change its number in Manage, then preview the source again."));if(c.value("number").toInt()==number)return fail(tr("That number is in use. Choose another number; the mapping is unchanged."));}
    for(int i=0;i<m_sourcePreview.size();++i)if(i!=index && m_sourcePreview[i].toMap().value("definition").toMap().value("number").toInt()==number)return fail(tr("That number is used by another preview channel. Choose another number."));
    def["number"]=number;entry["definition"]=def;m_sourcePreview[index]=entry;m_error.clear();emit changed();return true;
}
void GuideController::cancel() {
    ++m_catalogGeneration;
    m_completion={};
    m_pendingJob={};m_pendingCompletion={};m_pendingDefinition.clear();m_pendingKind.clear();
    if(m_cancel)m_cancel->store(true);
    if(m_reading){m_reading=m_busy=false;m_catalogDeadline.stop();closeBridge();}
    m_previewReady=m_sourcePreviewReady=false;m_candidates.clear();m_sourcePreview.clear();m_regenerationReview.clear();m_repairReview.clear();m_clockReview.clear();m_importPackage.clear();m_importRemapping.clear();m_feedListings.clear();m_status=m_busy?tr("Cancellation requested. Waiting for the current transaction result…"):tr("Preview cancelled.");emit changed();
}
bool GuideController::setPreferences(const QString &id,const QVariantMap &p,int revision){return submit([id,p,revision](GuideStore &store){return outcome(store.setPreferences(id,p,revision),store);});}
bool GuideController::setMetadata(const QString &id,const QVariantMap &p,int revision){return submit([id,p,revision](GuideStore &store){return outcome(store.setMetadata(id,p,revision),store);});}
QString GuideController::stationLogo(const QString &relative) const {return m_valid?GuideAssets::source(m_profileDirectory,relative):QString();}
bool GuideController::refreshRuleTemplates() {
    const auto profile=m_profileDirectory;
    return submit([profile](GuideStore &store){
        const auto path=QDir(profile).filePath("guide-templates");if(QFileInfo(path).isSymLink())return QVariantMap{{"ok",false},{"error","Template directory cannot be a symbolic link."}};
        const QDir directory(path);const auto files=directory.entryInfoList({"*.json"},QDir::Files|QDir::NoDotAndDotDot,QDir::Name);
        if(files.size()>64)return QVariantMap{{"ok",false},{"error","A profile supports at most 64 saved channel templates."}};
        QVariantList templates;
        for(const auto &info:files){QFile file(info.absoluteFilePath());if(info.isSymLink() || !file.open(QIODevice::ReadOnly) || file.size()>256*1024)return QVariantMap{{"ok",false},{"error","A saved template is unreadable or oversized. Original files were preserved."}};
            const auto value=QJsonDocument::fromJson(file.read(256*1024+1)).toVariant().toMap();QString error;
            if(value.size()!=3 || !integer(value.value("schemaVersion"),1,1) || value.value("name").metaType().id()!=QMetaType::QString || value.value("name").toString().trimmed().isEmpty() || value.value("name").toString().size()>80
                || ComponentSchema::guideChannel(value.value("definition").toMap(),&error).isEmpty() || value.value("definition").toMap().contains("owner"))return QVariantMap{{"ok",false},{"error","A saved template is invalid. Original files were preserved."}};
            templates.append(value);
        }auto result=outcome(true,store);result["templates"]=templates;return result;
    },[this](const QVariantMap &result){if(result.value("ok").toBool()){m_ruleTemplates=result.value("templates").toList();emit changed();}});
}
bool GuideController::saveRuleTemplate(const QString &name,const QVariantMap &definition) {
    if(name.trimmed()!=name || name.isEmpty() || name.size()>80 || ComponentSchema::guideChannel(definition).isEmpty() || definition.contains("owner"))return fail(tr("Choose a template name up to 80 characters and a valid local channel draft."));
    for(const auto c:name)if(c.unicode()<32)return fail(tr("Use a plain-text template name."));
    const auto profile=m_profileDirectory;
    return submit([name,definition,profile](GuideStore &store){
        const auto directory=QDir(profile).filePath("guide-templates");if(QFileInfo(directory).isSymLink() || !QDir().mkpath(directory))return QVariantMap{{"ok",false},{"error","Cannot create the profile template directory."}};
        if(QDir(directory).entryList({"*.json"},QDir::Files).size()>=64)return QVariantMap{{"ok",false},{"error","The profile already has 64 saved templates."}};
        const auto path=QDir(directory).filePath(QString::fromLatin1(QCryptographicHash::hash(name.toCaseFolded().toUtf8(),QCryptographicHash::Sha256).toHex())+".json");
        if(QFileInfo::exists(path))return QVariantMap{{"ok",false},{"error","That template name already exists. Choose a different name."}};
        auto copy=definition;copy.remove("logo");const auto data=QJsonDocument::fromVariant(QVariantMap{{"schemaVersion",1},{"name",name},{"definition",copy}}).toJson(QJsonDocument::Compact);
        if(!GuideAtomicFile::publish(path,data,[&store]{return !store.cancellationRequested();}))return QVariantMap{{"ok",false},{"error","Cannot atomically save the new template. Previous templates were preserved."}};
        return outcome(true,store);
    },[this](const QVariantMap &result){if(result.value("ok").toBool())refreshRuleTemplates();});
}
bool GuideController::useRuleTemplate(int index) {
    if(!m_valid || busy() || index<0 || index>=m_ruleTemplates.size())return fail(tr("Choose a saved template from this profile."));
    auto definition=m_ruleTemplates[index].toMap().value("definition").toMap();const auto fresh=newDefinition();definition["id"]=fresh.value("id");definition["number"]=fresh.value("number");
    m_importPackage.clear();m_importRemapping.clear();m_previewReady=false;m_candidates.clear();m_previewDefinition.clear();emit duplicatePrepared(definition);emit changed();return true;
}
bool GuideController::setStationLogo(const QString &id,const QString &path,int revision) {
    const auto profile=m_profileDirectory;
    return submit([id,path,revision,profile](GuideStore &store){
        if(path.isEmpty())return outcome(store.setMetadata(id,{{"logo",QString()}},revision),store);
        const QFileInfo info(path);if(!info.isAbsolute() || info.isSymLink() || !info.isFile())return QVariantMap{{"ok",false},{"error","Choose a local PNG or JPEG file."}};
        QFile input(path);if(!input.open(QIODevice::ReadOnly) || input.size()>20*1024*1024)return QVariantMap{{"ok",false},{"error","Station artwork must fit within 20 MiB."}};
        const auto data=input.read(20*1024*1024+1);const auto name=GuideAssets::name(data);
        if(name.isEmpty() || !GuideAssets::write(profile,name,data))return QVariantMap{{"ok",false},{"error","Cannot import this bounded station image. Previous artwork was preserved."}};
        return outcome(store.setMetadata(id,{{"logo",name}},revision),store);
    });
}
bool GuideController::swapNumbers(const QString &id,const QString &otherId,const QVariantMap &revisions){return submit([id,otherId,revisions](GuideStore &store){return outcome(store.swapNumbers(id,otherId,revisions),store);});}
bool GuideController::reorder(const QStringList &ids,const QVariantMap &revisions){return submit([ids,revisions](GuideStore &store){return outcome(store.reorder(ids,revisions),store);});}
bool GuideController::remove(const QString &id,int revision){return submit([id,revision](GuideStore &store){return outcome(store.remove(id,revision),store);});}
bool GuideController::markUnavailable(const QString &kind,int mediaId) {
    if(!QStringList{"movie","episode","musicvideo","song"}.contains(kind) || mediaId<1)return fail(tr("Invalid unavailable-media identity."));
    return submit([kind,mediaId](GuideStore &store){return outcome(store.markUnavailable(kind,mediaId),store);},[this](const QVariantMap &result){
        if(result.value("ok").toBool()){m_status=tr("Unavailable media marked only in affected programs. Select a program and review Repair; other programs keep their times.");emit changed();}
    });
}
bool GuideController::tune(const QString &id,const QString &origin) {
    if(!m_clockWarning.isEmpty())return fail(tr("Review the device clock warning before tuning. Cached listings remain available."));
    if(!ComponentSchema::safeId(id) || (!origin.isEmpty() && !ComponentSchema::safeId(origin)))return fail(tr("Invalid guide selection."));
    return submit([id](GuideStore &store){const auto target=store.tuneTarget(id,now());return QVariantMap{{"ok",!target.isEmpty()},{"target",target},{"error",target.isEmpty()?QString("This channel has no available scheduled program. Check its source or schedule."):QString()}};},
        [this,origin](const QVariantMap &r){if(r.value("ok").toBool())emit targetReady(r.value("target").toMap(),origin);});
}
bool GuideController::acceptDeviceClock() {
    const auto profile=m_profileDirectory;
    return submit([profile](GuideStore &store){if(!GuideClock::save(profile,GuideClock::observation()))return QVariantMap{{"ok",false},{"error","Cannot save the reviewed device-clock choice."}};return outcome(true,store);},[this](const QVariantMap &result){if(result.value("ok").toBool()){m_clockWarning.clear();m_maintenanceAttempted.clear();emit changed();QTimer::singleShot(0,this,&GuideController::maintainSchedules);}});
}
bool GuideController::readDiagnostics() {
    return submit([](GuideStore &store){auto result=outcome(true,store);result["diagnostics"]=store.diagnostics();return result;},[this](const QVariantMap &result){m_diagnostics=result.value("diagnostics").toMap();emit changed();});
}
void GuideController::recordGuidePaint() {
    if(!m_valid || !m_ready || !m_paintPending)return;
    m_paintPending=false;m_firstPaintMs=m_paintTimer.elapsed();
    qInfo().noquote()<<"T11_GUIDE_FIRST_PAINT_MS"<<m_firstPaintMs<<"CHANNELS"<<m_channels.size()<<"VISIBLE_ROWS"<<m_rows.size();emit changed();
}
bool GuideController::setSourceEnabled(const QString &instance,const QVariant &enabled) {
    if(enabled.metaType().id()!=QMetaType::Bool)return fail(tr("Choose Enable or Disable for this source."));
    QVariantMap revisions;for(const auto &v:m_channels){const auto channel=v.toMap();if(channel.contains("owner") && channel.value("source").toMap().value("instance")==instance)revisions[channel.value("id").toString()]=channel.value("revision");}
    return submit([instance,enabled,revisions](GuideStore &store){return outcome(store.setSourceEnabled(instance,enabled.toBool(),revisions),store);});
}
#include "GuideController.moc"
