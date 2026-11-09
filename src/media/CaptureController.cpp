#include "CaptureController.h"
#include "QtAudioCaptureAdapter.h"
#include "QtVideoCaptureAdapter.h"

#include <QMetaType>
#include <QSerialPortInfo>
#include <QSettings>
#include <QtGlobal>
#include <cmath>
#include <limits>

namespace {
constexpr int MaxCommandBytes = 128;
const QList<int> SupportedBaudRates{9600, 19200, 38400, 57600, 115200};

bool hasDevice(const QVariantList &devices, const QString &id)
{
    if (id.isEmpty()) return true;
    for (const auto &device : devices)
        if (device.toMap().value("id").toString() == id) return true;
    return false;
}

QSettings captureSettings()
{
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Capture");
}
}

CaptureController::CaptureController(QObject *parent) : CaptureController(nullptr, nullptr, parent) {}

CaptureController::CaptureController(AudioCaptureAdapter *audioAdapter, QObject *parent)
    : CaptureController(nullptr, audioAdapter, parent) {}

CaptureController::CaptureController(VideoCaptureAdapter *videoAdapter, AudioCaptureAdapter *audioAdapter, QObject *parent)
    : QObject(parent), m_videoAdapter(videoAdapter ? videoAdapter : new QtVideoCaptureAdapter(this)),
      m_audioAdapter(audioAdapter ? audioAdapter : new QtAudioCaptureAdapter(this))
{
    loadSettings();
    m_frameWatchdog.setInterval(1000);
    connect(&m_frameWatchdog, &QTimer::timeout, this, [this] {
        if (!m_previewRequested) return;
        if (!m_receivedFrame && m_previewClock.elapsed() >= 8000)
            setStatus("no-signal", "No video frames yet. Check the capture card, HDMI source, and selected input.");
        else if (m_receivedFrame && m_lastFrameClock.elapsed() >= 3500)
            setStatus("no-signal", "Video frames stopped. Check the HDMI source and capture-card connection.");
    });
    m_serialDevicePoll.setInterval(2000);
    connect(&m_serialDevicePoll, &QTimer::timeout, this, [this] {
        QStringList ports;
        for (const auto &port : QSerialPortInfo::availablePorts()) ports.append(port.portName());
        if (ports == m_serialPortNames) return;
        m_serialPortNames = ports;
        updateStatusForConfiguration();
        emit changed();
    });
    m_serialDevicePoll.start();
    connect(m_videoAdapter, &VideoCaptureAdapter::devicesChanged, this, &CaptureController::refreshDevices);
    connect(m_audioAdapter, &AudioCaptureAdapter::devicesChanged, this, &CaptureController::refreshDevices);
    connect(m_audioAdapter, &AudioCaptureAdapter::errorOccurred, this, [this](const QString &message) {
        ++m_audioErrorGeneration;
        m_audioConfigured = false;
        if (m_previewRequested) setStatus("audio-error", "Capture audio error: " + message);
    });
    connect(m_videoAdapter, &VideoCaptureAdapter::errorOccurred, this, [this](const QString &message) {
        if (m_previewRequested) {
            stopPreview();
            setStatus("error", "Capture card error: " + message + " Retry preview after checking the device.");
        }
    });
    connect(m_videoAdapter, &VideoCaptureAdapter::frameReceived, this, [this] {
        if (!m_previewRequested) return;
        m_receivedFrame = true;
        bool metricsChanged = false;
        if (m_switchClock.isValid()) {
            m_firstFrameCallbackLatencyMs = m_switchClock.elapsed();
            m_switchClock.invalidate();
            metricsChanged = true;
        }
        if (m_lastFrameClock.isValid()) m_maxFrameGapMs = qMax(m_maxFrameGapMs, m_lastFrameClock.elapsed());
        m_lastFrameClock.restart();
        if (!m_frameRateClock.isValid()) m_frameRateClock.start();
        ++m_framesInWindow;
        if (m_frameRateClock.elapsed() >= 1000) {
            m_previewFrameRate = m_framesInWindow * 1000.0 / m_frameRateClock.elapsed();
            m_framesInWindow = 0;
            m_frameRateClock.restart();
            metricsChanged = true;
        }
        if (m_state != "receiving-frames")
            setStatus("receiving-frames", "Video frames are arriving. Check that the picture matches the selected HDMI source.");
        else if (metricsChanged) emit changed();
    });
    connect(&m_serialWriter, &SerialCommandWriter::completed, this, [this](qint64 elapsed) {
        m_serial.close();
        m_serialWriteLatencyMs = elapsed;
        if (m_previewRequested) {
            m_receivedFrame = false;
            m_previewClock.restart();
            m_lastFrameClock.invalidate();
            setStatus("awaiting-frames", "Command sent. Waiting for a subsequent video frame; then check the selected source.");
        }
        m_switchState = "sent";
        if (m_activeInput > 0) {
            m_switchStatus = "Command sent for " + inputLabel(m_activeInput) + ". Confirm the preview shows that source.";
        }
        emit changed();
    });
    connect(&m_serialWriter, &SerialCommandWriter::failed, this, &CaptureController::serialWriteFailed);
    connect(&m_serialWriter, &SerialCommandWriter::stateChanged, this, [this] {
        if (m_serialWriter.state() == "sent") return;
        m_switchState = m_serialWriter.state();
        emit changed();
    });
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError || !m_serialWriter.isSending()) return;
        m_serialWriter.fail("Serial switch error: " + m_serial.errorString());
    });
    refreshDevices();
}

