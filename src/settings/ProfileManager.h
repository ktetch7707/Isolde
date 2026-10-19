#pragma once
#include <QObject>
#include <QVariantList>

class ProfileManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY changed)
    Q_PROPERTY(QString activeName READ activeName NOTIFY changed)
    Q_PROPERTY(QString activeId READ activeId NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
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
signals:
    void changed();
    void switchRequested();
private:
    bool save();
    QString m_root, m_active = "default", m_error;
    QVariantList m_profiles;
};
