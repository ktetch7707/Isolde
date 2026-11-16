#include "tst_component_common.h"
#include "../src/home/HomeLayout.h"
#include "../src/settings/SearchSettings.h"
#include "../src/settings/PowerSettings.h"
#include "../src/settings/ProfileManager.h"
#include <QSettings>
#include <QDBusContext>

class PowerBoundaryFixture final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.Manager")
public:
    int accepted = 0;
    bool interactive = false, deny = false;
public slots:
    QString CanSuspend() const { return "no"; }
    QString CanReboot() const { return "challenge"; }
    QString CanPowerOff() const { return "yes"; }
    void PowerOff(bool value) { interactive = value; ++accepted; }
    void Reboot(bool) { if (deny) sendErrorReply("org.freedesktop.DBus.Error.AccessDenied", "Fixture denial"); else ++accepted; }
    void Suspend(bool) { ++accepted; }
};

class SettingsPowerTests : public QObject {
    Q_OBJECT
private slots:
    void powerCapabilitiesGuardAndActions();
    void settingsNavigationIsBoundedAndProfileLocal();
};

QTEST_GUILESS_MAIN(SettingsPowerTests)
#include "tst_settings_power.moc"

void SettingsPowerTests::powerCapabilitiesGuardAndActions() {
    QTemporaryDir dir;
    PowerSettings power(dir.path());
    QVERIFY(power.capabilities().value("suspend").toBool());
    QVERIFY(!power.capabilities().value("reboot").toBool());
    QVERIFY(power.capabilities().value("poweroff").toBool());
    QCOMPARE(power.pendingActions().size(), 0);

    PowerBoundaryFixture fixture;
    QCoreApplication::processEvents();
    QVERIFY(power.requestAction("suspend"));
    QCOMPARE(power.pendingActions().size(), 1);
    QCOMPARE(fixture.accepted, 1);
    QCOMPARE(fixture.interactive, false);

    fixture.deny = true;
    QVERIFY(!power.requestAction("reboot"));
    QCOMPARE(fixture.accepted, 1);
    QVERIFY(fixture.deny);
    fixture.deny = false;

    QVERIFY(power.requestAction("poweroff"));
    QCOMPARE(fixture.accepted, 2);
    QCOMPARE(fixture.interactive, true);

    QVERIFY(power.requestAction("suspend"));
    QCOMPARE(fixture.accepted, 3);

    QVERIFY(!power.requestAction("suspend"));
    QCOMPARE(power.pendingActions().size(), 0);

    QVERIFY(power.requestAction("reboot"));
    QCOMPARE(fixture.accepted, 4);
}

void SettingsPowerTests::settingsNavigationIsBoundedAndProfileLocal() {
    QTemporaryDir dir; ProfileManager profiles(dir.path());
    QVERIFY(profiles.createProfile("Other"));
    const auto other = profiles.profiles().last().toMap()["id"].toString();
    QVERIFY(profiles.selectProfile(other));
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    HomeLayout home(&registry); QVERIFY(home.load(QFINDTESTDATA("../config/default-home-layout.json"), profiles.layoutPath()));
    const auto profileNavigation = home.settingsNavigation();
    QVERIFY(profileNavigation.contains("network"));
    QCOMPARE(profileNavigation.contains("system"), true);
    QCOMPARE(profileNavigation.contains("media"), true);
    QCOMPARE(profileNavigation.contains("store"), true);
    QCOMPARE(profileNavigation.contains("display-skins"), true);
    QVERIFY(profileNavigation.contains("skins"));
    QVERIFY(!profileNavigation.contains("edit"));
    QCOMPARE(profileNavigation.value("network").toString(), QString("network"));
    QCOMPARE(profileNavigation.value("vpn").toString(), QString("vpn"));
    QCOMPARE(profileNavigation.value("display").toString(), QString("display"));
    QCOMPARE(profileNavigation.value("audio").toString(), QString("audio"));
    QCOMPARE(profileNavigation.value("interface-sounds").toString(), QString("interface-sounds"));
    QCOMPARE(profileNavigation.value("studio").toString(), QString("studio"));
    QCOMPARE(profileNavigation.value("ambient").toString(), QString("ambient"));
    QCOMPARE(profileNavigation.value("store").toString(), QString("store"));
    QCOMPARE(profileNavigation.value("store-preferences").toString(), QString("store-preferences"));
    QCOMPARE(profileNavigation.value("capture").toString(), QString("capture"));
    QCOMPARE(profileNavigation.value("sources").toString(), QString("sources"));
    QCOMPARE(profileNavigation.value("remotes").toString(), QString("remotes"));
    QCOMPARE(profileNavigation.value("profiles").toString(), QString("profiles"));
    QCOMPARE(profileNavigation.value("media").toString(), QString("media"));
    QCOMPARE(profileNavigation.value("system").toString(), QString("system"));
    QCOMPARE(profileNavigation.value("skins").toString(), QString("skins"));
    QCOMPARE(profileNavigation.value("settings").toString(), QString("settings"));
    QCOMPARE(profileNavigation.value("guide").toString(), QString("guide"));
    QVERIFY(home.settingsNavigation() == profileNavigation);
    QVERIFY(profiles.selectProfile("default"));
    const auto defaultNavigation = home.settingsNavigation();
    QCOMPARE(defaultNavigation.size(), profileNavigation.size());
    QVERIFY(defaultNavigation.contains("skins"));
}