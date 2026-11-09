#pragma once

#include <QStringList>

class OpticalDiscGrants {
public:
    static QStringList mediaIds(const QString &profile);
    static bool approved(const QString &profile, const QString &mediaId);
    static QString authorize(const QString &profile, const QString &mediaId);
    static QString revoke(const QString &profile, const QString &mediaId);
};
