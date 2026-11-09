#include "media/SerialCommandWriter.h"

#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

class FakeSerialDevice final : public QIODevice {
public:
    explicit FakeSerialDevice(QObject *parent = nullptr) : QIODevice(parent) {}
    QByteArray written;
    qint64 writeLimit = -1;
    int completionDelayMs = -1;
    bool completeSynchronously = false;

    bool openForWriting() { return open(QIODevice::WriteOnly); }

protected:
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *data, qint64 size) override
    {
        const auto count = writeLimit < 0 ? size : qMin(size, writeLimit);
        written.append(data, count);
        if (completeSynchronously)
            emit bytesWritten(count);
        else if (completionDelayMs >= 0)
            QTimer::singleShot(completionDelayMs, this, [this, count] { emit bytesWritten(count); });
        return count;
    }
};

class SerialCommandWriterTests final : public QObject {
    Q_OBJECT
private slots:
    void completesOnlyAfterAllBytesAreWritten();
    void timesOutAndCanRetryAfterReconnect();
    void cancellationIgnoresLateCompletion();
    void reportsTransportFailureAndShortWrite();
    void deviceDisconnectIsRecoverable();
    void closeBeforeWriteCompletionFails();
};

void SerialCommandWriterTests::completesOnlyAfterAllBytesAreWritten()
{
    SerialCommandWriter writer;
    FakeSerialDevice device;
    QVERIFY(device.openForWriting());
    device.completionDelayMs = 10;
    QSignalSpy completed(&writer, &SerialCommandWriter::completed);
    QVERIFY(writer.start(&device, QByteArray("HDMI 3\r"), 200));
    QCOMPARE(writer.state(), QString("sending"));
    QCOMPARE(writer.pendingBytes(), qint64(7));
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(device.written, QByteArray("HDMI 3\r"));
    QCOMPARE(writer.state(), QString("sent"));
    QCOMPARE(writer.pendingBytes(), qint64(0));
    QVERIFY(completed.takeFirst().at(0).toLongLong() >= 0);

    FakeSerialDevice synchronous;
    QVERIFY(synchronous.openForWriting());
    synchronous.completeSynchronously = true;
    QVERIFY(writer.start(&synchronous, QByteArray("1\r"), 200));
    QCOMPARE(writer.state(), QString("sent"));
}

void SerialCommandWriterTests::timesOutAndCanRetryAfterReconnect()
{
    SerialCommandWriter writer;
    FakeSerialDevice stalled;
    QVERIFY(stalled.openForWriting());
    QSignalSpy failed(&writer, &SerialCommandWriter::failed);
    QVERIFY(writer.start(&stalled, QByteArray("1\r"), 20));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 200);
    QVERIFY(failed.first().first().toString().contains("time"));
    QCOMPARE(writer.state(), QString("error"));

    FakeSerialDevice reconnected;
    QVERIFY(reconnected.openForWriting());
    reconnected.completionDelayMs = 0;
    QSignalSpy completed(&writer, &SerialCommandWriter::completed);
    QVERIFY(writer.start(&reconnected, QByteArray("2\r"), 200));
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(reconnected.written, QByteArray("2\r"));
    QCOMPARE(writer.state(), QString("sent"));
}

void SerialCommandWriterTests::cancellationIgnoresLateCompletion()
{
    SerialCommandWriter writer;
    FakeSerialDevice device;
    QVERIFY(device.openForWriting());
    device.completionDelayMs = 20;
    QSignalSpy completed(&writer, &SerialCommandWriter::completed);
    QVERIFY(writer.start(&device, QByteArray("8\r"), 200));
    writer.cancel();
    QCOMPARE(writer.state(), QString("cancelled"));
    QTest::qWait(35);
    QCOMPARE(completed.size(), 0);
    QCOMPARE(writer.state(), QString("cancelled"));
}

void SerialCommandWriterTests::reportsTransportFailureAndShortWrite()
{
    SerialCommandWriter writer;
    FakeSerialDevice device;
    QVERIFY(device.openForWriting());
    QSignalSpy failed(&writer, &SerialCommandWriter::failed);
    QVERIFY(writer.start(&device, QByteArray("5\r"), 200));
    writer.fail("fixture transport disconnected");
    QCOMPARE(writer.state(), QString("error"));
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.first().first().toString().contains("disconnected"));

    FakeSerialDevice partial;
    QVERIFY(partial.openForWriting());
    partial.writeLimit = 1;
    QVERIFY(!writer.start(&partial, QByteArray("6\r"), 200));
    QCOMPARE(writer.state(), QString("error"));
    QCOMPARE(failed.size(), 2);
}

void SerialCommandWriterTests::deviceDisconnectIsRecoverable()
{
    SerialCommandWriter writer;
    auto *disconnected = new FakeSerialDevice;
    QVERIFY(disconnected->openForWriting());
    QSignalSpy failed(&writer, &SerialCommandWriter::failed);
    QVERIFY(writer.start(disconnected, QByteArray("4\r"), 200));
    delete disconnected;
    QTRY_COMPARE(failed.size(), 1);
    QVERIFY(failed.first().first().toString().contains("disconnected"));
    QCOMPARE(writer.state(), QString("error"));

    FakeSerialDevice reconnected;
    QVERIFY(reconnected.openForWriting());
    reconnected.completionDelayMs = 0;
    QVERIFY(writer.start(&reconnected, QByteArray("7\r"), 200));
    QTRY_COMPARE(writer.state(), QString("sent"));
    QCOMPARE(reconnected.written, QByteArray("7\r"));
}

void SerialCommandWriterTests::closeBeforeWriteCompletionFails()
{
    SerialCommandWriter writer;
    FakeSerialDevice device;
    QVERIFY(device.openForWriting());
    QSignalSpy failed(&writer, &SerialCommandWriter::failed);
    QVERIFY(writer.start(&device, QByteArray("3\r"), 200));
    device.close();
    QCOMPARE(writer.state(), QString("error"));
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.first().first().toString().contains("closed"));
}

QTEST_GUILESS_MAIN(SerialCommandWriterTests)
#include "tst_serial_command_writer.moc"
