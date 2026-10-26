#pragma once
#include "input/WebRemoteScript.h"
#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QSocketNotifier>
#include <QTimer>
#include <functional>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// Only the dedicated, owned remote browser inherits these private pipes.
// Fixed commands install the input bridge; no cookies/storage/network queries,
// remote listener, arbitrary command API, or browser-data export is provided.
class ChromiumRemotePipe {
public:
    ChromiumRemotePipe(QCoreApplication &app,QProcess &process,const QVariantMap &appearance,std::function<void()> failed)
        :owner(app),child(process),failure(std::move(failed)),source(webRemoteScript(appearance)) {
        valid=!source.isEmpty() && ::pipe2(input,O_CLOEXEC)==0 && ::pipe2(output,O_CLOEXEC)==0;
        if(!valid)return;
        ::fcntl(input[1],F_SETFL,::fcntl(input[1],F_GETFL)|O_NONBLOCK);
        ::fcntl(output[0],F_SETFL,::fcntl(output[0],F_GETFL)|O_NONBLOCK);
        reader=new QSocketNotifier(output[0],QSocketNotifier::Read,&owner);
        writer=new QSocketNotifier(input[1],QSocketNotifier::Write,&owner);writer->setEnabled(false);
        QObject::connect(reader,&QSocketNotifier::activated,&owner,[this]{read();});
        QObject::connect(writer,&QSocketNotifier::activated,&owner,[this]{flush();});
        const int readFd=input[0],writeFd=output[1];
        child.setChildProcessModifier([readFd,writeFd]{
            ::setsid();::dup2(readFd,3);::dup2(writeFd,4);
            ::fcntl(3,F_SETFD,0);::fcntl(4,F_SETFD,0);
            if(readFd>4)::close(readFd);if(writeFd>4)::close(writeFd);
        });
    }
    ~ChromiumRemotePipe(){delete reader;delete writer;for(int fd:input)if(fd>=0)::close(fd);for(int fd:output)if(fd>=0)::close(fd);}
    bool valid=false;
    void started(){
        ::close(input[0]);input[0]=-1;::close(output[1]);output[1]=-1;
        send("Target.setDiscoverTargets",{{"discover",true}});
        QTimer::singleShot(15000,&owner,[this]{if(!ready && child.state()==QProcess::Running)failure();});
    }
private:
    int send(const QString &method,const QJsonObject &params={},const QString &session={}){
        QJsonObject message{{"id",++sequence},{"method",method},{"params",params}};if(!session.isEmpty())message["sessionId"]=session;
        outgoing+=QJsonDocument(message).toJson(QJsonDocument::Compact);outgoing+='\0';
        if(outgoing.size()>256*1024){failure();return 0;}flush();return sequence;
    }
    void flush(){
        while(!outgoing.isEmpty()){
            const auto count=::write(input[1],outgoing.constData(),size_t(outgoing.size()));
            if(count>0)outgoing.remove(0,count);else if(count<0 && errno==EINTR)continue;else break;
        }
        if(writer)writer->setEnabled(!outgoing.isEmpty());
    }
    void read(){
        char buffer[16384];ssize_t count;
        while((count=::read(output[0],buffer,sizeof buffer))>0){
            incoming.append(buffer,count);if(incoming.size()>2*1024*1024){failure();return;}
            while(incoming.contains('\0')){const int end=incoming.indexOf('\0');const auto message=QJsonDocument::fromJson(incoming.left(end)).object();incoming.remove(0,end+1);receive(message);}
        }
        if(count==0)reader->setEnabled(false);
    }
    void receive(const QJsonObject &message){
        const auto method=message.value("method").toString();const auto params=message.value("params").toObject();
        if(method=="Target.targetCreated"){
            const auto target=params.value("targetInfo").toObject();const auto type=target.value("type").toString(),id=target.value("targetId").toString();
            if((type=="page" || type=="iframe") && !attached.contains(id) && attached.size()<32){
                attached.insert(id);attaching.insert(send("Target.attachToTarget",{{"targetId",id},{"flatten",true}}),id);
            }
        }
        const auto id=message.value("id").toInt();
        if(attaching.remove(id)){
            const auto session=message.value("result").toObject().value("sessionId").toString();if(session.isEmpty())return;
            send("Page.enable",{},session);
            scripts.insert(send("Page.addScriptToEvaluateOnNewDocument",{{"source",source},{"worldName","IsoldeRemote"},{"runImmediately",true}},session));
        }else if(scripts.remove(id) && !message.contains("error")){ready=true;}
    }
    QCoreApplication &owner;QProcess &child;std::function<void()> failure;QString source;
    int input[2]={-1,-1},output[2]={-1,-1},sequence=0;bool ready=false;
    QSocketNotifier *reader=nullptr,*writer=nullptr;QByteArray incoming,outgoing;QHash<int,QString> attaching;QSet<int> scripts;QSet<QString> attached;
};
#endif
