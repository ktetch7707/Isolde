#include "home/StudioSession.h"
#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QFile>
#include <QDirIterator>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QFileInfo>
#include <QUrl>
#include <QImageReader>
#include <QBuffer>
#include <QCryptographicHash>
#include <algorithm>
#include <functional>

StudioSession::StudioSession(QObject *parent)
    : QObject(parent)
{
    connect(this, &StudioSession::changed, this, [this] {
        if (m_model && !m_draft.isEmpty() && m_model->documentData() != m_draft) {
            m_model->loadDocument(m_draft, false);
            emit m_model->layoutChanged();
        }
    });
}

QObject *StudioSession::model() const { return m_model; }
StudioSession::~StudioSession(){delete m_assetStage;}
QVariantList StudioSession::artworkChoices() const
{
    QVariantList result;if(!m_active || !m_live)return result;
    QSet<QString> paths;
    const auto append=[&](const QString &path){if(result.size()>=256 || paths.contains(path))return;QImageReader reader(path);const auto size=reader.size();if(!size.isValid() || size.width()>16384 || size.height()>16384 || qint64(size.width())*size.height()>32*1024*1024)return;
        paths.insert(path);result.append(QVariantMap{{"name",QFileInfo(path).completeBaseName()},{"source",QUrl::fromLocalFile(path).toString()},{"width",size.width()},{"height",size.height()}});
    };
    const auto directory=QFileInfo(m_profilePath).absolutePath();
    const QStringList roots{QDir(directory).filePath("artwork"),QDir(QFileInfo(m_live->m_defaultPath).absolutePath()).filePath("skins"),QDir(QFileInfo(m_live->m_defaultPath).absolutePath()).filePath("artwork")};
    for(const auto &root:roots){QDirIterator files(root,{"*.png","*.jpg","*.jpeg"},QDir::Files,QDirIterator::Subdirectories);int inspected=0;while(files.hasNext() && ++inspected<=512 && result.size()<256){const auto path=files.next(),canonical=QFileInfo(path).canonicalFilePath(),base=QFileInfo(root).canonicalFilePath();if(!canonical.isEmpty() && !base.isEmpty() && !QDir(base).relativeFilePath(canonical).startsWith("../"))append(canonical);}}
    for(const auto &path:m_stagedAssets)append(path);
    return result;
}
QString StudioSession::stageArtwork(const QString &source)
{
    if(!m_active || !m_live){setError(tr("Reopen Studio before importing artwork."));return {};}
    const QUrl url(source);const auto path=url.isLocalFile()?url.toLocalFile():url.scheme().isEmpty()?source:QString();
    QFile input(path);if(path.isEmpty() || !input.open(QIODevice::ReadOnly) || input.size()<8 || input.size()>20*1024*1024){setError(tr("Choose a local PNG or JPEG of at most 20 MiB."));return {};}
    const auto data=input.readAll();QBuffer buffer;buffer.setData(data);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);const auto format=reader.format();const auto size=reader.size();
    if(!QList<QByteArray>{"png","jpeg","jpg"}.contains(format) || !size.isValid() || size.width()>16384 || size.height()>16384 || qint64(size.width())*size.height()>32*1024*1024 || reader.read().isNull()){setError(tr("Artwork must be a valid PNG/JPEG within 16384 pixels and 32 megapixels."));return {};}
    const auto relative="artwork/studio-"+QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex())+(format=="png"?".png":".jpg");
    if(m_stagedAssets.contains(relative))return relative;
    qint64 total=data.size();for(const auto &path:m_stagedAssets)total+=QFileInfo(path).size();
    if(m_stagedAssets.size()>=128 || total>64*1024*1024){setError(tr("This draft's artwork budget is 128 images / 64 MiB. Apply or discard before importing more."));return {};}
    if(!m_assetStage)m_assetStage=new QTemporaryDir;
    if(!m_assetStage->isValid()){setError(tr("Could not create private artwork staging."));return {};}
    const auto target=QDir(m_assetStage->path()).filePath(QFileInfo(relative).fileName());QSaveFile output(target);
    if(!output.open(QIODevice::WriteOnly) || output.write(data)!=data.size() || !output.commit()){setError(tr("Could not stage artwork. Nothing applied."));return {};}
    m_stagedAssets[relative]=target;if(m_model)m_model->m_assetPaths[relative]=target;
    setError({});if(m_model)emit m_model->layoutChanged();return relative;
}
QVariantList StudioSession::sourceChoices(const QString &id,const QString &pageId) const
{
    if(!m_active || !m_live)return {};
    HomeLayout worker(m_live->registry());configureWorker(worker);
    auto document=QJsonDocument::fromJson(m_draft).object();
    if(!pageId.isEmpty()){bool found=false;for(const auto &entry:document["pages"].toArray())if(entry.toObject()["id"]==pageId){document=entry.toObject()["layout"].toObject();found=true;break;}if(!found)return {};}
    if(!worker.loadDocument(QJsonDocument(document).toJson(),false))return {};
    return worker.sourceChoices(id);
}
QVariantList StudioSession::actionChoices() const
{
    QVariantList choices;if(!m_active || !m_live)return choices;
    for(const auto &entry:m_live->studioOptions().value("actions").toList())choices.append(entry);
    const auto document=QJsonDocument::fromJson(m_draft).object();const auto root=document["rootPage"].toObject();
    if(!root.contains("visible") || root["visible"].toBool())choices.append(QVariantMap{{"name",tr("Open page · ")+root.value("name").toString(m_live->studioOptions().value("rootPageName").toString())},{"action",QVariantMap{{"type","root-page"}}}});
    for(const auto &entry:document["pages"].toArray()){const auto page=entry.toObject();choices.append(QVariantMap{{"name",tr("Open page · ")+page["name"].toString()},{"action",QVariantMap{{"type","open-page"},{"pageId",page["id"].toString()}}}});}
    for(const auto &entry:m_live->registry()->apps()){const auto app=entry.toMap();const auto id=app.value("id").toString();choices.append(QVariantMap{{"name",tr("Open app · ")+app.value("name").toString()},{"action",QVariantMap{{"type","launch-app"},{"appId",id}}}});
        const auto *definition=m_live->registry()->find(id);if(definition)for(const auto &type:definition->handledActions)if(QStringList{"open-file","open-folder","open-url","open-disc"}.contains(type))choices.append(QVariantMap{{"name",type+" · "+app.value("name").toString()},{"needsTarget",true},{"action",QVariantMap{{"type",type},{"openWith",id}}}});
    }
    for(const auto &entry:m_live->m_collections){const auto collection=entry.toMap();choices.append(QVariantMap{{"name",tr("Open collection · ")+collection.value("name").toString()},{"mediaKind",collection.value("media","movies")},{"action",QVariantMap{{"type","open-collection"},{"collectionId",collection.value("id")}}}});}
    return choices;
}
QObject *StudioSession::proposalModel() const { return m_proposalModel; }
void StudioSession::configureWorker(HomeLayout &worker,bool assetPaths) const
{
    worker.m_draftOnly=true;
    if(!m_live)return;
    worker.m_defaultPath=m_live->m_defaultPath;
    worker.m_defaultTheme=m_live->m_defaultTheme;
    worker.m_collections=m_live->m_collections;
    worker.m_captureInputsReady=m_live->m_captureInputsReady;
    worker.m_assetPaths=m_live->m_assetPaths;
    for(auto it=m_stagedAssets.cbegin();it!=m_stagedAssets.cend();++it)worker.m_assetPaths[it.key()]=it.value();
    if(assetPaths)worker.m_userPath=m_live->m_userPath;
}
bool StudioSession::validNewAction(const QVariantMap &value)
{
    if(value.isEmpty())return true;
    QString error;const auto action=ComponentSchema::action(value,&error);if(action.isEmpty()){setError(error);return false;}
    const auto type=action.value("type").toString();
    if(type=="open-page"){bool exists=false;for(const auto &entry:m_model->studioPages())if(entry.toMap().value("id")==action.value("pageId"))exists=true;if(!exists){setError(tr("Choose an existing destination page."));return false;}}
    if(type=="root-page" && !m_model->rootPageVisible()){setError(tr("This page was removed. Choose an existing destination."));return false;}
    return true;
}
QVariantMap StudioSession::componentPrototype(const QString &type,const QVariantMap &configuration)
{
    if(!m_active || !m_live){setError(tr("Reopen Page Studio before adding components."));return {};}
    const auto options=m_live->studioOptions();QJsonArray nodes;
    if(options.value("componentPresets").toMap().contains(type))nodes.append(QJsonObject::fromVariantMap(options.value("componentPresets").toMap().value(type).toMap()));
    else if(type=="container")nodes.append(QJsonObject::fromVariantMap(options.value("groupPreset").toMap()));
    else if(type=="input-panel"){
        const auto preset=options.value("inputPanelPreset").toMap();nodes.append(QJsonObject::fromVariantMap(preset.value("root").toMap()));
        for(const auto &entry:preset.value("children").toList())nodes.append(QJsonObject::fromVariantMap(entry.toMap()));
    } else if(type=="clock" || type=="text"){
        HomeLayout factory(m_live->registry());configureWorker(factory);
        if(!factory.addWidget(type)){setError(factory.errorString());return {};}
        auto item=factory.m_items.last().toObject();
        const auto span=options.value("widgetSpans").toMap().value(type).toMap();
        item["placement"]=QJsonObject::fromVariantMap({{"x",0},{"y",0},{"width",span.value("width")},{"height",span.value("height")}});
        nodes.append(item);
    } else {setError(tr("Choose a registered component type."));return {};}
    if(nodes.isEmpty() || nodes.size()>33){setError(tr("The packaged component preset is invalid."));return {};}
    auto configured=nodes.first().toObject();
    for(auto it=configuration.cbegin();it!=configuration.cend();++it){
        if(!QStringList{"name","action","customLogo","presentation","apps","buttons"}.contains(it.key())){setError(tr("Unsupported component configuration."));return {};}
        if(it.key()=="apps" || it.key()=="buttons")continue;
        if(it.key()=="presentation"){
            if(it.value().metaType().id()!=QMetaType::QVariantMap){setError(tr("Choose valid presentation values."));return {};}
            auto style=configured["presentation"].toObject();const auto changes=it.value().toMap();for(auto change=changes.cbegin();change!=changes.cend();++change)style[change.key()]=QJsonValue::fromVariant(change.value());configured["presentation"]=style;
        }else if(it.key()=="action" && it.value().toMap().isEmpty())configured.remove("action");
        else configured[it.key()]=QJsonValue::fromVariant(it.value());
    }
    nodes[0]=configured;
    if(configuration.contains("buttons")){
        const auto buttons=configuration.value("buttons").toList();if(type!="carousel" || configuration.value("buttons").metaType().id()!=QMetaType::QVariantList || buttons.size()>16){setError(tr("Choose up to 16 carousel buttons."));return {};}
        int column=0;for(const auto &value:buttons){const auto changes=value.toMap();if(value.metaType().id()!=QMetaType::QVariantMap){setError(tr("Choose a named button."));return {};}
            auto child=QJsonObject::fromVariantMap(options.value("buttonChildPreset").toMap());for(auto it=changes.cbegin();it!=changes.cend();++it){if(!QStringList{"name","action"}.contains(it.key())){setError(tr("Unsupported carousel button property."));return {};}child[it.key()]=QJsonValue::fromVariant(it.value());}
            child["id"]="carousel-button-"+QString::number(column+1);child["parentId"]=configured["id"];auto p=child["placement"].toObject();p["x"]=column++;child["placement"]=p;nodes.append(child);
        }
    }
    if(configuration.contains("apps")){
        const auto apps=configuration.value("apps").toList();QSet<QString> selected;
        if(type!="app-bar" || configuration.value("apps").metaType().id()!=QMetaType::QVariantList || apps.size()>16){setError(tr("Choose up to 16 installed apps."));return {};}
        int column=0;for(const auto &value:apps){const auto id=value.toString();if(value.metaType().id()!=QMetaType::QString || selected.contains(id) || !m_live->registry()->find(id)){setError(tr("Choose distinct installed apps."));return {};}
            selected.insert(id);auto child=QJsonObject::fromVariantMap(options.value("appChildPreset").toMap());child["id"]="app-slot-"+QString::number(column+1);child["appId"]=id;child["parentId"]=configured["id"];child["action"]=QJsonObject{{"type","launch-app"},{"appId",id}};
            auto placement=child["placement"].toObject();placement["x"]=column++;child["placement"]=placement;nodes.append(child);
        }
    }
    QHash<QString,QString> remap;QSet<QString> allocated;
    for(int i=0;i<nodes.size();++i){
        if(!validNewAction(nodes[i].toObject()["action"].toObject().toVariantMap()))return {};
        QString error;const auto canonical=ComponentSchema::component(nodes[i].toObject().toVariantMap(),true,&error);
        if(canonical.isEmpty()){setError(error);return {};}
        auto item=QJsonObject::fromVariantMap(canonical);const auto oldId=item["id"].toString();
        if(remap.contains(oldId)){setError(tr("The preset contains duplicate identities."));return {};}
        auto identity=allocateId((type=="clock" || type=="text")?type:oldId);if(identity.isEmpty())return {};
        const auto base=identity;int suffix=2;while(allocated.contains(identity))identity=base+QString::number(suffix++);
        if(!ComponentSchema::safeId(identity)){setError(tr("The component identity exceeds its limit."));return {};}
        allocated.insert(identity);remap[oldId]=identity;item["id"]=identity;nodes[i]=item;
    }
    for(int i=0;i<nodes.size();++i){auto item=nodes[i].toObject();const auto parent=item.value("parentId").toString();if(!parent.isEmpty()){
        if(!remap.contains(parent)){setError(tr("The preset refers to an external parent."));return {};}
        item["parentId"]=remap[parent];nodes[i]=item;
    }}
    const auto root=nodes.first().toObject();
    if(!root.value("parentId").toString().isEmpty()){setError(tr("The preset root cannot have an external parent."));return {};}
    HomeLayout view(m_live->registry());configureWorker(view,true);view.m_items=nodes;
    return {{"type",type},{"rootId",root["id"].toString()},{"root",root.toVariantMap()},{"components",nodes.toVariantList()},{"viewItems",view.items()}};
}

