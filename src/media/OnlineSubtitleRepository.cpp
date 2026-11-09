#include "media/OnlineSubtitleRepository.h"

#include "settings/SearchSettings.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

namespace {
constexpr qsizetype MaxSubtitleBytes = 8 * 1024 * 1024;
constexpr qsizetype MaxApiBytes = 2 * 1024 * 1024;
}

OnlineSubtitleRepository::OnlineSubtitleRepository(SearchSettings *settings, QObject *parent,
                                                   const QUrl &apiBase, const QString &apiKey,
                                                   const QString &bearerToken)
    : QObject(parent), m_settings(settings), m_network(new QNetworkAccessManager(this)),
      m_apiBase(apiBase), m_apiKey(apiKey.trimmed())
{
    m_bearerToken = bearerToken.trimmed();
    if (m_apiKey.contains(QLatin1Char('\r')) || m_apiKey.contains(QLatin1Char('\n')) || m_apiKey.size() > 8192)
        m_apiKey.clear();
    if (m_bearerToken.contains(QLatin1Char('\r')) || m_bearerToken.contains(QLatin1Char('\n')) || m_bearerToken.size() > 8192)
        m_bearerToken.clear();
    if (m_settings) {
        connect(m_settings, &SearchSettings::changed, this, &OnlineSubtitleRepository::settingsChanged);
        settingsChanged();
    }
}

OnlineSubtitleRepository::~OnlineSubtitleRepository()
{
    cancel();
}

bool OnlineSubtitleRepository::enabled() const
{
    return m_settings && m_settings->webEnabled();
}

void OnlineSubtitleRepository::settingsChanged()
{
    if (!enabled()) {
        cancel();
        m_results.clear();
        m_status = QStringLiteral("Web search is off. No subtitle repository requests are sent.");
    } else if (!configured()) {
        m_status = QStringLiteral("OpenSubtitles API key is not configured. Local subtitle tools remain available.");
    } else if (!m_busy) {
        m_status = QStringLiteral("OpenSubtitles is ready for an explicit title/language search. No media file is uploaded.");
    }
    emit changed();
}

QUrl OnlineSubtitleRepository::endpoint(const QString &path, const QMap<QString, QString> &query) const
{
    QUrl url = m_apiBase;
    QString joined = url.path();
    if (!joined.endsWith('/')) joined += '/';
    joined += path;
    url.setPath(joined);
    QUrlQuery parameters;
    for (auto it = query.cbegin(); it != query.cend(); ++it)
        parameters.addQueryItem(it.key(), it.value());
    url.setQuery(parameters);
    return url;
}

QNetworkReply *OnlineSubtitleRepository::request(const QUrl &url, const QByteArray &method,
                                                 const QByteArray &body, bool sendApiKey, bool sendBearerToken)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Isolde v1.0");
    if (sendApiKey) request.setRawHeader("Api-Key", m_apiKey.toUtf8());
    if (sendBearerToken && !m_bearerToken.isEmpty())
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_bearerToken.toUtf8());
    if (method == "POST") request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = method == "POST" ? m_network->post(request, body) : m_network->get(request);
    m_replies.append(reply);
    const auto host = url.host().toLower();
    const auto responseLimit = host == QStringLiteral("dl.opensubtitles.com") ||
                               host == QStringLiteral("dl.opensubtitles.org")
        ? MaxSubtitleBytes : MaxApiBytes;
    connect(reply, &QIODevice::readyRead, reply, [reply, responseLimit] {
        if (reply->bytesAvailable() > responseLimit) reply->abort();
    });
    QTimer::singleShot(20000, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
    return reply;
}

void OnlineSubtitleRepository::search(const QString &query, const QString &language, const QString &type)
{
    if (!enabled()) {
        cancel();
        m_results.clear();
        m_status = QStringLiteral("Web search is off. No subtitle repository requests are sent.");
        emit changed();
        return;
    }
    if (!configured()) {
        m_status = QStringLiteral("OpenSubtitles API key is not configured; no external request was sent.");
        emit changed();
        return;
    }
    static const QRegularExpression languagePattern(QStringLiteral("^[a-z]{2,3}$"));
    const auto term = query.trimmed();
    const auto code = language.trimmed().toLower();
    if (term.size() < 2 || term.size() > 200 ||
        !languagePattern.match(code).hasMatch() ||
        (type != QStringLiteral("movie") && type != QStringLiteral("episode"))) {
        m_status = QStringLiteral("Enter a title of 2–200 characters, a two- or three-letter language code, and movie or episode.");
        emit changed();
        return;
    }
    cancel();
    m_results.clear();
    m_busy = true;
    m_pendingTitle = term;
    m_status = QStringLiteral("Searching OpenSubtitles for %1 (%2)…").arg(term, code);
    const auto generation = m_generation;
    const auto url = endpoint(QStringLiteral("subtitles"), {
        {QStringLiteral("query"), term},
        {QStringLiteral("languages"), code},
        {QStringLiteral("type"), type},
        {QStringLiteral("limit"), QStringLiteral("20")}
    });
    auto *reply = request(url, QByteArrayLiteral("GET"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        parseSearchReply(reply, generation);
        m_replies.removeAll(reply);
        reply->deleteLater();
    });
    emit changed();
}

