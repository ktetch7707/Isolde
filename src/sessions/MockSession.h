#pragma once

#include "sessions/AppSession.h"

class MockSession final : public AppSession {
    Q_OBJECT

public:
    using AppSession::AppSession;
    void launch() override;
    void suspend() override;
    void resume() override;
    void exit() override;
};
