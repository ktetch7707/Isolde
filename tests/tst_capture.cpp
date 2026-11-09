#include "media/CaptureController.h"
#include "media/AudioCaptureAdapter.h"
#include "media/VideoCaptureAdapter.h"
#include <QMediaCaptureSession>
#include <QtTest>

class FakeAudioAdapter final : public AudioCaptureAdapter {
public:
    QVariantList available;
    int stops = 0;
    QString current;
    QVariantList devices() const override { return available; }
    bool monitoringSupported() const override { return true; }
    QString name() const override { return "fixture audio"; }
    bool configure(QMediaCaptureSession *, const QString &id) override {
        current = id;
        if (id.isEmpty()) return true;
        for (const auto &entry : available)
            if (entry.toMap().value("id").toString() == id) return true;
        return false;
    }
    void stop(QMediaCaptureSession *) override { ++stops; }
    void publish() { emit devicesChanged(); }
};

class FakeVideoAdapter final : public VideoCaptureAdapter {
public:
    QVariantList available;
    QVariantList formatList;
    QMediaCaptureSession session;
    int starts = 0;
    int stops = 0;
    bool failStart = false;
    QObject *output = nullptr;
    QVariantList devices() const override { return available; }
    QVariantList formats(const QString &id) const override {
        return id == "fixture-video" && !available.isEmpty() ? formatList : QVariantList{};
    }
    QString name() const override { return "fixture video"; }
    void setOutput(QObject *value) override { output = value; }
    bool start(const QString &, const QString &) override { ++starts; return !failStart; }
    void stop() override { ++stops; }
    QMediaCaptureSession *audioSession() override { return &session; }
    void publish() { emit devicesChanged(); }
    void frame() { emit frameReceived(); }
    void fail() { emit errorOccurred("fixture failure"); }
};

class CaptureTests final : public QObject {
    Q_OBJECT
private slots:
    void encodesSwitchCommands();
    void rejectsUnsafeOrAmbiguousCommands();
    void validatesInputLabels();
    void injectedAudioDiscoveryAndHotplug();
    void injectablePreviewLifecycle();
};

void CaptureTests::encodesSwitchCommands()
{
    QCOMPARE(CaptureController::commandForInput("{input}\\r", 3), QByteArray("3\r"));
    QCOMPARE(CaptureController::commandForInput("HDMI {input}\\r\\n", 8), QByteArray("HDMI 8\r\n"));
    QCOMPARE(CaptureController::commandForInput("SW {input}\\\\r", 2), QByteArray("SW 2\\r"));
}

