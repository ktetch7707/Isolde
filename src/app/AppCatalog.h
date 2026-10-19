#pragma once

#include "app/AppRegistry.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QVariantList>

class AppCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit AppCatalog(AppRegistry *registry, QObject *parent = nullptr);
    [[nodiscard]] QVariantList entries() const;
    [[nodiscard]] QString errorString() const;
    Q_INVOKABLE bool load(const QString &catalogPath = {}, const QString &statePath = {}, const QString &installRoot = {});
    Q_INVOKABLE bool install(const QString &id);
    Q_INVOKABLE bool uninstall(const QString &id);
    Q_INVOKABLE bool isInstalled(const QString &id) const;
    Q_INVOKABLE QString packageState(const QString &id) const;
    Q_INVOKABLE bool completeSetup(const QString &id);
    Q_INVOKABLE bool markFailed(const QString &id, const QString &reason);

signals:
    void entriesChanged();
    void errorStringChanged();
    void operationFailed(const QString &message);
    void packageInstalled(const QString &appId);
    void packageUninstalled(const QString &appId);

private:
    struct Entry {
        AppDefinition app;
        QString packageId;
        QString category;
        QString version;
        QString minIsoldeVersion;
        QString description;
        QString publisher;
        QString provenance;
        bool setupRequired = false;
        QString setupPrompt;
        QStringList capabilities;
        QByteArray manifestData;
    };
    const Entry *findEntry(const QString &id) const;
    bool saveState();
    void setError(const QString &message);

    AppRegistry *m_registry;
    QVector<Entry> m_entries;
    QSet<QString> m_installedIds;
    QHash<QString, QString> m_packageStates;
    QHash<QString, QString> m_failureReasons;
    QString m_statePath;
    QString m_installRoot;
    QString m_errorString;
};
