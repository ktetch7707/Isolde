#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QDirIterator>
#include <QTimer>
#include <memory>

class AppRegistry;

// Profile-owned games metadata only. Never scans or modifies provider databases,
// game files or saves. Page actions contain IDs, never commands or file grants.
class GameLibrary final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QVariantList runtimes READ runtimes NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY importChanged)
    Q_PROPERTY(int importCount READ importCount NOTIFY importChanged)
    Q_PROPERTY(bool importReady READ importReady NOTIFY importChanged)
public:
    GameLibrary(const QString &databasePath, AppRegistry *apps, QObject *parent = nullptr);
    ~GameLibrary() override;
    bool ready() const { return m_ready; }
    QString errorString() const { return m_error; }
    QVariantList runtimes() const;
    // Explicit registration is also explicit consent to read this specific root.
    Q_INVOKABLE QString addGame(const QVariantMap &registration);
    Q_INVOKABLE QVariantMap registration(const QString &gameId);
    Q_INVOKABLE bool updateGame(const QString &gameId, const QVariantMap &registration);
    bool scanning() const { return bool(m_scan); }
    int importCount() const { return m_preview.size(); }
    bool importReady() const { return m_importReady; }
    Q_INVOKABLE bool beginImport(const QString &root, const QString &runtimeId, const QString &platform);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE QVariantList importPage(int offset = 0) const;
    Q_INVOKABLE bool commitImport();
    Q_INVOKABLE bool setFavorite(const QString &gameId, bool favorite);
    Q_INVOKABLE QVariantList page(const QString &view = "all", const QString &search = {}, int offset = 0);
    Q_INVOKABLE QVariantMap details(const QString &gameId);
    Q_INVOKABLE bool removeGame(const QString &gameId);
    QVariantMap resolveLaunch(const QString &gameId);
    void invalidate();
signals:
    void changed();
    void errorChanged();
    void importChanged();
private:
    bool fail(const QString &error);
    bool compatibleRuntime(const QString &id, bool file) const;
    QString checkedTarget(const QString &root, const QString &relative);
    QSqlDatabase m_db;
    QString m_connection, m_error;
    AppRegistry *m_apps;
    bool m_ready = false;
    void scanStep();
    QTimer m_scanTimer;
    std::unique_ptr<QDirIterator> m_scan;
    QVariantList m_preview;
    QString m_importRoot, m_importRuntime, m_importPlatform;
    QStringList m_importFormats;
    int m_scanEntries = 0;
    bool m_importReady = false;
};
