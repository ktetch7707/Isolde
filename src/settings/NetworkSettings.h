#pragma once

#include <QObject>
#include <QVariantList>

class NetworkSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString connectivity READ connectivity NOTIFY changed)
    Q_PROPERTY(bool internetAvailable READ internetAvailable NOTIFY changed)
    Q_PROPERTY(bool localNetworkAvailable READ localNetworkAvailable NOTIFY changed)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QVariantList connectionDetails READ connectionDetails NOTIFY changed)
    Q_PROPERTY(QVariantList savedConnections READ savedConnections NOTIFY changed)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit NetworkSettings(QObject *parent = nullptr);

    [[nodiscard]] QString summary() const;
    [[nodiscard]] QString connectivity() const;
    [[nodiscard]] bool internetAvailable() const;
    [[nodiscard]] bool localNetworkAvailable() const;
    [[nodiscard]] bool wifiEnabled() const;
    [[nodiscard]] QVariantList devices() const;
    [[nodiscard]] QVariantList connectionDetails() const;
    [[nodiscard]] QVariantList savedConnections() const;
    [[nodiscard]] QVariantList wifiNetworks() const;
    [[nodiscard]] QString errorString() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setWifiEnabled(bool enabled);
    Q_INVOKABLE bool connectToWifi(const QString &ssid, const QString &password = {});
    Q_INVOKABLE bool reconnect(const QString &uuid);
    Q_INVOKABLE bool forgetConnection(const QString &uuid);

signals:
    void changed();
    void errorStringChanged();

private:
    QString runNmcli(const QStringList &arguments, bool *ok = nullptr) const;
    static QString cleanCell(QString value);
    static QStringList splitCells(const QString &line);
    static bool isSafeUuid(const QString &uuid);
    void setError(const QString &message);

    QString m_summary = QStringLiteral("Unknown");
    QString m_connectivity = QStringLiteral("unknown");
    bool m_wifiEnabled = false;
    QVariantList m_devices;
    QVariantList m_connectionDetails;
    QVariantList m_savedConnections;
    QVariantList m_wifiNetworks;
    QString m_errorString;
};
