#include "settings/SoftwareUpdates.h"
#include "settings/SearchSettings.h"

#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

namespace
{
constexpr qint64 MaxReplyBytes = 512 * 1024;

// Require the project's four numeric components; never treat prereleases as stable.
QVersionNumber stableVersion(const QString& tag)
{
    static const QRegularExpression pattern(QStringLiteral("^v([0-9]{1,5}(?:\\.[0-9]{1,5}){3})$"));
    const auto match = pattern.match(tag);
    return match.hasMatch() ? QVersionNumber::fromString(match.captured(1)) : QVersionNumber{};
}

// Only known Ubuntu layouts have a corresponding release artifact.
QString installationPlatform()
{
    if (QSysInfo::productType() != QStringLiteral("ubuntu") ||
        QSysInfo::productVersion() != QStringLiteral("26.04"))
        return {};
    const QFileInfo executable(QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath());
    const auto arch = QSysInfo::currentCpuArchitecture();
    const auto packageArch = arch == QStringLiteral("x86_64") ? QStringLiteral("amd64")
        : (arch == QStringLiteral("arm64") || arch == QStringLiteral("aarch64"))
            ? QStringLiteral("arm64") : QString{};
    if (packageArch.isEmpty())
        return {};
    if (executable.canonicalFilePath() == QStringLiteral("/opt/isolde/bin/isolde"))
        return QStringLiteral("desktop-") + packageArch;
    const QDir release = executable.dir().dirName() == QStringLiteral("bin")
        ? QDir(executable.dir().filePath(QStringLiteral(".."))) : QDir();
    const QFileInfo releaseInfo(release.absolutePath());
    const QDir releases = releaseInfo.dir();
    const QFileInfo current(releases.filePath(QStringLiteral("../current")));
    if (releases.dirName() != QStringLiteral("releases") || !current.isSymLink() ||
        current.canonicalFilePath() != releaseInfo.canonicalFilePath())
        return {};
    return QStringLiteral("linux-") + packageArch;
}

// Keep desktop packages distinct from appliance archives when matching candidates.
QString releaseAsset(const QString& version, const QString& platform)
{
    if (platform == QStringLiteral("desktop-amd64") || platform == QStringLiteral("desktop-arm64"))
        return QStringLiteral("isolde_%1_%2.deb").arg(version, platform.mid(8));
    if (platform == QStringLiteral("linux-amd64") || platform == QStringLiteral("linux-arm64"))
        return QStringLiteral("isolde-%1-%2.tar.gz").arg(version, platform);
    return {};
}

QString statePath()
{
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return root.isEmpty() ? QString{} : QDir(root).filePath(QStringLiteral("software-updates.ini"));
}
}

SoftwareUpdates::SoftwareUpdates(SearchSettings* settings, const QString& installedVersion, QObject* parent,
                                 QNetworkAccessManager* network, const QUrl& endpoint,
                                 const QString& stateFile, const QString& platform)
    : QObject(parent), m_settings(settings), m_network(network ? network : new QNetworkAccessManager(this)),
      m_timer(new QTimer(this)), m_endpoint(endpoint),
      m_stateFile(stateFile.isEmpty() ? statePath() : stateFile),
      m_platform(platform.isNull() ? installationPlatform() : platform),
      m_installed(stableVersion(QStringLiteral("v") + installedVersion))
{
    if (!m_stateFile.isEmpty())
    {
        QSettings state(m_stateFile, QSettings::IniFormat);
        m_dismissed = state.value(QStringLiteral("dismissedVersion")).toString();
    }
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &SoftwareUpdates::check);
    if (m_settings)
        connect(m_settings, &SearchSettings::changed, this, &SoftwareUpdates::webChanged);
    webChanged();
}

bool SoftwareUpdates::available() const
{
    return !m_latest.isEmpty() &&
        QVersionNumber::compare(stableVersion(QStringLiteral("v") + m_latest),
                                stableVersion(QStringLiteral("v") + m_dismissed)) > 0;
}
bool SoftwareUpdates::checking() const { return m_reply != nullptr; }
QString SoftwareUpdates::latestVersion() const { return m_latest; }
QString SoftwareUpdates::status() const { return m_status; }

// Web OFF cancels any in-flight request and prevents automatic or manual checks.
void SoftwareUpdates::webChanged()
{
    if (!m_settings || !m_settings->webEnabled())
    {
        m_webEnabled = false;
        m_timer->stop();
        if (m_reply)
        {
            disconnect(m_reply, nullptr, this, nullptr);
            m_reply->abort();
            m_reply->deleteLater();
            m_reply = nullptr;
        }
        m_status = QStringLiteral("Web is off. Turn it on to check for updates.");
        emit changed();
        return;
    }
    if (!m_webEnabled)
    {
        m_webEnabled = true;
        QTimer::singleShot(0, this, &SoftwareUpdates::check);
    }
}

