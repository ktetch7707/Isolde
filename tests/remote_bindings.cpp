#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include <QJSValue>
#include "input/InputManager.h"
#include "settings/RemoteSettings.h"

namespace {
QByteArray read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{}; }
bool write(const QString &path, const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
void press(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, bool repeat = false) {
    QObject receiver;
    QKeyEvent event(QEvent::KeyPress, key, modifiers, {}, repeat);
    QCoreApplication::sendEvent(&receiver, &event);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QCoreApplication::sendEvent(&receiver, &release);
}
}

class RemoteBindingTests : public QObject {
    Q_OBJECT
private slots:
    void captureSaveReloadAndProfileIsolation();
    void cancelConflictRepeatAndInvalidInput();
    void malformedStorageAndAtomicFailure();
    void remoteUiCaptureReviewCancelRestore();
    void physicalKeyboardTextAndControls();
    void textPreferenceCompatibilityAndRollback();
};

void RemoteBindingTests::captureSaveReloadAndProfileIsolation() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("one.json");
    InputManager manager(path);
    QCOMPARE(manager.bindingRows().size(), 14);
    QVERIFY(manager.beginBinding(InputManager::NavigateUp));
    press(Qt::Key_F6);
    QVERIFY(!manager.listening());
    QVERIFY(manager.bindingActive());
    QCOMPARE(manager.commandForKey(Qt::Key_F6), InputManager::None);
    QVERIFY(!QFile::exists(path));
    QVERIFY(manager.saveBinding());
    QCOMPARE(manager.commandForKey(Qt::Key_F6), InputManager::NavigateUp);
    QCOMPARE(manager.webRemoteOptions({}).value("keyBindings").toMap().value("f6").toString(),QString("up"));
    QCOMPARE(manager.commandForKey(Qt::Key_Up), InputManager::NavigateUp);
    InputManager reopened(path), other(dir.filePath("two.json"));
    QCOMPARE(reopened.commandForKey(Qt::Key_F6), InputManager::NavigateUp);
    QCOMPARE(other.commandForKey(Qt::Key_F6), InputManager::None);
    QVERIFY(!other.webRemoteOptions({}).value("keyBindings").toMap().contains("f6"));
    const auto old = read(path);
    QVERIFY(manager.beginBinding(InputManager::NavigateDown)); press(Qt::Key_F7);
    QVERIFY(manager.saveBinding());
    QCOMPARE(read(path + ".bak"), old);
    QVERIFY(manager.clearBinding(InputManager::NavigateUp));
    QCOMPARE(manager.commandForKey(Qt::Key_F6), InputManager::None);
    QCOMPARE(manager.commandForKey(Qt::Key_F7), InputManager::NavigateDown);
    QVERIFY(manager.resetBindings());
    QCOMPARE(manager.commandForKey(Qt::Key_F7), InputManager::None);
    QVERIFY(manager.webRemoteOptions({}).value("keyBindings").toMap().isEmpty());
}

void RemoteBindingTests::cancelConflictRepeatAndInvalidInput() {
    QTemporaryDir dir; const auto path = dir.filePath("bindings.json");
    InputManager manager(path);
    QVERIFY(!manager.beginBinding(100));
    QVERIFY(manager.beginBinding(InputManager::Confirm));
    press(Qt::Key_F8, Qt::NoModifier, true);
    QVERIFY(manager.listening());
    press(Qt::Key_Up);
    QVERIFY(manager.listening()); QVERIFY(manager.errorString().contains("already controls"));
    press(Qt::Key_unknown); QVERIFY(manager.listening());
    press(Qt::Key_F8, Qt::ControlModifier); QVERIFY(manager.listening());
    press(Qt::Key_F8); QVERIFY(!manager.listening());
    manager.cancelBinding(); QVERIFY(!QFile::exists(path));
    QCOMPARE(manager.commandForKey(Qt::Key_F8), InputManager::None);
    QVERIFY(manager.beginBinding(InputManager::Back)); press(Qt::Key_Escape);
    QVERIFY(!manager.bindingActive());
    QVERIFY(manager.beginBinding(InputManager::Confirm)); press(Qt::Key_Back);
    QVERIFY(!manager.bindingActive()); QVERIFY(!QFile::exists(path));
    QVERIFY(manager.beginBinding(InputManager::NavigateDown)); press(Qt::Key_F9); QVERIFY(manager.saveBinding());
    const auto before = read(path);
    QVERIFY(manager.beginBinding(InputManager::Home)); press(Qt::Key_F9);
    QVERIFY(manager.listening()); QVERIFY(!manager.saveBinding());
    QCOMPARE(read(path), before); manager.cancelBinding();
    // Profile teardown removes the raw capture filter without creating a file.
    { InputManager departing(dir.filePath("departing.json")); QVERIFY(departing.beginBinding(InputManager::Home)); }
    press(Qt::Key_F10); QVERIFY(!QFile::exists(dir.filePath("departing.json")));
    QVERIFY(manager.beginBinding(InputManager::Confirm));
    QEvent deactivate(QEvent::ApplicationDeactivate);
    QCoreApplication::sendEvent(qApp, &deactivate);
    QVERIFY(!manager.bindingActive()); QCOMPARE(read(path), before);
}

