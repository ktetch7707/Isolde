#include "CompatibilityRuntime.h"
#include <QFileInfo>
#include <QFile>

QVariantMap CompatibilityRuntime::classify(bool present,bool trusted)
{
    return {{"id","wine"},{"name","Windows compatibility installer"},
        {"state",!present?"missing":trusted?"unverified":"unsafe"},
        {"supported",false},
        {"requirements","A pinned, verified Isolde compatibility runtime and isolated installer adapter are required. No Windows installer is executed."}};
}

QVariantMap CompatibilityRuntime::discover()
{
#ifndef Q_OS_LINUX
    return classify(false,false);
#else
    const QFileInfo candidate("/usr/bin/wine");
    if(!candidate.exists() && !candidate.isSymLink())return classify(false,false);
    const auto resolved=candidate.canonicalFilePath();
    if(!resolved.startsWith("/usr/") || !QFileInfo(resolved).isFile()
        || !QFileInfo(resolved).isExecutable())return classify(true,false);
    for(auto path=QFileInfo(resolved);;path=QFileInfo(path.absolutePath())){
        if(path.isSymLink() || path.ownerId()!=0 || (path.permissions() & (QFile::WriteGroup|QFile::WriteOther)))
            return classify(true,false);
        if(path.absoluteFilePath()=="/")break;
    }
    return classify(true,true);
#endif
}
