#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSerialPort>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include "SerialCommandWriter.h"

class AudioCaptureAdapter;
class VideoCaptureAdapter;

class CaptureController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList videoDevices READ videoDevices NOTIFY changed)
    Q_PROPERTY(QVariantList videoFormats READ videoFormats NOTIFY changed)
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY changed)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY changed)
    Q_PROPERTY(QVariantList serialPorts READ serialPorts NOTIFY changed)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY changed)
    Q_PROPERTY(QString captureDeviceId READ captureDeviceId NOTIFY changed)
    Q_PROPERTY(QString videoFormatKey READ videoFormatKey NOTIFY changed)
    Q_PROPERTY(QString audioDeviceId READ audioDeviceId NOTIFY changed)
    Q_PROPERTY(QString serialPortName READ serialPortName NOTIFY changed)
    Q_PROPERTY(QString commandTemplate READ commandTemplate NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString audioState READ audioState NOTIFY changed)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY changed)
    Q_PROPERTY(QString switchStatus READ switchStatus NOTIFY changed)
    Q_PROPERTY(QString switchState READ switchState NOTIFY changed)
    Q_PROPERTY(qint64 serialWriteLatencyMs READ serialWriteLatencyMs NOTIFY changed)
    Q_PROPERTY(qint64 firstFrameCallbackLatencyMs READ firstFrameCallbackLatencyMs NOTIFY changed)
    Q_PROPERTY(double previewFrameRate READ previewFrameRate NOTIFY changed)
    Q_PROPERTY(qint64 maxFrameGapMs READ maxFrameGapMs NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool previewActive READ previewActive NOTIFY changed)
    Q_PROPERTY(int selectedInput READ selectedInput NOTIFY changed)
    Q_PROPERTY(int activeInput READ activeInput NOTIFY changed)
    Q_PROPERTY(int baudRate READ baudRate NOTIFY changed)
    Q_PROPERTY(QString sessionTransition READ sessionTransition NOTIFY changed)
    Q_PROPERTY(int transitionDuration READ transitionDuration NOTIFY changed)
public:
    explicit CaptureController(QObject *parent = nullptr);
    // Injected adapters must outlive the controller.
    CaptureController(AudioCaptureAdapter *audioAdapter, QObject *parent);
    CaptureController(VideoCaptureAdapter *videoAdapter, AudioCaptureAdapter *audioAdapter, QObject *parent);
    ~CaptureController() override;

    QVariantList videoDevices() const;
    QVariantList videoFormats() const;
    QVariantList audioDevices() const;
    QVariantMap capabilities() const;
    QVariantList serialPorts() const;
    QVariantList inputs() const;
    QString captureDeviceId() const { return m_captureDeviceId; }
    QString videoFormatKey() const { return m_videoFormatKey; }
    QString audioDeviceId() const { return m_audioDeviceId; }
    QString serialPortName() const { return m_serialPortName; }
    QString commandTemplate() const { return m_commandTemplate; }
    QString status() const { return m_status; }
    QString audioState() const { return m_audioState; }
    QString audioStatus() const { return m_audioStatus; }
    QString switchStatus() const { return m_switchStatus; }
    QString switchState() const { return m_switchState; }
    qint64 serialWriteLatencyMs() const { return m_serialWriteLatencyMs; }
    qint64 firstFrameCallbackLatencyMs() const { return m_firstFrameCallbackLatencyMs; }
    double previewFrameRate() const { return m_previewFrameRate; }
    qint64 maxFrameGapMs() const { return m_maxFrameGapMs; }
    QString state() const { return m_state; }
    bool previewActive() const { return m_previewRequested; }
    int selectedInput() const { return m_selectedInput; }
    int activeInput() const { return m_activeInput; }
    int baudRate() const { return m_baudRate; }
    QString sessionTransition() const { return m_sessionTransition; }
    int transitionDuration() const { return m_transitionDuration; }

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE QVariantList videoFormatsFor(const QString &deviceId) const;
    Q_INVOKABLE void setVideoOutput(QObject *output);
    Q_INVOKABLE bool setCaptureDevice(const QString &id);
    Q_INVOKABLE bool setVideoFormat(const QString &key);
    Q_INVOKABLE bool setAudioDevice(const QString &id);
    Q_INVOKABLE bool setSerialPort(const QString &portName);
    Q_INVOKABLE bool setBaudRate(int baudRate);
    Q_INVOKABLE bool setCommandTemplate(const QString &value);
    Q_INVOKABLE bool setInputLabel(int input, const QString &label);
    Q_INVOKABLE void selectInput(int input);
    Q_INVOKABLE bool applyConfiguration(const QVariantMap &configuration);
    Q_INVOKABLE bool canSwitchInput(int input) const;
    Q_INVOKABLE bool startPreview();
    bool startPreviewFor(const QString &captureId, const QString &formatKey, const QString &audioId);
    Q_INVOKABLE void stopPreview();
    Q_INVOKABLE bool switchInput(int input);
    Q_INVOKABLE void cancelSwitch();

    static QByteArray commandForInput(const QString &commandTemplate, int input, QString *error = nullptr);
    static bool isValidInputLabel(const QString &label);

signals:
    void changed();

private:
    void loadSettings();
    bool saveValue(const QString &key, const QVariant &value);
    bool saveValues(const QMap<QString, QVariant> &values);
    void updateStatusForConfiguration();
    void updateIdleAudioStatus();
    bool configureAudio();
    QString inputLabel(int input) const;
    bool hasVideoFormat(const QString &deviceId, const QString &key) const;
    void setStatus(const QString &state, const QString &message);
    void setAudioStatus(const QString &state, const QString &message);
    void serialWriteFailed(const QString &message);

    VideoCaptureAdapter *m_videoAdapter = nullptr;
    AudioCaptureAdapter *m_audioAdapter = nullptr;
    QPointer<QObject> m_videoOutput;
    QMetaObject::Connection m_outputDestroyedConnection;
    QSerialPort m_serial;
    SerialCommandWriter m_serialWriter;
    QTimer m_frameWatchdog;
    QTimer m_serialDevicePoll;
    QElapsedTimer m_previewClock;
    QElapsedTimer m_lastFrameClock;
    QElapsedTimer m_switchClock;
    QElapsedTimer m_frameRateClock;
    QVariantList m_videoDevices;
    QVariantList m_audioDevices;
    QStringList m_serialPortNames;
    QString m_captureDeviceId;
    QString m_videoFormatKey;
    QString m_audioDeviceId;
    QString m_serialPortName;
    QString m_commandTemplate;
    QString m_sessionTransition = "expand";
    QString m_previewCaptureId;
    QString m_previewFormatKey;
    QString m_previewAudioId;
    QMap<int, QString> m_inputLabels;
    QString m_status = "Connect a USB HDMI capture card, then choose it here.";
    QString m_audioState = "idle";
    QString m_audioStatus;
    QString m_switchStatus = "Choose an input to test the RS-232 switch.";
    QString m_switchState = "idle";
    QString m_state = "needs-setup";
    int m_baudRate = 9600;
    int m_transitionDuration = 250;
    int m_selectedInput = 1;
    int m_activeInput = 0;
    qint64 m_serialWriteLatencyMs = -1;
    qint64 m_firstFrameCallbackLatencyMs = -1;
    qint64 m_maxFrameGapMs = -1;
    qint64 m_framesInWindow = 0;
    double m_previewFrameRate = 0.0;
    bool m_previewRequested = false;
    bool m_receivedFrame = false;
    bool m_audioConfigured = false;
    bool m_configuringAudio = false;
    quint64 m_audioErrorGeneration = 0;
};