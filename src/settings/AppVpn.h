#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>

// Rules are profile-owned. Connections affect the whole appliance.
class AppVpn final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap routes READ routes NOTIFY changed)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString pendingApp READ pendingApp NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
public:
    explicit AppVpn(QString path, QString profile="default", QString helper={}, QObject *parent=nullptr);
    ~AppVpn() override;
    QVariantMap routes() const {return m_routes;}
    QVariantList connections() const {return m_connections;}
    bool busy() const {return m_busy;}
    QString pendingApp() const {return m_app;}
    QString errorString() const {return m_error;}
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setRoute(const QString &app, const QString &uuid);
    Q_INVOKABLE void cancel();
    bool prepare(const QString &app);
signals:
    void changed();
    void ready(const QString &app);
    void failed(const QString &app, const QString &message);
private:
    void start(const QString &action, const QString &target={});
    void finish(int code, QProcess::ExitStatus exit);
    bool load();
    static bool safeApp(const QString &id);
    static bool safeUuid(const QString &id);
    QString m_path,m_profile,m_helper,m_app,m_verifiedApp,m_error;
    QVariantMap m_routes;
    QVariantList m_connections;
    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_output;
    bool m_busy=false,m_cancelled=false,m_invalidRules=false;
};
