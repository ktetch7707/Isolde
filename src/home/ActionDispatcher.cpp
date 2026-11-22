#include <QCoreApplication>
#include <QDateTime>
#include <QProcess>
#include <QScopedValueRollback>
#include <QTimer>
#include <algorithm>
#include "ActionDispatcher.h"
#include "ComponentSchema.h"
#include "HomeLayout.h"
#include "app/AppCatalog.h"
#include "games/GameLibrary.h"
#include "media/CaptureController.h"
#include "media/GuideController.h"
#include "media/KodiMedia.h"
#include "media/KodiService.h"
#include "sessions/SessionManager.h"
#include "settings/AudioSettings.h"
#include "settings/ProfileManager.h"

ActionDispatcher::ActionDispatcher(AppRegistry* apps,
                                   SessionManager* sessions,
                                   KodiMedia* media,
                                   KodiService* service,
                                   AppCatalog* catalog,
                                   CaptureController* capture,
                                   QObject* parent)
    : QObject(parent), m_apps(apps), m_sessions(sessions), m_media(media), m_service(service), m_catalog(catalog),
      m_capture(capture)
{
    connect(sessions,
            &SessionManager::launching,
            this,
            [this](const QString&)
            {
                if (!m_dispatching && !m_sessions->resumingPreparedLaunch())
                    m_origin.clear();
            });
    connect(sessions,
            &SessionManager::returnedHome,
            this,
            [this](const QString& app)
            {
                clearGuidePlayback();
                const auto gameId = m_pendingGameId;
                const bool record = !m_pendingSteamGame || m_pendingSteamObserved;
                const bool sameProfile = m_pendingGameProfile == qApp->property("isoldeProfile").toString();
                m_pendingGameId.clear();
                m_pendingGameProfile.clear();
                m_pendingSteamIdentity.clear();
                m_pendingSteamGame = false;
                m_pendingSteamObserved = false;
                if (record && sameProfile && !gameId.isEmpty() && m_games && !m_games->recordCompletedSession(gameId))
                    emit rejected(m_games->errorString());
                emit restoreFocusRequested(m_origin.isEmpty() ? app : m_origin);
            });
    connect(sessions,
            &SessionManager::steamGameProcessObserved,
            this,
            [this](const QString& identity)
            {
                if (identity == m_pendingSteamIdentity && m_pendingSteamGame && !m_pendingGameId.isEmpty() &&
                    m_pendingGameProfile == qApp->property("isoldeProfile").toString())
                    m_pendingSteamObserved = true;
            });
    connect(sessions,
            &SessionManager::sessionFailed,
            this,
            [this](const QString&, const QString&)
            {
                m_pendingGameId.clear();
                m_pendingGameProfile.clear();
                m_pendingSteamIdentity.clear();
                m_pendingSteamGame = false;
                m_pendingSteamObserved = false;
            });
    connect(sessions,
            &SessionManager::steamGameFailed,
            this,
            [this](const QString& identity, const QString&)
            {
                if (identity != m_pendingSteamIdentity)
                    return;
                m_pendingGameId.clear();
                m_pendingGameProfile.clear();
                m_pendingSteamIdentity.clear();
                m_pendingSteamGame = false;
                m_pendingSteamObserved = false;
            });
    connect(media,
            &KodiMedia::scheduledMediaUnavailable,
            this,
            [this](const QString& kind, int id)
            {
                if (m_guide && !m_guideChannel.isEmpty())
                    m_guide->markUnavailable(kind, id);
            });
    connect(media,
            &KodiMedia::playbackFailed,
            this,
            [this](const QString& reason)
            {
                if (!m_guideChannel.isEmpty())
                    emit rejected(reason);
            });
    connect(media,
            &KodiMedia::playbackReturned,
            this,
            [this](bool home)
            {
                if (!home && !m_surfChannel.isEmpty() && m_guide)
                {
                    const auto next = m_surfChannel;
                    m_surfChannel.clear();
                    m_guideTransition = true;
                    emit guidePlaybackChanged();
                    if (!m_guide->tune(next, m_origin))
                    {
                        clearGuidePlayback();
                        emit restoreFocusRequested(m_origin);
                    }
                    return;
                }
                if (!home && m_guide && !m_guideChannel.isEmpty() && m_media->lastPlaybackNaturalEnd())
                {
                    const auto now = QDateTime::currentSecsSinceEpoch();
                    if (m_guideEnd < 1 || m_guideEnd > now + 3)
                    {
                        clearGuidePlayback();
                        emit rejected(
                            "This program ended before its scheduled boundary. Repair the channel's media duration "
                            "before continuing.");
                    }
                    else
                    {
                        m_guideTransition = true;
                        emit guidePlaybackChanged();
                        const auto generation = ++m_guideGeneration;
                        QTimer::singleShot(int(std::max<qint64>(0, m_guideEnd - now) * 1000 + 30),
                                           this,
                                           [this, generation]
                                           {
                                               if (generation != m_guideGeneration || !m_guide || !m_guideTransition)
                                                   return;
                                               if (!m_guide->tune(m_guideChannel, m_origin))
                                               {
                                                   clearGuidePlayback();
                                                   emit restoreFocusRequested(m_origin);
                                               }
                                           });
                        return;
                    }
                }
                clearGuidePlayback();
                emit restoreFocusRequested(m_origin);
            });
    connect(media, &KodiMedia::guideSurfRequested, this, &ActionDispatcher::surfGuide);
}
bool
ActionDispatcher::surfGuide(int direction)
{
    if (!m_guide || !m_media->playbackActive() || m_guideChannel.isEmpty() || !m_surfChannel.isEmpty() ||
        (direction != 1 && direction != -1))
        return false;
    QStringList ids;
    for (const auto& v : m_guide->channels())
    {
        const auto channel = v.toMap();
        if (!channel.value("hidden").toBool() &&
            (!channel.contains("owner") ||
             (channel.value("sourceAvailable").toBool() && channel.value("sourceEnabled", true).toBool())))
            ids.append(channel.value("id").toString());
    }
    const int current = static_cast<int>(ids.indexOf(m_guideChannel));
    if (current < 0 || ids.size() < 2)
        return false;
    m_surfChannel = ids[(current + direction + ids.size()) % ids.size()];
    m_media->playbackControl("stop");
    return true;
}
void
ActionDispatcher::clearGuidePlayback()
{
    ++m_guideGeneration;
    m_guideChannel.clear();
    m_surfChannel.clear();
    m_guideSlot.clear();
    m_guideEnd = 0;
    if (m_guideTransition)
    {
        m_guideTransition = false;
        emit guidePlaybackChanged();
    }
}
void
ActionDispatcher::cancelGuidePlayback()
{
    const bool pending = m_guideTransition;
    clearGuidePlayback();
    if (pending && m_guide)
        m_guide->cancel();
}
bool
ActionDispatcher::launchApp(const QString& id)
{
    if (id == "desktop-mode")
    {
        emit desktopRequested(m_origin);
        return true;
    }
    const auto* definition = m_apps->find(id);
    if (definition && definition->presentationMode == "capture")
    {
        if (!m_capture)
            return false;
        emit captureSessionRequested(0, m_origin);
        return true;
    }
    if (id == "movies" || id == "shows")
    {
        const auto page =
            m_layout ? m_layout->composition().value(id == "movies" ? "moviePage" : "showsPage").toString() : QString();
        if (!page.isEmpty() && m_layout->pageModel(page))
        {
            emit pageRequested(page, m_origin);
            return true;
        }
        if (m_service->frontend() == "kodi")
            return m_sessions->launch("kodi");
        emit libraryRequested(false);
        m_media->openLibrary(id);
        return true;
    }
    if (m_catalog && m_catalog->isInstalled(id))
    {
        const auto state = m_catalog->packageState(id);
        if (state == "needs-setup")
            return m_sessions->launch("app-store");
    }
    return m_sessions->launch(id);
}
void
ActionDispatcher::setGuide(GuideController* guide)
{
    clearGuidePlayback();
    if (m_guide)
        disconnect(m_guide, nullptr, this, nullptr);
    m_guide = guide;
    if (!guide)
        return;
    connect(guide,
            &QObject::destroyed,
            this,
            [this]
            {
                m_guide = nullptr;
                clearGuidePlayback();
            });
    connect(guide,
            &GuideController::changed,
            this,
            [this]
            {
                if (m_guideTransition && m_guide && !m_guide->busy() && !m_guide->errorString().isEmpty())
                {
                    const auto error = m_guide->errorString();
                    clearGuidePlayback();
                    emit rejected(error);
                    emit restoreFocusRequested(m_origin);
                }
            });
    connect(guide,
            &GuideController::targetReady,
            this,
            [this](const QVariantMap& target, const QString& origin)
            {
                if (m_sessions->hasActiveSession() || m_media->busy() || m_media->playbackActive() ||
                    m_service->switching() || m_service->frontend() != "isolde")
                {
                    clearGuidePlayback();
                    emit rejected("Kodi is busy or in application mode. Return to Isolde playback mode and retry.");
                    return;
                }
                if (m_guideTransition && target.value("id").toString() == m_guideSlot)
                {
                    clearGuidePlayback();
                    emit rejected("The schedule has not advanced. Return to the guide and review its clock.");
                    return;
                }
                m_origin = origin;
                const auto source = target.value("source").toMap();
                const bool started = target.value("launchKind") == "kodi_pvr"
                                         ? m_media->playPvr(target.value("mediaId").toInt(),
                                                            source.value("clientId").toInt(),
                                                            source.value("uniqueId").toInt())
                                     : target.value("launchKind") == "guide_stream"
                                         ? m_media->playGuideStream(source.value("url").toString())
                                         : m_media->playScheduled(target.value("mediaType").toString(),
                                                                  target.value("mediaId").toInt(),
                                                                  target.value("offsetSeconds").toInt());
                if (!started)
                {
                    clearGuidePlayback();
                    emit rejected("This channel could not start. Return to the guide and retry.");
                }
                else if (target.value("launchKind") == "kodi_media")
                {
                    m_guideChannel = target.value("channelId").toString();
                    m_guideSlot = target.value("id").toString();
                    m_guideEnd = target.value("displayEndUtc").toLongLong();
                    if (m_guideTransition)
                    {
                        m_guideTransition = false;
                        emit guidePlaybackChanged();
                    }
                }
                else
                {
                    m_guideChannel = target.value("channelId").toString();
                    m_guideSlot.clear();
                    m_guideEnd = 0;
                    if (m_guideTransition)
                    {
                        m_guideTransition = false;
                        emit guidePlaybackChanged();
                    }
                }
            });
}
bool
ActionDispatcher::dispatch(const QVariantMap& input, const QString& originId)
{
    // Action sequence: execute an ordered list of actions, stopping on the
    // first rejection. Backward compatible with a single "action" map.
    if (input.contains("actions"))
    {
        QString error;
        const auto list = ComponentSchema::actions(input["actions"].toList(), &error);
        if (list.isEmpty())
        {
            emit rejected(error);
            return false;
        }
        bool anyAccepted = false;
        for (const auto& entry : list)
        {
            const auto map = entry.toMap();
            const auto type = map.value("type").toString();
            const auto destination =
                type == "open-page" ? map.value("pageId").toString() : map.value("appId").toString();
            if ((type == "open-page" && m_layout && !m_layout->pageAllowed(destination)) ||
                (type != "open-page" && !destination.isEmpty() && !ProfileManager::allows("destination", destination)))
            {
                emit rejected(
                    tr("This destination is restricted in this profile. Settings > Profiles provides recovery."));
                return anyAccepted;
            }
            const auto previous = m_origin;
            m_origin = originId;
            QScopedValueRollback<bool> dispatching(m_dispatching, true);
            if (!dispatchSingleAction(map, originId))
            {
                m_origin = previous;
                return anyAccepted;
            }
            anyAccepted = true;
        }
        return anyAccepted;
    }

    QString error;
    QVariantMap value;
    if (!validateAndPreflightSingleAction(input, originId, error, value))
        return false;
    const auto previous = m_origin;
    m_origin = originId;
    QScopedValueRollback<bool> dispatching(m_dispatching, true);
    return dispatchSingleAction(value, originId);
}

