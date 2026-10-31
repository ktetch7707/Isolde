import QtQuick
import QtTest
import "../../qml" as App

Item {
    width: 1280; height: 720
    TestCase {
        id: tests
        name: "StudioColors"
        when: windowShown
        property var editor: null
        property string diskBaseline: ""
        Component { id: editorComponent; App.PageStudioView { width: 1280; height: 720; layoutModel: homeLayout } }
        function init() {
            failOnWarning(/Binding loop detected|ReferenceError|TypeError/)
        }
        function cleanup() {
            if (editor) {
                settle()
                editor.close()
                settle()
            }
            editor = null
        }
        function settle() {
            let settled = false
            Qt.callLater(() => Qt.callLater(() => { settled = true }))
            tryVerify(() => settled)
        }
        function givenPage(full, scrolling) {
            verify(studioFixture.reset(full || false, scrolling || false))
            diskBaseline = studioFixture.disk()
            editor = createTemporaryObject(editorComponent, tests.parent)
            verify(editor !== null)
            editor.show("", "cell-0")
            editor.toolbarFocus = false
            tryVerify(() => editor.current !== null)
        }
        function command(value) { editor.handleCommand(value) }
        function document() { return JSON.stringify(editor.session.document) }
        function scenario(name, body) {
            try { body() } catch (error) {
                if (editor && studioFixture.artifacts)
                    grabImage(editor).save(studioFixture.artifacts + "/studio-test-" + name + ".png")
                throw error
            }
        }
        function test_brightness_slider_remote_pointer_match_one_transaction(){
            givenPage(false,false);const before=document()
            editor.openInspector(true);const inspector=findChild(editor,"studioInspector"),field=homeLayout.studioOptions.inspector.pageFields.find(f=>f.key==="backgroundEnd")
            inspector.activateField(field);let picker=inspector.colorPicker;picker.choose("#ffffff");settle()
            const slider=findChild(picker,"studioColorBrightness");verify(slider!==null);verify(waitForRendering(slider));mouseClick(slider,slider.width/2,slider.height/2);settle()
            compare(picker.hex(picker.sample),"#808080");compare(document(),before);picker.control=4;command(inputManager.confirm);const pointer=document();compare(editor.session.changeCount,1)
            verify(editor.session.undo());compare(document(),before)
            inspector.activateField(field);picker=inspector.colorPicker;picker.choose("#ffffff");picker.control=2
            for(let i=0;i<10;++i)command(inputManager.navigateLeft)
            compare(picker.hex(picker.sample),"#808080");picker.control=4;command(inputManager.confirm);compare(document(),pointer);compare(editor.session.changeCount,1)
            verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_shared_color_fields_cancel_and_single_accept_data(){
            return [{tag:"light",palette:{backgroundStart:"#ffffff",backgroundEnd:"#eeeeee",surfaceColor:"#f5f5f5",textColor:"#202126"},page:false,scope:"presentation",key:"labelColor"},{tag:"pastel",palette:{backgroundStart:"#f6e5ed",backgroundEnd:"#ccd4ef",surfaceColor:"#efe6f4",textColor:"#514b67"},page:false,scope:"presentation",key:"labelColor"},{tag:"component",page:false,scope:"presentation",key:"labelColor"},{tag:"body",page:false,scope:"presentation",key:"bodyColor"},{tag:"gradient",page:false,scope:"presentation",key:"fillEndColor",kind:"shape"},{tag:"page",page:true,scope:"theme",key:"backgroundEnd"},{tag:"tint",page:true,scope:"background",key:"tintColor"},{tag:"header",page:true,scope:"headerStyle",key:"backgroundColor"},{tag:"chrome",page:true,scope:"studioAppearance",key:"textColor",editor:true}]
        }
        function test_shared_color_fields_cancel_and_single_accept(data){
            givenPage(false,false)
            if(data.palette){verify(editor.session.execute("appearance",{value:data.palette}));settle()}
            if(data.kind){verify(editor.session.execute("addResolved",{type:data.kind,placement:{x:1,y:0,width:2,height:1}}));settle();editor.selectedId=editor.items.find(i=>i.id!=="cell-0").id;verify(editor.session.execute("patch",{id:editor.selectedId,value:{presentation:{fillMode:"gradient"}}}));settle()}
            const before=document(),count=editor.session.changeCount;editor.openInspector(data.page)
            const inspector=findChild(editor,"studioInspector");inspector.editorMode=!!data.editor
            const fields=data.editor?homeLayout.studioOptions.inspector.studioFields:data.page?homeLayout.studioOptions.inspector.pageFields:homeLayout.studioOptions.inspector.fields
            const field=fields.find(f=>f.scope===data.scope && f.key===data.key);verify(!!field)
            inspector.activateField(field);const picker=inspector.colorPicker;verify(picker!==null);picker.choose(data.editor?"#ffffff":"#82bce8");settle()
            compare(document(),before);compare(studioFixture.disk(),diskBaseline);verify(inspector.contrast().indexOf("NaN")<0)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-color-field-"+data.tag+".png")
            command(inputManager.back);compare(document(),before)
            inspector.activateField(field);inspector.colorPicker.choose(data.editor?"#ffffff":"#82bce8");inspector.colorPicker.control=4;command(inputManager.confirm);settle()
            compare(editor.session.changeCount,count+1);compare(inspector.recentColors.length,1)
            verify(editor.session.undo());compare(document(),before);compare(inspector.recentColors.length,0);compare(studioFixture.disk(),diskBaseline)
        }
        function test_mixed_color_preview_cancel_reset_and_recent_reopen(){
            givenPage(false,false)
            verify(editor.session.execute("addResolved",{type:"text",placement:{x:1,y:0,width:2,height:1}}));settle()
            const second=editor.items.find(i=>i.id!=="cell-0").id
            verify(editor.session.execute("patch",{id:second,value:{presentation:{bodyColor:"#ffffff"}}}));settle()
            editor.openInspector(false);const inspector=findChild(editor,"studioInspector");inspector.selectedIds=["cell-0",second]
            const field=homeLayout.studioOptions.inspector.fields.find(f=>f.key==="bodyColor"),before=document(),count=editor.session.changeCount
            verify(inspector.mixed(field));inspector.activateField(field);verify(inspector.colorPicker.mixedOriginal)
            inspector.colorPicker.choose("#82bce8");settle();verify(inspector.colorPicker.mixedOriginal);command(inputManager.back);compare(document(),before)
            inspector.activateField(field);inspector.colorPicker.choose("#82bce8");inspector.colorPicker.control=4;command(inputManager.confirm);settle();compare(editor.session.changeCount,count+1)
            for(const id of inspector.selectedIds)compare(editor.items.find(i=>i.id===id).presentation.bodyColor,"#82bce8")
            const accepted=document();inspector.activateField(field);inspector.colorPicker.category=2;compare(inspector.colorPicker.swatches[0].value,"#82bce8");command(inputManager.back)
            verify(editor.session.undo());compare(document(),before)
            inspector.selectedIds=[second];verify(editor.session.execute("appearance",{value:{backgroundStart:"#ffffff",backgroundEnd:"#ffffff"}}));verify(editor.session.execute("patch",{id:second,value:{presentation:{surfaceColor:"#000000",surfaceOpacity:0,bodyColor:"#ffffff"}}}));settle();inspector.activeField=field;verify(inspector.contrast().indexOf("1.0:1")>=0)
            verify(editor.session.execute("patch",{id:second,value:{presentation:{bodyColor:"#000000"}}}));settle();verify(inspector.contrast().indexOf("21.0:1")>=0)
            verify(editor.session.execute("patch",{id:second,value:{presentation:{surfaceColor:"#ffffff",surfaceOpacity:1,fillMode:"solid",fillEndColor:"#000000"}}}));settle();verify(inspector.contrast().indexOf("21.0:1")>=0)
        }
    }
}
