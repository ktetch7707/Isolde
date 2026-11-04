#pragma once
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QVariantMap>

class ProfileManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY changed)
    Q_PROPERTY(QString activeName READ activeName NOTIFY changed)
    Q_PROPERTY(QString activeId READ activeId NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool editingAllowed READ editingAllowed NOTIFY changed)
    Q_PROPERTY(bool installsAllowed READ installsAllowed NOTIFY changed)
    Q_PROPERTY(QStringList restrictedDestinations READ restrictedDestinations NOTIFY changed)
public:
    explicit ProfileManager(const QString &root = {}, QObject *parent = nullptr);
    QVariantList profiles() const { return m_profiles; }
    QString activeName() const;
    QString activeId() const { return m_active; }
    QString error() const { return m_error; }
    QString layoutPath() const;
    static QString ambientGroup();
    Q_INVOKABLE bool createProfile(const QString &name);
    Q_INVOKABLE bool renameProfile(const QString &name);
    Q_INVOKABLE bool selectProfile(const QString &id);
    bool editingAllowed() const;
    bool installsAllowed() const;
    QStringList restrictedDestinations() const;
    Q_INVOKABLE bool setEditingAllowed(bool allowed);
    Q_INVOKABLE bool setInstallsAllowed(bool allowed);
    Q_INVOKABLE bool setRestrictedDestinations(const QStringList &ids);
    static bool allows(const QString &operation, const QString &destination = {});
signals:
    void changed();
    void switchRequested();
private:
    QVariantMap activePolicy() const;
    bool setPolicy(const QString &key, const QVariant &value);
    void publishPolicy();
    bool save();
    QString m_root, m_active = "default", m_error;
    QVariantList m_profiles;
};
