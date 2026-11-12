#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
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
    QCOMPARE(manager.commandForKey(Qt::Key_Up), InputManager::NavigateUp);
    InputManager reopened(path), other(dir.filePath("two.json"));
    QCOMPARE(reopened.commandForKey(Qt::Key_F6), InputManager::NavigateUp);
    QCOMPARE(other.commandForKey(Qt::Key_F6), InputManager::None);
    const auto old = read(path);
    QVERIFY(manager.beginBinding(InputManager::NavigateDown)); press(Qt::Key_F7);
    QVERIFY(manager.saveBinding());
    QCOMPARE(read(path + ".bak"), old);
    QVERIFY(manager.clearBinding(InputManager::NavigateUp));
    QCOMPARE(manager.commandForKey(Qt::Key_F6), InputManager::None);
    QCOMPARE(manager.commandForKey(Qt::Key_F7), InputManager::NavigateDown);
    QVERIFY(manager.resetBindings());
    QCOMPARE(manager.commandForKey(Qt::Key_F7), InputManager::None);
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

QTEST_MAIN(RemoteBindingTests)
#include "remote_bindings.moc"
