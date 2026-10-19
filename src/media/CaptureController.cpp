#include "CaptureController.h"

#include <QCameraFormat>
#include <QMediaDevices>
#include <QSerialPortInfo>
#include <QSettings>
#include <QVideoFrame>
#include <QtGlobal>

namespace {
constexpr int MaxCommandBytes = 128;
const QList<int> SupportedBaudRates{9600, 19200, 38400, 57600, 115200};

QSettings captureSettings()
{
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Isolde", "Capture");
}
}

CaptureController::CaptureController(QObject *parent) : QObject(parent),
    m_camera(new QCamera(this)), m_audioInput(new QAudioInput(this)), m_audioOutput(new QAudioOutput(this))
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
    m_serialWatchdog.setSingleShot(true);
    m_serialWatchdog.setInterval(2500);
    connect(&m_serialWatchdog, &QTimer::timeout, this, [this] {
        if (m_serialBytesPending > 0) serialWriteFailed("The serial switch did not accept the command in time.");
    });
    connect(&m_mediaDevices, &QMediaDevices::videoInputsChanged, this, &CaptureController::refreshDevices);
    connect(&m_mediaDevices, &QMediaDevices::audioInputsChanged, this, &CaptureController::refreshDevices);
    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        if (m_previewRequested) setStatus("error", "Capture card could not start: " + message);
    });
    connect(&m_serial, &QSerialPort::bytesWritten, this, [this](qint64 count) {
        if (m_serialBytesPending <= 0) return;
        m_serialBytesPending -= count;
        if (m_serialBytesPending > 0) return;
        m_serialWatchdog.stop();
        m_serial.close();
        if (m_activeInput > 0) {
            m_switchStatus = "Command sent for " + inputLabel(m_activeInput) + ". Confirm the preview shows that source.";
            emit changed();
        }
    });
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError || m_serialBytesPending <= 0) return;
        serialWriteFailed("Serial switch error: " + m_serial.errorString());
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
    auto settings = captureSettings();
    settings.setValue(key, value);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setStatus("error", "Could not save capture settings. Check available storage and try again.");
        return false;
    }
    return true;
}

QVariantList CaptureController::videoDevices() const
{
    QVariantList result;
    for (const auto &device : m_videoDevices) {
        QStringList formats;
        for (const auto &format : device.videoFormats()) {
            const auto size = format.resolution();
            formats.append(QString("%1 × %2, %3–%4 fps").arg(size.width()).arg(size.height())
                .arg(format.minFrameRate(), 0, 'f', 0).arg(format.maxFrameRate(), 0, 'f', 0));
        }
        formats.removeDuplicates();
        result.append(QVariantMap{{"id", QString::fromLatin1(device.id())}, {"name", device.description()},
            {"formats", formats.mid(0, 8).join(" · ")}});
    }
    return result;
}

QVariantList CaptureController::videoFormats() const
{
    QVariantList result;
    for (const auto &device : m_videoDevices) {
        if (QString::fromLatin1(device.id()) != m_captureDeviceId) continue;
        for (const auto &format : device.videoFormats()) {
            const auto size = format.resolution();
            const auto key = formatKey(format);
            const auto label = QString("%1 × %2 · %3–%4 fps · format %5")
                .arg(size.width()).arg(size.height()).arg(format.minFrameRate(), 0, 'f', 0)
                .arg(format.maxFrameRate(), 0, 'f', 0).arg(static_cast<int>(format.pixelFormat()));
            bool duplicate = false;
            for (const auto &entry : result) duplicate |= entry.toMap().value("key").toString() == key;
            if (!duplicate) result.append(QVariantMap{{"key", key}, {"name", label}, {"selected", key == m_videoFormatKey}});
        }
        break;
    }
    return result;
}

