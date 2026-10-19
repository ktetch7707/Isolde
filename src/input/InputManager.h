#pragma once

#include <QObject>
#include <QString>

class InputManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(Command none READ none CONSTANT)
    Q_PROPERTY(Command navigateUp READ navigateUp CONSTANT)
    Q_PROPERTY(Command navigateDown READ navigateDown CONSTANT)
    Q_PROPERTY(Command navigateLeft READ navigateLeft CONSTANT)
    Q_PROPERTY(Command navigateRight READ navigateRight CONSTANT)
    Q_PROPERTY(Command confirm READ confirm CONSTANT)
    Q_PROPERTY(Command back READ back CONSTANT)
    Q_PROPERTY(Command home READ home CONSTANT)
    Q_PROPERTY(Command volumeUp READ volumeUp CONSTANT)
    Q_PROPERTY(Command volumeDown READ volumeDown CONSTANT)
    Q_PROPERTY(Command mute READ mute CONSTANT)
    Q_PROPERTY(Command power READ power CONSTANT)
    Q_PROPERTY(Command playPause READ playPause CONSTANT)
    Q_PROPERTY(Command menu READ menu CONSTANT)
    Q_PROPERTY(Command search READ search CONSTANT)

public:
    enum Command {
        None,
        NavigateUp,
        NavigateDown,
        NavigateLeft,
        NavigateRight,
        Confirm,
        Back,
        Home,
        VolumeUp,
        VolumeDown,
        Mute,
        Power,
        PlayPause,
        Menu,
        Search
    };
    Q_ENUM(Command)

    explicit InputManager(QObject *parent = nullptr);
    Q_INVOKABLE Command commandForKey(int key) const;
    Q_INVOKABLE QString labelForCommand(Command command) const;

    Command none() const { return None; }
    Command navigateUp() const { return NavigateUp; }
    Command navigateDown() const { return NavigateDown; }
    Command navigateLeft() const { return NavigateLeft; }
    Command navigateRight() const { return NavigateRight; }
    Command confirm() const { return Confirm; }
    Command back() const { return Back; }
    Command home() const { return Home; }
    Command volumeUp() const { return VolumeUp; }
    Command volumeDown() const { return VolumeDown; }
    Command mute() const { return Mute; }
    Command power() const { return Power; }
    Command playPause() const { return PlayPause; }
    Command menu() const { return Menu; }
    Command search() const { return Search; }
};
