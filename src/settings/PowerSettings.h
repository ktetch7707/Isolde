#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QVariantMap>
#include <functional>

class PowerSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit PowerSettings(QObject *parent=nullptr, const QDBusConnection &bus=QDBusConnection::systemBus(), const QString &service=QStringLiteral("org.freedesktop.login1"));
    bool busy() const { return m_busy; }
    QVariantMap capabilities() const { return m_capabilities; }
    QString status() const { return m_status; }
    void setGuard(std::function<bool()> guard) { m_guard=std::move(guard); }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool request(const QString &action);
signals:
    void changed();
private:
    QDBusConnection m_bus;
    QString m_service, m_status=QStringLiteral("Refresh to check this system's power capabilities.");
    QVariantMap m_capabilities;
    std::function<bool()> m_guard;
    bool m_busy=false;
    quint64 m_generation=0;
    int m_remaining=0;
};
