import QtQuick
import QtQuick.Dialogs

Rectangle {
    id: root
    objectName: "homeDesignEditor"
    required property var layoutModel
    property bool open: false
    property int scope: 0
    property int target: 0
    property int selected: 0
    property bool typing: false
    property var editField: ({})
    property string message: ""
    readonly property var scopes: ["Dashboard", "Section", "Card", "Navigation tab"]
    readonly property var config: layoutModel.composition
    readonly property var targets: scope === 1 ? layoutModel.categories : scope === 2 ? layoutModel.items : scope === 3 ? config.navigation || [] : []
    readonly property var current: targets[target] || ({})
    readonly property var style: current.presentation || ({})
    readonly property var inheritedStyle: {
        if (scope !== 2) return ({})
        for (const section of layoutModel.categories) if (section.id === current.categoryId) return section.presentation || ({})
        return ({})
    }
    readonly property var variants: ["launcher", "source", "media", "banner", "summary", "orb", "recent", "library", "text"]
    function number(key, label, min, max, step, fallback) { return {key:key,label:label,min:min,max:max,step:step,fallback:fallback} }
    function text(key, label) { return {key:key,label:label,text:true} }
    function toggle(key, label, fallback) { return {key:key,label:label,toggle:true,fallback:fallback} }
    function enumeration(key, label, choices) { return {key:key,label:label,choices:choices} }
    readonly property var fields: {
        if (scope === 0) return [toggle("enabled","Composed Home",false), number("columns","Dashboard columns",1,2,1,1),
            number("margin","Outer margin",0,100,2,40),number("gap","Section gap",0,100,1,20),toggle("fit","Fit sections on screen",true),
            number("designHeight","Design height",480,1440,10,800),toggle("showProfile","Profile label",false),
            enumeration("headerMode","Header style",["navigation","identity"]),text("identity","Header name"),toggle("showClock","Header clock",false),
            enumeration("headerControls","Header buttons",["search,settings","settings,edit","search,settings,edit","settings","edit"]),
            enumeration("initialTab","Starting tab",(config.navigation || []).map(t => t.id)),
            {key:"wallpaper",label:"Choose wallpaper",command:true},{key:"removeWallpaper",label:"Remove wallpaper",command:true}].filter(f => layoutModel.isPage || ["pageNavigation","sidebarWidth"].indexOf(f.key)<0)
        if (scope === 3) return [text("name","Tab name"),text("sections","Visible sections (IDs separated by commas; empty shows all)"),
            enumeration("action.type","Tab action",["none","open-page","app-drawer","settings","launch-app","open-collection","desktop-mode","open-url","open-folder","open-file","open-disc","play-movie"]),
            text("action.pageId","Destination page ID"),text("action.appId","Action app ID"),text("action.collectionId","Action collection ID"),text("action.target","Action target"),text("action.openWith","Open with app ID"),
            number("action.section","Settings section (-1 = overview)",-1,5,1,-1),number("action.movieId","Kodi movie ID",1,2147483647,1,1),
            {key:"addTab",label:"Add navigation tab",command:true},{key:"removeTab",label:"Remove this tab",command:true}]
        let result = [enumeration("variant","Card style",scope===1?variants:variants.filter(v=>v!=="library")),text("subtitle","Subtitle"),text("label","Artwork badge"),text("icon","Icon name"),
            text("surfaceColor","Card color"),text("iconColor","Icon color"),text("labelColor","Label color"),text("selectedLabelColor","Selected label color"),
            toggle("labelPill","Label pill",false),number("diameter","Circle diameter",0,400,2,110),number("iconSize","Icon size",0,400,2,48),
            number("labelSize","Label size",0,400,1,18),number("surfaceOpacity","Card opacity",0,1,0.05,1),number("radius","Corner radius",0,400,1,16),
            number("cardGap","Card gap",0,400,1,14),number("artHeight","Artwork height",0,400,2,84)]
        if (scope === 1) result = [text("sectionName","Section name"),number("columns","Cards per row",1,12,1,1),number("span","Dashboard lane span",1,2,1,1),
            number("height","Section height",60,800,10,180),number("contentWidth","Centered content width",100,3840,10,1200),
            enumeration("libraryKind","Kodi library",["movies","shows"]),enumeration("libraryView","Kodi view",["all","recent","continue","search","tag","genre","collection"]),
            text("libraryValue","Search / genre / tag / collection ID"),enumeration("libraryLayout","Library arrangement",["grid","list","row"]),number("libraryColumns","Library columns",1,12,1,4),
            enumeration("libraryCardStyle","Library card style",["media","recent","orb","source","summary","banner"]),
            toggle("showHeading","Show heading",true),number("headingSize","Heading size",0,400,1,24),toggle("headingBold","Bold heading",false),
            text("headingColor","Heading color"),text("panelColor","Panel tint"),number("panelOpacity","Panel opacity",0,1,0.05,0),number("blur","Panel blur",0,1,0.05,0),
            number("padding","Panel padding",0,400,1,0),text("trailingLabel","Section link label"),
            enumeration("trailingAction.type","Section link action",["none","open-page","app-drawer","settings","launch-app","open-collection"]),
            text("trailingAction.pageId","Link page ID"),text("trailingAction.appId","Link app ID"),text("trailingAction.collectionId","Link collection ID"),number("trailingAction.section","Link Settings section",-1,5,1,-1)].concat(result)
        else result = result.concat([text("cardName","Card name"),enumeration("action.type","Card action",["open-page","app-drawer","settings","launch-app","open-collection","desktop-mode","open-url","open-folder","open-file","open-disc","play-movie"]),text("action.pageId","Destination page ID"),text("action.appId","App ID"),text("action.collectionId","Collection ID"),text("action.target","Target"),text("action.openWith","Open with app ID"),number("action.section","Settings section",-1,5,1,-1),number("action.movieId","Kodi movie ID",1,2147483647,1,1),number("progress","Progress",0,1,0.01,0),number("imageCrop.x","Crop left",0,16384,1,0),number("imageCrop.y","Crop top",0,16384,1,0),
            number("imageCrop.width","Crop width",1,16384,1,100),number("imageCrop.height","Crop height",1,16384,1,100),{key:"cardArtwork",label:"Choose card artwork",command:true},
            {key:"removeArtwork",label:"Remove card artwork",command:true},{key:"removeCrop",label:"Use full artwork",command:true}])
        if(scope===2 && current.kind==="widget")result=result.filter(f=>!f.key.startsWith("action."))
        return result.concat([{key:"inherit",label:"Reset presentation overrides",command:true}])
    }
    color: "#101014"; visible: open
    function begin() { scope = 0; target = 0; selected = 0; typing = false; message = ""; open = true }
    function value(field) {
        let source = scope === 0 ? config : scope === 3 ? current : scope === 2 ? Object.assign({},inheritedStyle,style) : style
        if (field.key === "sectionName" || field.key === "cardName") return current.name
        if(scope === 2 && field.key.startsWith("action.")) return (current.action || {})[field.key.split(".")[1]] || field.fallback || ""
        const path = field.key.split(".")
        let value = source[path[0]]
        if (path.length > 1) value = (value || {})[path[1]]
        if (Array.isArray(value)) return value.join(",")
        return value === undefined ? field.fallback === undefined ? "" : field.fallback : value
    }
    function store(field, value) {
        if (field.key === "cardName") { if(!layoutModel.setItemName(current.id,value)) message=layoutModel.errorString; return }
        if (field.key === "sectionName") { if (!layoutModel.renameCategory(current.id,value)) message = layoutModel.errorString; return }
        let source = Object.assign({}, scope === 0 ? config : scope === 3 ? current : style)
        if(scope === 2 && field.key.startsWith("action.")) source.action=Object.assign({},current.action || {})
        const path = field.key.split(".")
        if (path.length > 1) {
            let nested = Object.assign({},source[path[0]] || (path[0] === "imageCrop" ? {x:0,y:0,width:100,height:100} : {}))
            if (path[1] === "type") {
                if (value === "none") delete source[path[0]]
                else {
                    nested = {type:value}
                    if(value === "open-page") nested.pageId = "choose-page"
                    if (value === "settings") nested.section = -1
                    if (value === "launch-app") nested.appId = "movies"
                    if (value === "open-collection" || value === "play-movie") nested.collectionId = "movies"
                    if (value === "play-movie") nested.movieId = 1
                    if (value === "open-url") nested.target = "https://example.com"
                    if (["open-folder","open-file","open-disc"].indexOf(value) >= 0) { nested.target = "/media"; nested.openWith = "system" }
                    source[path[0]] = nested
                }
            } else { nested[path[1]] = value; source[path[0]] = nested }
        } else if ((field.key === "sections" || field.key === "headerControls")) {
            if (!value.trim()) delete source[field.key]
            else source[field.key] = value.split(",").map(s => s.trim()).filter(s => s.length)
        } else if (field.text && value === "") delete source[field.key]
        else source[field.key] = value
        if(scope === 2 && field.key.startsWith("action.")){message=layoutModel.setItemAction(current.id,source.action)?"Saved":layoutModel.errorString;return}
        let ok
        if (scope === 0) { source.schemaVersion = 1; ok = layoutModel.setComposition(source) }
        else if (scope === 3) { let next = Object.assign({},config); let tabs = (next.navigation || []).slice(); tabs[target] = source; next.navigation = tabs; ok = layoutModel.setComposition(next) }
        else ok = scope === 1 ? layoutModel.setSectionPresentation(current.id,source) : layoutModel.setItemPresentation(current.id,source)
        message = ok ? "Saved" : layoutModel.errorString
    }
    function activate(direction) {
        if (selected === 0) { scope = (scope + direction + 4) % 4; target = 0; return }
        if (selected === 1) { target = Math.max(0,Math.min(targets.length-1,target+direction)); return }
        const field = fields[selected-2]; if (!field) return
        if (scope !== 0 && !current.id && field.key !== "addTab") { message = "Add a section, card or navigation tab first."; return }
        if (field.command) {
            if (field.key === "wallpaper") wallpaper.open()
            else if (field.key === "cardArtwork") cardArtwork.open()
            else if (field.key === "removeArtwork") layoutModel.clearTileLogo(current.id)
            else if (field.key === "removeWallpaper") { let next = Object.assign({},config); delete next.wallpaper; layoutModel.setComposition(next) }
            else if (field.key === "removeCrop") { let next = Object.assign({},style); delete next.imageCrop; layoutModel.setItemPresentation(current.id,next) }
            else if (field.key === "inherit") { if (scope === 1) layoutModel.setSectionPresentation(current.id,{}); else layoutModel.setItemPresentation(current.id,{}) }
            else {
                let next = Object.assign({},config); next.schemaVersion = 1; let tabs = (next.navigation || []).slice()
                if (field.key === "addTab") { const id = "page-" + Date.now(); tabs.push({id:id,name:"New page"}); target = tabs.length-1 }
                else { const removed = tabs.splice(target,1); if (removed.length && next.initialTab === removed[0].id) delete next.initialTab; target = Math.max(0,target-1) }
                next.navigation = tabs; if (!layoutModel.setComposition(next)) message = layoutModel.errorString
            }
        } else if (field.toggle) store(field,!value(field))
        else if (field.choices) {
            if (!field.choices.length) return
            store(field,field.choices[(Math.max(0,field.choices.indexOf(String(value(field))))+direction+field.choices.length)%field.choices.length])
        } else if (field.step) store(field,Math.round(Math.max(field.min,Math.min(field.max,Number(value(field))+direction*field.step))*100)/100)
        else { editField = field; keyboard.value = String(value(field)); keyboard.selectedIndex = 0; typing = true }
    }
    function handleCommand(command) {
        if (typing) { keyboard.handleCommand(command); return }
        if (command === inputManager.back) open = false
        else if (command === inputManager.navigateUp) selected = Math.max(0,selected-1)
        else if (command === inputManager.navigateDown) selected = Math.min(fields.length+1,selected+1)
        else if (command === inputManager.navigateLeft) activate(-1)
        else if (command === inputManager.navigateRight || command === inputManager.confirm) activate(1)
    }
    FileDialog { id:wallpaper; title:"Choose wallpaper"; nameFilters:["Images (*.png *.jpg *.jpeg)"]; onAccepted: { if (!root.layoutModel.setWallpaper(selectedFile.toString())) root.message = root.layoutModel.errorString } }
    FileDialog { id:cardArtwork; title:"Choose card artwork"; nameFilters:["Images (*.png *.jpg *.jpeg)"]; onAccepted: { if (!root.layoutModel.setTileLogo(root.current.id, selectedFile.toString())) root.message = root.layoutModel.errorString } }
    MouseArea { anchors.fill: parent }
    Column {
        anchors.fill: parent; anchors.margins: 40; spacing: 12
        Text { text: layoutModel.isPage ? "Design page" : "Design Home"; color: "#F5F1F3"; font.pixelSize: 34 }
        Text { width:parent.width; wrapMode:Text.WordWrap; text:"↑ ↓ choose a control · ← → adjust · OK edits text · Back closes. Changes save immediately. Empty text inherits the default. Colors use #RRGGBB."; color:"#AAA5AF"; font.pixelSize:16 }
        Text { text: "Editing: " + root.scopes[root.scope] + (root.current.id ? " · " + (root.current.name || root.current.id) + " (" + root.current.id + ")" : ""); color:"#E3ACC3"; font.pixelSize:20 }
        ListView {
            width:parent.width; height:Math.max(100,parent.height-180); spacing:6; clip:true; currentIndex:root.selected; highlightRangeMode:ListView.ApplyRange
            model: [{label:"Edit",key:"scope"},{label:"Choose target",key:"target"}].concat(root.fields)
            delegate: Rectangle {
                required property int index; required property var modelData
                width:ListView.view.width; height:48; radius:8; color:root.selected === index ? "#353039" : "#202126"; border.width:root.selected === index ? 2 : 0; border.color:"#D19AB1"
                Text { anchors.fill:parent; anchors.margins:12; color:"#F5F1F3"; font.pixelSize:16; elide:Text.ElideRight
                    text:modelData.label + (index === 0 ? ": " + root.scopes[root.scope] : index === 1 ? ": " + (root.current.name || root.current.id || "—") : modelData.command ? "" : ": " + (modelData.toggle ? root.value(modelData) ? "On" : "Off" : root.value(modelData))) }
                MouseArea { anchors.fill:parent; onClicked:{ root.selected=index; root.activate(1) } }
            }
        }
        Text { text:root.message; color:"#E3ACC3"; font.pixelSize:16 }
    }
    Rectangle { anchors.fill:parent; visible:root.typing; color:"#101014"
        OnScreenKeyboard { id:keyboard; anchors.fill:parent; anchors.margins:40; onAccepted:text => { root.store(root.editField,text); root.typing=false }; onCanceled:root.typing=false }
    }
}
