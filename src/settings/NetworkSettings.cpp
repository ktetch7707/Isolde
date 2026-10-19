#include "settings/NetworkSettings.h"

#include <QProcess>
#include <QRegularExpression>

NetworkSettings::NetworkSettings(QObject *parent) : QObject(parent)
{
    refresh();
}

QString NetworkSettings::summary() const { return m_summary; }
QString NetworkSettings::connectivity() const { return m_connectivity; }
bool NetworkSettings::internetAvailable() const { return m_connectivity == QStringLiteral("full"); }
bool NetworkSettings::localNetworkAvailable() const
{
    for (const auto &value : m_devices) {
        const auto device = value.toMap();
        if (device.value(QStringLiteral("type")).toString() == QStringLiteral("loopback"))
            continue;
        if (device.value(QStringLiteral("state")).toString().contains(QStringLiteral("connected"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}
bool NetworkSettings::wifiEnabled() const { return m_wifiEnabled; }
QVariantList NetworkSettings::devices() const { return m_devices; }
QVariantList NetworkSettings::connectionDetails() const { return m_connectionDetails; }
QVariantList NetworkSettings::savedConnections() const { return m_savedConnections; }
QVariantList NetworkSettings::wifiNetworks() const { return m_wifiNetworks; }
QString NetworkSettings::errorString() const { return m_errorString; }

bool NetworkSettings::refresh()
{
    bool ok = false;
    const auto state = runNmcli({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("STATE,CONNECTIVITY"),
                                QStringLiteral("general")}, &ok);
    if (!ok) return false;
    const auto stateParts = splitCells(state.trimmed());
    m_summary = cleanCell(stateParts.value(0, QStringLiteral("Unknown")));
    m_connectivity = cleanCell(stateParts.value(1, QStringLiteral("unknown")));

    const auto radio = runNmcli({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("WIFI"),
                                QStringLiteral("radio")}, &ok);
    if (!ok) return false;
    m_wifiEnabled = radio.trimmed() == QStringLiteral("enabled");

    QVariantList devices;
    QString activeDevice;
    const auto deviceOutput = runNmcli({QStringLiteral("-t"), QStringLiteral("-f"),
                                       QStringLiteral("DEVICE,TYPE,STATE,CONNECTION"), QStringLiteral("device")}, &ok);
    if (!ok) return false;
    for (const auto &line : deviceOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const auto parts = splitCells(line);
        const auto name = cleanCell(parts.value(0));
        const auto type = cleanCell(parts.value(1));
        const auto deviceState = cleanCell(parts.value(2));
        const auto connection = cleanCell(parts.value(3));
        devices.append(QVariantMap{{QStringLiteral("name"), name},
                                   {QStringLiteral("type"), type},
                                   {QStringLiteral("state"), deviceState},
                                   {QStringLiteral("connection"), connection}});
        if (activeDevice.isEmpty() && deviceState.contains(QStringLiteral("connected"), Qt::CaseInsensitive)
            && type != QStringLiteral("loopback")) {
            activeDevice = name;
        }
    }
    m_devices = devices;

    QVariantList details;
    if (!activeDevice.isEmpty()) {
        const auto detailOutput = runNmcli({QStringLiteral("-t"), QStringLiteral("-f"),
                                           QStringLiteral("IP4.ADDRESS,IP4.GATEWAY,IP4.DNS"),
                                           QStringLiteral("device"), QStringLiteral("show"), activeDevice}, &ok);
        if (ok) {
            for (const auto &line : detailOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const auto separator = line.indexOf(QLatin1Char(':'));
                if (separator <= 0) continue;
                const auto key = cleanCell(line.left(separator));
                const auto value = cleanCell(line.mid(separator + 1));
                if (value.isEmpty()) continue;
                QString label = key;
                if (key.startsWith(QStringLiteral("IP4.ADDRESS"))) label = tr("Address");
                else if (key == QStringLiteral("IP4.GATEWAY")) label = tr("Gateway");
                else if (key.startsWith(QStringLiteral("IP4.DNS"))) label = tr("DNS");
                details.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), value}});
            }
        }
    }
    m_connectionDetails = details;

    QVariantList savedConnections;
    const auto connectionsOutput = runNmcli({QStringLiteral("-t"), QStringLiteral("-f"),
                                            QStringLiteral("NAME,UUID,TYPE,DEVICE"), QStringLiteral("connection"),
                                            QStringLiteral("show")}, &ok);
    if (!ok) return false;
    for (const auto &line : connectionsOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const auto parts = splitCells(line);
        const auto uuid = cleanCell(parts.value(1));
        if (!isSafeUuid(uuid)) continue;
        savedConnections.append(QVariantMap{{QStringLiteral("name"), cleanCell(parts.value(0))},
                                            {QStringLiteral("uuid"), uuid},
                                            {QStringLiteral("type"), cleanCell(parts.value(2))},
                                            {QStringLiteral("device"), cleanCell(parts.value(3))}});
    }
    m_savedConnections = savedConnections;

    QVariantList networks;
    if (m_wifiEnabled) {
        const auto wifiOutput = runNmcli({QStringLiteral("-t"), QStringLiteral("-f"),
                                         QStringLiteral("IN-USE,SSID,SIGNAL,SECURITY"), QStringLiteral("device"),
                                         QStringLiteral("wifi"), QStringLiteral("list"), QStringLiteral("--rescan"),
                                         QStringLiteral("yes")}, &ok);
        if (!ok) return false;
        for (const auto &line : wifiOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const auto parts = splitCells(line);
            const auto ssid = cleanCell(parts.value(1));
            if (ssid.isEmpty()) continue;
            networks.append(QVariantMap{{QStringLiteral("active"), cleanCell(parts.value(0)) == QStringLiteral("*")},
                                        {QStringLiteral("ssid"), ssid},
                                        {QStringLiteral("signal"), cleanCell(parts.value(2))},
                                        {QStringLiteral("security"), cleanCell(parts.value(3))}});
        }
    }
    m_wifiNetworks = networks;
    setError({});
    emit changed();
    return true;
}

