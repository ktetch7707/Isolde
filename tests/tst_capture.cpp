#include "media/CaptureController.h"
#include <QtTest>

class CaptureTests final : public QObject {
    Q_OBJECT
private slots:
    void encodesSwitchCommands();
    void rejectsUnsafeOrAmbiguousCommands();
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

QTEST_APPLESS_MAIN(CaptureTests)
#include "tst_capture.moc"