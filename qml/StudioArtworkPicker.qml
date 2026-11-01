import QtQuick

Item {
    id:root
    required property var session
    required property var appearance
    property bool stillOnly:false
    property var listing:({entries:[],page:0,pages:1})
    property int selected:0
    property string preview:""
    property string candidate:""
    property string message:""
    readonly property real unit:Math.min(width/1280,height/720)
    readonly property var entries:(listing.entries || []).filter(e=>!stillOnly || e.folder || !e.path.toLowerCase().endsWith(".gif"))
    readonly property var tools:["Parent folder","Home folder","Pictures","Previous page","Next page","Advanced path…","Cancel"]
    signal picked(string source)
    signal canceled()
    signal advancedPath()
    function browse(path,page){
        const result=session.browseArtwork(path || "",page || 0)
        if(!result.directory){message=session.errorString;return false}
        listing=result;selected=entries.length?tools.length:0;preview="";candidate="";message=entries.length?"Select an image to preview, then choose Import.":"No supported images here. Choose another folder.";return true
    }
    function begin(){if(!browse("",0))browse(listing.home || "/",0)}
    function activate(){
        if(candidate){if(selected===0)picked(candidate);else{candidate="";preview="";selected=tools.length}return}
        if(selected<tools.length){
            if(selected===0)browse(listing.parent,0)
            else if(selected===1)browse(listing.home,0)
            else if(selected===2)browse(listing.pictures,0)
            else if(selected===3 && listing.page>0)browse(listing.directory,listing.page-1)
            else if(selected===4 && listing.page+1<listing.pages)browse(listing.directory,listing.page+1)
            else if(selected===5)advancedPath()
            else if(selected===6)canceled()
            return
        }
        const entry=entries[selected-tools.length];if(!entry)return
        if(entry.folder){browse(entry.path,0);return}
        const image=session.artworkPreview(entry.path)
        if(!image){message=session.errorString;return}
        preview=image;candidate=entry.path;selected=0;message="Import stages a private copy. Apply saves it to this profile."
    }
    function handleCommand(command){
        if(command===inputManager.back){if(candidate){candidate="";preview="";selected=tools.length}else canceled();return}
        if(command===inputManager.confirm){activate();return}
        const d=command===inputManager.navigateDown || command===inputManager.navigateRight?1:command===inputManager.navigateUp || command===inputManager.navigateLeft?-1:0
        selected=Math.max(0,Math.min((candidate?2:tools.length+entries.length)-1,selected+d))
    }
    Component.onCompleted:begin()
    Rectangle {anchors.fill:parent;color:root.appearance.surfaceColor;opacity:.98}
    Column {
        x:36*root.unit;y:24*root.unit;width:parent.width-72*root.unit;spacing:12*root.unit
        Text {text:root.candidate?"Import image":"Choose local image";color:root.appearance.textColor;font.pixelSize:28*root.unit;font.bold:true}
        Text {width:parent.width;text:root.candidate || root.listing.directory || "";textFormat:Text.PlainText;color:root.appearance.mutedColor;font.pixelSize:17*root.unit;elide:Text.ElideMiddle}
        Text {width:parent.width;text:root.message;textFormat:Text.PlainText;color:root.appearance.textColor;font.pixelSize:18*root.unit;wrapMode:Text.Wrap}
        Row {
            spacing:28*root.unit
            ListView {
                id:list;objectName:"studioArtworkFiles";width:root.width*.48;height:root.height-190*root.unit;clip:true
                model:root.candidate?["Import","Back to images"]:root.tools.concat(root.entries.map(e=>(e.folder?"Folder · ":"Image · ")+e.name))
                currentIndex:root.selected;onCurrentIndexChanged:positionViewAtIndex(currentIndex,ListView.Contain)
                delegate:Rectangle {
                    required property string modelData;required property int index
                    width:list.width;height:48*root.unit;radius:8*root.unit;color:root.selected===index?root.appearance.selectedColor:"transparent"
                    border.width:root.selected===index?2:0;border.color:root.appearance.focusColor
                    Text {anchors.fill:parent;anchors.margins:10*root.unit;text:parent.modelData;textFormat:Text.PlainText;elide:Text.ElideRight;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                    MouseArea {anchors.fill:parent;onClicked:{root.selected=parent.index;root.activate()}}
                }
            }
            Column {
                width:root.width*.4;spacing:16*root.unit
                Image {objectName:"studioArtworkPreview";width:parent.width;height:root.height*.48;source:root.preview;fillMode:Image.PreserveAspectFit;sourceSize:Qt.size(600,400);cache:false}
                Text {width:parent.width;text:root.candidate?"Validated preview · PNG/JPEG or bounded GIF still":"PNG / JPEG"+(root.stillOnly?"":" / GIF")+" · Page "+((root.listing.page || 0)+1)+" of "+(root.listing.pages || 1);color:root.appearance.mutedColor;font.pixelSize:18*root.unit;wrapMode:Text.Wrap}
            }
        }
    }
}