bool
ActionDispatcher::dispatchEditorAction(const QVariantMap& input)
{
    QString error;
    const auto value = ComponentSchema::editorAction(input, &error);
    if (value.isEmpty())
    {
        emit rejected(error);
        return false;
    }

    const auto type = value.value("type").toString();
    bool accepted = false;
    if (type == "capture-artwork-apply")
    {
        if (!m_layout)
            error = tr("Profile artwork settings are unavailable.");
        else
        {
            accepted = m_layout->setCaptureInputArtwork(value.value("artwork").toMap());
            if (!accepted)
                error = m_layout->errorString();
        }
    }
    else if (type == "audio-output-select")
    {
        if (!m_audio)
            error = tr("Audio output selection is unavailable.");
        else
            accepted = m_audio->selectOutput(value.value("outputId").toString());
    }
    else
    {
        if (!m_capture)
            error = tr("Capture input support is unavailable.");
        else if (type == "capture-apply")
        {
            accepted = m_capture->applyConfiguration(value.value("configuration").toMap());
        }
        else if (type == "capture-switch-input")
        {
            const auto hdmiInput = value.value("input").toInt();
            if (!m_capture->canSwitchInput(hdmiInput))
                error = m_capture->switchStatus();
            else
                accepted = m_capture->switchInput(hdmiInput);
        }
        else if (type == "capture-preview-start")
        {
            accepted = m_capture->startPreview();
        }
        else if (type == "capture-preview-draft")
        {
            accepted = m_capture->startPreviewFor(value.value("captureDeviceId").toString(),
                                                  value.value("videoFormatKey").toString(),
                                                  value.value("audioDeviceId").toString());
        }
        else if (type == "capture-preview-stop")
        {
            m_capture->stopPreview();
            accepted = true;
        }
        else if (type == "capture-switch-cancel")
        {
            m_capture->cancelSwitch();
            accepted = true;
        }
        else if (type == "capture-refresh")
        {
            m_capture->refreshDevices();
            accepted = true;
        }
    }

    if (!accepted)
    {
        if (error.isEmpty())
        {
            if ((type == "capture-preview-start" || type == "capture-preview-draft") && m_capture)
                error = (m_capture->audioState() == "error" || m_capture->audioState() == "missing")
                            ? m_capture->audioStatus()
                            : m_capture->status();
            else if (type.startsWith("capture-") && m_capture)
                error = m_capture->switchStatus().isEmpty() ? m_capture->status() : m_capture->switchStatus();
            else if (m_audio)
                error = m_audio->outputStatus();
            if (error.isEmpty())
                error = tr("The editor action could not be completed. Review the current device selection and retry.");
        }
        emit rejected(error);
    }
    return accepted;
}

