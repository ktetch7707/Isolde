#include "MediaGrants.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
bool within(const QString &path, const QString &root) { return path == root || path.startsWith(root + '/'); }
}
QStringList MediaGrants::folders(const QString &profile)
{
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "MediaAccess")
        .value("profiles/" + profile + "/kodi/folders").toStringList();
}
QString MediaGrants::validate(const QString &path, bool requirePresent)
{
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path
        || QStringList{"/", "/home", "/media", "/mnt", "/tmp"}.contains(path)
        || QFileInfo(path).absolutePath() == "/home")
        return "Choose a specific absolute media folder.";
    for (const auto &part : path.split('/', Qt::SkipEmptyParts))
        if (part.startsWith('.')) return "Hidden account and configuration folders cannot be shared.";
    for (const auto &root : QStringList{"/etc", "/usr", "/opt", "/sys", "/dev", "/root", "/var", "/run", "/proc", "/bin", "/sbin", "/lib", "/lib64"})
        if (within(path, root)) return "System folders cannot be shared as media.";
    for (const auto &privateRoot : QStringList{QDir::homePath(),
             QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
             QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}) {
        if (within(privateRoot, path)) return "Choose your movie folder, not an entire home or account folder.";
        if (privateRoot != QDir::homePath() && within(path, privateRoot))
            return "Isolde's private profile folders cannot be shared.";
    }
    QFileInfo info(path);
    if (!info.exists()) {
        // Missing drives are allowed at launch, but existing symlink ancestors
        // must not silently redirect a previously approved path.
        while (!info.exists()) {
            if (info.isSymLink()) return "The saved folder contains a symbolic link. Choose its real location again.";
            info = QFileInfo(info.absolutePath());
        }
        if (info.canonicalFilePath() != info.absoluteFilePath())
            return "The saved folder was redirected. Choose its real location again.";
        return requirePresent ? "That folder is unavailable. Connect the drive and try again." : QString();
    }
    if (!info.isDir() || !info.isReadable()) return "Choose a readable media folder.";
    if (info.canonicalFilePath() != path) return "Choose the real folder, not a symbolic link.";
    return {};
}
QString MediaGrants::save(const QString &profile, const QStringList &folders)
{
    if (folders.size() > 32) return "Up to 32 media folders are supported.";
    for (const auto &path : folders) {
        const auto error = validate(path, false);
        if (!error.isEmpty()) return error;
    }
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "MediaAccess");
    settings.setValue("profiles/" + profile + "/kodi/folders", folders);
    settings.sync();
    return settings.status() == QSettings::NoError ? QString() : "Could not save media-folder permissions.";
}
