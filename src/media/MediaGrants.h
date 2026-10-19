#pragma once
#include <QStringList>

// Explicit, per-user Kodi read grants. Never edits Kodi sources or media files.
class MediaGrants {
public:
    static QStringList folders(const QString &profile);
    static QString validate(const QString &path, bool requirePresent = true);
    static QString save(const QString &profile, const QStringList &folders);
};
