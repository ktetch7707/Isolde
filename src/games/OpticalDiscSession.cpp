#include "OpticalDiscIdentity.h"
#include "OpticalDiscSession.h"

#include <QStorageInfo>

QVariantMap
OpticalDiscSession::latchSession(const QVariantMap& previous, QVariantMap current)
{
    const auto previousState = previous.value("state").toString();
    if (current.value("state") == "pinned" && !previousState.isEmpty() && previousState != "pinned")
    {
        current["state"] = "interrupted";
        current["recovery"] =
            "The original session mount cannot be restored by reinsertion. Return Home and retry "
            "Play with the expected disc.";
    }
    return current;
}

namespace
{
struct SwapSession
{
    int expectedDiscNumber = 0;
    QString expectedMediaSetId;
    QString state; // "awaiting-swap", "swapped", "completed"
};
QMap<QString, SwapSession>&
swapSessions()
{
    static QMap<QString, SwapSession> sessions;
    return sessions;
}
} // namespace

QVariantMap
OpticalDiscSwap::requestDiscSwap(const QString& sessionId, int expectedDiscNumber, const QString& expectedMediaSetId)
{
    if (sessionId.isEmpty() || expectedDiscNumber < 1 || expectedMediaSetId.isEmpty())
        return {{"state", "invalid"}};
    auto& sessions = swapSessions();
    sessions.insert(sessionId, {expectedDiscNumber, expectedMediaSetId, "awaiting-swap"});
    return {{"schemaVersion", 1},
            {"state", "awaiting-swap"},
            {"sessionId", sessionId},
            {"expectedDisc", expectedDiscNumber},
            {"discCount", 0}};
}

QVariantMap
OpticalDiscSwap::completeDiscSwap(const QString& sessionId, const QString& observationId, const QString& device)
{
    auto& sessions = swapSessions();
    auto it = sessions.find(sessionId);
    if (it == sessions.end())
        return {{"state", "invalid"}};
    const auto& session = it.value();
    if (session.state != "awaiting-swap")
        return {{"state", "invalid"}};
    const auto current = OpticalDiscHash::mountedDiscs();
    for (const auto& item : current)
    {
        const auto disc = item.toMap();
        if (disc.value("state") != "mounted")
            continue;
        if (disc.value("observationId") == observationId && disc.value("device") == device)
        {
            const auto mediaSetId = disc.value("mediaSetId").toString();
            const auto discNumber = disc.value("discNumber").toInt();
            if (mediaSetId == session.expectedMediaSetId && discNumber == session.expectedDiscNumber)
            {
                SwapSession updated = session;
                updated.state = "swapped";
                sessions.insert(sessionId, updated);
                return {{"schemaVersion", 1}, {"state", "swapped"}, {"sessionId", sessionId}, {"disc", disc}};
            }
            return {{"schemaVersion", 1},
                    {"state", "wrong-disc"},
                    {"sessionId", sessionId},
                    {"expectedDisc", session.expectedDiscNumber},
                    {"gotDisc", discNumber}};
        }
    }
    return {{"schemaVersion", 1},
            {"state", "missing"},
            {"sessionId", sessionId},
            {"expectedDisc", session.expectedDiscNumber}};
}

QVariantMap
OpticalDiscSwap::swapSessionState(const QString& sessionId)
{
    auto& sessions = swapSessions();
    auto it = sessions.find(sessionId);
    if (it == sessions.end())
        return {{"state", "invalid"}};
    return {{"schemaVersion", 1}, {"state", it.value().state}, {"sessionId", sessionId}};
}

namespace
{
struct OpticalSession
{
    QString mediaId;
    QString device;
    QString state; // "active"
};
QMap<QString, OpticalSession>&
opticalSessions()
{
    static QMap<QString, OpticalSession> sessions;
    return sessions;
}
} // namespace

QString
OpticalDiscSession::registerOpticalSession(const QString& sessionId, const QString& mediaId, const QString& device)
{
    if (sessionId.isEmpty() || mediaId.isEmpty() || device.isEmpty())
        return {};
    auto& sessions = opticalSessions();
    sessions.insert(sessionId, {mediaId, device, "active"});
    return sessionId;
}

void
OpticalDiscSession::releaseOpticalSession(const QString& sessionId)
{
    auto& sessions = opticalSessions();
    sessions.remove(sessionId);
}

QVariantMap
OpticalDiscSession::opticalSessionArbitration(const QString& device)
{
    if (device.isEmpty())
        return {{"state", "invalid"}};
    QVariantList blocking;
    for (const auto& key : opticalSessions().keys())
    {
        const auto& session = opticalSessions().value(key);
        if (session.device == device)
        {
            blocking.append(QVariantMap{{"sessionId", key}, {"mediaId", session.mediaId}, {"state", session.state}});
        }
    }
    if (blocking.isEmpty())
        return {{"schemaVersion", 1}, {"state", "clear"}, {"device", device}};
    return {{"schemaVersion", 1}, {"state", "blocked"}, {"device", device}, {"blockingSessions", blocking}};
}
