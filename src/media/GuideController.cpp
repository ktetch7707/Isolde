#include "GuideController.h"
#include "GuideStore.h"
#include "GuideRules.h"
#include "home/ComponentSchema.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
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
QVariantMap outcome(bool ok,GuideStore &store) { return {{"ok",ok},{"error",ok?QString():store.errorString()}}; }
QVariantMap snapshot(GuideStore &store,qint64 from,qint64 to,int first) {
    QVariantList rows; const auto channels=store.channels(); int visible=0;
    for (const auto &v:channels) {
        auto channel=v.toMap(); if (channel.value("hidden").toBool()) continue;
        if (visible++<first) continue;
        if (rows.size()>=50) break;
        const auto id=channel.value("id").toString(); const auto wall=now(),clock=store.broadcastTime(id,wall);
        const auto delta=clock-wall;
        auto programs=store.programs(id,from+delta,to+delta,200);
        for (auto &entry:programs) {
            auto slot=entry.toMap(); slot["displayStartUtc"]=slot.value("startUtc").toLongLong()-delta;
            slot["displayEndUtc"]=slot.value("endUtc").toLongLong()-delta; entry=slot;
        }
        channel["programs"]=programs; channel["current"]=store.tuneTarget(id,wall); channel["sourceName"]="Local"; rows.append(channel);
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
    submit([](GuideStore &store){return outcome(store.resumeClocks(now()),store);},{},true);
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
    m_valid=false;++m_jobId;m_pendingJob={};m_pendingCompletion={};m_pendingDefinition.clear();if(m_cancel)m_cancel->store(true);m_checkpoint.stop();m_catalogDeadline.stop();m_reading=false;closeBridge();
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
void GuideController::setWindow(qint64 from,qint64 to,int first) {
    if(from<86400 || to<=from || to-from>86400 || first<0 || first>1024){fail(tr("Invalid guide time window."));return;}
    m_from=from;m_to=to;m_first=first;refresh();
}
QVariantMap GuideController::newDefinition() const {
    int number=1;for(const auto &v:m_channels)number=std::max(number,v.toMap().value("number").toInt()+1);
    return {{"schemaVersion",1},{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"name",tr("Local channel")},{"number",number},
        {"clock","real-time"},{"sequence","seeded-shuffle"},{"seed",int(QRandomGenerator::global()->bounded(2147483647u))},
        {"timezone",QString::fromUtf8(QTimeZone::systemTimeZoneId())},{"mediaKinds",QVariantList{QString("episode")}},{"rule",QVariantMap{{"field","source"},{"op","is"},{"value","kodi"}}}};
}
bool GuideController::previewLocal(const QVariantMap &definition,const QString &kind) {
    if(!m_valid)return fail(tr("This guide profile is no longer active."));
    QString error;const auto validated=ComponentSchema::guideChannel(definition,&error);if(validated.isEmpty())return fail(error);
    if(!QStringList{"movie","episode","musicvideo","song","mixed"}.contains(kind))return fail(tr("Choose movies, TV episodes, music videos, songs or mixed media."));
    const auto requested=kind=="mixed"?QStringList{"movie","episode"}:QStringList{kind};QSet<QString> selected;
    for(const auto &v:validated.value("mediaKinds").toList())selected.insert(v.toString());
    if(selected!=QSet<QString>(requested.cbegin(),requested.cend()))return fail(tr("The media selection changed. Preview the updated channel settings."));
    if(m_busy) {
        if(m_background && !m_pendingJob && m_pendingDefinition.isEmpty()) {m_pendingDefinition=validated;m_pendingKind=kind;m_status=tr("Waiting for the saved guide refresh…");emit changed();return true;}
        return fail(tr("Finish the current guide operation first."));
    }
    ++m_catalogGeneration;m_previewDefinition=validated;m_candidates.clear();m_previewReady=false;m_skipped=m_page=m_kindIndex=m_snapshotBytes=0;
    m_kinds=kind=="mixed"?QStringList{"movie","episode"}:QStringList{kind};
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
    ++m_catalogGeneration;m_reading=false;m_busy=false;m_previewReady=false;m_catalogDeadline.stop();closeBridge();m_candidates.clear();fail(message);
}
void GuideController::requestPage() {
    closeBridge();m_input.clear();
    m_request=QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",++m_requestId},{"method","Isolde.GuideCandidates"},
        {"params",QJsonObject{{"kind",m_kinds[m_kindIndex]},{"start",m_page},{"end",m_page+50}}}}).toJson(QJsonDocument::Compact)+'\n';
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
    const auto result=reply.value("result").toObject(),limits=result.value("limits").toObject();const auto items=result.value("items").toArray();
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
void GuideController::cancel() {
    ++m_catalogGeneration;
    m_pendingJob={};m_pendingCompletion={};m_pendingDefinition.clear();m_pendingKind.clear();
    if(m_cancel)m_cancel->store(true);
    if(m_reading){m_reading=m_busy=false;m_catalogDeadline.stop();closeBridge();}
    m_previewReady=false;m_candidates.clear();m_status=tr("Cancelled. Saved channels are unchanged.");emit changed();
}
bool GuideController::setPreferences(const QString &id,const QVariantMap &p,int revision){return submit([id,p,revision](GuideStore &store){return outcome(store.setPreferences(id,p,revision),store);});}
bool GuideController::setMetadata(const QString &id,const QVariantMap &p,int revision){return submit([id,p,revision](GuideStore &store){return outcome(store.setMetadata(id,p,revision),store);});}
bool GuideController::reorder(const QStringList &ids,const QVariantMap &revisions){return submit([ids,revisions](GuideStore &store){return outcome(store.reorder(ids,revisions),store);});}
bool GuideController::remove(const QString &id,int revision){return submit([id,revision](GuideStore &store){return outcome(store.remove(id,revision),store);});}
bool GuideController::tune(const QString &id,const QString &origin) {
    if(!ComponentSchema::safeId(id) || (!origin.isEmpty() && !ComponentSchema::safeId(origin)))return fail(tr("Invalid guide selection."));
    return submit([id](GuideStore &store){const auto target=store.tuneTarget(id,now());return QVariantMap{{"ok",!target.isEmpty()},{"target",target},{"error",target.isEmpty()?QString("This channel has no available scheduled program. Check its source or schedule."):QString()}};},
        [this,origin](const QVariantMap &r){if(r.value("ok").toBool())emit targetReady(r.value("target").toMap(),origin);});
}
#include "GuideController.moc"
