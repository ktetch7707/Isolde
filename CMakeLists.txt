cmake_minimum_required(VERSION 3.21)

project(Isolde VERSION 0.0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick Test WebEngineQuick WebChannel Multimedia SerialPort)
qt_standard_project_setup(REQUIRES 6.4)
qt_policy(SET QTP0001 NEW)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

add_library(isolde_core STATIC
    src/media/KodiArtwork.cpp
    src/media/KodiArtwork.h
    src/home/ComponentSchema.cpp
    src/home/ComponentSchema.h
    src/home/ActionDispatcher.cpp
    src/home/ActionDispatcher.h
    src/media/MediaGrants.cpp
    src/media/MediaGrants.h
    src/settings/ComponentHealth.cpp
    src/settings/ComponentHealth.h
    src/media/KodiMedia.cpp
    src/media/KodiMedia.h
    src/media/CaptureController.cpp
    src/media/CaptureController.h
    src/media/KodiShelves.cpp
    src/media/KodiShelves.h
    src/media/KodiService.cpp
    src/media/KodiService.h
    src/media/KodiImporter.cpp
    src/media/KodiImporter.h
    src/settings/SetupState.cpp
    src/settings/SetupState.h
    src/settings/ProfileManager.cpp
    src/settings/ProfileManager.h
    src/app/AppDefinition.h
    src/app/AppCatalog.cpp
    src/app/AppCatalog.h
    src/app/AppRegistry.cpp
    src/app/AppRegistry.h
    src/home/HomeLayout.cpp
    src/home/StudioSession.cpp
    src/home/StudioSession.h
    src/home/HomeSkins.cpp
    src/home/HomePages.cpp
    src/home/PageModel.cpp
    src/home/PageModel.h
    src/home/PageData.cpp
    src/home/PageData.h
    src/home/HomeLayout.h
    src/input/InputManager.cpp
    src/input/InputManager.h
    src/settings/NetworkSettings.cpp
    src/settings/NetworkSettings.h
    src/settings/DisplaySettings.cpp
    src/settings/DisplaySettings.h
    src/settings/AudioSettings.cpp
    src/settings/AudioSettings.h
    src/settings/RemoteSettings.cpp
    src/settings/RemoteSettings.h
    src/settings/AmbientSettings.cpp
    src/settings/AmbientSettings.h
    src/settings/SearchSettings.cpp
    src/settings/SearchSettings.h
    src/sources/SourceManager.cpp
    src/sources/SourceManager.h
    src/theme/ArtPackCatalog.cpp
    src/theme/ArtPackCatalog.h
    src/sessions/AppSession.cpp
    src/sessions/AppSession.h
    src/sessions/MockSession.cpp
    src/sessions/MockSession.h
    src/sessions/DesktopSession.cpp
    src/sessions/DesktopSession.h
    src/sessions/NativeSandbox.cpp
    src/sessions/NativeSandbox.h
    src/sessions/SessionManager.cpp
    src/sessions/SessionManager.h
)
target_include_directories(isolde_core PUBLIC src)
target_link_libraries(isolde_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Multimedia Qt6::SerialPort)

qt_add_executable(isolde src/main.cpp)
qt_add_executable(isolde-recovery src/recovery/main.cpp)
target_link_libraries(isolde-recovery PRIVATE Qt6::Qml Qt6::Quick)
qt_add_resources(isolde-recovery "recovery_ui" PREFIX "/recovery" BASE src/recovery FILES src/recovery/Recovery.qml)
qt_add_executable(isolde-native-host src/sessions/NativeAppHost.cpp)
target_link_libraries(isolde-native-host PRIVATE Qt6::Core)
add_dependencies(isolde isolde-native-host)
target_link_libraries(isolde PRIVATE isolde_core Qt6::Qml Qt6::Quick Qt6::WebEngineQuick Qt6::WebChannel)
target_compile_definitions(isolde PRIVATE ISOLDE_VERSION="${PROJECT_VERSION}")

qt_add_qml_module(isolde
    URI Isolde
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        qml/Main.qml
        qml/ComponentSettingsView.qml
        qml/CaptureSettingsView.qml
        qml/CaptureSessionView.qml
        qml/KodiLibraryView.qml
        qml/LibraryBrowseControls.qml
        qml/KodiSettingsView.qml
        qml/KodiImportView.qml
        qml/MediaFoldersView.qml
        qml/SetupWizard.qml
        qml/HomeScreen.qml
        qml/AppTile.qml
        qml/MockSessionView.qml
        qml/OnScreenKeyboard.qml
        qml/AppStoreView.qml
        qml/AmbientScreen.qml
        qml/AmbientSettingsView.qml
        qml/AudioSettingsView.qml
        qml/ClockWidget.qml
        qml/CommandCenter.qml
        qml/CustomizeHomeView.qml
        qml/AppearanceEditor.qml
        qml/ComposedHome.qml
        qml/HomeCard.qml
        qml/LineIcon.qml
        qml/ComponentLayoutEditor.qml
        qml/SkinLibrary.qml
        qml/HomeDesignEditor.qml
        qml/PageManager.qml
        qml/PageSurface.qml
        qml/PageEditor.qml
        qml/PageStudioView.qml
        qml/PlacedComponents.qml
        qml/PageLibraryWidget.qml
        qml/ArtworkImage.qml
        qml/FrostedPanel.qml
        qml/OrbCard.qml
        qml/DisplaySettingsView.qml
        qml/NetworkSettingsView.qml
        qml/RemoteSettingsView.qml
        qml/SettingsView.qml
        qml/ProfileSettingsView.qml
        qml/IsoldeWebProfile.qml
        qml/SourceSettingsView.qml
        qml/SystemSettingsView.qml
        qml/WebKioskView.qml
)

