#pragma once

#include <QSet>
#include <QRegularExpression>
#include <QVariantMap>

namespace GamepadTopology {

inline bool deviceNumber(const QString &name, const QString &text, uint *major, uint *minor)
{
    const auto match=QRegularExpression("^(event|js)([0-9]{1,3})$").match(name);
    if(!match.hasMatch())return false;
    bool indexOk=false,majorOk=false,minorOk=false;
    const auto index=match.captured(2).toUInt(&indexOk);
    const auto parsedMajor=text.section(':',0,0).toUInt(&majorOk);
    const auto parsedMinor=text.section(':',1,1).toUInt(&minorOk);
    if(!indexOk || !majorOk || !minorOk || text.count(':')!=1 || parsedMajor!=13)return false;
    const auto expectedMinor=match.captured(1)=="event"?64+index:index;
    if(expectedMinor>255 || parsedMinor!=expectedMinor)return false;
    if(major)*major=parsedMajor;
    if(minor)*minor=parsedMinor;
    return true;
}

inline QVariantMap summarize(const QSet<QString> &connected, const QSet<QString> &availableAtLaunch,
                             const QSet<QString> &availableAfterLaunch = {})
{
    const auto availableNodes=availableAtLaunch.united(availableAfterLaunch);
    int available = 0;
    for (const auto &node : connected)
        if (availableNodes.contains(node))
            ++available;
    int newlyConnected = 0, missing = 0;
    for (const auto &node : connected)
        if (!availableAtLaunch.contains(node))
            ++newlyConnected;
    for (const auto &node : availableAtLaunch)
        if (!connected.contains(node))
            ++missing;
    return {
        {"connected", connected.size()},
        {"available", available},
        {"new", newlyConnected},
        {"missing", missing}
    };
}

}