bool
ActionDispatcher::dispatchSingleAction(const QVariantMap& value, const QString& originId)
{
    const auto previous = m_origin;
    m_origin = originId;
    const auto type = value.value("type").toString();
    bool accepted = false;
    if (handleNavigationAction(value, originId))
        return true;
    if (handleMediaAction(value, originId))
        return true;
    else if (type == "app-drawer" || type == "open-apps")
    {
        emit allAppsRequested(originId);
        if (type == "app-drawer")
            emit drawerRequested();
        accepted = true;
    }
    else if (type == "open-search")
    {
        emit searchRequested(originId);
        accepted = true;
    }
    else if (type == "launch-app")
        accepted = launchApp(value.value("appId").toString());
    else if (type == "launch-game")
        accepted = handleLaunchGame(value, originId);
    else if (type == "settings")
    {
        accepted = launchApp("settings");
        if (accepted)
            emit settingsRequested(value.value("section").toInt());
    }

    else if (type == "switch-input")
    {
        const auto input = value.value("input").toInt();
        if (!m_capture)
            emit rejected("Capture input support is not installed.");
        else if (!m_capture->canSwitchInput(input))
            emit rejected(m_capture->switchStatus());
        else
        {
            emit captureSessionRequested(input, originId);
            accepted = true;
        }
    }
    else if (type == "run-script")
    {
        const auto script = value.value("target").toString().trimmed();
        const auto argsValue = value.value("args");
        QStringList args;
        if (argsValue.metaType().id() == QMetaType::QVariantList)
        {
            for (const auto& arg : argsValue.toList())
                args.append(arg.toString());
        }
        const int timeoutMs = value.value("timeoutMs", 30000).toInt();
        auto* process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, script](int code, QProcess::ExitStatus status)
        {
            process->deleteLater();
            if (code != 0 || status != QProcess::NormalExit)
                emit rejected(tr("Script %1 exited with code %2.").arg(script).arg(code));
        });
        connect(process, &QProcess::errorOccurred, this, [this, process, script](QProcess::ProcessError error)
        {
            process->deleteLater();
            if (error == QProcess::FailedToStart)
                emit rejected(tr("Script %1 could not be started.").arg(script));
        });
        QTimer::singleShot(timeoutMs, process, [process]() { if (process->state() == QProcess::Running) process->kill(); });
        process->start(script, args);
        if (!process->waitForStarted(5000))
        {
            process->kill();
            process->deleteLater();
            emit rejected(tr("Script %1 failed to start within 5 seconds.").arg(script));
        }
        else
        {
            accepted = true;
        }
    }
    else if (type == "desktop-mode")
    {
        emit desktopRequested(originId);
        accepted = true;
    }
    else
        accepted = m_sessions->launchAction(value);
    if (!accepted)
        m_origin = previous;
    return accepted;
}

