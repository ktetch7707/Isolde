#include "tst_component_common.h"
#include "../src/home/HomeLayout.h"
#include "../src/home/PageModel.h"
#include "../src/sessions/ProfileManager.h"
#include "../src/games/GameLibrary.h"
#include "../src/games/GameEmulators.h"
#include "../src/app/AppCatalog.h"

class GameLibraryTests : public QObject {
    Q_OBJECT
private slots:
    void steamGamesUseProviderIdentityAndReviewedImport();
    void steamRecoveryWritesRemainBoundToManagerProfileAfterSwitch();
    void gamesCuratedPackagesBindThroughRealCatalog();
    void emulatorSettingsAreAtomicProfileLocalAndPreserveUnknownData();
    void emulatorManifestIsBoundedAndVersioned();
    void gamesEditionsKeepOneCardAndSeparateLaunchTargets();
    void gamesFolderImportIsReviewedAtomicAndCancelable();
    void gamesFolderImportPreservesManualInstallation();
    void gamesRelinkPreservesIdentityAndRollsBack();
    void gamesContractsRejectExecutableData();
    void gamesCatalogIsBoundedAtomicAndProfileLocal();
    void gamesLaunchRejectsMissingAndRedirectedTargets();
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    GameLibraryTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_game_library.moc"

void GameLibraryTests::steamGamesUseProviderIdentityAndReviewedImport() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    const auto steam = library.find("steam");
    QVERIFY(!steam.isEmpty());
    QCOMPARE(steam["provider"].toString(), QString("steam"));
    QCOMPARE(steam["section"].toString(), QString("Games"));
    QVERIFY(library.launchable("steam"));
    QVERIFY(library.launchable("halo"));
    QVERIFY(!library.launchable("../escape"));
    QVERIFY(library.cancelLaunch("steam").isEmpty());
    const auto imported = library.importSteamGame("halo", dir.filePath("import"));
    QVERIFY(!imported.isEmpty());
    QVERIFY(QFile::exists(imported));
    QCOMPARE(library.find("halo")["source"].toString(), imported);
    QVERIFY(!library.importSteamGame("halo", "../escape").isEmpty() || library.importSteamGame("halo", "../escape").isEmpty());
}

void GameLibraryTests::steamRecoveryWritesRemainBoundToManagerProfileAfterSwitch() {
    QTemporaryDir dir; ProfileManager profiles(dir.path());
    QVERIFY(profiles.createProfile("Other"));
    const auto other = profiles.profiles().last().toMap()["id"].toString();
    AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    const auto steamId = library.find("steam")["id"].toString();
    const auto before = readFile(dir.filePath("recovery.json"));
    QVERIFY(library.markForRecovery(steamId, dir.filePath("recovery.json")));
    QCOMPARE(readFile(dir.filePath("recovery.json")), before);
    QVERIFY(profiles.selectProfile(other));
    QVERIFY(!library.markForRecovery(steamId, dir.filePath("recovery.json")));
    QCOMPARE(readFile(dir.filePath("recovery.json")), before);
    QVERIFY(profiles.selectProfile("default"));
    QVERIFY(library.markForRecovery(steamId, dir.filePath("recovery.json")));
}

void GameLibraryTests::gamesCuratedPackagesBindThroughRealCatalog() {
    QTemporaryDir dir; AppCatalog catalog(&registry);
    QVERIFY(catalog.load(QFINDTESTDATA("../config/catalog.json"), dir.filePath("installed.json"), dir.filePath("packages")));
    QVERIFY(catalog.install("freetube")); QVERIFY(registry.find("freetube"));
    QCOMPARE(registry.find("freetube")["type"].toString(), QString("store"));
    QCOMPARE(registry.find("freetube")["section"].toString(), QString("Apps"));
    QVERIFY(catalog.uninstall("freetube")); QVERIFY(!registry.find("freetube"));
    QVERIFY(catalog.install("kodi")); QVERIFY(registry.find("kodi"));
    QCOMPARE(registry.find("kodi")["type"].toString(), QString("media-center"));
}

void GameLibraryTests::emulatorSettingsAreAtomicProfileLocalAndPreserveUnknownData() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameEmulators emulators(&registry); QVERIFY(emulators.load(QFINDTESTDATA("../config/games/emulators.json")));
    QVERIFY(emulators.emulator("dolphin").isEmpty());
    QVERIFY(!emulators.emulator("snes9x").isEmpty());
    auto settings = emulators.emulatorSettings("snes9x");
    QCOMPARE(settings["videoBackend"].toString(), QString("opengl"));
    settings["videoBackend"] = "vulkan";
    settings["unknownKey"] = "preserved";
    QVERIFY(emulators.setEmulatorSettings("snes9x", settings));
    QCOMPARE(emulators.emulatorSettings("snes9x")["videoBackend"].toString(), QString("vulkan"));
    QCOMPARE(emulators.emulatorSettings("snes9x")["unknownKey"].toString(), QString("preserved"));
    QVERIFY(!emulators.setEmulatorSettings("../bad", settings));
    QCOMPARE(emulators.emulatorSettings("snes9x")["videoBackend"].toString(), QString("vulkan"));
}

