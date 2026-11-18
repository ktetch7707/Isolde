# Lizard Function-Level Violations (CCN > 15), src/ only

Fresh run: python -m lizard -C 15 -L 1000 src/
Distinguishes per-function CCN (real violations) from file-level aggregates.

Total function-level violations: 123

| Rank | CCN | NCSS | Function |
|------|-----|------|----------|
| 1 | 147 | 299 | HomeLayout::studioOptions(...) at src/home\HomeLayout_studio.cpp:14 |
| 2 | 124 | 270 | HomeLayout::loadDocument(...) at src/home\HomeLayout.cpp:1134 |
| 3 | 92 | 184 | GameEmulators::definitions(...) at src/games\GameEmulators.cpp:308 |
| 4 | 82 | 140 | validRule(...) at src/media\GuideRules.cpp:64 |
| 5 | 77 | 207 | GuideFeeds::parse(...) at src/media\GuideFeeds.h:77 |
| 6 | 60 | 165 | appIcon(...) at src/app\AppRegistry.cpp:48 |
| 7 | 57 | 236 | StudioSession::componentPrototype(...) at src/home\StudioSession_components.cpp:144 |
| 8 | 56 | 148 | InputManager::eventFilter(...) at src/input\InputManager.cpp:282 |
| 9 | 54 | 185 | main(...) at src/sessions\NativeAppHost.cpp:161 |
| 10 | 50 | 100 | GuideRules::matches(...) at src/media\GuideRules.cpp:401 |
| 11 | 49 | 112 | PageModel::setAppearance(...) at src/home\PageModel.cpp:460 |
| 12 | 48 | 124 | CaptureController::applyConfiguration(...) at src/media\CaptureController.cpp:666 |
| 13 | 47 | 164 | AudioSettings::completeCommand(...) at src/settings\AudioSettings.cpp:637 |
| 14 | 47 | 102 | InterfaceSounds::readKodiTheme(...) at src/settings\InterfaceSounds.cpp:248 |
| 15 | 46 | 118 | StudioSession::replacementProposal(...) at src/home\StudioSession_components.cpp:382 |
| 16 | 41 | 103 | StudioSession::previewImport(...) at src/home\StudioDesigns.cpp:668 |
| 17 | 39 | 193 | KodiService::start(...) at src/media\KodiService.cpp:593 |
| 18 | 38 | 84 | GuidePlaylists::decode(...) at src/media\GuidePlaylists.h:80 |
| 19 | 37 | 181 | DesktopSession::launch(...) at src/sessions\DesktopSession.cpp:377 |
| 20 | 36 | 130 | ArtPackCatalog::loadFromData(...) at src/theme\ArtPackCatalog.cpp:69 |
| 21 | 36 | 83 | StudioSession::healthFindings(...) at src/home\StudioSafety.cpp:140 |
| 22 | 36 | 73 | ComponentSchema::guideOptions(...) at src/media\GuideRules.cpp:208 |
| 23 | 36 | 58 | InterfaceSounds::validOptions(...) at src/settings\InterfaceSounds.cpp:136 |
| 24 | 35 | 128 | HomeLayout::applySkinDocument(...) at src/home\HomeSkins.cpp:167 |
| 25 | 35 | 43 | SessionManager::launchGameFile(...) at src/sessions\SessionManager.cpp:511 |
| 26 | 33 | 80 | StudioSession::stageArtwork(...) at src/home\StudioSession_artwork.cpp:198 |
| 27 | 32 | 138 | ActionDispatcher::ActionDispatcher(...) at src/home\ActionDispatcher.cpp:19 |
| 28 | 32 | 86 | loadSteamRecoveryStatuses(...) at src/sessions\SessionManager.cpp:65 |
| 29 | 32 | 71 | OpticalDiscParse::parseUdfVolumeDescriptors(...) at src/games\OpticalDiscIdentity.cpp:255 |
| 30 | 32 | 65 | PageModel::resolveItems(...) at src/home\PageModel.cpp:172 |
| 31 | 32 | 57 | PageData::client(...) at src/home\PageData.cpp:46 |
| 32 | 31 | 124 | InterfaceSounds::prepare(...) at src/settings\InterfaceSounds.cpp:612 |
| 33 | 31 | 84 | DesignSystem::validDocument(...) at src/home\DesignSystem.cpp:113 |
| 34 | 30 | 106 | SessionManager::launchDefinition(...) at src/sessions\SessionManager.cpp:358 |
| 35 | 30 | 88 | StudioSession::exportDesign(...) at src/home\StudioDesigns.cpp:772 |
| 36 | 30 | 86 | portableLayout(...) at src/home\StudioDesigns.cpp:219 |
| 37 | 30 | 80 | StudioSession::readVersions(...) at src/home\StudioSafety.cpp:287 |
| 38 | 30 | 74 | GuideAtomicFile::publish(...) at src/media\GuideAtomicFile.h:17 |
| 39 | 29 | 106 | OpticalDiscHash::mountedDiscs(...) at src/games\OpticalDiscIdentity.cpp:511 |
| 40 | 29 | 88 | SourceManager::loadState(...) at src/sources\SourceManager.cpp:620 |
| 41 | 29 | 73 | importSetupAsset(...) at src/games\GameEmulators.cpp:233 |
| 42 | 29 | 73 | HomeLayout::addActionTile(...) at src/home\HomeLayout.cpp:1454 |
| 43 | 29 | 53 | DesignSystem::valid(...) at src/home\DesignSystem.cpp:58 |
| 44 | 28 | 90 | ActionDispatcher::dispatchEditorAction(...) at src/home\ActionDispatcher.cpp:379 |
| 45 | 28 | 56 | KodiMedia::collections(...) at src/media\KodiMedia_collections.cpp:186 |
| 46 | 28 | 51 | AppDefinition::isValid(...) at src/app\AppDefinition.h:30 |
| 47 | 27 | 121 | StudioSession::additionProposal(...) at src/home\StudioSession_components.cpp:553 |
| 48 | 27 | 56 | validTokens(...) at src/home\SurfaceAppearance.cpp:54 |
| 49 | 27 | 50 | InterfaceSounds::validWave(...) at src/settings\InterfaceSounds.cpp:196 |
| 50 | 26 | 100 | resolveDesktopLaunch(...) at src/sessions\DesktopSession.cpp:267 |

## Notes
- File-level aggregates (e.g. ComponentSchema.cpp CCN=1275) are sums of all functions and are NOT individual violations; ComponentSchema was already refactored in T15.
- ComponentSchema.cpp appears only as file aggregate; its highest single function is guideOptions at CCN=36.