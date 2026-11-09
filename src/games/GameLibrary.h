#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QDirIterator>
#include <QTimer>
#include <QProcess>
#include <memory>

class AppRegistry;
class AppCatalog;

// Profile-owned games metadata only. Never scans or modifies provider databases,
// game files or saves. Page actions contain IDs, never commands or file grants.
class GameLibrary final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QVariantList runtimes READ runtimes NOTIFY changed)
    Q_PROPERTY(QVariantList emulators READ emulators NOTIFY changed)
    Q_PROPERTY(bool setupBusy READ setupBusy NOTIFY setupChanged)
    Q_PROPERTY(QString setupStatus READ setupStatus NOTIFY setupChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY importChanged)
    Q_PROPERTY(int importCount READ importCount NOTIFY importChanged)
    Q_PROPERTY(bool importReady READ importReady NOTIFY importChanged)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY importChanged)
    Q_PROPERTY(bool portableInstallBusy READ portableInstallBusy NOTIFY portableInstallChanged)
    Q_PROPERTY(QString portableInstallStatus READ portableInstallStatus NOTIFY portableInstallChanged)
    Q_PROPERTY(QVariantMap portableInstallPreview READ portableInstallPreview NOTIFY portableInstallChanged)
public:
    GameLibrary(const QString &databasePath, AppRegistry *apps, QObject *parent = nullptr);
    ~GameLibrary() override;
    bool ready() const { return m_ready; }
    QString errorString() const { return m_error; }
    QVariantList runtimes() const;
    QVariantList emulators() const;
    Q_INVOKABLE QVariantMap emulatorSettings(const QString &runtimeId);
    Q_INVOKABLE bool configureEmulator(const QString &runtimeId, const QVariantMap &options, const QString &firmware = {});
    void setCatalog(AppCatalog *catalog) { m_catalog=catalog; }
    bool setupBusy() const { return m_setup.state()!=QProcess::NotRunning; }
    QString setupStatus() const { return m_setupStatus; }
    Q_INVOKABLE bool installEmulators(bool consent);
    Q_INVOKABLE void cancelEmulatorSetup();
    // Explicit registration is also explicit consent to read this specific root.
    Q_INVOKABLE QString addGame(const QVariantMap &registration);
    Q_INVOKABLE QVariantMap registration(const QString &gameId);
    Q_INVOKABLE bool updateGame(const QString &gameId, const QVariantMap &registration);
    Q_INVOKABLE QVariantList editions(const QString &gameId);
    Q_INVOKABLE QString addEdition(const QString &gameId, const QVariantMap &registration);
    bool scanning() const { return bool(m_scan) || m_steamScan.state()!=QProcess::NotRunning; }
    QString importStatus() const { return m_importStatus; }
    Q_INVOKABLE bool beginSteamImport();
    Q_INVOKABLE QStringList steamLibraries() const;
    Q_INVOKABLE bool addSteamLibrary(const QString &path);
    Q_INVOKABLE bool removeSteamLibrary(const QString &path);
    Q_INVOKABLE QVariantList mountedOpticalDiscs() const;
    Q_INVOKABLE QVariantMap revalidateOpticalDisc(const QString &observationId, const QString &device) const;
    bool portableInstallBusy() const { return m_portableInstall.state()!=QProcess::NotRunning; }
    QString portableInstallStatus() const { return m_portableInstallStatus; }
    QVariantMap portableInstallPreview() const { return m_portableInstallPreview; }
    Q_INVOKABLE bool beginPortableInstall(const QString &source, const QString &runtimeId,
                                          const QString &opticalObservationId = {}, const QString &opticalDevice = {});
    Q_INVOKABLE bool confirmPortableInstall();
    Q_INVOKABLE void cancelPortableInstall();
    int importCount() const { return m_preview.size(); }
    bool importReady() const { return m_importReady; }
    Q_INVOKABLE bool beginImport(const QString &root, const QString &runtimeId, const QString &platform);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE QVariantList importPage(int offset = 0) const;
    Q_INVOKABLE bool commitImport();
    Q_INVOKABLE bool setFavorite(const QString &gameId, bool favorite);
    Q_INVOKABLE QVariantList page(const QString &view = "all", const QString &search = {}, int offset = 0,
                                  const QString &platform = {});
    Q_INVOKABLE QVariantList platforms();
    bool recordCompletedSession(const QString &gameId);
    Q_INVOKABLE bool setOrganization(const QString &gameId, const QString &franchise, const QStringList &tags,
                                     const QStringList &aliases = {}, const QString &summary = {});
    Q_INVOKABLE bool setArtwork(const QString &gameId, const QString &source);
    Q_INVOKABLE bool clearArtwork(const QString &gameId);
    Q_INVOKABLE QVariantList collections();
    Q_INVOKABLE QString createCollection(const QString &name, const QString &kind,
                                         const QString &filterType = {}, const QString &filterValue = {});
    Q_INVOKABLE bool updateCollection(const QString &collectionId, const QString &name,
                                      const QString &filterType = {}, const QString &filterValue = {});
    Q_INVOKABLE bool removeCollection(const QString &collectionId);
    Q_INVOKABLE bool setCollectionMember(const QString &collectionId, const QString &gameId, bool member);
    Q_INVOKABLE bool moveCollectionMember(const QString &collectionId, const QString &gameId, int direction);
    Q_INVOKABLE bool collectionContains(const QString &collectionId, const QString &gameId);
    Q_INVOKABLE QVariantList pageCollection(const QString &collectionId, int offset = 0);
    Q_INVOKABLE bool exportCatalog(const QString &destination);
    Q_INVOKABLE bool restoreCatalog(const QString &source, bool confirmed);
    Q_INVOKABLE QVariantMap details(const QString &gameId);
    Q_INVOKABLE bool removeGame(const QString &gameId);
    QVariantMap resolveLaunch(const QString &gameId, const QString &editionId = {});
    void invalidate();
signals:
    void changed();
    void errorChanged();
    void importChanged();
    void setupChanged();
    void portableInstallChanged();
    void opticalDiscsChanged(const QVariantList &discs, const QString &change);
private:
    bool fail(const QString &error);
    bool compatibleRuntime(const QString &id, bool file) const;
    QString checkedTarget(const QString &root, const QString &relative);
    QSqlDatabase m_db;
    QString m_connection, m_error;
    QString m_emulatorProfile;
    AppCatalog *m_catalog=nullptr;
    QProcess m_setup;
    QTimer m_setupDeadline;
    QString m_setupStatus, m_setupReceipt;
    QByteArray m_setupOutput;
    bool m_setupCanceled=false;
    AppRegistry *m_apps;
    bool m_ready = false;
    bool m_hasEditions = false;
    bool m_hasActivity = false;
    bool m_hasMetadata = false;
    bool m_hasRichMetadata = false;
    bool m_hasArtwork = false;
    bool m_hasCollections = false;
    bool installationExists(const QString &runtime, const QString &root, const QString &target);
    bool platformAvailable(const QString &platform);
    bool ensureOrganizationSchema();
    bool ensureRichMetadataSchema();
    bool ensureArtworkSchema();
    QString artworkDirectory() const;
    void scanStep();
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
    QTimer m_opticalRefreshTimer;
    QByteArray m_portableInstallOutput, m_portableInstallError;
    QVariantMap m_portableInstallPreview;
    QString m_portableInstallStatus, m_portableSource, m_portableRuntime, m_portableDestination;
    QString m_portableOpticalObservationId, m_portableOpticalDevice;
    QStringList m_lastOpticalObservationIds;
    QString m_portablePhase;
    bool m_portableCanceled=false;
};
