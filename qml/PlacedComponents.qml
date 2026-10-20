import QtQuick

Item {
    id:surface
    required property var items
    required property var metrics
    required property var host
    property real unit:1
    property var catalog:host.authoredItems || host.items || []
    signal activated(var item)
    signal focused(var card)
    Repeater {
        model:surface.metrics.valid===false ? []:surface.items
        delegate:Item {
            id:node
            required property var modelData
            readonly property bool group:modelData.item.componentType==="container"
            readonly property var style:modelData.item.presentation || ({})
            readonly property var layout:style.layout || ({})
            readonly property real padding:(layout.padding || 0)*surface.unit
            readonly property real headingHeight:style.showHeading?(style.headingSize || 24)*surface.unit*(modelData.item.unavailableReason?2.6:1.5):0
            readonly property var localMetrics: {
                const columns=layout.columns || 1,rows=layout.rows || 1,gap=(layout.gap || 0)*surface.unit
                const w=width-2*padding,h=height-2*padding-headingHeight
                const valid=w>(columns-1)*gap && h>(rows-1)*gap
                return {columns:columns,rows:rows,gap:gap,margin:0,cellW:valid?(w-(columns-1)*gap)/columns:0,cellH:valid?(h-(rows-1)*gap)/rows:0,valid:valid}
            }
            readonly property var childEntries:surface.catalog.filter(i=>(i.parentId || "")===modelData.item.id && !!i.placement).map(i=>({item:i,placement:i.placement}))
            visible:!surface.host.editMode || (surface.host.suppressedComponentIds || []).indexOf(modelData.item.id)<0
            objectName:"placed-"+modelData.item.id
            x:surface.metrics.margin+modelData.placement.x*(surface.metrics.cellW+surface.metrics.gap)
            y:surface.metrics.margin+modelData.placement.y*(surface.metrics.cellH+surface.metrics.gap)
            width:surface.metrics.cellW*modelData.placement.width+surface.metrics.gap*(modelData.placement.width-1)
            height:surface.metrics.cellH*modelData.placement.height+surface.metrics.gap*(modelData.placement.height-1)
            opacity:modelData.item.opacity===undefined?1:modelData.item.opacity
            HomeCard {
                visible:!node.group
                anchors.fill:parent;item:modelData.item;appearance:surface.host.appearance;unit:surface.unit
                selectMode:surface.host.selectMode;selectTrigger:surface.host.selectTrigger
                selectAction:surface.host.selectAction;selectionColor:surface.host.selectionColor
                selected:!surface.host.editMode && !surface.host.recoveryFocused && surface.host.focusedIndex===surface.host.itemIndex(modelData.item.id)
                outsideColor:surface.host.backgroundEnd
                onSelectedChanged:if(selected)surface.focused(node)
                onActivated:surface.activated(modelData.item)
            }
            Rectangle {
                anchors.fill:parent;visible:node.group;radius:Math.min((node.style.radius || 0)*surface.unit,width/2,height/2)
                readonly property color ink:node.style.surfaceColor || surface.host.appearance.surfaceColor || surface.host.backgroundEnd
                color:Qt.rgba(ink.r,ink.g,ink.b,node.style.surfaceOpacity===undefined?1:node.style.surfaceOpacity)
                ArtworkImage {anchors.fill:parent;source:node.modelData.item.logoSource || "";crop:node.style.imageCrop || null;radius:parent.radius}
                border.width:!surface.host.editMode && !surface.host.recoveryFocused && surface.host.focusedIndex===surface.host.itemIndex(node.modelData.item.id)?3*surface.unit:0
                border.color:surface.host.appearance.focusColor || surface.host.appearance.textColor || "transparent"
            }
            Text {
                visible:node.group && !!node.style.showHeading;x:node.padding;y:node.padding;width:parent.width-2*node.padding;height:node.headingHeight
                text:(node.modelData.item.name || "")+(node.modelData.item.unavailableReason?"\n"+node.modelData.item.unavailableReason:"");textFormat:Text.PlainText;elide:Text.ElideRight;maximumLineCount:2;wrapMode:Text.Wrap
                color:node.style.headingColor || surface.host.appearance.textColor || "transparent";font.pixelSize:(node.style.headingSize || 24)*surface.unit
                font.family:surface.host.appearance.fontFamily || "DejaVu Sans"
            }
            MouseArea {anchors.fill:parent;enabled:node.group;onClicked:surface.activated(node.modelData.item)}
            Text {
                anchors.centerIn:parent;width:Math.max(0,parent.width-2*node.padding);visible:node.group && !node.localMetrics.valid
                text:"Internal grid has no room. Increase the group size or reduce its spacing.";textFormat:Text.PlainText;wrapMode:Text.Wrap
                color:surface.host.appearance.textColor || "transparent";font.pixelSize:18*surface.unit
            }
            Loader {
                id:contents;x:node.padding;y:node.padding+node.headingHeight;width:parent.width-2*node.padding;height:parent.height-2*node.padding-node.headingHeight
                Component.onCompleted:if(node.group)setSource(Qt.resolvedUrl("PlacedComponents.qml"),{
                    items:Qt.binding(()=>node.childEntries),metrics:Qt.binding(()=>node.localMetrics),host:surface.host,
                    unit:Qt.binding(()=>surface.unit),catalog:Qt.binding(()=>surface.catalog)
                })
                onLoaded:if(surface.host.registerGroupSurface)surface.host.registerGroupSurface(node.modelData.item.id,contents,false)
                Component.onDestruction:if(node.group && surface.host.registerGroupSurface)surface.host.registerGroupSurface(node.modelData.item.id,contents,true)
            }
            Connections {
                target:contents.item
                function onActivated(item){surface.activated(item)}
                function onFocused(card){surface.focused(card)}
            }
        }
    }
}
