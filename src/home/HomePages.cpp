#include "HomeLayout.h"
#include "ThemeColors.h"
#include "PageModel.h"
#include "ComponentSchema.h"
#include <QJsonDocument>
#include <QUuid>
#include <functional>

QString HomeLayout::rootPageName() const {
    return m_rootPage.value("name").toString(studioOptions().value("rootPageName").toString());
}
QVariantList HomeLayout::studioPages() const {
    QVariantList result;
    for(const auto &entry:m_pages){const auto page=entry.toObject();result.append(QVariantMap{{"id",page["id"].toString()},{"name",page["name"].toString()}});}
    if(rootPageVisible())result.insert(qMin(m_rootPage.value("position").toInt(),int(result.size())),QVariantMap{{"id",QString()},{"name",rootPageName()}});
    return result;
}

QVariantMap HomeLayout::homeAppearance() const {
    if(m_owner)return m_owner->homeAppearance();
    for(const auto &entry:m_pages)if(entry.toObject()["id"]==m_homePageId) {
        auto result=appearance();const auto overrides=entry.toObject()["layout"].toObject()["theme"].toObject().toVariantMap();
        for(auto it=overrides.cbegin();it!=overrides.cend();++it) {
            if(it.key()=="surfaceThemes") {
                auto policies=result.value(it.key()).toMap();const auto local=it.value().toMap();
                for(auto policy=local.cbegin();policy!=local.cend();++policy)policies[policy.key()]=policy.value();
                result[it.key()]=policies;
            } else result[it.key()]=it.value();
        }
        auto resolved=ThemeColors::resolve(result,overrides);if(result.contains("surfaceThemes"))resolved["surfaceThemes"]=result["surfaceThemes"];return resolved;
    }
    return appearance();
}

QVariantMap HomeLayout::resolvedSurfacePalette(const QVariantMap &appearance) const
{
    // Derived roles identify a resolved context. Preserve it verbatim so panel
    // opacity is not composited a second time by nested shell consumers.
    if (appearance.contains("selectedTextColor") && appearance.contains("inputTextColor")
        && appearance.contains("backgroundTextColor")) return appearance;
    return ComponentSchema::surfacePalette(appearance);
}

QVariantMap HomeLayout::surfaceAppearance(const QString &surface, const QString &originId) const
{
    if (m_owner) return m_owner->surfaceAppearance(surface, originId);
    if (!QStringList{"search", "apps", "settings", "kodi"}.contains(surface)) return {};
    const auto home = homeAppearance();
    auto origin = home;
    QVariantMap originTheme;
    QString originName;
    if (originId.isEmpty()) {
        originName = rootPageName();
        const auto raw = appearance();
        for (const auto &key : m_themeKeys) if (raw.contains(key)) originTheme[key] = raw[key];
    }
    QString source = rootPageName();
    for (const auto &entry : m_pages) {
        const auto page = entry.toObject();
        if (page["id"] == m_homePageId) source = page["name"].toString();
        if (!originId.isEmpty() && page["id"] == originId) {
            originName = page["name"].toString();
            originTheme = page["layout"].toObject()["theme"].toObject().toVariantMap();
        }
    }
    for (auto it = originTheme.cbegin(); it != originTheme.cend(); ++it) origin[it.key()] = it.value();
    // Settings never observes an originating page or an editor's private draft.
    const auto policies = home.value("surfaceThemes").toMap();
    const auto localPolicies = originTheme.value("surfaceThemes").toMap();
    const auto policy = (surface != "settings" && localPolicies.contains(surface) ? localPolicies : policies).value(surface).toMap();
    const auto mode = policy.value("mode", surface == "settings" ? "home" : "origin").toString();
    auto effective = mode == "home" || surface == "settings" ? home : origin;
    if (mode == "custom") {
        const auto tokens = policy.value("tokens").toMap();
        for (auto it = tokens.cbegin(); it != tokens.cend(); ++it) effective[it.key()] = it.value();
    }
    auto result = ComponentSchema::surfacePalette(effective);
    result["themeSource"] = surface == "settings" || mode == "home" || originName.isEmpty() ? source : originName;
    result["themeMode"] = mode;
    return result;
}

