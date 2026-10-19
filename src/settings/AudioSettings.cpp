#include "settings/AudioSettings.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDataStream>
#include <QtMath>
#include <cmath>

AudioSettings::AudioSettings(QObject *parent, const QString &mixerExecutable, const QString &playerExecutable) : QObject(parent)
{
    load();
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");
    m_process.setProcessEnvironment(environment);
    m_playerExecutable = playerExecutable;
    if (m_playerExecutable.isEmpty()) m_playerExecutable = QStandardPaths::findExecutable("pw-play");
    if (m_playerExecutable.isEmpty()) m_playerExecutable = QStandardPaths::findExecutable("paplay");
    m_testDeadline.setSingleShot(true);
    m_testDeadline.setInterval(8000);
    connect(&m_testDeadline, &QTimer::timeout, this, [this] {
        stopTestSound(); m_testStatus = "Sound test timed out. Check the output and retry."; emit changed();
    });
    connect(&m_testPlayer, &QProcess::readyReadStandardError, this, [this] { m_testPlayer.readAllStandardError(); });
    connect(&m_testPlayer, &QProcess::readyReadStandardOutput, this, [this] { m_testPlayer.readAllStandardOutput(); });
    connect(&m_testPlayer, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_testDeadline.stop(); m_testPlaying = false; m_testFile.reset();
        m_testStatus = "Could not start the sound player."; emit changed();
    });
    connect(&m_testPlayer, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        m_testDeadline.stop(); m_testFile.reset();
        if (m_testPlaying) m_testStatus = code == 0 && status == QProcess::NormalExit
            ? "Test finished. Did you hear left, right, then both?"
            : "Sound playback failed. Check the output and retry.";
        m_testPlaying = false; emit changed();
    });
    m_mixerTool = mixerExecutable;
    if (m_mixerTool.isEmpty()) m_mixerTool = QStandardPaths::findExecutable("wpctl");
    if (m_mixerTool.isEmpty()) m_mixerTool = QStandardPaths::findExecutable("pactl");
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(2000);
    m_poll.setInterval(3000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (m_active || m_process.state() != QProcess::NotRunning) return;
        if (m_startupAttempts > 0) {
            --m_startupAttempts;
            submit(m_targetVolume, m_targetMuted);
        } else refreshBackend();
    });
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        mixerFailed("Audio service timed out; volume is unconfirmed.");
        m_process.kill();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > 65536) { mixerFailed("Invalid mixer response."); m_process.kill(); }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) mixerFailed("Cannot start the audio backend.");
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &AudioSettings::completeCommand);
}

AudioSettings::~AudioSettings()
{
    m_testPlayer.disconnect(this);
    if (m_testPlayer.state() != QProcess::NotRunning) { m_testPlayer.kill(); m_testPlayer.waitForFinished(1000); }
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) { m_process.kill(); m_process.waitForFinished(1000); }
}

void AudioSettings::playTestSound()
{
    if (m_testPlayer.state() != QProcess::NotRunning) return;
    if (m_playerExecutable.isEmpty()) { m_testStatus = "Install pw-play or paplay to play the sound test."; emit changed(); return; }
    if (!m_mixerAvailable || m_active) { m_testStatus = "Wait for the audio output to be confirmed, then retry."; emit changed(); return; }
    if (m_muted || m_volume == 0) { m_testStatus = "Unmute and raise the volume before testing. The test will not change your volume."; emit changed(); return; }
    m_testFile = std::make_unique<QTemporaryFile>();
    if (!m_testFile->open()) { m_testFile.reset(); m_testStatus = "Cannot prepare the sound test."; emit changed(); return; }
    constexpr int rate = 48000, frames = rate * 3, bytes = frames * 4;
    QDataStream stream(m_testFile.get()); stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4); stream << quint32(36 + bytes);
    stream.writeRawData("WAVEfmt ", 8); stream << quint32(16) << quint16(1) << quint16(2)
        << quint32(rate) << quint32(rate * 4) << quint16(4) << quint16(16);
    stream.writeRawData("data", 4); stream << quint32(bytes);
    for (int frame = 0; frame < frames; ++frame) {
        const int section = frame / rate;
        const double t = double(frame % rate) / rate;
        const double envelope = qBound(0.0, qMin(t / 0.03, (0.7 - t) / 0.03), 1.0);
        const qint16 sample = qRound(3276 * envelope * std::sin(2 * M_PI * (section == 1 ? 660 : 440) * t));
        stream << qint16(section == 1 ? 0 : sample) << qint16(section == 0 ? 0 : sample);
    }
    if (stream.status() != QDataStream::Ok || !m_testFile->flush()) { m_testFile.reset(); m_testStatus = "Cannot write the sound test."; emit changed(); return; }
    m_testPlaying = true;
    m_testStatus = "Playing: left → right → both. Enter stops.";
    m_testPlayer.start(m_playerExecutable, {m_testFile->fileName()});
    m_testDeadline.start(); emit changed();
}