qt_add_resources(isolde "isolde_config"
    PREFIX "/"
    FILES
        config/apps.json
        config/art-packs.json
        config/catalog.json
        config/default-home-layout.json
        config/studio-defaults.json
        config/skins/lounge.json
        config/skins/pastel.json
        config/packages/ambient-weather/manifest.json
        config/packages/living-room-radio/manifest.json
        config/packages/photo-gallery/manifest.json
        config/packages/plex/manifest.json
        config/packages/home-assistant/manifest.json
        config/packages/kodi/manifest.json
        config/packages/retro-arcade/manifest.json
        config/packages/spotify/manifest.json
)

qt_add_resources(isolde "isolde_store_web"
    PREFIX "/store"
    FILES store/index.html
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/config")
configure_file(config/native-session-bus.conf "${CMAKE_CURRENT_BINARY_DIR}/config/native-session-bus.conf" COPYONLY)
configure_file(config/apps.json "${CMAKE_CURRENT_BINARY_DIR}/config/apps.json" COPYONLY)
configure_file(config/art-packs.json "${CMAKE_CURRENT_BINARY_DIR}/config/art-packs.json" COPYONLY)
configure_file(config/catalog.json "${CMAKE_CURRENT_BINARY_DIR}/config/catalog.json" COPYONLY)
configure_file(config/default-home-layout.json "${CMAKE_CURRENT_BINARY_DIR}/config/default-home-layout.json" COPYONLY)
configure_file(config/studio-defaults.json "${CMAKE_CURRENT_BINARY_DIR}/config/studio-defaults.json" COPYONLY)
file(COPY config/skins DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/config")
set(ISOLDE_PACKAGE_MANIFESTS
    config/packages/ambient-weather/manifest.json
    config/packages/living-room-radio/manifest.json
    config/packages/photo-gallery/manifest.json
    config/packages/plex/manifest.json
    config/packages/home-assistant/manifest.json
    config/packages/kodi/manifest.json
    config/packages/retro-arcade/manifest.json
    config/packages/spotify/manifest.json
)
foreach(package_manifest IN LISTS ISOLDE_PACKAGE_MANIFESTS)
    get_filename_component(package_dir "${package_manifest}" DIRECTORY)
    configure_file("${package_manifest}" "${CMAKE_CURRENT_BINARY_DIR}/${package_dir}/manifest.json" COPYONLY)
endforeach()

install(TARGETS isolde RUNTIME DESTINATION bin)
install(FILES tools/component-health.py DESTINATION share/isolde)
install(FILES tools/kodi-window.py DESTINATION share/isolde)
configure_file(tools/kodi-window.py ${CMAKE_CURRENT_BINARY_DIR}/share/isolde/kodi-window.py COPYONLY)
configure_file(tools/component-health.py ${CMAKE_CURRENT_BINARY_DIR}/share/isolde/component-health.py COPYONLY)
install(TARGETS isolde-recovery RUNTIME DESTINATION bin)
install(TARGETS isolde-native-host RUNTIME DESTINATION bin)
install(FILES config/apps.json DESTINATION bin/config)
install(FILES config/native-session-bus.conf DESTINATION bin/config)
install(FILES config/art-packs.json DESTINATION bin/config)
install(FILES config/catalog.json DESTINATION bin/config)
install(FILES config/default-home-layout.json DESTINATION bin/config)
install(FILES config/studio-defaults.json DESTINATION bin/config)
install(DIRECTORY config/packages DESTINATION bin/config)
install(DIRECTORY config/skins DESTINATION bin/config)
install(DIRECTORY kodi/service.isolde.bridge DESTINATION share/isolde/kodi PATTERN "__pycache__" EXCLUDE)
foreach(bridge_file addon.xml bridge.py service.py playback.py artwork.py)
    configure_file(kodi/service.isolde.bridge/${bridge_file} ${CMAKE_CURRENT_BINARY_DIR}/share/isolde/kodi/service.isolde.bridge/${bridge_file} COPYONLY)
endforeach()

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(isolde_tests
        tests/tst_isolde.cpp
    )
    target_link_libraries(isolde_tests PRIVATE isolde_core Qt6::Test)
    add_test(NAME isolde_tests COMMAND isolde_tests)
    qt_add_executable(isolde_capture_tests tests/tst_capture.cpp)
    target_link_libraries(isolde_capture_tests PRIVATE isolde_core Qt6::Test)
    add_test(NAME capture_protocol COMMAND isolde_capture_tests)
    qt_add_executable(isolde_component_tests tests/tst_components.cpp)
    target_link_libraries(isolde_component_tests PRIVATE isolde_core Qt6::Test)
    add_test(NAME component_contracts COMMAND isolde_component_tests)
    qt_add_executable(isolde_home_navigation_tests tests/tst_home_navigation.cpp)
    target_link_libraries(isolde_home_navigation_tests PRIVATE isolde_core Qt6::Test Qt6::Qml Qt6::Quick)
    target_compile_definitions(isolde_home_navigation_tests PRIVATE HOME_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/HomeScreen.qml")
    target_compile_definitions(isolde_home_navigation_tests PRIVATE CUSTOMIZE_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/CustomizeHomeView.qml")
    target_compile_definitions(isolde_home_navigation_tests PRIVATE REFERENCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/visual/home-reference")
    target_compile_definitions(isolde_home_navigation_tests PRIVATE LIBRARY_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/KodiLibraryView.qml")
    target_compile_definitions(isolde_home_navigation_tests PRIVATE COMMAND_CENTER_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/CommandCenter.qml")
    target_compile_definitions(isolde_home_navigation_tests PRIVATE KODI_SETTINGS_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/KodiSettingsView.qml")
    add_test(NAME home_component_navigation COMMAND isolde_home_navigation_tests)
    set_tests_properties(home_component_navigation PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        qt_add_executable(isolde_audio_tests tests/tst_audio.cpp)
        target_link_libraries(isolde_audio_tests PRIVATE isolde_core Qt6::Test)
        target_compile_definitions(isolde_audio_tests PRIVATE AUDIO_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fake-mixer.py")
        target_compile_definitions(isolde_audio_tests PRIVATE AUDIO_PLAYER_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fake-audio-player.py")
        add_test(NAME audio_backend COMMAND isolde_audio_tests)
        add_test(NAME appliance_installation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_appliance_install.py)
        add_test(NAME appliance_recovery COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_recovery.py)
        add_test(NAME kodi_local_bridge COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kodi_bridge.py)
        add_test(NAME kodi_playback COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kodi_playback.py)
        add_test(NAME kodi_cached_artwork COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kodi_artwork.py)
        add_test(NAME component_health COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_component_health.py)
        qt_add_executable(isolde_kodi_media_tests tests/tst_kodi_media.cpp)
        qt_add_executable(isolde_kodi_startup_smoke tools/kodi-startup-smoke.cpp)
        qt_add_executable(isolde_kodi_frontend_smoke tools/kodi-frontend-smoke.cpp)
        qt_add_executable(isolde_kodi_library_smoke tools/kodi-library-smoke.cpp)
        target_link_libraries(isolde_kodi_library_smoke PRIVATE isolde_core)
        add_dependencies(isolde_kodi_library_smoke isolde-native-host)
        target_link_libraries(isolde_kodi_frontend_smoke PRIVATE isolde_core)
        add_dependencies(isolde_kodi_frontend_smoke isolde-native-host)
        target_link_libraries(isolde_kodi_startup_smoke PRIVATE isolde_core)
        add_dependencies(isolde_kodi_startup_smoke isolde-native-host)
        target_link_libraries(isolde_kodi_media_tests PRIVATE isolde_core Qt6::Test)
        add_test(NAME kodi_media_client COMMAND isolde_kodi_media_tests)
        qt_add_executable(isolde_native_smoke tools/native-session-smoke.cpp)
        target_link_libraries(isolde_native_smoke PRIVATE isolde_core)
        add_dependencies(isolde_native_smoke isolde-native-host)
        qt_add_executable(isolde_native_sandbox_tests tests/tst_native_sandbox.cpp)
        target_link_libraries(isolde_native_sandbox_tests PRIVATE isolde_core Qt6::Test)
        add_dependencies(isolde_native_sandbox_tests isolde-native-host)
        add_test(NAME native_profile_isolation COMMAND isolde_native_sandbox_tests)
        set_tests_properties(native_profile_isolation PROPERTIES TIMEOUT 90)
    endif()
    qt_add_executable(isolde_profile_browser_tests tests/profile_browser.cpp)
    target_link_libraries(isolde_profile_browser_tests PRIVATE Qt6::Qml Qt6::Quick Qt6::WebEngineQuick)
    target_compile_definitions(isolde_profile_browser_tests PRIVATE PROFILE_TEST_QML="${CMAKE_CURRENT_SOURCE_DIR}/tests/ProfileBrowserTest.qml")
    add_test(NAME profile_browser_isolation COMMAND isolde_profile_browser_tests)
    set_tests_properties(profile_browser_isolation PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
