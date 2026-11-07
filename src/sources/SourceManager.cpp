#include "sources/SourceManager.h"
#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "home/PageModel.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDirIterator>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>

namespace {

bool unredirectedStatePath(const QString &path)
{
    const QFileInfo file(path);
    if (!file.isAbsolute() || file.isSymLink()) return false;
    QDir directory(file.absolutePath());
    do { if (QFileInfo(directory.absolutePath()).isSymLink()) return false; } while (directory.cdUp());
    return true;
}

QString stableSourceId(const QString &path)
{
    const auto digest = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("source-") + QString::fromLatin1(digest);
}

QString classifyPath(const QFileInfo &info)
{
    const auto name = info.fileName().toLower();
    const QDir directory(info.absoluteFilePath());
    if (info.isDir() && (directory.exists(QStringLiteral("VIDEO_TS"))
                         || directory.exists(QStringLiteral("BDMV"))
                         || name.contains(QStringLiteral("dvd"))
                         || name.contains(QStringLiteral("bluray"))
                         || name.contains(QStringLiteral("blu-ray"))))
        return QStringLiteral("movie-disc");
    if (info.isDir() && (directory.exists(QStringLiteral("steamapps"))
                         || directory.exists(QStringLiteral("PS3_GAME"))
                         || directory.exists(QStringLiteral("PS4UPDATE.PUP"))
                         || name.contains(QStringLiteral("game"))
                         || name.contains(QStringLiteral("ps5"))
                         || name.contains(QStringLiteral("xbox"))))
        return QStringLiteral("game-disc");
    if (info.isDir())
        return QStringLiteral("media-folder");
    return QStringLiteral("file-source");
}

QStringList defaultProbeRoots()
{
    const auto user = qEnvironmentVariable("USER");
    QStringList roots;
    if (!user.isEmpty()) {
        roots << QStringLiteral("/media/") + user
              << QStringLiteral("/run/media/") + user;
    }
    roots << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return roots;
}

QString actionTypeForPath(const QFileInfo &info)
{
    if (info.isDir())
        return QStringLiteral("open-folder");
    return QStringLiteral("open-file");
}

QString symbolForPath(const QFileInfo &info)
{
    const auto classification = classifyPath(info);
    if (classification == QStringLiteral("movie-disc"))
        return QStringLiteral("◉");
    if (classification == QStringLiteral("game-disc"))
        return QStringLiteral("◆");
    if (info.isDir())
        return QStringLiteral("▣");
    return QStringLiteral("▤");
}

QVariantMap sourceTileForPath(const QFileInfo &info, const QString &openWith)
{
    const auto path = info.absoluteFilePath();
    const auto actionType = actionTypeForPath(info);
    const auto classification = classifyPath(info);
    const auto name = info.fileName().isEmpty() ? path : info.fileName();
    const auto presentationMode = classification == QStringLiteral("game-disc")
        ? QStringLiteral("disc-game")
        : classification == QStringLiteral("movie-disc") ? QStringLiteral("disc-media") : QStringLiteral("media");
    const QVariantMap action{{QStringLiteral("type"), actionType},
                             {QStringLiteral("target"), path},
                             {QStringLiteral("openWith"), openWith}};
    return ComponentSchema::component(QVariantMap{{QStringLiteral("kind"), QStringLiteral("action")},
                       {QStringLiteral("contextual"), true},
                       {QStringLiteral("id"), stableSourceId(path)},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("section"), QStringLiteral("Detected Sources")},
                       {QStringLiteral("sourceType"), classification},
                       {QStringLiteral("target"), path},
                       {QStringLiteral("openWith"), openWith},
                       {QStringLiteral("symbol"), symbolForPath(info)},
                       {QStringLiteral("accent"), classification == QStringLiteral("game-disc") ? QStringLiteral("#8B5CF6") : QStringLiteral("#06B6D4")},
                       {QStringLiteral("presentationMode"), presentationMode},
                       {QStringLiteral("networkRequirement"), QStringLiteral("none")},
                       {QStringLiteral("size"), QStringLiteral("standard")},
                       {QStringLiteral("actionType"), actionType},
                       {QStringLiteral("action"), action}}, true);
}

} // namespace

