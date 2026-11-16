import QtQuick

// Read-only rendering of a profile page through the same renderer as Home.
// The ambient overlay owns input; authored actions cannot execute here.
Item {
    id: root
    property var pageModel: null
    property bool reducedMotion: false
    Loader {
        anchors.fill: parent
        active: !!root.pageModel
        sourceComponent: Component { HomeScreen {
        anchors.fill: parent
        visible: !!root.pageModel
        enabled: false
        items: root.pageModel ? root.pageModel.items : []
        sections: root.pageModel ? root.pageModel.sections : []
        brandVisible: root.pageModel ? root.pageModel.brandVisible : false
        backgroundStart: root.pageModel ? root.pageModel.backgroundStart : "#000000"
        backgroundEnd: root.pageModel ? root.pageModel.backgroundEnd : "#000000"
        tileRoundness: root.pageModel ? root.pageModel.tileRoundness : 0
        appearance: root.pageModel ? root.pageModel.appearance : ({})
        wallpaperSource: root.pageModel ? root.pageModel.wallpaperSource : ""
        composition: root.pageModel ? root.pageModel.composition : ({})
        grid: root.pageModel ? root.pageModel.grid : ({})
        placements: root.pageModel ? root.pageModel.placements : []
        artworkMotionAllowed: !root.reducedMotion
        internetAvailable: false
        localNetworkAvailable: false
        hasContextualSources: false
    }
        }
    }
}