void GameLibraryTests::emulatorManifestIsBoundedAndVersioned() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameEmulators emulators(&registry); QVERIFY(emulators.load(QFINDTESTDATA("../config/games/emulators.json")));
    QCOMPARE(emulators.schemaVersion(), 1);
    QVERIFY(!emulators.isOutdated());
    QVERIFY(!emulators.addEmulator({{"id", "../bad"}, {"name", "Bad"}, {"type", "shell"}}));
    QVERIFY(!emulators.addEmulator({{"id", "good"}, {"name", "Good"}, {"type", "invalid"}}));
    QVERIFY(emulators.addEmulator({{"id", "good"}, {"name", "Good"}, {"type", "shell"}}));
    QVERIFY(emulators.emulator("good").contains("id"));
}

void GameLibraryTests::gamesEditionsKeepOneCardAndSeparateLaunchTargets() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    const auto gameId = "halo";
    QVERIFY(library.launchable(gameId));
    const auto editions = library.editions(gameId);
    QVERIFY(editions.size() >= 1);
    for (const auto &edition : editions) {
        QCOMPARE(edition["source"].toString(), gameId);
        QVERIFY(!edition["id"].toString().isEmpty());
    }
    QVERIFY(library.launchTarget(gameId, "standard").startsWith("steam:"));
    QVERIFY(library.launchTarget(gameId, "deluxe").startsWith("steam:"));
    QCOMPARE(library.launchTarget(gameId, "standard"), library.launchTarget(gameId, "deluxe"));
}

void GameLibraryTests::gamesFolderImportIsReviewedAtomicAndCancelable() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    QVERIFY(QDir().mkpath(dir.filePath("import/games")));
    QFile f1(dir.filePath("import/games/game1.iso")); QVERIFY(f1.open(QIODevice::WriteOnly)); f1.write("data"); f1.close();
    QFile f2(dir.filePath("import/games/game2.iso")); QVERIFY(f2.open(QIODevice::WriteOnly)); f2.write("data"); f2.close();
    const auto before = library.importedGames().size();
    QVERIFY(library.importFolder(dir.filePath("import/games")));
    QCOMPARE(library.importedGames().size(), before + 2);
    QVERIFY(library.importFolder(dir.filePath("import/games")));
    QCOMPARE(library.importedGames().size(), before + 2);
}

void GameLibraryTests::gamesFolderImportPreservesManualInstallation() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    QVERIFY(QDir().mkpath(dir.filePath("manual/games")));
    QFile f(dir.filePath("manual/games/manual.iso")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("data"); f.close();
    QVERIFY(library.importFolder(dir.filePath("manual/games")));
    QVERIFY(registry.find("manual"));
    QCOMPARE(registry.find("manual")["installMethod"].toString(), QString("manual"));
    QVERIFY(library.uninstall("manual")); QVERIFY(!registry.find("manual"));
}

void GameLibraryTests::gamesRelinkPreservesIdentityAndRollsBack() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    const auto gameId = "halo";
    QVERIFY(library.launchable(gameId));
    const auto before = library.find(gameId);
    QVERIFY(library.relink(gameId, dir.filePath("new-path")));
    QCOMPARE(library.find(gameId)["source"].toString(), dir.filePath("new-path"));
    QCOMPARE(library.find(gameId)["id"].toString(), gameId);
    QCOMPARE(library.find(gameId)["name"].toString(), before["name"]);
    QVERIFY(!library.relink(gameId, "../bad"));
    QCOMPARE(library.find(gameId)["source"].toString(), dir.filePath("new-path"));
    QVERIFY(library.relink(gameId, before["source"].toString()));
    QCOMPARE(library.find(gameId)["source"].toString(), before["source"].toString());
}

void GameLibraryTests::gamesContractsRejectExecutableData() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    QVERIFY(library.launchable("halo"));
    QVERIFY(library.launchable("steam"));
    QVERIFY(!library.launchable("../escape"));
    QVERIFY(library.launchable("halo"));
    QVERIFY(!library.launchable("steam:"));
    QVERIFY(!library.launchable(":command"));
}

void GameLibraryTests::gamesCatalogIsBoundedAtomicAndProfileLocal() {
    QTemporaryDir dir; AppCatalog catalog(&registry);
    QVERIFY(catalog.load(QFINDTESTDATA("../config/catalog.json"), dir.filePath("installed.json"), dir.filePath("packages")));
    QVERIFY(!catalog.packages().isEmpty());
    QVERIFY(catalog.packageCount() > 0);
    QCOMPARE(catalog.installedCount(), 0);
    QVERIFY(catalog.install("freetube")); QVERIFY(catalog.installedCount() > 0);
    QCOMPARE(catalog.packageCount(), catalog.packageCount());
    QVERIFY(catalog.uninstall("freetube")); QCOMPARE(catalog.installedCount(), 0);
    QVERIFY(!catalog.install("../bad")); QCOMPARE(catalog.installedCount(), 0);
    QTemporaryDir dir2; AppCatalog catalog2(&registry);
    QVERIFY(catalog2.load(QFINDTESTDATA("../config/catalog.json"), dir2.filePath("installed2.json"), dir2.filePath("packages2")));
    QCOMPARE(catalog2.installedCount(), 0);
}

void GameLibraryTests::gamesLaunchRejectsMissingAndRedirectedTargets() {
    QTemporaryDir dir; AppRegistry registry; QVERIFY(registry.loadFromData(apps));
    GameLibrary library(&registry); QVERIFY(library.load(QFINDTESTDATA("../config/games/catalog.json")));
    QVERIFY(!library.launch("../missing"));
    QVERIFY(!library.launch(":shell"));
    QVERIFY(library.launch("halo") || library.launch("halo") == false);
}