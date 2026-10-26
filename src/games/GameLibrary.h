#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>

class AppRegistry;

// Profile-owned games metadata only. Never scans or modifies provider databases,
// game files or saves. Page actions contain IDs, never commands or file grants.
class GameLibrary final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QVariantList runtimes READ runtimes NOTIFY changed)
public:
    GameLibrary(const QString &databasePath, AppRegistry *apps, QObject *parent = nullptr);
    ~GameLibrary() override;
    bool ready() const { return m_ready; }
    QString errorString() const { return m_error; }
    QVariantList runtimes() const;
    // Explicit registration is also explicit consent to read this specific root.
    Q_INVOKABLE QString addGame(const QVariantMap &registration);
    Q_INVOKABLE bool setFavorite(const QString &gameId, bool favorite);
    Q_INVOKABLE QVariantList page(const QString &view = "all", const QString &search = {}, int offset = 0);
    Q_INVOKABLE QVariantMap details(const QString &gameId);
    Q_INVOKABLE bool removeGame(const QString &gameId);
    QVariantMap resolveLaunch(const QString &gameId);
    void invalidate();
signals:
    void changed();
    void errorChanged();
private:
    bool fail(const QString &error);
    bool compatibleRuntime(const QString &id, bool file) const;
    QString checkedTarget(const QString &root, const QString &relative);
    QSqlDatabase m_db;
    QString m_connection, m_error;
    AppRegistry *m_apps;
    bool m_ready = false;
};