void SoftwareUpdates::check()
{
    if (!m_settings || !m_settings->webEnabled())
    {
        webChanged();
        return;
    }
    if (m_reply)
        return;
    if (m_retryAfter.isValid() && QDateTime::currentDateTimeUtc() < m_retryAfter)
    {
        fail(QStringLiteral("Update checks are temporarily rate limited. Try again later."));
        return;
    }
    if (m_installed.isNull() || m_platform.isEmpty())
    {
        fail(QStringLiteral("Update checks are not supported for this installation."));
        return;
    }
    QNetworkRequest request(m_endpoint);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Isolde-Software-Updates");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    m_reply = m_network->get(request);
    m_status = QStringLiteral("Checking for updates...");
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (m_reply && (received > MaxReplyBytes || total > MaxReplyBytes))
            m_reply->abort();
    });
    connect(m_reply, &QNetworkReply::finished, this, &SoftwareUpdates::finished);
    emit changed();
}

// A response is only a candidate: signed metadata and payload verification are required before installation.
void SoftwareUpdates::finished()
{
    auto* reply = m_reply;
    m_reply = nullptr;
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto retryAfter = reply->rawHeader("Retry-After");
    const auto reset = reply->rawHeader("X-RateLimit-Reset");
    if (code == 429 || (code == 403 && reply->rawHeader("X-RateLimit-Remaining") == "0"))
    {
        const auto now = QDateTime::currentDateTimeUtc();
        bool ok = false;
        const auto seconds = retryAfter.toLongLong(&ok);
        auto until = ok ? now.addSecs(qBound(qint64(1), seconds, qint64(86400)))
                        : QDateTime::fromString(QString::fromLatin1(retryAfter), Qt::RFC2822Date).toUTC();
        if (!until.isValid())
        {
            const auto epoch = reset.toLongLong(&ok);
            if (ok)
                until = QDateTime::fromSecsSinceEpoch(epoch, Qt::UTC);
        }
        if (!until.isValid() || until <= now)
            until = now.addSecs(3600);
        m_retryAfter = qMin(until, now.addSecs(86400));
    }
    const auto bytes = reply->readAll();
    const auto networkError = reply->error();
    reply->deleteLater();
    scheduleNext();
    if (code == 404)
    {
        m_latest.clear();
        m_status = QStringLiteral("No published stable release is available yet.");
    }
    else if (networkError != QNetworkReply::NoError || code != 200 || bytes.size() > MaxReplyBytes)
        m_status = QStringLiteral("Could not check for updates. Check your connection and try again later.");
    else
    {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(bytes, &error);
        const auto release = document.object();
        const auto tag = release.value(QStringLiteral("tag_name")).toString();
        const auto version = stableVersion(tag);
        if (error.error != QJsonParseError::NoError || !document.isObject() ||
            !release.value(QStringLiteral("draft")).isBool() || !release.value(QStringLiteral("prerelease")).isBool() ||
            release.value(QStringLiteral("draft")).toBool() || release.value(QStringLiteral("prerelease")).toBool() ||
            version.isNull() || !release.value(QStringLiteral("assets")).isArray())
            m_status = QStringLiteral("The release information is invalid. Try again later.");
        else
        {
            const auto payload = releaseAsset(version.toString(), m_platform);
            bool hasPayload = false, hasManifest = false, hasSignature = false;
            for (const auto& entry : release.value(QStringLiteral("assets")).toArray())
            {
                const auto name = entry.toObject().value(QStringLiteral("name")).toString();
                hasPayload |= !payload.isEmpty() && name == payload;
                hasManifest |= name == QStringLiteral("update-manifest.json");
                hasSignature |= name == QStringLiteral("update-manifest.sig");
            }
            if (QVersionNumber::compare(version, m_installed) > 0 && hasPayload && hasManifest && hasSignature)
            {
                m_latest = version.toString();
                m_status = QStringLiteral("A candidate update is available. Installation is not yet supported.");
            }
            else
            {
                m_latest.clear();
                m_status = QVersionNumber::compare(version, m_installed) > 0
                    ? QStringLiteral("The latest release has no update for this device.")
                    : QStringLiteral("Isolde is up to date.");
            }
        }
    }
    emit changed();
}

// Persist dismissal per device rather than modifying any profile's Home layout.
void SoftwareUpdates::dismiss()
{
    if (m_latest.isEmpty())
        return;
    if (m_stateFile.isEmpty())
    {
        fail(QStringLiteral("Could not save the dismissal. The device settings location is unavailable."));
        return;
    }
    const QFileInfo file(m_stateFile);
    if (!QDir().mkpath(file.absolutePath()))
    {
        fail(QStringLiteral("Could not save the dismissal. Check available storage and try again."));
        return;
    }
    QSettings state(m_stateFile, QSettings::IniFormat);
    state.setAtomicSyncRequired(true);
    state.setValue(QStringLiteral("dismissedVersion"), m_latest);
    state.sync();
    if (state.status() != QSettings::NoError)
    {
        fail(QStringLiteral("Could not save the dismissal. Check available storage and try again."));
        return;
    }
    m_dismissed = m_latest;
    m_status = QStringLiteral("Notification dismissed. The update remains listed here.");
    emit changed();
}

void SoftwareUpdates::scheduleNext()
{
    m_timer->start((5 * 60 * 60 + QRandomGenerator::global()->bounded(60 * 60)) * 1000);
}

void SoftwareUpdates::fail(const QString& message)
{
    m_status = message;
    emit changed();
}
