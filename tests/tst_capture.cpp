#include "media/CaptureController.h"
#include "media/AudioCaptureAdapter.h"
#include <QtTest>

class FakeAudioAdapter final : public AudioCaptureAdapter {
public:
    QVariantList available;
    int stops = 0;
    QVariantList devices() const override { return available; }
    bool monitoringSupported() const override { return true; }
    QString name() const override { return "fixture audio"; }
    bool configure(QMediaCaptureSession *, const QString &) override { return true; }
    void stop(QMediaCaptureSession *) override { ++stops; }
    void publish() { emit devicesChanged(); }
};

class CaptureTests final : public QObject {
    Q_OBJECT
private slots:
    void encodesSwitchCommands();
    void rejectsUnsafeOrAmbiguousCommands();
    void validatesInputLabels();
    void injectedAudioDiscoveryAndHotplug();
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

QTEST_GUILESS_MAIN(CaptureTests)
#include "tst_capture.moc"