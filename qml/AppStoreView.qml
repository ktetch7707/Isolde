import QtQuick
import QtQuick.Controls

Item {
 id:root;objectName:"appStoreView"
 required property var entries
 property var catalog:appCatalog
 property var appearance:({})
 readonly property var options:catalog.storeOptions || ({})
 readonly property var platforms:options.platforms || []
 readonly property real gap:options.spacing || 20
 readonly property int columns:options.columns || 2
 readonly property real typeScale:appearance.typographyScale || 1
 readonly property color ink:appearance.textColor
 readonly property color surface:appearance.surfaceColor
 readonly property color focusColor:appearance.focusColor
 readonly property real corners:appearance.cornerRadius===undefined?16:appearance.cornerRadius
 property int platformIndex:0
 property bool deviceApps:false
 property string selectedId:""
 property string area:"grid"
 property int headerIndex:2
 property int actionIndex:0
 property int toolIndex:0
 property string confirmation:""
 property string confirmationAppId:""
 property bool setupSoftwareRendering:false
 property bool toolsMenuOpen:false
 property int toolsMenuIndex:0
 property int confirmIndex:0
 property string searchText:""
 property bool searchOpen:false
 property string keyboardPurpose:"search"
 property bool addWebOpen:false
 property string webName:""
 property string webAddress:""
 property string webBackend:options.webBackend || "chromium"
 property int webField:0
 property string returnAppId:""
 property string pendingOperation:""
 readonly property bool busy:!!catalog.busy
 readonly property var filtered:(entries || []).filter(e=>{
  const type=e.storeType || (e.presentationMode==="web-kiosk"?"web":e.type)
  const matches=deviceApps?type!=="android" && type!=="web":platforms[platformIndex] && type===platforms[platformIndex].type
  const q=searchText.trim().toLowerCase()
  return matches && (!q || [e.name,e.category,e.description,e.publisher].some(v=>String(v || "").toLowerCase().includes(q)))
 })
 readonly property var selected:filtered.find(e=>e.id===selectedId) || null
 readonly property var confirmationApp:(entries || []).find(e=>e.id===confirmationAppId) || null
 readonly property var actionItems:{
  if(!selected)return []
  if(!selected.installed)return [{op:"install",label:qsTr("Install app")}]
  let items=[{op:"open",label:qsTr("Open app")},{op:"uninstall",label:qsTr("Uninstall")}]
  if(selected.updateAvailable)items.push({op:"update",label:qsTr("Update to %1").arg(selected.updateVersion)})
  if(selected.updatesManaged)items.push({op:"subscription",label:selected.updatesEnabled?qsTr("Unsubscribe from updates"):qsTr("Subscribe to updates")})
  return items
 }
 readonly property var toolsItems:[qsTr("F-Droid access: ")+(catalog.onlineStoreSearch?qsTr("On"):qsTr("Off")),qsTr("Previous page"),qsTr("Next page"),qsTr("Check app updates"),qsTr("Set up / repair Android")]
 readonly property string operationName:catalog.operationName || ((entries || []).find(e=>e.id===catalog.operationId) || selected || {}).name || ""
 Connections {
  target:root.catalog
  function onPackageInstalled(appId){root.selectedId=appId;root.area="actions";root.actionIndex=0}
 }
 signal openRequested(string appId)
 signal operationSettled()
 onVisibleChanged:if(visible)Qt.callLater(()=>{if(root.visible)root.forceActiveFocus()})
 onBusyChanged:if(!busy)operationSettled()
 function revealAction(){
  if(area!=="actions"){details.contentY=0;return}
  const item=actions.itemAt(actionIndex);if(!item)return
  const top=item.mapToItem(detailContent,0,0).y
  details.contentY=Math.max(0,Math.min(Math.max(0,details.contentHeight-details.height),top+item.height-details.height))
 }
 onAreaChanged:Qt.callLater(revealAction)
 onActionIndexChanged:Qt.callLater(revealAction)
 onSelectedChanged:Qt.callLater(revealAction)
 onFilteredChanged:{if(!filtered.some(e=>e.id===selectedId))selectedId=filtered.length?filtered[0].id:"";actionIndex=0}
 function choosePlatform(i){platformIndex=i;deviceApps=false;area="header";headerIndex=i+2;confirmation=""}
 function beginSearch(){keyboardPurpose="search";searchOpen=true;keyboard.value=searchText;keyboard.selectedIndex=0}
 function editWeb(field){keyboardPurpose=field===0?"webName":"webAddress";keyboard.value=field===0?webName:webAddress;keyboard.selectedIndex=0;searchOpen=true}
 function storeTool(){
  if(platforms[platformIndex].type==="web"){addWebOpen=true;webField=0;webName="";webAddress=""}
  else catalog.setOnlineStoreSearch(!catalog.onlineStoreSearch)
 }
 function request(op){
  if(busy)return
  if(op==="setup"){confirmation=op;confirmationAppId="";confirmIndex=0;return}
  if(!selected || (op==="install" && selected.compatible===false))return
  if(op==="open"){returnAppId=selected.id;openRequested(selected.id)}
  else{confirmation=op;confirmationAppId=selected.id;confirmIndex=0}
 }
 function commit(){
  if(busy)return
  const op=confirmation,id=confirmationAppId,app=confirmationApp;confirmation="";pendingOperation=op
  if(op==="setup")catalog.setupAndroid(setupSoftwareRendering)
  else if(op==="install")catalog.install(id)
  else if(op==="update")catalog.updateApp(id)
  else if(op==="subscription" && app)catalog.setUpdatesEnabled(id,!app.updatesEnabled)
  else if(op==="uninstall")catalog.uninstall(id)
  area="actions";actionIndex=0
 }
 function chooseTool(index){
  if(!toolAvailable(index))return
  toolsMenuOpen=false
  if(index===0)catalog.setOnlineStoreSearch(!catalog.onlineStoreSearch)
  else if(index===1 || index===2)catalog.changeStorePage(index===1?-1:1)
  else if(index===3)catalog.checkForUpdates()
  else request("setup")
 }
 function toolAvailable(index){return index===1?!!catalog.hasPreviousStorePage:index===2?!!catalog.hasNextStorePage:index===3?!!catalog.onlineStoreSearch:true}
 function confirmationTitle(){
  if(confirmation==="setup")return qsTr("Set up Android?")
  const app=confirmationApp,name=app?app.name:""
  if(confirmation==="subscription")return app && app.updatesEnabled?qsTr("Unsubscribe %1 from updates?").arg(name):qsTr("Subscribe %1 to updates?").arg(name)
  return (confirmation==="update"?qsTr("Update "):confirmation==="install"?qsTr("Install "):qsTr("Uninstall "))+name+"?"
 }
 function confirmationMessage(){
  if(confirmation==="setup")return qsTr("Download and configure Android support on this appliance. Administrator approval and an internet connection are required. Existing profiles and app data are preserved. Cancel waits for the current system step to finish.")
  if(confirmation==="subscription")return confirmationApp && confirmationApp.updatesEnabled?qsTr("Keep this app and its data. Stop offering its updates in this profile. You can subscribe again at any time."):qsTr("Include this app when you choose Check app updates. Updates are never installed automatically.")
  if(confirmation==="update")return qsTr("Install the verified newer version in this profile. Keep the previous version and app data for rollback until the update is saved. Administrator approval may be requested to back up protected Android files.")
  if(confirmation==="install")return confirmationApp && confirmationApp.type==="android"?qsTr("Install the verified Android package in your current profile. Administrator approval may be requested for a safe backup."):qsTr("Add this app to your current profile.")
  return confirmationApp && confirmationApp.type==="android"?qsTr("Remove this app and its Android app data from this profile. Your pages and other profiles stay unchanged."):qsTr("Remove this app from your profile. Saved sign-in data is kept. Your pages stay unchanged.")
 }
 function handleCommand(cmd){
  if(busy){if((cmd===inputManager.back || cmd===inputManager.confirm) && catalog.canCancelOperation)catalog.cancelOperation();return true}
  if(toolsMenuOpen){
   if(cmd===inputManager.back)toolsMenuOpen=false
   else if(cmd===inputManager.navigateUp)toolsMenuIndex=Math.max(0,toolsMenuIndex-1)
   else if(cmd===inputManager.navigateDown)toolsMenuIndex=Math.min(toolsItems.length-1,toolsMenuIndex+1)
   else if(cmd===inputManager.confirm)chooseTool(toolsMenuIndex)
   return true
  }
  if(searchOpen){keyboard.handleCommand(cmd);return true}
  if(addWebOpen){
   if(cmd===inputManager.back)addWebOpen=false
   else if(cmd===inputManager.navigateUp)webField=Math.max(0,webField-1)
   else if(cmd===inputManager.navigateDown)webField=Math.min(4,webField+1)
   else if((cmd===inputManager.navigateLeft || cmd===inputManager.navigateRight) && webField===2)webBackend=webBackend==="chromium"?"embedded":"chromium"
   else if(cmd===inputManager.confirm){
    if(webField<2)editWeb(webField)
    else if(webField===2)webBackend=webBackend==="chromium"?"embedded":"chromium"
    else if(webField===3)addWebOpen=false
    else{const id=catalog.addWebApp(webName,webAddress,webBackend);if(id){addWebOpen=false;searchText="";selectedId=id;area="actions";actionIndex=0}}
   }
   return true
  }
  if(confirmation){
   if(cmd===inputManager.back)confirmation=""
   else if(cmd===inputManager.navigateLeft)confirmIndex=Math.max(0,confirmIndex-1)
   else if(cmd===inputManager.navigateRight)confirmIndex=Math.min(confirmation==="setup"?2:1,confirmIndex+1)
   else if(cmd===inputManager.confirm){if(confirmIndex===0)confirmation="";else if(confirmation==="setup" && confirmIndex===1)setupSoftwareRendering=!setupSoftwareRendering;else commit()}
   return true
  }
  if(cmd===inputManager.back){if(area==="actions"){area="grid";return true}return false}
  if(cmd===inputManager.search){beginSearch();return true}
  if(cmd===inputManager.menu && !deviceApps){toolsMenuOpen=true;toolsMenuIndex=0;return true}
  if(area==="tools"){
   if(cmd===inputManager.navigateDown)area=filtered.length?"grid":"header"
   else if(cmd===inputManager.navigateUp)area="header"
   else if(cmd===inputManager.navigateLeft)toolIndex=Math.max(0,toolIndex-1)
   else if(cmd===inputManager.navigateRight)toolIndex=Math.min(pager.visible?2:0,toolIndex+1)
   else if(cmd===inputManager.confirm){if(toolIndex===0)storeTool();else catalog.changeStorePage(toolIndex===1?-1:1)}
   return true
  }
  if(area==="header"){
   if(cmd===inputManager.navigateUp && !deviceApps)area="tools"
   if(cmd===inputManager.navigateLeft)headerIndex=Math.max(0,headerIndex-1)
   if(cmd===inputManager.navigateRight)headerIndex=Math.min(3,headerIndex+1)
   if(cmd===inputManager.navigateDown && filtered.length)area="grid"
   if(cmd===inputManager.confirm){if(headerIndex===0)beginSearch();else if(headerIndex===1){deviceApps=true;area="grid"}else choosePlatform(headerIndex-2)}
   return true
  }
  if(area==="actions"){
   if(cmd===inputManager.navigateLeft)area="grid"
   if(cmd===inputManager.navigateUp)actionIndex=Math.max(0,actionIndex-1)
   if(cmd===inputManager.navigateDown || cmd===inputManager.navigateRight)actionIndex=Math.min(actionItems.length-1,actionIndex+1)
   if(cmd===inputManager.confirm && actionItems[actionIndex])request(actionItems[actionIndex].op)
   return true
  }
  const i=filtered.findIndex(e=>e.id===selectedId)
  if(cmd===inputManager.navigateUp && i<columns){area="header";headerIndex=platformIndex+2;return true}
  if(cmd===inputManager.navigateRight && (i%columns===columns-1 || i===filtered.length-1)){area="actions";actionIndex=0;return true}
  const d=cmd===inputManager.navigateLeft?-1:cmd===inputManager.navigateRight?1:cmd===inputManager.navigateUp?-columns:cmd===inputManager.navigateDown?columns:0
  if(d && filtered.length){const n=Math.max(0,Math.min(filtered.length-1,i+d));selectedId=filtered[n].id;grid.positionViewAtIndex(n,GridView.Contain)}
  if(cmd===inputManager.confirm){area="actions";actionIndex=0;request(selected && selected.installed?"open":"install")}
  return true
 }
 Rectangle {anchors.fill:parent;color:Qt.darker(root.surface,1.3)}
 Column {
  anchors.fill:parent;anchors.margins:root.gap;spacing:root.gap
  Row {
   width:parent.width;height:90*root.typeScale;spacing:root.gap
   Column {width:parent.width-controls.width-root.gap;spacing:6
    Text {textFormat:Text.PlainText;text:qsTr("App Store");color:root.ink;font.pixelSize:36*root.typeScale;font.weight:Font.DemiBold}
    Text {textFormat:Text.PlainText;text:qsTr("Find your next favourite.");color:root.ink;opacity:0.65;font.pixelSize:18*root.typeScale}
   }
   Row {id:controls;spacing:8;anchors.verticalCenter:parent.verticalCenter
    Repeater {model:[qsTr("Search"),qsTr("Device apps")].concat(root.platforms.map(p=>p.label))
     delegate:Rectangle {required property int index;required property string modelData;objectName:"store-header-"+index
      width:label.implicitWidth+28;height:50*root.typeScale;radius:root.corners;color:index>=2 && !root.deviceApps && root.platformIndex===index-2?Qt.lighter(root.surface,1.7):root.surface
      border.width:root.area==="header" && root.headerIndex===index?3:0;border.color:root.focusColor
      Text {textFormat:Text.PlainText;id:label;anchors.centerIn:parent;text:modelData;color:root.ink;font.pixelSize:18*root.typeScale;font.weight:index>=2?Font.DemiBold:Font.Normal}
      MouseArea {anchors.fill:parent;onClicked:{root.area="header";root.headerIndex=index;root.handleCommand(inputManager.confirm)}}
     }
    }
   }
  }
  Row {width:parent.width;height:50*root.typeScale;spacing:root.gap
   Text {textFormat:Text.PlainText;width:Math.max(0,parent.width-storeToolButton.width-(pager.visible?pager.width+root.gap:0)-root.gap);elide:Text.ElideRight;anchors.verticalCenter:parent.verticalCenter;text:(root.deviceApps?qsTr("Device apps"):root.platforms[root.platformIndex]?root.platforms[root.platformIndex].label+qsTr(" apps"):"")+"  ·  "+root.filtered.length;color:root.ink;font.pixelSize:23*root.typeScale;font.weight:Font.DemiBold}
   Rectangle {id:storeToolButton;objectName:"storeTool";visible:!root.deviceApps;width:toolLabel.implicitWidth+28;height:parent.height;radius:root.corners;color:root.surface;border.width:root.area==="tools" && root.toolIndex===0?3:1;border.color:root.focusColor
    Text {id:toolLabel;textFormat:Text.PlainText;anchors.centerIn:parent;text:root.platforms[root.platformIndex] && root.platforms[root.platformIndex].type==="web"?qsTr("Add website"):qsTr("F-Droid search: ")+(root.catalog.onlineStoreSearch?qsTr("On"):qsTr("Off"));color:root.ink;font.pixelSize:18*root.typeScale}
    MouseArea {anchors.fill:parent;onClicked:{root.area="tools";root.toolIndex=0;root.storeTool()}}
   }
   Row {id:pager;visible:!root.deviceApps && root.catalog.onlineStoreSearch && root.platforms[root.platformIndex] && root.platforms[root.platformIndex].type==="android";height:parent.height;spacing:root.gap
    Repeater {model:[qsTr("Previous"),qsTr("Next")]
     delegate:Rectangle {required property int index;required property string modelData;objectName:"store-page-"+index
      readonly property bool available:index===0?!!root.catalog.hasPreviousStorePage:!!root.catalog.hasNextStorePage
      width:pageLabel.implicitWidth+28;height:pager.height;radius:root.corners;color:root.surface;opacity:available?1:0.5;border.width:root.area==="tools" && root.toolIndex===index+1?3:1;border.color:root.focusColor
      Text {id:pageLabel;textFormat:Text.PlainText;anchors.centerIn:parent;text:modelData;color:root.ink;font.pixelSize:18*root.typeScale}
      MouseArea {anchors.fill:parent;enabled:parent.available;onClicked:{root.area="tools";root.toolIndex=index+1;root.catalog.changeStorePage(index===0?-1:1)}}
     }
    }
   }
  }
  Row {
   width:parent.width;height:parent.height-y-54;spacing:root.gap
   GridView {id:grid;objectName:"storeGrid";width:(parent.width-root.gap)*(1-(root.options.detailRatio || 0.42));height:parent.height;clip:true;model:root.filtered;cellWidth:width/root.columns;cellHeight:(root.options.cardHeight || 142)+root.gap
    delegate:Rectangle {required property var modelData;width:grid.cellWidth-root.gap;height:grid.cellHeight-root.gap;radius:root.corners;color:root.surface
     border.width:root.selectedId===modelData.id && root.area==="grid"?3:1;border.color:root.selectedId===modelData.id?root.focusColor:Qt.lighter(root.surface,1.3)
     Row {anchors.fill:parent;anchors.margins:root.gap;spacing:14
      Rectangle {width:52;height:52;radius:root.corners;color:modelData.accent
       Text {textFormat:Text.PlainText;anchors.centerIn:parent;text:modelData.symbol || "+";color:"white";font.pixelSize:28;font.weight:Font.DemiBold}}
      Column {width:parent.width-66;spacing:8
       Text {textFormat:Text.PlainText;width:parent.width;text:modelData.name;color:root.ink;font.pixelSize:21*root.typeScale;font.weight:Font.DemiBold;elide:Text.ElideRight}
       Text {textFormat:Text.PlainText;width:parent.width;text:modelData.category;color:root.ink;opacity:0.65;font.pixelSize:16*root.typeScale;elide:Text.ElideRight}
       Text {textFormat:Text.PlainText;text:modelData.updateAvailable?qsTr("Update available"):modelData.installed?qsTr("Installed"):qsTr("Add to Isolde");color:root.ink;opacity:0.7;font.pixelSize:15*root.typeScale}
      }
     }
     MouseArea {anchors.fill:parent;onClicked:{root.selectedId=modelData.id;root.area="actions";root.actionIndex=0}}
    }
    Text {textFormat:Text.PlainText;anchors.centerIn:parent;width:parent.width*0.8;visible:!root.filtered.length;text:qsTr("No apps found. Try another search or platform.");wrapMode:Text.WordWrap;color:root.ink;font.pixelSize:22*root.typeScale}
   }
   Rectangle {width:parent.width-grid.width-root.gap;height:parent.height;radius:root.corners;color:root.surface
    Flickable {id:details;objectName:"storeDetails";anchors.fill:parent;anchors.margins:root.gap;contentWidth:width;contentHeight:detailContent.implicitHeight;clip:true;visible:!!root.selected
    Column {id:detailContent;width:details.width;spacing:root.gap
     Text {textFormat:Text.PlainText;width:parent.width;text:root.selected?root.selected.name:"";color:root.ink;font.pixelSize:32*root.typeScale;font.weight:Font.DemiBold;wrapMode:Text.WordWrap}
     Text {textFormat:Text.PlainText;width:parent.width;text:root.selected?root.selected.publisher:"";color:root.ink;opacity:0.65;font.pixelSize:17*root.typeScale;wrapMode:Text.WordWrap}
     Text {textFormat:Text.PlainText;width:parent.width;visible:!!text;text:root.selected?root.selected.provenance || "":"";color:root.ink;font.pixelSize:17*root.typeScale;wrapMode:Text.WordWrap}
     Text {textFormat:Text.PlainText;width:parent.width;visible:!!text;text:root.selected?root.selected.compatibilityReason || "":"";color:root.ink;font.pixelSize:17*root.typeScale;wrapMode:Text.WordWrap}
     Text {textFormat:Text.PlainText;width:parent.width;text:root.selected?root.selected.description:"";color:root.ink;font.pixelSize:20*root.typeScale;wrapMode:Text.WordWrap;maximumLineCount:5;elide:Text.ElideRight}
     Text {textFormat:Text.PlainText;width:parent.width;text:root.selected && root.selected.installed?qsTr("Added to your current profile."):qsTr("Available to add to your profile.");color:root.ink;opacity:0.65;font.pixelSize:16*root.typeScale;wrapMode:Text.WordWrap}
     Repeater {id:actions;model:root.actionItems
      delegate:Rectangle {required property int index;required property var modelData;objectName:"store-action-"+index
       width:parent.width;height:54*root.typeScale;radius:root.corners;color:index===0?Qt.lighter(root.surface,1.8):root.surface
       border.width:root.area==="actions" && root.actionIndex===index?3:1;border.color:root.area==="actions" && root.actionIndex===index?root.focusColor:Qt.lighter(root.surface,1.4);opacity:root.busy || (root.selected && root.selected.compatible===false)?0.45:1
       Text {textFormat:Text.PlainText;anchors.centerIn:parent;text:modelData.label;color:root.ink;font.pixelSize:20*root.typeScale;font.weight:Font.DemiBold}
       MouseArea {anchors.fill:parent;enabled:!root.busy;onClicked:root.request(modelData.op)}
      }
     }
    }
    }
   }
  }
 }
 Text {textFormat:Text.PlainText;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:root.gap;text:root.busy?root.operationName:root.catalog.errorString || root.catalog.operationStatus || root.catalog.storeStatus || qsTr("D-pad Browse · OK Select · Search Find an app · Back Return");color:root.ink;font.pixelSize:17*root.typeScale;elide:Text.ElideRight}
 Rectangle {anchors.fill:parent;visible:root.addWebOpen;z:5;color:Qt.rgba(0,0,0,0.7)
  MouseArea {anchors.fill:parent}
  Rectangle {anchors.horizontalCenter:parent.horizontalCenter;y:root.searchOpen?root.gap:(parent.height-height)/2;width:parent.width*0.76;height:webForm.implicitHeight+root.gap*2;radius:root.corners;color:root.surface
   Column {id:webForm;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:root.gap;spacing:12
    Text {textFormat:Text.PlainText;width:parent.width;text:qsTr("Add a web app");color:root.ink;font.pixelSize:28*root.typeScale;font.weight:Font.DemiBold}
    Repeater {model:[qsTr("Name: ")+(root.webName || qsTr("Choose a name")),qsTr("Address: ")+(root.webAddress || "https://"),qsTr("Browser: ")+(root.webBackend==="chromium"?qsTr("Chromium · remote controls"):qsTr("Embedded · remote controls")),qsTr("Cancel"),qsTr("Add app")]
     delegate:Rectangle {required property int index;required property string modelData;objectName:"web-app-field-"+index;width:webForm.width;height:50*root.typeScale;radius:root.corners;color:root.surface;border.width:root.webField===index?3:1;border.color:root.focusColor
      Text {textFormat:Text.PlainText;anchors.fill:parent;anchors.margins:12;verticalAlignment:Text.AlignVCenter;text:modelData;color:root.ink;font.pixelSize:19*root.typeScale;elide:Text.ElideMiddle}
      MouseArea {anchors.fill:parent;onClicked:{root.webField=index;root.handleCommand(inputManager.confirm)}}
     }
    }
    Text {textFormat:Text.PlainText;width:parent.width;text:root.catalog.errorString || qsTr("Add to this profile. Opening the app connects to its website. Chromium remote controls use a separate private sign-in.");color:root.ink;font.pixelSize:17*root.typeScale;wrapMode:Text.WordWrap}
   }
  }
 }
 Rectangle {anchors.fill:parent;visible:!!root.confirmation;z:5;color:Qt.rgba(0,0,0,0.7)
  MouseArea {anchors.fill:parent}
  Rectangle {anchors.centerIn:parent;width:parent.width*0.64;height:body.implicitHeight+root.gap*2;radius:root.corners;color:root.surface
   Column {id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:root.gap;spacing:root.gap
    Text {textFormat:Text.PlainText;width:parent.width;text:root.confirmationTitle();color:root.ink;font.pixelSize:28*root.typeScale;font.weight:Font.DemiBold;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight}
    Text {textFormat:Text.PlainText;width:parent.width;text:root.confirmationMessage();color:root.ink;font.pixelSize:20*root.typeScale;wrapMode:Text.WordWrap}
    Text {textFormat:Text.PlainText;width:parent.width;visible:(root.confirmation==="install" || root.confirmation==="update") && !!text;text:root.confirmationApp?(root.confirmation==="update"?root.confirmationApp.updateConsent:root.confirmationApp.consentText) || "":"";color:root.ink;font.pixelSize:17*root.typeScale;wrapMode:Text.WordWrap;maximumLineCount:5;elide:Text.ElideRight}
    Row {width:parent.width;spacing:root.gap
     Repeater {id:confirmationButtons;model:root.confirmation==="setup"?[qsTr("Cancel"),root.setupSoftwareRendering?qsTr("Software renderer"):qsTr("Keep / default renderer"),qsTr("Set up")]:[qsTr("Cancel"),root.confirmation==="subscription"?qsTr("Confirm"):root.confirmation==="update"?qsTr("Update"):root.confirmation==="install"?qsTr("Install"):qsTr("Uninstall")]
      delegate:Rectangle {required property int index;required property string modelData;width:(body.width-root.gap*(confirmationButtons.count-1))/confirmationButtons.count;height:72*root.typeScale;radius:root.corners;color:root.surface;border.width:root.confirmIndex===index?3:1;border.color:root.focusColor
       Text {textFormat:Text.PlainText;anchors.fill:parent;anchors.margins:8;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;wrapMode:Text.WordWrap;text:modelData;color:root.ink;font.pixelSize:20*root.typeScale}
       MouseArea {anchors.fill:parent;onClicked:{root.confirmIndex=index;root.handleCommand(inputManager.confirm)}}
      }
     }
    }
   }
  }
 }
 Rectangle {anchors.fill:parent;visible:root.toolsMenuOpen;z:7;color:Qt.rgba(0,0,0,0.7)
  MouseArea {anchors.fill:parent}
  Rectangle {anchors.centerIn:parent;width:parent.width*0.64;height:storeMenu.implicitHeight+root.gap*2;radius:root.corners;color:root.surface
   Column {id:storeMenu;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:root.gap;spacing:root.gap
    Text {textFormat:Text.PlainText;text:qsTr("Store actions");color:root.ink;font.pixelSize:28*root.typeScale}
    Repeater {model:root.toolsItems
     delegate:Rectangle {required property int index;required property string modelData;width:storeMenu.width;height:50*root.typeScale;radius:root.corners;color:root.surface;border.color:root.focusColor;border.width:root.toolsMenuIndex===index?3:1;opacity:root.toolAvailable(index)?1:0.5
      Text {textFormat:Text.PlainText;anchors.fill:parent;anchors.margins:10;verticalAlignment:Text.AlignVCenter;text:modelData;color:root.ink;font.pixelSize:20*root.typeScale;elide:Text.ElideRight}
      MouseArea {anchors.fill:parent;onClicked:{root.toolsMenuIndex=index;root.chooseTool(index)}}
     }
    }
   }
  }
 }
 Rectangle {anchors.fill:parent;visible:root.busy;z:8;color:Qt.rgba(0,0,0,0.7)
  MouseArea {anchors.fill:parent}
  Rectangle {anchors.centerIn:parent;width:parent.width*0.64;height:progressBody.implicitHeight+root.gap*2;radius:root.corners;color:root.surface
   Column {id:progressBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:root.gap;spacing:root.gap
    Text {textFormat:Text.PlainText;width:parent.width;text:root.pendingOperation==="setup"?qsTr("Android setup"):root.operationName;color:root.ink;font.pixelSize:28*root.typeScale;wrapMode:Text.WordWrap}
    Text {textFormat:Text.PlainText;width:parent.width;text:root.catalog.operationStatus || qsTr("Please wait...");color:root.ink;font.pixelSize:20*root.typeScale;wrapMode:Text.WordWrap}
    Text {textFormat:Text.PlainText;width:parent.width;text:root.catalog.downloadProgress || "";visible:!!text;color:root.ink;font.pixelSize:20*root.typeScale}
    Rectangle {width:parent.width;height:54*root.typeScale;radius:root.corners;color:root.surface;border.width:3;border.color:root.focusColor;opacity:root.catalog.canCancelOperation?1:0.5
     Text {textFormat:Text.PlainText;anchors.centerIn:parent;text:root.catalog.canCancelOperation?qsTr("Cancel operation · OK / Back"):qsTr("Finishing safely...");color:root.ink;font.pixelSize:20*root.typeScale}
     MouseArea {anchors.fill:parent;enabled:!!root.catalog.canCancelOperation;onClicked:root.catalog.cancelOperation()}
    }
   }
  }
 }
 OnScreenKeyboard {id:keyboard;objectName:"storeKeyboard";appearance:root.appearance;visible:root.searchOpen;z:6;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:root.gap;height:Math.min(400,parent.height-root.gap*2)
  onAccepted:text=>{root.searchOpen=false;if(root.keyboardPurpose==="webName")root.webName=text;else if(root.keyboardPurpose==="webAddress")root.webAddress=text;else{root.searchText=text;root.catalog.searchStore(text);root.area="grid"}}
  onCanceled:root.searchOpen=false
 }
}
