#include "sessions/SessionManager.h"

#include "sessions/MockSession.h"
#include "sessions/DesktopSession.h"
#include "sessions/AndroidSession.h"
#include "home/ComponentSchema.h"
#include "media/MediaGrants.h"
#include "games/ManagedGamePaths.h"
#include "games/OpticalDiscAdapter.h"
#include "games/OpticalDiscGrants.h"
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>

#include <QUrl>
#include <QScopedValueRollback>

SessionManager::SessionManager(AppRegistry *registry, QObject *parent)
    : QObject(parent), m_registry(registry) {
    m_opticalTimer.setInterval(2500);
    connect(&m_opticalTimer,&QTimer::timeout,this,&SessionManager::refreshOpticalSession);
}

void SessionManager::refreshOpticalSession()
{
    QVariantMap status;
    if(m_session){
        const auto config=m_session->definition().launchConfiguration;
        const auto reference=config.value("opticalReference").toString();
        if(OpticalDiscAdapter::isRootReference(reference)){
            const auto profile=qApp->property("isoldeProfile").toString();
            status=OpticalDiscAdapter::classifySession(reference,config.value("gameSourceRoot").toString(),
                OpticalDiscGrants::approved(profile,OpticalDiscAdapter::mediaIdFromRoot(reference)),
                OpticalDiscAdapter::mountedDiscs(),config.value("opticalObservationId").toString());
            status=OpticalDiscAdapter::latchSession(m_opticalSessionStatus,status);
        }
    }
    if(status!=m_opticalSessionStatus){m_opticalSessionStatus=status;emit opticalSessionStatusChanged();}
    if(status.isEmpty())m_opticalTimer.stop();
}

SessionManager::~SessionManager()
{
    if(m_session)disconnect(m_session.get(),nullptr,this,nullptr);
    m_session.reset();
}

bool SessionManager::hasActiveSession() const { return m_session != nullptr; }
QString SessionManager::currentAppId() const { return m_session ? m_session->definition().id : QString(); }
QString SessionManager::currentAppName() const { return m_session ? m_session->definition().name : QString(); }
QString SessionManager::currentAppAccent() const { return m_session ? m_session->definition().accent.name() : QString(); }
QString SessionManager::currentAppType() const { return m_session ? m_session->definition().type : QString(); }
QString SessionManager::currentPresentationMode() const { return m_session ? m_session->definition().presentationMode : QString(); }
QString SessionManager::currentLaunchUrl() const { return m_session ? m_session->definition().launchConfiguration.value(QStringLiteral("url")).toString() : QString(); }
QStringList SessionManager::currentAllowedOrigins() const
{
    if (!m_session)
        return {};
    QStringList origins;
    const auto configuredOrigins = m_session->definition().launchConfiguration.value(QStringLiteral("allowedOrigins"));
    for (const auto &origin : configuredOrigins.toList()) {
        const auto text = origin.toString().trimmed();
        if (!text.isEmpty())
            origins.append(text);
    }
    if (!origins.isEmpty())
        return origins;
    const QUrl url(currentLaunchUrl());
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty())
        return {};
    QString origin = url.scheme() + QStringLiteral("://") + url.host();
    if (url.port() != -1)
        origin += QStringLiteral(":%1").arg(url.port());
    return QStringList{origin};
}
QString SessionManager::previousFocusId() const { return m_previousFocusId; }
bool SessionManager::resumePreparedLaunch(const AppDefinition &definition)
{
    QScopedValueRollback<bool> resuming(m_resumingPrepared,true);
    return launchDefinition(definition);
}

bool SessionManager::launch(const QString &appId)
{
    const auto *definition = m_registry->find(appId);
    if (!definition) {
        const auto reason = tr("Unknown app ID: %1").arg(appId);
        emit sessionRejected(reason);
        emit sessionFailed(appId, reason);
        return false;
    }
    return launchDefinition(*definition);
}

