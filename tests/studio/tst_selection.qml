import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioSelection";when:windowShown
        property var editor:null
        property string diskBaseline:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        Component {id:backdropComponent;App.PageBackdrop {width:320;height:180;startColor:"black";endColor:"blue"}}
        Component {id:orbComponent;App.OrbCard {width:140;height:150;item:({name:"Logo",logoSource:Qt.resolvedUrl("../visual/home-pastel-reference/wallpaper.png")})}}
        Component {id:menuComponent;App.PageMenuWidget {width:600;height:120;appearance:({textColor:"white",surfaceColor:"#202126",focusColor:"pink"});style:({layout:{mode:"row",columns:3,rows:1,padding:8,gap:8}})}}
        Component {id:menuHomeComponent;App.HomeScreen {width:1280;height:720;sections:[];brandVisible:false;backgroundStart:"black";backgroundEnd:"black";tileRoundness:0.2;internetAvailable:false;localNetworkAvailable:false;hasContextualSources:false}}
        SignalSpy {id:menuSpy;signalName:"launch"}
        SignalSpy {id:menuHomeSpy;signalName:"actionActivated"}
        QtObject {id:fakeRegistry;property var apps:[]}
        Component {id:carouselComponent;App.PageAppCarousel {width:900;height:160;registry:fakeRegistry;appearance:({textColor:"#514b67",surfaceColor:"#f7edff",focusColor:"#bd8198"});style:({variant:"orb",layout:{visibleColumns:2}})}}
        SignalSpy {id:launchSpy;signalName:"launch"}
        SignalSpy {id:switchSpy;signalName:"clicked"}
        function test_page_menu_remote_navigation_and_preview() {
            const menu=createTemporaryObject(menuComponent,tests.parent,{pages:[{id:"",name:"Home"},{id:"movies",name:"Movies"},{id:"shows",name:"Shows"}]})
            verify(menu!==null);menuSpy.target=menu;menuSpy.clear()
            verify(menu.handleCommand(inputManager.confirm));compare(menuSpy.signalArguments[0][0].type,"root-page")
            verify(menu.handleCommand(inputManager.navigateRight));verify(menu.handleCommand(inputManager.confirm))
            compare(menuSpy.signalArguments[1][0].pageId,"movies")
            compare(menu.handleCommand(inputManager.back),false)
            menu.readOnly=true;compare(menu.handleCommand(inputManager.confirm),false);compare(menuSpy.count,2)
            menu.readOnly=false;menu.pages=[];menu.handleCommand(inputManager.confirm);compare(menuSpy.count,2)
            menu.pages=[{id:"",name:"Home"},{id:"movies",name:"Movies"}];menu.activeId="movies";compare(menu.selected,1)
            menu.style={layout:{mode:"row",columns:16,rows:1,padding:80,gap:80}};compare(menu.validGeometry,false);compare(menu.handleCommand(inputManager.confirm),false)
            menuSpy.target=null
        }
        function test_page_menu_conversion_is_one_undoable_draft_transaction() {
            const grid=editor.page.grid
            verify(editor.session.execute("newPage",{name:"Movies",preset:"blank"}),editor.session.errorString)
            const id=editor.session.document.pages[editor.session.document.pages.length-1].id
            verify(editor.session.execute("pagePatch",{scope:"composition",value:{grid:grid,pageNavigation:"left"}},id),editor.session.errorString)
            const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            verify(editor.session.execute("addResolved",{type:"page-menu",placement:{x:0,y:0,width:4,height:1}},id),editor.session.errorString)
            const page=editor.session.document.pages.find(p=>p.id===id).layout
            compare(page.composition.pageNavigation,"none");compare(page.components.length,1)
            compare(page.components[0].family,"page-menu");compare(editor.session.changeCount,count+1)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before)
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_page_menu_uses_production_home_remote_dispatch() {
            const prototype=editor.session.componentPrototype("page-menu")
            verify(!!prototype.root)
            const item=Object.assign({},prototype.root,{placement:{x:0,y:0,width:4,height:1}})
            const screen=createTemporaryObject(menuHomeComponent,tests.parent,{items:[item],authoredItems:[item],placements:[{id:item.id,placement:item.placement}],composition:editor.page.composition,grid:editor.page.grid,appearance:editor.page.appearance,menuPages:[{id:"",name:"Home"},{id:"movies",name:"Movies"}]})
            verify(screen!==null);menuHomeSpy.target=screen;menuHomeSpy.clear();settle()
            screen.recoveryFocused=false;screen.focusedIndex=0
            screen.handleCommand(inputManager.navigateRight);screen.handleCommand(inputManager.confirm)
            compare(menuHomeSpy.count,1);compare(menuHomeSpy.signalArguments[0][0].pageId,"movies")
            menuHomeSpy.clear();screen.handleCommand(inputManager.back)
            const vertical=Object.assign({},item,{placement:{x:0,y:0,width:4,height:3},presentation:Object.assign({},item.presentation,{layout:{schemaVersion:1,mode:"column",columns:1,rows:3,gap:8,padding:8}})})
            screen.items=[vertical];screen.authoredItems=[vertical];screen.placements=[{id:vertical.id,placement:vertical.placement}];settle()
            screen.recoveryFocused=false;screen.focusedIndex=0
            screen.handleCommand(inputManager.navigateDown);screen.handleCommand(inputManager.confirm)
            compare(menuHomeSpy.count,1);compare(menuHomeSpy.signalArguments[0][0].pageId,"movies")
            menuHomeSpy.target=null
        }
        function test_logo_treatment_and_shape_following_focus() {
            const orb=createTemporaryObject(orbComponent,tests.parent,{style:{logoMode:"cutout",focusWidth:5,artFit:"contain"},selected:true})
            verify(orb!==null);compare(orb.logoOnly,true)
            const art=findChild(orb,"orbArtwork"),surface=findChild(orb,"orbSurface")
            compare(surface.border.width,0);compare(art.radius,0);compare(art.outlineWidth,5)
            orb.selected=false;compare(art.outlineWidth,0)
            orb.style={logoMode:"tile"};compare(orb.logoOnly,false);verify(art.radius>0)
            orb.style={logoMode:"original"};compare(orb.logoOnly,true)
            orb.item={name:"Missing artwork"};compare(orb.logoOnly,false)
        }
        function test_wallpaper_return_retains_ready_image_and_custom_crop() {
            const backdrop=createTemporaryObject(backdropComponent,tests.parent,{source:Qt.resolvedUrl("../visual/home-pastel-reference/wallpaper.png"),config:{mode:"image",artFit:"cover"}})
            verify(backdrop!==null)
            tryCompare(backdrop,"status",Image.Ready)
            compare(backdrop.directImage,true)
            const image=findChild(backdrop,"backdropImage")
            const original=image.source.toString()
            for(let i=0;i<5;i++) {backdrop.visible=false;wait(20);backdrop.visible=true;compare(backdrop.status,Image.Ready);compare(image.source.toString(),original)}
            backdrop.config={mode:"image",artFit:"contain"}
            compare(image.fillMode,Image.PreserveAspectFit)
            backdrop.config={mode:"image",artZoom:2,artFocalX:0.2}
            compare(backdrop.directImage,false)
            compare(image.source.toString(),"")
            tryCompare(backdrop,"status",Image.Ready)
            backdrop.config={mode:"solid"}
            compare(backdrop.status,Image.Null)
        }
        function test_app_collection_registry_membership_overflow_and_return() {
            fakeRegistry.apps=[{id:"one",name:"One",type:"desktop",section:"Games"},{id:"mock",name:"Fake",type:"mock"},{id:"two",name:"Two",type:"web",section:"Music"},{id:"three",name:"Three",type:"desktop",section:"Games"}]
            const carousel=createTemporaryObject(carouselComponent,tests.parent)
            verify(carousel!==null);launchSpy.target=carousel;launchSpy.clear()
            compare(carousel.eligible.length,3);compare(carousel.cards.length,2)
            compare(carousel.cards[1].id,"__more__")
            carousel.engaged=true;carousel.handleCommand(inputManager.navigateRight);carousel.handleCommand(inputManager.confirm)
            const gallery=findChild(carousel,"appCarouselGallery")
            tryCompare(gallery,"opened",true)
            compare(findChild(carousel,"appCarouselGalleryGrid").count,3)
            carousel.handleCommand(inputManager.navigateRight);carousel.handleCommand(inputManager.confirm)
            const remembered=carousel.focusState()
            const recreated=createTemporaryObject(carouselComponent,tests.parent)
            recreated.restoreFocusState(remembered)
            compare(recreated.selectedId,remembered.selectedId);compare(recreated.galleryId,remembered.galleryId)
            tryCompare(findChild(recreated,"appCarouselGallery"),"opened",true)
            recreated.handleCommand(inputManager.back)
            compare(launchSpy.count,1);compare(launchSpy.signalArguments[0][0].appId,"two")
            compare(carousel.galleryId,"two")
            carousel.handleCommand(inputManager.back);compare(carousel.selectedId,"__more__")
            carousel.collection={mode:"selected",ids:["three","one"]}
            compare(carousel.eligible[0].id,"three");compare(carousel.eligible[1].id,"one");compare(carousel.overflow,false)
            carousel.collection={mode:"all",ids:[],section:"Games"};compare(carousel.eligible.length,2)
            carousel.style={variant:"orb",layout:{visibleColumns:1}}
            compare(carousel.cards.length,1);compare(carousel.cards[0].id,"__more__")
            carousel.activate("__more__");tryCompare(gallery,"opened",true);compare(findChild(carousel,"appCarouselGalleryGrid").count,2)
            compare(carousel.eligible[0].id,"one");compare(carousel.eligible[1].id,"three")
            carousel.handleCommand(inputManager.back);compare(carousel.selectedId,"__more__")
            fakeRegistry.apps=fakeRegistry.apps.concat(Array.from({length:130},(_,i)=>({id:"music-"+i,name:"Music "+i,type:"desktop",section:"Music"})))
            compare(carousel.eligible.length,2);compare(carousel.collectionTooLarge,false)
            carousel.collection={mode:"all",ids:[]};compare(carousel.collectionTooLarge,true);compare(carousel.eligible.length,0)
            carousel.collection={mode:"selected",ids:["three","one"],section:"Games"};compare(carousel.eligible.length,2)
            fakeRegistry.apps=[{id:"one",name:"One",type:"desktop",section:"Games"}]
            compare(carousel.eligible.length,1);compare(carousel.selectedId,"one")
            carousel.readOnly=true;carousel.activate("one");compare(launchSpy.count,1)
            launchSpy.target=null
        }
        function init() {
            failOnWarning(/Binding loop detected|ReferenceError|TypeError/)
            verify(studioFixture.reset(false,false))
            diskBaseline=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,tests.parent)
            verify(editor!==null)
            editor.show("","cell-0");editor.toolbarFocus=false
            tryVerify(()=>editor.current!==null)
        }
        function cleanup(){if(editor)editor.close();editor=null}
        function command(value){editor.handleCommand(value)}
        function settle(){let ready=false;Qt.callLater(()=>Qt.callLater(()=>ready=true));tryVerify(()=>ready)}
        function test_recently_viewed_catalog_count_capacity_and_private_cancel() {
            verify(editor.session.execute("composition",{value:Object.assign({},editor.page.composition,{grid:Object.assign({},editor.page.grid,{columns:8,rows:5})})}),editor.session.errorString)
            editor.toolbarFocus=true;editor.menuIndex=0;command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
            compare(editor.mode,"catalog")
            command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
            compare(editor.mode,"move");compare(editor.addingType,"recently-viewed")
            const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            command(inputManager.confirm);compare(editor.mode,"canvas");compare(editor.session.changeCount,count+1)
            compare(editor.current.family,"recently-viewed");compare(editor.current.source.kind,"recently-viewed")
            compare(editor.current.presentation.libraryKind,"history");compare(editor.current.presentation.libraryView,"watched")
            const added=JSON.stringify(editor.session.document)
            editor.openInspector(false);settle()
            const inspector=findChild(editor,"studioInspector")
            if(inspector.collapsed)command(inputManager.confirm)
            command(inputManager.confirm);compare(inspector.view,"fields")
            verify(!inspector.fields.some(f=>["libraryKind","libraryView","libraryValue"].indexOf(f.key)>=0))
            const index=inspector.fields.findIndex(f=>f.key==="libraryLimit");verify(index>=0)
            for(let i=0;i<index;++i)command(inputManager.navigateDown)
            command(inputManager.confirm);compare(inspector.view,"scalar");command(inputManager.navigateRight)
            settle();if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-history-count.png")
            compare(JSON.stringify(editor.session.document),added)
            command(inputManager.back);compare(JSON.stringify(editor.session.document),added)
            command(inputManager.confirm);command(inputManager.navigateRight);command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
            compare(editor.current.presentation.libraryLimit,21);compare(editor.current.presentation.libraryColumns,5)
            compare(editor.session.changeCount,count+2)
            command(inputManager.back);command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
            const capacity=inspector.fields.findIndex(f=>f.key==="libraryColumns");verify(capacity>=0)
            for(let i=0;i<capacity;++i)command(inputManager.navigateDown)
            command(inputManager.confirm);compare(inspector.view,"scalar");command(inputManager.navigateLeft)
            command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
            compare(editor.current.presentation.libraryColumns,4);compare(editor.current.presentation.libraryLimit,21)
            verify(editor.session.undo());compare(editor.current.presentation.libraryColumns,5)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),added)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before)
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_authored_orb_focus_survives_context_and_toolbar() {
            verify(editor.session.execute("addResolved",{type:"button",placement:{x:1,y:0,width:2,height:1}}))
            const button=editor.items.find(i=>i.id!=="cell-0")
            verify(!!button)
            verify(editor.session.execute("patch",{id:button.id,value:{presentation:{variant:"orb",focusColor:"#12AB89",focusWidth:6,surfaceOpacity:.4}}}))
            editor.selectedId=button.id
            settle()
            let card=findChild(editor,"placedCard-"+button.id)
            verify(card!==null);tryCompare(card,"selected",true)
            let circle=findChild(card,"orbSurface")
            verify(circle!==null);compare(circle.radius,circle.width/2)
            compare(circle.border.color,Qt.rgba(18/255,171/255,137/255,1))
            compare(circle.border.width,6*card.unit)
            compare(findChild(editor,"studioSelectionTarget-"+button.id).border.width,0)
            verify(editor.session.execute("patch",{id:button.id,value:{opacity:0}}))
            settle();card=findChild(editor,"placedCard-"+button.id);circle=findChild(card,"orbSurface")
            const recovery=findChild(editor,"studioRecoveryFocus-"+button.id)
            verify(recovery!==null);tryCompare(recovery,"visible",true)
            compare(recovery.radius,circle.radius);compare(recovery.width,circle.width)
            compare(recovery.border.color,circle.border.color)
            const before=JSON.stringify(editor.session.document),history=editor.session.changeCount
            command(inputManager.confirm);compare(editor.mode,"context");compare(card.selected,true)
            editor.toolbarFocus=true;compare(card.selected,true)
            editor.toolbarFocus=false;editor.openInspector(false);compare(card.selected,true)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-selection-orb.png")
            compare(JSON.stringify(editor.session.document),before);compare(editor.session.changeCount,history)
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_rounded_focus_tracks_selection_without_editing_document() {
            verify(editor.session.execute("patch",{id:"cell-0",value:{presentation:{radius:30,focusColor:"#EAD048",focusWidth:5}}}))
            settle()
            const card=findChild(editor,"placedCard-cell-0")
            verify(card!==null);tryCompare(card,"selected",true)
            const surface=findChild(card,"cardSurface")
            compare(surface.radius,card.corner);verify(surface.radius>0)
            const before=JSON.stringify(editor.session.document)
            editor.selectedId="";tryCompare(card,"selected",false)
            editor.selectedId="cell-0";tryCompare(card,"selected",true)
            editor.openInspector(false);compare(card.selected,true)
            compare(JSON.stringify(editor.session.document),before)
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_switch_pointer_remote_parity_and_undo() {
            editor.openInspector(false)
            const inspector=findChild(editor,"studioInspector")
            inspector.advanced=true;inspector.view="fields"
            inspector.tab=inspector.tabs.indexOf("Appearance")
            const fieldIndex=inspector.fields.findIndex(f=>f.key==="showTitle")
            verify(fieldIndex>=0);inspector.selected=fieldIndex
            const fields=findChild(inspector,"studioInspectorFields")
            let control=findChild(fields.currentItem,"studioSwitch-showTitle")
            verify(control!==null)
            tryVerify(()=>control.visible && control.width>0)
            tryVerify(()=>{const point=control.mapToItem(fields,control.width/2,control.height/2);return point.y>=0 && point.y<fields.height})
            compare(control.checked,true)
            const before=JSON.stringify(editor.session.document)
            switchSpy.target=control;switchSpy.clear()
            verify(waitForRendering(control))
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-switch-before.png")
            mouseClick(control,control.width-28,control.height/2)
            compare(switchSpy.count,1)
            compare(control.checked,false);compare(editor.session.changeCount,1)
            const pointer=JSON.stringify(editor.session.document)
            switchSpy.target=null
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before)
            settle()
            compare(inspector.effective(inspector.current,inspector.fields[fieldIndex]),true,"Undo restores the canonical effective value")
            compare(inspector.selected,fieldIndex,"Inspector retains the selected field")
            compare(fields.currentIndex,fieldIndex,"List follows the selected field after model replacement")
            tryVerify(()=>{control=fields.currentItem?findChild(fields.currentItem,"studioSwitch-showTitle"):null;return control!==null && control.checked===true})
            command(inputManager.confirm)
            settle();tryVerify(()=>{control=fields.currentItem?findChild(fields.currentItem,"studioSwitch-showTitle"):null;return control!==null})
            compare(control.checked,false);compare(JSON.stringify(editor.session.document),pointer)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-selection-switch.png")
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before)
            compare(studioFixture.disk(),diskBaseline)
        }
    }
}
