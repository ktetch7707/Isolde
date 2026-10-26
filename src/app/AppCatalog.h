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

class AppCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QVariantMap storeOptions READ storeOptions NOTIFY entriesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY entriesChanged)
    Q_PROPERTY(QString operationId READ operationId NOTIFY entriesChanged)
    Q_PROPERTY(bool onlineStoreSearch READ onlineStoreSearch NOTIFY onlineStoreSearchChanged)
    Q_PROPERTY(QVariantList providerResults READ providerResults NOTIFY providerResultsChanged)

public:
    explicit AppCatalog(AppRegistry *registry, QObject *parent = nullptr);
    ~AppCatalog() override;
    QVariantMap storeOptions() const { return m_storeOptions; }
    bool busy() const { return !m_operationId.isEmpty(); }
    QString operationId() const { return m_operationId; }
    [[nodiscard]] QVariantList entries() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool onlineStoreSearch() const { return m_onlineStoreSearch; }
    [[nodiscard]] QVariantList providerResults() const { return m_providerResults; }
    Q_INVOKABLE void setStoreProvider(StoreProvider *provider);
    Q_INVOKABLE StoreProvider *storeProvider() const { return m_provider; }
    Q_INVOKABLE void setOnlineStoreSearch(bool enabled);
    Q_INVOKABLE bool load(const QString &catalogPath = {}, const QString &statePath = {}, const QString &installRoot = {});
    Q_INVOKABLE bool install(const QString &id);
    Q_INVOKABLE bool uninstall(const QString &id);
    Q_INVOKABLE bool isInstalled(const QString &id) const;
    Q_INVOKABLE QString packageState(const QString &id) const;
    Q_INVOKABLE bool completeSetup(const QString &id);
    Q_INVOKABLE bool markFailed(const QString &id, const QString &reason);
    Q_INVOKABLE QVariantList allEntries() const;
    Q_INVOKABLE bool downloadAndInstall(const QString &storeId);
    [[nodiscard]] QString downloadProgress() const { return m_downloadProgress; }

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
    bool androidOperation(const Entry &entry, const QString &operation);
    QVariantList mergedEntries() const;
    bool stageProviderArtifact(const QString &storeId, const QVariantMap &result);
    void onDownloadFinished();
    void onDownloadReadyRead();
    void onDownloadError(QNetworkReply::NetworkError code);

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
    StoreProvider *m_provider = nullptr;
    bool m_onlineStoreSearch = false;
    QVariantList m_providerResults;
    QString m_downloadProgress;
    QNetworkAccessManager m_network;
    QNetworkReply *m_downloadReply = nullptr;
    QByteArray m_downloadData;
    QString m_downloadStoreId;
    QByteArray m_downloadExpectedHash;
    QVariantMap m_downloadResult;
};
