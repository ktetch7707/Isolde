#include "ManagedGamePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QStandardPaths>

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