CaptureController::~CaptureController()
{
    stopPreview();
    if (m_serial.isOpen()) m_serial.close();
}

void CaptureController::loadSettings()
{
    auto settings = captureSettings();
    m_captureDeviceId = settings.value("capture/device").toString();
    m_videoFormatKey = settings.value("capture/format").toString();
    m_audioDeviceId = settings.value("capture/audio").toString();
    m_serialPortName = settings.value("switch/port").toString();
    m_baudRate = settings.value("switch/baud", 9600).toInt();
    if (!SupportedBaudRates.contains(m_baudRate)) m_baudRate = 9600;
    m_commandTemplate = settings.value("switch/commandTemplate").toString();
    const auto transition = settings.value("session/transition", "expand").toString();
    if (QStringList{"instant", "fade", "slide", "expand"}.contains(transition)) m_sessionTransition = transition;
    const auto duration = settings.value("session/duration", 250).toInt();
    if (QList<int>{200, 250, 300, 350}.contains(duration)) m_transitionDuration = duration;
    m_switchStatus = m_commandTemplate.isEmpty() ? "No switch command is configured."
        : "Saved command loaded. Send a test and visually verify the selected source.";
    for (int input = 1; input <= 8; ++input) {
        const auto key = QString("inputs/%1/name").arg(input);
        const auto stored = settings.value(key).toString().trimmed();
        m_inputLabels.insert(input, stored.isEmpty() || stored.size() > 40
            ? QString("HDMI %1").arg(input) : stored);
    }
    m_selectedInput = qBound(1, settings.value("inputs/selected", 1).toInt(), 8);
}

bool CaptureController::saveValue(const QString &key, const QVariant &value)
{
    return saveValues({{key, value}});
}

bool CaptureController::saveValues(const QMap<QString, QVariant> &values)
{
    auto settings = captureSettings();
    settings.setAtomicSyncRequired(true);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        settings.setValue(it.key(), it.value());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setStatus("error", "Could not save capture settings. Check available storage and try again.");
        return false;
    }
    return true;
}

QVariantList CaptureController::videoDevices() const
{
    return m_videoDevices;
}

QVariantList CaptureController::videoFormats() const
{
    return videoFormatsFor(m_captureDeviceId);
}

QVariantList CaptureController::videoFormatsFor(const QString &deviceId) const
{
    QVariantList result = m_videoAdapter->formats(deviceId);
    for (auto &entry : result) {
        auto value = entry.toMap();
        value.insert("selected", value.value("key").toString() == m_videoFormatKey);
        entry = value;
    }
    return result;
}

