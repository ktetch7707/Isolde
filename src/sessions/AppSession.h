#pragma once

#include "app/AppDefinition.h"

#include <QObject>
#include <QVariantMap>

enum class AppSessionLifecycleState {
    Created,
    Running,
    Suspended,
    Exited
};

struct AppSessionInfo {
    AppDefinition definition;
    AppSessionLifecycleState state = AppSessionLifecycleState::Created;
};

class AppSessionControl {
  public:
    virtual ~AppSessionControl() = default;

    virtual void launch() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void exit() = 0;

  protected:
    [[nodiscard]] QString errorString() const {
        return m_error;
    }
    void setError(const QString& error) {
        m_error = error;
    }

  private:
    QString m_error;
};

class AppSession : public QObject, public AppSessionControl {
    Q_OBJECT
  public:
    using State = AppSessionLifecycleState;

    explicit AppSession(AppDefinition definition, QObject* parent = nullptr);
    ~AppSession() override = default;

    [[nodiscard]] const AppDefinition& definition() const {
        return m_info.definition;
    }
    [[nodiscard]] AppSessionLifecycleState state() const {
        return m_info.state;
    }
    [[nodiscard]] QString errorString() const {
        return AppSessionControl::errorString();
    }

  signals:
    void stateChanged(AppSessionLifecycleState state);
    void steamProviderStateChanged(const QVariantMap& state);
    void gamepadSessionStatusChanged(const QVariantMap& state);

  protected:
    void setState(AppSessionLifecycleState state) {
        if (m_info.state == state)
            return;
        m_info.state = state;
        emit stateChanged(state);
    }

  private:
    AppSessionInfo m_info;
};
