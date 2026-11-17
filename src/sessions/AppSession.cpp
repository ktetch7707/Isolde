#include "sessions/AppSession.h"

#include <utility>

AppSession::AppSession(AppDefinition definition, QObject* parent)
    : AppSessionControl(parent), m_info{std::move(definition)}
{
}

const AppDefinition&
AppSession::definition() const
{
    return m_info.definition;
}
AppSessionLifecycleState
AppSession::state() const
{
    return m_info.state;
}

void
AppSession::setState(AppSessionLifecycleState state)
{
    if (m_info.state == state)
        return;
    m_info.state = state;
    emit stateChanged(state);
}
