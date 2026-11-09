#include "SteamLibraryGrants.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace {
bool within(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + '/');
}

bool allowedMountPath(const QString &path)
{
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 3 || (parts.at(0) != "media" && parts.at(0) != "mnt"))
        return false;
    for (const auto &part : parts)
        if (part.startsWith('.')) return false;
    return true;
}
}

QStringList SteamLibraryGrants::folders(const QString &profile)
{
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "MediaAccess")
        .value("profiles/" + profile + "/steam/libraries").toStringList();
}

QString SteamLibraryGrants::validate(const QString &path, bool requirePresent)
{
    if (path.size() > 4096 || !QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path || !allowedMountPath(path))
        return "Choose a specific Steam library folder under /media or /mnt.";

    const auto home = QDir::homePath();
    const auto config = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const auto data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (within(path, home) || within(home, path) || within(path, config) || within(config, path)
        || within(path, data) || within(data, path))
        return "Steam libraries inside the active account or Isolde profile cannot be added as external grants.";

    QFileInfo info(path);
    if (!info.exists()) {
        while (!info.exists()) {
            if (info.isSymLink()) return "The selected Steam library path contains a symbolic link.";
            info = QFileInfo(info.absolutePath());
        }
        if (info.canonicalFilePath() != info.absoluteFilePath())
            return "The selected Steam library path was redirected.";
        return requirePresent ? "That Steam library folder is unavailable." : QString();
    }

    const auto canonical = info.canonicalFilePath();
    if (!info.isDir() || canonical != path)
        return "Choose the real Steam library folder, not a symbolic link.";
    const QFileInfo steamapps(path + "/steamapps");
    if (!steamapps.isDir() || steamapps.isSymLink() || steamapps.canonicalFilePath() != path + "/steamapps")
        return "The selected folder is not a Steam library with a local steamapps directory.";
    return {};
}

QString SteamLibraryGrants::validateList(const QStringList &paths, bool requirePresent)
{
    if (paths.size() > 8) return "Up to eight external Steam libraries can be approved per profile.";
    QStringList checked;
    for (const auto &path : paths) {
        const auto error = validate(path, requirePresent);
        if (!error.isEmpty()) return error;
        if (checked.contains(path)) return "The same Steam library was approved more than once.";
        for (const auto &existing : checked)
            if (within(path, existing) || within(existing, path))
                return "Nested external Steam library grants are not supported.";
        checked.append(path);
    }
    return {};
}

QString SteamLibraryGrants::save(const QString &profile, const QStringList &paths)
{
    if ((profile != "default" && (QUuid(profile).isNull()
         || QUuid(profile).toString(QUuid::WithoutBraces) != profile)) || paths.size() > 8)
        return "Invalid profile or too many external Steam libraries.";
    const auto validation=validateList(paths,false);
    if(!validation.isEmpty())return validation;
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "MediaAccess");
    settings.setValue("profiles/" + profile + "/steam/libraries", paths);
    settings.sync();
    return settings.status() == QSettings::NoError ? QString() : "Could not save external Steam library access.";
}
