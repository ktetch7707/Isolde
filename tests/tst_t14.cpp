#include "media/OnlineDiscovery.h"
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
#include <QTcpServer>
#include <QTcpSocket>
#include <QScopeGuard>
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

class T14Tests final : public QObject {
    Q_OBJECT
private slots:
    void discoveryIsOptInRegionAwareAndDoesNotClaimEntitlement();
    void offlineTranslationWritesUnicodeSrtWithoutOverwriting();
    void offlineAudioGenerationUsesOnlyAnInstalledLocalModel();
    void subtitleInputsAreValidatedAndExplicitlyQueued();
    void subtitleQueueHasRemoteKeyboardAndBackBehavior();
};

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
                            {QStringLiteral("provider_id"), 9}, {QStringLiteral("provider_name"), QStringLiteral("Fixture Service")}
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

    discovery.loadAvailability(QStringLiteral("movie"), first.value(QStringLiteral("providerId")).toInt());
    QTRY_COMPARE(discovery.availability().size(), 1);
    QVERIFY(requestLines.last().contains(QStringLiteral("/movie/501/watch/providers")));
    QVERIFY(!discovery.availability().first().toMap().value(QStringLiteral("entitled")).toBool());
    QVERIFY(discovery.status().contains(QStringLiteral("do not prove access")));

    holdNext = true;
    discovery.search(QStringLiteral("Dune"));
    QTRY_COMPARE(requests, 3);
    settings.setWebEnabled(false);
    QVERIFY(!discovery.busy());
    QVERIFY(discovery.results().isEmpty());
    QVERIFY(discovery.availability().isEmpty());
    QTest::qWait(100);
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
    const auto output = job.value(QStringLiteral("output")).toString();
    QVERIFY(output.endsWith(QStringLiteral("film.en.isolde-1.srt")));
    QVERIFY(QFileInfo::exists(output));
    QFile translated(output);
    QVERIFY(translated.open(QIODevice::ReadOnly));
    const auto outputText = QString::fromUtf8(translated.readAll());
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
    QCOMPARE(provenance.value(QStringLiteral("sourceLanguage")).toString(), QStringLiteral("zh"));
    QCOMPARE(provenance.value(QStringLiteral("targetLanguage")).toString(), QStringLiteral("en"));
    QVERIFY(provenance.value(QStringLiteral("sourceSha256")).toString().size() == 64);
    QVERIFY(service.reviewText(id).contains(QStringLiteral("translated")));
    auto invalidEdit = QStringLiteral("not an srt");
    QVERIFY(!service.saveReview(id, invalidEdit));
    QVERIFY(service.errorString().contains(QStringLiteral("malformed")));
    QVERIFY(service.forgetJob(id));
    QCOMPARE(service.jobs().size(), 0);
    QVERIFY(QFileInfo::exists(output));
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

QTEST_MAIN(T14Tests)
#include "tst_t14.moc"
