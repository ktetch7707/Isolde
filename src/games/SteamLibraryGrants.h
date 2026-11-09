#pragma once

#include <QStringList>

class SteamLibraryGrants {
public:
    static QStringList folders(const QString &profile);
    static QString validate(const QString &path, bool requirePresent = true);
    static QString validateList(const QStringList &folders, bool requirePresent = false);
    static QString save(const QString &profile, const QStringList &folders);
};
