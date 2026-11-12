#include "input/InputManager.h"
#include "input/WebRemoteScript.h"

#include <Qt>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>

InputManager::InputManager(QObject *parent) : InputManager(QString{}, parent) {}
InputManager::InputManager(const QString &storagePath, QObject *parent)
    : QObject(parent), m_storagePath(storagePath)
{
    static const bool resourcesReady = [] { Q_INIT_RESOURCE(remote_buttons); return true; }();
    Q_UNUSED(resourcesReady);
    if (qApp) qApp->installEventFilter(this);
    m_captureDeadline.setSingleShot(true);
    connect(&m_captureDeadline, &QTimer::timeout, this, [this] {
        cancelBinding(); m_status = tr("No binding saved. Select an action to try again."); emit bindingsChanged();
    });
    loadBindings();
}
InputManager::~InputManager() { if (qApp) qApp->removeEventFilter(this); }
QString InputManager::webRemoteScript(const QVariantMap &appearance) const { return ::webRemoteScript(appearance); }

InputManager::Command InputManager::commandForKey(int key) const
{
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it)
        if (it.value() == key) return static_cast<Command>(it.key());
    return defaultCommandForKey(key);
}

InputManager::Command InputManager::defaultCommandForKey(int key)
{
    switch (key) {
    case Qt::Key_Up: return NavigateUp;
    case Qt::Key_Down: return NavigateDown;
    case Qt::Key_Left: return NavigateLeft;
    case Qt::Key_Right: return NavigateRight;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: return Confirm;
    case Qt::Key_Escape:
    case Qt::Key_Back: return Back;
    case Qt::Key_Home:
    case Qt::Key_H: return Home;
    case Qt::Key_VolumeUp:
    case Qt::Key_Plus:
    case Qt::Key_Equal: return VolumeUp;
    case Qt::Key_VolumeDown:
    case Qt::Key_Minus: return VolumeDown;
    case Qt::Key_VolumeMute:
    case Qt::Key_M: return Mute;
    case Qt::Key_PowerOff:
    case Qt::Key_Suspend: return Power;
    case Qt::Key_MediaTogglePlayPause:
    case Qt::Key_MediaPlay:
    case Qt::Key_MediaPause: return PlayPause;
    case Qt::Key_Menu: return Menu;
    case Qt::Key_Search: return Search;
    default: return None;
    }
}

QString InputManager::labelForCommand(Command command) const
{
    switch (command) {
    case NavigateUp: return QStringLiteral("Up");
    case NavigateDown: return QStringLiteral("Down");
    case NavigateLeft: return QStringLiteral("Left");
    case NavigateRight: return QStringLiteral("Right");
    case Confirm: return QStringLiteral("OK / Select");
    case Back: return QStringLiteral("Back");
    case Home: return QStringLiteral("Home");
    case VolumeUp: return QStringLiteral("Volume +");
    case VolumeDown: return QStringLiteral("Volume -");
    case Mute: return QStringLiteral("Mute");
    case Power: return QStringLiteral("Power");
    case PlayPause: return QStringLiteral("Play / Pause");
    case Menu: return QStringLiteral("Menu");
    case Search: return QStringLiteral("Search");
    case None: return QStringLiteral("Unknown input");
    }
    return QStringLiteral("Unknown input");
}

namespace {
bool safeBindingPath(QString path)
{
    if (!QDir::isAbsolutePath(path)) return false;
    for (;;) {
        const QFileInfo info(path);
        if (info.isSymLink()) return false;
        const auto parent = info.absolutePath();
        if (parent == path) return true;
        path = parent;
    }
}
bool usableKey(int key)
{
    switch (key) {
    case Qt::Key_Shift: case Qt::Key_Control: case Qt::Key_Meta: case Qt::Key_Alt:
    case Qt::Key_AltGr: case Qt::Key_CapsLock: case Qt::Key_NumLock: case Qt::Key_ScrollLock:
    case Qt::Key_Super_L: case Qt::Key_Super_R: case Qt::Key_Hyper_L: case Qt::Key_Hyper_R:
        return false;
    default: break;
    }
    return key >= Qt::Key_Space && key < Qt::Key_unknown
        && key != Qt::Key_Escape;
}
QString keyLabel(int key)
{
    const auto label = QKeySequence(key).toString(QKeySequence::NativeText);
    return label.isEmpty() ? QStringLiteral("Button %1").arg(key) : label;
}
}

