#pragma once
#include <QObject>
#include <QVariantMap>
class AppRegistry;
class AppCatalog;
class SessionManager;
class KodiMedia;
class KodiService;
class HomeLayout;
class CaptureController;
class GameLibrary;

class ActionDispatcher final : public QObject {
    Q_OBJECT
public:
    ActionDispatcher(AppRegistry *apps, SessionManager *sessions, KodiMedia *media,
        KodiService *service, AppCatalog *catalog = nullptr, CaptureController *capture = nullptr, QObject *parent = nullptr);
    Q_INVOKABLE bool dispatch(const QVariantMap &action, const QString &originId = {});
    void setLayout(HomeLayout *layout) { m_layout=layout; }
    void setGames(GameLibrary *games) { m_games=games; }
signals:
    void drawerRequested();
    void pageRequested(const QString &id, const QString &originId);
    void rejected(const QString &reason);
    void libraryRequested(bool returnHomeAfterPlayback);
    void settingsRequested(int section);
    void restoreFocusRequested(const QString &originId);
    void captureSessionRequested(int input, const QString &originId);
    void desktopRequested(const QString &originId);
private:
    bool launchApp(const QString &id);
    AppRegistry *m_apps;
    SessionManager *m_sessions;
    KodiMedia *m_media;
    KodiService *m_service;
    AppCatalog *m_catalog;
    CaptureController *m_capture;
    HomeLayout *m_layout = nullptr;
    GameLibrary *m_games = nullptr;
    QString m_origin;
    bool m_dispatching = false;
};