QVariantList CaptureController::audioDevices() const
{
    return m_audioDevices;
}

QVariantMap CaptureController::capabilities() const
{
    const bool captureConnected = !m_captureDeviceId.isEmpty() && hasDevice(m_videoDevices, m_captureDeviceId);
    const bool audioConnected = !m_audioDeviceId.isEmpty() && hasDevice(m_audioDevices, m_audioDeviceId);
    const bool previewAudioConnected = !m_previewAudioId.isEmpty() && hasDevice(m_audioDevices, m_previewAudioId);
    const bool serialConnected = !m_serialPortName.isEmpty() && m_serialPortNames.contains(m_serialPortName);
    const bool audioMonitoringSupported = m_audioAdapter->monitoringSupported();
    return {
        {"captureAdapter", m_videoAdapter->name()},
        {"captureConnected", captureConnected},
        {"previewSurfaceAvailable", !m_videoOutput.isNull()},
        {"audioAdapter", m_audioAdapter->name()},
        {"audioInputSelected", audioConnected},
        {"audioMonitoringSupported", audioMonitoringSupported},
        {"audioMonitoringConfigured", audioMonitoringSupported && m_audioConfigured && m_previewRequested && previewAudioConnected},
        {"serialAdapter", "Qt SerialPort"},
        {"serialConnected", serialConnected},
        {"switchConfigured", canSwitchInput(m_selectedInput)}
    };
}

QVariantList CaptureController::serialPorts() const
{
    QVariantList result;
    for (const auto &name : m_serialPortNames) result.append(QVariantMap{{"id", name}, {"name", name}});
    return result;
}

QVariantList CaptureController::inputs() const
{
    QVariantList result;
    for (int input = 1; input <= 8; ++input)
        result.append(QVariantMap{{"number", input}, {"name", inputLabel(input)}, {"selected", input == m_selectedInput}});
    return result;
}

QString CaptureController::inputLabel(int input) const
{
    return m_inputLabels.value(input, QString("HDMI %1").arg(input));
}

bool CaptureController::hasVideoFormat(const QString &deviceId, const QString &key) const
{
    if (key.isEmpty()) return true;
    for (const auto &format : m_videoAdapter->formats(deviceId))
        if (format.toMap().value("key").toString() == key) return true;
    return false;
}

void CaptureController::refreshDevices()
{
    const auto audioId = m_previewRequested ? m_previewAudioId : m_audioDeviceId;
    const bool selectedAudioWasPresent = hasDevice(m_audioDevices, audioId);
    m_videoDevices = m_videoAdapter->devices();
    m_audioDevices = m_audioAdapter->devices();
    m_serialPortNames.clear();
    for (const auto &port : QSerialPortInfo::availablePorts()) m_serialPortNames.append(port.portName());
    const bool selectedAudioIsPresent = hasDevice(m_audioDevices, audioId);
    if (m_previewRequested && selectedAudioWasPresent != selectedAudioIsPresent)
        configureAudio();
    if (m_previewRequested) {
        const bool present = hasDevice(m_videoDevices, m_previewCaptureId);
        if (!present) {
            stopPreview();
            setStatus("device-missing", "Capture card disconnected. Reconnect it, then choose it again.");
            return;
        }
        if (!hasVideoFormat(m_previewCaptureId, m_previewFormatKey)) {
            stopPreview();
            setStatus("format-missing", "The active capture format is no longer available. Choose another format and retry preview.");
            return;
        }
    }
    updateStatusForConfiguration();
    emit changed();
}

