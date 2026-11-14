#include "media/OnlineDiscovery.h"

#include "settings/SearchSettings.h"

#include <QDateTime>
#include <QDesktopServices>
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
bool supportedProviderRoute(int providerId)
{
    switch (providerId) {
    case 8: case 9: case 15: case 283: case 337: case 350: case 386: case 531: case 1899:
        return true;
    default:
        return false;
    }
}
}

OnlineDiscovery::OnlineDiscovery(SearchSettings *settings, QObject *parent, const QUrl &apiBase, const QString &readToken)
    : QObject(parent), m_settings(settings), m_network(new QNetworkAccessManager(this)),
      m_apiBase(apiBase), m_readToken(readToken.trimmed())
{
    if (m_readToken.contains(QLatin1Char('\r')) || m_readToken.contains(QLatin1Char('\n')) ||
        m_readToken.size() > 8192)
        m_readToken.clear();
    if (m_settings) {
        m_region = m_settings->discoveryRegion();
        connect(m_settings, &SearchSettings::changed, this, &OnlineDiscovery::settingsChanged);
        settingsChanged();
    }
}

OnlineDiscovery::~OnlineDiscovery()
{
    cancel();
}

bool OnlineDiscovery::enabled() const
{
    return m_settings && m_settings->webEnabled();
}

void OnlineDiscovery::settingsChanged()
{
    const auto configuredRegion = m_settings ? m_settings->discoveryRegion() : QStringLiteral("US");
    if (configuredRegion != m_region) {
        m_region = configuredRegion;
        cancel();
        m_results.clear();
        m_availability.clear();
        m_selectedTitle.clear();
    }
    if (!enabled()) {
        cancel();
        m_results.clear();
        m_availability.clear();
        m_selectedTitle.clear();
        m_status = QStringLiteral("Web search is off. Local results only.");
        emit changed();
    } else if (!configured()) {
        m_status = QStringLiteral("Web is on, but TMDB is not configured. Local results only.");
        emit changed();
    } else if (m_results.isEmpty() && !m_busy) {
        m_status = QStringLiteral("Web is on. Search metadata is provided by TMDB; availability does not imply entitlement.");
        emit changed();
    }
}

QVariantList OnlineDiscovery::availability() const
{
    QVariantList filtered;
    for (const auto &entry : m_availability) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("title")).toString() == m_selectedTitle)
            filtered.append(entry);
    }
    return filtered;
}

void OnlineDiscovery::setRegion(const QString &region)
{
    static const QRegularExpression regionPattern(QStringLiteral("^[A-Z]{2}$"));
    const auto normalized = region.trimmed().toUpper();
    if (normalized == m_region)
        return;
    if (!regionPattern.match(normalized).hasMatch()) {
        setError(QStringLiteral("Enter a two-letter ISO region code."));
        return;
    }
    if (!m_settings) {
        setError(QStringLiteral("No profile settings are available to save the discovery region."));
        return;
    }
    m_settings->setDiscoveryRegion(normalized);
}

QUrl OnlineDiscovery::endpoint(const QString &path, const QMap<QString, QString> &query) const
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

QNetworkReply *OnlineDiscovery::get(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_readToken.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = m_network->get(request);
    m_replies.append(reply);
    connect(reply, &QIODevice::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 2 * 1024 * 1024) reply->abort();
    });
    QTimer::singleShot(15000, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
    return reply;
}

void OnlineDiscovery::search(const QString &query)
{
    if (!enabled()) {
        cancel();
        m_results.clear();
        m_status = QStringLiteral("Web search is off. Local results only.");
        emit changed();
        return;
    }
    if (!configured()) {
        m_status = QStringLiteral("TMDB read token is not configured; no external request was sent.");
        emit changed();
        return;
    }
    const QString term = query.trimmed();
    if (term.size() < 2 || term.size() > 200) {
        clearResults();
        m_status = term.size() > 200 ? QStringLiteral("Search is limited to 200 characters.")
                                     : QStringLiteral("Type at least 2 characters for online discovery.");
        emit changed();
        return;
    }

    cancel();
    m_results.clear();
    m_availability.clear();
    m_busy = true;
    m_status = QStringLiteral("Searching TMDB (%1)…").arg(m_region);
    const auto generation = m_generation;
    const auto url = endpoint(QStringLiteral("search/multi"), {
        {QStringLiteral("query"), term},
        {QStringLiteral("region"), m_region},
        {QStringLiteral("include_adult"), QStringLiteral("false")}
    });
    auto *reply = get(url);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        parseSearchReply(reply, generation);
        m_replies.removeAll(reply);
        reply->deleteLater();
    });
    emit changed();
}

