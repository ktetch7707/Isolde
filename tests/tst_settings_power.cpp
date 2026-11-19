#include <QDBusContext>
#include <QDBusConnection>
#include <QProcess>
#include <QScopeGuard>
#include <QSettings>
#include <QtTest>
#include <algorithm>
#include "../src/home/ComponentSchema.h"
#include "../src/home/HomeLayout.h"
#include "../src/settings/PowerSettings.h"
#include "../src/settings/ProfileManager.h"
#include "../src/settings/SearchSettings.h"
#include "tst_component_common.h"

class PowerBoundaryFixture final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.Manager")
  public:
    int accepted = 0;
    bool interactive = false, deny = false;
  public slots:
    QString
    CanSuspend() const
    {
        return "no";
    }
    QString
    CanReboot() const
    {
        return "challenge";
    }
    QString
    CanPowerOff() const
    {
        return "yes";
    }
    void
    PowerOff(bool value)
    {
        interactive = value;
        ++accepted;
    }
    void
    Reboot(bool)
    {
        if (deny)
            sendErrorReply("org.freedesktop.DBus.Error.AccessDenied", "Fixture denial");
        else
            ++accepted;
    }
    void
    Suspend(bool)
    {
        ++accepted;
    }
};

class SettingsPowerTests : public QObject
{
    Q_OBJECT
  private slots:
    void
    powerCapabilitiesGuardAndActions();
    void
    settingsNavigationIsBoundedAndProfileLocal();
};

int
main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    SettingsPowerTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_settings_power.moc"

void SettingsPowerTests::powerCapabilitiesGuardAndActions()
{
#ifdef Q_OS_LINUX
    QProcess daemon;
    daemon.start("dbus-daemon",{"--session","--nofork","--nopidfile","--print-address=1"});
    QVERIFY(daemon.waitForStarted());QVERIFY(daemon.waitForReadyRead());
    const auto address=QString::fromUtf8(daemon.readLine()).trimmed();QVERIFY(!address.isEmpty());
    const auto connection=QString("isolde-power-fixture-%1").arg(QCoreApplication::applicationPid());
    auto bus=QDBusConnection::connectToBus(address,connection);QVERIFY(bus.isConnected());
    auto cleanup=qScopeGuard([&]{bus.unregisterService("org.isolde.PowerFixture");QDBusConnection::disconnectFromBus(connection);daemon.terminate();if(!daemon.waitForFinished(2000)){daemon.kill();daemon.waitForFinished();}});
    PowerBoundaryFixture fixture;
    QVERIFY(bus.registerService("org.isolde.PowerFixture"));
    QVERIFY(bus.registerObject("/org/freedesktop/login1",&fixture,QDBusConnection::ExportAllSlots));
    PowerSettings power(nullptr,bus,"org.isolde.PowerFixture");bool allowed=false;power.setGuard([&]{return allowed;});
    QVERIFY(!power.request("poweroff"));QCOMPARE(fixture.accepted,0);
    power.refresh();QTRY_VERIFY(!power.busy());
    QCOMPARE(power.capabilities().value("poweroff").toString(),QString("yes"));
    QCOMPARE(power.capabilities().value("reboot").toString(),QString("challenge"));
    QVERIFY(!power.request("poweroff"));QVERIFY(!power.request("shell"));QCOMPARE(fixture.accepted,0);
    allowed=true;QVERIFY(!power.request("suspend"));QVERIFY(power.request("poweroff"));QVERIFY(!power.request("poweroff"));
    QTRY_VERIFY(!power.busy());QCOMPARE(fixture.accepted,1);QVERIFY(fixture.interactive);
    fixture.deny=true;QVERIFY(power.request("reboot"));QTRY_VERIFY(!power.busy());
    QCOMPARE(fixture.accepted,1);QVERIFY(power.status().contains("denied"));
    bus.unregisterService("org.isolde.PowerFixture");power.refresh();QTRY_VERIFY(!power.busy());
    QCOMPARE(power.capabilities().value("poweroff").toString(),QString("unavailable"));
    QVERIFY(!power.request("poweroff"));
#else
    QSKIP("login1 power support targets Linux");
#endif
}

