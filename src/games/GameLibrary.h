#pragma once
#include <QDirIterator>
#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QTimer>
#include <QVariantList>
#include <functional>
#include <memory>

class AppRegistry;
class AppCatalog;

// Profile-owned games metadata only. Never scans or modifies provider databases,
// game files or saves. Page actions contain IDs, never commands or file grants.
class GameLibrary final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(int catalogLimit READ catalogLimit CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QVariantList runtimes READ runtimes NOTIFY changed)
    Q_PROPERTY(QVariantList emulators READ emulators NOTIFY changed)
    Q_PROPERTY(QVariantMap emulatorCoverage READ emulatorCoverage CONSTANT)
    Q_PROPERTY(bool setupBusy READ setupBusy NOTIFY setupChanged)
    Q_PROPERTY(QString setupStatus READ setupStatus NOTIFY setupChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY importChanged)
    Q_PROPERTY(int importCount READ importCount NOTIFY importChanged)
    Q_PROPERTY(bool importReady READ importReady NOTIFY importChanged)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY importChanged)
    Q_PROPERTY(bool portableInstallBusy READ portableInstallBusy NOTIFY portableInstallChanged)
    Q_PROPERTY(QString portableInstallStatus READ portableInstallStatus NOTIFY portableInstallChanged)
    Q_PROPERTY(QVariantMap portableInstallPreview READ portableInstallPreview NOTIFY portableInstallChanged)
    Q_PROPERTY(bool windowsInstallBusy READ windowsInstallBusy NOTIFY windowsInstallChanged)
    Q_PROPERTY(QString windowsInstallStatus READ windowsInstallStatus NOTIFY windowsInstallChanged)
    Q_PROPERTY(QVariantMap windowsInstallPreview READ windowsInstallPreview NOTIFY windowsInstallChanged)
  public:
    GameLibrary(const QString& databasePath, AppRegistry* apps, QObject* parent = nullptr);
    ~GameLibrary() override;
    bool
    ready() const
    {
        return m_ready;
    }
    int
    catalogLimit() const;
    QString
    errorString() const
    {
        return m_error;
    }
    QVariantList
    runtimes() const;
    QVariantList
    emulators() const;
    QVariantMap
    emulatorCoverage() const;
    Q_INVOKABLE QVariantMap
    emulatorSettings(const QString& runtimeId);
    Q_INVOKABLE bool
    configureEmulator(const QString& runtimeId, const QVariantMap& options, const QString& firmware = {});
    void
    setCatalog(AppCatalog* catalog)
    {
        m_catalog = catalog;
    }
    void
    setSessionActiveGuard(std::function<bool()> guard)
    {
        m_sessionActiveGuard = std::move(guard);
    }
    bool
    setupBusy() const
    {
        return m_setup.state() != QProcess::NotRunning;
    }
    QString
    setupStatus() const
    {
        return m_setupStatus;
    }
    Q_INVOKABLE bool
    installEmulators(bool consent, bool update = false, bool offline = false);
    bool
    validInstallEmulatorsArgs(bool consent) const;
    QString
    findInstallerRoot() const;
    bool
    validateInstaller(const QString& root) const;
    bool
    prepareEmulatorState();
    bool
    startEmulatorInstaller(const QString& root, bool update, bool offline);
    Q_INVOKABLE bool
    rebuildEmulatorData(const QString& id);
    Q_INVOKABLE void
    cancelEmulatorSetup();
    // Explicit registration is also explicit consent to read this specific root.
    Q_INVOKABLE QString
    addGame(const QVariantMap& registration);
    Q_INVOKABLE QVariantMap
    registration(const QString& gameId);
    Q_INVOKABLE bool
    updateGame(const QString& gameId, const QVariantMap& registration);
    bool
    validUpdateSource(const QString& id) const;
    bool
    validUpdateRegistration(const QVariantMap& value,
                            const QString& runtime,
                            const QString& root,
                            const QString& target) const;
    bool
    editionTargetUnused(const QString& runtime, const QString& root, const QString& target) const;
    Q_INVOKABLE QVariantList
    editions(const QString& gameId);
    Q_INVOKABLE bool
    steamGameContainsAppId(const QString& gameId, const QString& steamAppId);
    Q_INVOKABLE QString
    addEdition(const QString& gameId, const QVariantMap& registration);
    bool
    scanning() const
    {
        return bool(m_scan) || m_steamScan.state() != QProcess::NotRunning;
    }
    QString
    importStatus() const
    {
        return m_importStatus;
    }
    Q_INVOKABLE bool
    beginSteamImport();
    Q_INVOKABLE QStringList
    steamLibraries() const;
    Q_INVOKABLE bool
    addSteamLibrary(const QString& path);
    Q_INVOKABLE bool
    removeSteamLibrary(const QString& path);
    Q_INVOKABLE QVariantList
    mountedOpticalDiscs() const;
    Q_INVOKABLE QVariantList
    refreshOpticalDiscs() const;
    Q_INVOKABLE QVariantMap
    revalidateOpticalDisc(const QString& observationId, const QString& device) const;
    Q_INVOKABLE QString
    registerOpticalGame(const QString& title,
                        const QString& runtimeId,
                        const QString& relativeExecutable,
                        const QString& observationId,
                        const QString& device);
    Q_INVOKABLE bool
    approveOpticalMedia(const QString& observationId, const QString& device);
    Q_INVOKABLE bool
    revokeOpticalMedia(const QString& observationId, const QString& device);
    Q_INVOKABLE bool
    mountOpticalDisc(const QString& observationId, const QString& device);
    Q_INVOKABLE bool
    ejectOpticalDisc(const QString& observationId, const QString& device);
    bool
    portableInstallBusy() const
    {
        return m_portableInstall.state() != QProcess::NotRunning || m_windowsInstall.state() != QProcess::NotRunning;
    }
    QString
    portableInstallStatus() const
    {
        return m_portableInstallStatus;
    }
    QVariantMap
    portableInstallPreview() const
    {
        return m_portableInstallPreview;
    }
    bool
    windowsInstallBusy() const
    {
        return m_windowsInstall.state() != QProcess::NotRunning;
    }
    QString
    windowsInstallStatus() const
    {
        return m_windowsInstallStatus;
    }
    QVariantMap
    windowsInstallPreview() const
    {
        return m_windowsInstallPreview;
    }
    Q_INVOKABLE bool
    beginPortableInstall(const QString& source,
                         const QString& runtimeId,
                         const QString& opticalObservationId = {},
                         const QString& opticalDevice = {});
    bool
    validBeginPortableInstallArgs(const QString& runtimeId) const;
    bool
    validPortableSourceFile(const QFileInfo& file,
                            const QString& opticalObservationId,
                            const QString& opticalDevice) const;
    bool
    validateOpticalSource(const QFileInfo& file,
                          const QString& opticalObservationId,
                          const QString& opticalDevice) const;
    QString
    findInstallerHelper(const QString& scriptName) const;
    void
    initializePortableInstallState(const QFileInfo& file,
                                   const QString& runtimeId,
                                   const QString& opticalObservationId,
                                   const QString& opticalDevice);
    bool
    startPortableInspect(const QString& python, const QString& helper, const QString& source);
    Q_INVOKABLE bool
    beginPortableDirectoryInstall(const QString& source, const QString& entrypoint, const QString& runtimeId);
    bool
    validBeginPortableDirectoryInstallArgs(const QString& runtimeId) const;
    bool
    validPortableSourceDirectory(const QFileInfo& directory) const;
    bool
    validPortableEntryPoint(const QString& entrypoint) const;
    void
    initializePortableDirectoryInstallState(const QFileInfo& directory,
                                            const QString& entrypoint,
                                            const QString& runtimeId);
    bool
    startPortableDirectoryInspect(const QString& python,
                                  const QString& helper,
                                  const QString& source,
                                  const QString& entrypoint);
    Q_INVOKABLE bool
    confirmPortableInstall();
    bool
    validConfirmPortableInstallArgs() const;
    bool
    validatePortableSource() const;
    bool
    validateOpticalMediaForConfirm() const;
    bool
    validateInstallDestination() const;
    bool
    secureInstallDestination() const;
    void
    initializePortableInstallForConfirm();
    QStringList
    buildInstallArguments(const QString& helper) const;
    Q_INVOKABLE void
    cancelPortableInstall();
    int
    importCount() const
    {
        return m_preview.size();
    }
    bool
    importReady() const
    {
        return m_importReady;
    }
    Q_INVOKABLE bool
    beginImport(const QString& root, const QString& runtimeId, const QString& platform);
    Q_INVOKABLE void
    cancelImport();
    Q_INVOKABLE QVariantList
    importPage(int offset = 0) const;
    bool
    validCommitImportArgs() const;
    bool
    validateCommitRuntime(bool& steam) const;
    int
    readGameCount(QSqlQuery& query);
    bool
    processImportEntry(QSqlQuery& query, const QVariant& entry, bool steam, int& count, bool& checkedPlatform);
    bool
    checkExistingGame(QSqlQuery& query, const QVariantMap& value, bool steam);
    bool
    checkPlatformLimit(const QVariantMap& value, bool& checkedPlatform);
    bool
    insertImportedGame(QSqlQuery& query, const QVariantMap& value, bool steam);
    bool
    rollbackImport(const QString& message);
    bool
    finalizeCommitImport();
    Q_INVOKABLE bool
    commitImport();
    Q_INVOKABLE bool
    setFavorite(const QString& gameId, bool favorite);
    Q_INVOKABLE QVariantList
    page(const QString& view = "all", const QString& search = {}, int offset = 0, const QString& platform = {});
    Q_INVOKABLE QVariantList
    platforms();
    bool
    recordCompletedSession(const QString& gameId);
    Q_INVOKABLE bool
    setOrganization(const QString& gameId,
                    const QString& franchise,
                    const QStringList& tags,
                    const QStringList& aliases = {},
                    const QString& summary = {});
    bool
    validOrganizationInput(const QString& franchise,
                           const QStringList& inputTags,
                           const QStringList& inputAliases,
                           const QString& summary) const;
    bool
    normalizeTags(const QStringList& inputTags, QJsonArray& tags) const;
    bool
    normalizeAliases(const QStringList& inputAliases, QJsonArray& aliases) const;
    Q_INVOKABLE bool
    setArtwork(const QString& gameId, const QString& source);
    Q_INVOKABLE bool
    clearArtwork(const QString& gameId);
    Q_INVOKABLE QVariantList
    collections();
    Q_INVOKABLE QString
    createCollection(const QString& name,
                     const QString& kind,
                     const QString& filterType = {},
                     const QString& filterValue = {});
    Q_INVOKABLE bool
    updateCollection(const QString& collectionId,
                     const QString& name,
                     const QString& filterType = {},
                     const QString& filterValue = {});
    Q_INVOKABLE bool
    removeCollection(const QString& collectionId);
    Q_INVOKABLE bool
    setCollectionMember(const QString& collectionId, const QString& gameId, bool member);
    Q_INVOKABLE bool
    moveCollectionMember(const QString& collectionId, const QString& gameId, int direction);
    Q_INVOKABLE bool
    collectionContains(const QString& collectionId, const QString& gameId);
    Q_INVOKABLE QVariantList
    pageCollection(const QString& collectionId, int offset = 0);
    Q_INVOKABLE bool
    exportCatalog(const QString& destination);
    Q_INVOKABLE bool
    restoreCatalog(const QString& source, bool confirmed);
    Q_INVOKABLE QVariantMap
    details(const QString& gameId);
    Q_INVOKABLE bool
    removeGame(const QString& gameId);
    Q_INVOKABLE bool
    canUninstallManagedGame(const QString& gameId) const;
    bool
    validUninstallTarget(const QString& root, const QString& target) const;
    bool
    editionReferencesTarget(const QString& id, const QString& root) const;
    bool
    otherGameUsesRoot(const QString& root, const QString& id) const;
    bool
    validResumeUninstallArgs(bool confirmed) const;
    bool
    validateResumeInstallationId(const QString& installationId) const;
    bool
    validateResumePendingState(const QString& root) const;
    void
    initializeResumeUninstallState(const QString& installationId, const QString& root);
    bool
    startResumeUninstall(const QString& python, const QString& helper, const QString& root);
    Q_INVOKABLE bool
    uninstallManagedGame(const QString& gameId, bool confirmed);
    Q_INVOKABLE QVariantList
    pendingUninstallRecovery();
    Q_INVOKABLE QVariantList
    unregisteredManagedInstallRecovery();
    bool
    recoveryBusy() const;
    QString
    gamesBase() const;
    bool
    validManagedUuid(const QString& identity, const QString& parent) const;
    bool
    validPendingStaging(const QFileInfo& staged, const QString& pending) const;
    bool
    validUnregisteredMarker(const QString& root, const QFileInfo& marker) const;
    bool
    rootReferenced(const QString& root, bool* referenced) const;
    Q_INVOKABLE bool
    recoverUnregisteredManagedInstall(const QString& installationId, const QString& runtimeId, bool confirmed);
    Q_INVOKABLE QVariantMap
    compatibilityInstallerStatus() const;
    Q_INVOKABLE bool
    inspectWindowsInstaller(const QString& source);
    Q_INVOKABLE bool
    beginWindowsInstall(const QString& source);
    Q_INVOKABLE void
    cancelWindowsInstall();
    bool
    validRecoveryArgs(bool confirmed) const;
    bool
    validateRecoveryInstallationId(const QString& installationId) const;
    bool
    validateRecoveryRuntime(const QString& runtimeId) const;
    bool
    validateInstallRoot(const QString& root) const;
    void
    initializeRecoveryState(const QString& installationId, const QString& runtimeId, const QString& root);
    bool
    startRecoveryInspect(const QString& python, const QString& helper, const QString& root);
    Q_INVOKABLE bool
    recoverOrphanedUninstall(const QString& installationId, bool confirmed);
    Q_INVOKABLE bool
    resumeOrphanedUninstall(const QString& installationId, bool confirmed);
    QVariantMap
    resolveLaunch(const QString& gameId, const QString& editionId = {});
    bool
    validLaunchSource(const QString& id) const;
    bool
    resolveLaunchQuery(
        const QString& id, const QString& editionId, QString& runtime, QString& root, QString& relative) const;
    bool
    readyLaunchRuntime(const QString& runtime) const;
    QVariantMap
    resolveSteamLaunch(const QString& relative) const;
    QVariantMap
    resolveStandardLaunch(const QString& root, const QString& relative, const QString& runtime);
    void
    invalidate();
  signals:
    void
    changed();
    void
    errorChanged();
    void
    importChanged();
    void
    setupChanged();
    void
    portableInstallChanged();
    void
    windowsInstallChanged();
    void
    opticalDiscsChanged(const QVariantList& discs, const QString& change);

  private:
    bool
    fail(const QString& error);
    bool
    compatibleRuntime(const QString& id, bool file) const;
    QString
    checkedTarget(const QString& root, const QString& relative);
    bool
    validateOpticalTarget(const QString& root, QString& validatedRoot) const;
    bool
    validateRelativePath(const QString& relative) const;
    QString
    validateGameFile(const QString& validatedRoot, const QString& relative) const;
    QSqlDatabase m_db;
    QString m_connection, m_error;
    QString m_emulatorProfile;
    AppCatalog* m_catalog = nullptr;
    QProcess m_setup;
    QTimer m_setupDeadline;
    QString m_setupStatus, m_setupReceipt;
    QByteArray m_setupOutput;
    bool m_setupCanceled = false;
    AppRegistry* m_apps;
    bool m_ready = false;
    bool m_hasEditions = false;
    bool m_hasActivity = false;
    bool m_hasMetadata = false;
    bool m_hasRichMetadata = false;
    bool m_hasArtwork = false;
    bool m_hasCollections = false;
    bool m_hasProvenance = false;
    bool
    installationExists(const QString& runtime, const QString& root, const QString& target);
    bool
    catalogReferencesRoot(const QString& root, bool* referenced);
    bool
    platformAvailable(const QString& platform);
    QString
    buildSelectClause() const;
    QString
    buildWhereClause(const QString& view, const QString& search, const QString& platform) const;
    QString
    buildOrderByClause(const QString& view) const;
    void
    bindQueryParameters(
        QSqlQuery& query, const QString& view, const QString& search, const QString& platform, int offset) const;
    bool
    validPageView(const QString& view) const;
    bool
    validPageSearch(const QString& search) const;
    bool
    validPageOffset(int offset) const;
    bool
    validPagePlatform(const QString& platform) const;
    bool
    validPlatformView(const QString& view, const QString& platform) const;
    bool
    validFranchiseTagView(const QString& view, const QString& search) const;
    bool
    ensureOrganizationSchema();
    bool
    ensureRichMetadataSchema();
    bool
    ensureArtworkSchema();
    bool
    ensureProvenanceSchema();
    bool
    recordProvenance(const QString& entityId,
                     const QString& entityType,
                     const QString& runtime,
                     const QString& root,
                     const QString& target,
                     const QString& origin = {});
    bool
    validProvenanceIdentity(const QString& entityId, const QString& entityType) const;
    bool
    withinProvenanceBound() const;
    QString
    provenanceExpectedOrigin(const QString& kind) const;
    bool
    validProvenanceOrigin(const QString& recordedOrigin, const QString& kind, const QString& expectedOrigin) const;
    bool
    validRebuildArgs(const QString& id) const;
    bool
    validPreparedRoot(const QString& root) const;
    bool
    checkRetirementLimit(const QString& home) const;
    bool
    retirePreparedData(const QString& home, const QString& root);
    QString
    addGameWithOrigin(const QVariantMap& registration, const QString& origin);
    bool
    validAddGameSource() const;
    bool
    validGameRegistration(const QVariantMap& registration,
                          const QString& runtime,
                          const QString& root,
                          const QString& target) const;
    bool
    checkEditionConflict(const QString& runtime, const QString& root, const QString& target) const;
    bool
    ensureGameProvenanceSchema();
    QString
    insertGameRecord(const QVariantMap& registration,
                     const QString& runtime,
                     const QString& root,
                     const QString& target,
                     const QString& origin);
    bool
    addEditionWithOrigin(const QString& gameId, const QVariantMap& registration, const QString& origin);
    void
    onSteamScanFinished(int code, QProcess::ExitStatus status);
    bool
    validSteamDiscovery(QJsonParseError::ParseError parseError,
                        const QVariantMap& document,
                        const QVariantList& games) const;
    bool
    validSteamTitle(const QVariantMap& game, QSet<QString>& seen) const;
    void
    onSteamScanError(QProcess::ProcessError error);
    void
    onSteamScanOutput();
    void
    onSetupOutput(const QByteArray& output);
    void
    onSetupError();
    void
    onSetupFinished(int code, QProcess::ExitStatus status);
    void
    onPortableInstallOutput();
    void
    onPortableInstallError();
    void
    onPortableInstallErrorOccurred(QProcess::ProcessError error);
    void
    onPortableInstallFinished(int code, QProcess::ExitStatus status);
    void
    onWindowsInstallOutput();
    void
    onWindowsInstallError();
    void
    onWindowsInstallErrorOccurred(QProcess::ProcessError error);
    void
    onWindowsInstallFinished(int code, QProcess::ExitStatus status);
    void
    onOpticalRefreshTimeout();
    bool
    validateCatalogPath();
    bool
    openCatalogDatabase();
    bool
    validateCatalogVersion(QSqlQuery& query);
    bool
    configureCatalogDurability(QSqlQuery& query);
    bool
    initializeCatalogSchema(QSqlQuery& query);
    bool
    validateCatalogSchemas(QSqlQuery& query);
    bool
    validateCatalogProvenance(QSqlQuery& query);
    bool
    checkPendingRecoveries(QSqlQuery& query);
    QString
    artworkDirectory() const;
    bool
    validArtworkSource(const QFileInfo& inputFile) const;
    bool
    validScanState() const;
    void
    processScanBatch();
    bool
    validScanEntry(const QFileInfo& info, const QString& path) const;
    bool
    validScanTitle(const QString& title) const;
    void
    finalizeScan();
    void
    scanStep();
    QTimer m_scanTimer;
    std::unique_ptr<QDirIterator> m_scan;
    QVariantList m_preview;
    QString m_importRoot, m_importRuntime, m_importPlatform;
    QStringList m_importFormats;
    int m_scanEntries = 0;
    bool m_importReady = false;
    QProcess m_steamScan;
    QTimer m_steamScanDeadline;
    QByteArray m_steamScanOutput;
    bool m_steamScanCanceled = false;
    QString m_importStatus;
    QProcess m_portableInstall;
    QTimer m_portableInstallDeadline;
    QTimer m_windowsInstallDeadline;
    QProcess m_windowsInstall;
    QByteArray m_portableInstallOutput, m_portableInstallError;
    QVariantMap m_portableInstallPreview;
    QString m_portableInstallStatus, m_portableSource, m_portableRuntime, m_portableDestination, m_portableGameId;
    QString m_portableOpticalObservationId, m_portableOpticalDevice;
    QString m_windowsInstallStatus;
    QVariantMap m_windowsInstallPreview;
    QString m_portableDirectoryEntrypoint;
    QStringList m_lastOpticalObservationIds;
    QTimer m_opticalRefreshTimer;
    QString m_portablePhase;
    bool m_portableDirectoryCopy = false;
    bool m_portableCanceled = false;
    bool m_catalogExisted = false;
    int m_catalogVersion = 0;
    std::function<bool()> m_sessionActiveGuard;
};
