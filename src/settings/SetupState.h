#pragma once
#include <QObject>
#include <QProcess>

// Device setup is intentionally outside the individual profile directories.
class SetupState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int step READ step NOTIFY changed)
    Q_PROPERTY(bool complete READ complete NOTIFY changed)
    Q_PROPERTY(bool autostartEnabled READ autostartEnabled NOTIFY autostartEnabledChanged)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    explicit SetupState(const QString& path = {}, QObject* parent = nullptr);
    int
    step() const
    {
        return m_step;
    }
    bool
    complete() const
    {
        return m_complete;
    }
    bool
    autostartEnabled() const
    {
        return m_autostartEnabled;
    }
    QString
    error() const
    {
        return m_error;
    }
    Q_INVOKABLE bool
    advance();
    Q_INVOKABLE bool
    back();
    Q_INVOKABLE bool
    restart();
    // Enable or disable starting Isolde automatically at graphical login.
    #ifdef Q_OS_LINUX
    Q_INVOKABLE void
    setAutostartEnabled(bool enabled);
    #endif
  signals:
    void
    changed();
    void
    autostartEnabledChanged();

  private:
    bool
    store(int step, bool complete);
    bool
    writeAutostartFiles(bool enabled);
    QString m_path, m_error;
    int m_step = 0;
    bool m_complete = false;
    bool m_autostartEnabled = false;
    QProcess* m_systemctl = nullptr;
};