bool SessionManager::launchDefinition(AppDefinition definition)
{
    if (m_launchGuard) {
        const auto reason = m_launchGuard(definition.id);
        if (!reason.isEmpty()) { emit sessionRejected(reason); emit launchDeferred(reason); return false; }
    }
    if (m_session) {
        const auto reason = tr("A session is already active.");
        emit sessionRejected(reason);
        emit sessionFailed(definition.id, reason);
        return false;
    }
    if (definition.type != QStringLiteral("mock") && definition.type != QStringLiteral("store")
        && definition.type != QStringLiteral("customize") && definition.type != QStringLiteral("settings")
        && definition.type != QStringLiteral("desktop") && definition.type != QStringLiteral("web") && definition.type != "android") {
        const auto reason = tr("Unsupported session type: %1").arg(definition.type);
        emit sessionRejected(reason);
        emit sessionFailed(definition.id, reason);
        return false;
    }

    if(m_launchPreparation && !m_launchPreparation(definition)){
        return true;
    }
    m_previousFocusId = definition.id;
    if(!m_gamepadSessionStatus.isEmpty()){
        m_gamepadSessionStatus.clear();
        emit gamepadSessionStatusChanged();
    }
    emit launching(definition.id);
    emit previousFocusIdChanged();
    if (definition.type == QStringLiteral("desktop")) m_session = std::make_unique<DesktopSession>(definition);
    else if (definition.type == "android") m_session = std::make_unique<AndroidSession>(definition);
    else m_session = std::make_unique<MockSession>(definition);
    auto *session = m_session.get();
    connect(session,&AppSession::steamProviderStateChanged,this,[this,session](const QVariantMap &state){
        if(m_session.get()!=session)return;
        m_steamProviderState=state;
        emit steamProviderStateChanged();
    });
    connect(session,&AppSession::gamepadSessionStatusChanged,this,[this,session](const QVariantMap &state){
        if(m_session.get()!=session)return;
        if(m_gamepadSessionStatus==state)return;
        m_gamepadSessionStatus=state;
        emit gamepadSessionStatusChanged();
    });
    const auto appId = definition.id;
    connect(session, &AppSession::stateChanged, this, [this, session, appId, opened=false](AppSession::State state) mutable {
        if(state==AppSession::State::Running && m_session.get()==session && !opened) {
            opened=true;
            const auto steamGame=session->definition().launchConfiguration.value("steamGameId").toString();
            const auto type=session->definition().type;
            if(QStringList{"web","desktop","android"}.contains(type))emit appOpened(appId);
            if(!steamGame.isEmpty() && m_session.get()==session)emit steamGameProcessObserved(steamGame);
        }
        if (state == AppSession::State::Exited && !m_sessionCall)
            finishSession(session);
    });
    {
        QScopedValueRollback<bool> calling(m_sessionCall,true);
        session->launch();
    }
    if (!m_session)
        return false;
    if (m_session->state() == AppSession::State::Exited) {
        finishSession(session);
        return false;
    }
    emit currentAppChanged();
    emit activeChanged();
    refreshOpticalSession();
    if(!m_opticalSessionStatus.isEmpty())m_opticalTimer.start();
    return true;
}

void SessionManager::finishSession(AppSession *session)
{
    if(m_session.get()!=session)return;
    const bool intentionalExit=m_intentionalExit;
    const auto failure=session->errorString();
    const auto appName=session->definition().name;
    const auto appId=session->definition().id;
    const auto steamGame=session->definition().launchConfiguration.value("steamGameId").toString();
    const bool normalDesktopExit=(session->definition().type=="desktop" || session->definition().type=="android") && failure.isEmpty();
    m_intentionalExit=false;
    m_session.reset();
    if(!m_gamepadSessionStatus.isEmpty()){
        m_gamepadSessionStatus.clear();
        emit gamepadSessionStatusChanged();
    }
    refreshOpticalSession();
    emit currentAppChanged();
    emit activeChanged();
    if(intentionalExit && !failure.isEmpty() && !steamGame.isEmpty()){
        emit sessionRejected(failure);
        emit steamGameFailed(steamGame,failure);
    }else if(!intentionalExit && !normalDesktopExit){
        const auto reason=failure.isEmpty()?tr("%1 closed or failed to start.").arg(appName):failure;
        emit sessionRejected(reason);
        if(!steamGame.isEmpty())emit steamGameFailed(steamGame,reason);
        else emit sessionFailed(appId,reason);
    }
    emit returnedHome(m_previousFocusId);
}

