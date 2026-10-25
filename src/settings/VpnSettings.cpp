#include "VpnSettings.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

VpnSettings::VpnSettings(QString profile,QObject *parent):QObject(parent),m_profile(std::move(profile)) {
    m_timer.setSingleShot(true);
    connect(&m_timer,&QTimer::timeout,this,[this]{
        if(m_stopping){m_process.kill();return;}
        m_stopping=true;m_process.terminate();m_timer.start(45000);
    });
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error!=QProcess::FailedToStart)return;
        m_timer.stop();m_busy=false;m_message=tr("VPN support could not start. Reinstall Isolde support files.");emit changed();
    });
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
        m_timer.stop();m_busy=false;
        const auto bytes=m_process.readAllStandardOutput();m_process.readAllStandardError();
        const auto result=bytes.size()<=65536?QJsonDocument::fromJson(bytes).object():QJsonObject{};
        if(result.contains("providers"))m_providers=result["providers"].toArray().toVariantList();
        if(result.contains("connections"))m_connections=result["connections"].toArray().toVariantList();
        if(result.contains("downloads"))m_downloads=result["downloads"].toArray().toVariantList();
        if(result.contains("countries"))m_countries=result["countries"].toArray().toVariantList();
        m_message=code==0 && exit==QProcess::NormalExit && !result.isEmpty()
            ?result["message"].toString(tr("Local VPN status refreshed. Check sign-in and country in the provider app."))
            :result["error"].toString(tr("VPN setup failed or timed out. Refresh and retry."));
        if(code!=0 || exit!=QProcess::NormalExit)qWarning().noquote()<<"VPN_SETUP_FAILED"<<m_message<<result["diagnostic"].toString();
        emit changed();
        if(result.contains("connections"))emit refreshed();
    });
}
VpnSettings::~VpnSettings(){
    m_timer.stop();m_process.disconnect(this);
    if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(45000)){m_process.kill();m_process.waitForFinished(1000);}}
}
void VpnSettings::perform(const QString &action,const QString &target,const QString &provider,const QString &country){
    if(m_busy)return;
    if(!QStringList{"status","install","defaults","open","account","capture","import","connect","disconnect"}.contains(action))return;
    if((action=="install" || action=="open" || action=="account") && !QStringList{"surfshark","protonvpn"}.contains(target))return;
    if(action=="open" || action=="account"){
        m_externalAction=action;m_externalTarget=target;m_busy=true;
        m_message=tr("Preparing the return shortcut before opening the provider…");emit changed();emit externalOpening();return;
    }
    launchHelper(action,target,provider,country);
}
void VpnSettings::resumeExternal(){
    if(m_externalAction.isEmpty())return;
    const auto action=m_externalAction,target=m_externalTarget;m_externalAction.clear();m_externalTarget.clear();
    launchHelper(action,target,"surfshark","CA");
}
void VpnSettings::cancelExternal(){
    if(m_externalAction.isEmpty())return;
    m_externalAction.clear();m_externalTarget.clear();m_busy=false;m_message=tr("Provider opening cancelled. No account settings changed.");emit changed();
}
void VpnSettings::launchHelper(const QString &action,const QString &target,const QString &provider,const QString &country){
    m_stopping=false;
    auto script=QCoreApplication::applicationDirPath()+"/../share/isolde/vpn-setup.py";
    if(!QFileInfo::exists(script))script=QCoreApplication::applicationDirPath()+"/share/isolde/vpn-setup.py";
    m_busy=true;m_message=action=="install" || action=="defaults"?tr("Installing VPN clients. Approve any guest administrator prompts. This can take several minutes."):tr("Updating VPN…");emit changed();
    m_timer.start(action=="install" || action=="defaults"?1800000:action=="capture"?240000:60000);
    m_process.start("/usr/bin/python3",{script,action,target,"--profile",m_profile,"--provider",provider,"--country",country});
}
