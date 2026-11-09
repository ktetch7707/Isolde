#include "media/OnlineDiscovery.h"
#include "media/OnlineSubtitleRepository.h"
#include "media/SubtitleService.h"
#include "input/InputManager.h"
#include "settings/SearchSettings.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QScopeGuard>
#include <QUrlQuery>
#include <QUuid>
#include <QtTest>

class FixtureRoot {
public:
    FixtureRoot()
        : m_path(QDir::current().filePath(QStringLiteral(".t14-fixture-%1")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))))
    {
        m_valid = QDir().mkpath(m_path);
    }
    ~FixtureRoot() { if (m_valid) QDir(m_path).removeRecursively(); }
    bool isValid() const { return m_valid; }
    QString filePath(const QString &name) const { return QDir(m_path).filePath(name); }
private:
    QString m_path;
    bool m_valid = false;
};

class FakeSubtitleClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString subtitleStatus READ subtitleStatus NOTIFY changed)
public:
    QVariantList subtitleTracks() const
    {
        return {
            QVariantMap{{QStringLiteral("id"), 4}, {QStringLiteral("name"), QStringLiteral("English")}},
            QVariantMap{{QStringLiteral("id"), 8}, {QStringLiteral("name"), QStringLiteral("Spanish")}}
        };
    }
    bool busy() const { return false; }
    QString subtitleStatus() const { return QStringLiteral("2 subtitle tracks discovered from Kodi."); }
    Q_INVOKABLE void refreshSubtitleTracks() { ++refreshes; emit changed(); }
    Q_INVOKABLE void setSubtitleTrack(int trackId) { selected = trackId; emit changed(); }
    int refreshes = 0;
    int selected = -2;
signals:
    void changed();
};

class T14Tests final : public QObject {
    Q_OBJECT
private slots:
    void discoveryIsOptInRegionAwareAndDoesNotClaimEntitlement();
    void subtitleRepositoryIsOptInAndDownloadsOnlyExplicitPlainSrt();
    void offlineTranslationWritesUnicodeSrtWithoutOverwriting();
    void subtitleQualityReportFlagsUnchangedCuesWithoutCertifyingQuality();
    void offlineAudioGenerationUsesOnlyAnInstalledLocalModel();
    void languagePacksCanBeInspectedAndInstalledFromLocalFiles();
    void subtitleInputsAreValidatedAndExplicitlyQueued();
    void subtitleQueueHasRemoteKeyboardAndBackBehavior();
    void subtitleTrackPickerUsesRemoteControlsAndManagedClient();
};

