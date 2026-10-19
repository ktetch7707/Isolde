#pragma once

#include "sessions/AppSession.h"

#include <QProcess>
#include <QLockFile>
#include <memory>

class DesktopSession final : public AppSession {
    Q_OBJECT
public:
    using AppSession::AppSession;
    ~DesktopSession() override;
    QString diagnostics() const { return QString::fromUtf8(m_diagnostics); }
    void launch() override;
    void suspend() override;
    void resume() override;
    void exit() override;
private:
    bool applyContainment();
    void stopProcess();
    bool m_stopping = false;
    QByteArray m_diagnostics;
    QProcess m_launcher;
    bool m_containmentMatched = false;
    std::unique_ptr<QLockFile> m_kodiLock;
};