void OnlineSubtitleRepository::parseSearchReply(QNetworkReply *reply, quint64 generation)
{
    if (generation != m_generation || !enabled()) return;
    m_busy = false;
    if (reply->error() != QNetworkReply::NoError) {
        m_results.clear();
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        setError(statusCode ? QStringLiteral("OpenSubtitles search failed (HTTP %1): %2").arg(statusCode).arg(reply->errorString())
                            : QStringLiteral("OpenSubtitles search failed: %1").arg(reply->errorString()));
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        !document.object().value(QStringLiteral("data")).isArray()) {
        m_results.clear();
        setError(QStringLiteral("OpenSubtitles returned an invalid search response."));
        return;
    }
    QVariantList results;
    const auto entries = document.object().value(QStringLiteral("data")).toArray();
    for (const auto &value : entries) {
        if (!value.isObject() || results.size() >= 20) continue;
        const auto attributes = value.toObject().value(QStringLiteral("attributes")).toObject();
        const auto files = attributes.value(QStringLiteral("files")).toArray();
        if (files.isEmpty() || !files.first().isObject()) continue;
        const auto file = files.first().toObject();
        const auto fileId = file.value(QStringLiteral("file_id")).toInt(-1);
        const auto fileName = file.value(QStringLiteral("file_name")).toString().trimmed();
        if (fileId <= 0 || !fileName.endsWith(QStringLiteral(".srt"), Qt::CaseInsensitive)) continue;
        const auto feature = attributes.value(QStringLiteral("feature_details")).toObject();
        auto title = feature.value(QStringLiteral("title")).toString().trimmed();
        if (title.isEmpty()) title = feature.value(QStringLiteral("movie_name")).toString().trimmed();
        if (title.isEmpty()) title = m_pendingTitle;
        results.append(QVariantMap{
            {QStringLiteral("fileId"), fileId},
            {QStringLiteral("fileName"), fileName.left(240)},
            {QStringLiteral("title"), title.left(200)},
            {QStringLiteral("language"), attributes.value(QStringLiteral("language")).toString().left(16)},
            {QStringLiteral("release"), attributes.value(QStringLiteral("release")).toString().left(240)},
            {QStringLiteral("downloads"), attributes.value(QStringLiteral("download_count")).toInt()},
            {QStringLiteral("hearingImpaired"), attributes.value(QStringLiteral("hearing_impaired")).toBool()},
            {QStringLiteral("attribution"), QStringLiteral("Subtitle file hosted by OpenSubtitles.com; review before use")},
            {QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
        });
    }
    m_results = results;
    m_status = results.isEmpty()
        ? QStringLiteral("No matching OpenSubtitles SRT files found. Try another title or language.")
        : QStringLiteral("%1 OpenSubtitles SRT result(s). Downloads are explicit; no media is uploaded.").arg(results.size());
    emit changed();
}

void OnlineSubtitleRepository::download(int fileId)
{
    if (!enabled() || !configured()) {
        setError(QStringLiteral("OpenSubtitles download requires Web ON and a configured API key."));
        return;
    }
    QVariantMap selected;
    for (const auto &value : m_results) {
        const auto result = value.toMap();
        if (result.value(QStringLiteral("fileId")).toInt() == fileId) {
            selected = result;
            break;
        }
    }
    if (fileId <= 0 || selected.isEmpty()) {
        setError(QStringLiteral("Choose an SRT from the current OpenSubtitles result list."));
        return;
    }
    cancel();
    m_busy = true;
    m_pendingTitle = selected.value(QStringLiteral("title")).toString();
    m_pendingLanguage = selected.value(QStringLiteral("language")).toString();
    m_pendingFileName = selected.value(QStringLiteral("fileName")).toString();
    m_status = QStringLiteral("Requesting a fresh download link for the selected subtitle…");
    const auto generation = m_generation;
    const QJsonObject body{{QStringLiteral("file_id"), fileId}};
    auto *reply = request(endpoint(QStringLiteral("download")), QByteArrayLiteral("POST"),
                          QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, fileId] {
        parseDownloadLink(reply, generation, fileId);
        m_replies.removeAll(reply);
        reply->deleteLater();
    });
    emit changed();
}