QVariantList CaptureController::audioDevices() const
{
    QVariantList result;
    for (const auto &device : m_audioDevices)
        result.append(QVariantMap{{"id", QString::fromLatin1(device.id())}, {"name", device.description()}});
    return result;
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

QString CaptureController::formatKey(const QCameraFormat &format) const
{
    const auto size = format.resolution();
    return QString("%1x%2@%3-%4#%5").arg(size.width()).arg(size.height())
        .arg(format.minFrameRate(), 0, 'f', 2).arg(format.maxFrameRate(), 0, 'f', 2)
        .arg(static_cast<int>(format.pixelFormat()));
}

void CaptureController::refreshDevices()
{
    m_videoDevices = QMediaDevices::videoInputs();
    m_audioDevices = QMediaDevices::audioInputs();
    m_serialPortNames.clear();
    for (const auto &port : QSerialPortInfo::availablePorts()) m_serialPortNames.append(port.portName());
    if (m_previewRequested) {
        bool present = false;
        for (const auto &device : m_videoDevices) present |= QString::fromLatin1(device.id()) == m_captureDeviceId;
        if (!present) {
            stopPreview();
            setStatus("device-missing", "Capture card disconnected. Reconnect it, then choose it again.");
            return;
        }
    }
    updateStatusForConfiguration();
    emit changed();
}

void CaptureController::updateStatusForConfiguration()
{
    bool videoPresent = m_captureDeviceId.isEmpty();
    bool formatPresent = m_videoFormatKey.isEmpty();
    for (const auto &device : m_videoDevices) if (QString::fromLatin1(device.id()) == m_captureDeviceId) {
        videoPresent = true;
        for (const auto &format : device.videoFormats()) formatPresent |= formatKey(format) == m_videoFormatKey;
    }
    if (m_captureDeviceId.isEmpty()) setStatus("needs-capture-card", "Connect and choose the HDMI capture card.");
    else if (!videoPresent) setStatus("device-missing", "The saved capture card is not connected. Reconnect it or choose another card.");
    else if (!formatPresent) setStatus("format-missing", "The saved video format is no longer supported. Choose another format or use the card default.");
    else if (m_serialPortName.isEmpty()) setStatus("needs-switch", "Capture card found. Choose the RS-232 switch or preview the card directly.");
    else if (!m_serialPortNames.contains(m_serialPortName)) setStatus("serial-missing", "The saved RS-232 port is not connected. Reconnect the switch or choose another port.");
    else if (m_commandTemplate.isEmpty() || commandForInput(m_commandTemplate, 1).isEmpty()) {
        m_switchStatus = m_commandTemplate.isEmpty() ? "No switch command is configured."
            : "Saved switch command is invalid. Enter the exact command from the switch manual.";
        setStatus("needs-protocol", "Capture card and serial port are selected. Configure a valid command from the switch manual.");
    }
    else setStatus("ready", "Capture card is ready. Send a test switch command or preview the current HDMI signal.");
}

void CaptureController::setVideoOutput(QObject *output)
{
    if (m_videoOutput == output) return;
    if (m_frameConnection) disconnect(m_frameConnection);
    if (m_outputDestroyedConnection) disconnect(m_outputDestroyedConnection);
    m_videoOutput = output;
    m_captureSession.setVideoOutput(output);
    if (output) {
        auto *sink = m_captureSession.videoSink();
        m_frameConnection = connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (!m_previewRequested || !frame.isValid()) return;
            m_receivedFrame = true;
            m_lastFrameClock.restart();
            if (m_state != "receiving-frames")
                setStatus("receiving-frames", "Video frames are arriving. Check that the picture matches the selected HDMI source.");
        });
        m_outputDestroyedConnection = connect(output, &QObject::destroyed, this, [this] {
            m_videoOutput = nullptr;
            m_captureSession.setVideoOutput(nullptr);
            stopPreview();
        });
    } else stopPreview();
}

