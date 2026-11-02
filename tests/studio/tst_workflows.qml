import QtQuick
import QtTest
import "../../qml" as App

Item {
    width: 1280; height: 720
    TestCase {
        id: tests
        name: "StudioWorkflows"
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
        function test_proportional_size_cancel_refusal_and_pointer_remote_transaction() {
            givenPage(false,false)
            verify(editor.session.execute("addResolved",{type:"shape",placement:{x:1,y:0,width:1,height:1}}));settle();editor.selectedId=editor.items.find(i=>i.id!=="cell-0").id;settle()
            const before=document(),count=editor.session.changeCount
            editor.inspectorOperation("geometry");verify(editor.proportionalSize);editor.choice=6;command(inputManager.confirm)
            verify(editor.sizeAspectLocked);editor.choice=0;command(inputManager.navigateRight);compare(editor.candidate.width,2);compare(editor.candidate.height,2)
            compare(document(),before);command(inputManager.back);compare(document(),before)
            editor.inspectorOperation("geometry");editor.choice=6;command(inputManager.confirm)
            const slider=findChild(editor,"studioSize-width");verify(waitForRendering(slider));mouseClick(slider,slider.leftPadding+(slider.availableWidth)/3,slider.height/2)
            compare(editor.candidate.width,2);compare(editor.candidate.height,2);editor.choice=2;command(inputManager.confirm)
            const pointer=document();compare(editor.session.changeCount,count+1);verify(editor.current.presentation.sizeAspectLocked)
            verify(editor.session.undo());compare(document(),before)
            editor.inspectorOperation("geometry");editor.choice=6;command(inputManager.confirm);editor.choice=0;command(inputManager.navigateRight);editor.choice=2;command(inputManager.confirm)
            compare(document(),pointer);verify(editor.session.undo());compare(document(),before)
            editor.inspectorOperation("geometry");editor.choice=6;command(inputManager.confirm);editor.setDimension("width",4)
            verify(editor.sizeError.length>0);editor.choice=2;command(inputManager.confirm);compare(editor.mode,"size");compare(document(),before)
            command(inputManager.back);compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_size_role_labels_and_named_tools_are_reachable() {
            givenPage(false,false);editor.openInspector(false)
            const inspector=findChild(editor,"studioInspector");compare(inspector.labels()[2],"Size & layout");compare(inspector.labels()[3],"Action")
            inspector.selected=2;command(inputManager.confirm);compare(inspector.fields[0].label,"Outer size")
            command(inputManager.confirm);compare(editor.mode,"size");command(inputManager.back);compare(editor.mode,"inspector");compare(inspector.selected,0)
            command(inputManager.back);command(inputManager.back)
            const before=document();editor.activateMenu(editor.menuEntries.findIndex(e=>e.action==="editorSettings"));compare(editor.mode,"editorSettings")
            editor.choice=0;command(inputManager.confirm);compare(editor.mode,"view");editor.choice=0;command(inputManager.confirm);compare(document(),before)
            command(inputManager.back);compare(editor.mode,"editorSettings");compare(editor.choice,0);compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_hidden_header_layers_recovery_and_field_return() {
            givenPage(false,false)
            verify(editor.session.execute("pagePatch",{scope:"composition",value:{headerMode:"identity",identity:"Hidden title",showClock:false,headerControls:["edit"],header:{schemaVersion:1,showTitle:false}}}));settle()
            const before=document(),count=editor.session.changeCount
            editor.mode="layers";editor.choice=editor.dialogLabels().indexOf("Header");verify(editor.choice>=0);command(inputManager.confirm)
            const inspector=findChild(editor,"studioInspector");compare(inspector.tabs[inspector.tab],"Header");compare(inspector.view,"fields")
            inspector.selected=inspector.fields.findIndex(f=>f.key==="showTitle");verify(inspector.selected>=0);command(inputManager.confirm);settle()
            compare(editor.page.composition.header.showTitle,true);compare(editor.session.changeCount,count+1)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-header-recovered.png")
            command(inputManager.back);command(inputManager.back);compare(editor.mode,"canvas");compare(editor.headerSelection,"title")
            verify(editor.session.undo());compare(document(),before)
            editor.openHeader("settings");compare(inspector.fields[inspector.selected].key,"headerControls");command(inputManager.back);command(inputManager.back)
            compare(editor.headerSelection,"settings");compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_selection_palette_matrix_data(){return [{tag:"dark",surface:"#202126",ink:"#ffffff",focus:"#f0bbcb"},{tag:"light",surface:"#eeeeee",ink:"#202126",focus:"#71314f"},{tag:"pastel",surface:"#efe6f4",ink:"#514b67",focus:"#943f69"},{tag:"named-pastel",surface:"#efe6f4",ink:"#514b67",focus:"#943f69",named:true}]}
        function test_selection_palette_matrix(data){
            givenPage(false,false)
            if(data.named){const composition=editor.page.composition;verify(editor.session.execute("newPage",{name:"Named pastel",preset:"blank"}));const id=editor.session.document.pages[editor.session.document.pages.length-1].id;verify(editor.session.execute("pagePatch",{scope:"composition",value:composition},id));editor.pageId=id;editor.selectedId="";settle()}
            const palette={backgroundStart:data.surface,backgroundEnd:data.surface,surfaceColor:data.surface,textColor:data.ink,focusColor:data.focus,motionEnabled:false}
            verify(data.named?editor.session.execute("pagePatch",{scope:"theme",value:palette},editor.pageId):editor.session.execute("appearance",{value:palette}))
            verify(editor.session.execute("addResolved",{type:"shape",placement:{x:1,y:0,width:2,height:1}},editor.pageId));settle()
            const id=editor.items.find(i=>i.id!=="cell-0").id;verify(editor.session.execute("patch",{id:id,value:{presentation:{shape:"ellipse",surfaceOpacity:0,focusColor:data.focus,focusWidth:5,visibleLabel:"Decorative ellipse"}}},editor.pageId));editor.selectedId=id;settle()
            const card=findChild(editor,"placedCard-"+id);verify(card!==null);tryCompare(card,"selected",true)
            const surface=findChild(card,"cardFocus");verify(surface!==null);compare(surface.border.color,Qt.color(data.focus));compare(surface.border.width,5*card.unit)
            compare(findChild(editor,"studioSelectionTarget-"+id).border.width,0)
            const before=document();command(inputManager.confirm);compare(editor.mode,"context");compare(card.selected,true);settle()
            const context=findChild(editor,"studioContext"),preview=findChild(editor,"studioPreview"),r=preview.placementRectFor(editor.current.placement,"","")
            verify(context.compact || context.x+context.width<=preview.x+r.x || context.x>=preview.x+r.x+r.width || context.y+context.height<=preview.y+r.y || context.y>=preview.y+r.y+r.height)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-selection-"+data.tag+".png")
            command(inputManager.back);compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }        function test_header_region_placement_roundtrip_cancel_and_undo(){
            givenPage(false,false)
            const before=document(),preview=findChild(editor,"studioPreview"),header=findChild(preview,"pageHeader"),content=findChild(preview,"composedContent")
            verify(header!==null);verify(content!==null);verify(header.y<content.y)
            editor.openHeader("title");const inspector=findChild(editor,"studioInspector"),field=inspector.fields.find(f=>f.key==="position");verify(!!field)
            inspector.activateField(field);compare(inspector.view,"choices");inspector.selected=1;command(inputManager.back);compare(document(),before)
            inspector.activateField(field);command(inputManager.navigateDown);command(inputManager.confirm);settle();compare(editor.page.composition.header.position,"bottom")
            verify(header.y>=content.y+content.height);compare(editor.session.changeCount,1);compare(studioFixture.disk(),diskBaseline)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-header-bottom.png")
            verify(editor.session.undo());settle();compare(document(),before);verify(header.y<content.y)
        }    }
}