void T14Tests::subtitleRepositoryIsOptInAndDownloadsOnlyExplicitPlainSrt()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QStringList requests;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                auto input = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", input);
                const auto headerEnd = input.indexOf("\r\n\r\n");
                if (headerEnd < 0) return;
                const auto headers = input.left(headerEnd + 4);
                int contentLength = 0;
                const auto contentLengthLine = QRegularExpression(
                    QStringLiteral("(?im)^Content-Length:\\s*(\\d+)\\s*$")).match(QString::fromLatin1(headers));
                if (contentLengthLine.hasMatch()) contentLength = contentLengthLine.captured(1).toInt();
                if (input.size() < headerEnd + 4 + contentLength) return;
                const auto request = QString::fromLatin1(input);
                requests.append(request);
                QJsonObject body;
                QByteArray contentType = QByteArrayLiteral("application/json");
                QByteArray responseBody;
                if (request.startsWith(QStringLiteral("GET /api/v1/subtitles?"))) {
                    body = {{QStringLiteral("data"), QJsonArray{QJsonObject{
                        {QStringLiteral("attributes"), QJsonObject{
                            {QStringLiteral("language"), QStringLiteral("en")},
                            {QStringLiteral("release"), QStringLiteral("Arrival.2016")},
                            {QStringLiteral("download_count"), 77},
                            {QStringLiteral("feature_details"), QJsonObject{{QStringLiteral("title"), QStringLiteral("Arrival")}}},
                            {QStringLiteral("files"), QJsonArray{QJsonObject{
                                {QStringLiteral("file_id"), 755}, {QStringLiteral("file_name"), QStringLiteral("Arrival.en.srt")}
                            }}}
                        }}
                    }}}};
                    responseBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
                } else if (request.startsWith(QStringLiteral("POST /api/v1/download"))) {
                    body = {{QStringLiteral("link"), QStringLiteral("http://127.0.0.1:%1/download/file")
                        .arg(server.serverPort())}};
                    responseBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
                } else if (request.startsWith(QStringLiteral("GET /download/file"))) {
                    contentType = QByteArrayLiteral("text/plain; charset=utf-8");
                    responseBody = QByteArrayLiteral("1\n00:00:01,000 --> 00:00:02,000\nHello from repository\n");
                } else {
                    responseBody = QByteArrayLiteral("{}");
                }
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: ") + contentType +
                    QByteArrayLiteral("\r\nConnection: close\r\nContent-Length: ") +
                    QByteArray::number(responseBody.size()) + QByteArrayLiteral("\r\n\r\n") + responseBody);
                socket->disconnectFromHost();
            });
        }
    });

    FixtureRoot directory;
    QVERIFY(directory.isValid());
    SearchSettings settings(nullptr, directory.filePath(QStringLiteral("preferences.ini")), QStringLiteral("default"));
    const auto api = QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1").arg(server.serverPort()));
    OnlineSubtitleRepository repository(&settings, nullptr, api, QStringLiteral("fixture-api-key"),
                                       QStringLiteral("fixture-bearer"));
    repository.search(QStringLiteral("Arrival"), QStringLiteral("en"), QStringLiteral("movie"));
    QTest::qWait(50);
    QVERIFY(requests.isEmpty());

    settings.setWebEnabled(true);
    repository.search(QStringLiteral("Arrival"), QStringLiteral("en"), QStringLiteral("movie"));
    QTRY_COMPARE(repository.results().size(), 1);
    QCOMPARE(requests.size(), 1);
    QVERIFY(requests.first().contains("Api-Key: fixture-api-key"));
    QVERIFY(requests.first().contains("Authorization:"));
    QVERIFY(requests.first().contains("query=Arrival"));
    QVERIFY(!requests.first().contains("Content-Length: 8\r\n\r\nArrival"));
    const auto result = repository.results().first().toMap();
    QCOMPARE(result.value(QStringLiteral("fileId")).toInt(), 755);
    QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Arrival"));

    QSignalSpy downloaded(&repository, &OnlineSubtitleRepository::subtitleDownloaded);
    repository.download(755);
    QTRY_COMPARE(downloaded.count(), 1);
    QCOMPARE(requests.size(), 3);
    QVERIFY(requests.at(1).startsWith(QStringLiteral("POST /api/v1/download")));
    QVERIFY(requests.at(1).contains("Api-Key: fixture-api-key"));
    QVERIFY(requests.at(1).contains("Authorization:"));
    QVERIFY(requests.at(1).contains("\"file_id\":755"));
    QVERIFY(requests.at(2).startsWith(QStringLiteral("GET /download/file")));
    QVERIFY(!requests.at(2).contains("Api-Key:"));
    QVERIFY(!requests.at(2).contains("Authorization:"));
    const auto signal = downloaded.takeFirst();
    QCOMPARE(signal.at(0).toString(), QStringLiteral("Arrival"));
    QCOMPARE(signal.at(1).toString(), QStringLiteral("en"));
    QVERIFY(signal.at(2).toString().contains(QStringLiteral("Hello from repository")));
    QCOMPARE(signal.at(3).toString(), QStringLiteral("OpenSubtitles.com"));
    QCOMPARE(signal.at(4).toInt(), 755);

    settings.setWebEnabled(false);
    QVERIFY(repository.results().isEmpty());
    repository.search(QStringLiteral("Arrival"), QStringLiteral("en"));
    QCOMPARE(requests.size(), 3);
}

