#pragma once

#include <QString>

class ManagedGamePaths {
public:
    static QString gamesRoot(const QString &profile);
    static bool isInstallRoot(const QString &profile, const QString &path, bool requirePresent = true);
    static QString recoverPendingUninstall(const QString &profile, const QString &path, bool requireMarker = true);
};
