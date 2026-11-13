#include "KodiImporter.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QUuid>

namespace {
bool ordinaryPath(QString path)
{
    for(;!path.isEmpty();){if(QFileInfo(path).isSymLink())return false;const auto parent=QFileInfo(path).absolutePath();if(parent==path)break;path=parent;}
    return true;
}
}

KodiImporter::KodiImporter(const QString &profile,QObject *parent):QObject(parent),m_profile(profile)
{
    m_valid=profile=="default" || (!QUuid(profile).isNull() && QUuid(profile).toString(QUuid::WithoutBraces)==profile);
    m_deadline.setSingleShot(true);m_deadline.setInterval(180000);
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{
        m_output+=m_process.readAllStandardOutput();
        if(m_output.size()>65536){m_importStatus=tr("Kodi import returned excessive status data.");m_process.kill();}
    });
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{m_process.readAllStandardError();});
    connect(&m_process,&QProcess::started,this,[this]{if(m_cancelled)m_process.terminate();});
    connect(&m_deadline,&QTimer::timeout,this,[this]{
        m_importStatus=tr("Kodi import timed out. Choose Scan to verify recovery before retrying.");
        const auto pid=m_process.processId();m_process.terminate();
        QTimer::singleShot(7000,this,[this,pid]{if(pid && m_process.processId()==pid && m_process.state()!=QProcess::NotRunning)m_process.kill();});
    });
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){finish(code==0 && exit==QProcess::NormalExit);});
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){m_importStatus=tr("Kodi import support could not start. Repair the Isolde installation.");finish(false);}});
    loadImportState();
}

KodiImporter::~KodiImporter()
{
    shutdown();
}

void KodiImporter::shutdown()
{
    m_deadline.stop();m_deferredAction.clear();m_releaseBackend={};m_prepareBackend={};m_backendPreparing={};
    disconnect(&m_process,nullptr,this,nullptr);
    if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(7000)){m_process.kill();m_process.waitForFinished(3000);}}
    m_lock.reset();m_importInProgress=false;
}

QString KodiImporter::backupDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/kodi-import-backups/"+m_profile;
}

bool KodiImporter::recoveryPending() const {return m_valid && QFileInfo::exists(backupDir()+"/transaction.json");}

void KodiImporter::loadImportState()
{
    if(!m_valid){m_importStatus=tr("Invalid Kodi import profile.");return;}
    if(!ordinaryPath(backupDir())){m_importStatus=tr("Kodi import storage must not use redirected paths.");return;}
    QFile file(backupDir()+"/receipt.json");
    if(file.exists()){
        if(QFileInfo(file).isSymLink() || file.size()>8*1024*1024 || !file.open(QIODevice::ReadOnly)){m_importStatus=tr("Kodi import receipt is invalid. Choose Scan to inspect recovery.");return;}
        const auto state=QJsonDocument::fromJson(file.readAll()).object();
        const auto token=state.value("token").toString();
        if(state.value("schemaVersion").toInt()!=1 || state.value("profile").toString()!=m_profile || QUuid(token).isNull() || QUuid(token).toString(QUuid::WithoutBraces)!=token){m_importStatus=tr("Kodi import receipt belongs to another profile or version.");return;}
        m_backupAvailable=true;m_backupPath=backupDir()+"/"+token;m_canImport=true;
        m_canRollback=state.value("imported").toBool();
        m_importStatus=m_canRollback?tr("Imported into this Isolde profile. Desktop Kodi is unchanged."):tr("Snapshot ready for preview and import.");
    }
    if(recoveryPending()){m_canImport=false;m_canRollback=false;m_importStatus=tr("Interrupted Kodi import needs recovery. Choose Scan before using the library.");}
}

