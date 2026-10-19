#include "sessions/MockSession.h"

void MockSession::launch() { setState(State::Running); }
void MockSession::suspend() { if (state() == State::Running) setState(State::Suspended); }
void MockSession::resume() { if (state() == State::Suspended) setState(State::Running); }
void MockSession::exit() { if (state() != State::Exited) setState(State::Exited); }
