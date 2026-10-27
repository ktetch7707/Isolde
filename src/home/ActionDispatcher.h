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
class GuideController;

class ActionDispatcher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool guideTransitionPending READ guideTransitionPending NOTIFY guidePlaybackChanged)
public:
    ActionDispatcher(AppRegistry *apps, SessionManager *sessions, KodiMedia *media,
        KodiService *service, AppCatalog *catalog = nullptr, CaptureController *capture = nullptr, QObject *parent = nullptr);
    Q_INVOKABLE bool dispatch(const QVariantMap &action, const QString &originId = {});
    void setLayout(HomeLayout *layout) { m_layout=layout; }
    void setGames(GameLibrary *games) { m_games=games; }
    void setGuide(GuideController *guide);
    bool guideTransitionPending() const { return m_guideTransition; }
    Q_INVOKABLE void cancelGuidePlayback();
signals:
    void guidePlaybackChanged();
    void drawerRequested();
    void guideRequested(const QString &originId,const QVariantMap &presentation);
    void pageRequested(const QString &id, const QString &originId);
    void rejected(const QString &reason);
    void libraryRequested(bool returnHomeAfterPlayback);
    void settingsRequested(int section);
    void restoreFocusRequested(const QString &originId);
    void captureSessionRequested(int input, const QString &originId);
    void desktopRequested(const QString &originId);
private:
    bool launchApp(const QString &id);
    void clearGuidePlayback();
    AppRegistry *m_apps;
    SessionManager *m_sessions;
    KodiMedia *m_media;
    KodiService *m_service;
    AppCatalog *m_catalog;
    CaptureController *m_capture;
    HomeLayout *m_layout = nullptr;
    GameLibrary *m_games = nullptr;
    GuideController *m_guide = nullptr;
    QString m_origin;
    QString m_guideChannel,m_guideSlot;
    qint64 m_guideEnd=0;
    quint64 m_guideGeneration=0;
    bool m_guideTransition=false;
    bool m_dispatching = false;
};