void CaptureController::updateStatusForConfiguration()
{
    // Configuration changes must not turn a live signal state into "ready":
    // the session uses that state to gate source confirmation.
    if (m_previewRequested) return;
    const bool videoPresent = hasDevice(m_videoDevices, m_captureDeviceId);
    const bool formatPresent = hasVideoFormat(m_captureDeviceId, m_videoFormatKey);
    const bool audioPresent = hasDevice(m_audioDevices, m_audioDeviceId);
    if (m_captureDeviceId.isEmpty()) setStatus("needs-capture-card", "Connect and choose the HDMI capture card.");
    else if (!videoPresent) setStatus("device-missing", "The saved capture card is not connected. Reconnect it or choose another card.");
    else if (!formatPresent) setStatus("format-missing", "The saved video format is no longer supported. Choose another format or use the card default.");
    else if (!audioPresent) setStatus("audio-missing", "The saved capture audio input is not connected. Reconnect it, choose another input, or clear the optional audio selection.");
    else if (m_serialPortName.isEmpty()) setStatus("needs-switch", "Capture card found. Choose the RS-232 switch or preview the card directly.");
    else if (!m_serialPortNames.contains(m_serialPortName)) setStatus("serial-missing", "The saved RS-232 port is not connected. Reconnect the switch or choose another port.");
    else if (m_commandTemplate.isEmpty() || commandForInput(m_commandTemplate, 1).isEmpty()) {
        m_switchStatus = m_commandTemplate.isEmpty() ? "No switch command is configured."
            : "Saved switch command is invalid. Enter the exact command from the switch manual.";
        setStatus("needs-protocol", "Capture card and serial port are selected. Configure a valid command from the switch manual.");
    }
    else setStatus("ready", "Capture settings are complete. Preview the card and test the switch; source correctness is not verified.");
}

void CaptureController::setVideoOutput(QObject *output)
{
    if (m_videoOutput == output) return;
    if (m_outputDestroyedConnection) disconnect(m_outputDestroyedConnection);
    m_videoOutput = output;
    m_videoAdapter->setOutput(output);
    if (output) {
        m_outputDestroyedConnection = connect(output, &QObject::destroyed, this, [this] {
            m_videoOutput = nullptr;
            m_videoAdapter->setOutput(nullptr);
            stopPreview();
        });
    } else stopPreview();
}

bool CaptureController::setCaptureDevice(const QString &id)
{
    const bool found = hasDevice(m_videoDevices, id);
    if (!found) return false;
    QString nextFormat = m_videoFormatKey;
    const bool formatAvailable = hasVideoFormat(id, nextFormat);
    if (!formatAvailable) nextFormat.clear();
    if (!saveValues({{"capture/device", id}, {"capture/format", nextFormat}})) return false;
    stopPreview();
    m_captureDeviceId = id;
    m_videoFormatKey = nextFormat;
    updateStatusForConfiguration();
    emit changed();
    return true;
}

bool CaptureController::setVideoFormat(const QString &key)
{
    const bool found = hasVideoFormat(m_captureDeviceId, key);
    if (!found || (key.isEmpty() && m_captureDeviceId.isEmpty())) return false;
    if (!saveValue("capture/format", key)) return false;
    const bool restart = m_previewRequested;
    if (restart) stopPreview();
    m_videoFormatKey = key;
    emit changed();
    return !restart || startPreview();
}

bool CaptureController::setAudioDevice(const QString &id)
{
    const bool found = hasDevice(m_audioDevices, id);
    if (!found) return false;
    if (!saveValue("capture/audio", id)) return false;
    m_audioDeviceId = id;
    if (m_previewRequested) {
        m_previewAudioId = id;
        configureAudio();
    }
    emit changed();
    return true;
}

bool CaptureController::setSerialPort(const QString &portName)
{
    if (!portName.isEmpty() && !m_serialPortNames.contains(portName)) return false;
    if (m_serialWriter.isSending()) {
        m_switchStatus = "Wait for the current switch command to finish before changing ports.";
        emit changed();
        return false;
    }
    if (!saveValue("switch/port", portName)) return false;
    if (m_serial.isOpen()) m_serial.close();
    m_serialPortName = portName;
    m_switchStatus = portName.isEmpty() ? "Choose the serial port connected to the HDMI switch." : "Port selected. Choose a command format from the switch manual.";
    updateStatusForConfiguration();
    emit changed();
    return true;
}

