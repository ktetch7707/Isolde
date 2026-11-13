#include "PowerSettings.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QStringList>
#include <QMap>

PowerSettings::PowerSettings(QObject *parent,const QDBusConnection &bus,const QString &service)
    : QObject(parent),m_bus(bus),m_service(service) {}

void PowerSettings::refresh()
{
    if(m_busy)return;
    const auto generation=++m_generation;m_busy=true;m_remaining=3;m_capabilities.clear();
    m_status=tr("Checking system power capabilities…");emit changed();
    const QMap<QString,QString> queries{{"suspend","CanSuspend"},{"reboot","CanReboot"},{"poweroff","CanPowerOff"}};
    for(auto it=queries.cbegin();it!=queries.cend();++it){
        const auto action=it.key();
        auto message=QDBusMessage::createMethodCall(m_service,"/org/freedesktop/login1","org.freedesktop.login1.Manager",it.value());
        auto *watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(message,8000),this);
        connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation,action](QDBusPendingCallWatcher *call){
            QDBusPendingReply<QString> reply=*call;call->deleteLater();if(generation!=m_generation)return;
            const auto capability=reply.isError()?QStringLiteral("unavailable"):reply.value();
            m_capabilities[action]=QStringList{"yes","no","challenge","na"}.contains(capability)?capability:QStringLiteral("unavailable");
            if(--m_remaining==0){m_busy=false;m_status=tr("Choose an action and confirm it. Authentication may be required. Active playback, apps or profile restrictions prevent power changes.");}
            emit changed();
        });
    }
}

bool PowerSettings::request(const QString &action)
{
    const QMap<QString,QString> methods{{"suspend","Suspend"},{"reboot","Reboot"},{"poweroff","PowerOff"}};
    if(m_busy)return false;
    if(!methods.contains(action) || !QStringList{"yes","challenge"}.contains(m_capabilities.value(action).toString())){
        m_status=tr("That power action is unavailable. Refresh capabilities before retrying.");emit changed();return false;
    }
    if(m_guard && !m_guard()){m_status=tr("Close active apps, stop playback and finish pending media operations first. This profile must allow settings changes.");emit changed();return false;}
    const auto generation=++m_generation;m_busy=true;m_status=tr("Requesting the confirmed system power action…");emit changed();
    auto message=QDBusMessage::createMethodCall(m_service,"/org/freedesktop/login1","org.freedesktop.login1.Manager",methods[action]);
    message<<true; // login1 owns policy/authentication; no shell or privilege bypass.
    auto *watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(message,8000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *call){
        QDBusPendingReply<> reply=*call;call->deleteLater();if(generation!=m_generation)return;
        m_busy=false;m_status=reply.isError()?tr("Power request was denied, cancelled or failed. Refresh and retry when ready."):tr("The operating system accepted the power request.");emit changed();
    });
    return true;
}