QVariantList InputManager::bindingRows() const
{
    QFile file(QStringLiteral(":/config/remote-buttons.json"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(file.readAll()).object();
    if (doc.value("schemaVersion").toInt() != 1) return {};
    QVariantList result;
    QSet<int> seen;
    for (const auto &value : doc.value("actions").toArray()) {
        const auto row = value.toObject();
        const int command = row.value("command").toInt();
        if (command <= None || command > Search || seen.contains(command)
            || row.value("name").toString().isEmpty() || row.value("category").toString().isEmpty()) return {};
        seen.insert(command);
        auto data = row.toVariantMap();
        data["custom"] = m_bindings.contains(command);
        data["value"] = m_bindings.contains(command) ? keyLabel(m_bindings.value(command)) : row.value("defaultLabel").toString();
        result.append(data);
    }
    return seen.size() == Search ? result : QVariantList{};
}

bool InputManager::beginBinding(int command)
{
    if (command <= None || command > Search) return false;
    m_target = static_cast<Command>(command);
    m_candidate = 0;
    m_error.clear();
    m_status = (m_target == Back
        ? tr("Press the remote button for %1. Press Back twice to cancel; Escape or waiting 20 seconds also cancels.")
        : tr("Press the remote button for %1. Back or Escape cancels; waiting 20 seconds also cancels.")).arg(labelForCommand(m_target));
    m_captureDeadline.start(20000);
    emit bindingsChanged();
    return true;
}

bool InputManager::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    if (event->type() == QEvent::ApplicationDeactivate && bindingActive()) cancelBinding();
    if (event->type() == QEvent::KeyRelease) {
        const auto key = static_cast<QKeyEvent *>(event);
        if (m_textReleaseKeys.contains(key->key())) {
            if (!key->isAutoRepeat()) m_textReleaseKeys.remove(key->key());
            return true;
        }
        if (key->key() == m_releaseKey) {
            if (!key->isAutoRepeat()) m_releaseKey = 0;
            return true;
        }
    }
    if (event->type() != QEvent::KeyPress) return false;
    const auto key = static_cast<QKeyEvent *>(event);
    if (m_releaseKey == key->key() && key->isAutoRepeat()) return true;
    if (!bindingActive() && m_acceptTextKeys) {
        for (auto it = m_textInputTargets.rbegin(); it != m_textInputTargets.rend(); ++it) {
            const QPointer<QObject> target = *it;
            if (!target || !target->property("visible").toBool() || !target->property("enabled").toBool()) continue;
            QVariant handled;
            const bool invoked = QMetaObject::invokeMethod(target, "handlePhysicalKey", Qt::DirectConnection,
                Q_RETURN_ARG(QVariant, handled), Q_ARG(QVariant, key->key()), Q_ARG(QVariant, key->text()),
                Q_ARG(QVariant, static_cast<int>(key->modifiers())), Q_ARG(QVariant, key->isAutoRepeat()));
            if (invoked && handled.toBool()) {
                m_textReleaseKeys.insert(key->key());
                return true;
            }
            break; // only the frontmost visible keyboard receives text
        }
    }
    if (m_textReleaseKeys.contains(key->key()) && key->isAutoRepeat()) return true;
    if (!listening()) return false;
    if (key->isAutoRepeat()) return true;
    m_releaseKey = key->key();
    if (key->key() == Qt::Key_Escape) { cancelBinding(); return true; }
    if (commandForKey(key->key()) == Back && m_target != Back) { cancelBinding(); return true; }
    if (!usableKey(key->key()) || (key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        m_error = tr("Use a single remote button, without Ctrl, Alt or Meta.");
    } else {
        const auto assigned = commandForKey(key->key());
        if (assigned != None && assigned != m_target) {
            m_error = tr("%1 already controls %2. Choose another button; the existing binding is unchanged.")
                .arg(keyLabel(key->key()), labelForCommand(assigned));
        } else {
            m_candidate = key->key();
            m_error.clear();
            m_status = tr("Use %1 for %2? Select Save binding or Cancel. Standard buttons remain available.")
                .arg(keyLabel(m_candidate), labelForCommand(m_target));
            m_captureDeadline.start(30000);
        }
    }
    emit bindingsChanged();
    return true;
}

bool InputManager::saveBinding()
{
    if (!bindingActive() || !m_candidate) return false;
    auto bindings = m_bindings;
    bindings[m_target] = m_candidate;
    if (!persist(bindings, m_acceptTextKeys)) return false;
    m_bindings = bindings;
    cancelBinding();
    m_status = tr("Binding saved for this profile.");
    emit bindingsChanged();
    return true;
}

void InputManager::cancelBinding()
{
    m_captureDeadline.stop();
    m_target = None;
    m_candidate = 0;
    m_error.clear();
    m_status = tr("No binding changes pending.");
    emit bindingsChanged();
}

bool InputManager::clearBinding(int command)
{
    if (command <= None || command > Search) return false;
    auto bindings = m_bindings;
    bindings.remove(command);
    if (!persist(bindings, m_acceptTextKeys)) return false;
    m_bindings = bindings;
    cancelBinding();
    return true;
}

bool InputManager::resetBindings()
{
    if (!persist({}, m_acceptTextKeys)) return false;
    m_bindings.clear();
    cancelBinding();
    return true;
}

bool InputManager::setAcceptTextKeys(bool enabled)
{
    if (enabled == m_acceptTextKeys) return true;
    if (!persist(m_bindings, enabled)) return false;
    m_acceptTextKeys = enabled;
    emit bindingsChanged();
    return true;
}

void InputManager::setTextInputTarget(QObject *target, bool active)
{
    for (qsizetype i = m_textInputTargets.size(); i-- > 0;)
        if (!m_textInputTargets[i] || m_textInputTargets[i] == target) m_textInputTargets.removeAt(i);
    if (active && target && target->metaObject()->indexOfMethod("handlePhysicalKey(QVariant,QVariant,QVariant,QVariant)") >= 0)
        m_textInputTargets.append(target);
}

bool InputManager::persist(const QMap<int, int> &bindings, bool acceptTextKeys)
{
    auto fail = [this] {
        m_error = tr("Could not save remote input settings. Previous settings are unchanged. Check storage and retry.");
        emit bindingsChanged(); return false;
    };
    if (!safeBindingPath(m_storagePath) || !safeBindingPath(m_storagePath + ".bak")
        || !QDir().mkpath(QFileInfo(m_storagePath).absolutePath())) return fail();
    QJsonObject values;
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) values[QString::number(it.key())] = it.value();
    const auto bytes = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"bindings", values}, {"acceptTextKeys", acceptTextKeys}}).toJson();
    if (QFileInfo::exists(m_storagePath)) {
        QFile previous(m_storagePath);
        if (!previous.open(QIODevice::ReadOnly) || previous.size() > 8192) return fail();
        const auto old = previous.readAll();
        if (old.size() != previous.size()) return fail();
        QSaveFile backup(m_storagePath + ".bak");
        if (!backup.open(QIODevice::WriteOnly) || backup.write(old) != old.size() || !backup.commit()) return fail();
    }
    QSaveFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return fail();
    m_error.clear();
    return true;
}

