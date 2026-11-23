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
        Component { id: skinPickerComponent; App.SkinPicker { width:1280; height:720; layoutModel:homeLayout } }
        Component { id: smartPlaylistComponent; App.SmartPlaylistEditor { width:1280; height:720 } }
        QtObject {
            id:playlistFixture
            property bool busy:false
            property var smartPlaylist:({})
            property var playlistPreview:[]
            property string playlistStatus:""
            property var savedDraft:null
            property string savedFilename:""
            signal changed()
            signal playlistSaved(string name)
            // Stub Kodi's response while exercising the editor's D-pad and draft paths.
            function previewSmartPlaylist(draft){playlistPreview=[{title:"A decade film"}];changed()}
            // Loaded rules keep the filename used by an existing media shelf.
            function loadSmartPlaylist(name){smartPlaylist=savedDraft;changed()}
            // Capture the draft passed to persistence without writing shared test data.
            function saveSmartPlaylist(name,draft){savedFilename=name;savedDraft=draft;playlistSaved(name || "example.xsp");changed()}
        }
        Component {
            id: arcticHome
            App.HomeScreen {
                width:1280; height:720
                items:homeLayout.components; sections:homeLayout.sections
                composition:homeLayout.composition; appearance:homeLayout.appearance
                grid:composition.grid; brandVisible:homeLayout.brandVisible; tileRoundness:homeLayout.tileRoundness
                backgroundStart:homeLayout.backgroundStart; backgroundEnd:homeLayout.backgroundEnd
                libraryProvider:arcticLibrary; appProvider:({apps:studioFixture.iconAppsFixture()})
                internetAvailable:false;localNetworkAvailable:false;hasContextualSources:false
            }
        }
        QtObject {
            id: arcticLibrary
            property int revision: 1
            property QtObject fixtureShelf: QtObject {
                property bool busy: false
                property string status: ""
                property int selectedIndex: 0
                property var items: ["Northbound", "Night Crossing", "Signal Light", "Low Tide", "The Last Tram", "Blue Hour", "Snowline", "First Light", "Outpost"].map((title,index)=>({title:title,plot:title+" synopsis",mediaType:"movie",mediaId:index+1,year:2026,runtime:6240}))
                signal changed()
                // Emulate selection while leaving playback outside this visual fixture.
                function selectIndex(index) { selectedIndex=index;changed() }
            }
            property QtObject continuedShelf: QtObject {
                property bool busy:false
                property string status:""
                property int selectedIndex:0
                property var items:[{title:"After the Storm",plot:"A different shelf and a different story.",mediaType:"movie",mediaId:10,year:2025}]
                signal changed()
                // Preserve the second shelf's independent selection.
                function selectIndex(index) {selectedIndex=index;changed()}
            }
            // Feed the built-in skin a deterministic shelf through its normal library interface.
            function client(widgetKey,style) { return style.libraryView==="continue"?continuedShelf:fixtureShelf }
            // Deliver a predictable featured image through the same boundary as Kodi fanart.
            function backdrop(mediaType,id) {
                const sky=["#465b73","#804c60","#3e6863"][id%3]
                const svg='<svg xmlns="http://www.w3.org/2000/svg" width="800" height="450"><rect width="800" height="450" fill="'+sky+'"/><circle cx="620" cy="145" r="79" fill="#e8caa0"/><path d="M0 450 V380 L265 214 L380 306 L570 186 L800 390 V450Z" fill="#273744"/><path d="M0 450 V411 L290 335 L490 413 L690 332 L800 373 V450Z" fill="#14232e"/></svg>'
                return {source:"data:image/svg+xml,"+encodeURIComponent(svg)}
            }
            // Give each offline mock movie distinctive portrait artwork without any network dependency.
            function artwork(mediaType,id) {
                const colors=["#284760","#663d52","#445549","#236060","#6b4c32","#353d69","#486277"]
                const svg='<svg xmlns="http://www.w3.org/2000/svg" width="160" height="240"><rect width="160" height="240" fill="'+colors[(id-1)%colors.length]+'"/><circle cx="'+(30+id*13)+'" cy="80" r="34" fill="#d9c397"/><path d="M0 210 L75 '+(110+id*7)+' L160 200 V240 H0Z" fill="#162331"/><path d="M18 215 H142 M44 222 H116" stroke="#e3d3b8" stroke-width="3"/></svg>'
                return {source:"data:image/svg+xml,"+encodeURIComponent(svg)}
            }
        }
        Component {
            id: reflectionFixture
            Rectangle {
                width:100; height:220; color:"black"
                App.HomeCard {
                    anchors.fill:parent
                    item:({name:"Poster",logoSource:"data:image/svg+xml,"+encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="122"><rect width="100" height="122" fill="#ee0000"/><rect y="80" width="100" height="23" fill="#00ee00"/><rect y="103" width="100" height="19" fill="#0000ee"/></svg>'),presentation:{variant:"media",reflection:true,reflectionHeight:40,reflectionOpacity:.8,artFit:"contain",radius:0}})
                }
            }
        }
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
            verify(studioFixture.reset(full || false, scrolling || false),homeLayout.errorString)
            diskBaseline = studioFixture.disk()
            editor = createTemporaryObject(editorComponent, tests.parent)
            verify(editor !== null)
            editor.show("", "cell-0")
            editor.toolbarFocus = false
            tryVerify(() => editor.current !== null)
        }
        function command(value) { editor.handleCommand(value) }
        // Create a 1980s draft, preview it, save it, then edit it in place from the same filename.
        function test_smart_playlist_decade_remote_workflow(){
            const component=createTemporaryObject(smartPlaylistComponent,tests.parent,{client:playlistFixture})
            verify(!!component)
            component.begin("","movies")
            component.handleCommand(inputManager.navigateDown)
            component.handleCommand(inputManager.navigateDown)
            component.handleCommand(inputManager.confirm)
            compare(component.rows()[component.selected],"1980s")
            component.handleCommand(inputManager.confirm)
            compare(component.draft.title,"1980s")
            compare(component.draft.rules.map(rule=>rule.value),["1979","1990"])
            for(let i=0;i<8;++i)component.handleCommand(inputManager.navigateDown)
            component.handleCommand(inputManager.confirm)
            compare(component.screen,"preview")
            compare(component.rows()[1],"A decade film")
            compare(playlistFixture.savedDraft,null)
            component.handleCommand(inputManager.back)
            for(let i=0;i<9;++i)component.handleCommand(inputManager.navigateDown)
            component.handleCommand(inputManager.confirm)
            compare(playlistFixture.savedDraft.title,"1980s")
            component.begin("example.xsp","movies")
            compare(component.draft.rules.length,2)
            component.change("title","Eighties movies")
            for(let i=0;i<9;++i)component.handleCommand(inputManager.navigateDown)
            component.handleCommand(inputManager.confirm)
            compare(playlistFixture.savedFilename,"example.xsp")
            compare(playlistFixture.savedDraft.title,"Eighties movies")
        }
        // Reach Add new skin from the bottom of the list and activate the result independently.
        function test_skin_picker_add_and_activate() {
            verify(studioFixture.reset(),homeLayout.errorString)
            const picker=createTemporaryObject(skinPickerComponent,tests.parent)
            verify(!!picker)
            picker.begin()
            compare(picker.choice.id,"builtin-default")
            for(let index=0;index<picker.entries.length;++index)picker.handleCommand(inputManager.navigateDown)
            compare(picker.choice.id,"")
            picker.handleCommand(inputManager.confirm)
            compare(picker.mode,"create")
            picker.finishName("My new TV skin")
            compare(picker.mode,"list")
            compare(picker.choice.name,"My new TV skin")
            const id=picker.choice.id
            compare(homeLayout.activeSkinId,"builtin-default")
            picker.handleCommand(inputManager.confirm)
            compare(picker.mode,"actions")
            picker.handleCommand(inputManager.confirm)
            compare(homeLayout.activeSkinId,id)
            compare(picker.mode,"list")
        }
        // Browsing built-in skins previews the model without applying it or writing the draft.
        function test_skin_picker_preview_follows_action_focus() {
            verify(studioFixture.reset(),homeLayout.errorString)
            const original=studioFixture.disk()
            const picker=createTemporaryObject(skinPickerComponent,tests.parent)
            verify(!!picker)
            picker.begin()
            picker.handleCommand(inputManager.navigateDown)
            const id=picker.choice.id
            verify(id!==homeLayout.activeSkinId)
            picker.handleCommand(inputManager.confirm)
            tryVerify(()=>!!picker.previewHome)
            verify(!picker.screensaverPreview)
            const saver=picker.actions.findIndex(row=>row.id==="screensaver")
            verify(saver>0)
            for(let index=0;index<saver;++index)picker.handleCommand(inputManager.navigateDown)
            verify(picker.screensaverPreview)
            picker.handleCommand(inputManager.navigateUp)
            verify(!picker.screensaverPreview)
            picker.handleCommand(inputManager.navigateLeft)
            compare(picker.mode,"list")
            compare(picker.choice.id,id)
            compare(homeLayout.activeSkinId,"builtin-default")
            compare(studioFixture.disk(),original)
        }
        // Render the packaged Arctic skin through Home with an offline library fixture.
        function test_zz_arctic_builtin_renders_poster_shelf() {
            verify(studioFixture.reset(),homeLayout.errorString)
            verify(homeLayout.applySkin("builtin-arctic"),homeLayout.errorString)
            const movies=homeLayout.components.find(item=>item.id==="arctic-movies-shelf")
            verify(homeLayout.setItemPresentation(movies.id,Object.assign({},movies.presentation,{
                libraryHeroWidth:100,libraryHeroHeight:100,libraryShelfHeight:352,
                libraryPosterWidth:144,libraryPosterHeight:152,borderWidth:2,borderColor:"#123456",
                libraryShowLabel:true,visibleLabel:"Movie picks"
            })),homeLayout.errorString)
            const continued=homeLayout.components.find(item=>item.id==="arctic-continue-shelf")
            const custom=Object.assign({},continued.presentation,{libraryHeroWidth:57,libraryHeroHeight:48,
                libraryHeroBackground:"fixed",libraryHeroColor:"#26374b",libraryShowLabel:false})
            verify(homeLayout.setItemPresentation(continued.id,custom),homeLayout.errorString)
            diskBaseline=studioFixture.disk()
            const preview=createTemporaryObject(arcticHome,tests.parent)
            verify(!!preview)
            tryVerify(()=>!!findChild(preview,"placed-arctic-movies-shelf"))
            const shelf=findChild(preview,"placed-arctic-movies-shelf")
            tryVerify(()=>!!findChild(preview,"placed-arctic-continue-shelf"))
            tryVerify(()=>!!findChild(shelf,"libraryCard-0"))
            const card=findChild(shelf,"libraryCard-0")
            for(let index=0;index<arcticLibrary.fixtureShelf.items.length;++index){
                const rendered=findChild(shelf,"libraryCard-"+index)
                tryCompare(findChild(rendered,"cardArtwork"),"status",Image.Ready)
            }
            verify(card.reflectionHeight>0)
            compare(Math.round(card.width/card.unit),144)
            compare(Math.round(findChild(card,"cardSurface").height/card.unit),152)
            verify(findChild(card,"cardArtworkOutline").visible)
            verify(Qt.colorEqual(findChild(card,"cardArtworkOutline").border.color,"#123456"))
            settle()
            const title=findChild(preview,"focusedLibraryTitle")
            const synopsis=findChild(preview,"focusedLibrarySynopsis")
            const artwork=findChild(preview,"focusedLibraryArtwork")
            const background=findChild(preview,"focusedLibraryBackground")
            tryCompare(title,"text",arcticLibrary.fixtureShelf.items[0].title)
            tryVerify(()=>!!artwork.backdropColor && background.visible)
            compare(artwork.x,0)
            compare(artwork.y,0)
            compare(artwork.width,artwork.parent.width)
            compare(artwork.height,artwork.parent.height)
            verify(shelf.mapToItem(artwork,0,0).y+card.height<=artwork.height)
            compare(findChild(preview,"sectionHeading-arctic-movies").text,"Movie picks")
            const firstColor=String(artwork.backdropColor)
            const firstArt=String(artwork.source)
            preview.handleCommand(inputManager.navigateRight)
            tryCompare(title,"text",arcticLibrary.fixtureShelf.items[1].title)
            verify(findChild(shelf,"libraryCard-1").selected)
            compare(findChild(shelf,"libraryEmptyFocus").border.width,0)
            compare(synopsis.text,arcticLibrary.fixtureShelf.items[1].plot)
            verify(String(artwork.source)!==firstArt)
            tryCompare(artwork,"status",Image.Ready)
            tryVerify(()=>!!artwork.backdropColor && String(artwork.backdropColor)!==firstColor)
            if(studioFixture.artifacts)grabImage(preview).save(studioFixture.artifacts+"/studio-test-arctic-home.png")
            const nextArt=String(artwork.source)
            preview.handleCommand(inputManager.navigateDown)
            tryCompare(title,"text",arcticLibrary.continuedShelf.items[0].title)
            compare(synopsis.text,arcticLibrary.continuedShelf.items[0].plot)
            verify(String(artwork.source)!==nextArt)
            tryCompare(artwork,"status",Image.Ready)
            tryVerify(()=>background.visible && !artwork.sampleBackdropColor)
            const details=findChild(preview,"focusedLibraryDetails")
            compare(Math.round(100*artwork.width/details.width),57)
            compare(Math.round(100*artwork.height/details.parent.height),48)
            const viewport=findChild(preview,"composedContent")
            const nextShelf=findChild(preview,"placed-arctic-continue-shelf")
            verify(!findChild(preview,"sectionHeading-arctic-continue").visible)
            tryVerify(()=>{
                const bottom=nextShelf.mapToItem(viewport,0,nextShelf.height).y
                return bottom>0 && bottom<=viewport.height+1
            })
            tryVerify(()=>Qt.colorEqual(grabImage(preview).pixel(1,1),"#26374b"))
            if(studioFixture.artifacts)grabImage(preview).save(studioFixture.artifacts+"/studio-test-arctic-next-shelf.png")
            preview.handleCommand(inputManager.navigateUp)
            tryCompare(title,"text",arcticLibrary.fixtureShelf.items[1].title)
            // The editor checks the authored shelf without sharing Home's live fixture client.
            preview.destroy()
            settle()
            editor=createTemporaryObject(editorComponent,tests.parent)
            verify(!!editor)
            editor.show("",movies.id)
            editor.toolbarFocus=false
            tryCompare(editor,"selectedId",movies.id)
            command(inputManager.navigateDown);compare(editor.headerSelection,"title")
            command(inputManager.navigateUp);compare(editor.headerSelection,"")
            compare(editor.selectedId,movies.id)
            command(inputManager.confirm);compare(editor.mode,"context")
            command(inputManager.navigateDown)
            command(inputManager.confirm);compare(editor.mode,"inspector")
            const inspector=findChild(editor,"studioInspector")
            verify(!inspector.collapsed)
            verify(inspector.simpleFields.some(field=>field.key==="visibleLabel"))
            inspector.selected=inspector.simpleFields.findIndex(field=>field.key==="libraryShowLabel")
            verify(inspector.selected>=0)
            command(inputManager.confirm)
            tryVerify(()=>!inspector.simpleFields.some(field=>field.key==="visibleLabel"))
            compare(studioFixture.disk(),diskBaseline)
        }
        // Compare pixels across the poster edge to verify the mirrored order and alpha fade on the actual card.
        function test_poster_reflection_mirrors_art_and_fades() {
            const fixture=createTemporaryObject(reflectionFixture,tests.parent)
            verify(!!fixture)
            tryVerify(()=>grabImage(fixture).pixel(50,115).b>.7)
            const image=grabImage(fixture)
            const near=image.pixel(50,125), mid=image.pixel(50,136), far=image.pixel(50,153)
            if(studioFixture.artifacts)image.save(studioFixture.artifacts+"/studio-test-arctic-poster-reflection.png")
            verify(near.b>.5 && near.r<.05 && near.g<.05, String(near))
            verify(mid.b<near.b && mid.b>.2)
            verify(far.g>.03 && far.g<mid.b && far.b<.05)
        }
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
        // Author a shelf in the catalog, add a named HDMI shortcut, and exercise runtime focus and action routing.
        function test_button_shelf_authoring_and_runtime() {
            givenPage(false,false)
            editor.activateMenu(2)
            const catalog=findChild(editor,"studioCatalog")
            tryVerify(()=>catalog.visible)
            catalog.chooseFamily(catalog.families.findIndex(entry=>entry.id==="more"))
            catalog.chooseFamily(catalog.families.findIndex(entry=>entry.id==="carousel"))
            compare(catalog.view,"carousel")
            catalog.selected=1
            command(inputManager.confirm)
            compare(editor.mode,"move")
            command(inputManager.confirm)
            compare(editor.mode,"canvas")
            const shelf=editor.items.find(item=>item.family==="carousel")
            verify(!!shelf)
            compare(shelf.presentation.variant,"orb")
            editor.selectedId=shelf.id
            editor.context(14)
            editor.activateMenu(2)
            tryCompare(catalog,"view","variants")
            catalog.selected=catalog.variants.findIndex(entry=>entry.id==="round")
            command(inputManager.confirm)
            compare(catalog.view,"links")
            catalog.chooseAction(catalog.actions.find(entry=>entry.action && entry.action.type==="switch-input" && entry.action.input===1))
            compare(editor.mode,"move")
            command(inputManager.confirm)
            compare(editor.mode,"canvas")
            const hdmi=editor.items.find(item=>item.parentId===shelf.id)
            compare(hdmi.name,"HDMI 1")
            compare(hdmi.actions[0].input,1)
            verify(editor.command("addResolved",{type:"button",parentId:shelf.id,placement:{x:1,y:0,width:1,height:1},
                configuration:{name:"Games and apps",actions:[{type:"open-apps"},{type:"open-search"}]}}),editor.session.errorString)
            editor.mode="preview"
            const host=findChild(editor,"studioPreview")
            tryVerify(()=>!!findChild(host,"pageAppCarousel"))
            const widget=findChild(host,"pageAppCarousel")
            tryCompare(widget,"selectedId",hdmi.id)
            let invoked
            host.actionActivated.connect((action,origin)=>{invoked={action:action,origin:origin}})
            host.restoreFocus(shelf.id)
            host.handleCommand(inputManager.confirm)
            verify(host.widgetActive)
            host.handleCommand(inputManager.navigateRight)
            host.handleCommand(inputManager.confirm)
            verify(!!invoked)
            compare(invoked.origin,shelf.id)
            compare(invoked.action.actions.map(action=>action.type),["open-apps","open-search"])
            const selected=widget.selectedId
            host.handleCommand(inputManager.back)
            verify(!host.widgetActive)
            host.restoreFocus(shelf.id)
            tryVerify(()=>host.widgetActive)
            compare(widget.selectedId,selected)
            compare(studioFixture.disk(),diskBaseline)
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
            const inspector=findChild(editor,"studioInspector");compare(inspector.tabs[inspector.tab],"Header");compare(inspector.view,"headerFields")
            inspector.selected=inspector.headerFields().findIndex(f=>f.key==="showTitle");verify(inspector.selected>=0);command(inputManager.confirm);settle()
            compare(editor.page.composition.header.showTitle,true);compare(editor.session.changeCount,count+1)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-header-recovered.png")
            command(inputManager.back);command(inputManager.back);command(inputManager.back);compare(editor.mode,"canvas");compare(editor.headerSelection,"title")
            verify(editor.session.undo());compare(document(),before)
            editor.openHeader("settings");compare(inspector.headerFields()[inspector.selected].key,"settingsVisible");command(inputManager.back);command(inputManager.back);command(inputManager.back)
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
            editor.openHeader("title");const inspector=findChild(editor,"studioInspector");inspector.headerGroup=inspector.headerGroups.findIndex(group=>group.label==="Text")
            const field=inspector.headerFields().find(f=>f.key==="position");verify(!!field)
                if(inspector.view==="simple")inspector.showDetails()
            inspector.activateField(field);compare(inspector.view,"choices");inspector.selected=1;command(inputManager.back);compare(document(),before)
            inspector.activateField(field);command(inputManager.navigateDown);command(inputManager.confirm);settle();compare(editor.page.composition.header.position,"bottom")
            verify(header.y>=content.y+content.height);compare(editor.session.changeCount,1);compare(studioFixture.disk(),diskBaseline)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-header-bottom.png")
            verify(editor.session.undo());settle();compare(document(),before);verify(header.y<content.y)
        }
        // Verify alignment in both header modes and ensure a large clock can fit seconds.
        function test_header_alignment_clock_and_grouped_controls(){
            givenPage(false,false)
            const preview=findChild(editor,"studioPreview")
            const inspector=findChild(editor,"studioInspector")
            const slot=findChild(preview,"pageHeader-title")
            for(const mode of ["identity","navigation"]){
                const row=findChild(preview,mode==="identity"?"pageHeader-identity":"pageHeader-navigation")
                for(const alignment of ["left","center","right"]){
                    const value={headerMode:mode,identity:"Match night",showClock:true,
                        navigation:[{id:"home",name:"Home"},{id:"movies",name:"Movies"}],
                        headerControls:["settings","edit"],
                        header:{schemaVersion:1,titleAlignment:alignment,clockSize:42,clockColor:"#77ccff",height:88,timeFormat:"HH:mm:ss",dateFormat:"dddd, MMMM d",clockWidth:100}}
                    verify(editor.command("pagePatch",{scope:"composition",value:value}),editor.session.errorString)
                    settle()
                    const space=slot.width-row.width
                    verify(space>0)
                    compare(Math.round(row.x),Math.round(alignment==="left"?0:alignment==="center"?space/2:space))
                }
            }
            const time=findChild(preview,"pageHeader-timeLabel")
            const clock=findChild(preview,"pageHeader-clock")
            verify(/^\d\d:\d\d:\d\d$/.test(time.text))
            verify(Qt.colorEqual(time.color,"#77ccff"))
            verify(clock.width>100 && time.width>=time.implicitWidth && !time.truncated)
            editor.openInspector(true);inspector.controlsShown=true;inspector.openBasicTab("Header")
            compare(inspector.labels(),["Logo","Title","Text","Search","Settings","Time","Date"])
            for(let i=0;i<3;++i)command(inputManager.navigateDown)
            command(inputManager.confirm)
            compare(inspector.headerFields().map(field=>field.key),["searchVisible"])
            command(inputManager.confirm)
            verify(editor.page.composition.headerControls.includes("search"))
            command(inputManager.back)
            compare(inspector.labels()[inspector.selected],"Search")
            command(inputManager.navigateDown)
            command(inputManager.confirm)
            compare(inspector.headerGroups[inspector.headerGroup].label,"Settings")
            const policyIndex=inspector.headerFields().findIndex(field=>field.key==="sourcePolicy")
            verify(policyIndex>=0)
            for(let i=0;i<policyIndex;++i)command(inputManager.navigateDown)
            const beforePolicy=document()
            command(inputManager.confirm)
            compare(inspector.view,"sourcePolicy")
            settle()
            command(inputManager.back)
            compare(inspector.view,"headerFields")
            compare(inspector.selected,policyIndex)
            compare(document(),beforePolicy)
            compare(studioFixture.disk(),diskBaseline)
        }
    }
}