bool CaptureController::setBaudRate(int baudRate)
{
    if (!SupportedBaudRates.contains(baudRate)) return false;
    if (!saveValue("switch/baud", baudRate)) return false;
    m_baudRate = baudRate;
    emit changed();
    return true;
}

QByteArray CaptureController::commandForInput(const QString &commandTemplate, int input, QString *error)
{
    if (error) error->clear();
    if (input < 1 || input > 8 || commandTemplate.count("{input}") != 1) {
        if (error) *error = "Use one {input} placeholder and choose input 1–8.";
        return {};
    }
    QByteArray result;
    for (qsizetype index = 0; index < commandTemplate.size(); ++index) {
        if (commandTemplate.mid(index, 7) == "{input}") {
            result.append(QByteArray::number(input));
            index += 6;
        } else if (commandTemplate[index] == QLatin1Char('\\')) {
            if (++index >= commandTemplate.size()) {
                if (error) *error = "Finish the escape sequence with r, n, t, or a backslash.";
                return {};
            }
            const auto escaped = commandTemplate[index];
            if (escaped == QLatin1Char('r')) result.append('\r');
            else if (escaped == QLatin1Char('n')) result.append('\n');
            else if (escaped == QLatin1Char('t')) result.append('\t');
            else if (escaped == QLatin1Char('\\')) result.append('\\');
            else {
                if (error) *error = "Supported escapes are \\r, \\n, \\t, and \\\\.";
                return {};
            }
        } else {
            const auto character = commandTemplate[index].unicode();
            if (character < 0x20 || character > 0x7e) {
                if (error) *error = "Switch commands must use printable ASCII characters.";
                return {};
            }
            result.append(static_cast<char>(character));
        }
        if (result.size() > MaxCommandBytes) {
            if (error) *error = "Switch command is too long (maximum 128 bytes).";
            return {};
        }
    }
    return result;
}

bool CaptureController::setCommandTemplate(const QString &value)
{
    QString error;
    if (!value.isEmpty() && commandForInput(value, 1, &error).isEmpty()) {
        setStatus("needs-protocol", error);
        return false;
    }
    if (!saveValue("switch/commandTemplate", value)) return false;
    m_commandTemplate = value;
    m_switchStatus = value.isEmpty() ? "No switch command is configured." : "Command saved. Send a test and verify the picture.";
    updateStatusForConfiguration();
    emit changed();
    return true;
}

bool CaptureController::setInputLabel(int input, const QString &label)
{
    const auto clean = label.trimmed();
    if (input < 1 || input > 8 || !isValidInputLabel(clean)) return false;
    if (!saveValue(QString("inputs/%1/name").arg(input), clean)) return false;
    m_inputLabels.insert(input, clean);
    emit changed();
    return true;
}

bool CaptureController::isValidInputLabel(const QString &label)
{
    if (label.trimmed().isEmpty() || label.size() > 40) return false;
    for (const auto character : label) {
        const auto code = character.unicode();
        if (character.isNull() || (character.isSpace() && character != QLatin1Char(' ')))
            return false;
        if (code < 0x20 || (code >= 0x7f && code <= 0x9f)) return false;
    }
    return true;
}

void CaptureController::selectInput(int input)
{
    if (input < 1 || input > 8 || input == m_selectedInput) return;
    if (!saveValue("inputs/selected", input)) return;
    m_selectedInput = input;
    emit changed();
}

