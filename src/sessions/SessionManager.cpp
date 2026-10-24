#include "sessions/SessionManager.h"

#include "sessions/MockSession.h"
#include "sessions/DesktopSession.h"
#include "home/ComponentSchema.h"

#include <QUrl>
#include <QScopedValueRollback>

SessionManager::SessionManager(AppRegistry *registry, QObject *parent)
    : QObject(parent), m_registry(registry) {}

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
        && definition.type != QStringLiteral("desktop") && definition.type != QStringLiteral("web")) {
        const auto reason = tr("Unsupported session type: %1").arg(definition.type);
        emit sessionRejected(reason);
        emit sessionFailed(definition.id, reason);
        return false;
    }

    if(m_launchPreparation && !m_launchPreparation(definition)){
        return true;
    }
    m_previousFocusId = definition.id;
    emit launching(definition.id);
    emit previousFocusIdChanged();
    if (definition.type == QStringLiteral("desktop")) m_session = std::make_unique<DesktopSession>(definition);
    else m_session = std::make_unique<MockSession>(definition);
    auto *session = m_session.get();
    const auto appName = definition.name;
    const auto appId = definition.id;
    connect(session, &AppSession::stateChanged, this, [this, session, appName, appId](AppSession::State state) {
        if (state != AppSession::State::Exited || m_session.get() != session)
            return;
        const bool intentionalExit = m_intentionalExit;
        const auto failure = session->errorString();
        const bool normalDesktopExit = session->definition().type == "desktop" && failure.isEmpty();
        m_intentionalExit = false;
        m_session.reset();
        emit currentAppChanged();
        emit activeChanged();
        if (intentionalExit || normalDesktopExit) {
            emit returnedHome(m_previousFocusId);
            return;
        }
        const auto reason = failure.isEmpty() ? tr("%1 closed or failed to start.").arg(appName) : failure;
        emit sessionRejected(reason);
        emit sessionFailed(appId, reason);
        emit returnedHome(m_previousFocusId);
    });
    m_session->launch();
    if (!m_session)
        return false;
    if (m_session->state() == AppSession::State::Exited) {
        m_session.reset();
        const auto reason = tr("Unable to start %1.").arg(definition.name);
        emit sessionRejected(reason);
        emit sessionFailed(definition.id, reason);
        return false;
    }
    emit currentAppChanged();
    emit activeChanged();
    return true;
}

bool SessionManager::launchAction(const QVariantMap &input)
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
    qInfo() << "Launching action tile" << type << "target" << target << "with" << openWith;
    return launchDefinition(actionDefinition);
}

void SessionManager::exitCurrent()
{
    if (!m_session)
        return;
    auto *session = m_session.get();
    m_intentionalExit = true;
    session->exit();
    if (!m_session)
        return;
    m_intentionalExit = false;
    m_session.reset();
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