void RemoteBindingTests::malformedStorageAndAtomicFailure() {
    QTemporaryDir dir; const auto path = dir.filePath("bindings.json");
    const QList<QByteArray> invalid{
        R"({"schemaVersion":2,"bindings":{}})",
        R"({"schemaVersion":1,"bindings":{"1":16777235,"2":16777235}})",
        R"({"schemaVersion":1,"bindings":{"1":1.5}})",
        R"({"schemaVersion":1,"bindings":{"99":65}})", QByteArray(8193, 'x')};
    for (const auto &bytes : invalid) {
        QVERIFY(write(path, bytes)); InputManager manager(path);
        QVERIFY(!manager.errorString().isEmpty()); QCOMPARE(read(path), bytes);
        QCOMPARE(manager.commandForKey(Qt::Key_Up), InputManager::NavigateUp);
    }
    QVERIFY(QFile::remove(path));
    InputManager manager(path); QVERIFY(manager.beginBinding(InputManager::Home)); press(Qt::Key_F10);
    QVERIFY(manager.saveBinding()); const auto old = read(path);
    // A directory at the backup destination is a deterministic write failure.
    QVERIFY(QDir().mkdir(path + ".bak"));
    QVERIFY(manager.beginBinding(InputManager::Home)); press(Qt::Key_F11);
    QVERIFY(!manager.saveBinding()); QVERIFY(!manager.errorString().isEmpty());
    QCOMPARE(read(path), old);
    QCOMPARE(manager.commandForKey(Qt::Key_F10), InputManager::Home);
    QCOMPARE(manager.commandForKey(Qt::Key_F11), InputManager::None);
    QVERIFY(QDir().rmdir(path + ".bak")); QVERIFY(manager.saveBinding());
    QCOMPARE(manager.commandForKey(Qt::Key_F11), InputManager::Home);
}