void AudioSettings::stopTestSound()
{
    if (!m_testPlaying) return;
    m_testPlaying = false; m_testDeadline.stop();
    m_testStatus = "Sound test stopped.";
    m_testPlayer.kill(); emit changed();
}

void AudioSettings::initialize()
{
    if (m_initialized) return;
    m_initialized = true;
    m_startupAttempts = 10;
    m_poll.start();
    submit(m_startupPolicy == "fixed" ? m_startupVolume : m_volume,
           m_startupPolicy == "muted" || (m_startupPolicy == "remember" && m_muted));
}

int AudioSettings::volume() const { return m_volume; }
bool AudioSettings::muted() const { return m_muted; }
QString AudioSettings::startupPolicy() const { return m_startupPolicy; }
int AudioSettings::startupVolume() const { return m_startupVolume; }
int AudioSettings::volumeStep() const { return m_volumeStep; }
bool AudioSettings::nightMode() const { return m_nightMode; }
int AudioSettings::lipSyncMs() const { return m_lipSyncMs; }
QString AudioSettings::volumeMode() const { return m_volumeMode; }
int AudioSettings::tvReferenceVolume() const { return m_tvReferenceVolume; }
bool AudioSettings::mixerAvailable() const { return m_mixerAvailable; }
QString AudioSettings::backendStatus() const { return m_backendStatus; }

void AudioSettings::adjustVolume(int delta)
{
    const auto next = clamp((m_active || m_pending ? m_targetVolume : m_volume) + delta, 0, 100);
    submit(next, next > 0 ? false : m_muted);
}

void AudioSettings::toggleMute()
{
    submit(m_active || m_pending ? m_targetVolume : m_volume,
           !(m_active || m_pending ? m_targetMuted : m_muted));
}

void AudioSettings::nextStartupPolicy()
{
    if (m_startupPolicy == QStringLiteral("remember"))
        m_startupPolicy = QStringLiteral("fixed");
    else if (m_startupPolicy == QStringLiteral("fixed"))
        m_startupPolicy = QStringLiteral("muted");
    else
        m_startupPolicy = QStringLiteral("remember");
    save();
    emit changed();
}

void AudioSettings::adjustStartupVolume(int delta)
{
    const auto next = clamp(m_startupVolume + delta, 0, 100);
    if (next == m_startupVolume)
        return;
    m_startupVolume = next;
    save();
    emit changed();
}

void AudioSettings::adjustVolumeStep(int delta)
{
    const auto next = clamp(m_volumeStep + delta, 1, 20);
    if (next == m_volumeStep)
        return;
    m_volumeStep = next;
    save();
    emit changed();
}

void AudioSettings::toggleNightMode()
{
    m_nightMode = !m_nightMode;
    save();
    emit changed();
}

void AudioSettings::adjustLipSync(int delta)
{
    const auto next = clamp(m_lipSyncMs + delta, -250, 250);
    if (next == m_lipSyncMs)
        return;
    m_lipSyncMs = next;
    save();
    emit changed();
}

void AudioSettings::nextVolumeMode()
{
    m_volumeMode = QStringLiteral("isolde");
    save();
    emit changed();
}

void AudioSettings::adjustTvReferenceVolume(int delta)
{
    const auto next = clamp(m_tvReferenceVolume + delta, 0, 100);
    if (next == m_tvReferenceVolume)
        return;
    m_tvReferenceVolume = next;
    save();
    emit changed();
}

void AudioSettings::reset()
{
    m_startupPolicy = QStringLiteral("remember");
    m_startupVolume = 35;
    m_volumeStep = 5;
    m_nightMode = false;
    m_lipSyncMs = 0;
    m_volumeMode = QStringLiteral("isolde");
    m_tvReferenceVolume = 50;
    save();
    submit(50, false);
    emit changed();
}

bool AudioSettings::refreshBackend()
{
    if (!m_mixerEnabled) return m_mixerAvailable;
    if (m_active || m_process.state() != QProcess::NotRunning) { m_refreshPending = true; return m_mixerAvailable; }
    if (m_pending) { runNext(); return m_mixerAvailable; }
    m_targetVolume = m_volume; m_targetMuted = m_muted;
    enqueueRead();
    runNext();
    return m_mixerAvailable;
}

void AudioSettings::setMixerEnabledForTests(bool enabled)
{
    m_mixerEnabled = enabled;
}

