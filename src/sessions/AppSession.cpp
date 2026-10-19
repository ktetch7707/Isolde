#include "sessions/AppSession.h"

#include <utility>

AppSession::AppSession(AppDefinition definition, QObject *parent)
    : QObject(parent), m_definition(std::move(definition)) {}

const AppDefinition &AppSession::definition() const { return m_definition; }
AppSession::State AppSession::state() const { return m_state; }

void AppSession::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}
