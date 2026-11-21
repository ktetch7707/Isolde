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
        // Protect the remote-only create and duplicate flow from opening a naming keyboard.
        function test_page_creation_suggests_unique_names_and_keeps_edits_private() {
            givenPage(false, false)
            const before = document()
            editor.activateEntry({action:"new"})
            compare(editor.mode, "pageStarters")
            command(inputManager.confirm)
            compare(editor.mode, "canvas")
            const first = editor.pageEntries.find(entry => entry.id === editor.pageId)
            compare(first.name, "New page")
            editor.activateEntry({action:"new"})
            command(inputManager.confirm)
            const second = editor.pageEntries.find(entry => entry.id === editor.pageId)
            compare(second.name, "New page 2")
            editor.mode = "pageContext"
            editor.choice = 3
            command(inputManager.confirm)
            compare(editor.mode, "pageContext")
            const copy = editor.pageEntries.find(entry => entry.id === editor.pageId)
            compare(copy.name, "New page 2 copy")
            compare(studioFixture.disk(), diskBaseline)
            verify(editor.session.undo())
            verify(editor.session.undo())
            verify(editor.session.undo())
            compare(document(), before)
        }
        // Each starter must be usable from the remote and remain private until Apply.
        function test_page_starters_cancel_create_and_undo() {
            givenPage(false, false)
            const before = document()
            editor.mode = "pages"
            editor.choice = editor.pageEntries.length
            command(inputManager.confirm)
            compare(editor.mode, "pageStarters")
            const starters = editor.pageStarters.slice(1)
            verify(starters.length >= 4)
            command(inputManager.back)
            compare(editor.mode, "pages")
            compare(document(), before)
            for (const starter of starters) {
                editor.activateEntry({action:"new"})
                const index = editor.pageStarters.findIndex(entry=>entry.id===starter.id)
                for (let step = 0; step < index; ++step) command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(editor.mode, "canvas", editor.message)
                const page = editor.session.document.pages.find(entry=>entry.id===editor.pageId)
                compare(page.name, starter.name)
                verify(page.layout.components.length > 0)
                compare(studioFixture.disk(), diskBaseline)
                verify(editor.session.undo())
                compare(document(), before)
            }
            editor.activateEntry({action:"new"})
            command(inputManager.navigateDown)
            command(inputManager.confirm)
            const id = editor.pageId
            const created = editor.session.document.pages.find(entry=>entry.id===id)
            verify(editor.session.execute("renamePage", {id:id,name:"Living Room"}))
            verify(editor.session.apply(), editor.session.errorString)
            verify(studioFixture.disk() !== diskBaseline)
            editor.close()
            editor.show(id, "")
            const reopened = editor.session.document.pages.find(entry=>entry.id===id)
            compare(reopened.name, "Living Room")
            compare(JSON.stringify(reopened.layout.components), JSON.stringify(created.layout.components))
        }
        // Verify channel names can be selected by remote while their stable IDs stay in the draft only.
        function test_guide_channel_picker_accepts_once_and_back_discards() {
            givenPage(false, false)
            verify(editor.command("addResolved", {type:"tv-guide",placement:{x:0,y:1,width:4,height:2}}), editor.session.errorString)
            editor.selectedId = editor.items.find(item => item.family === "tv-guide").id
            editor.openInspector(false)
            const inspector = findChild(editor, "studioInspector")
            inspector.guideChannels = [{id:"news",number:2,name:"News",groups:["Local"]},
                                       {id:"sports",number:8,name:"Sports",groups:["Live"]}]
            const field = homeLayout.studioOptions.inspector.fields.find(entry => entry.type === "guideChannels")
            const before = document()
            inspector.activateField(field)
            compare(inspector.labels()[2], "□ 2 · News")
            inspector.selected = 2
            command(inputManager.confirm)
            compare(document(), before)
            command(inputManager.back)
            compare(document(), before)
            inspector.activateField(field)
            inspector.selected = 3
            command(inputManager.confirm)
            inspector.selected = 1
            command(inputManager.confirm)
            compare(editor.current.presentation.guideChannels, "sports")
            compare(studioFixture.disk(), diskBaseline)
            verify(editor.session.undo())
            compare(document(), before)
        }
        // Exercise the live renderer with two draft pages and verify that an embedded shortcut still dispatches.
        function test_page_panel_switches_and_dispatches_embedded_action() {
            givenPage(false, false)
            for (const name of ["First panel page", "Second panel page"]) {
                verify(editor.session.execute("newPage", {name:name}), editor.session.errorString)
                const id = editor.session.document.pages.slice(-1)[0].id
                verify(editor.session.execute("addResolved", {type:"button", placement:{x:0,y:0,width:1,height:1},
                    configuration:{name:name,action:{type:"open-apps"}}}, id), editor.session.errorString)
            }
            const pages = editor.session.document.pages
            const secondPageId = pages.find(page=>page.name==="Second panel page").id
            verify(editor.command("addResolved", {type:"page-panel",placement:{x:1,y:0,width:3,height:3}}), editor.session.errorString)
            const panel = editor.items.find(item=>item.family==="page-panel")
            verify(!!panel)
            editor.selectedId=panel.id
            editor.openInspector(false)
            const inspector=findChild(editor,"studioInspector")
            const field=inspector.simpleFields.find(entry=>entry.type==="panelPage")
            verify(!!field)
            inspector.activateField(field)
            compare(inspector.choices[0].name,"First panel page")
            inspector.activate()
            verify(editor.command("addResolved", {type:"button",placement:{x:0,y:1,width:1,height:1},
                configuration:{name:"Switch panel"}}), editor.session.errorString)
            const button=editor.items.find(item=>item.name==="Switch panel")
            editor.selectedId=button.id
            editor.openInspector(false)
            inspector.activateField(inspector.simpleFields.find(entry=>entry.type==="action"))
            inspector.chooseAction({localPanel:true})
            compare(inspector.choices[0].name,panel.name)
            inspector.activate()
            inspector.selected=1
            inspector.activate()
            editor.mode = "preview"
            const host = findChild(editor,"studioPreview")
            tryVerify(()=>!!findChild(host,"embeddedPage-"+panel.id))
            const child = findChild(host,"embeddedPage-"+panel.id)
            compare(child.items[0].name, "First panel page")
            host.restoreFocus(button.id)
            host.handleCommand(inputManager.confirm)
            tryVerify(()=>child.items.length && child.items[0].name==="Second panel page")
            let invoked
            host.actionActivated.connect((action,origin)=>{invoked={action:action,origin:origin}})
            host.restoreFocus(panel.id)
            host.handleCommand(inputManager.confirm)
            verify(host.widgetActive)
            child.restoreFocus(child.items[0].id)
            host.handleCommand(inputManager.confirm)
            verify(!!invoked)
            compare(invoked.origin, panel.id)
            compare(invoked.action.actions[0].type, "open-apps")
            host.handleCommand(inputManager.back)
            verify(!host.widgetActive)
            host.restoreFocus(panel.id)
            tryVerify(()=>host.widgetActive)
            compare(child.focusedItemId,child.items[0].id)
            verify(!editor.session.execute("removePage",{id:pages[0].id}))
            verify(editor.session.errorString.includes("incoming links"))
            verify(editor.session.execute("duplicatePage",{id:"",name:"Panel host copy"}),editor.session.errorString)
            const copy=editor.session.document.pages.find(page=>page.name==="Panel host copy")
            const copiedPanel=copy.layout.components.find(item=>item.family==="page-panel")
            const copiedButton=copy.layout.components.find(item=>item.name==="Switch panel")
            verify(copiedPanel.id!==panel.id)
            compare(copiedButton.actions[0].panelId,copiedPanel.id)
            compare(copiedButton.actions[0].pageId,secondPageId)
            compare(studioFixture.disk(), diskBaseline)
        }
        function test_editable_page_menu_add_cancel_accept_and_reopen() {
            givenPage(false,false)
            verify(editor.command("newPage",{name:"Menu page"}))
            const id=editor.session.document.pages[0].id
            editor.pageId=id;editor.selectedId="";editor.toolbarFocus=false
            const before=document(),disk=studioFixture.disk()
            editor.openInspector(true);findChild(editor,"studioInspector").showDetails()
            const inspector=findChild(editor,"studioInspector")
            verify(inspector!==null)
            const field=homeLayout.studioOptions.inspector.pageFields.find(f=>f.type==="editableMenu")
            verify(field!==undefined);inspector.activateField(field)
            compare(editor.mode,"move");compare(editor.addingType,"page-menu")
            command(inputManager.back);compare(document(),before);compare(studioFixture.disk(),disk)
            editor.openInspector(true);findChild(editor,"studioInspector").showDetails();inspector.activateField(field)
            command(inputManager.confirm);compare(editor.mode,"canvas")
            const items=editor.session.document.pages[0].layout.components
            compare(items.filter(item=>item.family==="page-menu").length,1)
            compare(editor.session.document.pages[0].layout.composition.pageNavigation,"none")
            compare(studioFixture.disk(),disk)
            editor.openInspector(true);findChild(editor,"studioInspector").showDetails();inspector.activateField(field)
            compare(editor.mode,"inspector");compare(editor.current.family,"page-menu")
            compare(editor.session.document.pages[0].layout.components.length,items.length)
            verify(editor.session.undo());compare(document(),before)
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
            givenPage(false,false)
            verify(editor.session.execute("addResolved",{type:"shape",placement:{x:1,y:0,width:1,height:1}}));settle()
            editor.selectedId=editor.items.find(i=>i.id!=="cell-0").id;settle();editor.openInspector(false);findChild(editor,"studioInspector").showDetails()
            const inspector=findChild(editor,"studioInspector");inspector.showDetails();compare(inspector.labels()[2],"Size & layout");compare(inspector.labels()[3],"Accessibility")
                if(inspector.view==="simple")inspector.showDetails()
            inspector.selected=2;command(inputManager.confirm);compare(inspector.fields[0].label,"Outer size")
            command(inputManager.confirm);compare(editor.mode,"size");command(inputManager.back);compare(editor.mode,"inspector");compare(inspector.selected,0)
            command(inputManager.back);command(inputManager.back)
            const before=document();editor.activateEntry({action:"editorSettings"});compare(editor.mode,"editorSettings")
            editor.choice=0;command(inputManager.confirm);compare(editor.mode,"view");editor.choice=0;command(inputManager.confirm);compare(document(),before)
            command(inputManager.back);compare(editor.mode,"editorSettings");compare(editor.choice,0);compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_size_context_dpad_route_restores_exact_field() {
            givenPage(false,false)
            verify(editor.session.execute("addResolved",{type:"shape",placement:{x:1,y:0,width:1,height:1}}));settle()
            editor.selectedId=editor.items.find(i=>i.id!=="cell-0").id;settle()
            const before=document(),selectedId=editor.selectedId
            command(inputManager.confirm);compare(editor.mode,"context")
            command(inputManager.navigateDown);command(inputManager.confirm)
            compare(editor.mode,"inspector")
            const inspector=findChild(editor,"studioInspector");verify(inspector!==null)
                if(inspector.view==="simple")inspector.showDetails()
            const sizeTab=inspector.labels().indexOf("Size & layout");verify(sizeTab>=0)
            for(let i=0;i<sizeTab;++i)command(inputManager.navigateDown)
            compare(inspector.selected,sizeTab);command(inputManager.confirm)
            compare(inspector.view,"fields");compare(inspector.fields[0].label,"Outer size")
            command(inputManager.confirm);compare(editor.mode,"size")
            command(inputManager.navigateRight);compare(editor.candidate.width,2)
            compare(document(),before);compare(editor.selectedId,selectedId)
            command(inputManager.back);compare(editor.mode,"inspector")
            compare(inspector.view,"fields");compare(inspector.tab,sizeTab);compare(inspector.selected,0)
            compare(document(),before);compare(studioFixture.disk(),diskBaseline)
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
            editor.openHeader("title");findChild(editor,"studioInspector").advanced=true;const inspector=findChild(editor,"studioInspector"),field=inspector.fields.find(f=>f.key==="position");verify(!!field)
                if(inspector.view==="simple")inspector.showDetails()
            inspector.activateField(field);compare(inspector.view,"choices");inspector.selected=1;command(inputManager.back);compare(document(),before)
            inspector.activateField(field);command(inputManager.navigateDown);command(inputManager.confirm);settle();compare(editor.page.composition.header.position,"bottom")
            verify(header.y>=content.y+content.height);compare(editor.session.changeCount,1);compare(studioFixture.disk(),diskBaseline)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-header-bottom.png")
            verify(editor.session.undo());settle();compare(document(),before);verify(header.y<content.y)
        }    }
}
