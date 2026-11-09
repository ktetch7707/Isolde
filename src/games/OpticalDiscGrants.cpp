#include "OpticalDiscGrants.h"

#include <QRegularExpression>
#include <QSettings>
#include <QUuid>
#include <algorithm>

namespace {
bool valid(const QString &value)
{
    return QRegularExpression("^[0-9a-f]{64}$").match(value).hasMatch();
}
bool validProfile(const QString &profile)
{
    if(profile=="default")return true;
    const auto id=QUuid(profile);
    return !id.isNull() && id.toString(QUuid::WithoutBraces)==profile;
}
QString key(const QString &profile)
{
    return "profiles/"+profile+"/optical-media";
}
}

QStringList OpticalDiscGrants::mediaIds(const QString &profile)
{
    if(!validProfile(profile))return {};
    QSettings store(QSettings::defaultFormat(),QSettings::UserScope,"Isolde","Games");
    auto values=store.value(key(profile)).toStringList();
    values.removeDuplicates();
    values.erase(std::remove_if(values.begin(),values.end(),[](const QString &id){return !valid(id);}),values.end());
    values.sort();
    return values;
}

bool OpticalDiscGrants::approved(const QString &profile,const QString &mediaId)
{
    return valid(mediaId) && mediaIds(profile).contains(mediaId);
}

QString OpticalDiscGrants::authorize(const QString &profile,const QString &mediaId)
{
    if(!validProfile(profile) || !valid(mediaId))return "Invalid profile-scoped optical media identity.";
    auto values=mediaIds(profile);
    if(values.contains(mediaId))return {};
    if(values.size()>=128)return "This profile already has the maximum of 128 approved optical identities.";
    values.append(mediaId);
    QSettings store(QSettings::defaultFormat(),QSettings::UserScope,"Isolde","Games");
    store.setValue(key(profile),values);store.sync();
    return store.status()==QSettings::NoError?QString():"Could not save this profile's optical-media approval.";
}

QString OpticalDiscGrants::revoke(const QString &profile,const QString &mediaId)
{
    if(!validProfile(profile) || !valid(mediaId))return "Invalid profile-scoped optical media identity.";
    auto values=mediaIds(profile);
    if(!values.removeOne(mediaId))return "This optical identity is not approved in the active profile.";
    QSettings store(QSettings::defaultFormat(),QSettings::UserScope,"Isolde","Games");
    store.setValue(key(profile),values);store.sync();
    return store.status()==QSettings::NoError?QString():"Could not update this profile's optical-media approval.";
}
