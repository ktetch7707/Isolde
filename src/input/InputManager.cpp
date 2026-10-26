#include "input/InputManager.h"
#include "input/WebRemoteScript.h"

#include <Qt>

InputManager::InputManager(QObject *parent) : QObject(parent) {}
QString InputManager::webRemoteScript(const QVariantMap &appearance) const { return ::webRemoteScript(appearance); }

InputManager::Command InputManager::commandForKey(int key) const
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
