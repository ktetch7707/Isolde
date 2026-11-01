#include "PageData.h"
#include "ComponentSchema.h"
#include "media/KodiMedia.h"
#include "media/KodiArtwork.h"
#include "app/AppRegistry.h"
#include "sessions/SessionManager.h"
#include "ActionDispatcher.h"
PageData::PageData(const QString &profile,KodiMedia *player,QObject *parent,const QString &directory)
    :QObject(parent),m_profile(profile),m_directory(directory),m_player(player){
    connect(player,&KodiMedia::changed,this,[this,wasConnected=false]() mutable {
        const bool connected=m_player->connected();
        // A Home shelf can reach the bridge before Kodi finishes starting.
        // Retry failed clients once on the owning player's connection transition.
        if(connected && !wasConnected && m_valid) {
            for(auto &entry:m_clients)if(!entry.client->connected() && !entry.client->busy())entry.client->refresh();
            for(auto it=m_artwork.cbegin();it!=m_artwork.cend();++it)
                if(it.value()->source().isEmpty())it.value()->load(it.key().section(':',0,0),it.key().section(':',1,1).toInt());
        }
        wasConnected=connected;
    });
    connect(player,&KodiMedia::playbackReturned,this,[this](bool){
        QTimer::singleShot(1500,this,[this]{if(m_valid)for(auto &entry:m_clients)entry.client->refresh();});
    });
}
QObject *PageData::client(const QString &key,const QVariantMap &source) {
    if(!m_valid)return nullptr;
    // The key is bounded and its id prefix must be a safe component id; path
    // separators are rejected so keys can never address arbitrary files.
    if(key.size()>ComponentSchema::MaxPageDataClientKeySize
       || !ComponentSchema::safeId(key.left(ComponentSchema::MaxPageDataClientIdLength))
       || key.contains(QLatin1String("/")) || key.contains('\\'))return nullptr;
    if(!ComponentSchema::validPresentation(source,true))return nullptr;
    QVariantMap query{{"libraryKind",source.value("libraryKind","movies")},{"libraryView",source.value("libraryView","all")},{"libraryValue",source.value("libraryValue","")}};
    const auto kind=query["libraryKind"].toString(),view=query["libraryView"].toString(),value=query["libraryValue"].toString();
    if(kind=="history"){query["libraryLimit"]=source.value("libraryLimit",50);query["historyApps"]=source.value("historyApps",QVariantList{});}
    if(kind=="games")return nullptr; // Games never creates a Kodi client.
    if(kind=="history" && (view!="watched" || !value.isEmpty()))return nullptr;
    if(kind=="albums" && (view!="all" || !value.isEmpty()))return nullptr;
    if(kind=="channels" && (view!="all" || !value.isEmpty()))return nullptr;
    if((view=="tag"||view=="genre")&&(kind!="movies"||value.trimmed().isEmpty()))return nullptr;
    if(view=="collection"&&!ComponentSchema::safeId(value))return nullptr;
    if(view=="playlist" && (!QStringList{"movies","shows"}.contains(kind) || !KodiMedia::validPlaylistName(value)))return nullptr;
    if(!m_clients.contains(key)) {
        if(m_clients.size()>=ComponentSchema::MaxPageDataClients)return nullptr;
        m_clients.insert(key,{new KodiMedia(m_profile,this,m_directory),{}});
    }
    auto &entry=m_clients[key];
    if(entry.source!=query){entry.source=query;if(kind=="history"){entry.client->setAppOpenings(appOpenings(query));entry.client->setHistoryNotice(historyNotice(query));entry.client->openHistory(query["libraryLimit"].toInt());}else if(view=="collection")entry.client->openCollection(value);else entry.client->openView(kind,view,value);}
    return entry.client;
}
QObject *PageData::playlistCatalog() {
    if(!m_valid)return nullptr;
    const QString key=":studio-playlist-catalog";
    if(!m_clients.contains(key)){
        if(m_clients.size()>=ComponentSchema::MaxPageDataClients)return nullptr;
        auto *catalog=new KodiMedia(m_profile,this,m_directory);
        m_clients.insert(key,{catalog,{{"catalog",true}}});catalog->openPlaylists();
    }
    return m_clients[key].client;
}
QObject *PageData::artwork(const QString &kind,int id) {
    if(!m_valid)return nullptr;
    if(!QStringList{"movie","episode","tvshow","season","album","song","channel"}.contains(kind)||id<1)return nullptr;
    const auto key=kind+":"+QString::number(id);
    if(!m_artwork.contains(key)) {
        if(m_artwork.size()>=ComponentSchema::MaxPageDataArtwork)return nullptr;
        auto *art=new KodiArtwork(m_profile,this,m_directory);m_artwork[key]=art;art->load(kind,id);
    }
    return m_artwork[key];
}
void PageData::activate(QObject *object,int index,bool resume,const QString &originId) {
    if(!m_valid)return;
    auto *client=qobject_cast<KodiMedia *>(object);
    bool owned=false;for(const auto &entry:m_clients)if(entry.client==client)owned=true;
    if(!owned||!client||client->busy()||index<0||index>=client->movies().size()||m_player->playbackActive())return;
    const auto item=client->movies()[index].toMap();
    if(item["providerId"]=="isolde-app-openings") {
        const auto id=item["appId"].toString();
        if(m_actions && m_registry && m_registry->find(id))m_actions->dispatch({{"schemaVersion",1},{"type","launch-app"},{"appId",id}},originId);
        else client->refresh();
        return;
    }
    if(item["mediaType"]=="tvshow"||item["mediaType"]=="season"||item["mediaType"]=="album")client->activate(index);
    else m_player->playMedia(item["mediaType"].toString(),item["mediaId"].toInt(),resume);
}
void PageData::refresh(QObject *object){
    if(!m_valid)return;
    for(auto *art:m_artwork)art->deleteLater();m_artwork.clear();++m_revision;emit changed();
    for(auto &entry:m_clients)if(entry.client==object)entry.client->refresh();
}
void PageData::invalidate(const QString &reason){m_valid=false;m_openings.clear();m_pendingOpening.clear();m_pendingOpeningTime={};if(m_sessions)disconnect(m_sessions,nullptr,this,nullptr);for(auto &entry:m_clients){entry.client->retire(reason);entry.source.clear();}for(auto *art:m_artwork)art->clear();}
void PageData::unavailable(const QString &reason){if(!m_valid)return;for(auto &entry:m_clients)entry.client->invalidate(reason);for(auto *art:m_artwork)art->clear();}
void PageData::setApplications(AppRegistry *registry,SessionManager *sessions,ActionDispatcher *dispatcher) {
    if(!m_valid || m_sessions || m_registry || !registry || !sessions || !dispatcher)return;
    m_registry=registry;m_sessions=sessions;m_actions=dispatcher;
    connect(sessions,&SessionManager::appOpened,this,[this](const QString &id){
        if(!m_valid || !m_registry || !m_registry->find(id))return;
        bool enabled=false;
        for(const auto &entry:m_clients)if(entry.source.value("libraryKind")=="history" && entry.source.value("historyApps").toList().contains(id))enabled=true;
        if(!enabled)return; // Opt in before observing app openings; no browser/service history reads.
        m_pendingOpening=id;m_pendingOpeningTime=QDateTime::currentDateTime();
    });
    connect(sessions,&SessionManager::sessionFailed,this,[this](const QString &id,const QString &){
        if(id==m_pendingOpening){m_pendingOpening.clear();m_pendingOpeningTime={};}
    });
    connect(sessions,&SessionManager::returnedHome,this,[this](const QString &){
        if(!m_valid)return;
        // Publish only after a successful session return; failed starts cannot
        // fabricate history or evict an earlier successful opening.
        if(!m_pendingOpening.isEmpty())m_openings[m_pendingOpening]=m_pendingOpeningTime;
        m_pendingOpening.clear();m_pendingOpeningTime={};
        while(m_openings.size()>50) {
            auto oldest=m_openings.begin();
            for(auto it=m_openings.begin();it!=m_openings.end();++it)
                if(it.value()<oldest.value() || (it.value()==oldest.value() && it.key()<oldest.key()))oldest=it;
            m_openings.erase(oldest);
        }
        updateHistoryClients();
    });
    connect(registry,&AppRegistry::appsChanged,this,[this]{if(m_valid)updateHistoryClients();});
}
QVariantList PageData::appOpenings(const QVariantMap &source) const {
    QVariantList items;if(!m_valid || !m_registry)return items;
    const auto selected=source.value("historyApps").toList();
    for(auto it=m_openings.cbegin();it!=m_openings.cend();++it) {
        const auto *app=m_registry->find(it.key());
        if(!selected.contains(it.key()) || !app || !QStringList{"web","desktop","android"}.contains(app->type))continue;
        items.append(QVariantMap{{"providerId","isolde-app-openings"},{"mediaType","app"},{"mediaId",it.key()},{"appId",it.key()},
            {"title",app->name.left(160)},{"logoSource",app->iconSource},{"viewedAt",it.value().toString("yyyy-MM-dd HH:mm:ss")},{"subtitle","App opened · this session"}});
    }
    return items;
}
void PageData::updateHistoryClients() {
    for(auto &entry:m_clients)if(entry.source.value("libraryKind")=="history") {
        entry.client->setAppOpenings(appOpenings(entry.source));
        entry.client->setHistoryNotice(historyNotice(entry.source));
        if(!entry.client->busy())entry.client->refresh();
    }
}
QString PageData::historyNotice(const QVariantMap &source) const {
    const auto selected=source.value("historyApps").toList();if(selected.isEmpty())return {};
    int unavailable=0;
    for(const auto &id:selected){const auto *app=m_registry?m_registry->find(id.toString()):nullptr;if(!m_sessions || !app || !QStringList{"web","desktop","android"}.contains(app->type))++unavailable;}
    return " · App openings: this session only; service viewing history unsupported."
        +(unavailable?QString(" %1 selected app source(s) unavailable; install or choose another.").arg(unavailable):QString());
}