void CaptureTests::rejectsUnsafeOrAmbiguousCommands()
{
    QString error;
    QVERIFY(CaptureController::commandForInput("input 1\\r", 1, &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(CaptureController::commandForInput("{input} {input}\\r", 1).isEmpty());
    QVERIFY(CaptureController::commandForInput("{input}\\x", 1).isEmpty());
    QVERIFY(CaptureController::commandForInput("{input}\n", 1).isEmpty());
    QVERIFY(CaptureController::commandForInput(QString::fromUtf8("HDMI é {input}\\r"), 1).isEmpty());
    QVERIFY(CaptureController::commandForInput(QString(130, QLatin1Char('A')) + "{input}", 1).isEmpty());
    QVERIFY(CaptureController::commandForInput("{input}", 0).isEmpty());
    QVERIFY(CaptureController::commandForInput("{input}", 9).isEmpty());
}

void CaptureTests::validatesInputLabels()
{
    QVERIFY(CaptureController::isValidInputLabel("Living room console"));
    QVERIFY(CaptureController::isValidInputLabel(QString(40, QLatin1Char('A'))));
    QVERIFY(!CaptureController::isValidInputLabel(""));
    QVERIFY(!CaptureController::isValidInputLabel("   "));
    QVERIFY(!CaptureController::isValidInputLabel(QString(41, QLatin1Char('A'))));
    QVERIFY(!CaptureController::isValidInputLabel("Two\nlines"));
    QVERIFY(!CaptureController::isValidInputLabel("Two\twords"));
    QVERIFY(!CaptureController::isValidInputLabel(QString(QChar::Null)));
    QVERIFY(!CaptureController::isValidInputLabel(QString(QChar(0x7f))));
    QVERIFY(!CaptureController::isValidInputLabel(QString(QChar(0x85))));
}

void CaptureTests::injectedAudioDiscoveryAndHotplug()
{
    FakeAudioAdapter audio;
    audio.available = {QVariantMap{{"id", "fixture-a"}, {"name", "Fixture audio"}}};
    {
        CaptureController controller(&audio, nullptr);
        QCOMPARE(controller.audioDevices().size(), 1);
        QCOMPARE(controller.audioDevices().first().toMap().value("id").toString(), QString("fixture-a"));
        QCOMPARE(controller.capabilities().value("audioAdapter").toString(), QString("fixture audio"));
        QVERIFY(controller.capabilities().value("audioMonitoringSupported").toBool());
        QVERIFY(!controller.setAudioDevice("missing"));
        audio.available.clear();
        audio.publish();
        QVERIFY(controller.audioDevices().isEmpty());
        audio.available = {QVariantMap{{"id", "fixture-b"}, {"name", "Reconnected audio"}}};
        audio.publish();
        QCOMPARE(controller.audioDevices().first().toMap().value("id").toString(), QString("fixture-b"));
        controller.stopPreview();
        QVERIFY(audio.stops > 0);
    }
}

void CaptureTests::injectablePreviewLifecycle()
{
    FakeVideoAdapter video;
    FakeAudioAdapter audio;
    video.available = {QVariantMap{{"id", "fixture-video"}, {"name", "Fixture video"}}};
    video.formatList = {QVariantMap{{"key", "fixture-format"}, {"name", "Fixture format"}}};
    audio.available = {QVariantMap{{"id", "fixture-a"}, {"name", "Fixture audio"}}};
    CaptureController controller(&video, &audio, nullptr);
    const auto savedVideo = controller.captureDeviceId();
    const auto savedAudio = controller.audioDeviceId();
    QObject surface;
    controller.setVideoOutput(&surface);
    QCOMPARE(controller.capabilities().value("captureAdapter").toString(), QString("fixture video"));
    QVERIFY(!controller.startPreviewFor("fixture-video", "invalid-format", "fixture-a"));
    QCOMPARE(video.starts, 0);
    QVERIFY(!controller.startPreviewFor("fixture-video", "fixture-format", "missing"));
    QCOMPARE(video.starts, 0);
    QVERIFY(controller.startPreviewFor("fixture-video", "fixture-format", "fixture-a"));
    QCOMPARE(audio.current, QString("fixture-a"));
    QCOMPARE(video.starts, 1);
    QCOMPARE(controller.captureDeviceId(), savedVideo);
    QCOMPARE(controller.audioDeviceId(), savedAudio);
    QCOMPARE(controller.state(), QString("awaiting-frames"));
    video.frame();
    QCOMPARE(controller.state(), QString("receiving-frames"));
    QVERIFY(!controller.startPreviewFor("fixture-video", "fixture-format", "missing"));
    QVERIFY(controller.previewActive());
    QCOMPARE(video.starts, 1);
    audio.available.clear();
    audio.publish();
    QCOMPARE(controller.state(), QString("audio-missing"));
    audio.available = {QVariantMap{{"id", "fixture-a"}, {"name", "Fixture audio"}}};
    audio.publish();
    QCOMPARE(audio.current, QString("fixture-a"));
    video.frame();
    QCOMPARE(controller.state(), QString("receiving-frames"));
    controller.stopPreview();
    QVERIFY(!controller.previewActive());
    video.frame();
    QVERIFY(controller.state() != "receiving-frames");
    QVERIFY(controller.startPreviewFor("fixture-video", "", ""));
    video.fail();
    QVERIFY(!controller.previewActive());
    QCOMPARE(controller.state(), QString("error"));
    QVERIFY(controller.startPreviewFor("fixture-video", "", ""));
    video.available.clear();
    video.publish();
    QVERIFY(!controller.previewActive());
    QCOMPARE(controller.state(), QString("device-missing"));
    video.available = {QVariantMap{{"id", "fixture-video"}, {"name", "Fixture video"}}};
    video.publish();
    video.failStart = true;
    QVERIFY(!controller.startPreviewFor("fixture-video", "", ""));
    QVERIFY(!controller.previewActive());
    video.failStart = false;
    QVERIFY(controller.startPreviewFor("fixture-video", "", ""));
    controller.setVideoOutput(nullptr);
    QVERIFY(!controller.previewActive());
    QVERIFY(video.output == nullptr);
}

QTEST_GUILESS_MAIN(CaptureTests)
#include "tst_capture.moc"