void OnlineDiscovery::loadAvailability(const QString &mediaType, int id, const QString &title)
{
    if (!enabled() || !configured()) {
        m_status = QStringLiteral("Provider availability is unavailable while Web is off or unconfigured.");
        emit changed();
        return;
    }
    const auto normalizedTitle = title.trimmed();
    if ((mediaType != QStringLiteral("movie") && mediaType != QStringLiteral("tv")) || id <= 0 ||
        normalizedTitle.isEmpty() || normalizedTitle.size() > 200) {
        setError(QStringLiteral("Invalid TMDB title identity."));
        return;
    }
    cancel();
    m_selectedTitle.clear();
    m_availability.clear();
    m_requestedTitle = normalizedTitle;
    const QString requestedRegion = m_region;
    const auto generation = m_generation;
    m_busy = true;
    m_status = QStringLiteral("Checking TMDB provider listings for %1…").arg(requestedRegion);
    auto *reply = get(endpoint(QStringLiteral("%1/%2/watch/providers").arg(mediaType).arg(id)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, requestedRegion] {
        parseAvailabilityReply(reply, generation, requestedRegion);
        m_replies.removeAll(reply);
        reply->deleteLater();
    });
    m_selectedTitle = normalizedTitle;
    emit changed();
}

void OnlineDiscovery::parseSearchReply(QNetworkReply *reply, quint64 generation)
{
    if (generation != m_generation || !enabled()) return;
    m_busy = false;
    if (reply->error() != QNetworkReply::NoError) {
        m_results.clear();
        setError(QStringLiteral("TMDB discovery failed: %1").arg(reply->errorString()));
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_results.clear();
        setError(QStringLiteral("TMDB returned an invalid discovery response."));
        return;
    }
    QVariantList items;
    const auto array = document.object().value(QStringLiteral("results")).toArray();
    for (const auto &value : array) {
        if (!value.isObject() || items.size() >= 20) continue;
        const auto item = value.toObject();
        const auto mediaType = item.value(QStringLiteral("media_type")).toString();
        if ((mediaType != QStringLiteral("movie") && mediaType != QStringLiteral("tv")) ||
            item.value(QStringLiteral("id")).toInt() <= 0)
            continue;
        const auto title = item.value(mediaType == QStringLiteral("movie") ? QStringLiteral("title") : QStringLiteral("name")).toString().trimmed();
        if (title.isEmpty()) continue;
        const auto release = item.value(mediaType == QStringLiteral("movie") ? QStringLiteral("release_date") : QStringLiteral("first_air_date")).toString();
        const auto year = release.left(4);
        items.append(QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("onlineTitle")},
            {QStringLiteral("provider"), QStringLiteral("TMDB")},
            {QStringLiteral("mediaType"), mediaType},
            {QStringLiteral("providerId"), item.value(QStringLiteral("id")).toInt()},
            {QStringLiteral("identity"), QStringLiteral("%1:%2:%3").arg(mediaType).arg(item.value(QStringLiteral("id")).toInt()).arg(year)},
            {QStringLiteral("name"), title},
            {QStringLiteral("year"), year},
            {QStringLiteral("overview"), item.value(QStringLiteral("overview")).toString()},
            {QStringLiteral("region"), m_region},
            {QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {QStringLiteral("attribution"), QStringLiteral("Metadata by TMDB. This product uses the TMDB API but is not endorsed or certified by TMDB.")},
            {QStringLiteral("entitled"), false},
            {QStringLiteral("symbol"), mediaType == QStringLiteral("movie") ? QStringLiteral("M") : QStringLiteral("TV")}
        });
    }
    m_results = items;
    m_status = items.isEmpty()
        ? QStringLiteral("No TMDB titles found for %1. Provider metadata is not an entitlement.").arg(m_region)
        : QStringLiteral("%1 TMDB result(s), checked now for %2. This product uses the TMDB API but is not endorsed or certified by TMDB; availability is not entitlement.").arg(items.size()).arg(m_region);
    emit changed();
}

