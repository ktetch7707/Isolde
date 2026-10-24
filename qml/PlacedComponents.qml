import QtQuick

Item {
    id:surface
    required property var items
    required property var metrics
    required property var host
    property real unit:1
    property var catalog:host.authoredItems || host.items || []
    // Proposal replicas render real components without becoming live focus,
    // geometry or provider clients in the underlying page.
    property bool previewOverlay:false
    property string editorSelectionId:host.editSelection || ""
    signal activated(var item)
    signal focused(var card)
    Repeater {
        model:surface.metrics.valid===false ? []:surface.items
        delegate:Item {
            id:node
            required property var modelData
            Accessible.ignored:!node.group && !node.library
            Accessible.role:Accessible.Button
            Accessible.name:(modelData.item.accessibility || {}).name || modelData.item.name || "Component"
            Accessible.description:(modelData.item.accessibility || {}).hint || modelData.item.unavailableReason || ""
            readonly property bool apps:!!modelData.item.appCollection
            readonly property bool pageMenu:modelData.item.family==="page-menu"
            readonly property bool group:modelData.item.componentType==="container" && !apps && !pageMenu
            readonly property bool library:modelData.item.componentType==="library"
            readonly property bool selected:surface.host.editMode ? surface.editorSelectionId===modelData.item.id : !surface.host.recoveryFocused && surface.host.focusedIndex===surface.host.itemIndex(modelData.item.id)
            readonly property var baseStyle:modelData.item.presentation || ({})
            readonly property var style:Object.assign({},baseStyle,(baseStyle.states || {}).normal || {},(baseStyle.states || {})[modelData.item.designState || (modelData.item.unavailableReason?"unavailable":node.selected?"focused":"normal")] || {})
            readonly property var layout:style.layout || ({})
            readonly property real padding:(layout.padding || 0)*surface.unit
            readonly property real headingHeight:style.showHeading?(style.headingSize || 24)*surface.unit*(modelData.item.unavailableReason?2.6:1.5):0
            readonly property var localMetrics: {
                const columns=layout.columns || 1,rows=layout.rows || 1,gap=(layout.gap || 0)*surface.unit
                const w=width-2*padding,h=height-2*padding-headingHeight,visibleColumns=layout.visibleColumns || columns
                const valid=w>(visibleColumns-1)*gap && h>(rows-1)*gap
                return {columns:columns,rows:rows,gap:gap,margin:0,cellW:valid?(w-(visibleColumns-1)*gap)/visibleColumns:0,cellH:valid?(h-(rows-1)*gap)/rows:0,valid:valid}
            }
            readonly property var childEntries:surface.catalog.filter(i=>(i.parentId || "")===modelData.item.id && !!i.placement).map(i=>({item:i,placement:i.placement}))
            visible:surface.previewOverlay || !surface.host.editMode || (surface.host.suppressedComponentIds || []).indexOf(modelData.item.id)<0
            objectName:"placed-"+modelData.item.id
            x:surface.metrics.margin+modelData.placement.x*(surface.metrics.cellW+surface.metrics.gap)
            y:surface.metrics.margin+modelData.placement.y*(surface.metrics.cellH+surface.metrics.gap)
            width:surface.metrics.cellW*modelData.placement.width+surface.metrics.gap*(modelData.placement.width-1)
            height:surface.metrics.cellH*modelData.placement.height+surface.metrics.gap*(modelData.placement.height-1)
            Item {
            anchors.fill:parent
            opacity:node.modelData.item.opacity===undefined?1:node.modelData.item.opacity
            HomeCard {
                id:card
                objectName:"placedCard-"+node.modelData.item.id
                visible:!node.group && !node.library && !node.apps && !node.pageMenu
                anchors.fill:parent;item:modelData.item;appearance:surface.host.appearance;unit:surface.unit
                selectMode:surface.host.selectMode;selectTrigger:surface.host.selectTrigger
                selectAction:surface.host.selectAction;selectionColor:surface.host.selectionColor
                selected:node.selected
                outsideColor:surface.host.backgroundEnd
                onSelectedChanged:if(selected && !surface.host.editMode)surface.focused(node)
                onActivated:surface.activated(modelData.item)
            }
            Rectangle {
                id:groupSurface
                anchors.fill:parent;visible:node.group;radius:Math.min((node.style.radius || 0)*surface.unit,width/2,height/2)
                readonly property color ink:node.style.surfaceColor || surface.host.appearance.surfaceColor || surface.host.backgroundEnd
                color:Qt.rgba(ink.r,ink.g,ink.b,node.style.surfaceOpacity===undefined?1:node.style.surfaceOpacity)
                ArtworkImage {anchors.fill:parent;source:node.modelData.item.logoSource || "";crop:node.style.imageCrop || null;radius:parent.radius;fit:node.style.artFit || "cover";focalX:node.style.artFocalX===undefined?.5:node.style.artFocalX;focalY:node.style.artFocalY===undefined?.5:node.style.artFocalY;zoom:node.style.artZoom || 1}
                readonly property bool selected:node.selected
                border.width:(selected?Math.max(2,node.style.focusWidth===undefined?3:node.style.focusWidth):node.style.borderWidth || 0)*surface.unit
                border.color:selected?node.style.focusColor || surface.host.appearance.focusColor || surface.host.appearance.textColor || "transparent":node.style.borderColor || "transparent"
            }
            Text {
                visible:node.group && !!node.style.showHeading;x:node.padding;y:node.padding;width:parent.width-2*node.padding;height:node.headingHeight
                text:(node.modelData.item.name || "")+(node.modelData.item.unavailableReason?"\n"+node.modelData.item.unavailableReason:"");textFormat:Text.PlainText;elide:Text.ElideRight;maximumLineCount:2;wrapMode:Text.Wrap
                color:node.style.headingColor || node.style.labelColor || surface.host.appearance.textColor || "transparent";font.pixelSize:(node.style.headingSize || 24)*surface.unit
                font.bold:!!node.style.headingBold; font.family:surface.host.appearance.fontFamily || "DejaVu Sans"
            }
            MouseArea {anchors.fill:parent;enabled:node.group;onClicked:surface.activated(node.modelData.item)}
            Text {
                anchors.centerIn:parent;width:Math.max(0,parent.width-2*node.padding);visible:node.group && !node.localMetrics.valid
                text:"Internal grid has no room. Increase the group size or reduce its spacing.";textFormat:Text.PlainText;wrapMode:Text.Wrap
                color:surface.host.appearance.textColor || "transparent";font.pixelSize:18*surface.unit
            }
            Flickable {
                id:viewport;x:node.padding;y:node.padding+node.headingHeight;width:parent.width-2*node.padding;height:parent.height-2*node.padding-node.headingHeight
                contentWidth:node.localMetrics.valid?node.localMetrics.columns*node.localMetrics.cellW+(node.localMetrics.columns-1)*node.localMetrics.gap:width
                contentHeight:height;clip:true;interactive:node.group && !surface.host.editMode;boundsBehavior:Flickable.StopAtBounds
                Loader {
                id:contents;width:viewport.contentWidth;height:viewport.height
                function viewportRect(){const point=viewport.mapToItem(surface.host,0,0);return {x:point.x,y:point.y,width:viewport.width,height:viewport.height}}
                function revealPlacement(p){const start=p.x*(node.localMetrics.cellW+node.localMetrics.gap),end=start+p.width*node.localMetrics.cellW+(p.width-1)*node.localMetrics.gap;if(start<viewport.contentX)viewport.contentX=start;else if(end>viewport.contentX+viewport.width)viewport.contentX=Math.min(viewport.contentWidth-viewport.width,end-viewport.width)}
                function editorViewport(){return {x:viewport.contentX,y:viewport.contentY}}
                function restoreEditorViewport(state){viewport.cancelFlick();viewport.contentX=state.x;viewport.contentY=state.y}
                Component.onCompleted:if(node.group)setSource(Qt.resolvedUrl("PlacedComponents.qml"),{
                    items:Qt.binding(()=>node.childEntries),metrics:Qt.binding(()=>node.localMetrics),host:surface.host,
                    unit:Qt.binding(()=>surface.unit),catalog:Qt.binding(()=>surface.catalog),previewOverlay:surface.previewOverlay,
                    editorSelectionId:Qt.binding(()=>surface.editorSelectionId)
                })
                onLoaded:if(!surface.previewOverlay && surface.host.registerGroupSurface)surface.host.registerGroupSurface(node.modelData.item.id,contents,false)
                Component.onDestruction:if(!surface.previewOverlay && node.group && surface.host.registerGroupSurface)surface.host.registerGroupSurface(node.modelData.item.id,contents,true)
                }
            }
            Connections {
                target:contents.item
                function onActivated(item){surface.activated(item)}
                function onFocused(card){if(card && card.modelData)contents.revealPlacement(card.modelData.placement);surface.focused(card)}
            }
            Loader {
                id:appsLoader;anchors.fill:parent;active:node.apps
                sourceComponent:PageAppCarousel {
                    registry:surface.host.appProvider;collection:node.modelData.item.appCollection;style:node.style;appearance:surface.host.appearance;unit:surface.unit
                    readOnly:surface.host.editMode || surface.host.previewOnly
                    engaged:surface.host.activeMediaWidgetId===node.modelData.item.id;highlighted:node.selected
                    onEngage:if(!readOnly)surface.host.engageMediaWidget(node.modelData.item.id)
                    onLaunch:action=>{if(surface.host.rememberAppFocus)surface.host.rememberAppFocus(node.modelData.item.id,focusState());surface.host.actionActivated(action,node.modelData.item.id)}
                }
                onLoaded:if(!surface.previewOverlay)surface.host.registerMediaWidget(node.modelData.item.id,item,false)
                Component.onDestruction:if(!surface.previewOverlay && node.apps && surface.host)surface.host.registerMediaWidget(node.modelData.item.id,item,true)
            }
            Loader {
                id:menuLoader;anchors.fill:parent;active:node.pageMenu
                sourceComponent:PageMenuWidget {
                    pages:surface.host.menuPages;activeId:surface.host.menuActiveId
                    style:node.style;appearance:surface.host.appearance;unit:surface.unit
                    readOnly:surface.host.editMode || surface.host.previewOnly
                    engaged:surface.host.activeMediaWidgetId===node.modelData.item.id;highlighted:node.selected
                    onEngage:if(!readOnly)surface.host.engageMediaWidget(node.modelData.item.id)
                    onLaunch:action=>surface.host.actionActivated(action,node.modelData.item.id)
                }
                onLoaded:if(!surface.previewOverlay)surface.host.registerMediaWidget(node.modelData.item.id,item,false)
                Component.onDestruction:if(!surface.previewOverlay && node.pageMenu && surface.host)surface.host.registerMediaWidget(node.modelData.item.id,item,true)
            }
            Loader {
                id:media;anchors.fill:parent;active:node.library
                sourceComponent:PageLibraryWidget {
                    provider:surface.previewOverlay?null:surface.host.libraryProvider;widgetKey:surface.host.pageKey+"."+node.modelData.item.id
                    style:node.style;appearance:surface.host.appearance
                    readOnly:surface.host.editMode || surface.host.previewOnly;frozen:surface.host.freezeData
                    samplePreview:surface.host.samplePreview;sampleItems:surface.host.sampleItems
                highlighted:node.selected
                    engaged:surface.host.activeMediaWidgetId===node.modelData.item.id
                    onEngage:if(!readOnly)surface.host.engageMediaWidget(node.modelData.item.id)
                }
                onLoaded:if(!surface.previewOverlay)surface.host.registerMediaWidget(node.modelData.item.id,item,false)
                Component.onDestruction:if(!surface.previewOverlay && node.library && surface.host)surface.host.registerMediaWidget(node.modelData.item.id,item,true)
            }
            }
            Rectangle {
                objectName:"studioRecoveryFocus-"+node.modelData.item.id
                // Selection is editor state; never brighten the user's artwork.
                visible:surface.host.editMode && node.selected && ((node.modelData.item.opacity===undefined?1:node.modelData.item.opacity)<.35 || node.style.focusEnabled===false)
                readonly property var frame:!node.group && !node.library && !node.apps ? card.focusFrame : ({x:0,y:0,width:node.width,height:node.height,radius:groupSurface.radius,color:node.style.focusColor || surface.host.appearance.focusColor,lineWidth:Math.max(2,node.style.focusWidth===undefined?3:node.style.focusWidth)*surface.unit})
                x:frame.x;y:frame.y;width:frame.width;height:frame.height;radius:frame.radius
                color:"transparent";border.color:frame.color;border.width:frame.lineWidth
            }
        }
    }
}