void RemoteBindingTests::remoteUiCaptureReviewCancelRestore() {
    QTemporaryDir dir; const auto path = dir.filePath("bindings.json");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    InputManager manager(path); RemoteSettings remote;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inputManager", &manager);
    engine.rootContext()->setContextProperty("remoteSettings", &remote);
    QQmlComponent component(&engine, QUrl::fromLocalFile(REMOTE_TEST_QML));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280, 720);
    auto *item = qobject_cast<QQuickItem *>(object.data()); QVERIFY(item);
    item->setParentItem(window.contentItem()); item->setSize(QSizeF(1160, 660)); item->setPosition({60,30});
    window.show(); window.requestActivate(); item->forceActiveFocus();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *page = object->findChild<QObject *>("remotePage"); QVERIFY(page);
    auto capture = [&](const QString &name) {
        const auto folder = qEnvironmentVariable("ISOLDE_TEST_ARTIFACTS");
        return folder.isEmpty() || window.grabWindow().save(QDir(folder).filePath("studio-test-remote-" + name + ".png"));
    };
    QTest::qWait(50); QVERIFY(capture("actions"));
    QTest::keyClick(&window, Qt::Key_Return); QVERIFY(manager.listening());
    QTest::keyClick(&window, Qt::Key_Back); QVERIFY(!manager.bindingActive());
    QTest::keyClick(&window, Qt::Key_Return); QVERIFY(manager.listening());
    QTest::keyClick(&window, Qt::Key_F6); QVERIFY(!manager.listening()); QVERIFY(manager.bindingActive());
    QTest::qWait(50); QVERIFY(capture("review"));
    QVERIFY(!QFile::exists(path));
    QTest::keyClick(&window, Qt::Key_Escape); QVERIFY(!manager.bindingActive());
    QCOMPARE(page->property("selectedIndex").toInt(), 0);
    QVERIFY(!QFile::exists(path));
    QTest::keyClick(&window, Qt::Key_Return); QTest::keyClick(&window, Qt::Key_F6);
    QTest::keyClick(&window, Qt::Key_Return);
    QCOMPARE(manager.commandForKey(Qt::Key_F6), InputManager::NavigateUp);
    QTest::keyClick(&window, Qt::Key_Menu); QVERIFY(page->property("editing").toBool());
    QTest::keyClick(&window, Qt::Key_Up); QTest::keyClick(&window, Qt::Key_Return);
    QCOMPARE(manager.commandForKey(Qt::Key_F6), InputManager::None);
    // Every action remains reachable by the D-pad, including the reset review.
    for (int i = 0; i < 14; ++i) QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(page->property("selectedIndex").toInt(), 14);
    QTest::keyClick(&window, Qt::Key_Return); QVERIFY(page->property("editing").toBool());
    QTest::keyClick(&window, Qt::Key_Escape); QVERIFY(!page->property("editing").toBool());
    QVERIFY(capture("recovery"));
}