bool StudioSession::previewCommand(const QString &operation, const QVariantMap &arguments, const QString &pageId)
{
    if (!m_active || !m_live) return false;
    HomeLayout temporary(m_live->registry()); configureWorker(temporary);
    if (!temporary.loadDocument(m_draft, false)) return false;
    StudioSession transaction;
    if (!transaction.bind(&temporary) || !transaction.execute(operation, arguments, pageId)) {
        setError(transaction.errorString()); return false;
    }
    m_proposed = transaction.draft();
    m_proposalBaseline = m_draft;
    if (!m_proposalModel) m_proposalModel = new HomeLayout(m_live->registry(),this);
    configureWorker(*m_proposalModel,true);
    m_proposalModel->loadDocument(m_proposed,false);
    emit m_proposalModel->layoutChanged(); emit changed();
    return true;
}

bool StudioSession::acceptPreview()
{
    if (m_proposed.isEmpty()) return false;
    if (m_draft != m_proposalBaseline) { setError(QStringLiteral("The draft changed. Preview the arrangement again.")); return false; }
    const auto candidate = m_proposed;
    if (!pushEdit(candidate)) return false;
    cancelPreview(); return true;
}

void StudioSession::cancelPreview()
{
    m_proposed.clear();
    m_proposalBaseline.clear();
    if (m_proposalModel) { m_proposalModel->deleteLater(); m_proposalModel = nullptr; }
    emit changed();
}

bool StudioSession::bind(HomeLayout *layout)
{
    if (!layout) return false;
    m_live = layout;
    m_profilePath = layout->m_userPath;
    QFile disk(m_profilePath);
    m_diskExisted = disk.exists();
    m_diskReadable = !m_diskExisted || (disk.size() <= kMaxDocumentBytes && disk.open(QIODevice::ReadOnly));
    m_diskBaseline = m_diskReadable && m_diskExisted ? disk.readAll() : QByteArray();
    m_model = new HomeLayout(layout->registry(), this);
    m_model->m_draftOnly = true;
    m_model->m_userPath=layout->m_userPath;
    m_model->m_defaultPath=layout->m_defaultPath;
    m_model->m_defaultTheme=layout->m_defaultTheme;
    m_model->m_collections = layout->m_collections;
    m_model->m_captureInputsReady = layout->m_captureInputsReady;
    m_model->m_assetPaths=layout->m_assetPaths;
    m_active = open(layout->committedDocument());
    connect(layout, &QObject::destroyed, this, &StudioSession::invalidate);
    return m_active;
}

void StudioSession::invalidate()
{
    m_active = false;
    m_live.clear();
    m_stagedAssets.clear();delete m_assetStage;m_assetStage=nullptr;
    m_diskBaseline.clear();
    m_profilePath.clear();
    cancelPreview();
    m_draft.clear();
    m_committed.clear();
    m_undo.clear();
    m_redo.clear();
    if (m_model) {
        m_model->m_items = {};
        m_model->m_assetPaths.clear();
        m_model->m_categories = {};
        m_model->m_pages = {};
        m_model->m_collections = {};
        m_model->m_composition.clear();
        m_model->m_homePageId.clear();
        m_model->m_rootPage={};m_model->m_studioAppearance.clear();
        m_model->m_userPath.clear();
        m_model->m_defaultPath.clear();
        m_model->syncPages();
        emit m_model->layoutChanged();
    }
    setError(QStringLiteral("The profile edit session has closed."));
    emit changed();
}

bool StudioSession::apply(const QString &focusId)
{
    if (!m_active || !m_live || m_live->m_userPath != m_profilePath) {
        setError(QStringLiteral("The profile changed. Reopen Page Studio."));
        return false;
    }
    if (isStaleAgainst(m_live->committedDocument())) {
        setError(QStringLiteral("The applied design changed. Reload or copy your draft before applying."));
        return false;
    }
    QFile disk(m_profilePath);
    if (!m_diskReadable || disk.exists() != m_diskExisted ||
        (m_diskExisted && (disk.size() > kMaxDocumentBytes || !disk.open(QIODevice::ReadOnly) || disk.readAll() != m_diskBaseline))) {
        setError(QStringLiteral("The saved design changed or cannot be read. Reopen Studio before applying; your draft is retained."));
        return false;
    }
    disk.close();
    QStringList created;
    const auto artDirectory=QDir(QFileInfo(m_profilePath).absolutePath()).filePath("artwork");const bool hadArtDirectory=QFileInfo::exists(artDirectory);
    if(QFileInfo(artDirectory).isSymLink()){setError(tr("The managed artwork directory must stay inside this profile."));return false;}
    const auto cleanup=[&]{for(const auto &path:created)QFile::remove(path);if(!hadArtDirectory)QDir().rmdir(artDirectory);};
    const auto serialized=QString::fromUtf8(m_draft);
    for(auto it=m_stagedAssets.cbegin();it!=m_stagedAssets.cend();++it){
        if(!serialized.contains(it.key()))continue;
        QFile source(it.value());if(!source.open(QIODevice::ReadOnly)){cleanup();setError(tr("Staged artwork is missing. Choose it again; the draft is retained."));return false;}
        const auto data=source.readAll(),target=QDir(QFileInfo(m_profilePath).absolutePath()).filePath(it.key()).toUtf8();
        const auto expected="artwork/studio-"+QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex())+(it.key().endsWith(".png")?".png":".jpg");
        if(data.size()>20*1024*1024 || expected!=it.key()){cleanup();setError(tr("Staged artwork changed. Import it again before applying."));return false;}
        const auto targetPath=QString::fromUtf8(target);
        if(QFile::exists(targetPath)){QFile existing(targetPath);if(!existing.open(QIODevice::ReadOnly) || existing.readAll()!=data){cleanup();setError(tr("Managed artwork conflicts with an existing file."));return false;}continue;}
        if(!QDir().mkpath(QFileInfo(targetPath).absolutePath())){cleanup();setError(tr("Could not create the managed artwork directory."));return false;}
        QSaveFile output(targetPath);if(!output.open(QIODevice::WriteOnly) || output.write(data)!=data.size() || !output.commit()){cleanup();setError(tr("Could not save artwork. Nothing applied."));return false;}
        created.append(targetPath);
    }
    if (!m_live->applyStudioDraft(m_draft, focusId)) {
        cleanup();
        setError(m_live->errorString());
        return false;
    }
    m_draft = m_live->committedDocument();
    m_diskBaseline = m_draft; m_diskExisted = true; m_diskReadable = true;
    markApplied();
    return true;
}

