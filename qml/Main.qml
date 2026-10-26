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
    property bool launchErrorVpn: false
    property bool desktopHidden:false
    property int desktopRestoreVisibility:Window.FullScreen
    DesktopReturnWindow {controller:desktopMode;appearance:homeLayout.homeAppearance}
    Connections {
        target:desktopMode
        function onChanged(){
            if(desktopMode.active && !desktopMode.nativeMode && !window.desktopHidden){window.desktopRestoreVisibility=window.visibility;window.desktopHidden=true;window.visibility=Window.Hidden}
            else if(!desktopMode.active && window.desktopHidden){window.desktopHidden=false;window.visibility=window.desktopRestoreVisibility;window.raise();window.requestActivate()}
            if(desktopMode.active || desktopMode.busy)idleTimer.stop();else idleTimer.restart()
            if(desktopMode.errorString){window.launchErrorAppId="";window.launchErrorText=desktopMode.errorString}
        }
        function onReturned(origin){window.raise();window.requestActivate();if(!desktopMode.nativeMode)Qt.callLater(()=>{if(pageSurface.open)pageSurface.restoreOrigin(origin);else home.restoreFocus(origin)})}
    }
    PageBackdrop {
        id: windowWallpaper; anchors.fill: parent; source: homeLayout.wallpaperSource
        config:homeLayout.composition.background || {};startColor:homeLayout.backgroundStart;endColor:homeLayout.backgroundEnd; visible: (!sessionManager.active || (sessionManager.currentAppType === "desktop" || sessionManager.currentAppType === "android")) && !pageSurface.open && !window.ambientActive && !!source.toString() && (homeLayout.composition.enabled || !!homeLayout.composition.background)
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

    function showDesignatedHome() {
        guideSearchReturn=null
        if(tvGuide.open){guideController.cancel();tvGuide.open=false}
        sessionManager.goHome()
        if(homeLayout.homePageId)pageSurface.showPage(homeLayout.homePageId,"",true)
        else if(pageSurface.open)pageSurface.closeAll()
    }
    Component.onCompleted:Qt.callLater(()=>{if(setupState.complete && homeLayout.homePageId)window.showDesignatedHome()})

    function handleCommand(command) {
        if(appVpn.busy && appVpn.pendingApp){
            if(command===inputManager.back || command===inputManager.home)appVpn.cancel()
            return
        }
        if(desktopMode.busy || desktopMode.active && !desktopMode.nativeMode){if(command===inputManager.back || command===inputManager.home || desktopMode.active && command===inputManager.confirm)desktopMode.cancel();return}
        if(desktopMode.active && desktopMode.nativeMode && (command===inputManager.home || command===inputManager.back)){desktopMode.cancel();return}
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
        if(command===inputManager.home){window.showDesignatedHome();return}
        if(tvGuide.open && !sessionManager.active && !launchErrorOverlay.visible){tvGuide.handleCommand(command);return}
        if (kodiLibrary.open) { kodiLibrary.handleCommand(command); return }
        if (launchErrorOverlay.visible) {
            launchErrorOverlay.handleCommand(command)
        } else if (!sessionManager.active && pageSurface.open) {
            pageSurface.handleCommand(command)
        } else if (!sessionManager.active && command===inputManager.menu && (home.widgetActive || home.groupActive)) {
            home.handleCommand(command)
        } else if (!sessionManager.active && (command === inputManager.menu || command === inputManager.search)) {
            window.openSearchFromPage()
        } else if (command === inputManager.back && sessionManager.active && sessionManager.currentAppType === "settings" && settings.inPage) {
            settings.handleCommand(command)
        } else if (command === inputManager.back && sessionManager.active && sessionManager.currentAppType === "store") {
            if(!store.handleCommand(command))sessionManager.exitCurrent()
        } else if(command===inputManager.back && sessionManager.active && sessionManager.currentAppType==="web"){
            webKiosk.handleCommand(command)
        } else if (command === inputManager.back) {
            if (sessionManager.active)
                sessionManager.exitCurrent()
            else if(home.groupActive || home.widgetActive)
                home.handleCommand(command)
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
        Rectangle {
            anchors.fill:parent;z:1200;visible:desktopMode.busy;color:homeLayout.homeAppearance.surfaceColor
            Column {anchors.centerIn:parent;spacing:24
                Text {text:"Allow the desktop return shortcut in the system dialog.";color:homeLayout.homeAppearance.textColor;font.pixelSize:24}
                Text {text:"Isolde stays open until a return shortcut is granted.";color:homeLayout.homeAppearance.mutedColor;font.pixelSize:18}
                Rectangle {width:parent.width;height:56;radius:12;color:homeLayout.homeAppearance.selectedColor
                    Text {anchors.centerIn:parent;text:"Cancel · Back";color:homeLayout.homeAppearance.textColor;font.pixelSize:20}
                    MouseArea {anchors.fill:parent;onClicked:desktopMode.cancel()}
                }
            }
        }

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
                visible: (!sessionManager.active || (sessionManager.currentAppType === "desktop" || sessionManager.currentAppType === "android")) && !pageSurface.open
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
                grid: homeLayout.grid
                placements: homeLayout.placements
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
                visible: sessionManager.active && sessionManager.currentAppType !== "desktop" && sessionManager.currentAppType !== "android" && sessionManager.currentAppType !== "store" && sessionManager.currentAppType !== "settings" && sessionManager.currentAppType !== "web"
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
                appearance:homeLayout.homeAppearance
                onExitRequested:sessionManager.exitCurrent()
            }

            AppStoreView {
                id: store
                anchors.fill: parent
                visible: sessionManager.active && sessionManager.currentAppType === "store"
                entries: appCatalog.entries
                appearance: homeLayout.homeAppearance
                onOperationSettled: { if(visible){window.raise();window.requestActivate();shellSurface.forceActiveFocus()} }
                onOpenRequested: appId => { sessionManager.exitCurrent(); Qt.callLater(()=>sessionManager.launch(appId)) }
            }

            SettingsView {
                id: settings
                anchors.fill: parent
                visible: sessionManager.active && sessionManager.currentAppType === "settings"
                homeItems: homeLayout.items
                availableApps: appRegistry.apps
                artwork: artPacks.artwork
                themes: artPacks.themes
                appearance: homeLayout.homeAppearance
                onPreviewAmbientRequested: window.ambientActive = true
                onPageRequested:id=>{ sessionManager.goHome(); actionDispatcher.dispatch({type:"open-page",pageId:id},"") }
            }

            PageSurface {
                id:pageSurface;anchors.fill:parent;visible:open && (!sessionManager.active || (sessionManager.currentAppType === "desktop" || sessionManager.currentAppType === "android"))
                layoutModel:homeLayout;provider:pageData;availableApps:appRegistry.apps;artwork:artPacks.artwork;themes:artPacks.themes
                internetAvailable:networkSettings.internetAvailable;localNetworkAvailable:networkSettings.localNetworkAvailable
                onClosed:origin=>{if(!homeLayout.rootPageVisible)Qt.callLater(()=>window.showDesignatedHome());else home.restoreFocus(origin)}
                onSearchRequested:window.openSearchFromPage()
                onSettingsRequested:section=>actionDispatcher.dispatch({type:"settings",section:section},"")
            }

            AmbientOverlay {
                id: ambientScreen
                scaleFactor: window.uiScaleFactor
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
                        sessionManager.launch(window.launchErrorVpn ? "settings" : "app-store")
                        if(window.launchErrorVpn)settings.openSection(6)
                    } else if (!window.launchErrorVpn && command === inputManager.navigateLeft && window.launchErrorAppId.length && appCatalog.isInstalled(window.launchErrorAppId)) {
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
                            text: window.launchErrorVpn ? "Enter Retry    → VPN settings    Esc Close" : window.launchErrorAppId.length ? "Enter Retry    → App Store    ← Uninstall    Esc Close" : "Esc Close"
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

    KodiLibraryView { id: kodiLibrary; anchors.fill: parent; z: 900; provider:pageData;appearance: homeLayout.homeAppearance }
    property var pendingGuideSearchReturn:null
    property var guideSearchReturn:null
    GuideView {
        id:tvGuide;x:window.safeAreaPixels;y:window.safeAreaPixels
        width:Math.max(1,(parent.width-2*window.safeAreaPixels)/window.uiScaleFactor)
        height:Math.max(1,(parent.height-2*window.safeAreaPixels)/window.uiScaleFactor)
        scale:window.uiScaleFactor;transformOrigin:Item.TopLeft;z:950
        controller:guideController;dispatcher:actionDispatcher;options:homeLayout.studioOptions.guide;appearance:homeLayout.homeAppearance
        suppressed:sessionManager.active || kodiMedia.playbackActive
        onClosed:origin=>{
            const state=window.guideSearchReturn;window.guideSearchReturn=null
            if(state){commandCenter.open=true;Qt.callLater(()=>{commandCenter.selectedIndex=state.index;commandCenter.focusArea=state.area;commandCenter.lastHeader=state.header})}
            else if(pageSurface.open)pageSurface.restoreOrigin(origin);else home.restoreFocus(origin)
        }
    }
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
        appearance:homeLayout.homeAppearance
        movieSearchClient: searchMovies
        showSearchClient: searchShows
        episodeSearchClient: searchEpisodes
        mediaCollections: kodiMedia.collections
        shellActions:homeLayout.studioOptions.actions || []
        onShellActionRequested:action=>{
            window.pendingGuideSearchReturn={index:commandCenter.selectedIndex,area:commandCenter.focusArea,header:commandCenter.lastHeader}
            actionDispatcher.dispatch(action,"")
            window.pendingGuideSearchReturn=null
        }
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
        target: appVpn
        function onFailed(appId,message){window.launchErrorVpn=true;window.launchErrorAppId=appId;window.launchErrorText=message}
    }
    Rectangle {
        anchors.fill:parent;z:1200
        visible:appVpn.busy && !!appVpn.pendingApp
        color:homeLayout.homeAppearance.surfaceColor
        Column {
            anchors.centerIn:parent;width:parent.width*0.7;spacing:24
            Text {width:parent.width;wrapMode:Text.WordWrap;text:"Connecting to your app’s VPN location…";color:homeLayout.homeAppearance.textColor;font.pixelSize:28}
            Text {width:parent.width;wrapMode:Text.WordWrap;text:"Checking the country before opening the app. Back cancels and restores the previous connection.";color:homeLayout.homeAppearance.textColor;font.pixelSize:20}
        }
    }
    Connections {
        target: sessionManager
        function onReturnedHome(appId) {
            if(store.returnAppId===appId){store.returnAppId="";Qt.callLater(()=>sessionManager.launch("app-store"))}
        }
        function onSessionRejected(reason) {
            window.launchErrorVpn=false
            window.launchErrorAppId = ""
            window.launchErrorText = reason
        }
        function onLaunchDeferred(reason) {
            window.launchErrorVpn=false
            window.launchErrorAppId = ""
            window.launchErrorText = reason
        }
        function onSessionFailed(appId, reason) {
            window.launchErrorVpn=false
            if (appCatalog.isInstalled(appId))
                appCatalog.markFailed(appId, reason)
            window.launchErrorAppId = appId
            window.launchErrorText = reason
        }
    }
    Connections {
        target:kodiMedia
        function onPlaybackReturned(home){
            if(tvGuide.open){if(home){tvGuide.open=false;window.showDesignatedHome()}else guideController.refresh()}
            if(home && pageSurface.open && !kodiLibrary.returnToSearchAfterPlayback)pageSurface.closeAll()
        }
    }
    Connections {
        target: actionDispatcher
        function onDrawerRequested() { commandCenter.pageId = ""; commandCenter.pageSources = []; commandCenter.open = true }
        function onGuideRequested(origin,presentation){window.guideSearchReturn=window.pendingGuideSearchReturn;commandCenter.open=false;kodiLibrary.open=false;tvGuide.show(origin,presentation)}
        function onRestoreFocusRequested(originId) {
            if(tvGuide.open)return
            if (kodiLibrary.returnToSearchAfterPlayback) return
            if (pageSurface.open) pageSurface.restoreOrigin(originId); else home.restoreFocus(originId)
        }
        function onPageRequested(id,originId) {commandCenter.open=false;if(id)pageSurface.showPage(id,originId,false);else{sessionManager.goHome();pageSurface.closeAll();home.restoreFocus(originId)}}
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
