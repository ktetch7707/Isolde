import QtQuick
import QtQuick.Window

Window {
    id: window
    width: 1280
    height: 720
    visible: true
    visibility: Qt.application.arguments.indexOf("--windowed") >= 0 ? Window.Windowed : Window.FullScreen
    color: "#0B1020"
    title: "Isolde " + isoldeVersion
    readonly property real uiScaleFactor: displaySettings.uiScale / 100
    readonly property int safeAreaPixels: Math.round(Math.min(width, height) * displaySettings.safeArea / 100)
    property bool ambientActive: false
    property string launchErrorText: ""
    property string launchErrorAppId: ""
    Image {
        id: windowWallpaper; anchors.fill: parent; source: homeLayout.wallpaperSource
        fillMode: Image.PreserveAspectCrop; visible: !sessionManager.active && !pageSurface.open && !window.ambientActive && !!source.toString() && homeLayout.composition.enabled
    }

    function launchAppFromShell(appId, originId) {
        if (!setupState.complete) return
        commandCenter.open = false
        actionDispatcher.dispatch({ type: "launch-app", appId: appId }, originId || "")
    }

    function openSearchFromPage() {
        const onPage = pageSurface.open && pageSurface.model
        commandCenter.pageId = onPage ? pageSurface.activeId : ""
        commandCenter.pageSources = onPage ? pageSurface.model.pageSearchSources() : []
        commandCenter.open = true
    }

    function handleCommand(command) {
        idleTimer.restart()
        if (kodiMedia.playbackActive) {
            if (command === inputManager.home) kodiMedia.playbackControl("home")
            else if (command === inputManager.back) kodiMedia.playbackControl("stop")
            else if (command === inputManager.playPause || command === inputManager.confirm) kodiMedia.playbackControl("pause")
            else if (command === inputManager.navigateLeft) kodiMedia.playbackControl("backward")
            else if (command === inputManager.navigateRight) kodiMedia.playbackControl("forward")
            if (command !== inputManager.volumeUp && command !== inputManager.volumeDown && command !== inputManager.mute) return
        }
        if (command !== inputManager.none && remoteSettings.setupMode)
            remoteSettings.recordTestInput(inputManager.labelForCommand(command))
        if (ambientActive) {
            ambientActive = false
            return
        }
        if (command === inputManager.volumeUp) {
            audioSettings.adjustVolume(audioSettings.volumeStep)
            return
        }
        if (command === inputManager.volumeDown) {
            audioSettings.adjustVolume(-audioSettings.volumeStep)
            return
        }
        if (command === inputManager.mute) {
            audioSettings.toggleMute()
            return
        }
        if (captureSession.open) { captureSession.handleCommand(command); return }
        if (!setupState.complete) {
            setupWizard.handleCommand(command)
            return
        }
        if (command === inputManager.power) {
            remoteSettings.recordTestInput("Power")
            if (!remoteSettings.sendTvPowerCommand() && !sessionManager.active && !commandCenter.open)
                ambientActive = !ambientActive
            return
        }
        if (commandCenter.open) { commandCenter.handleCommand(command); return }
        if (kodiLibrary.open) { kodiLibrary.handleCommand(command); return }
        if (launchErrorOverlay.visible) {
            launchErrorOverlay.handleCommand(command)
        } else if (!sessionManager.active && pageSurface.open) {
            pageSurface.handleCommand(command)
        } else if (!sessionManager.active && (command === inputManager.menu || command === inputManager.search)) {
            window.openSearchFromPage()
        } else if (command === inputManager.back && sessionManager.active && sessionManager.currentAppType === "settings" && settings.inPage) {
            settings.handleCommand(command)
        } else if (command === inputManager.back) {
            if (sessionManager.active)
                sessionManager.exitCurrent()
        } else if (command === inputManager.home) {
            sessionManager.goHome()
        } else if (sessionManager.active && sessionManager.currentAppType === "store") {
            store.handleCommand(command)
        } else if (sessionManager.active && sessionManager.currentAppType === "settings") {
            settings.handleCommand(command)
        } else if (sessionManager.active && sessionManager.currentAppType === "web") {
            webKiosk.handleCommand(command)
        } else if (!sessionManager.active) {
            home.handleCommand(command)
        }
    }

    Shortcut { sequence: "Escape"; onActivated: window.handleCommand(inputManager.back) }
    Shortcut { sequence: "Ctrl+M"; onActivated: if (setupState.complete && !sessionManager.active) {
        window.launchAppFromShell("movies", "")
    } }
    Shortcut { sequence: "Home"; onActivated: window.handleCommand(inputManager.home) }
    Shortcut { sequence: "H"; onActivated: window.handleCommand(inputManager.home) }
    Shortcut { sequence: "Volume Up"; onActivated: window.handleCommand(inputManager.volumeUp) }
    Shortcut { sequence: "Volume Down"; onActivated: window.handleCommand(inputManager.volumeDown) }
    Shortcut { sequence: "Volume Mute"; onActivated: window.handleCommand(inputManager.mute) }
    Shortcut { sequence: "/"; onActivated: if (setupState.complete && !sessionManager.active) window.openSearchFromPage() }
    Shortcut { sequence: "S"; onActivated: if (setupState.complete && !sessionManager.active) window.openSearchFromPage() }
    Shortcut { sequence: "A"; onActivated: if (setupState.complete) ambientActive = true }
    Shortcut {
        sequence: "P"
        onActivated: if (setupState.complete && !sessionManager.active && !window.ambientActive) {
            sessionManager.launch("settings")
            settings.openSection(5)
        }
    }
    Shortcut { sequence: "K"; onActivated: if (setupState.complete && !sessionManager.active) home.keepFocusedSource() }
    Shortcut { sequence: "Delete"; onActivated: if (setupState.complete && !sessionManager.active) home.hideFocusedSource() }

    Timer {
        id: idleTimer
        running: setupState.complete && ambientSettings.enabled && !window.ambientActive
        repeat: false
        interval: ambientSettings.timeoutMinutes * 60000
        onTriggered: if (!sessionManager.active && !captureSession.open && !commandCenter.open && !kodiLibrary.open && !kodiMedia.playbackActive) window.ambientActive = true
    }

    Item {
        anchors.fill: parent
        anchors.margins: window.safeAreaPixels

        Item {
            id: shellSurface
            width: parent.width / window.uiScaleFactor
            height: parent.height / window.uiScaleFactor
            anchors.centerIn: parent
            scale: window.uiScaleFactor
            focus: true
            Keys.onPressed: event => {
                const command = inputManager.commandForKey(event.key)
                if (window.ambientActive || command !== inputManager.none) {
                    window.handleCommand(command)
                    event.accepted = true
                }
            }

            HomeScreen {
                id: home
                anchors.fill: parent
                visible: !sessionManager.active && !pageSurface.open
                items: homeLayout.components.concat(sourceManager.items).concat(kodiShelves.items)
                authoredItems: homeLayout.components
                sections: sourceManager.sections.concat(homeLayout.sections).concat(kodiShelves.sections)
                authoredSections: homeLayout.sections
                brandVisible: homeLayout.brandVisible
                backgroundStart: homeLayout.backgroundStart
                backgroundEnd: homeLayout.backgroundEnd
                tileRoundness: homeLayout.tileRoundness
                appearance: homeLayout.appearance
                composition: homeLayout.composition
                setItemPlacement: homeLayout.setItemPlacement
                gridCollisions: homeLayout.gridCollisions
                libraryProvider: pageData
                wallpaperSource: homeLayout.wallpaperSource
                wallpaperItem: windowWallpaper
                internetAvailable: networkSettings.internetAvailable
                localNetworkAvailable: networkSettings.localNetworkAvailable
                hasContextualSources: sourceManager.items.length > 0
                onActionActivated: (action, originId) => { if (setupState.complete) actionDispatcher.dispatch(action, originId) }
                onSettingsRequested: section => actionDispatcher.dispatch({ type: "settings", section: section }, "")
                onSearchRequested: window.openSearchFromPage()
                onKeepSourceRequested: source => {
                    if (source && source.action && homeLayout.addActionTile(source.name, source.action.type, source.action.target, "", source.action.openWith))
                        sourceManager.markPermanent(source.id)
                }
                onHideSourceRequested: sourceId => sourceManager.hideSource(sourceId)
            }

            Text {
                anchors.top: parent.top; anchors.right: parent.right
                anchors.margins: 16; width: 300; horizontalAlignment: Text.AlignRight
                visible: !sessionManager.active && !pageSurface.open && !window.ambientActive && (!homeLayout.composition.enabled || homeLayout.composition.showProfile === true)
                text: profileManager.activeName + "  ·  P Profiles"
                color: "#AAB5C9"; font.pixelSize: 17; elide: Text.ElideRight
                MouseArea {
                    anchors.fill: parent
                    onClicked: { sessionManager.launch("settings"); settings.openSection(5) }
                }
            }

            MockSessionView {
                anchors.fill: parent
                visible: sessionManager.active && sessionManager.currentAppType !== "store" && sessionManager.currentAppType !== "settings" && sessionManager.currentAppType !== "web"
                appName: sessionManager.currentAppName
                accent: sessionManager.currentAppAccent
                presentationMode: sessionManager.currentPresentationMode
            }

            WebKioskView {
                id: webKiosk
                anchors.fill: parent
                visible: sessionManager.active && sessionManager.currentAppType === "web"
                appName: sessionManager.currentAppName
                accent: sessionManager.currentAppAccent
                url: sessionManager.currentLaunchUrl
                allowedOrigins: sessionManager.currentAllowedOrigins
            }

            AppStoreView {
                id: store
                anchors.fill: parent
                visible: sessionManager.active && sessionManager.currentAppType === "store"
                entries: appCatalog.entries
            }

            SettingsView {
                id: settings
                anchors.fill: parent
                visible: sessionManager.active && sessionManager.currentAppType === "settings"
                homeItems: homeLayout.items
                availableApps: appRegistry.apps
                artwork: artPacks.artwork
                themes: artPacks.themes
                appearance: homeLayout.appearance
                onPreviewAmbientRequested: window.ambientActive = true
                onPageRequested:id=>{ sessionManager.goHome(); actionDispatcher.dispatch({type:"open-page",pageId:id},"") }
            }

            PageSurface {
                id:pageSurface;anchors.fill:parent;visible:open && !sessionManager.active
                layoutModel:homeLayout;provider:pageData;availableApps:appRegistry.apps;artwork:artPacks.artwork;themes:artPacks.themes
                internetAvailable:networkSettings.internetAvailable;localNetworkAvailable:networkSettings.localNetworkAvailable
                onClosed:origin=>home.restoreFocus(origin)
                onSearchRequested:window.openSearchFromPage()
                onSettingsRequested:section=>actionDispatcher.dispatch({type:"settings",section:section},"")
            }

            AmbientScreen {
                id: ambientScreen
                anchors.fill: parent
                active: window.ambientActive
                onDismissRequested: window.ambientActive = false
            }

            Rectangle {
                id: launchErrorOverlay
                anchors.fill: parent
                visible: window.launchErrorText.length > 0
                z: 70
                color: "#EE050812"

                function handleCommand(command) {
                    if (command === inputManager.back || command === inputManager.home) {
                        window.launchErrorText = ""
                        window.launchErrorAppId = ""
                    } else if (command === inputManager.confirm && window.launchErrorAppId.length) {
                        const retryId = window.launchErrorAppId
                        window.launchErrorText = ""
                        window.launchErrorAppId = ""
                        sessionManager.launch(retryId)
                    } else if (command === inputManager.navigateRight) {
                        window.launchErrorText = ""
                        window.launchErrorAppId = ""
                        sessionManager.launch("app-store")
                    } else if (command === inputManager.navigateLeft && window.launchErrorAppId.length && appCatalog.isInstalled(window.launchErrorAppId)) {
                        appCatalog.uninstall(window.launchErrorAppId)
                        window.launchErrorText = ""
                        window.launchErrorAppId = ""
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 120, 820)
                    height: 360
                    radius: 22
                    color: "#111A2D"
                    border.width: 3
                    border.color: "#FCA5A5"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 34
                        spacing: 18
                        Text {
                            text: window.launchErrorAppId.length ? "APP DIDN'T START" : "ACTION NEEDED"
                            color: "#FCA5A5"
                            font.pixelSize: 34
                            font.weight: Font.DemiBold
                            font.letterSpacing: 4
                        }
                        Text {
                            text: window.launchErrorText
                            color: "#F8FAFC"
                            font.pixelSize: 22
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                        Text {
                            text: window.launchErrorAppId.length ? "Isolde returned to Home instead of leaving you in the desktop. You can retry, open the App Store, or uninstall the package." : "The app was not started. Your installed package and configuration have not been changed."
                            color: "#AAB5C9"
                            font.pixelSize: 17
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                        Rectangle { width: parent.width; height: 1; color: "#2B3853" }
                        Text {
                            text: window.launchErrorAppId.length ? "Enter Retry    → App Store    ← Uninstall    Esc Close" : "Esc Close"
                            color: "#D5B083"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 1000
        color: "#080C18"
        visible: !setupState.complete
        Item {
            anchors.fill: parent
            anchors.margins: window.safeAreaPixels
            SetupWizard {
                id: setupWizard
                anchors.centerIn: parent
                width: parent.width / window.uiScaleFactor
                height: parent.height / window.uiScaleFactor
                scale: window.uiScaleFactor
            }
        }
    }

    KodiLibraryView { id: kodiLibrary; anchors.fill: parent; z: 900; appearance: homeLayout.appearance }
    CommandCenter {
        id: commandCenter
        x: window.safeAreaPixels
        y: window.safeAreaPixels
        width: Math.max(1, (parent.width - 2 * window.safeAreaPixels) / window.uiScaleFactor)
        height: Math.max(1, (parent.height - 2 * window.safeAreaPixels) / window.uiScaleFactor)
        scale: window.uiScaleFactor
        transformOrigin: Item.TopLeft
        z: 1100
        apps: appRegistry.apps
        packages: appCatalog.entries
        searchSettings: searchPreferences
        movieSearchClient: searchMovies
        showSearchClient: searchShows
        episodeSearchClient: searchEpisodes
        mediaCollections: kodiMedia.collections
        onCloseRequested: open = false
        onLaunchApp: appId => { kodiLibrary.open = false; window.launchAppFromShell(appId) }
        onInstallPackage: packageId => {
            kodiLibrary.open = false
            appCatalog.install(packageId)
            sessionManager.launch("app-store")
        }
        onSetupPackage: packageId => {
            kodiLibrary.open = false
            appCatalog.completeSetup(packageId)
            sessionManager.launch("app-store")
        }
        onOpenSettings: section => {
            kodiLibrary.open = false
            if (sessionManager.launch("settings")) settings.openSection(section)
        }
        onMediaRequested: (mediaType, mediaId, query, collectionId, pageStart) => {
            const libraryWasOpen = kodiLibrary.open
            kodiLibrary.open = true
            kodiLibrary.returnToHomeAfterPlayback = false
            kodiLibrary.returnToSearchAfterPlayback = !libraryWasOpen && (mediaType === "movie" || mediaType === "episode")
            if (mediaType === "collection") kodiMedia.openCollection(collectionId)
            else if (mediaType === "show") kodiMedia.openShowResult(query, mediaId, pageStart)
            else kodiMedia.playMedia(mediaType, mediaId, true)
        }
        onPageSourceRequested: (pageId, sourceId, kind, view, value) => {
            if (pageId && pageSurface.activeId !== pageId) pageSurface.showPage(pageId, "", true)
        }
    }
    CaptureSessionView {
        id: captureSession
        anchors.fill: parent
        onReturned: originId => { if (pageSurface.open) pageSurface.restoreOrigin(originId); else home.restoreFocus(originId) }
    }

    Connections {
        target: sessionManager
        function onSessionRejected(reason) {
            window.launchErrorAppId = ""
            window.launchErrorText = reason
        }
        function onLaunchDeferred(reason) {
            window.launchErrorAppId = ""
            window.launchErrorText = reason
        }
        function onSessionFailed(appId, reason) {
            if (appCatalog.isInstalled(appId))
                appCatalog.markFailed(appId, reason)
            window.launchErrorAppId = appId
            window.launchErrorText = reason
        }
    }
    Connections {
        target:kodiMedia
        function onPlaybackReturned(home){if(home && pageSurface.open && !kodiLibrary.returnToSearchAfterPlayback)pageSurface.closeAll()}
    }
    Connections {
        target: actionDispatcher
        function onDrawerRequested() { commandCenter.pageId = ""; commandCenter.pageSources = []; commandCenter.open = true }
        function onRestoreFocusRequested(originId) {
            if (kodiLibrary.returnToSearchAfterPlayback) return
            if (pageSurface.open) pageSurface.restoreOrigin(originId); else home.restoreFocus(originId)
        }
        function onPageRequested(id,originId) {commandCenter.open=false;pageSurface.showPage(id,originId,false)}
        function onRejected(reason) { window.launchErrorAppId = ""; window.launchErrorText = reason }
        function onLibraryRequested(returnHome) {
            commandCenter.open = false
            kodiLibrary.returnToHomeAfterPlayback = returnHome
            kodiLibrary.open = true
        }
        function onCaptureSessionRequested(input, originId) {
            commandCenter.open = false
            captureSession.openSession(input, originId)
        }
        function onSettingsRequested(section) { if (section < 0) settings.inPage = false; else settings.openSection(section) }
    }
    Connections {
        target: kodiLibrary
        function onSearchRequested() { commandCenter.pageId = ""; commandCenter.pageSources = []; commandCenter.open = true }
    }
}
