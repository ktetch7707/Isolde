#pragma once

#include <QAudioDevice>
#include <QAudioInput>
#include <QAudioOutput>
#include <QCamera>
#include <QCameraDevice>
#include <QElapsedTimer>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSerialPort>
#include <QTimer>
#include <QVariantList>
#include <QVideoSink>

class CaptureController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList videoDevices READ videoDevices NOTIFY changed)
    Q_PROPERTY(QVariantList videoFormats READ videoFormats NOTIFY changed)
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY changed)
    Q_PROPERTY(QVariantList serialPorts READ serialPorts NOTIFY changed)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY changed)
    Q_PROPERTY(QString captureDeviceId READ captureDeviceId NOTIFY changed)
    Q_PROPERTY(QString videoFormatKey READ videoFormatKey NOTIFY changed)
    Q_PROPERTY(QString audioDeviceId READ audioDeviceId NOTIFY changed)
    Q_PROPERTY(QString serialPortName READ serialPortName NOTIFY changed)
    Q_PROPERTY(QString commandTemplate READ commandTemplate NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString switchStatus READ switchStatus NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool previewActive READ previewActive NOTIFY changed)
    Q_PROPERTY(int selectedInput READ selectedInput NOTIFY changed)
    Q_PROPERTY(int activeInput READ activeInput NOTIFY changed)
    Q_PROPERTY(int baudRate READ baudRate NOTIFY changed)
public:
    explicit CaptureController(QObject *parent = nullptr);
    ~CaptureController() override;

    QVariantList videoDevices() const;
    QVariantList videoFormats() const;
    QVariantList audioDevices() const;
    QVariantList serialPorts() const;
    QVariantList inputs() const;
    QString captureDeviceId() const { return m_captureDeviceId; }
    QString videoFormatKey() const { return m_videoFormatKey; }
    QString audioDeviceId() const { return m_audioDeviceId; }
    QString serialPortName() const { return m_serialPortName; }
    QString commandTemplate() const { return m_commandTemplate; }
    QString status() const { return m_status; }
    QString switchStatus() const { return m_switchStatus; }
    QString state() const { return m_state; }
    bool previewActive() const { return m_previewRequested; }
    int selectedInput() const { return m_selectedInput; }
    int activeInput() const { return m_activeInput; }
    int baudRate() const { return m_baudRate; }

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void setVideoOutput(QObject *output);
    Q_INVOKABLE bool setCaptureDevice(const QString &id);
    Q_INVOKABLE bool setVideoFormat(const QString &key);
    Q_INVOKABLE bool setAudioDevice(const QString &id);
    Q_INVOKABLE bool setSerialPort(const QString &portName);
    Q_INVOKABLE bool setBaudRate(int baudRate);
    Q_INVOKABLE bool setCommandTemplate(const QString &value);
    Q_INVOKABLE bool setInputLabel(int input, const QString &label);
    Q_INVOKABLE void selectInput(int input);
    Q_INVOKABLE bool canSwitchInput(int input) const;
    Q_INVOKABLE bool startPreview();
    Q_INVOKABLE void stopPreview();
    Q_INVOKABLE bool switchInput(int input);

    static QByteArray commandForInput(const QString &commandTemplate, int input, QString *error = nullptr);

signals:
    void changed();

private:
    void loadSettings();
    bool saveValue(const QString &key, const QVariant &value);
    void updateStatusForConfiguration();
    void configureAudio();
    QString inputLabel(int input) const;
    QString formatKey(const QCameraFormat &format) const;
    void setStatus(const QString &state, const QString &message);
    void serialWriteFailed(const QString &message);

    QMediaDevices m_mediaDevices;
    QMediaCaptureSession m_captureSession;
    QCamera *m_camera = nullptr;
    QAudioInput *m_audioInput = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QPointer<QObject> m_videoOutput;
    QMetaObject::Connection m_frameConnection;
    QMetaObject::Connection m_outputDestroyedConnection;
    QSerialPort m_serial;
    QTimer m_frameWatchdog;
    QTimer m_serialWatchdog;
    QElapsedTimer m_previewClock;
    QElapsedTimer m_lastFrameClock;
    QList<QCameraDevice> m_videoDevices;
    QList<QAudioDevice> m_audioDevices;
    QStringList m_serialPortNames;
    QString m_captureDeviceId;
    QString m_videoFormatKey;
    QString m_audioDeviceId;
    QString m_serialPortName;
    QString m_commandTemplate;
    QMap<int, QString> m_inputLabels;
    QString m_status = "Connect a USB HDMI capture card, then choose it here.";
    QString m_switchStatus = "Choose an input to test the RS-232 switch.";
    QString m_state = "needs-setup";
    int m_baudRate = 9600;
    int m_selectedInput = 1;
    int m_activeInput = 0;
    qint64 m_serialBytesPending = 0;
    bool m_previewRequested = false;
    bool m_receivedFrame = false;
};