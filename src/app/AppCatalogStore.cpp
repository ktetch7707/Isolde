#include "app/AppCatalog.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

bool AppCatalog::checkForUpdates()
{
    if(busy() || !m_onlineStoreSearch || !m_provider){setError(tr("Enable F-Droid access before checking for updates."));return false;}
    QStringList packages;
    for(const auto &entry:m_entries)if(isInstalled(entry.app.id) && entry.custom && entry.app.type=="android"
        && !m_updatesDisabled.contains(entry.app.id))
        packages.append(QJsonDocument::fromJson(entry.manifestData).object().value("packageName").toString());
    if(packages.isEmpty()){setError(tr("No apps are subscribed to update notifications."));return false;}
    setError({});m_operationStatus.clear();m_availableUpdates.clear();m_provider->requestUpdates(packages);emit entriesChanged();return true;
}

bool AppCatalog::setUpdatesEnabled(const QString &id,bool enabled)
{
    const auto *entry=findEntry(id);
    if(busy() || !entry || !isInstalled(id) || !entry->custom || entry->app.type!="android")return false;
    if(!enabled && !m_updatesDisabled.contains(id) && m_updatesDisabled.size()>=256){
        setError(tr("This profile has reached its 256 saved update preferences."));return false;
    }
    const bool previous=m_updatesDisabled.contains(id);
    if(enabled)m_updatesDisabled.remove(id);else m_updatesDisabled.insert(id);
    if(!saveState()){
        if(previous)m_updatesDisabled.insert(id);else m_updatesDisabled.remove(id);
        return false;
    }
    if(!enabled)m_availableUpdates.remove(id);
    emit entriesChanged();return true;
}

bool AppCatalog::updateApp(const QString &id)
{
    if(!isInstalled(id) || m_updatesDisabled.contains(id) || !m_availableUpdates.contains(id)){
        setError(tr("Subscribe and check for updates before choosing Update."));return false;
    }
    return beginDownload(m_availableUpdates.value(id),true);
}

bool AppCatalog::commitUpdate(const Entry &entry)
{
    for(auto &current:m_entries)if(current.app.id==entry.app.id){
        const Entry previous=current;
        if(!m_registry->replaceApp(entry.app)){setError(tr("Unable to register the updated app."));return false;}
        current=entry;m_customPackages.insert(entry.app.id,entry.version);
        if(!saveState()){
            current=previous;m_customPackages.insert(previous.app.id,previous.version);
            m_registry->replaceApp(previous.app);return false;
        }
        m_availableUpdates.remove(entry.app.id);emit entriesChanged();return true;
    }
    return false;
}

void AppCatalog::androidEvent(const QJsonObject &event)
{
    const auto type=event.value("event").toString();
    if(type=="error"){setError(event.value("message").toString().left(512));return;}
    if(type=="status"){m_operationStatus=event.value("message").toString().left(512);emit entriesChanged();return;}
    if(type=="cancelled"){
        m_operationStatus=tr("Cancelled; restoring the previous Android data...");
        if(event.value("rolledBack").toBool())setError(tr("Cancelled. The previous Android data was restored."));
        emit entriesChanged();return;
    }
    if(type=="starting" || type=="installing" || type=="removing"){
        m_operationStatus=type=="starting"?tr("Starting this profile's Android session..."):
            type=="installing"?tr("Installing the verified package..."):tr("Removing the app...");
        emit entriesChanged();return;
    }
    if(type!="prepared")return;
    if(!m_cancellable || m_activationCommitted)return;
    auto profile=qApp->property("isoldeProfile").toString();if(profile.isEmpty())profile="default";
    const auto token=event.value("token").toString();
    if(m_cancelRequested || profile!=m_operationProfile || event.value("operation")!=m_operation
        || !QRegularExpression("^[a-f0-9]{32}$").match(token).hasMatch()
        || !event.value(m_operation=="install"?"packagePresent":"packageAbsent").toBool()){
        m_androidProcess.write("rollback\n");return;
    }
    // The helper has stopped its session and retained a full profile-data snapshot.
    // Persisting this token atomically makes commit recoverable after either process exits.
    m_cancellable=false;m_operationStatus=tr("Finishing transaction...");emit entriesChanged();
    const auto previousToken=m_storeTransaction;m_storeTransaction=token;
    m_committing=true;
    m_activationCommitted=m_operation=="install"?
        (m_pendingUpdate?commitUpdate(*m_pendingUpdate):install(m_operationId)):uninstall(m_operationId);
    m_committing=false;
    if(!m_activationCommitted)m_storeTransaction=previousToken;
    m_androidProcess.write(m_activationCommitted?"commit\n":"rollback\n");
}

void AppCatalog::cancelOperation()
{
    if(!canCancelOperation())return;
    m_cancelRequested=true;
    m_operationStatus=m_operation=="setup"?tr("Cancelling after the current system step..."):
        tr("Cancelling and restoring the previous Android data...");
    if(m_downloadReply)m_downloadReply->abort();
    else if(m_operation=="setup")m_androidProcess.write("cancel\n");
    else m_androidProcess.terminate();
    emit entriesChanged();
}

bool AppCatalog::setupAndroid(bool softwareRendering)
{
    if(busy())return false;
    QString helper;
    const auto base=QCoreApplication::applicationDirPath();
    for(const auto &path:{base+"/../share/isolde/android-setup.py",base+"/share/isolde/android-setup.py"})
        if(QFileInfo(path).isFile()){helper=path;break;}
    if(helper.isEmpty()){setError(tr("Android setup support is missing. Repair the Isolde distribution."));return false;}
    m_operation="setup";m_operationId="android-setup";m_cancelRequested=false;m_cancellable=true;
    m_operationName=tr("Android setup");
    m_activationCommitted=false;m_operationEvents.clear();setError({});
    m_operationStatus=tr("Preparing Android setup; administrator approval may be requested...");
    m_androidProcess.disconnect(this);
    connect(&m_androidProcess,&QProcess::readyReadStandardOutput,this,[this]{
        m_operationEvents+=m_androidProcess.readAllStandardOutput();
        if(m_operationEvents.size()>65536){cancelOperation();return;}
        while(m_operationEvents.contains('\n')){
            const auto end=m_operationEvents.indexOf('\n');
            const auto event=QJsonDocument::fromJson(m_operationEvents.left(end)).object();m_operationEvents.remove(0,end+1);
            if(event.value("event")=="completed")m_activationCommitted=event.value("ready").toBool();
            else androidEvent(event);
        }
    });
    connect(&m_androidProcess,&QProcess::readyReadStandardError,this,[this]{m_androidProcess.readAllStandardError();});
    connect(&m_androidProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){m_operationId.clear();setError(tr("Android setup could not start. Retry setup."));emit entriesChanged();}
    });
    connect(&m_androidProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        if(!code && status==QProcess::NormalExit && m_activationCommitted){
            setError({});m_operationStatus=tr("Android setup complete. Choose an app to install.");
            if(m_onlineStoreSearch && m_provider)m_provider->requestSearch(m_storeQuery);
        }else if(m_errorString.isEmpty())setError(m_cancelRequested?tr("Setup cancelled at a safe checkpoint. Retry to continue."):tr("Android setup did not finish. Review administrator approval and retry."));
        m_operationId.clear();emit entriesChanged();
    });
    QStringList arguments{helper};if(softwareRendering)arguments << "--software-rendering";
    m_androidProcess.setProgram("/usr/bin/python3");m_androidProcess.setArguments(arguments);
    m_androidProcess.start();emit entriesChanged();return true;
}