void OnlineDiscovery::parseAvailabilityReply(QNetworkReply *reply, quint64 generation, QString requestedRegion)
{
    if (generation != m_generation || !enabled()) return;
    m_busy = false;
    if (reply->error() != QNetworkReply::NoError) {
        m_availability.clear();
        setError(QStringLiteral("TMDB provider lookup failed: %1").arg(reply->errorString()));
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_availability.clear();
        setError(QStringLiteral("TMDB returned an invalid provider response."));
        return;
    }
    const auto country = document.object().value(QStringLiteral("results")).toObject().value(requestedRegion).toObject();
    QVariantList providers;
    const QStringList tiers{QStringLiteral("flatrate"), QStringLiteral("free"), QStringLiteral("ads"),
                            QStringLiteral("rent"), QStringLiteral("buy")};
    for (const auto &tier : tiers) {
        for (const auto &value : country.value(tier).toArray()) {
            const auto provider = value.toObject();
            const auto name = provider.value(QStringLiteral("provider_name")).toString().trimmed();
            if (name.isEmpty()) continue;
            providers.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("tier"), tier},
                {QStringLiteral("providerId"), provider.value(QStringLiteral("provider_id")).toInt()},
                {QStringLiteral("routeSupported"), supportedProviderRoute(provider.value(QStringLiteral("provider_id")).toInt())},
                {QStringLiteral("region"), requestedRegion},
                {QStringLiteral("title"), m_requestedTitle},
                {QStringLiteral("attribution"), QStringLiteral("Listing by TMDB; verify with the service")},
                {QStringLiteral("entitled"), false}
            });
        }
    }
    m_availability = providers;
    m_status = providers.isEmpty()
        ? QStringLiteral("TMDB has no provider listing for %1. This does not confirm unavailability.").arg(requestedRegion)
        : QStringLiteral("%1 provider listing(s) for %2. Listings do not prove access or entitlement.").arg(providers.size()).arg(requestedRegion);
    emit changed();
}

QUrl OnlineDiscovery::providerLaunchUrl(int providerId) const
{
    if (!enabled() || providerId <= 0) return {};
    for (const auto &entry : m_availability) {
        const auto listing = entry.toMap();
        if (listing.value(QStringLiteral("providerId")).toInt() != providerId) continue;
        const auto title = listing.value(QStringLiteral("title")).toString().trimmed();
        if (title.isEmpty() || !listing.value(QStringLiteral("routeSupported")).toBool()) return {};

        QString host;
        QString path;
        QString parameter;
        switch (providerId) {
        case 8: host = QStringLiteral("www.netflix.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("q"); break;
        case 9: host = QStringLiteral("www.primevideo.com"); path = QStringLiteral("/search/ref=atv_nb_sr"); parameter = QStringLiteral("phrase"); break;
        case 15: host = QStringLiteral("www.hulu.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("q"); break;
        case 283: host = QStringLiteral("www.crunchyroll.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("q"); break;
        case 337: host = QStringLiteral("www.disneyplus.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("q"); break;
        case 350: host = QStringLiteral("tv.apple.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("term"); break;
        case 386: host = QStringLiteral("www.peacocktv.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("q"); break;
        case 531: host = QStringLiteral("www.paramountplus.com"); path = QStringLiteral("/search/"); parameter = QStringLiteral("query"); break;
        case 1899: host = QStringLiteral("www.max.com"); path = QStringLiteral("/search"); parameter = QStringLiteral("q"); break;
        default: return {};
        }

        QUrl url;
        url.setScheme(QStringLiteral("https"));
        url.setHost(host);
        url.setPath(path);
        QUrlQuery query;
        query.addQueryItem(parameter, title);
        url.setQuery(query);
        return url;
    }
    return {};
}

bool OnlineDiscovery::launchProvider(int providerId)
{
    const auto url = providerLaunchUrl(providerId);
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || !enabled()) {
        setError(QStringLiteral("No supported official web search route is available for this provider."));
        return false;
    }
    if (!QDesktopServices::openUrl(url)) {
        setError(QStringLiteral("The system could not open this provider's official search route. Open the service manually."));
        return false;
    }
    m_status = QStringLiteral("Opened the provider's official title-search route. Sign-in, availability and entitlement are determined by the service.");
    emit changed();
    return true;
}

void OnlineDiscovery::setError(const QString &message)
{
    m_busy = false;
    m_status = message;
    emit changed();
}

void OnlineDiscovery::cancel()
{
    ++m_generation;
    const auto pending = m_replies;
    m_replies.clear();
    for (const auto &reply : pending)
        if (reply) reply->abort();
    m_busy = false;
}

void OnlineDiscovery::clearResults()
{
    cancel();
    m_results.clear();
    m_availability.clear();
    m_selectedTitle.clear();
    if (!enabled())
        m_status = QStringLiteral("Web search is off. Local results only.");
    else if (!configured())
        m_status = QStringLiteral("TMDB read token is not configured; no external request was sent.");
    else
        m_status = QStringLiteral("Online discovery results cleared.");
    emit changed();
}