SourceManager::SourceManager(AppRegistry *registry, QObject *parent, const QString &statePath)
    : QObject(parent), m_registry(registry), m_statePath(statePath)
{
    const auto profile=qApp->property("isoldeProfile").toString();
    if (m_statePath.isEmpty()) {
        if (!profile.isEmpty() && profile!="default" && !ComponentSchema::safeId(profile)) {
            m_writable=false; m_error=tr("Invalid source profile identity.");
        } else m_statePath=QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
            .filePath(profile.isEmpty() || profile=="default" ? "sources.json" : "profiles/"+profile+"/sources.json");
    }
    loadState(); m_probeRoots=defaultProbeRoots();
    m_refreshTimer.setInterval(1000);
    connect(&m_refreshTimer,&QTimer::timeout,this,[this] {
        // The clock expires notices without turning a timer tick into a new event.
        const auto duration=policy().value("duration").toInt();
        if (duration) for (auto it=m_events.begin();it!=m_events.end();++it)
            if (it.value()>0 && QDateTime::currentMSecsSinceEpoch()-it.value()>=qint64(duration)*1000) { it.value()=0; ++m_revision; emit sourcesChanged(); }
        if (++m_scanTicks%5==0) refresh();
    });
    m_refreshTimer.start();
    if (registry) connect(registry,&AppRegistry::appsChanged,this,&SourceManager::refresh);
    refresh();
}
void SourceManager::setLayout(HomeLayout *layout)
{
    if (m_layout) disconnect(m_layout,nullptr,this,nullptr);
    m_layout=layout;
    if (layout) connect(layout,&HomeLayout::layoutChanged,this,[this] { ++m_revision; emit sourcesChanged(); });
    refresh(); ++m_revision; emit sourcesChanged();
}
QVariantMap SourceManager::policy() const
{
    static const QVariantMap defaults=[] {
        QFile file(":/config/studio-defaults.json");
        if (!file.open(QIODevice::ReadOnly)) {
            file.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath("config/studio-defaults.json"));
            if (!file.open(QIODevice::ReadOnly)) return QVariantMap{};
        }
        if (file.size()>256*1024) return QVariantMap{};
        const auto value=QJsonDocument::fromJson(file.readAll()).object().value("sourcePolicyDefaults").toObject().toVariantMap();
        return ComponentSchema::validSourcePolicy(value)?value:QVariantMap{};
    }();
    QVariantMap result=defaults;
    if (m_layout) {
        const auto authored=m_layout->homeAppearance().value("sourcePolicy").toMap();
        for (auto it=authored.cbegin();it!=authored.cend();++it) result[it.key()]=it.value();
        if (!authored.contains("mode") && !m_automaticSourcesEnabled) result["mode"]="off";
    } else if (!m_automaticSourcesEnabled) result["mode"]="off";
    return result;
}
bool SourceManager::automaticSourcesEnabled() const { return policy().value("mode").toString()!="off"; }
void SourceManager::setAutomaticSourcesEnabled(bool enabled)
{
    if (m_layout) { setPolicy({{"mode",enabled?"compact":"off"}}); return; }
    const bool before=m_automaticSourcesEnabled; m_automaticSourcesEnabled=enabled;
    if (!saveState()) { m_automaticSourcesEnabled=before; return; }
    m_error.clear();
    ++m_revision; emit sourcesChanged();
}
bool SourceManager::fail(const QString &message) { m_error=message; ++m_revision; emit sourcesChanged(); return false; }
bool SourceManager::setPolicy(const QVariantMap &changes)
{
    if (!m_layout) return fail(tr("Open this profile's Home before editing its display policy."));
    auto next=policy(); for (auto it=changes.cbegin();it!=changes.cend();++it) next[it.key()]=it.value();
    if (!ComponentSchema::validSourcePolicy(next)) return fail(tr("Invalid source policy. Nothing changed."));
    bool saved=false;
    if (m_layout->homePageId().isEmpty()) saved=m_layout->setAppearance({{"sourcePolicy",next}});
    else if (auto *page=qobject_cast<PageModel *>(m_layout->pageModel(m_layout->homePageId()))) saved=page->setAppearance({{"sourcePolicy",next}});
    if (!saved) return fail(m_layout->errorString().isEmpty()?tr("Could not save source policy. Retry."):m_layout->errorString());
    m_error.clear(); refresh(); ++m_revision; emit sourcesChanged(); return true;
}
QVariantList SourceManager::sources() const {
    QVariantList result;
    for (const auto &value:m_discovered) if (!m_decisions.value(value.toMap().value("id").toString()).toMap().value("hidden").toBool()) result.append(value);
    return result;
}
QVariantList SourceManager::items() const { return automaticSourcesEnabled()?m_sources:QVariantList{}; }
QVariantList SourceManager::sections() const { return {}; }
int SourceManager::visibleSourceCount() const { return items().size(); }
QVariantList SourceManager::hiddenSources() const
{
    QVariantList result;
    for (const auto &value:m_discovered) if (m_decisions.value(value.toMap().value("id").toString()).toMap().value("hidden").toBool()) result.append(value);
    return result;
}
void SourceManager::refresh()
{
    QString error; const auto next=policy().value("discoveryEnabled",true).toBool()?scanSources(&error):QVariantList{};
    if (!error.isEmpty()) { fail(error); return; }
    QSet<QString> present; const auto rules=policy(); QVariantList visible;
    for (const auto &value:next) {
        const auto source=value.toMap(); const auto id=source.value("id").toString(); present.insert(id);
        if (!m_events.contains(id) && (!m_seen.contains(id) || rules.value("reconnect",true).toBool())) {
            if (m_seen.size()>=1024 && !m_seen.contains(id)) { fail(tr("Source event history is full. Restart this profile before detecting more devices.")); return; }
            m_events[id]=QDateTime::currentMSecsSinceEpoch();
        }
        m_seen.insert(id);
        const auto decision=m_decisions.value(id).toMap();
        if (source.value("compatibleHandler").toBool() && !decision.value("hidden").toBool() && !decision.value("permanent").toBool()) visible.append(value);
    }
    for (const auto &id:m_events.keys()) if (!present.contains(id)) { m_events.remove(id); m_dismissed.remove(id); }
    if (next==m_discovered && visible==m_sources) return;
    m_discovered=next; m_sources=visible; ++m_revision; emit sourcesChanged();
}
QVariantList SourceManager::projected(const QString &pageId,const QString &slotId,bool notice,const QVariantMap &workflow) const
{
    const auto rules=policy();
    if (!automaticSourcesEnabled() || rules.value("mode")!=(notice?"compact":"widget")) return {};
    if ((workflow.value("playback").toBool() && rules.value("suppressPlayback").toBool())
        || (workflow.value("setup").toBool() && rules.value("suppressSetup").toBool())
        || (workflow.value("editing").toBool() && rules.value("suppressEditing").toBool())) return {};
    const auto pages=rules.value("pages").toList(),slotIds=rules.value("slots").toList();
    if (!pages.contains(pageId) && !(pages.contains("home") && m_layout && pageId==m_layout->homePageId())) return {};
    if (!slotIds.isEmpty() && !slotIds.contains(slotId)) return {};
    QVariantList result;
    for (const auto &value:m_sources) {
        const auto source=value.toMap(); const auto id=source.value("id").toString();
        if (m_dismissed.contains(id) || !rules.value("events").toList().contains(source.value("sourceType"))) continue;
        if (notice && (!m_events.contains(id) || (rules.value("duration").toInt() && (!m_events.value(id) || QDateTime::currentMSecsSinceEpoch()-m_events.value(id)>=qint64(rules.value("duration").toInt())*1000)))) continue;
        result.append(value);
    }
    return result;
}
bool SourceManager::decide(const QString &id,const QString &key,const QVariant &value)
{
    bool known=m_decisions.contains(id);
    for (const auto &entry:m_discovered) known|=entry.toMap().value("id")==id;
    if (!ComponentSchema::safeId(id) || !known) return fail(tr("This source is no longer available."));
    if (!m_decisions.contains(id) && m_decisions.size()>=512) return fail(tr("Restore an old source decision before saving another."));
    const auto before=m_decisions; auto decision=m_decisions.value(id).toMap();
    if (value.isNull()) decision.remove(key); else decision[key]=value;
    if (decision.isEmpty()) m_decisions.remove(id); else m_decisions[id]=decision;
    if (!saveState()) { m_decisions=before; return false; }
    m_error.clear(); refresh(); ++m_revision; emit sourcesChanged(); return true;
}
bool SourceManager::hideSource(const QString &id) { return decide(id,"hidden",true); }
bool SourceManager::markPermanent(const QString &id) { return decide(id,"permanent",true); }
bool SourceManager::forgetSourceDecision(const QString &id)
{
    if (!m_decisions.contains(id)) return ComponentSchema::safeId(id);
    const auto before=m_decisions; m_decisions.remove(id);
    if (!saveState()) { m_decisions=before; return false; }
    m_dismissed.remove(id); m_events[id]=QDateTime::currentMSecsSinceEpoch(); m_error.clear(); refresh(); ++m_revision; emit sourcesChanged(); return true;
}
bool SourceManager::dismissSource(const QString &id)
{
    if (sourceAction(id).isEmpty()) return fail(tr("This source disconnected. Reconnect it and retry."));
    m_dismissed.insert(id); ++m_revision; emit sourcesChanged(); return true;
}
QVariantMap SourceManager::sourceAction(const QString &id) const
{
    for (const auto &value:m_discovered) if (value.toMap().value("id")==id && value.toMap().value("compatibleHandler").toBool()) return value.toMap().value("action").toMap();
    return {};
}
QVariantList SourceManager::handlers(const QString &id) const
{
    QVariantList result; const auto action=sourceAction(id);
    if (!m_registry || action.isEmpty()) return result;
    for (const auto &value:m_registry->apps()) if (value.toMap().value("handledActions").toStringList().contains(action.value("type").toString())) result.append(value);
    return result;
}
bool SourceManager::chooseHandler(const QString &id,const QString &handler)
{
    for (const auto &value:handlers(id)) if (value.toMap().value("id")==handler) return decide(id,"handler",handler);
    return fail(tr("Choose an installed compatible handler."));
}
bool SourceManager::promoteSource(const QString &id)
{
    if (!m_layout) return fail(tr("Home is unavailable for this profile."));
    // A durable decision makes repeated confirmation idempotent.
    if (m_decisions.value(id).toMap().value("permanent").toBool()) return true;
    const auto action=sourceAction(id);
    if (action.isEmpty()) return fail(tr("This source disconnected. Reconnect it and retry."));
    bool exists=false;
    for (const auto &value:m_layout->components()) if (value.toMap().value("action").toMap()==action) exists=true;
    if (!exists) {
        QString name; for (const auto &value:m_sources) if (value.toMap().value("id")==id) name=value.toMap().value("name").toString();
        if (!m_layout->addActionTile(name,action.value("type").toString(),action.value("target").toString(),{},action.value("openWith").toString())) return fail(m_layout->errorString());
    }
    return markPermanent(id); // If the decision write fails, retry finds the already durable tile.
}
void SourceManager::setProbeRootsForTests(const QStringList &roots) { m_probeRoots=roots; refresh(); }
QVariantList SourceManager::scanSources(QString *error) const
{
    QVariantList result; QSet<QString> seen; int examined=0;
    const auto permittedTypes=policy().value("types").toList();
    for (const auto &root:m_probeRoots) {
        if (QFileInfo(root).isSymLink()) continue;
        QDirIterator entries(root,QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot,QDirIterator::NoIteratorFlags);
        while (entries.hasNext()) {
            entries.next(); const auto entry=entries.fileInfo();
            if (++examined>4096) { *error=tr("Source scan exceeds 4096 entries. Narrow the configured media roots."); return {}; }
            if (entry.isSymLink()) continue;
            const auto type=classifyPath(entry);
            if (!permittedTypes.contains(type)) continue;
            const auto id=stableSourceId(entry.absoluteFilePath()); if (seen.contains(id)) continue; seen.insert(id);
            if (seen.size()>512) { *error=tr("More than 512 detected sources. Narrow the configured media roots."); return {}; }
            const auto actionType=actionTypeForPath(entry);
            const auto chosen=m_decisions.value(id).toMap().value("handler").toString();
            const auto handler=chosen.isEmpty()?defaultHandlerForAction(actionType):chosen;
            auto tile=sourceTileForPath(entry,handler);
            if (tile.isEmpty()) { *error=tr("A detected source has an invalid name or action."); return {}; }
            tile["compatibleHandler"]=!handler.isEmpty();
            tile["unavailableReason"]=handler.isEmpty()?tr("No compatible installed app. Install a handler to open this source."):ComponentSchema::unavailable(tile.value("action").toMap(),m_registry,{});
            result.append(tile);
        }
    }
    return result;
}
QString SourceManager::defaultHandlerForAction(const QString &type) const
{
    if (m_registry) for (const auto &value:m_registry->apps()) if (value.toMap().value("handledActions").toStringList().contains(type)) return value.toMap().value("id").toString();
    return {};
}
void SourceManager::loadState()
{
    if (!m_writable) return;
    if (!unredirectedStatePath(m_statePath)) { m_writable=false; fail(tr("Source decision storage is redirected. No file was read or changed.")); return; }
    QFile file(m_statePath);
    if (!file.exists()) {
        // Read-only compatibility with the old Household settings. New profiles never read that group's decisions.
        const auto profile=qApp->property("isoldeProfile").toString();
        if (profile.isEmpty() || profile=="default") {
            QSettings settings; settings.beginGroup("sources");
            m_automaticSourcesEnabled=settings.value("automaticSourcesEnabled",true).toBool();
            for (const auto &key:{"hiddenSources","permanentSources"}) for (const auto &id:settings.value(key).toStringList()) {
                if (ComponentSchema::safeId(id) && m_decisions.size()<512) { auto decision=m_decisions.value(id).toMap(); decision[key==QString("hiddenSources")?"hidden":"permanent"]=true; m_decisions[id]=decision; }
            }
        }
        return;
    }
    auto invalid=[this] { m_writable=false; fail(tr("Saved source decisions are invalid. The original file was preserved.")); };
    if (!file.open(QIODevice::ReadOnly) || file.size()>256*1024) { invalid(); return; }
    const auto doc=QJsonDocument::fromJson(file.readAll());
    const auto root=doc.object();
    if (!doc.isObject() || root.size()!=3 || root.value("schemaVersion").toDouble()!=1 || !root.value("automaticSourcesEnabled").isBool() || !root.value("decisions").isObject() || root.value("decisions").toObject().size()>512) { invalid(); return; }
    const auto decisions=root.value("decisions").toObject().toVariantMap();
    for (auto it=decisions.cbegin();it!=decisions.cend();++it) {
        if (!ComponentSchema::safeId(it.key()) || it.value().metaType().id()!=QMetaType::QVariantMap) { invalid(); return; }
        const auto value=it.value().toMap();
        for (auto v=value.cbegin();v!=value.cend();++v) {
            if (v.key()=="hidden" || v.key()=="permanent") { if (v.value().metaType().id()!=QMetaType::Bool) { invalid(); return; } }
            else if (v.key()=="handler") { if (v.value().metaType().id()!=QMetaType::QString || !ComponentSchema::safeId(v.value().toString())) { invalid(); return; } }
            else { invalid(); return; }
        }
    }
    m_automaticSourcesEnabled=root.value("automaticSourcesEnabled").toBool(); m_decisions=decisions;
}
bool SourceManager::saveState()
{
    if (!m_writable) return fail(tr("Saved source decisions need repair. No file was changed."));
    if (!unredirectedStatePath(m_statePath)) return fail(tr("Source decision storage is redirected. No file was changed."));
    const auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",1},{"automaticSourcesEnabled",m_automaticSourcesEnabled},{"decisions",QJsonObject::fromVariantMap(m_decisions)}}).toJson();
    if (bytes.size()>256*1024 || !QDir().mkpath(QFileInfo(m_statePath).absolutePath())) return fail(tr("Cannot save source decisions. Nothing changed; retry."));
    QSaveFile file(m_statePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) return fail(tr("Cannot save source decisions. Nothing changed; retry."));
    return true;
}
