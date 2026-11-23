#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

class FakeUpdates : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available MEMBER available NOTIFY changed)
    Q_PROPERTY(bool checking MEMBER checking NOTIFY changed)
    Q_PROPERTY(bool installing MEMBER installing NOTIFY changed)
    Q_PROPERTY(QString latestVersion MEMBER latestVersion NOTIFY changed)
    Q_PROPERTY(QString status MEMBER status NOTIFY changed)
  public:
    bool available = true;
    bool checking = false;
    bool installing = false;
    QString latestVersion = "0.0.1.3";
    QString status = "Candidate update";
    int checks = 0;
    int dismissals = 0, installs = 0;
    // Record remote requests without contacting a release server or changing the installation.
    Q_INVOKABLE void check() { ++checks; }
    // Model starting the installation so the view can disable duplicate activation.
    Q_INVOKABLE void install() { ++installs; installing = true; emit changed(); }
    // Clear the candidate notification as the production controller does.
    Q_INVOKABLE void dismiss() { ++dismissals; available = false; emit changed(); }
  signals:
    void changed();
    void installFinished(bool success, const QString& message);
};

class FakeInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int navigateUp MEMBER up CONSTANT)
    Q_PROPERTY(int navigateDown MEMBER down CONSTANT)
    Q_PROPERTY(int confirm MEMBER confirm CONSTANT)
  public:
    int up = 1, down = 2, confirm = 3;
};

class SoftwareUpdatesQmlTests : public QObject
{
    Q_OBJECT
  private slots:
    void badgesFollowSettingsActions();
    void remoteActionsCheckAndDismiss();
};

void SoftwareUpdatesQmlTests::badgesFollowSettingsActions()
{
    QQmlEngine engine;
    FakeUpdates updates;
    engine.rootContext()->setContextProperty("softwareUpdates", &updates);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(UPDATES_QML_DIR "/SettingsUpdateBadge.qml")));
    std::unique_ptr<QObject> badge(component.create());
    QVERIFY2(badge, qPrintable(component.errorString()));
    QQuickWindow window;
    qobject_cast<QQuickItem*>(badge.get())->setParentItem(window.contentItem());
    window.show();
    QVERIFY(!badge->property("visible").toBool());
    QVERIFY(badge->setProperty("item", QVariantMap{{"actions", QVariantList{QVariantMap{{"type", "settings"}}}}}));
    QVERIFY(badge->property("targetsSettings").toBool());
    QTRY_VERIFY(badge->property("visible").toBool());
    badge->setProperty("item", QVariantMap{{"action", QVariantMap{{"type", "launch-app"}, {"appId", "settings"}}}});
    QVERIFY(badge->property("visible").toBool());
    badge->setProperty("item", QVariantMap{{"action", QVariantMap{{"type", "launch-app"}, {"appId", "other"}}}});
    QVERIFY(!badge->property("visible").toBool());
    badge->setProperty("settingsTarget", true);
    QVERIFY(badge->property("visible").toBool());
    updates.dismiss();
    QVERIFY(!badge->property("visible").toBool());
}

void SoftwareUpdatesQmlTests::remoteActionsCheckAndDismiss()
{
    QQmlEngine engine;
    FakeUpdates updates;
    FakeInput input;
    engine.rootContext()->setContextProperty("softwareUpdates", &updates);
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("isoldeVersion", "0.0.1.2");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(UPDATES_QML_DIR "/SoftwareUpdatesView.qml")));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.checks, 1);
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.installs, 1);
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.installs, 1);
    updates.installing = false;
    emit updates.changed();
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.dismissals, 1);
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.dismissals, 1);
}

QTEST_MAIN(SoftwareUpdatesQmlTests)
#include "tst_software_updates_qml.moc"