void InputManager::loadBindings()
{
    if (m_storagePath.isEmpty() || !QFileInfo::exists(m_storagePath)) return;
    auto invalid = [this] { m_error = tr("Saved remote bindings are invalid or unreadable. Standard buttons are active; the saved file was preserved."); };
    QFile file(m_storagePath);
    if (!safeBindingPath(m_storagePath) || !file.open(QIODevice::ReadOnly) || file.size() > 8192) { invalid(); return; }
    const auto doc = QJsonDocument::fromJson(file.readAll()).object();
    if ((doc.size() != 2 && doc.size() != 3) || doc.value("schemaVersion") != QJsonValue(1) || !doc.value("bindings").isObject()
        || (doc.size() == 3 && (!doc.contains("acceptTextKeys") || !doc.value("acceptTextKeys").isBool()))) { invalid(); return; }
    const auto values = doc.value("bindings").toObject();
    if (values.size() > Search) { invalid(); return; }
    QMap<int, int> bindings;
    QSet<int> keys;
    for (auto it = values.begin(); it != values.end(); ++it) {
        const int command = it.key().toInt(), key = it.value().toInt();
        const auto standard = defaultCommandForKey(key);
        if (command <= None || command > Search || it.key() != QString::number(command)
            || !it.value().isDouble() || it.value().toDouble() != key || !usableKey(key)
            || keys.contains(key) || (standard != None && standard != command)) { invalid(); return; }
        bindings[command] = key;
        keys.insert(key);
    }
    m_bindings = bindings;
    m_acceptTextKeys = doc.value("acceptTextKeys").toBool(true);
}