void T14Tests::discoveryIsOptInRegionAwareAndDoesNotClaimEntitlement()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int requests = 0;
    QStringList requestLines;
    bool holdNext = false;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const auto input = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", input);
                if (!input.contains("\r\n\r\n")) return;
                const auto request = QString::fromLatin1(input);
                requestLines.append(request);
                ++requests;
                if (holdNext) {
                    holdNext = false;
                    return;
                }
                QJsonObject body;
                if (request.contains("/search/multi")) {
                    body = {{QStringLiteral("results"), QJsonArray{
                        QJsonObject{{QStringLiteral("id"), 501}, {QStringLiteral("media_type"), QStringLiteral("movie")},
                                    {QStringLiteral("title"), QStringLiteral("Dune")}, {QStringLiteral("release_date"), QStringLiteral("2021-10-22")},
                                    {QStringLiteral("overview"), QStringLiteral("A fixture result.")}},
                        QJsonObject{{QStringLiteral("id"), 502}, {QStringLiteral("media_type"), QStringLiteral("movie")},
                                    {QStringLiteral("title"), QStringLiteral("Dune")}, {QStringLiteral("release_date"), QStringLiteral("1984-12-14")}}
                    }}};
                } else {
                    body = {{QStringLiteral("results"), QJsonObject{
                        {QStringLiteral("CA"), QJsonObject{{QStringLiteral("flatrate"), QJsonArray{QJsonObject{
                            {QStringLiteral("provider_id"), 8}, {QStringLiteral("provider_name"), QStringLiteral("Netflix")}
                        }}}}}
                    }}};
                }
                const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
                const auto response = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ")
                    + QByteArray::number(bytes.size()) + QByteArrayLiteral("\r\n\r\n") + bytes;
                socket->write(response);
                socket->disconnectFromHost();
            });
        }
    });

    FixtureRoot directory;
    QVERIFY(directory.isValid());
    SearchSettings settings(nullptr, directory.filePath(QStringLiteral("preferences.ini")), QStringLiteral("default"));
    const auto api = QUrl(QStringLiteral("http://127.0.0.1:%1/3").arg(server.serverPort()));
    OnlineDiscovery discovery(&settings, nullptr, api, QStringLiteral("fixture-read-token"));
    discovery.setRegion(QStringLiteral("CA"));

    discovery.search(QStringLiteral("Dune"));
    QTest::qWait(100);
    QCOMPARE(requests, 0);
    QVERIFY(discovery.results().isEmpty());

    settings.setWebEnabled(true);
    discovery.search(QStringLiteral("Dune"));
    QTRY_COMPARE(discovery.results().size(), 2);
    QVERIFY(requestLines.first().contains(QStringLiteral("region=CA")));
    QVERIFY(requestLines.first().contains(QStringLiteral("search/multi")));
    QVERIFY(requestLines.first().contains(QStringLiteral("Bearer fixture-read-token")));
    const auto first = discovery.results().at(0).toMap();
    const auto second = discovery.results().at(1).toMap();
    QVERIFY(first.value(QStringLiteral("identity")).toString() != second.value(QStringLiteral("identity")).toString());
    QVERIFY(!first.value(QStringLiteral("entitled")).toBool());
    QVERIFY(first.value(QStringLiteral("checkedAt")).isValid());
    QVERIFY(first.value(QStringLiteral("attribution")).toString().contains(QStringLiteral("not endorsed or certified")));

    discovery.loadAvailability(QStringLiteral("movie"), first.value(QStringLiteral("providerId")).toInt(),
                                first.value(QStringLiteral("name")).toString());
    QTRY_COMPARE(discovery.availability().size(), 1);
    QVERIFY(requestLines.last().contains(QStringLiteral("/movie/501/watch/providers")));
    QVERIFY(!discovery.availability().first().toMap().value(QStringLiteral("entitled")).toBool());
    QVERIFY(discovery.status().contains(QStringLiteral("do not prove access")));
    const auto serviceRoute = discovery.providerLaunchUrl(8);
    QCOMPARE(serviceRoute.scheme(), QStringLiteral("https"));
    QCOMPARE(serviceRoute.host(), QStringLiteral("www.netflix.com"));
    QCOMPARE(QUrlQuery(serviceRoute).queryItemValue(QStringLiteral("q")), QStringLiteral("Dune"));
    QVERIFY(!discovery.providerLaunchUrl(999).isValid());

    discovery.search(QStringLiteral("D"));
    QVERIFY(discovery.results().isEmpty());
    QVERIFY(discovery.availability().isEmpty());
    QCOMPARE(requests, 2);
    QVERIFY(discovery.status().contains(QStringLiteral("2 characters")));

    holdNext = true;
    discovery.search(QStringLiteral("Dune"));
    QTRY_COMPARE(requests, 3);
    QVERIFY(discovery.busy());
    discovery.clearResults();
    QVERIFY(!discovery.busy());
    QVERIFY(discovery.results().isEmpty());
    QVERIFY(discovery.availability().isEmpty());
    QTest::qWait(100);
    QCOMPARE(requests, 3);

    settings.setWebEnabled(false);
    QVERIFY(!discovery.busy());
    QVERIFY(discovery.results().isEmpty());
    QVERIFY(discovery.availability().isEmpty());
    QVERIFY(discovery.status().contains(QStringLiteral("Web search is off")));
    QCOMPARE(requests, 3);
    settings.setWebEnabled(true);
    OnlineDiscovery unconfigured(&settings, nullptr, api, {});
    unconfigured.search(QStringLiteral("Dune"));
    QCOMPARE(requests, 3);
    QVERIFY(unconfigured.status().contains(QStringLiteral("not configured")));
}