void AudioSettings::load()
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", QCoreApplication::applicationName());
    settings.beginGroup(QStringLiteral("audio"));
    m_volume = clamp(settings.value(QStringLiteral("volume"), m_volume).toInt(), 0, 100);
    m_muted = settings.value(QStringLiteral("muted"), m_muted).toBool();
    m_startupPolicy = cleanStartupPolicy(settings.value(QStringLiteral("startupPolicy"), m_startupPolicy).toString());
    m_startupVolume = clamp(settings.value(QStringLiteral("startupVolume"), m_startupVolume).toInt(), 0, 100);
    m_volumeStep = clamp(settings.value(QStringLiteral("volumeStep"), m_volumeStep).toInt(), 1, 20);
    m_nightMode = settings.value(QStringLiteral("nightMode"), m_nightMode).toBool();
    m_lipSyncMs = clamp(settings.value(QStringLiteral("lipSyncMs"), m_lipSyncMs).toInt(), -250, 250);
    m_volumeMode = cleanVolumeMode(settings.value(QStringLiteral("volumeMode"), m_volumeMode).toString());
    m_tvReferenceVolume = clamp(settings.value(QStringLiteral("tvReferenceVolume"), m_tvReferenceVolume).toInt(), 0, 100);
}

void AudioSettings::save()
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", QCoreApplication::applicationName());
    settings.beginGroup(QStringLiteral("audio"));
    settings.setValue(QStringLiteral("volume"), m_volume);
    settings.setValue(QStringLiteral("muted"), m_muted);
    settings.setValue(QStringLiteral("startupPolicy"), m_startupPolicy);
    settings.setValue(QStringLiteral("startupVolume"), m_startupVolume);
    settings.setValue(QStringLiteral("volumeStep"), m_volumeStep);
    settings.setValue(QStringLiteral("nightMode"), m_nightMode);
    settings.setValue(QStringLiteral("lipSyncMs"), m_lipSyncMs);
    settings.setValue(QStringLiteral("volumeMode"), m_volumeMode);
    settings.setValue(QStringLiteral("tvReferenceVolume"), m_tvReferenceVolume);
    settings.sync();
    if (settings.status() != QSettings::NoError) m_backendStatus = "Audio settings could not be saved.";
}

int AudioSettings::clamp(int value, int minimum, int maximum)
{
    return qBound(minimum, value, maximum);
}

QString AudioSettings::cleanStartupPolicy(const QString &policy)
{
    const auto normalized = policy.trimmed().toLower();
    if (normalized == QStringLiteral("fixed") || normalized == QStringLiteral("muted"))
        return normalized;
    return QStringLiteral("remember");
}

QString AudioSettings::cleanVolumeMode(const QString &mode)
{
    Q_UNUSED(mode);
    return QStringLiteral("isolde");
}

void AudioSettings::submit(int volume, bool muted)
{
    m_targetVolume = volume; m_targetMuted = muted;
    if (!m_mixerEnabled) { m_volume = volume; m_muted = muted; save(); emit changed(); return; }
    m_pending = true;
    if (!m_active && m_process.state() == QProcess::NotRunning) runNext();
}

void AudioSettings::enqueueRead()
{
    if (QFileInfo(m_mixerTool).fileName() == "pactl") {
        m_commands.append(QStringList{"get-sink-volume", "@DEFAULT_SINK@"});
        m_commands.append(QStringList{"get-sink-mute", "@DEFAULT_SINK@"});
    } else {
        m_commands.append(QStringList{"get-volume", "@DEFAULT_AUDIO_SINK@"});
        m_commands.append(QStringList{"status"});
    }
}

bool AudioSettings::selectOutput(const QString &id)
{
    if (m_testPlaying) { m_outputStatus = "Stop the sound test before changing outputs."; emit changed(); return false; }
    if (m_active || m_process.state() != QProcess::NotRunning) {
        m_outputStatus = "Audio is busy; try Apply again."; emit changed(); return false;
    }
    bool found = false;
    for (const auto &entry : m_outputs) if (entry.toMap().value("id").toString() == id) found = true;
    if (!found || !QRegularExpression("^[0-9]+$").match(id).hasMatch()) {
        m_outputStatus = "Output is no longer available. Refresh outputs."; emit changed(); return false;
    }
    m_requestedOutput = id;
    m_outputStatus = "Switching output…";
    // Do not copy the previous output's volume onto different speakers.
    m_commands.append(QStringList{"set-default", id});
    enqueueRead();
    runNext(); emit changed(); return true;
}