bool CaptureController::applyConfiguration(const QVariantMap &configuration)
{
    if (m_serial.isOpen() || m_serialWriter.isSending()) {
        m_switchStatus = "Wait for the current switch command to finish before applying capture settings.";
        emit changed();
        return false;
    }

    const QStringList expectedKeys{
        "audioDeviceId", "baudRate", "captureDeviceId", "commandTemplate",
        "inputLabels", "selectedInput", "serialPortName", "sessionTransition", "transitionDuration", "videoFormatKey"
    };
    const auto captureId = configuration.value("captureDeviceId").toString();
    const auto format = configuration.value("videoFormatKey").toString();
    const auto audioId = configuration.value("audioDeviceId").toString();
    const auto portName = configuration.value("serialPortName").toString();
    const auto commandTemplate = configuration.value("commandTemplate").toString();
    const auto exactInt = [](const QVariant &value, int *result) {
        const auto type = value.metaType().id();
        if (type == QMetaType::Int) {
            *result = value.toInt();
            return true;
        }
        if (type != QMetaType::Double) return false;
        bool ok = false;
        const auto number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number) || std::floor(number) != number
            || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
            return false;
        *result = static_cast<int>(number);
        return true;
    };
    const auto transition = configuration.value("sessionTransition").toString();
    int baud = 0, selectedInput = 0, duration = 0;
    const bool integerTypesValid = exactInt(configuration.value("baudRate"), &baud)
        && exactInt(configuration.value("selectedInput"), &selectedInput)
        && exactInt(configuration.value("transitionDuration"), &duration);
    const auto labels = configuration.value("inputLabels").toMap();

    const bool captureFound = hasDevice(m_videoDevices, captureId);
    const bool formatFound = hasVideoFormat(captureId, format);
    const bool audioFound = hasDevice(m_audioDevices, audioId);
    bool portFound = portName.isEmpty();
    portFound |= m_serialPortNames.contains(portName);
    QString commandError;
    const bool commandValid = commandTemplate.isEmpty()
        || !commandForInput(commandTemplate, 1, &commandError).isEmpty();
    bool labelsValid = labels.size() == 8;
    for (int input = 1; input <= 8 && labelsValid; ++input) {
        const auto label = labels.value(QString::number(input)).toString().trimmed();
        labelsValid = isValidInputLabel(label);
    }
    const bool typesValid = configuration.value("captureDeviceId").metaType().id() == QMetaType::QString
        && configuration.value("videoFormatKey").metaType().id() == QMetaType::QString
        && configuration.value("audioDeviceId").metaType().id() == QMetaType::QString
        && configuration.value("serialPortName").metaType().id() == QMetaType::QString
        && configuration.value("commandTemplate").metaType().id() == QMetaType::QString
        && configuration.value("sessionTransition").metaType().id() == QMetaType::QString
        && integerTypesValid
        && configuration.value("inputLabels").metaType().id() == QMetaType::QVariantMap;
    if (configuration.keys() != expectedKeys || !typesValid
        || !captureFound || !formatFound || !audioFound || !portFound
        || !SupportedBaudRates.contains(baud) || selectedInput < 1 || selectedInput > 8
        || !QStringList{"instant", "fade", "slide", "expand"}.contains(transition)
        || !QList<int>{200, 250, 300, 350}.contains(duration)
        || !commandValid || !labelsValid) {
        m_switchState = "error";
        m_switchStatus = !commandValid && !commandError.isEmpty()
            ? commandError : "Capture settings changed or are invalid. Refresh devices and review the draft.";
        emit changed();
        return false;
    }

    QMap<QString, QVariant> values{
        {"capture/device", captureId},
        {"capture/format", format},
        {"capture/audio", audioId},
        {"switch/port", portName},
        {"switch/baud", baud},
        {"switch/commandTemplate", commandTemplate},
        {"inputs/selected", selectedInput},
        {"session/transition", transition}, {"session/duration", duration}
    };
    for (int input = 1; input <= 8; ++input)
        values.insert(QString("inputs/%1/name").arg(input),
                      labels.value(QString::number(input)).toString().trimmed());
    if (!saveValues(values)) return false;

    if (m_previewRequested && (m_previewCaptureId != captureId
        || m_previewFormatKey != format || m_previewAudioId != audioId)) stopPreview();
    if (m_serial.isOpen()) m_serial.close();
    m_captureDeviceId = captureId;
    m_videoFormatKey = format;
    m_audioDeviceId = audioId;
    m_serialPortName = portName;
    m_baudRate = baud;
    m_commandTemplate = commandTemplate;
    m_sessionTransition = transition;
    m_transitionDuration = duration;
    m_selectedInput = selectedInput;
    for (int input = 1; input <= 8; ++input)
        m_inputLabels.insert(input, labels.value(QString::number(input)).toString().trimmed());
    m_switchState = "idle";
    m_switchStatus = commandTemplate.isEmpty()
        ? "No switch command is configured."
        : "Capture settings saved. Send a test and visually verify the selected source.";
    updateStatusForConfiguration();
    emit changed();
    return true;
}

