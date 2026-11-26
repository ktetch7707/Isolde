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
    bool webDisabled = false;
    QString latestVersion = "0.0.1.3";
    QString status = "Candidate update";
    QString currentVersion = "0.0.1.2";
    QVariantList savedVersionsList;
    int checks = 0;
    int dismissals = 0, installs = 0;
    int saves = 0, drops = 0, restores = 0;
    QString lastSave, lastDrop, lastRestore;
    // Record remote requests without contacting a release server or changing the installation.
    Q_INVOKABLE void check() { if (webDisabled) emit webEnableRequested(); else ++checks; }
    // Model starting the installation so the view can disable duplicate activation.
    Q_INVOKABLE void install() { ++installs; installing = true; emit changed(); }
    // Clear the candidate notification as the production controller does.
    Q_INVOKABLE void dismiss() { ++dismissals; available = false; emit changed(); }
    Q_INVOKABLE QVariantList savedVersions() { return savedVersionsList; }
    Q_INVOKABLE QString currentInstalledVersion() { return currentVersion; }
    Q_INVOKABLE void saveVersion(const QString& version)
    {
        ++saves; lastSave = version;
        bool already = false;
        for (const auto& v : savedVersionsList) {
            if (v.toMap()["version"] == version) { already = true; break; }
        }
        if (!already) {
            QVariantMap entry; entry["version"] = version; entry["current"] = version == currentVersion;
            savedVersionsList.append(entry);
        }
        emit changed();
    }
    Q_INVOKABLE void dropVersion(const QString& version)
    {
        ++drops; lastDrop = version;
        for (int i = 0; i < savedVersionsList.size(); ++i) {
            if (savedVersionsList[i].toMap()["version"] == version) {
                savedVersionsList.removeAt(i);
                break;
            }
        }
        emit changed();
    }
    Q_INVOKABLE void restoreVersion(const QString& version)
    {
        ++restores; lastRestore = version;
        installing = true; emit changed();
    }
  signals:
    void changed();
    void installFinished(bool success, const QString& message);
    void webEnableRequested();
};

class FakeSearchSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool webEnabled MEMBER m_webEnabled NOTIFY changed)
  public:
    bool m_webEnabled = false;
    bool webEnabled() const { return m_webEnabled; }
    Q_INVOKABLE void setWebEnabled(bool enabled)
    {
        if (m_webEnabled == enabled) return;
        m_webEnabled = enabled;
        emit changed();
    }
  signals:
    void changed();
};

class FakeInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int back MEMBER back CONSTANT)
    Q_PROPERTY(int navigateUp MEMBER up CONSTANT)
    Q_PROPERTY(int navigateDown MEMBER down CONSTANT)
    Q_PROPERTY(int navigateRight MEMBER navigateRight CONSTANT)
    Q_PROPERTY(int confirm MEMBER confirm CONSTANT)
  public:
    int back = 0, up = 1, down = 2, navigateRight = 4, confirm = 3;
};

class SoftwareUpdatesQmlTests : public QObject
{
    Q_OBJECT
  private slots:
    void badgesFollowSettingsActions();
    void remoteActionsCheckAndDismiss();
    void webOffCheckOffersEnableWeb();
    void saveCurrentVersion();
    void restoreAndDropSavedVersion();
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

void SoftwareUpdatesQmlTests::webOffCheckOffersEnableWeb()
{
    QQmlEngine engine;
    FakeUpdates updates;
    updates.webDisabled = true;
    FakeSearchSettings search;
    FakeInput input;
    engine.rootContext()->setContextProperty("softwareUpdates", &updates);
    engine.rootContext()->setContextProperty("searchPreferences", &search);
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("isoldeVersion", "0.0.1.2");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(UPDATES_QML_DIR "/SoftwareUpdatesView.qml")));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    QCOMPARE(page->property("confirmation").toString(), QString());
    // Check while web disabled offers to enable web instead of checking directly.
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(page->property("confirmation").toString(), QStringLiteral("enableWeb"));
    QCOMPARE(search.webEnabled(), false);
    QCOMPARE(updates.checks, 0);
    // Back dismisses the offer without enabling web or checking.
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.back)));
    QCOMPARE(page->property("confirmation").toString(), QString());
    QCOMPARE(search.webEnabled(), false);
    QCOMPARE(updates.checks, 0);
    // Re-offer, then confirm enable: web turns on and the check runs.
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(page->property("confirmation").toString(), QStringLiteral("enableWeb"));
    updates.webDisabled = false;
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.navigateRight)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(page->property("confirmation").toString(), QString());
    QCOMPARE(search.webEnabled(), true);
    QCOMPARE(updates.checks, 1);
}

void SoftwareUpdatesQmlTests::saveCurrentVersion()
{
    QQmlEngine engine;
    FakeUpdates updates;
    updates.currentVersion = "0.0.1.1";
    updates.available = true;
    FakeSearchSettings search;
    FakeInput input;
    engine.rootContext()->setContextProperty("softwareUpdates", &updates);
    engine.rootContext()->setContextProperty("searchPreferences", &search);
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("isoldeVersion", "0.0.1.1");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(UPDATES_QML_DIR "/SoftwareUpdatesView.qml")));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    QCOMPARE(updates.saves, 0);
    // Index 0: check, 1: install, 2: dismiss, 3: save current version
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.saves, 1);
    QCOMPARE(updates.lastSave, QStringLiteral("0.0.1.1"));
}

void SoftwareUpdatesQmlTests::restoreAndDropSavedVersion()
{
    QQmlEngine engine;
    FakeUpdates updates;
    updates.currentVersion = "0.0.1.1";
    updates.available = true;
    QVariantMap entry;
    entry["version"] = QStringLiteral("0.0.1.2");
    entry["current"] = false;
    updates.savedVersionsList.append(entry);
    FakeSearchSettings search;
    FakeInput input;
    engine.rootContext()->setContextProperty("softwareUpdates", &updates);
    engine.rootContext()->setContextProperty("searchPreferences", &search);
    engine.rootContext()->setContextProperty("inputManager", &input);
    engine.rootContext()->setContextProperty("isoldeVersion", "0.0.1.1");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(UPDATES_QML_DIR "/SoftwareUpdatesView.qml")));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    QCOMPARE(updates.restores, 0);
    QCOMPARE(updates.drops, 0);
    // Index 0: check, 1: install, 2: dismiss, 3: save current version,
    // 4: restore-0.0.1.2, 5: drop-0.0.1.2
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.restores, 1);
    QCOMPARE(updates.lastRestore, QStringLiteral("0.0.1.2"));
    QVERIFY(updates.installing);
    updates.installing = false;
    emit updates.changed();
    // selectedIndex is 4; down goes to 5 (drop-0.0.1.2)
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.down)));
    QVERIFY(QMetaObject::invokeMethod(page.get(), "handleCommand", Q_ARG(QVariant, input.confirm)));
    QCOMPARE(updates.drops, 1);
    QCOMPARE(updates.lastDrop, QStringLiteral("0.0.1.2"));
}

QTEST_MAIN(SoftwareUpdatesQmlTests)
#include "tst_software_updates_qml.moc"