void OnlineSubtitleRepository::parseDownloadLink(QNetworkReply *reply, quint64 generation, int fileId)
{
    if (generation != m_generation || !enabled()) return;
    if (reply->error() != QNetworkReply::NoError) {
        m_busy = false;
        setError(QStringLiteral("OpenSubtitles could not create a download link: %1").arg(reply->errorString()));
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_busy = false;
        setError(QStringLiteral("OpenSubtitles returned an invalid download-link response."));
        return;
    }
    const auto responseObject = document.object();
    const auto returnedFileName = responseObject.value(QStringLiteral("file_name")).toString().trimmed();
    if (!returnedFileName.isEmpty() &&
        !returnedFileName.endsWith(QStringLiteral(".srt"), Qt::CaseInsensitive)) {
        m_busy = false;
        setError(QStringLiteral("OpenSubtitles returned a non-SRT file; it was not downloaded."));
        return;
    }
    if (!returnedFileName.isEmpty()) m_pendingFileName = returnedFileName.left(240);
    const auto url = QUrl(responseObject.value(QStringLiteral("link")).toString());
    const bool localFixture = m_apiBase.scheme() == QStringLiteral("http") &&
        QHostAddress(m_apiBase.host()).isLoopback();
    const auto expectedScheme = localFixture ? QStringLiteral("http") : QStringLiteral("https");
    const auto urlHost = url.host().toLower();
    const bool allowedDownloadHost = localFixture
        ? urlHost == m_apiBase.host().toLower()
        : urlHost == QStringLiteral("dl.opensubtitles.com") ||
          urlHost == QStringLiteral("dl.opensubtitles.org");
    if (!url.isValid() || url.scheme() != expectedScheme ||
        !allowedDownloadHost ||
        !url.userInfo().isEmpty() || (!localFixture && url.port(-1) != -1) ||
        (localFixture && url.port(-1) != m_apiBase.port(-1)) || !url.fragment().isEmpty()) {
        m_busy = false;
        setError(QStringLiteral("OpenSubtitles returned an unsupported download host; the file was not fetched."));
        return;
    }
    auto *downloadReply = request(url, QByteArrayLiteral("GET"), {}, false, false);
    const auto generationForFile = generation;
    connect(downloadReply, &QNetworkReply::finished, this, [this, downloadReply, generationForFile, fileId] {
        parseSubtitleReply(downloadReply, generationForFile, fileId);
        m_replies.removeAll(downloadReply);
        downloadReply->deleteLater();
    });
}

void OnlineSubtitleRepository::parseSubtitleReply(QNetworkReply *reply, quint64 generation, int fileId)
{
    if (generation != m_generation || !enabled()) return;
    m_busy = false;
    if (reply->error() != QNetworkReply::NoError || reply->bytesAvailable() > MaxSubtitleBytes) {
        setError(QStringLiteral("OpenSubtitles subtitle download failed or exceeded the 8 MiB limit."));
        return;
    }
    const auto bytes = reply->readAll();
    if (bytes.size() > MaxSubtitleBytes) {
        setError(QStringLiteral("OpenSubtitles subtitle exceeds the 8 MiB limit."));
        return;
    }
    const auto text = QString::fromUtf8(bytes);
    if (bytes.isEmpty() || text.toUtf8() != bytes ||
        bytes.startsWith("PK") || bytes.startsWith("\x1f\x8b")) {
        setError(QStringLiteral("OpenSubtitles did not return a bounded plain UTF-8 SRT; archives and binary files are refused."));
        return;
    }
    m_status = QStringLiteral("Subtitle downloaded from OpenSubtitles. It will be stored locally for review; no media was uploaded.");
    emit subtitleDownloaded(m_pendingTitle, m_pendingLanguage, text,
                            QStringLiteral("OpenSubtitles.com"), fileId);
    emit changed();
}

void OnlineSubtitleRepository::setError(const QString &message)
{
    m_busy = false;
    m_status = message;
    emit changed();
}

void OnlineSubtitleRepository::cancel()
{
    ++m_generation;
    const auto pending = m_replies;
    m_replies.clear();
    for (const auto &reply : pending)
        if (reply) reply->abort();
    m_busy = false;
}