bool CaptureController::canSwitchInput(int input) const
{
    if (input < 1 || input > 8 || m_captureDeviceId.isEmpty() || m_serialPortName.isEmpty()
        || !m_serialPortNames.contains(m_serialPortName)) return false;
    const bool videoPresent = hasDevice(m_videoDevices, m_captureDeviceId);
    const bool formatPresent = hasVideoFormat(m_captureDeviceId, m_videoFormatKey);
    if (!videoPresent || !formatPresent) return false;
    const auto command = commandForInput(m_commandTemplate, input);
    return !command.isEmpty() && m_baudRate > 0;
}

bool CaptureController::configureAudio()
{
    m_audioConfigured = false;
    const auto generation = m_audioErrorGeneration;
    if (!m_audioAdapter->configure(m_videoAdapter->audioContext(), m_previewAudioId)
        || m_audioErrorGeneration != generation) {
        setStatus("audio-missing", "The capture audio input is unavailable or incompatible with the video adapter.");
        return false;
    }
    m_audioConfigured = true;
    return true;
}

bool CaptureController::startPreview()
{
    return startPreviewFor(m_captureDeviceId, m_videoFormatKey, m_audioDeviceId);
}

bool CaptureController::startPreviewFor(const QString &captureId, const QString &formatKeyValue, const QString &audioId)
{
    if (m_serialWriter.isSending() && (m_previewRequested || captureId != m_captureDeviceId
        || formatKeyValue != m_videoFormatKey || audioId != m_audioDeviceId)) {
        setStatus("error", "Wait for the current switch command before changing the capture preview.");
        return false;
    }
    if (captureId.isEmpty()) {
        setStatus("needs-capture-card", "Choose the HDMI capture card before starting preview.");
        return false;
    }
    if (!m_videoOutput) {
        setStatus("needs-preview-surface", "Open the preview screen before starting capture.");
        return false;
    }
    if (!hasDevice(m_videoDevices, captureId)) {
        setStatus("device-missing", "Capture card is not connected. Reconnect it, then refresh devices.");
        return false;
    }
    if (!hasVideoFormat(captureId, formatKeyValue)) {
        setStatus("format-missing", "The saved video format is no longer supported. Choose another format or use the card default.");
        return false;
    }
    const bool audioFound = hasDevice(m_audioDevices, audioId);
    if (!audioFound) {
        setStatus("audio-missing", "The selected capture audio input is no longer available.");
        return false;
    }
    if (m_previewRequested && m_previewCaptureId == captureId
        && m_previewFormatKey == formatKeyValue && m_previewAudioId == audioId) return true;
    if (m_previewRequested) stopPreview();
    m_previewCaptureId = captureId;
    m_previewFormatKey = formatKeyValue;
    m_previewAudioId = audioId;
    if (!configureAudio()) {
        stopPreview();
        setStatus("audio-missing", "The capture audio input is unavailable or incompatible with the video adapter.");
        return false;
    }
    m_previewRequested = true;
    m_receivedFrame = false;
    m_framesInWindow = 0;
    m_previewFrameRate = 0.0;
    m_maxFrameGapMs = -1;
    m_frameRateClock.invalidate();
    m_previewClock.restart();
    m_lastFrameClock.invalidate();
    m_frameWatchdog.start();
    QString startupStatus = "Starting capture card. Waiting for video frames…";
    if (!audioId.isEmpty() && !m_audioAdapter->monitoringSupported())
        startupStatus += " Audio monitoring is unavailable; video preview only on this system.";
    setStatus("awaiting-frames", startupStatus);
    if (!m_videoAdapter->start(captureId, formatKeyValue)) {
        stopPreview();
        setStatus("device-missing", "The capture card or video format is no longer available.");
        return false;
    }
    return m_previewRequested;
}

