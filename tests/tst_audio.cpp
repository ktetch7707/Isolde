#include "settings/AudioSettings.h"
#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>

class AudioTests : public QObject {
    Q_OBJECT
    QTemporaryDir config;
    static void write(const QString &path, const QByteArray &data) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), data.size());
    }
    static QString mixer(QTemporaryDir &dir, const QString &name = "wpctl") {
        const auto path = dir.filePath(name);
        if (!QFile::copy(AUDIO_FIXTURE, path)) return {};
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }
private slots:
    void initTestCase() {
        QCoreApplication::setApplicationName("AudioBackendTests");
        QCoreApplication::setOrganizationName({}); // Production must not depend on this.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    }
    void init() { QSettings(QSettings::IniFormat, QSettings::UserScope, "Isolde", "AudioBackendTests").clear(); }
    void startup_data() {
        QTest::addColumn<QString>("policy"); QTest::addColumn<int>("volume"); QTest::addColumn<bool>("mute");
        QTest::newRow("remember") << "remember" << 42 << false;
        QTest::newRow("fixed") << "fixed" << 25 << false;
        QTest::newRow("muted") << "muted" << 42 << true;
    }
    void startup() {
        QFETCH(QString, policy); QFETCH(int, volume); QFETCH(bool, mute);
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Isolde", "AudioBackendTests");
        settings.setValue("audio/volume", 42); settings.setValue("audio/startupVolume", 25);
        settings.setValue("audio/startupPolicy", policy); settings.sync();
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir));
        audio.initialize(); QTRY_VERIFY(audio.mixerAvailable());
        QCOMPARE(audio.volume(), volume); QCOMPARE(audio.muted(), mute);
        audio.initialize(); // Initialization is idempotent.
        AudioSettings restored(nullptr, mixerPathUnused());
        QCOMPARE(restored.volume(), volume);
        if (mute) { QFile log(dir.filePath("commands")); QVERIFY(log.open(QIODevice::ReadOnly)); QVERIFY(log.readAll().startsWith("set-mute")); }
    }
    void confirmedReadAndFailure_data() {
        QTest::addColumn<QString>("backend");
        QTest::newRow("pipewire") << "wpctl"; QTest::newRow("pulse") << "pactl";
    }
    void confirmedReadAndFailure() {
        QFETCH(QString, backend);
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir, backend));
        audio.refreshBackend(); QTRY_VERIFY(audio.mixerAvailable());
        audio.adjustVolume(5); audio.adjustVolume(5); audio.adjustVolume(5);
        QCOMPARE(audio.volume(), 50); // No optimistic success before readback.
        QTRY_COMPARE(audio.volume(), 65);
        audio.toggleMute(); QTRY_VERIFY(audio.muted());
        write(dir.filePath("state.json"), "{\"volume\":33,\"mute\":false}");
        audio.refreshBackend(); QTRY_COMPARE(audio.volume(), 33); QVERIFY(!audio.muted());
        write(dir.filePath("mode"), "fail"); audio.adjustVolume(10);
        QTRY_VERIFY(!audio.mixerAvailable()); QCOMPARE(audio.volume(), 33);
        write(dir.filePath("mode"), ""); audio.refreshBackend(); QTRY_VERIFY(audio.mixerAvailable());
        audio.nextVolumeMode(); QCOMPARE(audio.volumeMode(), QString("isolde"));
    }
    void timeoutDoesNotBlock() {
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir));
        write(dir.filePath("mode"), "hang");
        QElapsedTimer clock; clock.start(); audio.refreshBackend(); QVERIFY(clock.elapsed() < 100);
        QTRY_VERIFY_WITH_TIMEOUT(audio.backendStatus().contains("timed out"), 4000);
        QCOMPARE(audio.volume(), 50);
    }
    void rejectsMalformed() {
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir));
        write(dir.filePath("mode"), "malformed"); audio.refreshBackend();
        QTRY_VERIFY(audio.backendStatus().contains("Invalid")); QVERIFY(!audio.mixerAvailable());
    }
    void choosesOnlyDiscoveredPlaybackOutputs() {
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir));
        audio.refreshBackend(); QTRY_COMPARE(audio.outputs().size(), 2);
        QCOMPARE(audio.outputs()[0].toMap().value("name").toString(), QString("TV HDMI"));
        QVERIFY(!audio.selectOutput("88")); // Never select a microphone.
        QVERIFY(!audio.selectOutput("99")); // Never select a video node.
        QVERIFY(audio.selectOutput("77"));
        QTRY_VERIFY(audio.outputs()[1].toMap().value("active").toBool());
        write(dir.filePath("mode"), "unplug"); audio.refreshBackend();
        QTRY_COMPARE(audio.outputs().size(), 1);
        QVERIFY(!audio.selectOutput("77"));
    }
    void startupWaitsForAudioService() {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Isolde", "AudioBackendTests");
        settings.setValue("audio/startupPolicy", "muted"); settings.sync();
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir));
        write(dir.filePath("mode"), "fail"); audio.initialize();
        QTRY_VERIFY(audio.backendStatus().contains("rejected"));
        write(dir.filePath("mode"), "");
        QTRY_VERIFY_WITH_TIMEOUT(audio.mixerAvailable(), 6000);
        QVERIFY(audio.muted());
    }
    void selectionMustBeConfirmed() {
        QTemporaryDir dir; AudioSettings audio(nullptr, mixer(dir));
        audio.refreshBackend(); QTRY_COMPARE(audio.outputs().size(), 2);
        write(dir.filePath("mode"), "ignore-selection");
        QVERIFY(audio.selectOutput("77"));
        QTRY_VERIFY(audio.outputStatus().contains("not confirmed"));
        QVERIFY(audio.outputs()[0].toMap().value("active").toBool());
        QVERIFY(!audio.outputs()[1].toMap().value("active").toBool());
    }
    void soundTestPlaybackAndStop() {
        QTemporaryDir dir;
        const auto player = dir.filePath("test-player");
        QVERIFY(QFile::copy(AUDIO_PLAYER_FIXTURE, player));
        QVERIFY(QFile::setPermissions(player, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        AudioSettings audio(nullptr, mixer(dir), player);
        audio.refreshBackend(); QTRY_COMPARE(audio.outputs().size(), 2);
        audio.playTestSound(); QVERIFY(audio.testPlaying());
        QVERIFY(!audio.selectOutput("77"));
        QTRY_VERIFY(!audio.testPlaying());
        QVERIFY(audio.testStatus().contains("finished"));
        QVERIFY(QFile::exists(dir.filePath("validated-tone")));
        QCOMPARE(audio.volume(), 50); QVERIFY(!audio.muted());
        audio.playTestSound(); QVERIFY(audio.testPlaying());
        audio.stopTestSound(); QVERIFY(!audio.testPlaying());
        QCOMPARE(audio.testStatus(), QString("Sound test stopped."));
    }
    void soundTestRespectsMuteAndMissingPlayer() {
        QTemporaryDir dir;
        AudioSettings audio(nullptr, mixer(dir), "/nonexistent/player");
        audio.refreshBackend(); QTRY_COMPARE(audio.outputs().size(), 2);
        audio.playTestSound(); QTRY_VERIFY(!audio.testPlaying());
        QVERIFY(audio.testStatus().contains("Could not start"));
        audio.toggleMute(); QTRY_VERIFY(audio.muted());
        // Muted state can be published before the async confirmation queue
        // becomes idle. Retry the safe, muted request until confirmation
        // completes instead of assuming the backend finishes within 100 ms.
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            audio.playTestSound();
            return audio.testStatus().contains("Unmute");
        })(), 5000);
        QVERIFY(!audio.testPlaying());
        QVERIFY(audio.muted());
    }
private:
    static QString mixerPathUnused() { return "/nonexistent/test-mixer"; }
};
QTEST_GUILESS_MAIN(AudioTests)
#include "tst_audio.moc"
