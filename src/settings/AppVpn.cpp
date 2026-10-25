#include "AppVpn.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QDebug>

bool AppVpn::safeApp(const QString &id){return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$").match(id).hasMatch();}
bool AppVpn::safeUuid(const QString &id){return QRegularExpression("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$").match(id).hasMatch();}
AppVpn::AppVpn(QString path,QString profile,QString helper,QObject *parent)
    :QObject(parent),m_path(std::move(path)),m_profile(std::move(profile)),m_helper(std::move(helper)) {
    m_invalidRules=!load();m_timeout.setSingleShot(true);
    connect(&m_timeout,&QTimer::timeout,this,[this]{
        if(!m_cancelled){cancel();return;}
        m_process.kill();
    });
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{
        m_output+=m_process.readAllStandardOutput();if(m_output.size()>65536)cancel();
    });
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{m_process.readAllStandardError();});
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&AppVpn::finish);
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart)finish(-1,QProcess::CrashExit);
    });
}
AppVpn::~AppVpn(){
    m_timeout.stop();m_process.disconnect(this);
    if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(45000)){m_process.kill();m_process.waitForFinished(1000);}}
}
bool AppVpn::load(){
    QFile file(m_path);if(!file.exists())return true;
    if(QFileInfo(file).canonicalFilePath()!=QFileInfo(file).absoluteFilePath() || !file.open(QIODevice::ReadOnly) || file.size()>65536){m_error=tr("Saved app VPN rules could not be read. Repair VPN settings before opening apps.");return false;}
    const auto doc=QJsonDocument::fromJson(file.readAll()).object();const auto routes=doc["routes"].toObject();
    if(doc.size()!=2 || doc["schemaVersion"]!=1 || !doc["routes"].isObject() || routes.size()>128){m_error=tr("Unsupported app VPN rules. The saved file was left unchanged.");return false;}
    for(auto it=routes.begin();it!=routes.end();++it)if(!safeApp(it.key()) || !it.value().isString() || !safeUuid(it.value().toString())){m_error=tr("Invalid app VPN rule. The saved file was left unchanged.");return false;}
    m_routes=routes.toVariantMap();return true;
}
bool AppVpn::setRoute(const QString &app,const QString &uuid){
    if(m_invalidRules || busy() || !safeApp(app) || (!uuid.isEmpty() && !safeUuid(uuid))){m_error=tr("Choose a valid app and saved VPN location. Invalid saved rules must be repaired first.");emit changed();return false;}
    bool known=uuid.isEmpty();for(const auto &entry:m_connections)if(entry.toMap()["uuid"]==uuid)known=true;
    if(!known){m_error=tr("Refresh and choose a saved location in this profile.");emit changed();return false;}
    auto next=m_routes;if(uuid.isEmpty())next.remove(app);else next[app]=uuid;
    if(next.size()>128){m_error=tr("App VPN rules are limited to 128 apps.");emit changed();return false;}
    const auto directory=QFileInfo(m_path).absoluteDir();QFileInfo ancestor(directory.absolutePath());
    while(!ancestor.exists() && ancestor.absolutePath()!=ancestor.absoluteFilePath())ancestor=QFileInfo(ancestor.absolutePath());
    if(ancestor.canonicalFilePath()!=ancestor.absoluteFilePath() || QFileInfo(m_path).isSymLink() || QFileInfo(m_path+".bak").isSymLink() || !QDir().mkpath(directory.absolutePath())){m_error=tr("The app VPN rules path is unavailable.");emit changed();return false;}
    const auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",1},{"routes",QJsonObject::fromVariantMap(next)}}).toJson();
    if(QFileInfo::exists(m_path)){
        QFile old(m_path);QSaveFile backup(m_path+".bak");backup.setDirectWriteFallback(false);
        if(!old.open(QIODevice::ReadOnly) || !backup.open(QIODevice::WriteOnly)){m_error=tr("VPN rule backup could not be saved. Previous rules remain active.");emit changed();return false;}
        const auto previous=old.readAll();
        if(backup.write(previous)!=previous.size() || !backup.commit()){m_error=tr("VPN rule backup could not be saved. Previous rules remain active.");emit changed();return false;}
    }
    QSaveFile output(m_path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()){m_error=tr("App VPN rules could not be saved. Previous rules remain active.");emit changed();return false;}
    m_routes=next;m_verifiedApp.clear();m_error.clear();emit changed();return true;
}
void AppVpn::start(const QString &action,const QString &target){
    auto helper=m_helper;
    if(helper.isEmpty()){
        helper=QCoreApplication::applicationDirPath()+"/../share/isolde/vpn-setup.py";
        if(!QFileInfo::exists(helper))helper=QCoreApplication::applicationDirPath()+"/share/isolde/vpn-setup.py";
    }
    m_busy=true;m_cancelled=false;m_output.clear();emit changed();m_timeout.start(100000);
    m_process.start("/usr/bin/python3",{helper,action,target,"--profile",m_profile});
}
bool AppVpn::refresh(){if(busy())return false;start("status");return true;}
bool AppVpn::prepare(const QString &app){
    if(m_invalidRules){emit failed(app,m_error);return false;}
    if(m_routes.value(app).toString().isEmpty())return true;
    if(m_verifiedApp==app){m_verifiedApp.clear();return true;}
    if(busy()){emit failed(app,tr("VPN settings are refreshing. Try opening the app again when refresh finishes."));return false;}
    m_error.clear();m_app=app;start("switch",m_routes.value(app).toString());return false;
}
void AppVpn::cancel(){
    if(!m_busy)return;
    m_cancelled=true;m_verifiedApp.clear();m_process.terminate();m_timeout.start(45000);
}
void AppVpn::finish(int code,QProcess::ExitStatus exit){
    if(!m_busy)return;
    m_timeout.stop();m_output+=m_process.readAllStandardOutput();
    const auto result=m_output.size()<=65536?QJsonDocument::fromJson(m_output).object():QJsonObject{};
    const auto app=m_app;m_app.clear();m_busy=false;
    if(code==0 && exit==QProcess::NormalExit && !m_cancelled && !result.isEmpty() && !result.contains("error")){
        if(result.contains("connections"))m_connections=result["connections"].toArray().toVariantList();
        if(!app.isEmpty() && result["country"].toString().size()==2){
            qInfo().noquote()<<"VPN_LOCATION_VERIFIED"<<app<<result["country"].toString()<<result["verifiedAt"].toInteger();
            m_verifiedApp=app;m_error.clear();emit changed();emit ready(app);return;
        }
        if(app.isEmpty()){emit changed();return;}
    }
    m_error=result["error"].toString(m_cancelled?tr("VPN switching cancelled. The app was not opened."):tr("VPN location could not be verified. The app was not opened. Refresh VPN settings and retry."));
    emit changed();if(!app.isEmpty())emit failed(app,m_error);
}
