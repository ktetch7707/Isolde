#include "DesktopMode.h"
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QGuiApplication>
#include <QWindow>
#include <QUuid>

namespace {
const QString service="org.freedesktop.portal.Desktop";
const QString path="/org/freedesktop/portal/desktop";
const QString shortcuts="org.freedesktop.portal.GlobalShortcuts";
QString token(){return "isolde_"+QUuid::createUuid().toString(QUuid::Id128);}
}
QDBusArgument &operator<<(QDBusArgument &a,const DesktopShortcut &s){a.beginStructure();a<<s.id<<s.properties;a.endStructure();return a;}
const QDBusArgument &operator>>(const QDBusArgument &a,DesktopShortcut &s){a.beginStructure();a>>s.id>>s.properties;a.endStructure();return a;}

DesktopMode::DesktopMode(QObject *parent,const QDBusConnection &bus):QObject(parent),m_bus(bus)
{
    qDBusRegisterMetaType<DesktopShortcut>();qDBusRegisterMetaType<QList<DesktopShortcut>>();
    m_deadline.setSingleShot(true);m_deadline.setInterval(180000);
    connect(&m_deadline,&QTimer::timeout,this,[this]{fail("Desktop permission timed out. Isolde stayed open; try again.");});
    m_bus.connect(service,path,shortcuts,"Activated",this,SLOT(activated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
}
DesktopMode::~DesktopMode(){closePortal();}
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
        if(generation==m_generation && reply.type()==QDBusMessage::ErrorMessage)
            fail("This desktop does not provide the required return-shortcut permission. Isolde stayed open.");
    });
}
void DesktopMode::start(const QString &origin)
{
    if(m_active || m_busy)return;
    m_error.clear();m_shortcut.clear();m_origin=origin.left(320);
    if(!m_bus.isConnected() || m_presentation.value("returnShortcut").toString().isEmpty()){
        fail("Desktop return support is unavailable. Isolde stayed open.");return;
    }
    ++m_generation;m_busy=true;m_phase=Creating;m_sessionToken=token();m_deadline.start();emit changed();
    const auto handle=token();request("CreateSession",{QVariantMap{{"handle_token",handle},{"session_handle_token",m_sessionToken}}},handle);
}
void DesktopMode::response(uint code,const QVariantMap &results)
{
    if(!m_busy || m_request.isEmpty() || !calledFromDBus() || message().path()!=m_request)return;
    m_bus.disconnect(service,m_request,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)));m_request.clear();
    if(code!=0){fail("Desktop return permission was not granted. Isolde stayed open.");return;}
    if(m_phase==Creating){
        auto sender=m_bus.baseService();sender.remove(':');sender.replace('.','_');
        const auto session=results.value("session_handle").toString();
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
