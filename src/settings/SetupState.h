#pragma once
#include <QObject>

// Device setup is intentionally outside the individual profile directories.
class SetupState final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int step READ step NOTIFY changed)
    Q_PROPERTY(bool complete READ complete NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit SetupState(const QString &path = {}, QObject *parent = nullptr);
    int step() const { return m_step; }
    bool complete() const { return m_complete; }
    QString error() const { return m_error; }
    Q_INVOKABLE bool advance();
    Q_INVOKABLE bool back();
    Q_INVOKABLE bool restart();
signals:
    void changed();
private:
    bool store(int step, bool complete);
    QString m_path, m_error;
    int m_step = 0;
    bool m_complete = false;
};
