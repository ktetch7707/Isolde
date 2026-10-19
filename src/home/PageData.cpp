#include "PageData.h"
#include "ComponentSchema.h"
#include "media/KodiMedia.h"
#include "media/KodiArtwork.h"
PageData::PageData(const QString &profile,KodiMedia *player,QObject *parent,const QString &directory)
    :QObject(parent),m_profile(profile),m_directory(directory),m_player(player){}
QObject *PageData::client(const QString &key,const QVariantMap &source) {
    if(key.size()>330 || !ComponentSchema::safeId(key.left(160)) || key.contains('/') || key.contains('\\'))return nullptr;
    if(!ComponentSchema::validPresentation(source,true))return nullptr;
    QVariantMap query{{"libraryKind",source.value("libraryKind","movies")},{"libraryView",source.value("libraryView","all")},{"libraryValue",source.value("libraryValue","")}};
    const auto kind=query["libraryKind"].toString(),view=query["libraryView"].toString(),value=query["libraryValue"].toString();
    if((view=="tag"||view=="genre")&&(kind!="movies"||value.trimmed().isEmpty()))return nullptr;
    if(view=="collection"&&!ComponentSchema::safeId(value))return nullptr;
    if(!m_clients.contains(key)) {
        if(m_clients.size()>=128)return nullptr;
        m_clients.insert(key,{new KodiMedia(m_profile,this,m_directory),{}});
    }
    auto &entry=m_clients[key];
    if(entry.source!=query){entry.source=query;if(view=="collection")entry.client->openCollection(value);else entry.client->openView(kind,view,value);}
    return entry.client;
}
QObject *PageData::artwork(const QString &kind,int id) {
    if(!QStringList{"movie","episode","tvshow","season"}.contains(kind)||id<1)return nullptr;
    const auto key=kind+":"+QString::number(id);
    if(!m_artwork.contains(key)) {
        if(m_artwork.size()>=512)return nullptr;
        auto *art=new KodiArtwork(m_profile,this,m_directory);m_artwork[key]=art;art->load(kind,id);
    }
    return m_artwork[key];
}
void PageData::activate(QObject *object,int index,bool resume) {
    auto *client=qobject_cast<KodiMedia *>(object);
    bool owned=false;for(const auto &entry:m_clients)if(entry.client==client)owned=true;
    if(!owned||!client||client->busy()||index<0||index>=client->movies().size()||m_player->playbackActive())return;
    const auto item=client->movies()[index].toMap();
    if(item["mediaType"]=="tvshow"||item["mediaType"]=="season")client->activate(index);
    else m_player->playMedia(item["mediaType"].toString(),item["mediaId"].toInt(),resume);
}
void PageData::refresh(QObject *object){
    for(auto *art:m_artwork)art->deleteLater();m_artwork.clear();++m_revision;emit changed();
    for(auto &entry:m_clients)if(entry.client==object)entry.client->refresh();
}
void PageData::invalidate(const QString &reason){for(auto &entry:m_clients){entry.client->invalidate(reason);entry.source.clear();}for(auto *art:m_artwork)art->clear();}
