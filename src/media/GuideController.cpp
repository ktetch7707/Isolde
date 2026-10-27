#include "GuideController.h"
#include "GuideStore.h"
#include "GuideRules.h"
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
        channel["programs"]=programs; channel["current"]=store.tuneTarget(id,wall); channel["sourceName"]=channel.value("owner")=="kodi-pvr"?"Kodi PVR":"Local"; rows.append(channel);
    }
    QVariantList compact;
    for(const auto &v:channels) {
        auto channel=v.toMap();if(channel.value("hidden").toBool())continue;if(compact.size()>=5)break;
        const auto id=channel.value("id").toString();channel["current"]=store.tuneTarget(id,now());compact.append(channel);
    }
    return {{"channels",channels},{"rows",rows},{"compactRows",compact},{"ready",store.ready()}};
}
}
GuideController::GuideController(const QString &path,const QString &profile,QObject *parent,const QString &bridgeDirectory)
    :QObject(parent),m_directory(bridgeDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        +"/native-profiles/"+profile+"/kodi/home/.kodi/userdata/isolde":bridgeDirectory) {
    m_from=now(); m_to=m_from+7200;
    m_worker=new GuideWorker(path); m_worker->moveToThread(&m_thread);
    connect(&m_thread,&QThread::finished,m_worker,&QObject::deleteLater);
    connect(m_worker,&GuideWorker::completed,this,[this](quint64 id,const QVariantMap &result) {
        if (!m_valid || id!=m_jobId) return;
        m_busy=m_background=false; m_ready=result.value("ready").toBool(); m_channels=result.value("channels").toList();m_rows=result.value("rows").toList();m_compactRows=result.value("compactRows").toList();
        m_error=result.value("error").toString(); m_status=m_error.isEmpty()?tr("Saved guide ready"):m_error;
        auto completed=std::move(m_completion); m_completion={}; emit changed(); if (completed) completed(result);
        if(!m_busy && !m_pendingDefinition.isEmpty()) {const auto definition=m_pendingDefinition;const auto kind=m_pendingKind;m_pendingDefinition.clear();m_pendingKind.clear();previewLocal(definition,kind);return;}
        if(!m_busy && m_pendingJob) {auto job=std::move(m_pendingJob);auto completion=std::move(m_pendingCompletion);m_pendingJob={};m_pendingCompletion={};submit(std::move(job),std::move(completion));return;}
        if (m_refreshPending && !m_busy) {m_refreshPending=false;refresh();}
    });
    m_thread.start();
    submit([](GuideStore &store){return outcome(store.resumeClocks(now()),store);},[this](const QVariantMap &){QTimer::singleShot(0,this,&GuideController::maintainSchedules);},true);
    m_maintenance.setInterval(60000);
    connect(&m_maintenance,&QTimer::timeout,this,[this]{m_maintenanceAttempted.clear();maintainSchedules();});
    m_maintenance.start();
    m_checkpoint.setInterval(30000);
    connect(&m_checkpoint,&QTimer::timeout,this,[this]{if(m_valid && m_ready && !m_busy)submit([](GuideStore &store){return outcome(store.setActive(true,now()),store);},{},true);});
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
    m_thread.quit();m_thread.wait();m_busy=false;m_ready=false;m_channels.clear();m_rows.clear();m_compactRows.clear();m_candidates.clear();emit changed();
}
bool GuideController::fail(const QString &message) {m_error=message;emit changed();return false;}
bool GuideController::submit(Job job,std::function<void(const QVariantMap &)> completion,bool background) {
    if(!m_valid)return fail(tr("This guide profile is no longer active."));
    if(m_busy) {
        if(m_background && !background && !m_pendingJob && m_pendingDefinition.isEmpty()) {m_pendingJob=std::move(job);m_pendingCompletion=std::move(completion);m_status=tr("Waiting for the saved guide refresh…");emit changed();return true;}
        return fail(tr("Finish or cancel the current guide operation first."));
    }
    m_busy=true;m_background=background;m_error.clear();m_completion=std::move(completion);const auto id=++m_jobId;
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
    if(!m_valid || !m_ready || busy())return;
    const auto wall=now();
    for(const auto &v:m_channels){
        const auto channel=v.toMap();const auto id=channel.value("id").toString();
        if(channel.value("owner")=="kodi-pvr")continue;
        const auto clock=channel.value("clock")=="real-time"?wall:channel.value("clockUtc").toLongLong()
            +(channel.value("active").toBool()?std::max<qint64>(0,wall-channel.value("anchorUtc").toLongLong()):0);
        if(channel.value("generatedThroughUtc").toLongLong()>=clock+6*86400 || m_maintenanceAttempted.contains(id))continue;
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
        const auto params=m_sourceIndex<0?QVariantMap{{"channelgroupid","alltv"},{"properties",QVariantList{"channelnumber","locked","hidden","clientid"}},{"limits",QVariantMap{{"start",m_page},{"end",m_page+50}}}}
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
        const auto item=v.toObject().toVariantMap();if(!GuideRules::matches(m_previewDefinition.value("rule").toMap(),item))continue;
        m_snapshotBytes+=QJsonDocument::fromVariant(item).toJson(QJsonDocument::Compact).size()+1;
        if(m_candidates.size()>=10000 || m_snapshotBytes>4*1024*1024-2){readFailure(tr("More media matches than one channel can hold. Narrow the rules before saving."));return;}
        m_candidates.append(item);
    }
    m_skipped+=skipped;closeBridge();
    if(end<total)m_page=end;else{++m_kindIndex;m_page=0;}
    if(m_kindIndex<m_kinds.size()){m_status=tr("Previewing local media · %1 matches").arg(m_candidates.size());emit changed();const auto generation=m_catalogGeneration;QTimer::singleShot(0,this,[this,generation]{if(m_valid && m_reading && generation==m_catalogGeneration)requestPage();});return;}
    m_catalogDeadline.stop();m_reading=m_busy=false;m_previewReady=!m_candidates.isEmpty();
    m_status=tr("%1 matching items · %2 items skipped (remote or unknown duration)").arg(m_candidates.size()).arg(m_skipped);
    if(!m_previewReady)m_error=tr("No playable local media matches. Adjust the rules or update Kodi's metadata.");emit changed();
}
bool GuideController::createLocal(const QVariantMap &definition) {
    if(!m_previewReady || definition!=m_previewDefinition)return fail(tr("Preview these exact channel settings before saving."));
    const auto candidates=m_candidates;const auto id=definition.value("id").toString();
    return submit([definition,candidates](GuideStore &store){return outcome(store.createLocal(definition,candidates,now()),store);},[this,id](const QVariantMap &r){if(r.value("ok").toBool()){m_previewReady=false;m_candidates.clear();emit channelCreated(id);emit changed();}});
}
QVariantMap GuideController::editableDefinition(const QString &id) const {
    if(!m_valid || !ComponentSchema::safeId(id))return {};
    for(const auto &v:m_channels){auto c=v.toMap();if(c.value("id")!=id || c.contains("owner"))continue;
        for(const auto &key:QStringList{"revision","hidden","favorite","position","anchorUtc","clockUtc","active","generatedThroughUtc"})c.remove(key);
        if(!c.contains("mediaKinds"))c["mediaKinds"]=QVariantList{"movie","episode","musicvideo","song"};
        return ComponentSchema::guideChannel(c);
    }
    return {};
}
bool GuideController::updateLocal(const QVariantMap &definition,int revision) {
    if(!m_previewReady || definition!=m_previewDefinition)return fail(tr("Preview these exact updated rules before saving."));
    const auto candidates=m_candidates;const auto id=definition.value("id").toString();
    return submit([definition,candidates,revision](GuideStore &s){return outcome(s.updateRules(definition,candidates,revision),s);},[this,id](const QVariantMap &r){if(r.value("ok").toBool()){m_previewReady=false;m_candidates.clear();emit channelCreated(id);emit changed();}});
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
bool GuideController::previewPvr() {
    if(!m_valid)return fail(tr("This guide profile is no longer active."));
    if(m_busy){
        if(m_background)return submit([](GuideStore &s){return outcome(true,s);},[this](const QVariantMap &){previewPvr();});
        return fail(tr("Finish or cancel the current guide operation first."));
    }
    ++m_catalogGeneration;m_sourcePreview.clear();m_sourceRevisions.clear();
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
    return submit([entries,revisions,refreshed](GuideStore &store){return outcome(store.importPvr("kodi-pvr",entries,refreshed,revisions),store);},[this](const QVariantMap &r){if(r.value("ok").toBool()){m_sourcePreviewReady=false;emit changed();}});
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
    m_previewReady=m_sourcePreviewReady=false;m_candidates.clear();m_sourcePreview.clear();m_regenerationReview.clear();m_status=m_busy?tr("Cancellation requested. Waiting for the current transaction result…"):tr("Preview cancelled.");emit changed();
}
bool GuideController::setPreferences(const QString &id,const QVariantMap &p,int revision){return submit([id,p,revision](GuideStore &store){return outcome(store.setPreferences(id,p,revision),store);});}
bool GuideController::setMetadata(const QString &id,const QVariantMap &p,int revision){return submit([id,p,revision](GuideStore &store){return outcome(store.setMetadata(id,p,revision),store);});}
bool GuideController::swapNumbers(const QString &id,const QString &otherId,const QVariantMap &revisions){return submit([id,otherId,revisions](GuideStore &store){return outcome(store.swapNumbers(id,otherId,revisions),store);});}
bool GuideController::reorder(const QStringList &ids,const QVariantMap &revisions){return submit([ids,revisions](GuideStore &store){return outcome(store.reorder(ids,revisions),store);});}
bool GuideController::remove(const QString &id,int revision){return submit([id,revision](GuideStore &store){return outcome(store.remove(id,revision),store);});}
bool GuideController::tune(const QString &id,const QString &origin) {
    if(!ComponentSchema::safeId(id) || (!origin.isEmpty() && !ComponentSchema::safeId(origin)))return fail(tr("Invalid guide selection."));
    return submit([id](GuideStore &store){const auto target=store.tuneTarget(id,now());return QVariantMap{{"ok",!target.isEmpty()},{"target",target},{"error",target.isEmpty()?QString("This channel has no available scheduled program. Check its source or schedule."):QString()}};},
        [this,origin](const QVariantMap &r){if(r.value("ok").toBool())emit targetReady(r.value("target").toMap(),origin);});
}
#include "GuideController.moc"
