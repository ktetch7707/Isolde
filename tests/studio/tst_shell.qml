import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    QtObject {
        id:client
        property int requests:0
        property int invalidations:0
        property bool busy:false
        property bool connected:true
        property var items:[]
        property int pageStart:0
        property int totalMovies:0
        signal changed()
        function openView(kind,view,query){requests++}
        function invalidate(reason){invalidations++}
    }
    Component {id:searchComponent;App.CommandCenter {width:1280;height:720;apps:[];packages:[];movieSearchClient:client;showSearchClient:client;episodeSearchClient:client}}
    Component {id:keyboardComponent;App.OnScreenKeyboard {width:1000;height:400}}
    Component {id:displayComponent;App.DisplaySettingsView {width:1000;height:700}}
    Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
    QtObject {
        id: networkSettings
        property bool wifiEnabled: false
        property var devices: []
        property var wifiNetworks: []
        property var savedConnections: []
        property var connectionDetails: []
        property string summary: "Isolated network boundary"
        property string connectivity: "Offline"
        property string errorString: ""
    }
    QtObject {
        id: themeCatalog
        property var storeOptions: ({platforms:[{type:"web",label:"Web apps"}],columns:2,spacing:20,cardHeight:142})
        property bool busy: false
        property bool onlineStoreSearch: false
        property string errorString: ""
        property string storeSourceName: "Isolated catalog boundary"
        signal packageInstalled(string appId)
    }
    Component {id:networkComponent;App.NetworkSettingsView {
        width:1280;height:720
        Keys.onPressed: event => {const command=inputManager.commandForKey(event.key);if(command!==inputManager.none){handleCommand(command);event.accepted=true}}
    }}
    QtObject {id:galleryRegistry;property var apps:[{id:"one",name:"One",type:"web",symbol:"1"},{id:"two",name:"Two",type:"web",symbol:"2"},{id:"three",name:"Three",type:"web",symbol:"3"}]}
    Component {id:galleryComponent;App.PageAppCarousel {width:1000;height:250;registry:galleryRegistry;style:({layout:{visibleColumns:2}})}}
    Component {id:preferencesComponent;App.StoreSettingsView {width:800;height:600;catalog:themeCatalog}}
    Component {id:storeComponent;App.AppStoreView {width:1280;height:720;catalog:themeCatalog;entries:[{id:"theme-app",name:"Theme fixture",type:"web",storeType:"web",category:"Local",description:"Appearance changes do not operate the app catalog.",publisher:"Fixture",accent:"#334455",symbol:"F",compatible:true,installed:false}]}}
    QtObject {
        id: audioSettings
        property int volume: 50
        property bool muted: false
        property bool mixerAvailable: true
        property var outputs: []
        property string startupPolicy: "remember"
        property int startupVolume: 50
        property int tvReferenceVolume: 50
        property string volumeMode: "isolde"
        property int volumeStep: 5
        property string outputStatus: "Isolated output"
        property bool testPlaying: false
        property string testStatus: ""
        property string backendStatus: "Isolated audio"
        function stopTestSound() {}
    }
    QtObject {
        id: ambientSettings
        property bool enabled: true
        property int timeoutMinutes: 5
        property string mode: "slideshow"
        property int slideshowSeconds: 15
        property int photoCount: 3
        property string photoDirectory: "/isolated/photos"
        property string imageFit: "fill"
        property bool shuffle: false
        property bool showClock: true
        property bool showDate: true
        property bool showSystemHint: true
        property string errorString: ""
        property var pageIds: []
        property var photoUrls: []
        function setPageIds(ids){pageIds=ids.slice();return true}
    }
    Component {id:audioComponent;App.AudioSettingsView {width:800;height:600}}
    Component {id:ambientComponent;App.AmbientSettingsView {width:800;height:600}}
    Component {id:ambientScreenComponent;App.AmbientScreen {width:1280;height:720;settings:ambientSettings}}
    TestCase {
        name:"ShellAppearance";when:windowShown
        property var editor:null
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/);client.requests=0;client.invalidations=0}
        function cleanup(){if(editor){editor.close();editor=null}}
        function settle(){let done=false;Qt.callLater(()=>Qt.callLater(()=>{done=true}));tryVerify(()=>done)}
        function palette(light){return light?{surfaceColor:"#ffffff",selectedColor:"#000000",textColor:"#000000",mutedColor:"#333333",focusColor:"#000000",selectedTextColor:"#ffffff",selectedFocusColor:"#ffffff",inputColor:"#eeeeee",inputTextColor:"#000000",highContrast:true,motionEnabled:false}:{surfaceColor:"#202126",selectedColor:"#353039",textColor:"#f5f1f3",mutedColor:"#cccccc",focusColor:"#c994a5",inputColor:"#353039",inputTextColor:"#ffffff"}}
        function test_settings_values_remain_readable_data() {return test_nested_surface_roles_data()}
        function test_settings_values_remain_readable(data) {
            const tokens=studioFixture.surfacePalette(data.raw)
            const audio=createTemporaryObject(audioComponent,parent,{appearance:tokens})
            verify(audio!==null);settle()
            const value=findChild(audio,"audioVolumeValue"),bar=findChild(audio,"audioVolumeBar"),fill=findChild(audio,"audioVolumeFill")
            verify(value!==null && bar!==null && fill!==null)
            compare(value.text,"50%");compare(value.color,tokens.backgroundTextColor)
            compare(fill.color,tokens.inputTextColor);compare(fill.width,bar.width/2)
            verify(value.y+value.height<=bar.y,"Volume text must not overlap either progress color")
            audio.appearance=studioFixture.surfacePalette(test_nested_surface_roles_data()[0].raw);settle()
            compare(value.text,"50%");compare(audio.selectedIndex,0);compare(audioSettings.volume,50)
            audio.appearance=tokens;settle()
            if(studioFixture.artifacts)grabImage(audio).save(studioFixture.artifacts+"/studio-test-shell-audio-"+data.tag+".png")
            audio.visible=false
            const ambient=createTemporaryObject(ambientComponent,parent,{appearance:tokens})
            verify(ambient!==null);settle()
            const selected=findChild(ambient,"ambientValue-0"),ordinary=findChild(ambient,"ambientValue-1")
            verify(selected!==null && ordinary!==null)
            compare(selected.text,"On");compare(selected.color,tokens.selectedTextColor)
            compare(ordinary.color,tokens.textColor)
            ambient.selectedIndex=1;settle()
            compare(selected.color,tokens.textColor);compare(ordinary.color,tokens.selectedTextColor)
            compare(ambientSettings.timeoutMinutes,5);verify(ambientSettings.enabled)
            if(studioFixture.artifacts)grabImage(ambient).save(studioFixture.artifacts+"/studio-test-shell-ambient-"+data.tag+".png")
        }
        function test_network_keyboard_owns_remote_focus_and_cancels_without_reopening() {
            const network=createTemporaryObject(networkComponent,parent,{appearance:studioFixture.surfacePalette({}),selectedIndex:2})
            verify(network!==null)
            network.handleCommand(inputManager.confirm);settle()
            const keyboard=findChild(network,"networkTvKeyboard"),ssid=findChild(network,"networkSsidField")
            verify(keyboard!==null && ssid!==null)
            verify(network.keyboardOpen);verify(network.activeFocus);verify(!ssid.activeFocus)
            const original=keyboard.selectedIndex
            keyClick(Qt.Key_Right);compare(keyboard.selectedIndex,original+1)
            keyClick(Qt.Key_Escape);settle()
            verify(!network.keyboardOpen);verify(!network.hiddenNetworkPrompt);verify(!network.passwordPrompt)
            verify(network.activeFocus);compare(network.selectedIndex,2);compare(ssid.text,"")
            keyClick(Qt.Key_Left);compare(network.selectedIndex,1)
        }
        function test_nested_surface_roles_data() {
            return [
                {tag:"dark",raw:{surfaceColor:"#202126",selectedColor:"#353039",backgroundEnd:"#101014"}},
                {tag:"pastel",raw:{surfaceColor:"#f5edf3",selectedColor:"#e9d8e3",backgroundEnd:"#e9d8e3",textColor:"#241d2b",focusColor:"#6d3b69"}},
                {tag:"split-custom",raw:{surfaceColor:"#000000",selectedColor:"#ffffff",elevatedColor:"#ffffff",backgroundEnd:"#ffffff",inputColor:"#ffffff",textColor:"#ffffff",mutedColor:"#ffffff",focusColor:"#ffffff",highContrast:true,motionEnabled:false}}
            ]
        }
        function test_nested_surface_roles(data) {
            const tokens=studioFixture.surfacePalette(data.raw)
            const network=createTemporaryObject(networkComponent,parent,{appearance:tokens,passwordPrompt:true,selectedSsid:"Private fixture"})
            verify(network!==null)
            const password=findChild(network,"networkPasswordField")
            verify(password!==null);password.text="secret fixture"
            compare(password.color,tokens.inputTextColor);compare(password.placeholderTextColor,tokens.inputMutedColor)
            network.passwordPrompt=false;network.connectionPrompt=true;network.selectedConnectionAction=1
            settle()
            const action=findChild(network,"networkConnectionAction-1")
            verify(action!==null);compare(action.color,tokens.selectedColor);compare(action.border.color,tokens.selectedFocusColor)
            compare(action.children[0].color,tokens.selectedTextColor)
            network.appearance=studioFixture.surfacePalette(test_nested_surface_roles_data()[0].raw);settle()
            compare(password.text,"secret fixture");compare(network.selectedConnectionAction,1);verify(network.connectionPrompt)
            network.appearance=tokens;settle()
            if(studioFixture.artifacts)grabImage(network).save(studioFixture.artifacts+"/studio-test-shell-network-"+data.tag+".png")
            network.visible=false
            const preferences=createTemporaryObject(preferencesComponent,parent,{appearance:tokens})
            verify(preferences!==null);settle()
            const preference=findChild(preferences,"storePreferenceRow-0")
            verify(preference!==null);compare(preference.color,tokens.selectedColor);compare(preference.border.color,tokens.selectedFocusColor)
            preferences.visible=false
            const carousel=createTemporaryObject(galleryComponent,parent,{appearance:studioFixture.surfacePalette(test_nested_surface_roles_data()[0].raw),galleryAppearance:tokens,engaged:true,selectedId:"__more__"})
            verify(carousel!==null);carousel.handleCommand(inputManager.confirm);settle()
            const gallery=findChild(carousel,"appCarouselGallery")
            verify(gallery!==null);verify(gallery.opened);compare(gallery.background.color,tokens.surfaceColor)
            const galleryLabel=findChild(gallery.contentItem,"orbLabel"),galleryPill=findChild(gallery.contentItem,"orbLabelSurface")
            verify(galleryLabel!==null && galleryPill!==null);compare(galleryLabel.color,tokens.textColor);compare(galleryPill.color,tokens.surfaceColor);compare(gallery.background.border.color,tokens.focusColor)
            const selectedGalleryId=carousel.galleryId
            carousel.galleryAppearance=studioFixture.surfacePalette(test_nested_surface_roles_data()[0].raw);settle()
            compare(carousel.galleryId,selectedGalleryId);verify(gallery.opened)
            carousel.handleCommand(inputManager.back);verify(!gallery.opened);compare(carousel.selectedId,"__more__")
            carousel.visible=false
            const search=createTemporaryObject(searchComponent,parent,{appearance:tokens,apps:[{id:"theme-result",name:"Theme result",section:"Local",presentationMode:"native-tv",networkRequirement:"none",symbol:"F"}]})
            verify(search!==null);search.query="theme";search.open=true;settle()
            search.focusArea="results";search.selectedIndex=0;settle()
            const resultTitle=findChild(search,"searchResultTitle-0"),queryControl=findChild(search,"searchQueryControl")
            verify(resultTitle!==null && queryControl!==null)
            compare(resultTitle.color,tokens.selectedTextColor);compare(resultTitle.parent.parent.border.color,tokens.selectedFocusColor)
            compare(queryControl.border.color,tokens.inputFocusColor)
            search.open=false;search.visible=false
            const store=createTemporaryObject(storeComponent,parent,{appearance:tokens})
            verify(store!==null);store.choosePlatform(0);settle()
            compare(store.selectedId,"theme-app")
            const header=findChild(store,"store-header-2"),primary=findChild(store,"store-action-0")
            verify(header!==null && primary!==null)
            compare(header.color,tokens.selectedColor);compare(header.border.color,tokens.selectedFocusColor)
            compare(primary.color,tokens.selectedColor)
            store.area="actions";settle();compare(primary.border.color,tokens.selectedFocusColor)
            const id=store.selectedId,index=store.actionIndex
            store.appearance=studioFixture.surfacePalette(test_nested_surface_roles_data()[0].raw);settle()
            compare(store.selectedId,id);compare(store.actionIndex,index);compare(store.area,"actions")
            store.appearance=tokens;settle()
            if(studioFixture.artifacts)grabImage(store).save(studioFixture.artifacts+"/studio-test-shell-apps-"+data.tag+".png")
            store.visible=false
            const keyboard=createTemporaryObject(keyboardComponent,parent,{appearance:tokens,passwordMode:true,value:"secret fixture",selectedIndex:14})
            verify(keyboard!==null);compare(keyboard.value,"secret fixture");verify(keyboard.passwordMode)
            if(studioFixture.artifacts)grabImage(keyboard).save(studioFixture.artifacts+"/studio-test-shell-keyboard-"+data.tag+".png")
        }
        function test_search_theme_change_preserves_query_focus_keyboard_and_requests(){
            const search=createTemporaryObject(searchComponent,parent,{appearance:palette(false)})
            verify(search!==null);search.query="films";search.open=true
            tryCompare(client,"requests",3)
            search.focusArea="results";search.selectedIndex=0;search.keyboardOpen=true
            const requests=client.requests,invalidations=client.invalidations,query=search.query,index=search.selectedIndex
            search.appearance=palette(true);settle()
            compare(search.query,query);compare(search.selectedIndex,index);compare(search.focusArea,"results");verify(search.keyboardOpen)
            compare(client.requests,requests);compare(client.invalidations,invalidations)
            search.open=false
        }
        function test_search_long_fallback_symbols_stay_inside_the_icon_data(){return [{tag:"dark",light:false},{tag:"pastel",light:true}]}
        function test_search_long_fallback_symbols_stay_inside_the_icon(data){
            const symbol="GameCube <b>PlayStation</b>"
            const search=createTemporaryObject(searchComponent,parent,{appearance:palette(data.light),apps:[{id:"overflow",name:"Overflow app",section:"Local",presentationMode:"native-tv",networkRequirement:"none",symbol:symbol}]})
            verify(search!==null);search.query="overflow";search.open=true;settle()
            compare(search.results.length,1)
            const icon=findChild(search,"searchResultIcon-0"),glyph=findChild(search,"searchResultSymbol-0"),title=findChild(search,"searchResultTitle-0")
            verify(icon!==null && glyph!==null && title!==null);verify(icon.clip)
            compare(glyph.text,symbol);compare(glyph.textFormat,Text.PlainText)
            tryVerify(()=>glyph.width>0 && glyph.paintedWidth>0 && glyph.paintedWidth<=glyph.width+1,5000,"Fallback text must paint inside its icon bounds")
            verify(glyph.mapToItem(search,0,0).x+glyph.width<title.mapToItem(search,0,0).x)
            compare(title.text,"Overflow app");compare(search.query,"overflow");compare(search.selectedIndex,0)
            if(studioFixture.artifacts)grabImage(search).save(studioFixture.artifacts+"/studio-test-search-long-symbol-"+data.tag+".png")
            search.open=false
        }
        function test_keyboard_theme_change_preserves_password_and_cursor(){
            const keyboard=createTemporaryObject(keyboardComponent,parent,{appearance:palette(false),passwordMode:true,value:"private",selectedIndex:14,shift:true})
            verify(keyboard!==null);keyboard.appearance=palette(true);settle()
            compare(keyboard.value,"private");compare(keyboard.selectedIndex,14);verify(keyboard.passwordMode);verify(keyboard.shift)
            keyboard.handleCommand(inputManager.back);compare(keyboard.value,"private")
        }
        function test_keyboard_remote_selection_stays_visible_in_short_panel(){
            const keyboard=createTemporaryObject(keyboardComponent,parent,{width:480,height:300})
            verify(keyboard!==null)
            for(let i=0;i<keyboard.keys.length;i++)keyboard.handleCommand(inputManager.navigateRight)
            settle()
            const viewport=findChild(keyboard,"keyboardViewport")
            verify(viewport!==null);verify(viewport.contentY>0)
            const selected=findChild(keyboard,"keyboardKey-"+keyboard.selectedIndex)
            verify(selected!==null)
            const position=selected.mapToItem(viewport,0,0)
            verify(position.y>=-1 && position.y+selected.height<=viewport.height+1)
            const done=keyboard.keys.indexOf("Done")
            while(keyboard.selectedIndex>done)keyboard.handleCommand(inputManager.navigateLeft)
            settle()
            if(studioFixture.artifacts)grabImage(keyboard).save(studioFixture.artifacts+"/studio-test-keyboard-short-panel-done.png")
            keyboard.selectedIndex=0;settle();compare(viewport.contentY,0)
        }
        function test_display_contrast_changes_saved_profile_appearance(){
            verify(studioFixture.reset(false,false))
            const display=createTemporaryObject(displayComponent,parent)
            verify(display!==null)
            display.selectedIndex=3
            const original=!!homeLayout.homeAppearance.highContrast
            const disk=studioFixture.disk()
            display.handleCommand(inputManager.confirm)
            compare(!!homeLayout.homeAppearance.highContrast,!original)
            compare(display.errorString,"")
            verify(studioFixture.disk()!==disk)
            display.handleCommand(inputManager.confirm)
            compare(!!homeLayout.homeAppearance.highContrast,original)
        }
        function test_ambient_pages_use_shared_models_and_cancel_selection(){
            verify(studioFixture.reset(false,false))
            const first=homeLayout.createPage("First screen","blank"),second=homeLayout.createPage("Second screen","blank")
            verify(first.length>0 && second.length>0)
            ambientSettings.pageIds=[first,second];ambientSettings.mode="pages"
            const screen=createTemporaryObject(ambientScreenComponent,parent)
            verify(screen!==null);settle()
            compare(screen.currentPage,homeLayout.pageModel(first))
            screen.slideIndex=1;compare(screen.currentPage,homeLayout.pageModel(second))
            const settings=createTemporaryObject(ambientComponent,parent)
            verify(settings!==null);settings.selectedIndex=13;settings.handleCommand(inputManager.confirm)
            verify(settings.pagePickerOpen)
            settings.handleCommand(inputManager.confirm)
            compare(settings.draftPageIds.length,1);compare(ambientSettings.pageIds.length,2)
            settings.handleCommand(inputManager.back);verify(!settings.pagePickerOpen)
            compare(ambientSettings.pageIds.length,2)
            ambientSettings.pageIds=["removed-page"];settle();compare(screen.currentPage,null)
            ambientSettings.pageIds=[];ambientSettings.mode="slideshow"
        }
        function test_shell_inspector_source_override_reset_and_cancel_are_private(){
            verify(studioFixture.reset(false,false));const disk=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,parent);verify(editor!==null);editor.show("","cell-0");settle()
            editor.openInspector(true);const inspector=findChild(editor,"studioInspector");verify(inspector!==null)
            inspector.tab=inspector.tabs.indexOf("Shell");inspector.view="fields";inspector.shellSurface="settings"
            const fields=homeLayout.studioOptions.inspector.pageFields.filter(f=>f.scope==="surfaceTheme")
            const mode=fields.find(f=>f.key==="mode"),text=fields.find(f=>f.key==="textColor")
            const initial=JSON.stringify(editor.session.document),live=JSON.stringify(homeLayout.surfaceAppearance("settings",""))
            inspector.activateField(mode);compare(inspector.view,"choices")
            inspector.handleCommand(inputManager.navigateDown);inspector.handleCommand(inputManager.confirm)
            settle();compare(inspector.view,"fields");compare(inspector.shellPolicy.mode,"custom")
            compare(studioFixture.disk(),disk);compare(JSON.stringify(homeLayout.surfaceAppearance("settings","")),live)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),initial)
            inspector.activateField(text);verify(inspector.colorPicker!==null)
            inspector.colorPicker.choose("#ffffff");settle();editor.handleCommand(inputManager.back)
            compare(JSON.stringify(editor.session.document),initial);compare(studioFixture.disk(),disk)
            verify(inspector.set(mode,"home",false));verify(inspector.set(mode,null,true));settle()
            verify(!((editor.session.document.theme || {}).surfaceThemes || {}).settings)
            compare(studioFixture.disk(),disk)
        }
    }
}