void SettingsPowerTests::settingsNavigationIsBoundedAndProfileLocal()
{
    const auto options = QJsonDocument::fromJson(readFile(QFINDTESTDATA("../config/studio-defaults.json"))).object();
    const auto navigation = options["settingsNavigation"].toObject().toVariantMap();
    QVERIFY(ComponentSchema::validSettingsNavigation(navigation));
    for (const auto &version : QVariantList{0, 2, true, "1", 1.5}) {
        auto invalid = navigation; invalid["schemaVersion"] = version;
        QVERIFY(!ComponentSchema::validSettingsNavigation(invalid));
    }
    auto categories = navigation["categories"].toList();
    QVariantList order;
    for (const auto &category : categories) order.append(category.toMap()["id"]);
    auto invalid = navigation;
    auto duplicate = categories; duplicate[1] = duplicate[0]; invalid["categories"] = duplicate;
    QVERIFY(!ComponentSchema::validSettingsNavigation(invalid));
    auto category = categories[0].toMap(); auto rows = category["rows"].toList();
    auto row = rows[0].toMap(); row["route"] = "run-command"; rows[0] = row; category["rows"] = rows;
    auto unsafe = categories; unsafe[0] = category; invalid["categories"] = unsafe;
    QVERIFY(!ComponentSchema::validSettingsNavigation(invalid));
    auto composition = QVariantMap{{"schemaVersion", 1}, {"settingsCategoryOrder", order}};
    QVERIFY(ComponentSchema::validComposition(composition));
    auto duplicateOrder = order; duplicateOrder[1] = duplicateOrder[0];
    QVERIFY(!ComponentSchema::validComposition({{"schemaVersion", 1}, {"settingsCategoryOrder", duplicateOrder}}));
    auto shortOrder = order; shortOrder.removeLast();
    QVERIFY(!ComponentSchema::validComposition({{"schemaVersion", 1}, {"settingsCategoryOrder", shortOrder}}));
    auto unknownOrder = order; unknownOrder[0] = "unknown";
    QVERIFY(!ComponentSchema::validComposition({{"schemaVersion", 1}, {"settingsCategoryOrder", unknownOrder}}));

    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    const auto defaults = QFINDTESTDATA("../config/default-home-layout.json");
    const auto user = dir.filePath("a.json"), otherUser = dir.filePath("b.json");
    HomeLayout home(&registry), other(&registry);
    QVERIFY(home.load(defaults, user)); QVERIFY(other.load(defaults, otherUser));
    QVERIFY(!QFile::exists(user)); QVERIFY(!QFile::exists(otherUser));
    composition = home.composition(); composition["settingsCategoryOrder"] = order;
    QVERIFY2(home.setComposition(composition), qPrintable(home.errorString()));
    const auto original = readFile(user);
    HomeLayout reopened(&registry); QVERIFY(reopened.load(defaults, user));
    QCOMPARE(reopened.composition()["settingsCategoryOrder"].toList(), order);
    QCOMPARE(readFile(user), original); QVERIFY(!QFile::exists(otherUser));
    QVERIFY(!other.composition().contains("settingsCategoryOrder"));
    const auto before = home.composition();
    QVERIFY(QFile::rename(user, user + ".held")); QVERIFY(QDir().mkdir(user));
    std::reverse(order.begin(), order.end()); composition["settingsCategoryOrder"] = order;
    QVERIFY(!home.setComposition(composition)); QCOMPARE(home.composition(), before);
    QCOMPARE(readFile(user + ".held"), original);
    QVERIFY(QDir().rmdir(user)); QVERIFY(QFile::rename(user + ".held", user));
}