void AudioSettings::runNext()
{
    if (m_mixerTool.isEmpty()) { mixerFailed("No supported audio backend installed."); return; }
    if (m_commands.isEmpty() && m_pending) {
        m_pending = false;
        const bool pulse = QFileInfo(m_mixerTool).fileName() == "pactl";
        const QString sink = pulse ? "@DEFAULT_SINK@" : "@DEFAULT_AUDIO_SINK@";
        // Mute before changing levels, so start-muted never briefly exposes audio.
        if (m_targetMuted) m_commands.append(QStringList{pulse ? "set-sink-mute" : "set-mute", sink, "1"});
        m_commands.append(QStringList{pulse ? "set-sink-volume" : "set-volume", sink, QString::number(m_targetVolume) + "%"});
        if (!m_targetMuted) m_commands.append(QStringList{pulse ? "set-sink-mute" : "set-mute", sink, "0"});
        enqueueRead();
    }
    if (m_commands.isEmpty() && m_refreshPending) { m_refreshPending = false; enqueueRead(); }
    if (m_commands.isEmpty()) { m_active = false; return; }
    m_active = true;
    m_currentCommand = m_commands.takeFirst();
    m_output.clear();
    m_deadline.start();
    m_process.start(m_mixerTool, m_currentCommand);
}

void AudioSettings::mixerFailed(const QString &reason)
{
    m_deadline.stop(); m_commands.clear(); m_pending = false; m_active = false;
    m_mixerAvailable = false;
    m_refreshPending = false;
    m_outputs.clear();
    m_requestedOutput.clear();
    m_outputStatus = "Outputs unavailable; refresh after the audio service recovers.";
    m_backendStatus = reason;
    emit changed();
}

void AudioSettings::completeCommand(int exitCode, QProcess::ExitStatus status)
{
    m_deadline.stop();
    if (!m_active) return;
    if (status != QProcess::NormalExit || exitCode != 0) {
        mixerFailed("Audio service rejected the command; volume is unconfirmed."); return;
    }
    m_output += m_process.readAllStandardOutput();
    const auto command = m_currentCommand.first();
    const QString output = QString::fromUtf8(m_output).trimmed();
    bool readComplete = false, mute = false;
    if (command == "status") {
        QVariantList outputs;
        bool audio = false, sinks = false, sawSinks = false;
        QString current;
        for (const auto &line : output.split('\n')) {
            if (line == "Audio") { audio = true; continue; }
            if (line == "Video" || line == "Settings") break;
            if (!audio) continue;
            if (line.contains("Sinks:")) { sinks = true; sawSinks = true; continue; }
            if (line.contains("Sources:") || line.contains("Filters:") || line.contains("Streams:")) sinks = false;
            if (!sinks) continue;
            const auto match = QRegularExpression("([*]?)\\s+([0-9]+)\\.\\s+(.+?)\\s+\\[vol:").match(line);
            if (!match.hasMatch()) continue;
            const auto id = match.captured(2);
            const bool active = match.captured(1) == "*";
            if (active) current = id;
            outputs.append(QVariantMap{{"id", id}, {"name", match.captured(3).trimmed()}, {"active", active}});
        }
        m_outputs = outputs;
        if (!sawSinks) m_outputStatus = "Cannot read outputs from this WirePlumber version.";
        else if (outputs.isEmpty()) m_outputStatus = "No audio outputs detected.";
        else if (!m_requestedOutput.isEmpty() && current != m_requestedOutput) m_outputStatus = "Output change was not confirmed. Refresh and retry.";
        else m_outputStatus = "Choose with ← →, Enter applies. Restart playback if it stays on the old output.";
        m_requestedOutput.clear();
        emit changed();
    } else if (command == "get-volume") {
        const auto match = QRegularExpression("^Volume: ([0-9]+(?:\\.[0-9]+)?)(?: (\\[MUTED\\]))?$").match(output);
        if (!match.hasMatch()) { mixerFailed("Invalid mixer volume response."); return; }
        m_readVolume = qRound(match.captured(1).toDouble() * 100);
        mute = !match.captured(2).isEmpty(); readComplete = true;
    } else if (command == "get-sink-volume") {
        const auto match = QRegularExpression("([0-9]+)%").match(output);
        if (!match.hasMatch()) { mixerFailed("Invalid mixer volume response."); return; }
        m_readVolume = match.captured(1).toInt();
    } else if (command == "get-sink-mute") {
        if (output != "Mute: yes" && output != "Mute: no") { mixerFailed("Invalid mixer mute response."); return; }
        mute = output == "Mute: yes"; readComplete = true;
    }
    if (readComplete) {
        m_startupAttempts = 0;
        m_volume = m_readVolume; m_muted = mute;
        m_mixerAvailable = true;
        m_backendStatus = "Confirmed system output via " + QFileInfo(m_mixerTool).fileName();
        save(); emit changed();
    }
    runNext();
}
