#pragma once

#include "app/AppRegistry.h"
#include "app/StoreProvider.h"

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QVariantList>
#include <optional>

class AppCatalogEntry;

class AppCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QVariantMap storeOptions READ storeOptions NOTIFY entriesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY entriesChanged)
    Q_PROPERTY(QString operationId READ operationId NOTIFY entriesChanged)
    Q_PROPERTY(bool onlineStoreSearch READ onlineStoreSearch NOTIFY onlineStoreSearchChanged)
    Q_PROPERTY(QVariantList providerResults READ providerResults NOTIFY providerResultsChanged)
    Q_PROPERTY(QVariantList allEntries READ allEntries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList galleryEntries READ galleryEntries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantMap galleryPresentation READ galleryPresentation CONSTANT)
    Q_PROPERTY(QString downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString storeSourceName READ storeSourceName NOTIFY entriesChanged)
    Q_PROPERTY(QString storeStatus READ storeStatus NOTIFY entriesChanged)
    Q_PROPERTY(bool hasNextStorePage READ hasNextStorePage NOTIFY entriesChanged)
    Q_PROPERTY(bool hasPreviousStorePage READ hasPreviousStorePage NOTIFY entriesChanged)
    Q_PROPERTY(bool canCancelOperation READ canCancelOperation NOTIFY entriesChanged)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY entriesChanged)
    Q_PROPERTY(QString operationName READ operationName NOTIFY entriesChanged)

  public:
    Q_PROPERTY(QString androidContainment READ androidContainment NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList browserApps READ browserApps NOTIFY entriesChanged)
    Q_PROPERTY(QString defaultBrowser READ defaultBrowser NOTIFY entriesChanged)
    QVariantList browserApps() const;
    QString defaultBrowser() const;
    Q_INVOKABLE bool setDefaultBrowser(const QString& id);
    QString
    androidContainment() const
    {
        return m_androidContainment;
    }
    Q_INVOKABLE bool
    setAndroidContainment(const QString& mode);
    explicit AppCatalog(AppRegistry* registry, QObject* parent = nullptr);
    ~AppCatalog() override;
    QVariantMap
    storeOptions() const
    {
        return m_storeOptions;
    }
    bool
    busy() const
    {
        return !m_operationId.isEmpty() || m_downloadReply;
    }
    QString
    operationId() const
    {
        return m_operationId;
    }
    [[nodiscard]] QVariantList
    entries() const;
    [[nodiscard]] QString
    errorString() const;
    [[nodiscard]] bool
    onlineStoreSearch() const
    {
        return m_onlineStoreSearch;
    }
    [[nodiscard]] QVariantList
    providerResults() const
    {
        return m_providerResults;
    }
    Q_INVOKABLE void
    setStoreProvider(StoreProvider* provider);
    bool
    isValidStoreProvider(StoreProvider* provider) const;
    void
    onStoreResultsChanged();
    void
    onStoreUpdatesChanged();
    void
    processStoreUpdateResult(const QVariant& value);
    void
    onStoreProviderError(const QString& error);
    Q_INVOKABLE StoreProvider*
    storeProvider() const
    {
        return m_provider;
    }
    Q_INVOKABLE void
    setOnlineStoreSearch(bool enabled);
    Q_INVOKABLE void
    searchStore(const QString& query);
    Q_INVOKABLE void
    browseOnlineStore(const QString& query);
    Q_INVOKABLE void
    changeStorePage(int direction);
    Q_INVOKABLE bool
    checkForUpdates();
    Q_INVOKABLE bool
    setUpdatesEnabled(const QString& id, bool enabled);
    Q_INVOKABLE bool
    updateApp(const QString& id);
    Q_INVOKABLE void
    cancelOperation();
    Q_INVOKABLE bool
    setupAndroid(bool softwareRendering = false);
    bool
    canCancelOperation() const
    {
        return busy() && m_cancellable && !m_cancelRequested;
    }
    QString
    operationStatus() const
    {
        return m_operationStatus;
    }
    QString
    operationName() const
    {
        return m_operationName;
    }
    QString
    storeStatus() const
    {
        const auto* provider = m_provider.data();
        return m_onlineStoreSearch && provider ? provider->status() : QString();
    }
    bool
    hasNextStorePage() const
    {
        const auto* provider = m_provider.data();
        return m_onlineStoreSearch && provider && provider->hasNextPage();
    }
    bool
    hasPreviousStorePage() const
    {
        const auto* provider = m_provider.data();
        return m_onlineStoreSearch && provider && provider->hasPreviousPage();
    }
    Q_INVOKABLE bool
    load(const QString& catalogPath = {}, const QString& statePath = {}, const QString& installRoot = {});
    Q_INVOKABLE bool
    install(const QString& id);
    Q_INVOKABLE bool
    uninstall(const QString& id);
    Q_INVOKABLE bool
    isInstalled(const QString& id) const;
    Q_INVOKABLE QString
    packageState(const QString& id) const;
    Q_INVOKABLE bool
    completeSetup(const QString& id);
    Q_INVOKABLE bool
    markFailed(const QString& id, const QString& reason);
    Q_INVOKABLE QVariantList
    allEntries() const;
    Q_INVOKABLE QVariantList
    galleryEntries() const;
    QVariantMap
    galleryPresentation() const
    {
        return m_galleryPresentation;
    }
    Q_INVOKABLE bool
    downloadAndInstall(const QString& storeId);
    Q_INVOKABLE QString
    addWebApp(const QString& name, const QString& url, const QString& backend);
    [[nodiscard]] QString
    downloadProgress() const
    {
        return m_downloadProgress;
    }
    QString
    storeSourceName() const
    {
        const auto* provider = m_provider.data();
        return provider ? provider->providerName() : tr("No online repository connected");
    }

  signals:
    void
    entriesChanged();
    void
    errorStringChanged();
    void
    onlineStoreSearchChanged();
    void
    providerResultsChanged();
    void
    downloadProgressChanged();
    void
    operationFailed(const QString& message);
    void
    packageInstalled(const QString& appId);
    void
    packageUninstalled(const QString& appId);

  private:
    friend class IsoldeTests;
    struct EntryCore
    {
        AppDefinition app;
        QString packageId;
        QString category;
        QString version;
        QString minIsoldeVersion;
    };
    struct EntryMetadata
    {
        QString description;
        QString publisher;
        QString provenance;
        QStringList capabilities;
    };
    struct EntrySetup
    {
        bool setupRequired = false;
        QString setupPrompt;
        QByteArray manifestData;
        bool custom = false;
    };
    struct Entry
    {
        EntryCore core;
        EntryMetadata meta;
        EntrySetup setup;
    };
    bool
    isInstallableEntry(const Entry* entry, const QString& id);
    // Keep online searches on the provider path shared by browse and search.
    void
    beginOnlineSearch(const QString& query);
    bool
    stagePackageDirectory(const Entry* entry, const QString& id, bool& createdPackage);
    bool
    registerAndSaveInstall(const Entry* entry, const QString& id, bool createdPackage);
    [[nodiscard]] QString
    resolveCatalogPath(const QString& catalogPath) const;
    [[nodiscard]] bool
    resolveStateAndInstallPaths(const QString& statePath, const QString& installRoot, const QString& profile);
    [[nodiscard]] bool
    loadStoreOptions(const QJsonObject& options);
    [[nodiscard]] bool
    validateStoreOptionKeys(const QJsonObject& options);
    [[nodiscard]] bool
    validateStoreSizes(const QJsonObject& options);
    [[nodiscard]] bool
    validateStoreDetailRatio(const QJsonObject& options);
    [[nodiscard]] bool
    validateStorePlatforms(const QJsonObject& options);
    [[nodiscard]] bool
    validateStorePlatformOptions(const QJsonArray& platforms, const QJsonObject& options);
    [[nodiscard]] bool
    validateStorePlatformItem(const QJsonObject& p, QSet<QString>& types);
    [[nodiscard]] bool
    validateStoreWebOptions(const QJsonObject& options);
    [[nodiscard]] bool
    validateStoreAppOptions(const QJsonObject& options);
    [[nodiscard]] bool
    loadPackageEntries(const QJsonDocument& document, const QDir& catalogDirectory, QVector<Entry>& loaded);
    [[nodiscard]] bool
    loadPackageEntry(const QJsonValue& value, const QDir& catalogDirectory, Entry& entry);
    [[nodiscard]] bool
    loadStateFile(const QString& stateReadPath);
    [[nodiscard]] bool
    loadStateCustomPackages(const QJsonObject& stateObject);
    [[nodiscard]] bool
    loadStateInstalledApps(const QJsonObject& stateObject);
    bool
    loadStateStoreTransaction(const QJsonObject& stateObject);
    bool
    loadStateUpdatePreferences(const QJsonObject& stateObject);
    bool
    loadStateAndroidContainment(const QJsonObject& stateObject);
    bool
    isValidAndroidContainment(const QVariant& presentation);
    [[nodiscard]] bool
    parseFdroidEntry(const QJsonObject& data, Entry* entry);
    [[nodiscard]] bool
    parseWebEntry(const QJsonObject& data, Entry* entry);
    bool
    validateWebEntryFields(const QJsonObject& data);
    bool
    validateWebEntryValues(const QJsonObject& data, const QString& name, const QUrl& url, const QString& backend);
    bool
    addressSizeValid(const QUrl& url);
    bool
    validateWebEntryId(const QJsonObject& data, const QString& id);
    QUrl
    webEntryOrigin(const QUrl& url);
    void
    configureWebEntryApp(Entry* entry,
                         const QString& id,
                         const QString& name,
                         const QString& backend,
                         const QString& canonical,
                         const QUrl& origin,
                         const QJsonObject& storeOptions);
    bool
    configureWebEntryChromium(Entry* entry, const QJsonObject& storeOptions);
    bool
    validateWebEntryAppearance(Entry* entry, const QJsonObject& storeOptions);
    void
    finalizeWebEntry(Entry* entry,
                     const QString& id,
                     const QString& canonical,
                     const QString& publisher,
                     const QJsonObject& storeOptions,
                     const QJsonObject& data);
    [[nodiscard]] bool
    validateProviderStorage(const QString& installRoot, const QString& id, const QString& version);
    [[nodiscard]] bool
    writeProviderPackage(const QString& staging,
                         const QString& directory,
                         const QByteArray& downloadData,
                         const QByteArray& manifestData);
    [[nodiscard]] bool
    canUpdateOrAddEntry(const Entry* previous, bool updating, quint64 newVersion);
    [[nodiscard]] bool
    verifyExistingPackage(const QString& directory, const QByteArray& manifestData);
    [[nodiscard]] bool
    resolveProviderInstall(const QString& id, QString& localId);
    const Entry*
    findEntry(const QString& id) const;
    bool
    saveState();
    void
    abortDownloadOrProcess();
    void
    cleanStagingDirectories();
    QString
    resolveFailureReason(const QString& reason);
    void
    setError(const QString& message);
    bool
    androidOperation(const Entry& entry, const QString& operation);
    void
    configureAndroidProcess(const Entry& entry, const QString& helper, const QString& profile);
    void
    connectAndroidProcessSignals();
    QString
    findAndroidHelper() const;
    QString
    resolveAndroidProfile() const;
    void
    initializeAndroidOperationState(const Entry& entry, const QString& operation, const QString& profile);
    QVariantList
    mergedEntries() const;
    bool
    stageProviderArtifact(const QString& storeId, const QVariantMap& result);
    void
    onDownloadFinished();
    void
    onDownloadReadyRead();
    void
    onDownloadError(QNetworkReply::NetworkError code);
    bool
    customEntry(const QJsonObject& data, Entry* entry);
    bool
    installCustom(const QJsonObject& data);
    bool
    beginDownload(const QVariantMap& result, bool update);
    bool
    validBeginDownloadArgs() const;
    bool
    validateArtifactUrl(const QUrl& artifactUrl) const;
    void
    setupDownloadState(const QVariantMap& result, const QString& storeId, bool update);
    bool
    validateDownloadResult(const QVariantMap& result);
    void
    initializeDownloadOperation(const QVariantMap& result, bool update);
    bool
    startDownload(const QUrl& artifactUrl);
    bool
    commitUpdate(const Entry& entry);
    void
    androidEvent(const QJsonObject& event);

    AppRegistry* m_registry;
    QVector<Entry> m_entries;
    QSet<QString> m_installedIds;
    QHash<QString, QString> m_packageStates;
    QHash<QString, QString> m_failureReasons;
    QString m_statePath;
    QString m_androidContainment;
    QString m_defaultBrowser;
    QString m_installRoot;
    QString m_errorString;
    QString m_legacyInstallRoot;
    QVariantMap m_storeOptions;
    QVariantMap m_galleryPresentation;
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
    QNetworkReply* m_downloadReply = nullptr;
    QByteArray m_downloadData;
    QString m_downloadStoreId;
    QString m_downloadProfile;
    QByteArray m_downloadExpectedHash;
    QVariantMap m_downloadResult;
    QHash<QString, QString> m_customPackages;
    QSet<QString> m_updatesDisabled;
    QHash<QString, QVariantMap> m_availableUpdates;
    std::optional<Entry> m_pendingUpdate;
    QString m_storeTransaction;
    QString m_operationStatus;
    QString m_operationName;
    QString m_operationProfile;
    bool m_cancelRequested = false;
    bool m_cancellable = true;
    bool m_activationCommitted = false;
};
