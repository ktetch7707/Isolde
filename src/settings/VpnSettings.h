#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

class VpnSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY changed)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY changed)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY changed)
    Q_PROPERTY(QVariantList countries READ countries NOTIFY changed)
    Q_PROPERTY(bool waitingExternal READ waitingExternal NOTIFY changed)
public:
    explicit VpnSettings(QString profile="default", QObject *parent=nullptr);
    ~VpnSettings() override;
    bool busy() const { return m_busy; }
    QString message() const { return m_message; }
    QVariantList providers() const { return m_providers; }
    QVariantList connections() const { return m_connections; }
    QVariantList downloads() const { return m_downloads; }
    QVariantList countries() const { return m_countries; }
    bool waitingExternal() const { return !m_externalAction.isEmpty(); }
    Q_INVOKABLE void perform(const QString &action, const QString &target = {}, const QString &provider = "surfshark", const QString &country = "CA");
    void resumeExternal();
    void cancelExternal();
signals:
    void changed();
    void refreshed();
    void externalOpening();
private:
    QProcess m_process;
    QTimer m_timer;
    bool m_busy=false;
    bool m_stopping=false;
    QString m_message;
    QString m_profile;
    QString m_externalAction,m_externalTarget;
    void launchHelper(const QString &action,const QString &target,const QString &provider,const QString &country);
    QVariantList m_providers, m_connections, m_downloads, m_countries;
};
