#include "sessions/AppSession.h"

#include <utility>

// Attach the session to its QObject owner while retaining its launch definition.
AppSession::AppSession(AppDefinition definition, QObject* parent)
    : QObject(parent), m_info{std::move(definition)}
{
}

// Expose the launch definition to session management without copying it.
const AppDefinition&
AppSession::definition() const
{
    return m_info.definition;
}
// Report lifecycle state to callers coordinating focus and playback.
AppSessionLifecycleState
AppSession::state() const
{
    return m_info.state;
}

// Notify observers only when the lifecycle actually changes.
void
AppSession::setState(AppSessionLifecycleState state)
{
    if (m_info.state == state)
        return;
    m_info.state = state;
    emit stateChanged(state);
}
