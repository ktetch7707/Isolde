#include "ActionDispatcher.h"
#include "ComponentSchema.h"
#include "games/GameLibrary.h"
#include "HomeLayout.h"
#include "app/AppCatalog.h"
#include "sessions/SessionManager.h"
#include "media/KodiMedia.h"
#include "media/CaptureController.h"
#include "media/KodiService.h"
#include "media/GuideController.h"
#include <QScopedValueRollback>
#include <QDateTime>
#include <QTimer>
#include <algorithm>

ActionDispatcher::ActionDispatcher(AppRegistry *apps, SessionManager *sessions, KodiMedia *media,
    KodiService *service, AppCatalog *catalog, CaptureController *capture, QObject *parent)
    : QObject(parent), m_apps(apps), m_sessions(sessions), m_media(media), m_service(service), m_catalog(catalog), m_capture(capture)
{
    connect(sessions, &SessionManager::launching, this, [this](const QString &) { if (!m_dispatching && !m_sessions->resumingPreparedLaunch()) m_origin.clear(); });
    connect(sessions, &SessionManager::returnedHome, this, [this](const QString &app) {
        clearGuidePlayback();
        emit restoreFocusRequested(m_origin.isEmpty() ? app : m_origin);
    });
    connect(media,&KodiMedia::scheduledMediaUnavailable,this,[this](const QString &kind,int id){
        if(m_guide && !m_guideChannel.isEmpty())m_guide->markUnavailable(kind,id);
    });
    connect(media, &KodiMedia::playbackReturned, this, [this](bool home) {
        if(!home && m_guide && !m_guideChannel.isEmpty() && m_media->lastPlaybackNaturalEnd()) {
            const auto now=QDateTime::currentSecsSinceEpoch();
            if(m_guideEnd<1 || m_guideEnd>now+3) {
                clearGuidePlayback();emit rejected("This program ended before its scheduled boundary. Repair the channel's media duration before continuing.");
            }else{
                m_guideTransition=true;emit guidePlaybackChanged();const auto generation=++m_guideGeneration;
                QTimer::singleShot(int(std::max<qint64>(0,m_guideEnd-now)*1000+30),this,[this,generation]{
                    if(generation!=m_guideGeneration || !m_guide || !m_guideTransition)return;
                    if(!m_guide->tune(m_guideChannel,m_origin)){clearGuidePlayback();emit restoreFocusRequested(m_origin);}
                });return;
            }
        }
        clearGuidePlayback();emit restoreFocusRequested(m_origin);
    });
}
void ActionDispatcher::clearGuidePlayback() {
    ++m_guideGeneration;m_guideChannel.clear();m_guideSlot.clear();m_guideEnd=0;
    if(m_guideTransition){m_guideTransition=false;emit guidePlaybackChanged();}
}
void ActionDispatcher::cancelGuidePlayback() {
    const bool pending=m_guideTransition;clearGuidePlayback();
    if(pending && m_guide)m_guide->cancel();
}
bool ActionDispatcher::launchApp(const QString &id)
{
    if(id=="desktop-mode"){emit desktopRequested(m_origin);return true;}
    const auto *definition = m_apps->find(id);
    if (definition && definition->presentationMode == "capture") {
        if (!m_capture) return false;
        emit captureSessionRequested(0, m_origin);
        return true;
    }
    if (id == "movies" || id == "shows") {
        const auto page=m_layout?m_layout->composition().value(id=="movies"?"moviePage":"showsPage").toString():QString();
        if(!page.isEmpty() && m_layout->pageModel(page)){emit pageRequested(page,m_origin);return true;}
        if (m_service->frontend() == "kodi") return m_sessions->launch("kodi");
        emit libraryRequested(false);
        m_media->openLibrary(id);
        return true;
    }
    if (m_catalog && m_catalog->isInstalled(id)) {
        const auto state = m_catalog->packageState(id);
        if (state == "needs-setup" || state == "failed") return m_sessions->launch("app-store");
    }
    return m_sessions->launch(id);
}
void ActionDispatcher::setGuide(GuideController *guide) {
    clearGuidePlayback();
    if(m_guide)disconnect(m_guide,nullptr,this,nullptr);m_guide=guide;if(!guide)return;
    connect(guide,&QObject::destroyed,this,[this]{m_guide=nullptr;clearGuidePlayback();});
    connect(guide,&GuideController::changed,this,[this]{
        if(m_guideTransition && m_guide && !m_guide->busy() && !m_guide->errorString().isEmpty()) {
            const auto error=m_guide->errorString();clearGuidePlayback();emit rejected(error);emit restoreFocusRequested(m_origin);
        }
    });
    connect(guide,&GuideController::targetReady,this,[this](const QVariantMap &target,const QString &origin){
        if(m_sessions->hasActiveSession() || m_media->busy() || m_media->playbackActive() || m_service->switching() || m_service->frontend()!="isolde") {
            clearGuidePlayback();emit rejected("Kodi is busy or in application mode. Return to Isolde playback mode and retry.");return;
        }
        if(m_guideTransition && target.value("id").toString()==m_guideSlot){clearGuidePlayback();emit rejected("The schedule has not advanced. Return to the guide and review its clock.");return;}
        m_origin=origin;
        const auto source=target.value("source").toMap();
        const bool started=target.value("launchKind")=="kodi_pvr"
            ?m_media->playPvr(target.value("mediaId").toInt(),source.value("clientId").toInt(),source.value("uniqueId").toInt())
            :m_media->playScheduled(target.value("mediaType").toString(),target.value("mediaId").toInt(),target.value("offsetSeconds").toInt());
        if(!started){clearGuidePlayback();emit rejected("This channel could not start. Return to the guide and retry.");}
        else if(target.value("launchKind")!="kodi_pvr") {
            m_guideChannel=target.value("channelId").toString();m_guideSlot=target.value("id").toString();m_guideEnd=target.value("displayEndUtc").toLongLong();
            if(m_guideTransition){m_guideTransition=false;emit guidePlaybackChanged();}
        }else clearGuidePlayback();
    });
}
bool ActionDispatcher::dispatch(const QVariantMap &input, const QString &originId)
{
    QString error;
    const auto value = ComponentSchema::action(input, &error);
    if (value.isEmpty()) { emit rejected(error); return false; }
    error = ComponentSchema::unavailable(value, m_apps, m_media->collections());
    if (!error.isEmpty()) { emit rejected(error); return false; }
    if (m_sessions->hasActiveSession() || m_media->playbackActive()) {
        emit rejected("Return to Home before starting another action."); return false;
    }
    if (!originId.isEmpty() && !ComponentSchema::safeId(originId)) { emit rejected("Invalid origin."); return false; }
    const auto type = value.value("type").toString();
    if (m_service->switching() && (type == "open-collection" || type == "play-movie" || type == "open-show"
        || (type == "launch-app" && (value.value("appId") == "movies" || value.value("appId") == "shows")))) {
        emit rejected("Kodi is changing frontend. Try again when it finishes."); return false;
    }
    if ((type == "open-collection" || type == "play-movie" || type == "open-show") && m_service->frontend() != "kodi" && m_media->busy()) {
        emit rejected("The library is busy. Try again when the current operation finishes."); return false;
    }
    const auto previous = m_origin;
    m_origin = originId;
    QScopedValueRollback<bool> dispatching(m_dispatching, true);
    bool accepted = false;
    if(type=="root-page") {
        if(m_layout && m_layout->rootPageVisible()){emit pageRequested(QString(),originId);accepted=true;}
        else emit rejected("This page was removed. Repair its link in Page Studio.");
    }
    else if (type == "open-page") {
        const auto id=value.value("pageId").toString();
        if(m_layout && m_layout->pageModel(id)){emit pageRequested(id,originId);accepted=true;}
        else emit rejected("This page was removed. Open Settings → Home → Pages to repair its link.");
    }
    else if (type == "open-guide") { emit guideRequested(originId,value.value("presentation").toMap(),value.value("focusChannelId").toString());accepted=true; }
    else if (type == "tune-channel") {
        if(!m_guide)emit rejected("The guide is unavailable for this profile.");
        else accepted=m_guide->tune(value.value("channelId").toString(),originId);
    }
    else if (type == "app-drawer") { emit drawerRequested(); accepted = true; }
    else if (type == "launch-app") accepted = launchApp(value.value("appId").toString());
    else if (type == "launch-game") {
        auto resolved = m_games ? m_games->resolveLaunch(value.value("gameId").toString(),value.value("editionId").toString()) : QVariantMap();
        if(resolved.isEmpty()) emit rejected(m_games ? m_games->errorString() : tr("Games is unavailable in this profile."));
        else if(resolved.value("type")=="launch-app") accepted=launchApp(resolved.value("appId").toString());
        else if(resolved.value("type")=="steam-game") accepted=m_sessions->launchSteamGame(resolved.value("steamAppId").toString());
        else {const auto root=resolved.take("sourceRoot").toString();accepted=m_sessions->launchGameFile(resolved,root);}
    }
    else if (type == "settings") {
        accepted = launchApp("settings");
        if (accepted) emit settingsRequested(value.value("section").toInt());
    } else if (type == "open-collection" || type == "play-movie" || type == "open-show") {
        if (m_service->frontend() == "kodi") accepted = launchApp("kodi");
        else {
            emit libraryRequested(type == "play-movie");
            const auto id = value.value("collectionId").toString();
            if (type == "play-movie") m_media->playCollectionMovie(id, value.value("movieId").toInt());
            else if (type == "open-show") m_media->openCollectionShow(id, value.value("tvShowId").toInt());
            else m_media->openCollection(id);
            accepted = true;
        }
    } else if (type == "switch-input") {
        const auto input = value.value("input").toInt();
        if (!m_capture) emit rejected("Capture input support is not installed.");
        else if (!m_capture->canSwitchInput(input)) emit rejected(m_capture->switchStatus());
        else { emit captureSessionRequested(input, originId); accepted = true; }
    } else if (type == "desktop-mode") {
        emit desktopRequested(originId);accepted=true;
    } else accepted = m_sessions->launchAction(value);
    if (!accepted) m_origin = previous;
    return accepted;
}
