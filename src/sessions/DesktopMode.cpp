#include "DesktopMode.h"
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QGuiApplication>
#include <QWindow>
#include <QUuid>
#include <QDebug>
#include <QDBusServiceWatcher>

namespace {
const QString service="org.freedesktop.portal.Desktop";
const QString path="/org/freedesktop/portal/desktop";
const QString shortcuts="org.freedesktop.portal.GlobalShortcuts";
QString token(){return "isolde_"+QUuid::createUuid().toString(QUuid::Id128);}
}
QDBusArgument &operator<<(QDBusArgument &a,const DesktopShortcut &s){a.beginStructure();a<<s.id<<s.properties;a.endStructure();return a;}
const QDBusArgument &operator>>(const QDBusArgument &a,DesktopShortcut &s){a.beginStructure();a>>s.id>>s.properties;a.endStructure();return a;}

DesktopMode::DesktopMode(QObject *parent):DesktopMode(parent,QDBusConnection::connectToBus(QDBusConnection::SessionBus,"isolde-desktop-"+token()))
{
    m_connectionName=m_bus.name();
}
DesktopMode::DesktopMode(QObject *parent,const QDBusConnection &bus):QObject(parent),m_bus(bus)
{
    qDBusRegisterMetaType<DesktopShortcut>();qDBusRegisterMetaType<QList<DesktopShortcut>>();
    m_deadline.setSingleShot(true);m_deadline.setInterval(180000);
    connect(&m_deadline,&QTimer::timeout,this,[this]{fail("Desktop permission timed out. Isolde stayed open; try again.");});
    m_bus.connect(service,path,shortcuts,"Activated",this,SLOT(activated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    auto *owner=new QDBusServiceWatcher(service,m_bus,QDBusServiceWatcher::WatchForOwnerChange,this);
    connect(owner,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString &,const QString &previous,const QString &){
        if(previous.isEmpty())return;
        m_registered=false;m_registering=false;++m_identityGeneration;
        if(m_active || m_busy)fail("The desktop service restarted. Returned to Isolde; try Desktop again.");
    });
}
DesktopMode::~DesktopMode(){closePortal();if(!m_connectionName.isEmpty())QDBusConnection::disconnectFromBus(m_connectionName);}
QString DesktopMode::requestPath(const QString &value) const
{
    auto sender=m_bus.baseService();sender.remove(':');sender.replace('.','_');
    return path+"/request/"+sender+"/"+value;
}
void DesktopMode::request(const QString &method,const QVariantList &arguments,const QString &value)
{
    m_request=requestPath(value);
    if(!m_bus.connect(service,m_request,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)))){
        fail("Desktop return permission is unavailable. Isolde stayed open.");return;
    }
    auto call=QDBusMessage::createMethodCall(service,path,shortcuts,method);call.setArguments(arguments);
    auto *watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(call,10000),this);const auto generation=m_generation;
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *done){
        const auto reply=done->reply();done->deleteLater();
        if(generation==m_generation && reply.type()==QDBusMessage::ErrorMessage){
            qWarning() << "Desktop shortcut portal" << reply.errorName() << reply.errorMessage();
            fail("This desktop does not provide the required return-shortcut permission. Isolde stayed open.");
        }
    });
}
void DesktopMode::start(const QString &origin)
{
    begin(origin,false);
}
void DesktopMode::prepareNative(const QString &origin)
{
    begin(origin,true);
}
void DesktopMode::begin(const QString &origin,bool native)
{
    if(m_active || m_busy)return;
    m_nativeMode=native;
    m_error.clear();m_shortcut.clear();m_origin=origin.left(320);
    if(!m_bus.isConnected() || m_presentation.value("returnShortcut").toString().isEmpty()){
        fail("Desktop return support is unavailable. Isolde stayed open.");return;
    }
    ++m_generation;m_busy=true;m_deadline.start();emit changed();
    if(m_registered){beginSession();return;}
    m_phase=Registering;
    if(m_registering)return;
    m_registering=true;
    auto call=QDBusMessage::createMethodCall(service,path,"org.freedesktop.host.portal.Registry","Register");
    call.setArguments({QString::fromLatin1(ApplicationId),QVariantMap{}});
    const auto generation=m_identityGeneration;
    auto *watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(call,10000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *done){
        const auto reply=done->reply();done->deleteLater();
        if(generation!=m_identityGeneration)return;
        m_registering=false;
        if(reply.type()==QDBusMessage::ErrorMessage){qWarning()<<"Desktop portal identity"<<reply.errorName()<<reply.errorMessage();if(m_busy)fail("Could not register Isolde with this desktop. Isolde stayed open.");return;}
        m_registered=true;if(m_busy && m_phase==Registering)beginSession();
    });
}
void DesktopMode::beginSession()
{
    m_phase=Creating;m_sessionToken=token();
    const auto handle=token();request("CreateSession",{QVariantMap{{"handle_token",handle},{"session_handle_token",m_sessionToken}}},handle);
}
void DesktopMode::response(uint code,const QVariantMap &results)
{
    if(!m_busy || m_request.isEmpty() || !calledFromDBus() || message().path()!=m_request)return;
    m_bus.disconnect(service,m_request,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)));m_request.clear();
    if(code!=0){fail("Desktop return permission was not granted. Isolde stayed open.");return;}
    if(m_phase==Creating){
        auto sender=m_bus.baseService();sender.remove(':');sender.replace('.','_');
        const auto sessionHandle=results.value("session_handle");
        const auto session=sessionHandle.metaType()==QMetaType::fromType<QDBusObjectPath>()?sessionHandle.value<QDBusObjectPath>().path():sessionHandle.toString();
        if(session!=path+"/session/"+sender+"/"+m_sessionToken){fail("The desktop returned an invalid shortcut session. Isolde stayed open.");return;}
        m_session=session;m_phase=Binding;
        m_bus.connect(service,m_session,"org.freedesktop.portal.Session","Closed",this,SLOT(sessionClosed()));
        QString parentWindow;
        if(QGuiApplication::platformName()=="xcb")if(auto *window=QGuiApplication::focusWindow())parentWindow="x11:"+QString::number(window->winId(),16);
        const QList<DesktopShortcut> entries{{"return-home",{{"description",m_presentation.value("returnLabel")},{"preferred_trigger",m_presentation.value("returnShortcut")}}}};
        const auto handle=token();request("BindShortcuts",{QVariant::fromValue(QDBusObjectPath(m_session)),QVariant::fromValue(entries),parentWindow,QVariantMap{{"handle_token",handle}}},handle);
        return;
    }
    const auto entries=qdbus_cast<QList<DesktopShortcut>>(results.value("shortcuts"));
    for(const auto &entry:entries)if(entry.id=="return-home")m_shortcut=entry.properties.value("trigger_description").toString().left(160);
    if(m_shortcut.isEmpty()){fail("No return shortcut was granted. Isolde stayed open.");return;}
    m_deadline.stop();m_phase=Idle;m_busy=false;m_active=true;emit changed();
}
void DesktopMode::activated(const QDBusObjectPath &session,const QString &id,qulonglong,const QVariantMap &)
{
    if(m_active && session.path()==m_session && id=="return-home")returnHome();
}
void DesktopMode::sessionClosed()
{
    if(!calledFromDBus() || message().path()!=m_session)return;
    if(m_active || m_busy){m_error="The desktop return shortcut closed. Returned to Isolde.";returnHome();}
}
void DesktopMode::closePortal()
{
    ++m_generation;m_deadline.stop();
    if(!m_request.isEmpty()){
        m_bus.disconnect(service,m_request,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)));
        m_bus.asyncCall(QDBusMessage::createMethodCall(service,m_request,"org.freedesktop.portal.Request","Close"),1000);m_request.clear();
    }
    if(!m_session.isEmpty()){
        m_bus.disconnect(service,m_session,"org.freedesktop.portal.Session","Closed",this,SLOT(sessionClosed()));
        m_bus.asyncCall(QDBusMessage::createMethodCall(service,m_session,"org.freedesktop.portal.Session","Close"),1000);m_session.clear();
    }
    m_phase=Idle;
}
void DesktopMode::fail(const QString &message)
{
    closePortal();m_active=m_busy=false;m_error=message;emit changed();emit returned(m_origin);
}
void DesktopMode::returnHome(){const bool hadSession=m_active || m_busy;closePortal();m_active=m_busy=false;emit changed();if(hadSession)emit returned(m_origin);}
void DesktopMode::cancel(){m_error.clear();returnHome();}
