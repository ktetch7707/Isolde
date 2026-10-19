#pragma once

#include "app/AppDefinition.h"

#include <QObject>

class AppSession : public QObject {
    Q_OBJECT

public:
    enum class State { Created, Running, Suspended, Exited };
    Q_ENUM(State)

    explicit AppSession(AppDefinition definition, QObject *parent = nullptr);
    ~AppSession() override = default;

    [[nodiscard]] const AppDefinition &definition() const;
    [[nodiscard]] State state() const;
    [[nodiscard]] QString errorString() const { return m_error; }

    virtual void launch() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void exit() = 0;

signals:
    void stateChanged(AppSession::State state);

protected:
    void setError(const QString &error) { m_error = error; }
    void setState(State state);

private:
    AppDefinition m_definition;
    QString m_error;
    State m_state = State::Created;
};