QString HomeLayout::createPage(const QString &name, const QString &preset) {
    if (m_owner) return m_owner->createPage(name,preset);
    if (name.trimmed().isEmpty() || name.trimmed().size()>MaxPageNameLength || m_pages.size()+(rootPageVisible()?1:0)>=MaxPages || !QStringList{"blank","movies","shows","standalone","home"}.contains(preset)) {
        setError(tr("Choose a page name and template. Up to %1 pages are supported.").arg(MaxPages));return {};
    }
    const auto id="page-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject config{{"schemaVersion",1},{"enabled",true},{"fit",false},{"columns",1},{"margin",DefaultHomeMargin},{"gap",DefaultHomeGap},{"headerMode","identity"},
        {"identity",name.trimmed()},{"headerControls",QJsonArray{"settings","edit"}},{"pageNavigation","none"}};
    QJsonArray sections;
    if(preset!="blank")sections.append(QJsonObject{{"id","library"},{"name",preset=="shows"?"Shows":"Movies"},{"presentation",QJsonObject{
        {"variant","library"},{"height",PresetLibrarySectionHeight},{"libraryKind",preset=="shows"?"shows":"movies"},{"libraryView","all"},{"libraryLayout","grid"},{"libraryColumns",DefaultLibraryColumns},{"padding",DefaultLibraryPadding},{"radius",DefaultLibraryRadius},{"panelOpacity",DefaultPanelOpacity}}}});
    QJsonObject document{{"schemaVersion",2},{"theme",QJsonObject{}},{"composition",config},{"categories",sections},{"components",QJsonArray{}}};
    if(preset=="home"){document=QJsonDocument::fromJson(documentData()).object();document.remove("pages");document.remove("homePageId");document.remove("rootPage");document.remove("studioAppearance");}
    m_pages.append(QJsonObject{{"id",id},{"name",name.trimmed()},{"layout",document}});
    if(!save()){m_pages.removeLast();return {};}emit layoutChanged();return id;
}
bool HomeLayout::renamePage(const QString &id,const QString &name) {
    if(m_owner)return m_owner->renamePage(id,name);
    if(name.trimmed().isEmpty()||name.trimmed().size()>80){setError(tr("Use a page name of 1-80 characters."));return false;}
    for(qsizetype i=0;i<m_pages.size();++i){auto entry=m_pages[i].toObject();if(entry["id"]!=id)continue;
        const auto previous=m_pages;entry["name"]=name.trimmed();m_pages[i]=entry;
        if(!save()){m_pages=previous;return false;}emit layoutChanged();return true;}
    setError(tr("Page not found."));return false;
}
bool HomeLayout::removePage(const QString &id) {
    if(m_owner)return m_owner->removePage(id);
    if(id==m_homePageId){setError(tr("Set another page as Home before deleting this page."));return false;}
    std::function<bool(const QJsonValue &)> linksToPage=[&](const QJsonValue &value) {
        if(value.isArray()){for(const auto &child:value.toArray())if(linksToPage(child))return true;}
        if(value.isObject()) {
            const auto object=value.toObject();
            if(object["type"]=="open-page" && object["pageId"]==id)return true;
            for(auto it=object.begin();it!=object.end();++it)if(linksToPage(it.value()))return true;
        }
        return false;
    };
    if(linksToPage(QJsonDocument::fromJson(documentData()).object())) {
        setError(tr("This page has incoming links. Change or remove its links before deleting it."));return false;
    }
    for(qsizetype i=0;i<m_pages.size();++i)if(m_pages[i].toObject()["id"]==id){const auto previous=m_pages;m_pages.removeAt(i);

        if(!save()){m_pages=previous;return false;}emit layoutChanged();return true;}
    setError(tr("Page not found."));return false;
}

