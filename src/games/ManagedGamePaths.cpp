#include "ManagedGamePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QUuid>
#include <QStandardPaths>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef Q_OS_LINUX
#include <sys/syscall.h>
#endif

QString ManagedGamePaths::gamesRoot(const QString &profile)
{
    if (profile != "default" && (QUuid(profile).isNull()
        || QUuid(profile).toString(QUuid::WithoutBraces) != profile)) return {};
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/native-profiles/" + profile + "/games");
}

bool ManagedGamePaths::isInstallRoot(const QString &profile, const QString &path, bool requirePresent)
{
    const auto base = gamesRoot(profile);
    if (base.isEmpty() || path.size() > 4096 || !QDir::isAbsolutePath(path)
        || QDir::cleanPath(path) != path || !path.startsWith(base + '/')) return false;
    const auto relative = QDir(base).relativeFilePath(path).split('/');
    if (relative.size() != 2 || relative.at(1) != "install") return false;
    const auto identity = relative.at(0);
    const auto uuid = QUuid(identity);
    if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != identity) return false;
    QFileInfo current(base);
    while (!current.exists()) {
        if (current.isSymLink()) return false;
        current = QFileInfo(current.absolutePath());
    }

    if (current.canonicalFilePath() != current.absoluteFilePath()) return false;
    if (!requirePresent && !QFileInfo::exists(path)) return true;
    const QFileInfo root(path), marker(path + "/.isolde-install.json");
    return root.isDir() && !root.isSymLink() && root.canonicalFilePath() == path
        && marker.isFile() && !marker.isSymLink() && marker.canonicalFilePath() == marker.absoluteFilePath();
}

QString ManagedGamePaths::recoverPendingUninstall(const QString &profile, const QString &path)
{
    const auto pending=path+".uninstall-pending";
    const QFileInfo staged(pending);
    if(!staged.exists() && !staged.isSymLink())return {};
    if(QFileInfo::exists(path) || QFileInfo(path).isSymLink()
        || !isInstallRoot(profile,path,false) || staged.isSymLink() || !staged.isDir()
        || staged.canonicalFilePath()!=pending
        || QFileInfo(pending+"/.isolde-install.json").isSymLink()
        || !QFileInfo(pending+"/.isolde-install.json").isFile())
        return "A pending managed uninstall needs manual recovery; all files were preserved.";
#ifdef Q_OS_LINUX
    const auto sourceBytes=QFile::encodeName(pending), targetBytes=QFile::encodeName(path);
    if(::syscall(SYS_renameat2,AT_FDCWD,sourceBytes.constData(),AT_FDCWD,targetBytes.constData(),1)!=0)
#else
    if(true)
#endif
        return "A pending managed uninstall could not be restored; all files were preserved.";
#ifdef Q_OS_UNIX
    const auto parent=QFileInfo(path).absolutePath().toLocal8Bit();
    const int fd=::open(parent.constData(),O_RDONLY|O_DIRECTORY);
    if(fd<0)return "The managed install was restored, but its parent directory could not be synced.";
    const int result=::fsync(fd);::close(fd);
    if(result!=0)return "The managed install was restored, but its parent directory could not be synced.";
#endif
    return {};
}