void RemoteBindingTests::physicalKeyboardTextAndControls() {
    QTemporaryDir dir; InputManager manager(dir.filePath("bindings.json"));
    QVERIFY(manager.beginBinding(InputManager::NavigateUp)); press(Qt::Key_A); QVERIFY(manager.saveBinding());
    QQmlEngine engine; engine.rootContext()->setContextProperty("inputManager", &manager);
    const auto path = QFileInfo(REMOTE_TEST_QML).dir().filePath("PhysicalKeyboardTest.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    QScopedPointer<QObject> object(component.create()); QVERIFY2(object, qPrintable(component.errorString()));
    QQuickWindow window; window.resize(1280,720);
    auto *item = qobject_cast<QQuickItem *>(object.data()); QVERIFY(item);
    item->setParentItem(window.contentItem()); item->setSize({1160,660}); item->setPosition({60,30});
    window.show(); window.requestActivate(); item->forceActiveFocus(); QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *keyboard = object->findChild<QObject *>("physicalKeyboard"); QVERIFY(keyboard);
    auto type = [&](int key, const QString &text, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QKeyEvent down(QEvent::KeyPress,key,modifiers,text); QCoreApplication::sendEvent(&window,&down);
        QKeyEvent up(QEvent::KeyRelease,key,modifiers,text); QCoreApplication::sendEvent(&window,&up);
    };
    type(Qt::Key_H,"h"); type(Qt::Key_M,"m"); type(Qt::Key_Plus,"+"); type(Qt::Key_Equal,"=");
    type(Qt::Key_Space," "); type(Qt::Key_A,"A",Qt::ShiftModifier); type(Qt::Key_Eacute,QString::fromUtf8("é"));
    type(Qt::Key_unknown,QString::fromUtf8("🙂"));
    QCOMPARE(keyboard->property("value").toString(),QString::fromUtf8("hm+= Aé🙂"));
    QCOMPARE(object->property("shellCommands").toInt(),0);
    QTest::keyClick(&window,Qt::Key_Backspace);
    QCOMPARE(keyboard->property("value").toString(),QString::fromUtf8("hm+= Aé"));
    keyboard->setProperty("maximumLength",keyboard->property("value").toString().size());
    type(Qt::Key_Z,"z"); QCOMPARE(keyboard->property("value").toString(),QString::fromUtf8("hm+= Aé"));
    QVERIFY(!keyboard->property("inputMessage").toString().isEmpty()); keyboard->setProperty("maximumLength",4096);
    // D-pad restores onscreen selection; Confirm selects a key rather than submitting.
    QTest::keyClick(&window,Qt::Key_Right); QTest::keyClick(&window,Qt::Key_Return);
    QCOMPARE(keyboard->property("value").toString(),QString::fromUtf8("hm+= Aé2"));
    QVERIFY(object->property("typing").toBool());
    auto keys = keyboard->property("keys").value<QJSValue>().toVariant().toList();
    if (keys.isEmpty()) keys = keyboard->property("keys").toList();
    QVERIFY(!keys.isEmpty()); QCOMPARE(keys.last().toString(),QString("Accept keys"));
    keyboard->setProperty("selectedIndex",keys.size()-1); QTest::keyClick(&window,Qt::Key_Return);
    QVERIFY(!manager.acceptTextKeys());
    const auto before = keyboard->property("value");
    type(Qt::Key_A,"a"); QCOMPARE(keyboard->property("value"),before);
    QCOMPARE(keyboard->property("selectedIndex").toInt(),keys.size()-1-10);
    keyboard->setProperty("selectedIndex",keys.size()-1); QTest::keyClick(&window,Qt::Key_Return);
    QVERIFY(manager.acceptTextKeys()); type(Qt::Key_H,"h");
    QTest::qWait(50);
    const auto folder = qEnvironmentVariable("ISOLDE_TEST_ARTIFACTS");
    QVERIFY(folder.isEmpty() || window.grabWindow().save(QDir(folder).filePath("studio-test-remote-keyboard.png")));
    const auto expected = keyboard->property("value").toString();
    // A held Enter cannot activate the underlying screen after accepting text.
    QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier,"\r"); QCoreApplication::sendEvent(&window,&enter);
    QVERIFY(!object->property("typing").toBool()); QCOMPARE(object->property("acceptedValue").toString(),expected);
    QKeyEvent repeat(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier,"\r",true); QCoreApplication::sendEvent(&window,&repeat);
    QKeyEvent release(QEvent::KeyRelease,Qt::Key_Return,Qt::NoModifier); QCoreApplication::sendEvent(&window,&release);
    QCOMPARE(object->property("shellCommands").toInt(),0);
    type(Qt::Key_H,"h"); QCOMPARE(object->property("shellCommands").toInt(),1);
    object->setProperty("typing",true); QTest::keyClick(&window,Qt::Key_Escape);
    QVERIFY(!object->property("typing").toBool()); QCOMPARE(object->property("acceptedValue").toString(),expected);
}

void RemoteBindingTests::textPreferenceCompatibilityAndRollback() {
    QTemporaryDir dir; const auto path = dir.filePath("bindings.json");
    const QByteArray legacy = R"({"schemaVersion":1,"bindings":{}})";
    QVERIFY(write(path,legacy));
    InputManager manager(path); QVERIFY(manager.acceptTextKeys()); QCOMPARE(read(path),legacy);
    QVERIFY(manager.setAcceptTextKeys(false));
    InputManager reopened(path), other(dir.filePath("other.json"));
    QVERIFY(!reopened.acceptTextKeys()); QVERIFY(other.acceptTextKeys());
    const auto before = read(path);
    QVERIFY(QFile::remove(path+".bak")); QVERIFY(QDir().mkdir(path+".bak"));
    QVERIFY(!manager.setAcceptTextKeys(true)); QVERIFY(!manager.acceptTextKeys()); QCOMPARE(read(path),before);
    QVERIFY(QDir().rmdir(path+".bak")); QVERIFY(manager.setAcceptTextKeys(true));
    const QByteArray malformed = R"({"schemaVersion":1,"bindings":{},"acceptTextKeys":"yes"})";
    QVERIFY(write(path,malformed)); InputManager invalid(path);
    QVERIFY(invalid.acceptTextKeys()); QVERIFY(!invalid.errorString().isEmpty()); QCOMPARE(read(path),malformed);
}

QTEST_MAIN(RemoteBindingTests)
#include "remote_bindings.moc"