bool
ActionDispatcher::handleLaunchGame(const QVariantMap& value, const QString& originId)
{
    Q_UNUSED(originId);
    const auto gameId = value.value("gameId").toString();
    auto resolved = m_games ? m_games->resolveLaunch(gameId, value.value("editionId").toString()) : QVariantMap();
    if (!resolved.isEmpty())
    {
        m_pendingGameId = gameId;
        m_pendingGameProfile = qApp->property("isoldeProfile").toString();
        m_pendingSteamGame = resolved.value("type") == "steam-game";
        m_pendingSteamIdentity = resolved.value("steamAppId").toString();
        m_pendingSteamObserved = false;
    }
    if (resolved.isEmpty())
    {
        emit rejected(m_games ? m_games->errorString() : tr("Games is unavailable for this profile."));
        return false;
    }
    if (resolved.value("type") == "launch-app")
        return launchApp(resolved.value("appId").toString());
    if (resolved.value("type") == "steam-game")
        return m_sessions->launchSteamGame(resolved.value("steamAppId").toString());
    const auto root = resolved.take("sourceRoot").toString();
    return m_sessions->launchGameFile(resolved, root);
}

bool
ActionDispatcher::handleNavigationAction(const QVariantMap& value, const QString& originId)
{
    const auto type = value.value("type").toString();
    if (type == "root-page")
        return handleRootPage(value, originId);
    if (type == "open-page")
        return handleOpenPage(value, originId);
    if (type == "open-guide")
    {
        emit guideRequested(originId, value.value("presentation").toMap(), value.value("focusChannelId").toString());
        return true;
    }
    return false;
}

