#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QTemporaryFile>
#include <memory>

class AudioSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(QString startupPolicy READ startupPolicy NOTIFY changed)
    Q_PROPERTY(int startupVolume READ startupVolume NOTIFY changed)
    Q_PROPERTY(int volumeStep READ volumeStep NOTIFY changed)
    Q_PROPERTY(bool nightMode READ nightMode NOTIFY changed)
    Q_PROPERTY(int lipSyncMs READ lipSyncMs NOTIFY changed)
    Q_PROPERTY(QString volumeMode READ volumeMode NOTIFY changed)
    Q_PROPERTY(int tvReferenceVolume READ tvReferenceVolume NOTIFY changed)
    Q_PROPERTY(bool mixerAvailable READ mixerAvailable NOTIFY changed)
    Q_PROPERTY(QString backendStatus READ backendStatus NOTIFY changed)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString outputStatus READ outputStatus NOTIFY changed)
    Q_PROPERTY(bool testPlaying READ testPlaying NOTIFY changed)
    Q_PROPERTY(QString testStatus READ testStatus NOTIFY changed)

public:
    explicit AudioSettings(QObject *parent = nullptr, const QString &mixerExecutable = {}, const QString &playerExecutable = {});
    ~AudioSettings() override;
    void initialize();
    QVariantList outputs() const { return m_outputs; }
    QString outputStatus() const { return m_outputStatus; }
    Q_INVOKABLE bool selectOutput(const QString &id);
    bool testPlaying() const { return m_testPlaying; }
    QString testStatus() const { return m_testStatus; }
    Q_INVOKABLE void playTestSound();
    Q_INVOKABLE void stopTestSound();

    [[nodiscard]] int volume() const;
    [[nodiscard]] bool muted() const;
    [[nodiscard]] QString startupPolicy() const;
    [[nodiscard]] int startupVolume() const;
    [[nodiscard]] int volumeStep() const;
    [[nodiscard]] bool nightMode() const;
    [[nodiscard]] int lipSyncMs() const;
    [[nodiscard]] QString volumeMode() const;
    [[nodiscard]] int tvReferenceVolume() const;
    [[nodiscard]] bool mixerAvailable() const;
    [[nodiscard]] QString backendStatus() const;

    Q_INVOKABLE void adjustVolume(int delta);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void nextStartupPolicy();
    Q_INVOKABLE void adjustStartupVolume(int delta);
    Q_INVOKABLE void adjustVolumeStep(int delta);
    Q_INVOKABLE void toggleNightMode();
    Q_INVOKABLE void adjustLipSync(int delta);
    Q_INVOKABLE void nextVolumeMode();
    Q_INVOKABLE void adjustTvReferenceVolume(int delta);
    Q_INVOKABLE bool refreshBackend();
    Q_INVOKABLE void reset();

    void setMixerEnabledForTests(bool enabled);

signals:
    void changed();

private:
    void load();
    void save();
    static int clamp(int value, int minimum, int maximum);
    static QString cleanStartupPolicy(const QString &policy);
    static QString cleanVolumeMode(const QString &mode);
    void submit(int volume, bool muted);
    void runNext();
    void completeCommand(int exitCode, QProcess::ExitStatus status);
    void mixerFailed(const QString &reason);
    void enqueueRead();
    QProcess m_process;
    QTimer m_deadline, m_poll;
    QList<QStringList> m_commands;
    QStringList m_currentCommand;
    QByteArray m_output;
    bool m_initialized = false, m_active = false, m_pending = false;
    int m_targetVolume = 50, m_readVolume = 50;
    bool m_targetMuted = false;
    int m_startupAttempts = 0;
    QVariantList m_outputs;
    QString m_outputStatus = "Output selection requires PipeWire / wpctl.";
    QString m_requestedOutput;
    bool m_refreshPending = false;
    QProcess m_testPlayer;
    QTimer m_testDeadline;
    std::unique_ptr<QTemporaryFile> m_testFile;
    QString m_playerExecutable;
    QString m_testStatus = "Three quiet tones: left, right, then both. Uses current volume and mute.";
    bool m_testPlaying = false;

    int m_volume = 50;
    bool m_muted = false;
    QString m_startupPolicy = QStringLiteral("remember");
    int m_startupVolume = 35;
    int m_volumeStep = 5;
    bool m_nightMode = false;
    int m_lipSyncMs = 0;
    QString m_volumeMode = QStringLiteral("isolde");
    int m_tvReferenceVolume = 50;
    bool m_mixerAvailable = false;
    bool m_mixerEnabled = true;
    QString m_mixerTool;
    QString m_backendStatus = QStringLiteral("Mixer backend not checked yet");
};
