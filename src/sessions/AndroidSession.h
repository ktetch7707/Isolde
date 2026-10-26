#pragma once
#include "sessions/AppSession.h"
#include <QProcess>

class AndroidSession final : public AppSession {
    Q_OBJECT
public:
    using AppSession::AppSession;
    ~AndroidSession() override;
    void launch() override;
    void suspend() override;
    void resume() override;
    void exit() override;
private:
    void stop();
    QProcess m_process;
    QByteArray m_events;
    bool m_stopping = false;
};