bool
ActionDispatcher::handleMediaAction(const QVariantMap& value, const QString& originId)
{
    const auto type = value.value("type").toString();
    if (type == "tune-channel")
        return handleTuneChannel(value, originId);
    if (type == "open-collection" || type == "play-movie" || type == "open-show")
        return handleOpenCollection(value, originId);
    return false;
}

bool
ActionDispatcher::handleOpenPage(const QVariantMap& value, const QString& originId)
{
    const auto id = value.value("pageId").toString();
    if (m_layout && m_layout->pageModel(id))
    {
        emit pageRequested(id, originId);
        return true;
    }
    emit rejected("This page was removed. Open Settings → Home → Pages to repair its link.");
    return false;
}

bool
ActionDispatcher::handleOpenCollection(const QVariantMap& value, const QString& originId)
{
    Q_UNUSED(originId);
    if (m_service->frontend() == "kodi")
        return launchApp("kodi");
    emit libraryRequested(value.value("type").toString() == "play-movie");
    const auto id = value.value("collectionId").toString();
    if (value.value("type").toString() == "play-movie")
        m_media->playCollectionMovie(id, value.value("movieId").toInt());
    else if (value.value("type").toString() == "open-show")
        m_media->openCollectionShow(id, value.value("tvShowId").toInt());
    else
        m_media->openCollection(id);
    return true;
}

