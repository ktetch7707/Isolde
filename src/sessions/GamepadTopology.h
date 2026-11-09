#pragma once

#include <QSet>
#include <QVariantMap>

namespace GamepadTopology {

inline QVariantMap summarize(const QSet<QString> &connected, const QSet<QString> &availableAtLaunch)
{
    int available = 0;
    for (const auto &node : connected)
        if (availableAtLaunch.contains(node))
            ++available;
    return {
        {"connected", connected.size()},
        {"available", available},
        {"new", connected.size() - available},
        {"missing", availableAtLaunch.size() - available}
    };
}

}
