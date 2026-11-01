#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusArgument>
#include <QDBusContext>
#include <QVariantMap>
#include <QTimer>

struct DesktopShortcut {QString id;QVariantMap properties;};
Q_DECLARE_METATYPE(DesktopShortcut)
Q_DECLARE_METATYPE(QList<DesktopShortcut>)
QDBusArgument &operator<<(QDBusArgument &,const DesktopShortcut &);
const QDBusArgument &operator>>(const QDBusArgument &,DesktopShortcut &);

// The desktop is revealed only after the compositor grants a return shortcut.
// The portal owns global input and permission; Isolde changes no OS shortcuts.
class DesktopMode final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool nativeMode READ nativeMode NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QString shortcutDescription READ shortcutDescription NOTIFY changed)
    Q_PROPERTY(QVariantMap presentation READ presentation NOTIFY changed)
public:
    static constexpr const char *ApplicationId="org.isolde.Isolde";
    explicit DesktopMode(QObject *parent=nullptr);
    DesktopMode(QObject *parent,const QDBusConnection &bus);
    ~DesktopMode() override;
    bool active() const{return m_active;}
    bool busy() const{return m_busy;}
    bool nativeMode() const{return m_nativeMode;}
    QString errorString() const{return m_error;}
    QString shortcutDescription() const{return m_shortcut;}
    QVariantMap presentation() const{return m_presentation;}
    void setPresentation(const QVariantMap &value){m_presentation=value;emit changed();}
    Q_INVOKABLE void start(const QString &origin={});
    void prepareNative(const QString &origin);
    Q_INVOKABLE void returnHome();
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void returned(const QString &origin);
private slots:
    void response(uint code,const QVariantMap &results);
    void activated(const QDBusObjectPath &session,const QString &id,qulonglong timestamp,const QVariantMap &options);
    void sessionClosed();
private:
    void request(const QString &method,const QVariantList &arguments,const QString &token);
    void beginSession();
    void begin(const QString &origin,bool native);
    void closePortal();
    void fail(const QString &message);
    QString requestPath(const QString &token) const;
    QDBusConnection m_bus;
    QTimer m_deadline;
    QVariantMap m_presentation;
    QString m_origin,m_error,m_shortcut,m_request,m_session,m_sessionToken,m_connectionName;
    enum Phase {Idle,Registering,Creating,Binding} m_phase=Idle;
    bool m_active=false,m_busy=false,m_registered=false,m_registering=false,m_nativeMode=false;
    quint64 m_generation=0,m_identityGeneration=0;
};