bool SessionManager::launchAction(const QVariantMap &input)
{
    return launchFileAction(input,{});
}
bool SessionManager::launchGameFile(const QVariantMap &input,const QString &sourceRoot)
{
    const auto target=input.value("target").toString();
    const auto *runtime=m_registry->find(input.value("openWith").toString());
    const auto error=MediaGrants::validate(sourceRoot);
    const auto profile=qApp->property("isoldeProfile").toString();
    const bool managed=ManagedGamePaths::isInstallRoot(profile,sourceRoot);
    const auto opticalMediaId=input.value("opticalMediaId").toString();
    const auto opticalReference=input.value("opticalReference").toString();
    const auto opticalObservationId=input.value("opticalObservationId").toString();
    bool optical=false;
    const auto discs=OpticalDiscAdapter::mountedDiscs();
    for(const auto &item:discs){
        const auto disc=item.toMap();
        if(disc.value("mountRoot")==sourceRoot && disc.value("mediaId")==opticalMediaId
            && disc.value("observationId")==opticalObservationId
            && OpticalDiscAdapter::classifyRootReference(opticalReference,{disc}).value("state")=="ready"
            && OpticalDiscGrants::approved(profile,opticalMediaId)
            && disc.value("directPlaySupported").toBool())optical=true;
    }
    QFile executable(target);
    const bool nativeElf=(!(managed || optical) || (executable.open(QIODevice::ReadOnly)
        && executable.read(4)==QByteArray("\x7f""ELF",4)));
    if((!opticalMediaId.isEmpty() && (!optical || !OpticalDiscAdapter::isRootReference(opticalReference)
        || OpticalDiscAdapter::mediaIdFromRoot(opticalReference)!=opticalMediaId))
        || (opticalMediaId.isEmpty() && (!opticalReference.isEmpty() || !opticalObservationId.isEmpty())) || (!error.isEmpty() && !managed && !optical)
        || !nativeElf || input.value("type").toString()!="open-file" || !runtime || runtime->type!="desktop"
        || !QStringList{"game","game-hub"}.contains(runtime->presentationMode) || !target.startsWith(sourceRoot+'/')
        || !QFileInfo(target).isFile() || QFileInfo(target).canonicalFilePath()!=target
        || (managed && !QFileInfo(target).isExecutable())
        || (optical && !(QFileInfo(target).permissions() & (QFile::ExeOwner|QFile::ExeGroup|QFile::ExeOther)))){
        emit sessionRejected(tr("The reviewed game folder is missing, redirected or incompatible. Review its source and retry."));return false;
    }
    return launchFileAction(input,sourceRoot,optical?opticalMediaId:QString(),optical?opticalReference:QString());
}
bool SessionManager::launchSteamGame(const QString &identity)
{
    const auto *steam=m_registry->find("steam");bool valid=false;const auto number=identity.toULongLong(&valid);
    if(!steam || steam->type!="desktop" || steam->presentationMode!="game-hub" || steam->launchConfiguration.value("desktopId")!="isolde-steam"
        || !QRegularExpression("^[1-9][0-9]{0,9}$").match(identity).hasMatch() || !valid || number>4294967295ULL || QString::number(number)!=identity){
        emit sessionRejected(tr("This Steam game or its client is unavailable. Repair Steam and import the game again."));return false;
    }
    auto definition=*steam;definition.launchConfiguration.insert("steamGameId",identity);
    definition.launchConfiguration.remove("containment"); // Never fullscreen the Steam client instead of its game.
    m_steamProviderState.clear();
    emit steamProviderStateChanged();
    return launchDefinition(definition);
}
bool SessionManager::launchFileAction(const QVariantMap &input,const QString &sourceRoot,const QString &opticalMediaId,const QString &opticalReference)
{
    QString error;
    const auto action = ComponentSchema::action(input, &error);
    if (action.isEmpty()) { emit sessionRejected(error); return false; }
    const auto type = action.value(QStringLiteral("type")).toString().trimmed();
    if (type == "launch-app") return launch(action.value("appId").toString());
    const auto target = action.value(QStringLiteral("target")).toString().trimmed();
    auto openWith = action.value(QStringLiteral("openWith")).toString().trimmed();
    if (openWith.isEmpty())
        openWith = QStringLiteral("system");
    if (type.isEmpty() || target.isEmpty()) {
        emit sessionRejected(tr("This tile has an incomplete action."));
        return false;
    }
    if (openWith.isEmpty() || openWith == QStringLiteral("system")) {
        emit sessionRejected(tr("System default handling for %1 is not implemented yet. Choose an app in Open With.").arg(type));
        return false;
    }
    const auto *definition = m_registry->find(openWith);
    if (!definition) {
        emit sessionRejected(tr("Open With app is not installed: %1").arg(openWith));
        return false;
    }
    if (!definition->handledActions.contains(type)) {
        emit sessionRejected(tr("%1 cannot handle %2 actions.").arg(definition->name, type));
        return false;
    }
    AppDefinition actionDefinition = *definition;
    actionDefinition.launchConfiguration.insert(QStringLiteral("actionType"), type);
    actionDefinition.launchConfiguration.insert(QStringLiteral("actionTarget"), target);
    if(!sourceRoot.isEmpty())actionDefinition.launchConfiguration.insert("gameSourceRoot",sourceRoot);
    if(!opticalMediaId.isEmpty())actionDefinition.launchConfiguration.insert("opticalMediaId",opticalMediaId);
    if(!opticalReference.isEmpty())actionDefinition.launchConfiguration.insert("opticalReference",opticalReference);
    if(!opticalReference.isEmpty())actionDefinition.launchConfiguration.insert("opticalObservationId",input.value("opticalObservationId"));
    if(!opticalMediaId.isEmpty() || ManagedGamePaths::isInstallRoot(qApp->property("isoldeProfile").toString(),sourceRoot)){
        actionDefinition.name=QFileInfo(target).completeBaseName();
        if(opticalMediaId.isEmpty())actionDefinition.launchConfiguration.insert("managedGameRoot",sourceRoot);
        actionDefinition.launchConfiguration.insert("managedGameExecutable",target);
        actionDefinition.launchConfiguration.remove("emulatorRuntime");
    }
    qInfo() << "Launching action tile" << type << "target" << target << "with" << openWith;
    return launchDefinition(actionDefinition);
}

void SessionManager::exitCurrent()
{
    if (!m_session)
        return;
    auto *session = m_session.get();
    m_intentionalExit = true;
    {
        QScopedValueRollback<bool> calling(m_sessionCall,true);
        session->exit();
    }
    if (!m_session)
        return;
    if(m_session->state()==AppSession::State::Exited){
        finishSession(session);
        return;
    }
    m_intentionalExit = false;
    m_session.reset();
    if(!m_gamepadSessionStatus.isEmpty()){
        m_gamepadSessionStatus.clear();
        emit gamepadSessionStatusChanged();
    }
    refreshOpticalSession();
    emit currentAppChanged();
    emit activeChanged();
    emit returnedHome(m_previousFocusId);
}

void SessionManager::goHome()
{
    if (m_session)
        exitCurrent();
    else
        emit returnedHome(m_previousFocusId);
}
