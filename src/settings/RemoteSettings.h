#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class RemoteSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString profile READ profile NOTIFY changed)
    Q_PROPERTY(bool setupMode READ setupMode NOTIFY changed)
    Q_PROPERTY(QString lastInput READ lastInput NOTIFY changed)
    Q_PROPERTY(QString setupStatus READ setupStatus NOTIFY changed)
    Q_PROPERTY(QVariantList requiredButtons READ requiredButtons NOTIFY changed)
    Q_PROPERTY(bool cecAvailable READ cecAvailable NOTIFY changed)
    Q_PROPERTY(QString cecStatus READ cecStatus NOTIFY changed)
    Q_PROPERTY(QString powerAction READ powerAction NOTIFY changed)

public:
    explicit RemoteSettings(QObject *parent = nullptr);

    [[nodiscard]] QString profile() const;
    [[nodiscard]] bool setupMode() const;
    [[nodiscard]] QString lastInput() const;
    [[nodiscard]] QString setupStatus() const;
    [[nodiscard]] QVariantList requiredButtons() const;
    [[nodiscard]] bool cecAvailable() const;
    [[nodiscard]] QString cecStatus() const;
    [[nodiscard]] QString powerAction() const;

    Q_INVOKABLE void nextProfile();
    Q_INVOKABLE void toggleSetupMode();
    Q_INVOKABLE void nextPowerAction();
    Q_INVOKABLE bool refreshCec();
    Q_INVOKABLE bool sendTvPowerCommand();
    Q_INVOKABLE bool turnTvOn();
    Q_INVOKABLE bool putTvInStandby();
    Q_INVOKABLE void recordTestInput(const QString &inputName);
    Q_INVOKABLE void reset();
    void setCecEnabledForTests(bool enabled);

signals:
    void changed();

private:
    void load();
    void save() const;
    static QString cleanProfile(const QString &profile);
    static QString cleanPowerAction(const QString &action);
    bool runCecCommand(const QString &command);

    QString m_profile = QStringLiteral("simple");
    bool m_setupMode = false;
    QString m_lastInput = QStringLiteral("No button tested yet");
    QString m_powerAction = QStringLiteral("ambient");
    bool m_cecAvailable = false;
    bool m_cecEnabled = true;
    QString m_cecTool;
    QString m_cecStatus = QStringLiteral("CEC backend not checked yet");
};
