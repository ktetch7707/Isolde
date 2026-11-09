#pragma once

#include "app/AppRegistry.h"
#include "sessions/AppSession.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QTimer>
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
    Q_PROPERTY(QVariantMap opticalSessionStatus READ opticalSessionStatus NOTIFY opticalSessionStatusChanged)
    Q_PROPERTY(QVariantMap steamProviderState READ steamProviderState NOTIFY steamProviderStateChanged)
    Q_PROPERTY(QVariantMap steamRecoveryStatuses READ steamRecoveryStatuses NOTIFY steamRecoveryStatusesChanged)
    Q_PROPERTY(QVariantMap gamepadSessionStatus READ gamepadSessionStatus NOTIFY gamepadSessionStatusChanged)

public:
    explicit SessionManager(AppRegistry *registry, QObject *parent = nullptr);
    ~SessionManager() override;
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
    [[nodiscard]] QVariantMap opticalSessionStatus() const { return m_opticalSessionStatus; }
    [[nodiscard]] QVariantMap steamProviderState() const { return m_steamProviderState; }
    [[nodiscard]] QVariantMap steamRecoveryStatuses() const { return m_steamRecoveryStatuses; }
    [[nodiscard]] QVariantMap gamepadSessionStatus() const { return m_gamepadSessionStatus; }

    Q_INVOKABLE bool launch(const QString &appId);
    Q_INVOKABLE bool launchAction(const QVariantMap &action);
    Q_INVOKABLE bool acknowledgeSteamRecovery(const QString &steamAppId);
    // Only the identity-resolving Games dispatcher supplies this grant. It is
    // deliberately not a QML/layout action argument.
    bool launchGameFile(const QVariantMap &action, const QString &sourceRoot);
    bool launchSteamGame(const QString &steamAppId);
    Q_INVOKABLE void exitCurrent();
    Q_INVOKABLE void goHome();

signals:
    void launchDeferred(const QString &reason);
    void launching(const QString &appId);
    void appOpened(const QString &appId);
    void steamGameProcessObserved(const QString &steamAppId);
    void steamProviderStateChanged();
    void steamRecoveryStatusesChanged();
    void activeChanged();
    void currentAppChanged();
    void previousFocusIdChanged();
    void sessionRejected(const QString &reason);
    void sessionFailed(const QString &appId, const QString &reason);
    void steamGameFailed(const QString &steamAppId, const QString &reason);
    void returnedHome(const QString &focusId);
    void opticalSessionStatusChanged();
    void gamepadSessionStatusChanged();

private:
    bool launchFileAction(const QVariantMap &action, const QString &sourceRoot,
                          const QString &opticalMediaId = {}, const QString &opticalReference = {});
    bool launchDefinition(AppDefinition definition);
    void refreshOpticalSession();
    void finishSession(AppSession *session);

    AppRegistry *m_registry;
    std::unique_ptr<AppSession> m_session;
    QString m_previousFocusId;
    QVariantMap m_steamProviderState;
    QVariantMap m_steamRecoveryStatuses;
    QVariantMap m_gamepadSessionStatus;
    bool m_intentionalExit = false;
    bool m_sessionCall = false;
    bool m_resumingPrepared=false;
    std::function<QString(const QString &)> m_launchGuard;
    std::function<bool(const AppDefinition &)> m_launchPreparation;
    QTimer m_opticalTimer;
    QVariantMap m_opticalSessionStatus;
};
