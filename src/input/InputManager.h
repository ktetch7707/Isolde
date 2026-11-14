#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QMap>
#include <QTimer>
#include <QPointer>
#include <QSet>

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
    Q_PROPERTY(QVariantList bindingRows READ bindingRows NOTIFY bindingsChanged)
    Q_PROPERTY(bool bindingActive READ bindingActive NOTIFY bindingsChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY bindingsChanged)
    Q_PROPERTY(QString bindingStatus READ bindingStatus NOTIFY bindingsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY bindingsChanged)
    Q_PROPERTY(bool acceptTextKeys READ acceptTextKeys NOTIFY bindingsChanged)

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
    explicit InputManager(const QString &storagePath, QObject *parent = nullptr);
    ~InputManager() override;
    Q_INVOKABLE Command commandForKey(int key) const;
    Q_INVOKABLE QString labelForCommand(Command command) const;
    Q_INVOKABLE QString webRemoteScript(const QVariantMap &appearance) const;
    QVariantMap webRemoteOptions(const QVariantMap &appearance) const;
    QString nativeReturnShortcut() const;
    QVariantList bindingRows() const;
    bool bindingActive() const { return m_target != None; }
    bool listening() const { return bindingActive() && !m_candidate; }
    QString bindingStatus() const { return m_status; }
    QString errorString() const { return m_error; }
    Q_INVOKABLE bool beginBinding(int command);
    Q_INVOKABLE bool saveBinding();
    Q_INVOKABLE void cancelBinding();
    Q_INVOKABLE bool clearBinding(int command);
    Q_INVOKABLE bool resetBindings();
    bool acceptTextKeys() const { return m_acceptTextKeys; }
    Q_INVOKABLE bool setAcceptTextKeys(bool enabled);
    Q_INVOKABLE void setTextInputTarget(QObject *target, bool active);

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
signals:
    void bindingsChanged();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    static Command defaultCommandForKey(int key);
    bool persist(const QMap<int, int> &bindings, bool acceptTextKeys);
    void loadBindings();
    QString m_storagePath, m_status, m_error;
    QMap<int, int> m_bindings; // command -> additional key; standard keys remain available
    Command m_target = None;
    int m_candidate = 0;
    int m_releaseKey = 0;
    QTimer m_captureDeadline;
    bool m_acceptTextKeys = true;
    QList<QPointer<QObject>> m_textInputTargets;
    QSet<int> m_textReleaseKeys;
};