bool CaptureController::setCaptureDevice(const QString &id)
{
    bool found = id.isEmpty();
    for (const auto &device : m_videoDevices) found |= QString::fromLatin1(device.id()) == id;
    if (!found) return false;
    QString nextFormat = m_videoFormatKey;
    bool formatAvailable = false;
    for (const auto &device : m_videoDevices) if (QString::fromLatin1(device.id()) == id)
        for (const auto &format : device.videoFormats()) formatAvailable |= formatKey(format) == nextFormat;
    if (!formatAvailable) nextFormat.clear();
    if (!saveValue("capture/device", id)) return false;
    if (nextFormat != m_videoFormatKey && !saveValue("capture/format", nextFormat)) return false;
    stopPreview();
    m_captureDeviceId = id;
    m_videoFormatKey = nextFormat;
    updateStatusForConfiguration();
    emit changed();
    return true;
}

bool CaptureController::setVideoFormat(const QString &key)
{
    bool found = key.isEmpty();
    QCameraFormat selected;
    for (const auto &device : m_videoDevices) if (QString::fromLatin1(device.id()) == m_captureDeviceId)
        for (const auto &format : device.videoFormats()) if (formatKey(format) == key) { found = true; selected = format; }
    if (!found || (key.isEmpty() && m_captureDeviceId.isEmpty())) return false;
    if (!saveValue("capture/format", key)) return false;
    const bool restart = m_previewRequested;
    if (restart) stopPreview();
    m_videoFormatKey = key;
    if (!key.isEmpty()) m_camera->setCameraFormat(selected);
    emit changed();
    return !restart || startPreview();
}

bool CaptureController::setAudioDevice(const QString &id)
{
    bool found = id.isEmpty();
    for (const auto &device : m_audioDevices) found |= QString::fromLatin1(device.id()) == id;
    if (!found) return false;
    if (!saveValue("capture/audio", id)) return false;
    m_audioDeviceId = id;
    if (m_previewRequested) configureAudio();
    emit changed();
    return true;
}