bool NetworkSettings::setWifiEnabled(bool enabled)
{
    bool ok = false;
    runNmcli({QStringLiteral("radio"), QStringLiteral("wifi"), enabled ? QStringLiteral("on") : QStringLiteral("off")}, &ok);
    if (!ok) return false;
    return refresh();
}

bool NetworkSettings::reconnect(const QString &uuid)
{
    const auto cleanUuid = uuid.trimmed();
    if (!isSafeUuid(cleanUuid)) {
        setError(tr("Choose a saved network first."));
        return false;
    }

    bool ok = false;
    runNmcli({QStringLiteral("connection"), QStringLiteral("up"), QStringLiteral("uuid"), cleanUuid}, &ok);
    if (!ok) return false;
    return refresh();
}

bool NetworkSettings::forgetConnection(const QString &uuid)
{
    const auto cleanUuid = uuid.trimmed();
    if (!isSafeUuid(cleanUuid)) {
        setError(tr("Choose a saved network first."));
        return false;
    }

    bool ok = false;
    runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), QStringLiteral("uuid"), cleanUuid}, &ok);
    if (!ok) return false;
    return refresh();
}

bool NetworkSettings::connectToWifi(const QString &ssid, const QString &password)
{
    const auto cleanSsid = ssid.trimmed();
    if (cleanSsid.isEmpty()) {
        setError(tr("Choose a Wi-Fi network first."));
        return false;
    }

    bool secured = false;
    for (const auto &value : m_wifiNetworks) {
        const auto network = value.toMap();
        if (network.value(QStringLiteral("ssid")).toString() == cleanSsid
            && !network.value(QStringLiteral("security")).toString().trimmed().isEmpty()) {
            secured = true;
            break;
        }
    }
    if (secured && password.isEmpty()) {
        setError(tr("This Wi-Fi network requires a password."));
        return false;
    }

    QStringList arguments{QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("connect"), cleanSsid};
    if (!password.isEmpty()) arguments << QStringLiteral("password") << password;
    bool ok = false;
    runNmcli(arguments, &ok);
    if (!ok) return false;
    return refresh();
}

QString NetworkSettings::runNmcli(const QStringList &arguments, bool *ok) const
{
    QProcess process;
    process.start(QStringLiteral("nmcli"), arguments);
    const bool finished = process.waitForFinished(8000);
    const auto output = QString::fromUtf8(process.readAllStandardOutput());
    const auto error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    const bool success = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (ok) *ok = success;
    if (!success) const_cast<NetworkSettings *>(this)->setError(error.isEmpty() ? tr("Network settings are unavailable.") : error);
    return output;
}

QString NetworkSettings::cleanCell(QString value)
{
    return value.replace(QStringLiteral("\\:"), QStringLiteral(":")).trimmed();
}

QStringList NetworkSettings::splitCells(const QString &line)
{
    QStringList cells;
    QString cell;
    bool escaping = false;
    for (const auto character : line) {
        if (escaping) {
            cell.append(character);
            escaping = false;
        } else if (character == QLatin1Char('\\')) {
            escaping = true;
        } else if (character == QLatin1Char(':')) {
            cells.append(cell.trimmed());
            cell.clear();
        } else {
            cell.append(character);
        }
    }
    if (escaping) cell.append(QLatin1Char('\\'));
    cells.append(cell.trimmed());
    return cells;
}

bool NetworkSettings::isSafeUuid(const QString &uuid)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return pattern.match(uuid).hasMatch();
}

void NetworkSettings::setError(const QString &message)
{
    if (m_errorString == message) return;
    m_errorString = message;
    emit errorStringChanged();
}
