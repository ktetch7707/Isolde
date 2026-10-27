#pragma once

#include "app/AppRegistry.h"
#include "app/StoreProvider.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QJsonObject>

class AppCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QVariantMap storeOptions READ storeOptions NOTIFY entriesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY entriesChanged)
    Q_PROPERTY(QString operationId READ operationId NOTIFY entriesChanged)
    Q_PROPERTY(bool onlineStoreSearch READ onlineStoreSearch NOTIFY onlineStoreSearchChanged)
    Q_PROPERTY(QVariantList providerResults READ providerResults NOTIFY providerResultsChanged)
    Q_PROPERTY(QVariantList allEntries READ allEntries NOTIFY entriesChanged)
    Q_PROPERTY(QString downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString storeSourceName READ storeSourceName NOTIFY entriesChanged)
    Q_PROPERTY(QString storeStatus READ storeStatus NOTIFY entriesChanged)
    Q_PROPERTY(bool hasNextStorePage READ hasNextStorePage NOTIFY entriesChanged)
    Q_PROPERTY(bool hasPreviousStorePage READ hasPreviousStorePage NOTIFY entriesChanged)

public:
    explicit AppCatalog(AppRegistry *registry, QObject *parent = nullptr);
    ~AppCatalog() override;
    QVariantMap storeOptions() const { return m_storeOptions; }
    bool busy() const { return !m_operationId.isEmpty() || m_downloadReply; }
    QString operationId() const { return m_operationId; }
    [[nodiscard]] QVariantList entries() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool onlineStoreSearch() const { return m_onlineStoreSearch; }
    [[nodiscard]] QVariantList providerResults() const { return m_providerResults; }
    Q_INVOKABLE void setStoreProvider(StoreProvider *provider);
    Q_INVOKABLE StoreProvider *storeProvider() const { return m_provider; }
    Q_INVOKABLE void setOnlineStoreSearch(bool enabled);
    Q_INVOKABLE void searchStore(const QString &query);
    Q_INVOKABLE void changeStorePage(int direction);
    QString storeStatus() const { return m_onlineStoreSearch && m_provider ? m_provider->status() : QString(); }
    bool hasNextStorePage() const { return m_onlineStoreSearch && m_provider && m_provider->hasNextPage(); }
    bool hasPreviousStorePage() const { return m_onlineStoreSearch && m_provider && m_provider->hasPreviousPage(); }
    Q_INVOKABLE bool load(const QString &catalogPath = {}, const QString &statePath = {}, const QString &installRoot = {});
    Q_INVOKABLE bool install(const QString &id);
    Q_INVOKABLE bool uninstall(const QString &id);
    Q_INVOKABLE bool isInstalled(const QString &id) const;
    Q_INVOKABLE QString packageState(const QString &id) const;
    Q_INVOKABLE bool completeSetup(const QString &id);
    Q_INVOKABLE bool markFailed(const QString &id, const QString &reason);
    Q_INVOKABLE QVariantList allEntries() const;
    Q_INVOKABLE bool downloadAndInstall(const QString &storeId);
    Q_INVOKABLE QString addWebApp(const QString &name,const QString &url,const QString &backend);
    [[nodiscard]] QString downloadProgress() const { return m_downloadProgress; }
    QString storeSourceName() const { return m_provider ? m_provider->providerName() : tr("No online repository connected"); }

signals:
    void entriesChanged();
    void errorStringChanged();
    void onlineStoreSearchChanged();
    void providerResultsChanged();
    void downloadProgressChanged();
    void operationFailed(const QString &message);
    void packageInstalled(const QString &appId);
    void packageUninstalled(const QString &appId);

private:
    friend class IsoldeTests;
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
        bool custom=false;
    };
    const Entry *findEntry(const QString &id) const;
    bool saveState();
    void setError(const QString &message);
    bool androidOperation(const Entry &entry, const QString &operation);
    QVariantList mergedEntries() const;
    bool stageProviderArtifact(const QString &storeId, const QVariantMap &result);
    void onDownloadFinished();
    void onDownloadReadyRead();
    void onDownloadError(QNetworkReply::NetworkError code);
    bool customEntry(const QJsonObject &data,Entry *entry);
    bool installCustom(const QJsonObject &data);

    AppRegistry *m_registry;
    QVector<Entry> m_entries;
    QSet<QString> m_installedIds;
    QHash<QString, QString> m_packageStates;
    QHash<QString, QString> m_failureReasons;
    QString m_statePath;
    QString m_installRoot;
    QString m_errorString;
    QString m_legacyInstallRoot;
    QVariantMap m_storeOptions;
    QProcess m_androidProcess;
    QString m_operationId;
    QString m_operation;
    bool m_committing = false;
    QByteArray m_operationEvents;
    QPointer<StoreProvider> m_provider;
    bool m_onlineStoreSearch = false;
    QString m_storeQuery;
    QVariantList m_providerResults;
    QString m_downloadProgress;
    QNetworkAccessManager m_network;
    QNetworkReply *m_downloadReply = nullptr;
    QByteArray m_downloadData;
    QString m_downloadStoreId;
    QString m_downloadProfile;
    QByteArray m_downloadExpectedHash;
    QVariantMap m_downloadResult;
    QHash<QString,QString> m_customPackages;
};
