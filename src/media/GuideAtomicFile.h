#pragma once
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QSaveFile>
#include <QFile>
#include <functional>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif
namespace GuideAtomicFile {
// Publish a new standalone export/template without an exists/rename overwrite
// race. Profile database transactions remain SQLite's responsibility.
inline bool publish(const QString &path,const QByteArray &bytes,const std::function<bool()> &allowed={}) {
    for(const auto c:path)if(c.unicode()<32 || c.unicode()==127)return false;
#ifdef Q_OS_LINUX
    const QFileInfo info(path);if(!info.isAbsolute() || info.fileName().isEmpty())return false;
    int directory=::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(directory<0)return false;
    const auto parts=info.absolutePath().split('/',Qt::SkipEmptyParts);
    for(const auto &part:parts){if(part=="." || part==".."){::close(directory);return false;}const int next=::openat(directory,part.toUtf8().constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);::close(directory);if(next<0)return false;directory=next;}
    const auto temporary=(".isolde-export-"+QUuid::createUuid().toString(QUuid::WithoutBraces)).toUtf8();const auto name=info.fileName().toUtf8();
    const int file=::openat(directory,temporary.constData(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(file<0){::close(directory);return false;}
    qsizetype written=0;bool ok=true;
    while(written<bytes.size()){const auto count=::write(file,bytes.constData()+written,size_t(bytes.size()-written));if(count<=0){ok=false;break;}written+=count;}
    ok=ok && ::fsync(file)==0;::close(file);bool published=false;
    if(ok && allowed && !allowed())ok=false;
    if(ok){published=::linkat(directory,temporary.constData(),directory,name.constData(),0)==0;ok=published && ::fsync(directory)==0;}
    if(!ok && published)::unlinkat(directory,name.constData(),0);
    ::unlinkat(directory,temporary.constData(),0);if(!ok)::fsync(directory);::close(directory);return ok;
#else
    const auto temporary=path+".isolde-"+QUuid::createUuid().toString(QUuid::WithoutBraces);QSaveFile file(temporary);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())return false;
    const bool ok=(!allowed || allowed()) && QFile::rename(temporary,path);if(!ok)QFile::remove(temporary);return ok;
#endif
}
}