void HomeLayout::syncPages() {
    if(m_owner)return;
    for(const auto &id:m_pageModels.keys()) {
        bool found=false;for(const auto &value:m_pages)if(value.toObject()["id"]==id)found=true;
        if(!found)m_pageModels.take(id)->deleteLater();
    }
    for(auto it=m_pageModels.begin();it!=m_pageModels.end();++it)for(const auto &value:m_pages){
        const auto entry=value.toObject();if(entry["id"]!=it.key())continue;auto *model=it.value();
        model->setParent(this);
        if(QJsonObject::fromVariantMap(model->document())!=entry["layout"].toObject()){
            model->loadDocument(QJsonDocument(entry["layout"].toObject()).toJson(),false);}}
}
bool HomeLayout::linkPage(const QString &id,bool asTab) {
    if(!pageModel(id)){setError(tr("Page not found."));return false;}QString name=id;
    for(const auto &entry:(m_owner?m_owner->pages():pages()))if(entry.toMap()["id"]==id)name=entry.toMap()["name"].toString();
    const auto linkId="link-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(asTab){auto config=m_composition;config["schemaVersion"]=1;config["enabled"]=true;config["headerMode"]="navigation";auto tabs=config.value("navigation").toList();
        if(tabs.isEmpty()){tabs.append(QVariantMap{{"id","home"},{"name","Home"}});config["initialTab"]="home";}
        tabs.append(QVariantMap{{"id",linkId},{"name",name},{"action",QVariantMap{{"type","open-page"},{"pageId",id}}}});config["navigation"]=tabs;return setComposition(config);}
    const auto previous=m_items;m_items.append(QJsonObject{{"id",linkId},{"kind","action"},{"name",name},{"action",QJsonObject{{"type","open-page"},{"pageId",id}}}});
    if(!save()){m_items=previous;return false;}emit layoutChanged();return true;
}
QString HomeLayout::addLibraryWidget(const QString &name,const QString &kind) {
    if(name.trimmed().isEmpty() || !QStringList{"movies","shows"}.contains(kind))return {};
    const auto previousComposition=m_composition;const auto previousCategories=m_categories;
    if(!m_composition.value("enabled").toBool()){
        m_composition["schemaVersion"]=1;m_composition["enabled"]=true;m_composition["fit"]=false;
    }
    const auto id="library-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_categories.append(QJsonObject{{"id",id},{"name",name.trimmed().left(60)},{"presentation",QJsonObject{
        {"variant","library"},{"height",500},{"libraryKind",kind},{"libraryView","all"},{"libraryLayout","grid"},{"libraryColumns",4}}}});
    if(!save()){m_composition=previousComposition;m_categories=previousCategories;return {};}emit layoutChanged();return id;
}
bool HomeLayout::setItemAction(const QString &id,const QVariantMap &action) {
    const auto normalized=ComponentSchema::action(action);if(normalized.isEmpty()){setError(tr("Choose a valid action and destination."));return false;}
    for(qsizetype i=0;i<m_items.size();++i){auto item=m_items[i].toObject();if(item["id"]!=id)continue;
        const bool visualLink=item["componentType"]=="button" || item["componentType"]=="image";
        if(item["kind"]=="widget" && !visualLink)continue;
        const auto previous=m_items;if(!visualLink)item["kind"]="action";item["action"]=QJsonObject::fromVariantMap(normalized);item.remove("appId");
        if(item["name"].toString().isEmpty())item["name"]="Link";
        QString error;const auto canonical=ComponentSchema::component(item.toVariantMap(),true,&error);
        if(canonical.isEmpty()){setError(error);return false;}m_items[i]=QJsonObject::fromVariantMap(canonical);
        if(!save()){m_items=previous;return false;}emit layoutChanged();return true;}
    setError(tr("Choose a launcher card."));return false;
}
bool HomeLayout::setItemName(const QString &id,const QString &name) {
    if(name.trimmed().isEmpty()||name.trimmed().size()>80){setError(tr("Use a card name of 1-80 characters."));return false;}
    for(qsizetype i=0;i<m_items.size();++i){auto item=m_items[i].toObject();if(item["id"]!=id)continue;const auto previous=m_items;
        item["name"]=name.trimmed();if(item["kind"]=="app"){item["kind"]="action";item.remove("appId");}m_items[i]=item;
        if(!save()){m_items=previous;return false;}emit layoutChanged();return true;}
    return false;
}
bool HomeLayout::addButton(){
    QString error;
    const auto item=ComponentSchema::component({{"id","button-"+QUuid::createUuid().toString(QUuid::WithoutBraces)},{"kind","widget"},{"widgetType","button"},{"componentType","button"},{"name","Button"},{"action",QVariantMap{{"type","open-apps"}}}},true,&error);
    if(item.isEmpty()){setError(error);return false;}
    m_items.append(QJsonObject::fromVariantMap(item));
    if(!save()){m_items.removeLast();return false;}emit layoutChanged();return true;
}


bool HomeLayout::storePage(const QString &id,const QJsonObject &document) {
    HomeLayout validator(m_registry);validator.m_owner=this;
    if(!validator.loadDocument(QJsonDocument(document).toJson(),false)){setError(validator.errorString());return false;}
    for(qsizetype i=0;i<m_pages.size();++i){auto entry=m_pages[i].toObject();if(entry["id"]!=id)continue;
        const auto previous=m_pages;entry["layout"]=document;m_pages[i]=entry;
        QString validationError;
        if(!validateStudioDocument(documentData(),&validationError,m_defaultTheme) || !save()){
            m_pages=previous;if(!validationError.isEmpty())setError(validationError);return false;
        }emit layoutChanged();return true;}
    setError(tr("This page was removed. Choose another page."));return false;
}
QObject *HomeLayout::pageModel(const QString &id) {
    if(m_owner)return m_owner->pageModel(id);
    for(const auto &value:m_pages){const auto entry=value.toObject();if(entry["id"]!=id)continue;
        if(m_pageModels.contains(id))return m_pageModels[id];
        auto *model=new PageModel(this,id,this);
        model->setParent(this);
        model->loadDocument(QJsonDocument(entry["layout"].toObject()).toJson(),false);
        m_pageModels[id]=model;return model;}
    return nullptr;
}