bool CaptureController::setSerialPort(const QString &portName)
{
    if (!portName.isEmpty() && !m_serialPortNames.contains(portName)) return false;
    if (m_serialBytesPending > 0) {
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
    if (input < 1 || input > 8 || clean.isEmpty() || clean.size() > 40) return false;
    for (const auto character : clean) if (character.isNull() || character.isSpace() && character != QLatin1Char(' ')) return false;
    if (!saveValue(QString("inputs/%1/name").arg(input), clean)) return false;
    m_inputLabels.insert(input, clean);
    emit changed();
    return true;
}

void CaptureController::selectInput(int input)
{
    if (input < 1 || input > 8 || input == m_selectedInput) return;
    if (!saveValue("inputs/selected", input)) return;
    m_selectedInput = input;
    emit changed();
}

bool CaptureController::canSwitchInput(int input) const
{
    if (input < 1 || input > 8 || m_captureDeviceId.isEmpty() || m_serialPortName.isEmpty()
        || !m_serialPortNames.contains(m_serialPortName)) return false;
    bool videoPresent = false;
    bool formatPresent = m_videoFormatKey.isEmpty();
    for (const auto &device : m_videoDevices) if (QString::fromLatin1(device.id()) == m_captureDeviceId) {
        videoPresent = true;
        for (const auto &format : device.videoFormats()) formatPresent |= formatKey(format) == m_videoFormatKey;
    }
    if (!videoPresent || !formatPresent) return false;
    const auto command = commandForInput(m_commandTemplate, input);
    return !command.isEmpty() && m_baudRate > 0;
}

void CaptureController::configureAudio()
{
    QAudioDevice selected;
    for (const auto &device : m_audioDevices)
        if (QString::fromLatin1(device.id()) == m_audioDeviceId) selected = device;
    if (selected.isNull()) {
        m_captureSession.setAudioInput(nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        m_captureSession.setAudioOutput(nullptr);
#endif
        return;
    }
    m_audioInput->setDevice(selected);
    m_captureSession.setAudioInput(m_audioInput);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    m_audioOutput->setDevice(QMediaDevices::defaultAudioOutput());
    m_captureSession.setAudioOutput(m_audioOutput);
#else
    m_captureSession.setAudioInput(nullptr);
#endif
}

bool CaptureController::startPreview()
{
    if (m_previewRequested) return true;
    if (m_captureDeviceId.isEmpty()) {
        setStatus("needs-capture-card", "Choose the HDMI capture card before starting preview.");
        return false;
    }
    if (!m_videoOutput) {
        setStatus("needs-preview-surface", "Open the preview screen before starting capture.");
        return false;
    }
    QCameraDevice selected;
    for (const auto &device : m_videoDevices)
        if (QString::fromLatin1(device.id()) == m_captureDeviceId) selected = device;
    if (selected.isNull()) {
        setStatus("device-missing", "Capture card is not connected. Reconnect it, then refresh devices.");
        return false;
    }
    m_camera->setCameraDevice(selected);
    if (!m_videoFormatKey.isEmpty())
        for (const auto &format : selected.videoFormats())
            if (formatKey(format) == m_videoFormatKey) m_camera->setCameraFormat(format);
    m_captureSession.setCamera(m_camera);
    configureAudio();
    m_previewRequested = true;
    m_receivedFrame = false;
    m_previewClock.restart();
    m_lastFrameClock.invalidate();
    m_frameWatchdog.start();
    QString startupStatus = "Starting capture card. Waiting for video frames…";
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    if (!m_audioDeviceId.isEmpty()) startupStatus += " Audio monitoring needs Qt 6.8 or newer; video preview only on this system.";
#endif
    setStatus("awaiting-frames", startupStatus);
    m_camera->start();
    return true;
}

void CaptureController::stopPreview()
{
    m_frameWatchdog.stop();
    m_previewRequested = false;
    m_receivedFrame = false;
    if (m_camera->isActive()) m_camera->stop();
    m_captureSession.setAudioInput(nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    m_captureSession.setAudioOutput(nullptr);
#endif
    updateStatusForConfiguration();
}

bool CaptureController::switchInput(int input)
{
    if (input < 1 || input > 8) return false;
    if (m_captureDeviceId.isEmpty()) {
        m_switchStatus = "Choose the HDMI capture card before switching inputs.";
        emit changed();
        return false;
    }
    if (m_serialPortName.isEmpty() || !m_serialPortNames.contains(m_serialPortName)) {
        m_switchStatus = "Connect and choose the RS-232 switch port before switching inputs.";
        emit changed();
        return false;
    }
    QString commandError;
    const auto command = commandForInput(m_commandTemplate, input, &commandError);
    if (command.isEmpty()) {
        m_switchStatus = commandError.isEmpty() ? "Choose a switch command from the device manual." : commandError;
        emit changed();
        return false;
    }
    if (!canSwitchInput(input)) {
        m_switchStatus = "Capture card, serial port, or saved video format is unavailable. Refresh devices and check the setup page.";
        emit changed();
        return false;
    }
    if (m_serial.isOpen() || m_serialBytesPending > 0) {
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
        m_switchStatus = "Could not open the RS-232 switch: " + m_serial.errorString();
        emit changed();
        return false;
    }
    const auto count = m_serial.write(command);
    if (count != command.size()) {
        serialWriteFailed("Could not queue the complete RS-232 command: " + m_serial.errorString());
        return false;
    }
    m_serialBytesPending = count;
    m_activeInput = input;
    selectInput(input);
    m_serialWatchdog.start();
    m_switchStatus = "Sending the switch command for " + inputLabel(input) + ". Verify the picture after it arrives.";
    m_receivedFrame = false;
    m_previewClock.restart();
    m_lastFrameClock.invalidate();
    setStatus("awaiting-frames", "Input switch requested. Video must be checked before treating it as active.");
    emit changed();
    if (!m_previewRequested) startPreview();
    return true;
}

void CaptureController::serialWriteFailed(const QString &message)
{
    m_serialWatchdog.stop();
    m_serialBytesPending = 0;
    if (m_serial.isOpen()) m_serial.close();
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