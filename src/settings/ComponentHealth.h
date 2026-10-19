#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

class ComponentHealth final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList components READ components NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit ComponentHealth(QObject *parent = nullptr);
    ~ComponentHealth() override;
    QVariantList components() const { return m_components; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    QProcess m_process;
    QTimer m_timeout;
    QVariantList m_components;
    bool m_busy = false;
    QString m_status = "Check installed local media engines. No network requests or device capture.";
};
