#pragma once

#include "app/AppRegistry.h"
#include "sessions/AppSession.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <memory>
#include <functional>

class SessionManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ hasActiveSession NOTIFY activeChanged)
    Q_PROPERTY(QString currentAppId READ currentAppId NOTIFY currentAppChanged)
    Q_PROPERTY(QString currentAppName READ currentAppName NOTIFY currentAppChanged)
    Q_PROPERTY(QString currentAppAccent READ currentAppAccent NOTIFY currentAppChanged)
    Q_PROPERTY(QString currentAppType READ currentAppType NOTIFY currentAppChanged)
    Q_PROPERTY(QString currentPresentationMode READ currentPresentationMode NOTIFY currentAppChanged)
    Q_PROPERTY(QString currentLaunchUrl READ currentLaunchUrl NOTIFY currentAppChanged)
    Q_PROPERTY(QStringList currentAllowedOrigins READ currentAllowedOrigins NOTIFY currentAppChanged)
    Q_PROPERTY(QString previousFocusId READ previousFocusId NOTIFY previousFocusIdChanged)

public:
    explicit SessionManager(AppRegistry *registry, QObject *parent = nullptr);
    void setLaunchGuard(std::function<QString(const QString &)> guard) { m_launchGuard = std::move(guard); }
    void setLaunchPreparation(std::function<bool(const AppDefinition &)> prepare) { m_launchPreparation=std::move(prepare); }
    bool resumePreparedLaunch(const AppDefinition &definition);
    bool resumingPreparedLaunch() const { return m_resumingPrepared; }

    [[nodiscard]] bool hasActiveSession() const;
    [[nodiscard]] QString currentAppId() const;
    [[nodiscard]] QString currentAppName() const;
    [[nodiscard]] QString currentAppAccent() const;
    [[nodiscard]] QString currentAppType() const;
    [[nodiscard]] QString currentPresentationMode() const;
    [[nodiscard]] QString currentLaunchUrl() const;
    [[nodiscard]] QStringList currentAllowedOrigins() const;
    [[nodiscard]] QString previousFocusId() const;

    Q_INVOKABLE bool launch(const QString &appId);
    Q_INVOKABLE bool launchAction(const QVariantMap &action);
    // Only the identity-resolving Games dispatcher supplies this grant. It is
    // deliberately not a QML/layout action argument.
    bool launchGameFile(const QVariantMap &action, const QString &sourceRoot);
    Q_INVOKABLE void exitCurrent();
    Q_INVOKABLE void goHome();

signals:
    void launchDeferred(const QString &reason);
    void launching(const QString &appId);
    void activeChanged();
    void currentAppChanged();
    void previousFocusIdChanged();
    void sessionRejected(const QString &reason);
    void sessionFailed(const QString &appId, const QString &reason);
    void returnedHome(const QString &focusId);

private:
    bool launchFileAction(const QVariantMap &action, const QString &sourceRoot);
    bool launchDefinition(AppDefinition definition);

    AppRegistry *m_registry;
    std::unique_ptr<AppSession> m_session;
    QString m_previousFocusId;
    bool m_intentionalExit = false;
    bool m_resumingPrepared=false;
    std::function<QString(const QString &)> m_launchGuard;
    std::function<bool(const AppDefinition &)> m_launchPreparation;
};
