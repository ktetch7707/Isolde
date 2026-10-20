import QtQuick

Item {
    id:surface
    required property var items
    required property var metrics
    required property var host
    property real unit:1
    signal activated(var item)
    signal focused(var card)
    Repeater {
        model:surface.metrics.valid===false ? []:surface.items
        delegate:Item {
            id:node
            required property var modelData
            objectName:"placed-"+modelData.item.id
            x:surface.metrics.margin+modelData.placement.x*(surface.metrics.cellW+surface.metrics.gap)
            y:surface.metrics.margin+modelData.placement.y*(surface.metrics.cellH+surface.metrics.gap)
            width:surface.metrics.cellW*modelData.placement.width+surface.metrics.gap*(modelData.placement.width-1)
            height:surface.metrics.cellH*modelData.placement.height+surface.metrics.gap*(modelData.placement.height-1)
            HomeCard {
                anchors.fill:parent;item:modelData.item;appearance:surface.host.appearance;unit:surface.unit
                selectMode:surface.host.selectMode;selectTrigger:surface.host.selectTrigger
                selectAction:surface.host.selectAction;selectionColor:surface.host.selectionColor
                selected:!surface.host.editMode && !surface.host.recoveryFocused && surface.host.focusedIndex===surface.host.itemIndex(modelData.item.id)
                outsideColor:surface.host.backgroundEnd
                onSelectedChanged:if(selected)surface.focused(node)
                onActivated:surface.activated(modelData.item)
            }
        }
    }
}