void CaptureController::stopPreview()
{
    m_frameWatchdog.stop();
    m_previewRequested = false;
    m_receivedFrame = false;
    m_switchClock.invalidate();
    m_frameRateClock.invalidate();
    m_framesInWindow = 0;
    m_videoAdapter->stop();
    m_audioAdapter->stop(m_videoAdapter->audioContext());
    m_audioConfigured = false;
    m_previewCaptureId.clear();
    m_previewFormatKey.clear();
    m_previewAudioId.clear();
    updateStatusForConfiguration();
}

bool CaptureController::switchInput(int input)
{
    const auto fail = [this](const QString &message) {
        m_switchState = "error";
        m_switchStatus = message;
        emit changed();
        return false;
    };
    if (input < 1 || input > 8)
        return fail("Choose an HDMI input from 1 to 8.");
    if (m_captureDeviceId.isEmpty()) {
        return fail("Choose the HDMI capture card before switching inputs.");
    }
    if (m_serialPortName.isEmpty() || !m_serialPortNames.contains(m_serialPortName)) {
        return fail("Connect and choose the RS-232 switch port before switching inputs.");
    }
    QString commandError;
    const auto command = commandForInput(m_commandTemplate, input, &commandError);
    if (command.isEmpty()) {
        return fail(commandError.isEmpty() ? "Choose a switch command from the device manual." : commandError);
    }
    if (!canSwitchInput(input)) {
        return fail("Capture card, serial port, or saved video format is unavailable. Refresh devices and check the setup page.");
    }
    if (m_serial.isOpen() || m_serialWriter.isSending()) {
        m_switchStatus = "The previous switch command is still being sent. Please wait.";
        emit changed();
        return false;
    }
    m_serial.setPortName(m_serialPortName);
    m_serial.setBaudRate(m_baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial.open(QIODevice::WriteOnly)) {
        return fail("Could not open the RS-232 switch: " + m_serial.errorString());
    }
    m_serialWriteLatencyMs = -1;
    m_firstFrameCallbackLatencyMs = -1;
    m_activeInput = input;
    m_switchStatus = "Sending the switch command for " + inputLabel(input) + ". Verify the picture after it arrives.";
    selectInput(input);
    m_switchClock.restart();
    if (!m_serialWriter.start(&m_serial, command, 2500)) {
        if (m_serialWriter.state() != "error" && m_serialWriter.state() != "cancelled")
            serialWriteFailed("Could not queue the complete RS-232 command.");
        return false;
    }
    m_receivedFrame = false;
    m_previewClock.restart();
    m_lastFrameClock.invalidate();
    setStatus("awaiting-frames", "Input switch requested. Video must be checked before treating it as active.");
    emit changed();
    if (!m_previewRequested) startPreview();
    return true;
}

void CaptureController::cancelSwitch()
{
    if (!m_serialWriter.isSending() && !m_serial.isOpen()) return;
    m_serialWriter.cancel();
    if (m_serial.isOpen()) {
        m_serial.close();
    }
    m_switchClock.invalidate();
    m_serialWriteLatencyMs = -1;
    m_firstFrameCallbackLatencyMs = -1;
    m_switchState = "cancelled";
    m_switchStatus = "Switch command cancelled before write completion; the switch may have received partial bytes. Verify its state before retrying.";
    emit changed();
}

void CaptureController::serialWriteFailed(const QString &message)
{
    if (m_serial.isOpen()) m_serial.close();
    m_switchClock.invalidate();
    m_serialWriteLatencyMs = -1;
    m_firstFrameCallbackLatencyMs = -1;
    m_switchState = "error";
    m_switchStatus = message;
    emit changed();
}

void CaptureController::setStatus(const QString &state, const QString &message)
{
    if (m_state == state && m_status == message) return;
    m_state = state;
    m_status = message;
    emit changed();
}