void T14Tests::offlineTranslationWritesUnicodeSrtWithoutOverwriting()
{
    FixtureRoot directory;
    QVERIFY(directory.isValid());
    const auto tools = directory.filePath(QStringLiteral("tools"));
    QVERIFY(QDir().mkpath(tools));
    const auto translator = QDir(tools).filePath(QStringLiteral("argos-translate"));
    QFile script(translator);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nprintf 'translated'\n");
    script.close();
    QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    const auto previousPath = qgetenv("PATH");
    qputenv("PATH", (tools + QLatin1Char(':') + QString::fromLocal8Bit(previousPath)).toLocal8Bit());
    QScopeGuard restorePath([&] { qputenv("PATH", previousPath); });

    const auto source = directory.filePath(QStringLiteral("film.zh.srt"));
    const QByteArray original = QStringLiteral("1\n00:00:01,000 --> 00:00:02,000\n你好，世界\n\n2\n00:00:03,000 --> 00:00:04,000\n再见\n").toUtf8();
    QFile input(source);
    QVERIFY(input.open(QIODevice::WriteOnly));
    QCOMPARE(input.write(original), original.size());
    input.close();
    const auto occupied = directory.filePath(QStringLiteral("film.en.srt"));
    QFile prior(occupied);
    QVERIFY(prior.open(QIODevice::WriteOnly));
    prior.write("keep user subtitle");
    prior.close();

    SubtitleService service(nullptr, directory.filePath(QStringLiteral("managed-cache")));
    const auto id = service.queueTranslation(source, QStringLiteral("zh"), QStringLiteral("en"));
    QVERIFY(!id.isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(service.jobs().first().toMap().value(QStringLiteral("state")) == QStringLiteral("Ready for review"), 5000);
    const auto job = service.jobs().first().toMap();
    const auto quality = job.value(QStringLiteral("qualityReport")).toMap();
    QVERIFY(!quality.value(QStringLiteral("languageQualityCertified")).toBool());
    QCOMPARE(quality.value(QStringLiteral("outputCueCount")).toInt(), 2);
    QVERIFY(quality.value(QStringLiteral("timestampsPreserved")).toBool());
    const auto output = job.value(QStringLiteral("output")).toString();
    QVERIFY(output.endsWith(QStringLiteral("film.en.isolde-1.srt")));
    QVERIFY(QFileInfo::exists(output));
    QFile translated(output);
    QVERIFY(translated.open(QIODevice::ReadOnly));
    const auto outputText = QString::fromUtf8(translated.readAll());
    translated.close();
    QVERIFY(outputText.contains(QStringLiteral("00:00:01,000 --> 00:00:02,000")));
    QVERIFY(outputText.contains(QStringLiteral("translated")));
    QVERIFY(input.open(QIODevice::ReadOnly));
    QCOMPARE(input.readAll(), original);
    QVERIFY(prior.open(QIODevice::ReadOnly));
    QCOMPARE(prior.readAll(), QByteArray("keep user subtitle"));
    QFile metadata(output + QStringLiteral(".isolde.json"));
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    const auto provenance = QJsonDocument::fromJson(metadata.readAll()).object();
    QCOMPARE(provenance.value(QStringLiteral("offlineOnly")).toBool(), true);
    QVERIFY(provenance.value(QStringLiteral("qualityReport")).toObject().contains(QStringLiteral("assessment")));
    QCOMPARE(provenance.value(QStringLiteral("sourceLanguage")).toString(), QStringLiteral("zh"));
    QCOMPARE(provenance.value(QStringLiteral("targetLanguage")).toString(), QStringLiteral("en"));
    QVERIFY(provenance.value(QStringLiteral("sourceSha256")).toString().size() == 64);
    metadata.close();
    QVERIFY(service.reviewText(id).contains(QStringLiteral("translated")));
    auto invalidEdit = QStringLiteral("not an srt");
    QVERIFY(!service.saveReview(id, invalidEdit));
    QVERIFY(service.errorString().contains(QStringLiteral("malformed")));
    auto reviewed = service.reviewText(id);
    reviewed.replace(QStringLiteral("translated"), QStringLiteral("Human reviewed"));
    QVERIFY(service.saveReview(id, reviewed));
    QFile reviewedMetadata(output + QStringLiteral(".isolde.json"));
    QVERIFY(reviewedMetadata.open(QIODevice::ReadOnly));
    const auto reviewedProvenance = QJsonDocument::fromJson(reviewedMetadata.readAll()).object();
    QVERIFY(reviewedProvenance.value(QStringLiteral("reviewedAt")).isString());
    QVERIFY(reviewedProvenance.value(QStringLiteral("qualityReport")).toObject()
                .value(QStringLiteral("reviewedByUser")).toBool());
    reviewedMetadata.close();
    QVERIFY(service.forgetJob(id));
    QCOMPARE(service.jobs().size(), 0);
    QVERIFY(QFileInfo::exists(output));
}

void T14Tests::subtitleQualityReportFlagsUnchangedCuesWithoutCertifyingQuality()
{
    FixtureRoot directory;
    QVERIFY(directory.isValid());
    const auto tools = directory.filePath(QStringLiteral("tools"));
    QVERIFY(QDir().mkpath(tools));
    const auto translator = QDir(tools).filePath(QStringLiteral("argos-translate"));
    QFile script(translator);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nwhile [ \"$#\" -gt 0 ]; do\n"
                 "if [ \"$1\" = \"--text\" ]; then shift; printf '%s' \"$1\"; exit 0; fi\n"
                 "shift\ndone\nexit 2\n");
    script.close();
    QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    const auto previousPath = qgetenv("PATH");
    qputenv("PATH", (tools + QLatin1Char(':') + QString::fromLocal8Bit(previousPath)).toLocal8Bit());
    QScopeGuard restorePath([&] { qputenv("PATH", previousPath); });

    const auto source = directory.filePath(QStringLiteral("same.zh.srt"));
    QFile input(source);
    QVERIFY(input.open(QIODevice::WriteOnly));
    input.write("1\n00:00:01,000 --> 00:00:02,000\nUntranslated source line\n");
    input.close();
    SubtitleService service(nullptr, directory.filePath(QStringLiteral("managed-cache")));
    QVERIFY(!service.queueTranslation(source, QStringLiteral("zh"), QStringLiteral("en")).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(service.jobs().first().toMap().value(QStringLiteral("state")) == QStringLiteral("Ready for review"), 5000);
    const auto report = service.jobs().first().toMap().value(QStringLiteral("qualityReport")).toMap();
    QCOMPARE(report.value(QStringLiteral("unchangedCueCount")).toInt(), 1);
    QCOMPARE(report.value(QStringLiteral("timestampCheck")).toString(), QStringLiteral("passed"));
    QVERIFY(!report.value(QStringLiteral("languageQualityCertified")).toBool());
    QVERIFY(report.value(QStringLiteral("warnings")).toStringList().contains(QStringLiteral("possible-untranslated-cues")));
}

void T14Tests::subtitleInputsAreValidatedAndExplicitlyQueued()
{
    FixtureRoot directory;
    QVERIFY(directory.isValid());
    SubtitleService service(nullptr, directory.filePath(QStringLiteral("cache")));
    QVERIFY(service.queueTranslation(directory.filePath(QStringLiteral("missing.srt")),
                                    QStringLiteral("zh"), QStringLiteral("en")).isEmpty());
    QCOMPARE(service.jobs().size(), 0);
    const auto media = directory.filePath(QStringLiteral("movie.mkv"));
    QFile file(media);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("fixture-media");
    file.close();
    QVERIFY(service.queueGeneration(media, directory.filePath(QStringLiteral("missing.pt"))).isEmpty());
    QVERIFY(!service.installLanguagePack(directory.filePath(QStringLiteral("missing.argosmodel"))));
    QVERIFY(service.languagePackStatus().contains(QStringLiteral("local .argosmodel")));
    const auto downloadedSrt = QStringLiteral("1\n00:00:01,000 --> 00:00:02,000\nHello from repository\n");
    const auto firstImport = service.importOnlineSubtitle(QStringLiteral("Arrival"),
        QStringLiteral("en"), downloadedSrt, 755);
    QVERIFY(!firstImport.isEmpty());
    const auto firstJob = service.jobs().first().toMap();
    const auto importedPath = firstJob.value(QStringLiteral("output")).toString();
    QVERIFY(importedPath.contains(QStringLiteral("online-subtitles")));
    QVERIFY(QFileInfo::exists(importedPath));
    QFile provenance(importedPath + QStringLiteral(".isolde.json"));
    QVERIFY(provenance.open(QIODevice::ReadOnly));
    const auto metadata = QJsonDocument::fromJson(provenance.readAll()).object();
    QCOMPARE(metadata.value(QStringLiteral("offlineOnly")).toBool(), false);
    QCOMPARE(metadata.value(QStringLiteral("repository")).toString(), QStringLiteral("OpenSubtitles.com"));
    QCOMPARE(metadata.value(QStringLiteral("source")).toString(), QStringLiteral("OpenSubtitles.com file 755 · Arrival"));
    QVERIFY(service.importOnlineSubtitle(QStringLiteral("Arrival"), QStringLiteral("en"),
        QStringLiteral("not an srt"), 755).isEmpty());
    const auto secondImport = service.importOnlineSubtitle(QStringLiteral("Arrival"),
        QStringLiteral("en"), downloadedSrt, 756);
    QVERIFY(!secondImport.isEmpty());
    QVERIFY(service.jobs().at(1).toMap().value(QStringLiteral("output")).toString() != importedPath);
    QCOMPARE(service.jobs().size(), 0);
    QVERIFY(service.errorString().toLower().contains(QStringLiteral("model downloads are never started")));
}

void T14Tests::offlineAudioGenerationUsesOnlyAnInstalledLocalModel()
{
    FixtureRoot directory;
    QVERIFY(directory.isValid());
    const auto tools = directory.filePath(QStringLiteral("tools"));
    QVERIFY(QDir().mkpath(tools));
    const auto whisper = QDir(tools).filePath(QStringLiteral("whisper-cli"));
    QFile script(whisper);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/usr/bin/python3\n"
                 "import os,sys\n"
                 "args=sys.argv[1:]\n"
                 "model=args[args.index('-m')+1]\n"
                 "prefix=args[args.index('-of')+1]\n"
                 "assert os.path.isfile(model) and '-tr' in args and '-osrt' in args\n"
                 "open(prefix+'.srt','w',encoding='utf-8').write('1\\n00:00:01,000 --> 00:00:03,000\\nGenerated offline\\n')\n");
    script.close();
    QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    const auto previousPath = qgetenv("PATH");
    qputenv("PATH", (tools + QLatin1Char(':') + QString::fromLocal8Bit(previousPath)).toLocal8Bit());
    QScopeGuard restorePath([&] { qputenv("PATH", previousPath); });

    const auto media = directory.filePath(QStringLiteral("episode.mkv"));
    QFile input(media);
    QVERIFY(input.open(QIODevice::WriteOnly));
    const QByteArray original("fixture-media");
    QCOMPARE(input.write(original), original.size());
    input.close();
    const auto model = directory.filePath(QStringLiteral("ggml-small.bin"));
    QFile modelFile(model);
    QVERIFY(modelFile.open(QIODevice::WriteOnly));
    modelFile.write("installed fixture model");
    modelFile.close();

    SubtitleService service(nullptr, directory.filePath(QStringLiteral("managed-cache")));
    const auto id = service.queueGeneration(media, model);
    QVERIFY(!id.isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(service.jobs().first().toMap().value(QStringLiteral("state")) == QStringLiteral("Ready for review"), 5000);
    const auto job = service.jobs().first().toMap();
    QFile generated(job.value(QStringLiteral("output")).toString());
    QVERIFY(generated.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(generated.readAll()).contains(QStringLiteral("Generated offline")));
    QVERIFY(input.open(QIODevice::ReadOnly));
    QCOMPARE(input.readAll(), original);
    QFile metadata(job.value(QStringLiteral("output")).toString() + QStringLiteral(".isolde.json"));
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    const auto provenance = QJsonDocument::fromJson(metadata.readAll()).object();
    QVERIFY(provenance.value(QStringLiteral("tool")).toString().contains(QStringLiteral("whisper.cpp")));
    QCOMPARE(provenance.value(QStringLiteral("offlineOnly")).toBool(), true);
}

void T14Tests::languagePacksCanBeInspectedAndInstalledFromLocalFiles()
{
    FixtureRoot directory;
    QVERIFY(directory.isValid());
    const auto python = directory.filePath(QStringLiteral("python3"));
    QFile interpreter(python);
    QVERIFY(interpreter.open(QIODevice::WriteOnly));
    interpreter.write("#!/bin/sh\ncase \"$2\" in\n"
                      "*get_installed_languages*) printf '{\"pairs\":[{\"source\":\"en\",\"target\":\"es\"}]}' ;;\n"
                      "*install_from_path*) [ -f \"$3\" ] || exit 4; printf '{\"pairs\":[{\"source\":\"en\",\"target\":\"es\"}]}' ;;\n"
                      "*) exit 5 ;;\nesac\n");
    interpreter.close();
    QVERIFY(interpreter.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    const auto packagePath = directory.filePath(QStringLiteral("en_es.argosmodel"));
    QFile package(packagePath);
    QVERIFY(package.open(QIODevice::WriteOnly));
    QCOMPARE(package.write(QByteArray::fromHex("504b0304")), qint64(4));
    package.close();

    SubtitleService service(nullptr, directory.filePath(QStringLiteral("cache")), python);
    QTRY_COMPARE_WITH_TIMEOUT(service.languagePacks().size(), 1, 5000);
    QCOMPARE(service.languagePacks().first().toMap().value(QStringLiteral("source")).toString(), QStringLiteral("en"));
    QVERIFY(service.installLanguagePack(packagePath));
    QTRY_VERIFY_WITH_TIMEOUT(service.languagePackStatus().contains(QStringLiteral("installed offline Argos")), 5000);
    QCOMPARE(service.languagePacks().size(), 1);
}

void T14Tests::subtitleQueueHasRemoteKeyboardAndBackBehavior()
{
    FixtureRoot directory;
    QVERIFY(directory.isValid());
    SubtitleService service(nullptr, directory.filePath(QStringLiteral("cache")));
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("inputManager"), &input);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(T14_SUBTITLE_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QVariantMap properties{
        {QStringLiteral("service"), QVariant::fromValue(static_cast<QObject *>(&service))},
        {QStringLiteral("open"), true},
        {QStringLiteral("width"), 1280},
        {QStringLiteral("height"), 720}
    };
    std::unique_ptr<QObject> root(component.createWithInitialProperties(properties));
    QVERIFY2(root, qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(1280, 720);
    qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());
    window.show();
    auto command = [&](int value) {
        return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value));
    };
    QVERIFY(command(InputManager::Confirm));
    QVERIFY(root->property("keyboardOpen").toBool());
    auto *keyboard = root->findChild<QObject *>(QStringLiteral("linguistKeyboard"));
    QVERIFY(keyboard);
    QVERIFY(command(InputManager::Back));
    QVERIFY(!root->property("keyboardOpen").toBool());
    QVERIFY(root->property("open").toBool());
    QSignalSpy closed(root.get(), SIGNAL(closeRequested()));
    QVERIFY(command(InputManager::Back));
    QCOMPARE(closed.count(), 1);
}

void T14Tests::subtitleTrackPickerUsesRemoteControlsAndManagedClient()
{
    FakeSubtitleClient client;
    InputManager input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("inputManager"), &input);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(T14_SUBTITLE_TRACK_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QVariantMap properties{
        {QStringLiteral("kodiClient"), QVariant::fromValue(static_cast<QObject *>(&client))},
        {QStringLiteral("open"), true},
        {QStringLiteral("width"), 1280},
        {QStringLiteral("height"), 720}
    };
    std::unique_ptr<QObject> root(component.createWithInitialProperties(properties));
    QVERIFY2(root, qPrintable(component.errorString()));
    QTRY_COMPARE(client.refreshes, 1);
    auto command = [&](int value) {
        return QMetaObject::invokeMethod(root.get(), "handleCommand", Q_ARG(QVariant, value));
    };
    QVERIFY(command(InputManager::NavigateDown));
    QVERIFY(command(InputManager::Confirm));
    QCOMPARE(client.selected, 4);
    QVERIFY(command(InputManager::NavigateUp));
    QVERIFY(command(InputManager::Confirm));
    QCOMPARE(client.selected, -1);
    QVERIFY(root->property("open").toBool());
    QSignalSpy closed(root.get(), SIGNAL(closeRequested()));
    QVERIFY(command(InputManager::Back));
    QCOMPARE(closed.count(), 1);
}

QTEST_MAIN(T14Tests)
#include "tst_t14.moc"