bool
ActionDispatcher::validateAndPreflightSingleAction(const QVariantMap& input, const QString& originId,
                                                    QString& error, QVariantMap& value)
{
    value = ComponentSchema::action(input, &error);
    if (value.isEmpty())
    {
        emit rejected(error);
        return false;
    }
    error = ComponentSchema::unavailable(value, m_apps, m_media->collections());
    if (!error.isEmpty())
    {
        emit rejected(error);
        return false;
    }
    if (m_sessions->hasActiveSession() || m_media->playbackActive())
    {
        emit rejected("Return to Home before starting another action.");
        return false;
    }
    if (!originId.isEmpty() && !ComponentSchema::safeId(originId))
    {
        emit rejected("Invalid origin.");
        return false;
    }
    if (!checkDestinationRestriction(value, error))
    {
        emit rejected(error);
        return false;
    }
    if (!checkServiceRestriction(value, error))
    {
        emit rejected(error);
        return false;
    }
    return true;
}

bool
ActionDispatcher::handleRootPage(const QVariantMap& value, const QString& originId)
{
    Q_UNUSED(value);
    if (m_layout && m_layout->rootPageVisible())
    {
        emit pageRequested(QString(), originId);
        return true;
    }
    emit rejected("This page was removed. Repair its link in Skin Studio.");
    return false;
}

bool
ActionDispatcher::handleTuneChannel(const QVariantMap& value, const QString& originId)
{
    if (!m_guide)
    {
        emit rejected("The guide is unavailable for this profile.");
        return false;
    }
    return m_guide->tune(value.value("channelId").toString(), originId);
}

bool
ActionDispatcher::checkDestinationRestriction(const QVariantMap& value, QString& error)
{
    const auto type = value.value("type").toString();
    const auto destination = type == "open-page" ? value.value("pageId").toString() : value.value("appId").toString();
    if ((type == "open-page" && m_layout && !m_layout->pageAllowed(destination)) ||
        (type != "open-page" && !destination.isEmpty() && !ProfileManager::allows("destination", destination)))
    {
        error = tr("This destination is restricted in this profile. Settings > Profiles provides recovery.");
        return false;
    }
    return true;
}

bool
ActionDispatcher::checkServiceRestriction(const QVariantMap& value, QString& error)
{
    const auto type = value.value("type").toString();
    if (m_service->switching() &&
        (type == "open-collection" || type == "play-movie" || type == "open-show" ||
         (type == "launch-app" && (value.value("appId") == "movies" || value.value("appId") == "shows"))))
    {
        error = "Kodi is changing frontend. Try again when it finishes.";
        return false;
    }
    if ((type == "open-collection" || type == "play-movie" || type == "open-show") && m_service->frontend() != "kodi" &&
        m_media->busy())
    {
        error = "The library is busy. Try again when the current operation finishes.";
        return false;
    }
    return true;
}