void KodiImporter::runAction(const QString &action)
{
    if(!m_valid || m_importInProgress)return;
    QString helper;
    for(const auto &candidate:{QCoreApplication::applicationDirPath()+"/../share/isolde/kodi-import.py",QCoreApplication::applicationDirPath()+"/share/isolde/kodi-import.py"})
        if(QFileInfo(candidate).isFile()){helper=QFileInfo(candidate).absoluteFilePath();break;}
    const auto python=QStandardPaths::findExecutable("python3");
    if(helper.isEmpty() || python.isEmpty()){m_importStatus=tr("Kodi import support is missing. Repair the Isolde installation.");emit changed();return;}
    if(!m_lock){
        if(m_prepareBackend && !m_prepareBackend()){
            if(m_backendPreparing && m_backendPreparing()){
                m_deferredAction=action;m_backendPrepared=true;m_importInProgress=true;
                m_importStatus=tr("Waiting for Kodi to finish saving and close safely. Back cancels; playback or scanning prevents import.");
            }else m_importStatus=tr("Stop playback, wait for scans and close the Kodi application before importing or restoring its configuration.");
            emit changed();return;
        }
        m_backendPrepared=true;
    }
    const auto home=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/native-profiles/"+m_profile+"/kodi/home";
    if(!ordinaryPath(home)){m_importStatus=tr("Private Kodi storage must not use redirected paths.");releaseBackend();emit changed();return;}
    if(!QDir().mkpath(home)){m_importStatus=tr("Could not prepare private Kodi storage.");releaseBackend();emit changed();return;}
    const auto dataRoot=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(!QFile::setPermissions(dataRoot,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner) || !QFile::setPermissions(home,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)){
        m_importStatus=tr("Could not protect private Kodi storage.");releaseBackend();emit changed();return;
    }
    if(!m_lock){
        m_lock=std::make_unique<QLockFile>(home+"/.isolde-managed.lock");
        if(!m_lock->tryLock()){m_lock.reset();m_importStatus=tr("This profile's Kodi is still in use. Close it and retry.");releaseBackend();emit changed();return;}
    }
    m_action=action;++m_generation;m_cancelled=false;m_releaseRequested=false;m_output.clear();m_importInProgress=true;m_importStatus=tr("Working on the private Kodi snapshot…");emit changed();
    m_process.setProgram(python);
    m_process.setArguments({helper,"--profile",m_profile,"--config-root",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),"--data-root",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),action});
    m_deadline.start();m_process.start();
}

void KodiImporter::finish(bool success)
{
    if(!m_importInProgress)return;
    m_deadline.stop();m_output+=m_process.readAllStandardOutput();
    const auto result=QJsonDocument::fromJson(m_output).object();success=success && result.value("ok").toBool();
    m_importInProgress=false;
    if(success){
        m_backupAvailable=result.value("backupAvailable").toBool();m_backupPath=result.value("backupPath").toString();
        m_canImport=result.value("canImport").toBool();m_canRollback=result.value("canRollback").toBool();
        if(!result.value("previewText").toString().isEmpty())m_previewText=result.value("previewText").toString();
        m_conflicts.clear();for(const auto &entry:result.value("conflicts").toArray())m_conflicts.append(entry.toString());
        m_importStatus=result.value("importStatus").toString();
        if(m_cancelled)m_importStatus=tr("The operation finished before cancellation. Use Rollback to restore an imported profile.");
    }else{
        loadImportState();
        m_importStatus=result.value("error").toString(tr("Kodi import did not complete. Choose Scan to verify recovery, then retry."));
    }
    const bool held=success && !m_cancelled && !m_releaseRequested && m_backupAvailable && !m_canRollback && (m_action=="backup" || m_action=="preview");
    if(recoveryPending()){m_lock.reset();m_canImport=false;m_canRollback=false;m_importStatus+=tr(" Recovery is pending; choose Scan before restarting Kodi.");}
    else if(!held)releaseBackend();
    else m_importStatus=tr("Verified snapshot ready. The library is paused during review. Back resumes without importing.");
    if(m_action=="backup" && success)emit backupCreated(m_backupPath);
    if(m_action=="import")emit importCompleted(success,m_importStatus);
    if(m_action=="rollback")emit rollbackCompleted(success,m_importStatus);
    emit changed();
}

void KodiImporter::scanExistingKodi(){runAction("scan");}
void KodiImporter::continueWhenBackendReady()
{
    if(m_deferredAction.isEmpty() || (m_backendPreparing && m_backendPreparing()))return;
    const auto action=m_deferredAction;m_deferredAction.clear();m_importInProgress=false;
    runAction(action);
    if(!m_importInProgress && !m_lock)releaseBackend();
}
void KodiImporter::createBackup(){runAction("backup");}
void KodiImporter::previewImport(){runAction("preview");}
void KodiImporter::import(){runAction("import");}
void KodiImporter::rollback(){runAction("rollback");}
void KodiImporter::clearConflicts(){m_conflicts.clear();emit changed();}
void KodiImporter::releaseBackend()
{
    m_lock.reset();const bool prepared=m_backendPrepared;m_backendPrepared=false;
    if(prepared && m_releaseBackend)m_releaseBackend();
}
void KodiImporter::releasePreview()
{
    m_releaseRequested=true;
    if(m_importInProgress)cancel();else if(!recoveryPending())releaseBackend();
}
void KodiImporter::cancel()
{
    if(!m_importInProgress)return;
    if(!m_deferredAction.isEmpty()){
        m_deferredAction.clear();m_importInProgress=false;m_cancelled=true;
        m_importStatus=tr("Import preparation cancelled. No configuration changed.");releaseBackend();emit changed();return;
    }
    m_cancelled=true;m_importStatus=tr("Cancelling and restoring the private Kodi configuration…");emit changed();
    m_process.terminate();const auto generation=m_generation;
    QTimer::singleShot(7000,this,[this,generation]{if(m_importInProgress && m_cancelled && m_generation==generation)m_process.kill();});
}
