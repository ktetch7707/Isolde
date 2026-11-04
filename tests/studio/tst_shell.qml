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
        function openView(kind,view,query){requests++}
        function invalidate(reason){invalidations++}
    }
    Component {id:searchComponent;App.CommandCenter {width:1280;height:720;apps:[];packages:[];movieSearchClient:client;showSearchClient:client;episodeSearchClient:client}}
    Component {id:keyboardComponent;App.OnScreenKeyboard {width:1000;height:400}}
    Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
    TestCase {
        name:"ShellAppearance";when:windowShown
        property var editor:null
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/);client.requests=0;client.invalidations=0}
        function cleanup(){if(editor){editor.close();editor=null}}
        function settle(){let done=false;Qt.callLater(()=>Qt.callLater(()=>{done=true}));tryVerify(()=>done)}
        function palette(light){return light?{surfaceColor:"#ffffff",selectedColor:"#000000",textColor:"#000000",mutedColor:"#333333",focusColor:"#000000",selectedTextColor:"#ffffff",selectedFocusColor:"#ffffff",inputColor:"#eeeeee",inputTextColor:"#000000",highContrast:true,motionEnabled:false}:{surfaceColor:"#202126",selectedColor:"#353039",textColor:"#f5f1f3",mutedColor:"#cccccc",focusColor:"#c994a5",inputColor:"#353039",inputTextColor:"#ffffff"}}
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
        function test_shell_inspector_source_override_reset_and_cancel_are_private(){
            verify(studioFixture.reset(false,false));const disk=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,parent);verify(editor!==null);editor.show("","cell-0");settle()
            editor.openInspector(true);const inspector=findChild(editor,"studioInspector");verify(inspector!==null)
            inspector.tab=inspector.tabs.indexOf("Shell");inspector.view="fields";inspector.shellSurface="settings"
            const fields=homeLayout.studioOptions.inspector.pageFields.filter(f=>f.scope==="surfaceTheme")
            const mode=fields.find(f=>f.key==="mode"),text=fields.find(f=>f.key==="textColor")
            const initial=JSON.stringify(editor.session.document),live=JSON.stringify(homeLayout.surfaceAppearance("settings",""))
            verify(inspector.set(mode,"custom",false));settle();compare(inspector.shellPolicy.mode,"custom")
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