bool StudioSession::execute(const QString &operation, const QVariantMap &a, const QString &pageId)
{
    if (!m_active || !m_live) { setError(QStringLiteral("Reopen Page Studio before editing.")); return false; }
    if(operation=="studioAppearance") {
        auto document=QJsonDocument::fromJson(m_draft).object();
        const auto value=a.value("value").toMap();
        if(value.isEmpty())document.remove("studioAppearance");else document["studioAppearance"]=QJsonObject::fromVariantMap(value);
        return pushEdit(QJsonDocument(document).toJson());
    }
    if(QStringList{"renamePage","duplicatePage","reorderPage","removePage"}.contains(operation)) {
        auto document=QJsonDocument::fromJson(m_draft).object();auto pages=document["pages"].toArray();
        const QString id=a.value("id").toString();int index=-1;
        for(int i=0;i<pages.size();++i)if(pages[i].toObject()["id"]==id)index=i;
        auto rootMeta=document["rootPage"].toObject();
        if(rootMeta.isEmpty())rootMeta={{"name",m_model->rootPageName()},{"visible",true},{"position",0}};
        if((id.isEmpty() && !rootMeta.value("visible").toBool()) || (!id.isEmpty() && index<0)){setError(tr("This page no longer exists."));return false;}
        QStringList order;for(const auto &entry:m_model->studioPages())order.append(entry.toMap()["id"].toString());
        const auto storeOrder=[&] {
            QJsonArray sorted;
            for(const auto &key:order)if(!key.isEmpty())for(const auto &entry:pages)if(entry.toObject()["id"]==key)sorted.append(entry);
            pages=sorted;rootMeta["position"]=qMax(0,int(order.indexOf(QString())));
        };
        if(operation=="renamePage" || operation=="duplicatePage") {
            const auto name=a.value("name").toString().trimmed();
            if(name.isEmpty() || name.size()>80 || name.contains(QChar::Null)){setError(tr("Use a page name of 1–80 characters."));return false;}
            if(operation=="renamePage") {
                if(id.isEmpty())rootMeta["name"]=name;
                else{auto entry=pages[index].toObject();entry["name"]=name;pages[index]=entry;}
            } else {
                if(order.size()>=64){setError(tr("Up to 64 pages are supported per profile."));return false;}
                auto layout=id.isEmpty()?document:pages[index].toObject()["layout"].toObject();
                layout.remove("pages");layout.remove("homePageId");layout.remove("rootPage");layout.remove("studioAppearance");
                QHash<QString,QString> copiedIds;QSet<QString> allocated;
                auto composition=layout["composition"].toObject();
                const auto remapIdentity=[&](const QString &old)->QString {
                    if(copiedIds.contains(old))return copiedIds[old];
                    auto fresh=allocateId(old);if(fresh.isEmpty())return {};
                    const auto base=fresh;int suffix=2;
                    while(allocated.contains(fresh))fresh=base+QString::number(suffix++);
                    if(!ComponentSchema::safeId(fresh))return {};
                    copiedIds[old]=fresh;allocated.insert(fresh);return fresh;
                };
                for(const auto &key:QStringList{"categories","components"}) {
                    auto values=layout[key].toArray();
                    for(int i=0;i<values.size();++i){auto entry=values[i].toObject();const auto fresh=remapIdentity(entry["id"].toString());if(fresh.isEmpty())return false;entry["id"]=fresh;values[i]=entry;}
                    layout[key]=values;
                }
                auto components=layout["components"].toArray();
                for(int i=0;i<components.size();++i){auto item=components[i].toObject();for(const auto &key:QStringList{"categoryId","parentId"}){const auto reference=item.value(key).toString();if(copiedIds.contains(reference))item[key]=copiedIds[reference];}components[i]=item;}
                layout["components"]=components;
                auto tabs=composition.value("navigation").toArray();
                for(int i=0;i<tabs.size();++i){auto tab=tabs[i].toObject();const auto old=tab["id"].toString(),fresh=remapIdentity(old);if(fresh.isEmpty())return false;tab["id"]=fresh;
                    auto sections=tab.value("sections").toArray();for(int j=0;j<sections.size();++j)if(copiedIds.contains(sections[j].toString()))sections[j]=copiedIds[sections[j].toString()];
                    if(tab.contains("sections"))tab["sections"]=sections;tabs[i]=tab;}
                if(composition.contains("navigation"))composition["navigation"]=tabs;
                const auto initial=composition.value("initialTab").toString();if(copiedIds.contains(initial))composition["initialTab"]=copiedIds[initial];
                layout["composition"]=composition;
                // Only local identities/references change; page/app/media links
                // retain their original destinations until explicitly edited.
                auto copyId=allocateId("page");if(copyId.isEmpty())return false;
                const auto copyBase=copyId;int copySuffix=2;
                while(allocated.contains(copyId))copyId=copyBase+QString::number(copySuffix++);
                if(!ComponentSchema::safeId(copyId)){setError(tr("The duplicated page identity exceeds its limit."));return false;}
                pages.append(QJsonObject{{"id",copyId},{"name",name},{"layout",layout}});
                order.insert(order.indexOf(id)+1,copyId);storeOrder();
            }
        } else if(operation=="reorderPage") {
            const int from=order.indexOf(id),offset=a.value("offset").toInt(),to=from+offset;
            if((offset!=1 && offset!=-1) || to<0 || to>=order.size()){setError(tr("This page is already at the edge of the page list."));return false;}
            order.move(from,to);storeOrder();
        } else {
            if(document["homePageId"].toString()==id){setError(tr("Set another page as Home before deleting this page."));return false;}
            const bool remap=a.contains("replacementPageId");const QString replacement=a.value("replacementPageId").toString();
            bool validReplacement=replacement.isEmpty()?rootMeta.value("visible").toBool():false;
            for(const auto &entry:pages)if(entry.toObject()["id"]==replacement)validReplacement=true;
            if(remap && (!validReplacement || replacement==id)){setError(tr("Choose another existing page for incoming links."));return false;}
            if(id.isEmpty()) {
                document["components"]=QJsonArray{};document["categories"]=QJsonArray{};document["composition"]=QJsonObject{};rootMeta["visible"]=false;
            } else pages.removeAt(index);
            document["pages"]=pages;
            int links=0;bool invalidOverride=false;
            std::function<QJsonValue(QJsonValue)> repair=[&](QJsonValue value)->QJsonValue {
                if(value.isArray()){auto values=value.toArray();for(int i=0;i<values.size();++i)values[i]=repair(values[i]);return values;}
                if(!value.isObject())return value;
                auto object=value.toObject();
                if((!id.isEmpty() && object.value("type")=="open-page" && object.value("pageId")==id) || (id.isEmpty() && object.value("type")=="root-page")) {
                    ++links;
                    if(remap){object["type"]=replacement.isEmpty()?"root-page":"open-page";if(replacement.isEmpty())object.remove("pageId");else object["pageId"]=replacement;}
                }
                for(auto it=object.begin();it!=object.end();++it) {
                    if(!id.isEmpty() && QStringList{"moviePage","showsPage"}.contains(it.key()) && it.value()==id) {
                        ++links;if(remap){if(replacement.isEmpty())invalidOverride=true;else it.value()=replacement;}
                    } else it.value()=repair(it.value());
                }
                return object;
            };
            document=repair(document).toObject();
            if((links && !remap) || invalidOverride){setError(tr("%1 incoming links require a named replacement page before deletion.").arg(links));return false;}
            pages=document["pages"].toArray();order.removeAll(id);storeOrder();
        }
        const auto normalizeComponents=[](QJsonObject layout) {
            const auto key=layout.value("schemaVersion").toInt()==1?QStringLiteral("items"):QStringLiteral("components");
            auto components=layout.value(key).toArray();
            for(int i=0;i<components.size();++i)components[i]=QJsonObject::fromVariantMap(ComponentSchema::component(components[i].toObject().toVariantMap(),true));
            layout[key]=components;return layout;
        };
        document=normalizeComponents(document);
        for(int i=0;i<pages.size();++i){auto entry=pages[i].toObject();entry["layout"]=normalizeComponents(entry["layout"].toObject());pages[i]=entry;}
        document["pages"]=pages;document["rootPage"]=rootMeta;
        return pushEdit(QJsonDocument(document).toJson());
    }
    if(operation=="setHomePage") {
        auto document=QJsonDocument::fromJson(m_draft).object();
        const auto id=a.value("id").toString();bool found=id.isEmpty() && m_model->rootPageVisible();
        for(const auto &entry:document["pages"].toArray())if(entry.toObject()["id"]==id)found=true;
        if(!found){setError(QStringLiteral("Choose an existing page as Home."));return false;}
        document["homePageId"]=id;return pushEdit(QJsonDocument(document).toJson());
    }
    if(operation=="addResolved" || operation=="addPlaced") {
        if(a.contains("enableScrolling") && a.value("enableScrolling").metaType().id()!=QMetaType::Bool){setError(tr("Choose whether this page scrolls."));return false;}
        const auto proposal=additionProposal(a.value("type").toString(),a.value("placement").toMap(),
            a.value("strategy","cancel").toString(),pageId,a.value("categoryId").toString(),a.value("parentId").toString(),a.value("configuration").toMap(),a.value("enableScrolling",false).toBool());
        if(!proposal.value("valid").toBool()){setError(proposal.value("reason").toString());return false;}
        return pushEdit(QJsonDocument(QJsonObject::fromVariantMap(proposal.value("document").toMap())).toJson());
    }
    if (operation == "batch") {
        const auto commands = a.value("commands").toList();
        if (commands.isEmpty() || commands.size() > 32) { setError(QStringLiteral("Choose between 1 and 32 grouped edits.")); return false; }
        HomeLayout temporary(m_live->registry()); configureWorker(temporary);
        if (!temporary.loadDocument(m_draft, false)) { setError(temporary.errorString()); return false; }
        StudioSession transaction;
        if (!transaction.bind(&temporary)) { setError(transaction.errorString()); return false; }
        for (const auto &entry : commands) {
            const auto command = entry.toMap();
            const auto op = command.value("operation").toString();
            if (op == "batch" || !transaction.execute(op, command.value("arguments").toMap(), command.value("pageId", pageId).toString())) {
                setError(op == "batch" ? QStringLiteral("Nested batches are not supported.") : transaction.errorString());
                return false;
            }
        }
        return pushEdit(transaction.draft());
    }
    auto root = QJsonDocument::fromJson(m_draft).object();
    auto pages = root["pages"].toArray();
    int pageIndex = -1;
    QJsonObject document = root;
    if (!pageId.isEmpty()) {
        for (int i = 0; i < pages.size(); ++i) if (pages[i].toObject()["id"] == pageId) pageIndex = i;
        if (pageIndex < 0) { setError(QStringLiteral("That page no longer exists.")); return false; }
        document = pages[pageIndex].toObject()["layout"].toObject();
    }
    HomeLayout worker(m_live->registry());
    configureWorker(worker);
    if (!worker.loadDocument(QJsonDocument(document).toJson(), false)) { setError(worker.errorString()); return false; }
    const auto id = a.value("id").toString();
    bool ok = false;
    if(operation=="repairSource") {
        ok=worker.rebindSource(id,a.value("sourceId").toString());
    } else if(operation=="assignSection") {
        ok=worker.assignItemCategory(id,a.value("categoryId").toString());
    } else if(operation=="navigation") {
        auto composition=document["composition"].toObject();auto tabs=composition["navigation"].toArray();const auto kind=a.value("kind").toString();int index=-1;
        for(int i=0;i<tabs.size();++i)if(tabs[i].toObject()["id"]==id)index=i;
        if(kind=="add"){
            if(tabs.size()>=64){setError(tr("Use at most 64 navigation tabs."));return false;}
            const auto identity=allocateId("tab");if(identity.isEmpty())return false;tabs.append(QJsonObject{{"id",identity},{"name",a.value("name").toString()}});
        }else{
            if(index<0){setError(tr("Choose an existing navigation tab."));return false;}
            if(kind=="remove")tabs.removeAt(index);
            else if(kind=="move") {const int offset=a.value("offset").toInt(),target=index+offset;if((offset!=1 && offset!=-1) || target<0 || target>=tabs.size()){setError(tr("The tab is already at that edge."));return false;}const QJsonValue entry=tabs.at(index);tabs.removeAt(index);tabs.insert(target,entry);}
            else if(kind=="patch") {auto tab=tabs[index].toObject();const auto values=a.value("value").toMap();if(values.contains("action") && !validNewAction(values.value("action").toMap()))return false;for(auto it=values.cbegin();it!=values.cend();++it){if(!QStringList{"name","action","sections"}.contains(it.key())){setError(tr("Unsupported navigation property."));return false;}if(it.value().isNull() || (it.key()=="action" && it.value().toMap().isEmpty()))tab.remove(it.key());else tab[it.key()]=QJsonValue::fromVariant(it.value());}tabs[index]=tab;}
            else {setError(tr("Choose a supported navigation operation."));return false;}
        }
        composition["navigation"]=tabs;
        if(composition.contains("initialTab")){bool present=false;for(const auto &tab:tabs)if(tab.toObject()["id"]==composition["initialTab"])present=true;if(!present){if(tabs.isEmpty())composition.remove("initialTab");else composition["initialTab"]=tabs.first().toObject()["id"];}}
        document["composition"]=composition;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="pagePatch") {
        const auto scope=a.value("scope").toString();if(!QStringList{"theme","composition"}.contains(scope)){setError(tr("Choose a page property group."));return false;}
        const auto values=a.value("value").toMap();auto nested=document[scope].toObject();if(scope=="composition" && nested.isEmpty())nested={{"schemaVersion",1},{"enabled",false}};
        if(scope=="composition")for(const auto &key:QStringList{"moviePage","showsPage"})if(values.contains(key) && !values.value(key).isNull() && !validNewAction({{"type","open-page"},{"pageId",values.value(key)}}))return false;
        for(auto it=values.cbegin();it!=values.cend();++it){if(it.value().isNull())nested.remove(it.key());else nested[it.key()]=QJsonValue::fromVariant(it.value());}
        document[scope]=nested;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="scrolling") {
        if(a.value("enabled").metaType().id()!=QMetaType::Bool){setError(tr("Choose whether this page scrolls."));return false;}
        auto composition=document["composition"].toObject();
        if(!composition.value("enabled").toBool() || !composition.contains("grid") || ComponentSchema::normalizedGrid(composition["grid"].toObject().toVariantMap()).value("mode")!="screen"){
            setError(tr("This layout uses scrolling sections or automatic sizing. Preview a page grid before changing this option."));return false;
        }
        composition["verticalScrolling"]=a.value("enabled").toBool();document["composition"]=composition;
        if(!a.value("enabled").toBool() && !ComponentSchema::layoutPlacementsFit(document["components"].toArray(),composition.toVariantMap(),document["categories"].toArray())){
            setError(tr("Components extend below the first screen. Move or resize them before turning scrolling off."));return false;
        }
        ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="setApps") {
        auto entries=document["components"].toArray();int rootIndex=-1;QSet<QString> removed;
        for(int i=0;i<entries.size();++i)if(entries[i].toObject()["id"]==id && entries[i].toObject()["family"]=="app-bar")rootIndex=i;
        if(rootIndex<0 || ComponentSchema::positionLocked(entries,id)){setError(tr("Choose an unlocked app bar."));return false;}
        removed.insert(id);for(int depth=0;depth<4;++depth)for(const auto &entry:entries)if(removed.contains(entry.toObject()["parentId"].toString()))removed.insert(entry.toObject()["id"].toString());removed.remove(id);
        for(const auto &entry:entries)if(removed.contains(entry.toObject()["id"].toString()) && entry.toObject().value("locked").toBool()){setError(tr("Unlock the app bar's contents before replacing them."));return false;}
        const auto prototype=componentPrototype("app-bar",{{"apps",a.value("apps")}});if(prototype.isEmpty())return false;
        const auto newRoot=prototype.value("rootId").toString();QJsonArray result;
        for(const auto &entry:entries)if(!removed.contains(entry.toObject()["id"].toString()))result.append(entry);
        for(const auto &entry:prototype.value("components").toList()){auto child=QJsonObject::fromVariantMap(entry.toMap());if(child["id"]==newRoot)continue;child["parentId"]=id;result.append(child);}
        document["components"]=result;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="patch") {
        auto ids=a.value("ids").toList();if(ids.isEmpty() && !id.isEmpty())ids.append(id);
        const auto values=a.value("value").toMap();QSet<QString> targets;
        if(ids.isEmpty() || ids.size()>32 || values.isEmpty()){setError(tr("Choose 1–32 components and a property to edit."));return false;}
        for(const auto &value:ids){if(value.metaType().id()!=QMetaType::QString || targets.contains(value.toString())){setError(tr("Choose distinct component identities."));return false;}targets.insert(value.toString());}
        for(auto it=values.cbegin();it!=values.cend();++it)if(!QStringList{"name","opacity","showTitle","showType","showLogo","logoFill","symbol","accent","action","customLogo","presentation","accessibility","size"}.contains(it.key())){setError(tr("Unsupported inspector property."));return false;}
        if(values.contains("action") && !validNewAction(values.value("action").toMap()))return false;
        auto entries=document["components"].toArray();int found=0;
        for(int i=0;i<entries.size();++i){auto item=entries[i].toObject();if(!targets.contains(item["id"].toString()))continue;++found;
            for(auto it=values.cbegin();it!=values.cend();++it){
                const auto key=it.key();if(key=="presentation" || key=="accessibility"){
                    if(it.value().metaType().id()!=QMetaType::QVariantMap){setError(tr("Choose valid property values."));return false;}
                    auto nested=item[key].toObject();const auto changes=it.value().toMap();for(auto v=changes.cbegin();v!=changes.cend();++v){if(v.value().isNull())nested.remove(v.key());else nested[v.key()]=QJsonValue::fromVariant(v.value());}item[key]=nested;
                }else if(it.value().isNull() || (key=="action" && it.value().metaType().id()==QMetaType::QVariantMap && it.value().toMap().isEmpty()))item.remove(key);
                else item[key]=QJsonValue::fromVariant(it.value());
            }
            if(values.contains("action") && (item["componentType"]=="launcher" || item["componentType"]=="media-shelf")){
                item["kind"]="widget";item["widgetType"]="button";item["componentType"]="button";item.remove("appId");
            }
            QString error;const auto canonical=ComponentSchema::component(item.toVariantMap(),true,&error);if(canonical.isEmpty()){setError(error);return false;}entries[i]=QJsonObject::fromVariantMap(canonical);
        }
        if(found!=targets.size()){setError(tr("A selected component no longer exists."));return false;}
        document["components"]=entries;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="section") {
        const auto kind=a.value("kind").toString();
        if(kind=="add")ok=!worker.addCategory(a.value("name").toString()).isEmpty();
        else if(kind=="rename")ok=worker.renameCategory(id,a.value("name").toString());
        else if(kind=="remove")ok=worker.removeCategory(id);
        else if(kind=="move")ok=worker.moveCategory(id,a.value("offset").toInt());
        else if(kind=="presentation"){const auto style=a.value("value").toMap();if(style.contains("trailingAction") && !validNewAction(style.value("trailingAction").toMap()))return false;ok=worker.setSectionPresentation(id,style);}
    } else if(operation=="brand")ok=worker.setBrandVisible(a.value("value").toBool());
    else if(operation=="arrange") {
        const auto selection=a.value("ids").toList();const auto mode=a.value("mode").toString();
        if(selection.size()<2 || selection.size()>32 || !QStringList{"left","center-x","right","top","center-y","bottom","distribute-x","distribute-y"}.contains(mode)){setError(tr("Choose an alignment and between 2 and 32 components."));return false;}
        auto entries=document["components"].toArray();QList<int> indices;QSet<QString> ids;QString parent,category;int low=16,high=0,total=0;
        const bool horizontal=QStringList{"left","center-x","right","distribute-x"}.contains(mode);const QString axis=horizontal?"x":"y",span=horizontal?"width":"height";
        for(const auto &value:selection){const auto identity=value.toString();if(value.metaType().id()!=QMetaType::QString || ids.contains(identity)){setError(tr("Choose distinct component identities."));return false;}ids.insert(identity);}
        for(int i=0;i<entries.size();++i){const auto item=entries[i].toObject();if(!ids.contains(item["id"].toString()))continue;
            if(ComponentSchema::positionLocked(entries,item["id"].toString()) || !item.contains("placement")){setError(tr("Unlock the selected components and choose a grid layout."));return false;}
            if(indices.isEmpty()){parent=item.value("parentId").toString();category=item.value("categoryId").toString();}
            if(parent!=item.value("parentId").toString() || category!=item.value("categoryId").toString()){setError(tr("Arrange components in the same group or section."));return false;}
            indices.append(i);const auto p=item["placement"].toObject();low=qMin(low,p[axis].toInt());high=qMax(high,p[axis].toInt()+p[span].toInt());total+=p[span].toInt();
        }
        if(indices.size()!=ids.size()){setError(tr("A selected component no longer exists."));return false;}
        const bool distribute=mode.startsWith("distribute");int gap=0;
        if(distribute){const int space=high-low-total;if(space<0 || space%(indices.size()-1)){setError(tr("Equal spacing requires whole cells. Adjust the outer positions first."));return false;}gap=space/(indices.size()-1);
            std::stable_sort(indices.begin(),indices.end(),[&](int x,int y){return entries[x].toObject()["placement"].toObject()[axis].toInt()<entries[y].toObject()["placement"].toObject()[axis].toInt();});}
        int cursor=low;
        for(int index:indices){auto item=entries[index].toObject();auto p=item["placement"].toObject();const int size=p[span].toInt();int position=low;
            if(distribute){position=cursor;cursor+=size+gap;}
            else if(mode.startsWith("center")){if((low+high-size)%2){setError(tr("Center alignment falls between grid cells. Adjust the sizes or outer positions first."));return false;}position=(low+high-size)/2;}
            else if(mode=="right" || mode=="bottom")position=high-size;
            p[axis]=position;item["placement"]=p;entries[index]=item;
        }
        document["components"]=entries;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="group") {
        const auto selection=a.value("ids").toList();QSet<QString> ids;
        if(selection.size()<2 || selection.size()>32){setError(tr("Select between 2 and 32 components to group."));return false;}
        for(const auto &value:selection){if(value.metaType().id()!=QMetaType::QString || ids.contains(value.toString())){setError(tr("Choose distinct component identities."));return false;}ids.insert(value.toString());}
        auto entries=document["components"].toArray();QString parent,category;bool first=true;int left=16,top=10,right=0,bottom=0,found=0;
        for(const auto &value:entries){const auto item=value.toObject();if(!ids.contains(item["id"].toString()))continue;
            if(ComponentSchema::positionLocked(entries,item["id"].toString()) || !item.contains("placement")){setError(tr("Unlock the selected components and use a page grid before grouping."));return false;}
            if(first){parent=item.value("parentId").toString();category=item.value("categoryId").toString();first=false;}
            if(item.value("parentId").toString()!=parent || item.value("categoryId").toString()!=category){setError(tr("Select components in the same group or section."));return false;}
            const auto p=item["placement"].toObject();left=qMin(left,p["x"].toInt());top=qMin(top,p["y"].toInt());right=qMax(right,p["x"].toInt()+p["width"].toInt());bottom=qMax(bottom,p["y"].toInt()+p["height"].toInt());++found;
        }
        if(found!=ids.size()){setError(tr("A selected component no longer exists."));return false;}
        const QVariantMap bounds{{"x",left},{"y",top},{"width",right-left},{"height",bottom-top}};
        for(const auto &value:entries){const auto item=value.toObject();if(ids.contains(item["id"].toString()) || item.value("parentId").toString()!=parent || item.value("categoryId").toString()!=category)continue;
            if(ComponentSchema::placementsOverlap(bounds,item["placement"].toObject().toVariantMap())){setError(tr("The group bounds include another component. Select it too or move it first."));return false;}}
        const auto prototype=componentPrototype("container");if(prototype.isEmpty())return false;
        auto group=QJsonObject::fromVariantMap(prototype.value("root").toMap());const auto identity=group["id"].toString();
        group["placement"]=QJsonObject::fromVariantMap(bounds);if(!parent.isEmpty())group["parentId"]=parent;if(!category.isEmpty())group["categoryId"]=category;
        auto presentation=group["presentation"].toObject();auto layout=presentation["layout"].toObject();
        layout["columns"]=right-left;layout["rows"]=bottom-top;layout["padding"]=0;
        layout["gap"]=ComponentSchema::componentGrid(entries,document["composition"].toObject().toVariantMap(),document["categories"].toArray(),parent,category).value("gap").toDouble();
        presentation["layout"]=layout;presentation["showHeading"]=false;group["presentation"]=presentation;
        QJsonArray result;bool inserted=false;
        for(const auto &value:entries){auto item=value.toObject();if(ids.contains(item["id"].toString())){
            if(!inserted){result.append(group);inserted=true;}
            auto placement=item["placement"].toObject();placement["x"]=placement["x"].toInt()-left;placement["y"]=placement["y"].toInt()-top;
            item["placement"]=placement;item["parentId"]=identity;item.remove("categoryId");
        }result.append(item);}
        document["components"]=result;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="ungroup") {
        auto entries=document["components"].toArray();QJsonObject group;
        for(const auto &value:entries)if(value.toObject()["id"]==id)group=value.toObject();
        if(group["componentType"]!="container" || ComponentSchema::positionLocked(entries,id)){setError(tr("Choose an unlocked group to ungroup."));return false;}
        const auto parent=group.value("parentId").toString(),category=group.value("categoryId").toString();
        const auto outer=group["placement"].toObject(),style=group["presentation"].toObject(),inner=style["layout"].toObject();
        const auto parentGrid=ComponentSchema::componentGrid(entries,document["composition"].toObject().toVariantMap(),document["categories"].toArray(),parent,category);
        if(inner["columns"].toInt()!=outer["width"].toInt() || inner["rows"].toInt()!=outer["height"].toInt() || inner["padding"].toDouble()!=0 || inner["gap"].toDouble()!=parentGrid.value("gap").toDouble() || style.value("showHeading").toBool()){
            setError(tr("This group's internal grid differs from its parent. Move its children out explicitly to preserve their sizes."));return false;
        }
        QJsonArray result;
        for(const auto &value:entries){auto item=value.toObject();if(item["id"]==id)continue;
            if(item.value("parentId").toString()==id){
                if(ComponentSchema::positionLocked(entries,item["id"].toString())){setError(tr("Unlock the group's contents before ungrouping."));return false;}
                auto placement=item["placement"].toObject();placement["x"]=placement["x"].toInt()+outer["x"].toInt();placement["y"]=placement["y"].toInt()+outer["y"].toInt();item["placement"]=placement;
                if(parent.isEmpty())item.remove("parentId");else item["parentId"]=parent;
                if(category.isEmpty())item.remove("categoryId");else item["categoryId"]=category;
            }result.append(item);
        }
        document["components"]=result;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if(operation=="childLayout") {
        const auto layout=a.value("value").toMap();if(!ComponentSchema::validChildLayout(layout)){setError(tr("Choose a bounded group layout."));return false;}
        auto entries=document["components"].toArray();bool found=false;
        if(ComponentSchema::positionLocked(entries,id)){setError(tr("Unlock this group before arranging it."));return false;}
        for(int i=0;i<entries.size();++i){auto item=entries[i].toObject();if(item["id"]==id && item["componentType"]=="container"){
            auto style=item["presentation"].toObject();style["layout"]=QJsonObject::fromVariantMap(layout);item["presentation"]=style;entries[i]=item;found=true;
        }}
        if(!found){setError(tr("Choose an existing group."));return false;}
        if(layout.value("mode")!="grid"){
            int cursor=0;const bool row=layout.value("mode")=="row";
            for(int i=0;i<entries.size();++i){auto item=entries[i].toObject();if(item.value("parentId").toString()!=id)continue;
                if(ComponentSchema::positionLocked(entries,item["id"].toString())){setError(tr("Unlock the group's contents before arranging them."));return false;}
                auto p=item["placement"].toObject();p["x"]=row?cursor:0;p["y"]=row?0:cursor;cursor+=p[row?"width":"height"].toInt();item["placement"]=p;entries[i]=item;
            }
        }
        document["components"]=entries;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if (operation == "place") {
        // Validate the complete result once: swaps/groups must not fail on an intermediate collision.
        auto entries = document["components"].toArray();
        const auto changes = a.value("placements").toList();
        QSet<QString> touched;
        if (changes.isEmpty() || changes.size() > 64) { setError(QStringLiteral("Choose between 1 and 64 placements.")); return false; }
        for (const auto &value : changes) {
            const auto change = value.toMap();
            const auto target = change.value("id").toString();
            bool found = false;
            if (touched.contains(target)) { setError(QStringLiteral("Duplicate placement identity.")); return false; }
            for (int i = 0; i < entries.size(); ++i) {
                auto entry = entries[i].toObject();
                if (entry["id"] != target) continue;
                if(change.contains("parentId") && change.value("parentId").metaType().id()!=QMetaType::QString){setError(tr("Choose a named destination group."));return false;}
                const auto parent=change.value("parentId",entry.value("parentId").toString()).toString();
                if(!parent.isEmpty() && ComponentSchema::positionLocked(worker.m_items,parent)){setError(tr("Choose an unlocked destination group."));return false;}
                if(change.contains("categoryId") && change.value("categoryId").metaType().id()!=QMetaType::QString){setError(tr("Choose a named destination section."));return false;}
                if(change.contains("categoryId") && entry.value("categoryId").toString()!=change.value("categoryId").toString()) {
                    const auto destination=change.value("categoryId").toString();bool eligible=destination.isEmpty();
                    for(const auto &value:worker.m_categories){const auto category=value.toObject();if(category["id"]==destination)eligible=category["presentation"].toObject().value("variant")!="library";}
                    if((!destination.isEmpty() && worker.grid().value("mode")!="sections") || !eligible){setError(tr("Choose an existing component section in a scrolling layout."));return false;}
                }
                if(ComponentSchema::positionLocked(worker.m_items,target) && (entry["placement"].toObject()!=QJsonObject::fromVariantMap(change.value("placement").toMap()) || parent!=entry.value("parentId").toString() || (change.contains("categoryId") && entry.value("categoryId").toString()!=change.value("categoryId").toString()))) {setError(tr("Unlock the component and its containing groups before moving or resizing it."));return false;}
                entry["placement"] = QJsonObject::fromVariantMap(change.value("placement").toMap());
                if(change.contains("categoryId")){const auto category=change.value("categoryId").toString();if(category.isEmpty())entry.remove("categoryId");else entry["categoryId"]=category;}
                if(parent.isEmpty())entry.remove("parentId");else entry["parentId"]=parent;
                entries[i] = entry; found = true;
            }
            if (!found) { setError(QStringLiteral("The component no longer exists.")); return false; }
            touched.insert(target);
        }
        document["components"] = entries;
        ok = worker.loadDocument(QJsonDocument(document).toJson(), false);
    } else if(operation=="orderRelative") {
        const auto relative=a.value("relativeId").toString();
        if(a.value("before").metaType().id()!=QMetaType::Bool || relative==id){setError(tr("Choose another component and an ordering direction."));return false;}
        auto entries=document["components"].toArray();int from=-1,to=-1;
        for(int i=0;i<entries.size();++i){if(entries[i].toObject()["id"]==id)from=i;if(entries[i].toObject()["id"]==relative)to=i;}
        if(from<0 || to<0 || entries[from].toObject().value("categoryId")!=entries[to].toObject().value("categoryId") || entries[from].toObject().value("parentId")!=entries[to].toObject().value("parentId")){setError(tr("Choose a component in the same section or group."));return false;}
        const QJsonValue moving=entries.at(from);
        for(int i=qMin(from,to);i<=qMax(from,to);++i)if(entries[i].toObject().value("locked").toBool()){setError(tr("Unlock the affected components before changing their order."));return false;}
        entries.removeAt(from);if(from<to)--to;
        entries.insert(to+(a.value("before").toBool()?0:1),moving);document["components"]=entries;
        ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if (operation == "convertGrid") {
        for(const auto &entry:worker.m_items)if(entry.toObject()["componentType"]=="container"){
            setError(tr("This page already contains groups. Edit its grid without converting its components."));return false;
        }
        const auto grid = ComponentSchema::normalizedGrid(a.value("value").toMap());
        if (!ComponentSchema::validGrid(grid)) {
            setError(QStringLiteral("The chosen grid cannot contain this page. Choose a larger grid or cancel.")); return false;
        }
        auto composition = worker.m_composition;
        composition["schemaVersion"] = 1; composition["enabled"] = true;
        composition["grid"] = grid;
        if(grid.value("mode")=="sections") {
            composition["columns"]=1;composition["fit"]=false;
            composition["sectionHeight"]=a.value("sectionHeight",m_live->studioOptions().value("sectionHeight"));
            for(int i=0;i<worker.m_categories.size();++i) {
                auto category=worker.m_categories[i].toObject();auto style=category["presentation"].toObject();
                if(style.value("variant")!="library"){style["height"]=QJsonValue::fromVariant(composition["sectionHeight"]);category["presentation"]=style;worker.m_categories[i]=category;}
            }
        } else {
            for(const auto &entry:worker.m_categories) {
                const auto style=entry.toObject()["presentation"].toObject();
                if(style["variant"]=="library" || style.contains("trailingAction")) {
                    setError(QStringLiteral("Choose scrolling sections to preserve this page's library or section actions."));return false;
                }
            }
        }
        worker.m_composition = composition;
        QHash<QString,int> nextCell;
        for (int i = 0; i < worker.m_items.size(); ++i) {
            auto item = worker.m_items[i].toObject();
            const auto category=grid.value("mode")=="sections"?item["categoryId"].toString():QString();
            const auto local=ComponentSchema::sectionGrid(composition,worker.m_categories,category);
            const int columns=local["columns"].toInt(),rows=local["rows"].toInt(),cell=nextCell[category]++;
            if(cell>=columns*rows){setError(QStringLiteral("A section cannot contain its components in the chosen grid. Nothing changed."));return false;}
            item["placement"] = QJsonObject{{"x", cell%columns}, {"y", cell/columns}, {"width", 1}, {"height", 1}};
            worker.m_items[i] = item;
        }
        ok = worker.save();
    } else if(operation=="state") {
        const auto key=a.value("key").toString();
        if(!QStringList{"hidden","locked"}.contains(key) || a.value("value").metaType().id()!=QMetaType::Bool){setError(tr("Choose a valid visibility or lock state."));return false;}
        auto entries=document["components"].toArray();bool found=false;
        for(int i=0;i<entries.size();++i)if(entries[i].toObject()["id"]==id){auto item=entries[i].toObject();item[key]=QJsonValue::fromVariant(a.value("value"));entries[i]=item;found=true;}
        if(!found){setError(tr("Choose an existing component."));return false;}
        document["components"]=entries;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    } else if (operation == "rename") ok = worker.setItemName(id, a.value("name").toString());
    else if (operation == "remove") {
        auto entries=document["components"].toArray();QSet<QString> removed{id};bool found=false;
        for(const auto &entry:entries)if(entry.toObject()["id"]==id)found=true;
        if(!found || ComponentSchema::positionLocked(entries,id)){setError(tr("Choose an unlocked component to remove."));return false;}
        for(int depth=0;depth<4;++depth)for(const auto &entry:entries)if(removed.contains(entry.toObject()["parentId"].toString()))removed.insert(entry.toObject()["id"].toString());
        for(const auto &entry:entries)if(removed.contains(entry.toObject()["id"].toString()) && entry.toObject().value("locked").toBool()){setError(tr("Unlock the group's contents before removing it."));return false;}
        QJsonArray result;for(const auto &entry:entries)if(!removed.contains(entry.toObject()["id"].toString()))result.append(entry);
        document["components"]=result;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    }
    else if (operation == "duplicate") {
        auto entries=document["components"].toArray();QJsonObject copy;
        for(const auto &entry:entries)if(entry.toObject()["id"]==id)copy=entry.toObject();
        if(copy.isEmpty()){setError(tr("Choose an existing component to duplicate."));return false;}
        const auto identity=allocateId(id+"-copy");if(identity.isEmpty())return false;copy["id"]=identity;
        if(worker.m_composition.contains("grid")) {
            auto shape=copy["placement"].toObject().toVariantMap();
            if (shape.isEmpty()) shape = {{"x",0},{"y",0},{"width",1},{"height",1}};
            const auto category=copy.value("categoryId").toString(),parent=copy.value("parentId").toString();const bool sections=worker.grid().value("mode")=="sections";
            if(!parent.isEmpty() && ComponentSchema::positionLocked(entries,parent)){setError(tr("Unlock the containing group before adding a duplicate."));return false;}
            const auto grid=ComponentSchema::componentGrid(entries,worker.m_composition,worker.m_categories,parent,category);bool free=false;
            for (int row=0; row<grid["rows"].toInt() && !free; ++row) for (int col=0; col<grid["columns"].toInt() && !free; ++col) {
                shape["x"]=col; shape["y"]=row;
                if(!ComponentSchema::validPlacement(shape,grid["columns"].toInt(),grid["rows"].toInt()))continue;
                free=true;for(const auto &entry:entries){const auto item=entry.toObject();if(item.value("parentId").toString()==parent && (!sections || !parent.isEmpty() || item.value("categoryId").toString()==category) && item.contains("placement") && ComponentSchema::placementsOverlap(item["placement"].toObject().toVariantMap(),shape)){free=false;break;}}
            }
            if(!free){setError(tr("There is no free region for the duplicate. Nothing changed."));return false;}
            copy["placement"]=QJsonObject::fromVariantMap(shape);
        }
        QHash<QString,QString> remap{{id,identity}};QSet<QString> allocated{identity};
        for(int depth=0;depth<4;++depth)for(const auto &entry:entries){const auto item=entry.toObject();const auto child=item["id"].toString();if(remap.contains(child) || !remap.contains(item["parentId"].toString()))continue;
            auto fresh=allocateId(child+"-copy");if(fresh.isEmpty())return false;const auto base=fresh;int suffix=2;while(allocated.contains(fresh))fresh=base+QString::number(suffix++);
            if(!ComponentSchema::safeId(fresh)){setError(tr("The copied identity exceeds its limit."));return false;}allocated.insert(fresh);remap[child]=fresh;
        }
        QJsonArray copies;copies.append(copy);
        for(const auto &entry:entries){auto item=entry.toObject();const auto child=item["id"].toString();if(child==id || !remap.contains(child))continue;item["id"]=remap[child];item["parentId"]=remap[item["parentId"].toString()];copies.append(item);}
        for(const auto &entry:copies)entries.append(entry);
        document["components"]=entries;ok=worker.loadDocument(QJsonDocument(document).toJson(),false);
    }
    else if (operation == "reorder") ok = worker.moveItem(id, a.value("offset").toInt());
    else if (operation == "presentation") ok = worker.setItemPresentation(id, a.value("value").toMap());
    else if (operation == "appearance") ok = worker.setAppearance(a.value("value").toMap());
    else if (operation == "composition") ok = worker.setComposition(a.value("value").toMap());
    else if (operation == "grid") ok = worker.setGrid(a.value("value").toMap());
    else if (operation == "action") ok = worker.setItemAction(id, a.value("value").toMap());
    else if (operation == "addWidget" || operation == "addPlaced") {
        ok = worker.addWidget(a.value("type").toString());
        if (ok && operation == "addPlaced") {
            const auto newId = worker.m_items.last().toObject()["id"].toString();
            ok = worker.setItemPlacement(newId, a.value("placement").toMap());
        }
    }
    else if (operation == "addButton") ok = worker.addButton();
    else if (operation == "newPage" && pageId.isEmpty()) ok = !worker.createPage(a.value("name").toString(), a.value("preset", "blank").toString()).isEmpty();
    else if (operation == "renamePage" && pageId.isEmpty()) ok = worker.renamePage(id, a.value("name").toString());
    else if (operation == "removePage" && pageId.isEmpty()) ok = worker.removePage(id);
    else { setError(QStringLiteral("Unknown or unavailable edit command.")); return false; }
    if (!ok) { setError(worker.errorString().isEmpty() ? QStringLiteral("The edit was refused; nothing changed.") : worker.errorString()); return false; }
    auto result = QJsonDocument::fromJson(worker.documentData()).object();
    if(operation=="section" && a.value("kind")=="add"){
        auto sections=result["categories"].toArray();auto entry=sections.last().toObject();const auto identity=allocateId("section");if(identity.isEmpty())return false;entry["id"]=identity;sections[sections.size()-1]=entry;result["categories"]=sections;
    }
    if (operation == "newPage") {
        auto entries = result["pages"].toArray();
        auto entry = entries.last().toObject();
        entry["id"] = allocateId("page");
        if(a.value("preset","blank")=="blank") {
            auto layout=entry["layout"].toObject();auto composition=layout["composition"].toObject();
            composition["grid"]=QJsonObject::fromVariantMap(m_live->studioOptions().value("defaultGrid").toMap());
            layout["composition"]=composition;entry["layout"]=layout;
        }
        entries[entries.size() - 1] = entry;
        result["pages"] = entries;
    } else if (operation == "addButton" || operation == "addWidget" || operation == "addPlaced") {
        auto entries = result["components"].toArray();
        auto entry = entries.last().toObject();
        const auto base = operation == "addButton" ? QStringLiteral("button") : operation == "duplicate" ? id + QStringLiteral("-copy") : a.value("type").toString();
        const auto identity = allocateId(base);
        if (identity.isEmpty()) return false;
        entry["id"] = identity;
        entries[entries.size() - 1] = entry;
        result["components"] = entries;
    }
    if (pageIndex >= 0) {
        result.remove("pages"); // A page document cannot recursively contain the interface page collection.
        result.remove("homePageId");
        result.remove("rootPage");
        result.remove("studioAppearance");
        auto entry = pages[pageIndex].toObject(); entry["layout"] = result; pages[pageIndex] = entry; root["pages"] = pages;
    } else root = result;
    return pushEdit(QJsonDocument(root).toJson());
}

bool StudioSession::open(const QByteArray &committedDocument)
{
    if (!validDraft(committedDocument)) return false;
    m_committed = committedDocument;
    m_draft = committedDocument;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
    return true;
}

bool StudioSession::pushEdit(const QByteArray &candidateDraft)
{
    if (m_committed.isEmpty()) {
        setError(QStringLiteral("Open a page before editing."));
        return false;
    }
    if (candidateDraft == m_draft) return true;
    if (!validDraft(candidateDraft)) return false;
    qsizetype retained = m_draft.size();
    for (const auto &entry : m_undo) retained += entry.size();
    if (m_undo.size() >= kMaxUndoSteps || retained > kMaxHistoryBytes) {
        setError(QStringLiteral("History budget reached. Continue with a new history baseline or cancel this edit."));
        return false;
    }
    m_undo.push(m_draft);
    m_draft = candidateDraft;
    m_redo.clear();
    setError({});
    emit changed();
    return true;
}

bool StudioSession::undo()
{
    if (m_undo.isEmpty()) {
        setError(QStringLiteral("Nothing to undo."));
        return false;
    }
    m_redo.push(m_draft);
    m_draft = m_undo.pop();
    setError({});
    emit changed();
    return true;
}

bool StudioSession::redo()
{
    if (m_redo.isEmpty()) {
        setError(QStringLiteral("Nothing to redo."));
        return false;
    }
    m_undo.push(m_draft);
    m_draft = m_redo.pop();
    setError({});
    emit changed();
    return true;
}

void StudioSession::revert()
{
    m_draft = m_committed;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

void StudioSession::markApplied()
{
    m_committed = m_draft;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

bool StudioSession::isDirty() const
{
    return !m_committed.isEmpty() && m_draft != m_committed;
}

QString StudioSession::allocateId(const QString &baseId)
{
    // Deterministic "-copy"/"-2" suffix walk, mirroring duplicateItem so both
    // native and scripted paths converge on the same bytes from the same ids.
    QString base = baseId.trimmed();
    if (!ComponentSchema::safeId(base)) {
        setError(QStringLiteral("That name cannot be used as an identity — use letters, digits, dot, dash or underscore."));
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(m_draft);
    QSet<QString> taken;
    if (doc.isObject()) {
        const auto collect = [&taken](const QJsonObject &object) {
            for (const auto &key : {"components", "items", "pages", "categories"})
                for (const auto &entry : object.value(key).toArray()) taken.insert(entry.toObject().value("id").toString());
        };
        collect(doc.object());
        for (const auto &page : doc.object().value("pages").toArray()) collect(page.toObject().value("layout").toObject());
    }
    if (!taken.contains(base)) return base;
    const QString copyBase = base + QStringLiteral("-copy");
    if (!ComponentSchema::safeId(copyBase)) { setError(QStringLiteral("The generated identity exceeds its length limit. Rename the component before duplicating.")); return {}; }
    if (!taken.contains(copyBase)) return copyBase;
    int suffix = 2;
    while (taken.contains(copyBase + QString::number(suffix))) ++suffix;
    return copyBase + QString::number(suffix);
}

namespace {
QJsonValue canonicalValue(const QJsonValue &value)
{
    if (value.isObject()) {
        QJsonObject sorted;
        QStringList keys;
        const auto object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            keys.append(it.key());
        std::sort(keys.begin(), keys.end());
        for (const QString &key : keys)
            sorted.insert(key, canonicalValue(object.value(key)));
        return sorted;
    }
    if (value.isArray()) {
        QJsonArray out;
        for (const QJsonValue &entry : value.toArray())
            out.append(canonicalValue(entry));
        return out;
    }
    return value;
}
}

bool StudioSession::canonicalize(const QByteArray &first, const QByteArray &second,
                                 QByteArray &firstOut, QByteArray &secondOut)
{
    const QJsonDocument firstDoc = QJsonDocument::fromJson(first);
    const QJsonDocument secondDoc = QJsonDocument::fromJson(second);
    if (firstDoc.isNull() || secondDoc.isNull()) {
        setError(QStringLiteral("Cannot compare drafts — one is not valid JSON."));
        return false;
    }
    const auto emitDoc = [](const QJsonDocument &doc) {
        if (doc.isObject())
            return QJsonDocument(canonicalValue(doc.object()).toObject()).toJson(QJsonDocument::Compact);
        if (doc.isArray())
            return QJsonDocument(canonicalValue(doc.array()).toArray()).toJson(QJsonDocument::Compact);
        return doc.toJson(QJsonDocument::Compact);
    };
    firstOut = emitDoc(firstDoc);
    secondOut = emitDoc(secondDoc);
    return true;
}

bool StudioSession::isStaleAgainst(const QByteArray &currentApplied) const
{
    if (!isDirty()) return false;
    if (currentApplied.isEmpty()) return true; // applied doc vanished: conflict
    QByteArray mine, theirs;
    // Compare canonically so formatting-only rewrites are not conflicts.
    StudioSession *self = const_cast<StudioSession *>(this);
    if (!self->canonicalize(m_committed, currentApplied, mine, theirs)) return true;
    return mine != theirs;
}

bool StudioSession::validDraft(const QByteArray &data)
{
    if (data.isEmpty()) {
        setError(QStringLiteral("The draft document is empty — nothing was written."));
        return false;
    }
    if (data.size() > kMaxDocumentBytes) {
        setError(QStringLiteral("The draft exceeds 4 MiB and was refused."));
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        setError(QStringLiteral("The draft is not a valid layout document."));
        return false;
    }
    const QJsonObject object = doc.object();
    const int version = object.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version != 1 && version != 2) {
        setError(QStringLiteral("Unknown layout version — draft refused."));
        return false;
    }
    if (object.contains(version == 1 ? QStringLiteral("components") : QStringLiteral("items"))) {
        setError(QStringLiteral("The draft mixes incompatible layout versions."));
        return false;
    }
    const QJsonArray components = object.value(version == 1 ? QStringLiteral("items") : QStringLiteral("components")).toArray();
    if (components.size() > kMaxComponents) {
        setError(QStringLiteral("The draft exceeds 1024 components and was refused."));
        return false;
    }
    QString error;
    if (!HomeLayout::validateStudioDocument(data, &error)) {
        setError(error);
        return false;
    }
    return true;
}

void StudioSession::setError(const QString &message)
{
    if (m_error == message) return;
    m_error = message;
    emit changed();
}

QVariantMap StudioSession::placementProposal(const QString &id, const QVariantMap &placement,
                                            const QString &strategy, const QString &pageId, const QVariantMap &options)
{
    const auto fail = [](const QString &reason) { return QVariantMap{{"valid", false}, {"reason", reason}}; };
    if (!m_active) return fail(QStringLiteral("The edit session has closed."));
    if(!QStringList{"cancel","shift","swap"}.contains(strategy))return fail(tr("Choose Shift, Swap or Cancel."));
    for(auto it=options.cbegin();it!=options.cend();++it)if((it.key()!="categoryId" && it.key()!="parentId") || it.value().metaType().id()!=QMetaType::QString)return fail(tr("Choose a valid destination section or group."));
    auto doc = QJsonDocument::fromJson(m_draft).object();
    if (!pageId.isEmpty()) {
        bool found = false;
        for (const auto &entry : doc["pages"].toArray()) if (entry.toObject()["id"] == pageId) {
            doc = entry.toObject()["layout"].toObject(); found = true; break;
        }
        if (!found) return fail(QStringLiteral("The page no longer exists."));
    }
    auto entries = doc["components"].toArray();
    QString category,parent;bool selectedFound=false;
    for(const auto &entry:entries)if(entry.toObject()["id"]==id){if(ComponentSchema::positionLocked(entries,id))return fail(tr("Unlock the component and its containing groups before moving or resizing it."));category=entry.toObject()["categoryId"].toString();parent=entry.toObject()["parentId"].toString();selectedFound=true;}
    if(!selectedFound)return fail(QStringLiteral("The component no longer exists."));
    const auto composition=doc["composition"].toObject().toVariantMap();
    const bool sections=composition.value("grid").toMap().value("mode")=="sections";
    const auto sourceCategory=category;const auto sourceParent=parent;
    if(options.contains("parentId"))parent=options.value("parentId").toString();
    if(!parent.isEmpty()) {
        bool found=false;QString ancestor=parent;
        for(const auto &entry:entries)if(entry.toObject()["id"]==parent)found=entry.toObject()["componentType"]=="container";
        if(!found || ComponentSchema::positionLocked(entries,parent))return fail(tr("Choose an unlocked destination group."));
        for(int depth=0;!ancestor.isEmpty() && depth<6;++depth) {
            if(ancestor==id)return fail(tr("A group cannot contain itself or its ancestors."));
            QString next;for(const auto &entry:entries)if(entry.toObject()["id"]==ancestor)next=entry.toObject()["parentId"].toString();
            ancestor=next;
        }
        category.clear();
    } else if(!sourceParent.isEmpty())category=ComponentSchema::effectiveCategory(entries,id);
    if(options.contains("categoryId")) {
        if(!parent.isEmpty() && !options.value("categoryId").toString().isEmpty())return fail(tr("Group contents inherit their section."));
        if(!sections && !options.value("categoryId").toString().isEmpty())return fail(tr("Moving between sections requires a scrolling section layout."));
        category=options.value("categoryId").toString();bool found=category.isEmpty();
        for(const auto &entry:doc["categories"].toArray())if(entry.toObject()["id"]==category){found=true;if(entry.toObject()["presentation"].toObject().value("variant")=="library")return fail(tr("A media shelf manages its own contents. Choose a component section."));}
        if(!found)return fail(tr("The destination section no longer exists."));
    }
    const auto grid = ComponentSchema::componentGrid(entries,composition,doc["categories"].toArray(),parent,category);
    const int cols = grid["columns"].toInt(), rows = grid["rows"].toInt();
    if (!ComponentSchema::validPlacement(placement, cols, rows)) return fail(QStringLiteral("The candidate is outside the grid."));
    if(placement.value("height").toInt()>10)return fail(tr("A component can be at most ten rows tall."));
    int selected = -1;
    QVariantMap origin;
    QList<int> colliders;
    for (int i = 0; i < entries.size(); ++i) {
        const auto item = entries[i].toObject();
        if (item["id"] == id) { selected = i; origin = item["placement"].toObject().toVariantMap(); }
        else if (item["parentId"].toString()==parent && (!sections || !parent.isEmpty() || item["categoryId"].toString()==category) && item.contains("placement") && ComponentSchema::placementsOverlap(item["placement"].toObject().toVariantMap(), placement)) colliders.append(i);
    }
    if (selected < 0) return fail(QStringLiteral("The component no longer exists."));
    for(const int index:colliders)if(entries[index].toObject().value("locked").toBool())return fail(tr("A locked component occupies this region. Unlock it or choose another position."));
    QVariantList changes;
    auto moving = entries[selected].toObject(); moving["placement"] = QJsonObject::fromVariantMap(placement);
    if(category.isEmpty())moving.remove("categoryId");else moving["categoryId"]=category;
    if(parent.isEmpty())moving.remove("parentId");else moving["parentId"]=parent;
    entries[selected] = moving;
    if (!colliders.isEmpty() && strategy == "cancel") return fail(QStringLiteral("Occupied cells: choose Shift, Swap or Cancel."));
    if (!colliders.isEmpty() && strategy == "swap") {
        if (colliders.size() != 1 || origin.isEmpty()) return fail(QStringLiteral("Swap needs one occupant and an existing origin."));
        auto other = entries[colliders.first()].toObject();
        auto replacement = other["placement"].toObject().toVariantMap();
        replacement["x"] = origin["x"]; replacement["y"] = origin["y"];
        if(sourceCategory!=category){if(sourceCategory.isEmpty())other.remove("categoryId");else other["categoryId"]=sourceCategory;}
        if(sourceParent.isEmpty())other.remove("parentId");else other["parentId"]=sourceParent;
        other["placement"] = QJsonObject::fromVariantMap(replacement); entries[colliders.first()] = other;
    } else if (!colliders.isEmpty() && strategy == "shift") {
        const auto originalEntries=doc["components"].toArray();
        const auto stableOrder=[&originalEntries](int a,int b) {
            const auto x = originalEntries[a].toObject(), y = originalEntries[b].toObject();
            const auto p = x["placement"].toObject(), q = y["placement"].toObject();
            if (p["y"] != q["y"]) return p["y"].toInt() < q["y"].toInt();
            if (p["x"] != q["x"]) return p["x"].toInt() < q["x"].toInt();
            return x["id"].toString() < y["id"].toString();
        };
        std::sort(colliders.begin(),colliders.end(),stableOrder);
        if (colliders.size() >= 64) return fail(QStringLiteral("The shift exceeds 64 affected components."));
        QSet<int> fixed{selected};int attempts=0;bool affectedLimit=false;
        // Prefer empty regions, then try bounded deterministic cascades. Each
        // branch owns its complete result; failed branches publish nothing.
        std::function<bool(QList<int>)> solve=[&](QList<int> pending)->bool {
            pending.erase(std::remove_if(pending.begin(),pending.end(),[&](int i){return fixed.contains(i);}),pending.end());
            std::sort(pending.begin(),pending.end(),stableOrder);pending.erase(std::unique(pending.begin(),pending.end()),pending.end());
            if(pending.isEmpty())return true;
            const int index=pending.takeFirst();
            if(fixed.size()>=64){affectedLimit=true;return false;}
            if(originalEntries[index].toObject().value("locked").toBool())return false;
            const auto baseline=entries;const auto fixedBefore=fixed;
            auto item=entries[index].toObject();auto target=originalEntries[index].toObject()["placement"].toObject().toVariantMap();
            item.remove("placement");entries[index]=item;
            const auto vacant=entries;
            // Keep neighbors near their authored positions instead of sending
            // every displaced item to the first free cell at the top-left.
            QList<QPair<int,int>> cells;
            int searchRows=rows;
            if(rows==ComponentSchema::MaxScrollableRows){
                int bottom=placement.value("y").toInt()+placement.value("height").toInt();
                for(const auto &value:originalEntries){const auto entry=value.toObject();if(entry.value("parentId").toString()!=parent)continue;const auto p=entry["placement"].toObject();bottom=qMax(bottom,p["y"].toInt()+p["height"].toInt());}
                searchRows=qMin(rows,bottom+10);
            }
            for(int row=0;row<searchRows;++row)for(int col=0;col<cols;++col)cells.append({col,row});
            const int originalX=target["x"].toInt(),originalY=target["y"].toInt();
            std::stable_sort(cells.begin(),cells.end(),[=](const auto &a,const auto &b){
                return std::abs(a.first-originalX)+std::abs(a.second-originalY)<std::abs(b.first-originalX)+std::abs(b.second-originalY);
            });
            for(int pass=0;pass<2;++pass)for(const auto &cell:cells) {
                if(++attempts>4096){entries=baseline;fixed=fixedBefore;return false;}
                target["x"]=cell.first;target["y"]=cell.second;
                if(!ComponentSchema::validPlacement(target,cols,rows))continue;
                QList<int> displaced;bool blocked=false;
                for(int j=0;j<entries.size();++j) {
                    const auto other=entries[j].toObject();
                    if(j==index || other.value("parentId").toString()!=parent || (sections && parent.isEmpty() && other.value("categoryId").toString()!=category) || !other.contains("placement") || !ComponentSchema::placementsOverlap(other["placement"].toObject().toVariantMap(),target))continue;
                    if(fixed.contains(j) || other.value("locked").toBool()){blocked=true;break;}
                    displaced.append(j);
                }
                if(blocked || (pass==0 && !displaced.isEmpty()) || (pass==1 && displaced.isEmpty()))continue;
                auto placed=item;placed["placement"]=QJsonObject::fromVariantMap(target);entries[index]=placed;fixed.insert(index);
                auto next=pending;next.append(displaced);
                if(solve(next))return true;
                entries=vacant;fixed=fixedBefore;
                if(attempts>4096){entries=baseline;return false;}
            }
            entries=baseline;fixed=fixedBefore;return false;
        };
        if(!solve(colliders))return fail(attempts>4096?tr("Shift reached its 4096-proposal search limit. Choose another cell or Swap."):affectedLimit?tr("The shift exceeds 64 affected components."):tr("No complete shift fits. Cancel or choose another cell."));
    } else if (!colliders.isEmpty()) return fail(QStringLiteral("Choose a collision strategy."));
    const auto originalEntries=doc["components"].toArray();
    const auto appendChange=[&](int index) {const auto item=entries[index].toObject();changes.append(QVariantMap{{"id",item["id"].toString()},{"placement",item["placement"].toObject().toVariantMap()},{"categoryId",item.value("categoryId").toString()},{"parentId",item.value("parentId").toString()}});};
    appendChange(selected);
    for(int i=0;i<entries.size();++i)if(i!=selected && (entries[i].toObject().value("placement")!=originalEntries[i].toObject().value("placement") || entries[i].toObject().value("categoryId")!=originalEntries[i].toObject().value("categoryId") || entries[i].toObject().value("parentId")!=originalEntries[i].toObject().value("parentId")))appendChange(i);
    doc["components"] = entries;
    QString error;
    if (!HomeLayout::validateStudioDocument(QJsonDocument(doc).toJson(), &error)) return fail(error);
    return {{"valid", true}, {"placements", changes}, {"reason", QString()}};
}

QVariantMap StudioSession::additionProposal(const QString &type, const QVariantMap &placement,
                                           const QString &strategy, const QString &pageId, const QString &categoryId, const QString &parentId,const QVariantMap &configuration,bool enableScrolling)
{
    if (!m_active || !m_live) return {{"valid",false},{"reason",QStringLiteral("The edit session has closed.")}};
    HomeLayout temporary(m_live->registry()); configureWorker(temporary);
    if (!temporary.loadDocument(m_draft,false)) return {{"valid",false},{"reason",temporary.errorString()}};
    StudioSession transaction;
    if (!transaction.bind(&temporary)) return {{"valid",false},{"reason",transaction.errorString()}};
    const auto prototype=componentPrototype(type,configuration);
    if(prototype.isEmpty())return {{"valid",false},{"reason",errorString()}};
    const auto identity=prototype.value("rootId").toString();
    auto root=QJsonDocument::fromJson(m_draft).object();auto pages=root["pages"].toArray();auto document=root;int pageIndex=-1;
    if(!pageId.isEmpty()) {
        for(int i=0;i<pages.size();++i)if(pages[i].toObject()["id"]==pageId)pageIndex=i;
        if(pageIndex<0)return {{"valid",false},{"reason",QStringLiteral("The page no longer exists.")}};
        document=pages[pageIndex].toObject()["layout"].toObject();
    }
    if(enableScrolling){
        if(!parentId.isEmpty() || !categoryId.isEmpty())return {{"valid",false},{"reason",tr("Choose the page before enabling vertical scrolling.")}};
        auto composition=document["composition"].toObject();composition["verticalScrolling"]=true;document["composition"]=composition;
    }
    auto entries=document["components"].toArray();
    for(const auto &value:prototype.value("components").toList()) {
        auto item=QJsonObject::fromVariantMap(value.toMap());
        if(item["id"]==identity) {
            item.remove("placement");
            if(!parentId.isEmpty())item["parentId"]=parentId;
            else if(!categoryId.isEmpty())item["categoryId"]=categoryId;
        }
        entries.append(item);
    }
    document["components"]=entries;
    const auto store=[&] {
        if(pageIndex>=0){auto entry=pages[pageIndex].toObject();entry["layout"]=document;pages[pageIndex]=entry;root["pages"]=pages;}
        else root=document;
    };
    store();transaction.m_draft=QJsonDocument(root).toJson();
    auto proposal = transaction.placementProposal(identity,placement,strategy,pageId,QVariantMap{{"parentId",parentId},{"categoryId",categoryId}});
    if (!proposal.value("valid").toBool()) return proposal;
    proposal["id"] = identity;
    for(const auto &change:proposal["placements"].toList()) {
        const auto value=change.toMap();
        for(int i=0;i<entries.size();++i)if(entries[i].toObject()["id"]==value["id"].toString()) {
            auto entry=entries[i].toObject();entry["placement"]=QJsonObject::fromVariantMap(value["placement"].toMap());
            for(const auto &key:QStringList{"parentId","categoryId"}){const auto destination=value.value(key).toString();if(destination.isEmpty())entry.remove(key);else entry[key]=destination;}
            entries[i]=entry;
        }
    }
    document["components"]=entries;store();QString error;
    if(!HomeLayout::validateStudioDocument(QJsonDocument(root).toJson(),&error))return {{"valid",false},{"reason",error}};
    proposal["document"]=root.toVariantMap();
    proposal["commands"] = QVariantList{
        QVariantMap{{"operation","addResolved"},{"arguments",QVariantMap{{"type",type},{"placement",placement},{"strategy",strategy},{"categoryId",categoryId},{"parentId",parentId},{"configuration",configuration},{"enableScrolling",enableScrolling}}},{"pageId",pageId}}
    };
    return proposal;
}
