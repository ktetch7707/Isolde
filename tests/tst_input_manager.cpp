/****************************************************************************
** $QT_BEGIN_HEADER**
** $QT_END_HEADER**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtCore/QObject>
#include <QtCore/QString>
#include "../src/input/InputManager.h"

class InputManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void test_commandForKey_mapping();
    void test_commandForKey_edge_cases();
    void test_labelForCommand_labels();
    void test_labelForCommand_edge_cases();
};

void InputManagerTests::test_commandForKey_mapping()
{
    InputManager manager;

    // Test all standard key mappings
    QCOMPARE(manager.commandForKey(Qt::Key_Up), InputManager::NavigateUp);
    QCOMPARE(manager.commandForKey(Qt::Key_Down), InputManager::NavigateDown);
    QCOMPARE(manager.commandForKey(Qt::Key_Left), InputManager::NavigateLeft);
    QCOMPARE(manager.commandForKey(Qt::Key_Right), InputManager::NavigateRight);
    QCOMPARE(manager.commandForKey(Qt::Key_Return), InputManager::Confirm);
    QCOMPARE(manager.commandForKey(Qt::Key_Enter), InputManager::Confirm);
    QCOMPARE(manager.commandForKey(Qt::Key_Space), InputManager::Confirm);
    QCOMPARE(manager.commandForKey(Qt::Key_Escape), InputManager::Back);
    QCOMPARE(manager.commandForKey(Qt::Key_Back), InputManager::Back);
    QCOMPARE(manager.commandForKey(Qt::Key_Home), InputManager::Home);
    QCOMPARE(manager.commandForKey(Qt::Key_H), InputManager::Home);
    QCOMPARE(manager.commandForKey(Qt::Key_VolumeUp), InputManager::VolumeUp);
    QCOMPARE(manager.commandForKey(Qt::Key_Plus), InputManager::VolumeUp);
    QCOMPARE(manager.commandForKey(Qt::Key_VolumeDown), InputManager::VolumeDown);
    QCOMPARE(manager.commandForKey(Qt::Key_Minus), InputManager::VolumeDown);
    QCOMPARE(manager.commandForKey(Qt::Key_VolumeMute), InputManager::Mute);
    QCOMPARE(manager.commandForKey(Qt::Key_M), InputManager::Mute);
    QCOMPARE(manager.commandForKey(Qt::Key_PowerOff), InputManager::Power);
    QCOMPARE(manager.commandForKey(Qt::Key_Suspend), InputManager::Power);
    QCOMPARE(manager.commandForKey(Qt::Key_MediaTogglePlayPause), InputManager::PlayPause);
    QCOMPARE(manager.commandForKey(Qt::Key_MediaPlay), InputManager::PlayPause);
    QCOMPARE(manager.commandForKey(Qt::Key_MediaPause), InputManager::PlayPause);
    QCOMPARE(manager.commandForKey(Qt::Key_Menu), InputManager::Menu);
    QCOMPARE(manager.commandForKey(Qt::Key_Search), InputManager::Search);
}

void InputManagerTests::test_commandForKey_edge_cases()
{
    InputManager manager;

    // Test default case returns None
    QCOMPARE(manager.commandForKey(Qt::Key_unknown), InputManager::None);
    QCOMPARE(manager.commandForKey(999), InputManager::None);

    // Verify None is the default
    QCOMPARE(manager.commandForKey(0), InputManager::None);
}

void InputManagerTests::test_labelForCommand_labels()
{
    InputManager manager;

    // Test all command labels
    QCOMPARE(manager.labelForCommand(InputManager::NavigateUp), QStringLiteral("Up"));
    QCOMPARE(manager.labelForCommand(InputManager::NavigateDown), QStringLiteral("Down"));
    QCOMPARE(manager.labelForCommand(InputManager::NavigateLeft), QStringLiteral("Left"));
    QCOMPARE(manager.labelForCommand(InputManager::NavigateRight), QStringLiteral("Right"));
    QCOMPARE(manager.labelForCommand(InputManager::Confirm), QStringLiteral("OK / Select"));
    QCOMPARE(manager.labelForCommand(InputManager::Back), QStringLiteral("Back"));
    QCOMPARE(manager.labelForCommand(InputManager::Home), QStringLiteral("Home"));
    QCOMPARE(manager.labelForCommand(InputManager::VolumeUp), QStringLiteral("Volume +"));
    QCOMPARE(manager.labelForCommand(InputManager::VolumeDown), QStringLiteral("Volume -"));
    QCOMPARE(manager.labelForCommand(InputManager::Mute), QStringLiteral("Mute"));
    QCOMPARE(manager.labelForCommand(InputManager::Power), QStringLiteral("Power"));
    QCOMPARE(manager.labelForCommand(InputManager::PlayPause), QStringLiteral("Play / Pause"));
    QCOMPARE(manager.labelForCommand(InputManager::Menu), QStringLiteral("Menu"));
    QCOMPARE(manager.labelForCommand(InputManager::Search), QStringLiteral("Search"));
    QCOMPARE(manager.labelForCommand(InputManager::None), QStringLiteral("Unknown input"));
}

void InputManagerTests::test_labelForCommand_edge_cases()
{
    InputManager manager;

    // Test unknown command returns default label
    QCOMPARE(manager.labelForCommand(static_cast<InputManager::Command>(999)), QStringLiteral("Unknown input"));
}