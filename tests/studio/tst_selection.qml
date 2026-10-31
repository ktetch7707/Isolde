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
        Component {id:appPickerCatalogComponent;App.StudioCatalog {width:1280;height:720}}
        Component {id:appPickerInspectorComponent;App.StudioInspector {width:1280;height:720}}
        SignalSpy {id:appPickerSpy;signalName:"picked"}
        function test_starting_styles_remote_cancel_and_pointer_parity(){
            const catalog=createTemporaryObject(appPickerCatalogComponent,tests.parent,{session:editor.session,options:homeLayout.studioOptions,appearance:editor.page.appearance});verify(catalog!==null);catalog.z=10000
            appPickerSpy.target=catalog;appPickerSpy.clear();const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            const family=catalog.families.findIndex(f=>f.id==="button");catalog.chooseFamily(family);compare(catalog.view,"variants")
            catalog.handleCommand(inputManager.back);compare(catalog.view,"families");compare(appPickerSpy.count,0)
            catalog.chooseFamily(family);catalog.handleCommand(inputManager.navigateRight);catalog.handleCommand(inputManager.confirm)
            compare(catalog.configuration.variant,"round");compare(catalog.view,"links")
            catalog.handleCommand(inputManager.navigateDown);catalog.handleCommand(inputManager.confirm);compare(appPickerSpy.count,1)
            const remote=editor.session.componentPrototype("button",appPickerSpy.signalArguments[0][1]);compare(remote.root.placement.width,2);compare(remote.root.presentation.variant,"orb")
            appPickerSpy.clear();catalog.begin();catalog.chooseFamily(family);const round=findChild(catalog,"studioVariant-round");verify(round!==null)
            mouseClick(round,round.width/2,round.height/2);compare(catalog.view,"links");catalog.handleCommand(inputManager.navigateDown);catalog.handleCommand(inputManager.confirm)
            compare(appPickerSpy.count,1);const pointer=editor.session.componentPrototype("button",appPickerSpy.signalArguments[0][1]);compare(JSON.stringify(pointer),JSON.stringify(remote))
            compare(JSON.stringify(editor.session.document),before);compare(editor.session.changeCount,count);compare(studioFixture.disk(),diskBaseline)
        }
        Component {id:collectionPickerComponent;App.StudioAppPicker {width:1280;height:720;settings:homeLayout.studioOptions.appPicker;appearance:homeLayout.studioAppearance}}
        SignalSpy {id:collectionPickerSpy;signalName:"accepted"}
        Component {id:actionPickerComponent;App.StudioActionPicker {width:1280;height:720;categories:homeLayout.studioOptions.actionCategories;appearance:homeLayout.studioAppearance}}
        SignalSpy {id:actionPickedSpy;signalName:"picked"}
        SignalSpy {id:actionCanceledSpy;signalName:"canceled"}
        function test_action_picker_search_categories_and_cancel(){
            const actions=[{name:"Open Home",action:{type:"root-page"}},{name:"Cinema input",aliases:["HDMI","capture source"],action:{type:"switch-input",input:2}},{name:"Movie",needsMedia:"movies"}]
            const picker=createTemporaryObject(actionPickerComponent,tests.parent,{actions:actions});verify(picker!==null)
            actionPickedSpy.target=picker;actionCanceledSpy.target=picker;actionPickedSpy.clear();actionCanceledSpy.clear()
            picker.query="HDMI source";compare(picker.entries.length,1);compare(picker.entries[0].name,"Cinema input")
            picker.handleCommand(inputManager.navigateDown);picker.handleCommand(inputManager.confirm)
            compare(actionPickedSpy.count,1);compare(actionPickedSpy.signalArguments[0][0].action.input,2)
            picker.query="";picker.category=homeLayout.studioOptions.actionCategories.findIndex(c=>c.types.indexOf("root-page")>=0)
            compare(picker.entries.length,1);compare(picker.entries[0].name,"Open Home")
            picker.query="no matching link";compare(picker.entries.length,0);picker.toolsFocused=true;picker.tool=0
            picker.handleCommand(inputManager.confirm);verify(picker.searching)
            picker.handleCommand(inputManager.back);verify(!picker.searching);compare(picker.query,"no matching link")
            picker.handleCommand(inputManager.back);compare(actionCanceledSpy.count,1);compare(actionPickedSpy.count,1)
            compare(actions[1].action.input,2)
        }
        Component {id:backdropComponent;App.PageBackdrop {width:320;height:180;startColor:"black";endColor:"blue"}}
        Component {id:shapeCardComponent;App.HomeCard {width:240;height:120;appearance:homeLayout.appearance}}
        Component {id:motionImageComponent;App.ArtworkImage {width:100;height:100;property bool screenGate:true;function onScreen(){return screenGate}}}
        function test_gif_motion_default_pause_and_release(){
            let acquired=0,released=0
            const still=Qt.resolvedUrl("../visual/home-pastel-reference/wallpaper.png").toString()
            const resolver={artworkAnimation:function(relative,owner){acquired++;return {frames:[still,still],delays:[1000,1000]}},releaseArtworkAnimation:function(owner){released++}}
            const image=createTemporaryObject(motionImageComponent,tests.parent,{managedAsset:"artwork/animation.gif",animationResolver:resolver});verify(image!==null)
            image.advanceAnimation();compare(acquired,0);compare(image.animationFrame,0)
            image.motionEnabled=true;image.advanceAnimation();compare(acquired,1);compare(image.animation.frames.length,2)
            image.advanceAnimation();compare(image.animationFrame,1)
            image.reducedMotion=true;compare(released,1);compare(image.animation.frames,undefined);image.advanceAnimation();compare(acquired,1)
            image.reducedMotion=false;image.screenGate=false;image.advanceAnimation();compare(acquired,1)
            image.screenGate=true;image.advanceAnimation();compare(acquired,2)
            image.screenGate=false;image.advanceAnimation();compare(released,2);compare(image.animation.frames,undefined)
            image.screenGate=true;image.advanceAnimation();compare(acquired,3);image.animationResolver=null;compare(released,3)
        }
        Component {id:orbComponent;App.OrbCard {width:140;height:150;item:({name:"Logo",logoSource:Qt.resolvedUrl("../visual/home-pastel-reference/wallpaper.png")})}}
        Component {id:menuComponent;App.PageMenuWidget {width:600;height:120;appearance:({textColor:"white",surfaceColor:"#202126",focusColor:"pink"});style:({layout:{mode:"row",columns:3,rows:1,padding:8,gap:8}})}}
        Component {id:menuHomeComponent;App.HomeScreen {width:1280;height:720;sections:[];brandVisible:false;backgroundStart:"black";backgroundEnd:"black";tileRoundness:0.2;internetAvailable:false;localNetworkAvailable:false;hasContextualSources:false}}
        SignalSpy {id:menuSpy;signalName:"launch"}
        SignalSpy {id:menuHomeSpy;signalName:"actionActivated"}
        QtObject {id:fakeRegistry;property var apps:[]}
        Component {id:carouselComponent;App.PageAppCarousel {width:900;height:160;registry:fakeRegistry;appearance:({textColor:"#514b67",surfaceColor:"#f7edff",focusColor:"#bd8198"});style:({variant:"orb",layout:{visibleColumns:2}})}}
        SignalSpy {id:launchSpy;signalName:"launch"}
        QtObject {
            id:historyClientFixture
            property var items:[]
            property int selectedIndex:-1
            property bool busy:false
            property string status:"Nothing viewed yet"
            signal changed()
            function selectIndex(value){selectedIndex=Math.max(-1,Math.min(value,items.length-1))}
        }
        QtObject {
            id:historyProviderFixture
            property int revision:0
            function client(key,style){return historyClientFixture}
            function artwork(kind,id){return null}
        }
        Component {id:historyWidgetComponent;App.PageLibraryWidget {width:900;height:220;provider:historyProviderFixture;widgetKey:"history-empty-update";style:({libraryKind:"history",libraryView:"watched",libraryChrome:false,libraryLayout:"row",libraryColumns:3});appearance:homeLayout.studioAppearance}}
        function test_headless_history_selects_first_card_after_empty_update(){
            historyClientFixture.items=[];historyClientFixture.selectedIndex=-1
            const widget=createTemporaryObject(historyWidgetComponent,tests.parent);verify(widget!==null)
            compare(widget.displayedItems.length,0)
            historyClientFixture.items=[{providerId:"kodi",mediaType:"episode",mediaId:7,title:"Actually viewed episode"}]
            tryCompare(historyClientFixture,"selectedIndex",0)
            widget.engaged=true
            widget.frozen=true;const selected=historyClientFixture.selectedIndex
            historyClientFixture.items=[];historyClientFixture.selectedIndex=-1
            compare(widget.frozenIndex,selected);compare(widget.displayedItems.length,1)
            widget.frozen=false
        }
        function test_button_carousel_scoped_overflow_and_focus(){
            const items=[];for(let i=0;i<20;i++)items.push({id:"button-"+i,name:"Button "+i,action:{type:"switch-input",input:i%8+1}})
            const carousel=createTemporaryObject(carouselComponent,tests.parent,{buttonItems:items,style:{layout:{mode:"row",columns:20,rows:1,visibleColumns:4,gap:12},moreLabel:"More buttons",galleryTitle:"Input shortcuts"}});verify(carousel!==null)
            launchSpy.target=carousel;launchSpy.clear();compare(carousel.cards.length,4);compare(carousel.cards[3].name,"More buttons");compare(carousel.galleryApps.length,20)
            carousel.selectedId="__more__";carousel.handleCommand(inputManager.confirm);verify(carousel.focusState().galleryOpen)
            carousel.handleCommand(inputManager.navigateDown);compare(carousel.galleryId,"button-4");carousel.handleCommand(inputManager.confirm)
            compare(launchSpy.count,1);compare(launchSpy.signalArguments[0][0].input,5)
            const saved=carousel.focusState();verify(saved.galleryOpen);carousel.restoreFocusState(saved);compare(carousel.galleryId,"button-4")
            carousel.handleCommand(inputManager.back);verify(!carousel.focusState().galleryOpen);compare(carousel.selectedId,"__more__")
            carousel.style={layout:{mode:"row",columns:20,rows:1,visibleColumns:1},moreLabel:"More buttons"};compare(carousel.cards.length,1);compare(carousel.cards[0].id,"__more__");compare(carousel.galleryApps.length,20)
            carousel.style={layout:{mode:"grid",columns:2,rows:2},moreLabel:"More buttons"};carousel.selectedId="button-0"
            verify(carousel.handleCommand(inputManager.navigateDown));compare(carousel.selectedId,"button-2");compare(carousel.cards.length,4)
            carousel.buttonItems=[{id:"unassigned",name:"Unassigned"}];carousel.activate("unassigned");compare(launchSpy.count,1);verify(carousel.message.length>0)
        }
        QtObject {
            id:storeCatalog
            property var storeOptions:({platforms:[{type:"android",label:"Android"},{type:"web",label:"Chromium"}],columns:2,spacing:20,cardHeight:142})
            property bool busy:false
            property string errorString:""
            property bool onlineStoreSearch:false
            property int webAdds:0
            property var entries:[]
            function setOnlineStoreSearch(value){onlineStoreSearch=value}
            function searchStore(query){}
            function addWebApp(name,url,backend){webAdds++;entries=[{id:"web-custom",name:name,type:"web",storeType:"web",category:"Web",publisher:"example.org",description:url,symbol:"W",accent:"#347d9c",installed:true}];return "web-custom"}
            function install(id){entries=entries.map(e=>Object.assign({},e,{installed:e.id===id?true:e.installed}));return true}
            function uninstall(id){entries=entries.map(e=>Object.assign({},e,{installed:e.id===id?false:e.installed}));return true}
        }
        Component {id:storeComponent;App.AppStoreView {width:1280;height:720;catalog:storeCatalog;entries:storeCatalog.entries;appearance:({surfaceColor:"#f1e6f5",textColor:"#40384d",focusColor:"#8d4b73"})}}
        SignalSpy {id:storeOpenSpy;signalName:"openRequested"}
        function test_store_add_website_cancel_and_remote_form() {
            storeCatalog.entries=[];storeCatalog.webAdds=0
            const store=createTemporaryObject(storeComponent,tests.parent);verify(store!==null)
            editor.visible=false;store.z=10000;store.choosePlatform(1)
            store.visible=false;editor.visible=true;editor.forceActiveFocus()
            store.visible=true;tryCompare(store,"activeFocus",true);editor.visible=false
            store.handleCommand(inputManager.menu);store.handleCommand(inputManager.confirm);verify(store.addWebOpen)
            store.handleCommand(inputManager.back);verify(!store.addWebOpen);compare(storeCatalog.webAdds,0)
            store.handleCommand(inputManager.confirm);store.handleCommand(inputManager.confirm)
            verify(store.searchOpen);const keyboard=findChild(store,"storeKeyboard");verify(keyboard!==null);keyboard.accepted("Example website")
            compare(store.webName,"Example website");verify(!store.searchOpen)
            store.handleCommand(inputManager.navigateDown);store.handleCommand(inputManager.confirm);keyboard.accepted("https://example.org/")
            compare(store.webAddress,"https://example.org/")
            wait(100);if(studioFixture.artifacts)grabImage(store).save(studioFixture.artifacts+"/studio-test-store-add-website.png")
            store.handleCommand(inputManager.navigateDown);store.handleCommand(inputManager.navigateDown);store.handleCommand(inputManager.navigateDown)
            store.handleCommand(inputManager.confirm);compare(storeCatalog.webAdds,1);verify(!store.addWebOpen);compare(store.selectedId,"web-custom");compare(store.area,"actions")
        }
        function test_store_chromium_wrappers_and_large_text_actions() {
            storeCatalog.entries=[{id:"browser-wrapper",name:"A Chromium desktop wrapper",type:"desktop",storeType:"web",presentationMode:"web-kiosk",category:"Video",publisher:"Example",description:"A long description for a remote-friendly browser application that wraps over several lines at larger text sizes. The selected action must remain fully visible when navigating with the remote control.",symbol:"C",accent:"#347d9c",installed:true}]
            const store=createTemporaryObject(storeComponent,tests.parent);verify(store!==null)
            editor.visible=false;store.z=10000
            store.appearance={surfaceColor:"#f1e6f5",textColor:"#40384d",focusColor:"#8d4b73",typographyScale:1.6}
            store.choosePlatform(1);compare(store.filtered.length,1)
            store.deviceApps=true;compare(store.filtered.length,0)
            store.deviceApps=false;store.area="actions";store.actionIndex=1
            const details=findChild(store,"storeDetails"),action=findChild(store,"store-action-1")
            verify(details!==null);verify(action!==null)
            tryVerify(()=>{const p=action.mapToItem(details,0,0);return p.y>=-1 && p.y+action.height<=details.height+1})
        }
        function test_store_platform_install_open_uninstall_and_cancel() {
            storeCatalog.entries=[{id:"android-example",name:"Android app",type:"android",category:"Video",publisher:"Example",description:"A remote-friendly Android app.",symbol:"A",accent:"#b34747",installed:false},{id:"web-example",name:"Chromium app",type:"web",category:"Video",publisher:"Example",description:"A Chromium web app.",symbol:"C",accent:"#347d9c",installed:false}]
            const store=createTemporaryObject(storeComponent,tests.parent);verify(store!==null)
            editor.visible=false;store.z=10000;waitForRendering(store)
            storeOpenSpy.target=store
            for(let platform=0;platform<2;++platform){
                store.choosePlatform(platform);compare(store.filtered.length,1)
                store.handleCommand(inputManager.navigateDown);store.handleCommand(inputManager.confirm)
                compare(store.confirmation,"install");store.handleCommand(inputManager.back);compare(store.selected.installed,false)
                store.handleCommand(inputManager.confirm);store.handleCommand(inputManager.navigateRight);store.handleCommand(inputManager.confirm)
                compare(store.selected.installed,true);const id=store.selectedId
                storeOpenSpy.clear();store.handleCommand(inputManager.confirm)
                compare(storeOpenSpy.count,1);compare(storeOpenSpy.signalArguments[0][0],id);compare(store.returnAppId,id)
                waitForRendering(store)
                if(studioFixture.artifacts)grabImage(store).save(studioFixture.artifacts+"/studio-test-store-"+(platform===0?"android":"chromium")+".png")
                store.handleCommand(inputManager.navigateDown);store.handleCommand(inputManager.confirm)
                compare(store.confirmation,"uninstall");store.handleCommand(inputManager.back);compare(store.selected.installed,true)
                store.handleCommand(inputManager.confirm);store.handleCommand(inputManager.navigateRight);store.handleCommand(inputManager.confirm)
                compare(store.selected.installed,false);compare(store.selectedId,id)
            }
            storeOpenSpy.target=null
        }
        SignalSpy {id:switchSpy;signalName:"clicked"}
        Component {id:ambientComponent;App.AmbientOverlay {}}
        QtObject {id:fakePlaylistCatalog;property var items:[{title:"Family films",playlistName:"Family films.xsp"}];property string status:"Kodi video playlists · 1 of 1"}
        QtObject {id:playlistProvider;property int refreshes:0;function playlistCatalog(){return fakePlaylistCatalog}function refresh(client){refreshes++}}
        Component {id:playlistInspector;App.StudioInspector {width:1280;height:720}}
        SignalSpy {id:ambientSpy;signalName:"dismissRequested"}
        function test_kodi_playlist_picker_is_remote_and_draft_only() {
            verify(editor.session.execute("newPage",{name:"Playlist room",preset:"blank"}),editor.session.errorString)
            const pageId=editor.session.document.pages[editor.session.document.pages.length-1].id
            verify(editor.session.execute("addResolved",{type:"library",placement:{x:0,y:0,width:4,height:2}},pageId),editor.session.errorString)
            editor.pageId=pageId;settle()
            const item=editor.page.items[0],inspector=createTemporaryObject(playlistInspector,tests.parent,{session:editor.session,page:editor.page,pageId:pageId,options:homeLayout.studioOptions,appearance:editor.page.appearance,selectedIds:[item.id],provider:playlistProvider})
            verify(inspector!==null);inspector.advanced=true
            const field={scope:"presentation",key:"libraryView",type:"choice",choices:["all","playlist"]}
            const before=JSON.stringify(editor.session.document)
            verify(inspector.set(field,"playlist",false));compare(inspector.current.presentation.libraryView,"playlist")
            const source={scope:"presentation",key:"libraryValue",type:"source"}
            inspector.activateField(source);compare(inspector.view,"playlistSource");compare(inspector.labels()[1],"Family films")
            const unchanged=JSON.stringify(editor.session.document)
            inspector.handleCommand(inputManager.back);compare(JSON.stringify(editor.session.document),unchanged)
            inspector.activateField(source);inspector.selected=0;inspector.handleCommand(inputManager.confirm);verify(playlistProvider.refreshes>0)
            inspector.handleCommand(inputManager.navigateDown);inspector.handleCommand(inputManager.confirm)
            compare(inspector.view,"fields");compare(inspector.current.presentation.libraryValue,"Family films.xsp")
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),unchanged)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before);compare(studioFixture.disk(),diskBaseline)
            inspector.activateField(source);compare(inspector.view,"choices")
            inspector.selected=inspector.choices.findIndex(c=>c.kind==="playlist");verify(inspector.selected>=0)
            inspector.handleCommand(inputManager.confirm);compare(inspector.view,"playlistSource")
            inspector.handleCommand(inputManager.navigateDown);inspector.handleCommand(inputManager.confirm)
            compare(inspector.current.presentation.libraryView,"playlist");compare(inspector.current.presentation.libraryValue,"Family films.xsp")
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_screensaver_covers_gallery_and_restores_selection() {
            fakeRegistry.apps=[{id:"one",name:"One",type:"desktop"},{id:"two",name:"Two",type:"web"},{id:"three",name:"Three",type:"desktop"}]
            const carousel=createTemporaryObject(carouselComponent,tests.parent)
            carousel.activate("__more__");carousel.handleCommand(inputManager.navigateRight)
            const gallery=findChild(carousel,"appCarouselGallery")
            tryCompare(gallery,"opened",true);compare(carousel.galleryId,"two")
            const ambient=createTemporaryObject(ambientComponent,tests.parent,{settings:{photoUrls:[],photoDirectory:"",mode:"dim",imageFit:"fit",shuffle:false,slideshowSeconds:30,showClock:false,showDate:false,showSystemHint:false}})
            verify(ambient!==null);ambientSpy.target=ambient;ambientSpy.clear()
            ambient.active=true;tryCompare(ambient,"opened",true)
            compare(ambient.parent,gallery.parent);verify(ambient.z>gallery.z)
            const image=grabImage(ambient.parent)
            compare(image.pixel(10,10),Qt.rgba(2/255,4/255,10/255,1))
            keyClick(Qt.Key_Left);compare(ambientSpy.count,1)
            ambient.active=false;tryCompare(ambient,"opened",false)
            compare(gallery.opened,true);compare(carousel.galleryId,"two")
            carousel.handleCommand(inputManager.back);ambientSpy.target=null
        }
        function test_more_apps_store_launch_and_return() {
            fakeRegistry.apps=[{id:"one",name:"One",type:"desktop"},{id:"two",name:"Two",type:"web"},{id:"three",name:"Three",type:"desktop"},{id:"registered-store",name:"App Store",type:"store"}]
            const carousel=createTemporaryObject(carouselComponent,tests.parent)
            launchSpy.target=carousel;launchSpy.clear()
            compare(carousel.eligible.length,3);compare(carousel.galleryApps.length,4)
            carousel.selectedId="__more__";carousel.activate("__more__")
            const gallery=findChild(carousel,"appCarouselGallery")
            tryCompare(gallery,"opened",true)
            for(let i=0;i<3;i++)carousel.handleCommand(inputManager.navigateRight)
            compare(carousel.galleryId,"registered-store")
            carousel.handleCommand(inputManager.confirm)
            compare(launchSpy.count,1);compare(launchSpy.signalArguments[0][0].appId,"registered-store")
            tryCompare(gallery,"opened",false)
            const state=carousel.focusState();verify(state.galleryOpen)
            carousel.restoreFocusState(state);tryCompare(gallery,"opened",true)
            compare(carousel.galleryId,"registered-store")
            carousel.collection={mode:"all",ids:[],showStore:false}
            compare(carousel.galleryApps.length,3);compare(carousel.galleryId,"one")
            carousel.handleCommand(inputManager.back);compare(carousel.selectedId,"__more__")
            launchSpy.target=null
        }
        function test_android_app_is_in_gallery_and_restores_its_card() {
            fakeRegistry.apps=[{id:"one",name:"One",type:"desktop"},{id:"smarttube",name:"SmartTube",type:"android"},{id:"three",name:"Three",type:"web"}]
            const carousel=createTemporaryObject(carouselComponent,tests.parent)
            launchSpy.target=carousel;launchSpy.clear()
            compare(carousel.eligible.length,3)
            carousel.activate("__more__")
            const gallery=findChild(carousel,"appCarouselGallery");tryCompare(gallery,"opened",true)
            carousel.handleCommand(inputManager.navigateRight);compare(carousel.galleryId,"smarttube")
            carousel.handleCommand(inputManager.confirm)
            compare(launchSpy.count,1);compare(launchSpy.signalArguments[0][0].appId,"smarttube")
            const state=carousel.focusState();verify(state.galleryOpen)
            carousel.restoreFocusState(state);tryCompare(gallery,"opened",true)
            compare(carousel.galleryId,"smarttube")
            carousel.handleCommand(inputManager.back);launchSpy.target=null
        }
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
        function test_app_collection_overflow_icon_remote_cancel_and_undo() {
            verify(editor.session.execute("composition",{value:Object.assign({},editor.page.composition,{grid:Object.assign({},editor.page.grid,{columns:8,rows:5})})}),editor.session.errorString)
            verify(editor.session.execute("addResolved",{type:"app-bar",placement:{x:1,y:0,width:6,height:2}}),editor.session.errorString)
            const appBar=editor.items.find(i=>i.family==="app-bar")
            verify(!!appBar);editor.selectedId=appBar.id;settle()
            compare(editor.current.presentation.icon,"grid")
            fakeRegistry.apps=[{id:"one",name:"One",type:"web"},{id:"two",name:"Two",type:"web"},{id:"three",name:"Three",type:"web"}]
            const carousel=createTemporaryObject(carouselComponent,tests.parent)
            carousel.style=Qt.binding(()=>Object.assign({},editor && editor.current?editor.current.presentation:{},{variant:"orb",layout:{visibleColumns:2}}))
            tryVerify(()=>findChild(carousel,"appCarouselCard-__more__")!==null)
            const more=findChild(carousel,"appCarouselCard-__more__")
            compare(more.item.presentation.icon,"grid")
            const before=JSON.stringify(editor.session.document),history=editor.session.changeCount
            editor.openInspector(false);settle()
            const inspector=findChild(editor,"studioInspector")
            if(inspector.collapsed)command(inputManager.confirm)
            command(inputManager.confirm);compare(inspector.view,"fields")
            const field=inspector.fields.findIndex(f=>f.key==="icon")
            verify(field>=0);compare(inspector.fields[field].label,"More apps icon")
            for(let i=0;i<field;++i)command(inputManager.navigateDown)
            command(inputManager.confirm);compare(inspector.view,"choices")
            const clock=inspector.choices.findIndex(c=>c.value==="clock");verify(clock>=0)
            for(let i=0;i<clock;++i)command(inputManager.navigateDown)
            command(inputManager.back);compare(JSON.stringify(editor.session.document),before)
            compare(inspector.selected,field)
            command(inputManager.confirm)
            for(let i=0;i<clock;++i)command(inputManager.navigateDown)
            command(inputManager.confirm);settle()
            compare(editor.current.presentation.icon,"clock");compare(more.item.presentation.icon,"clock")
            compare(editor.session.changeCount,history+1)
            verify(editor.session.undo());settle()
            compare(JSON.stringify(editor.session.document),before);compare(more.item.presentation.icon,"grid")
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_shapes_have_authored_bounds_labels_and_no_runtime_focus() {
            const shape={id:"shape",name:"Editor name",componentType:"shape",widgetType:"shape",presentation:{shape:"ellipse",surfaceColor:"#ff0000",fillMode:"gradient",fillEndColor:"#0000ff",fillDirection:"horizontal",visibleLabel:"Visible caption",labelPosition:"hidden"}}
            const card=createTemporaryObject(shapeCardComponent,tests.parent,{item:shape,selected:true})
            verify(card!==null);verify(waitForRendering(card));verify(card.ellipse);compare(card.interactive,false)
            compare(card.title,"Visible caption");compare(card.item.name,"Editor name")
            const body=findChild(card,"cardSurface");compare(body.width,240);compare(body.height,240)
            compare(card.ellipseScaleY,0.5);compare(card.focusFrame.width,246);compare(card.focusFrame.height,123)
            verify(body.gradient!==null)
            const host=createTemporaryObject(menuHomeComponent,tests.parent,{items:[shape]})
            verify(host!==null);compare(host.focusable(shape),false)
            host.editMode=true;compare(host.focusable(shape),true)
            const text=createTemporaryObject(shapeCardComponent,tests.parent,{item:{id:"body",name:"Independent editor name",componentType:"text",widgetType:"text",presentation:{bodyText:"Body content",visibleLabel:"Caption",labelPosition:"hidden"}}})
            compare(text.title,"Caption");compare(findChild(text,"componentBodyText").text,"Body content")
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_visual_app_picker_order_cancel_missing_and_pointer_parity() {
            const apps=["one","two","three"].map(id=>({name:id,action:{type:"launch-app",appId:id}}))
            const picker=createTemporaryObject(collectionPickerComponent,tests.parent,{apps:apps})
            verify(picker!==null);collectionPickerSpy.target=picker;collectionPickerSpy.clear()
            const initial=["three","one","missing"]
            picker.begin(initial);picker.control=3;picker.handleCommand(inputManager.confirm)
            verify(picker.selectedView);verify(picker.entries[2].missing)
            picker.handleCommand(inputManager.confirm);compare(picker.movingId,"three")
            picker.handleCommand(inputManager.navigateRight);compare(picker.selection.join(","),"one,three,missing")
            picker.handleCommand(inputManager.back);compare(picker.selection.join(","),initial.join(","))
            picker.index=0;picker.handleCommand(inputManager.confirm);picker.handleCommand(inputManager.navigateRight);picker.handleCommand(inputManager.confirm)
            compare(picker.movingId,"");picker.toolsFocused=true;picker.control=5;picker.handleCommand(inputManager.confirm)
            compare(collectionPickerSpy.count,1);const remote=JSON.stringify(collectionPickerSpy.signalArguments[0][1])
            compare(remote,JSON.stringify(["one","three","missing"]))
            picker.begin(initial)
            editor.visible=false;picker.z=10000;verify(waitForRendering(picker))
            mouseClick(findChild(picker,"studioAppPickerTool-3"))
            verify(waitForRendering(findChild(picker,"studioAppChoice-three")))
            mouseClick(findChild(picker,"studioAppChoice-three"));compare(picker.movingId,"three")
            mouseClick(findChild(picker,"studioAppChoice-one"));compare(picker.movingId,"")
            mouseClick(findChild(picker,"studioAppPickerTool-5"))
            compare(collectionPickerSpy.count,2);compare(JSON.stringify(collectionPickerSpy.signalArguments[1][1]),remote)
            picker.apps=Array.from({length:129},(_,i)=>({name:"App "+i,action:{type:"launch-app",appId:"app-"+i}}))
            picker.begin(initial);picker.control=1;picker.handleCommand(inputManager.confirm)
            compare(picker.selection.join(","),initial.join(","));verify(picker.message.length>0)
            compare(studioFixture.disk(),diskBaseline);collectionPickerSpy.target=null
        }
        function test_android_app_selection_catalog_inspector_and_dynamic_registry() {
            const actions=[
                {name:"Desktop app",appType:"desktop",appSection:"TV",action:{type:"launch-app",appId:"desktop-app"}},
                {name:"Android app",appType:"android",appSection:"TV",action:{type:"launch-app",appId:"android-app"}},
                {name:"Web app",appType:"web",appSection:"Music",action:{type:"launch-app",appId:"web-app"}},
                {name:"Mock app",appType:"mock",appSection:"TV",action:{type:"launch-app",appId:"mock-app"}},
                {name:"Store",appType:"store",appSection:"TV",action:{type:"launch-app",appId:"store"}}
            ]
            const fixtureSession={availableActions:actions,document:{},componentPrototype:(type,configuration)=>editor.session.componentPrototype(type,configuration)}
            const catalog=createTemporaryObject(appPickerCatalogComponent,tests.parent,{session:fixtureSession,options:homeLayout.studioOptions,appearance:editor.page.appearance})
            const inspector=createTemporaryObject(appPickerInspectorComponent,tests.parent,{session:fixtureSession,options:homeLayout.studioOptions,appearance:editor.page.appearance,page:{items:[{id:"apps",appCollection:{section:"TV"}}]},selectedIds:["apps"]})
            verify(catalog!==null);verify(inspector!==null)
            compare(catalog.apps.map(a=>a.action.appId).join(","),"desktop-app,android-app,web-app")
            compare(inspector.apps.map(a=>a.action.appId).join(","),"desktop-app,android-app")
            const family=catalog.families.findIndex(f=>f.id==="app-bar")
            verify(family>=0);catalog.chooseFamily(family)
            verify(catalog.appPicker!==null);catalog.appPicker.toolsFocused=false;catalog.appPicker.index=1
            catalog.handleCommand(inputManager.confirm)
            verify(catalog.appPicker.selection.indexOf("android-app")<0)
            catalog.handleCommand(inputManager.confirm)
            verify(catalog.appPicker.selection.indexOf("android-app")>=0)
            appPickerSpy.target=catalog;appPickerSpy.clear()
            catalog.appPicker.toolsFocused=true;catalog.appPicker.control=5;catalog.handleCommand(inputManager.confirm)
            compare(appPickerSpy.count,1)
            verify(appPickerSpy.signalArguments[0][1].appCollection.ids.indexOf("android-app")>=0)
            inspector.appSelection=[];inspector.view="apps";verify(inspector.appPicker!==null);inspector.appPicker.toolsFocused=false;inspector.appPicker.index=1
            inspector.handleCommand(inputManager.confirm)
            compare(inspector.appPicker.selection.join(","),"android-app")
            // Registry refresh/removal must update both selectors without
            // silently replacing the user's explicit selected identities.
            const remaining={availableActions:actions.filter(a=>a.action.appId!=="android-app"),document:{}}
            catalog.session=remaining;inspector.session=remaining
            compare(catalog.apps.length,2);compare(inspector.apps.length,1)
            compare(inspector.appPicker.selection.join(","),"android-app")
            compare(studioFixture.disk(),diskBaseline)
            appPickerSpy.target=null
        }
        function test_open_gallery_registry_changes_preserve_scope_and_recover_focus() {
            fakeRegistry.apps=[{id:"one",name:"One",type:"desktop"},{id:"two",name:"Two",type:"android"},{id:"three",name:"Three",type:"web"}]
            const carousel=createTemporaryObject(carouselComponent,tests.parent,{collection:{mode:"all",ids:[],showStore:false}})
            verify(carousel!==null);launchSpy.target=carousel;launchSpy.clear()
            carousel.selectedId="__more__";carousel.handleCommand(inputManager.confirm)
            const gallery=findChild(carousel,"appCarouselGallery")
            tryCompare(gallery,"opened",true)
            carousel.handleCommand(inputManager.navigateRight);compare(carousel.galleryId,"two")
            // A registry refresh changes live membership without authoring nodes.
            fakeRegistry.apps=[fakeRegistry.apps[2],fakeRegistry.apps[1],fakeRegistry.apps[0],{id:"four",name:"Four",type:"desktop"},{id:"internal",name:"Internal",type:"navigation"}]
            compare(carousel.galleryApps.map(a=>a.id).join(","),"three,two,one,four")
            compare(carousel.galleryId,"two");verify(gallery.opened)
            fakeRegistry.apps=fakeRegistry.apps.filter(a=>a.id!=="two")
            verify(carousel.galleryApps.some(a=>a.id===carousel.galleryId));verify(gallery.opened)
            carousel.handleCommand(inputManager.back);compare(carousel.selectedId,"__more__")
            // Selected mode keeps authored order and a repairable missing member.
            const membership={mode:"selected",ids:["two","one"],showStore:false}
            carousel.collection=membership;carousel.style={layout:{visibleColumns:1}}
            carousel.activate("__more__");tryCompare(gallery,"opened",true)
            compare(carousel.galleryApps.map(a=>a.id).join(","),"two,one")
            carousel.handleCommand(inputManager.confirm);compare(launchSpy.count,0);verify(carousel.message.length>0)
            fakeRegistry.apps=fakeRegistry.apps.concat([{id:"two",name:"Two restored",type:"android"}])
            compare(carousel.galleryId,"two");verify(!carousel.galleryApps[0].unavailableReason)
            carousel.handleCommand(inputManager.confirm);compare(launchSpy.count,1)
            compare(launchSpy.signalArguments[0][0].appId,"two")
            const returned=carousel.focusState();verify(returned.galleryOpen)
            carousel.restoreFocusState(returned);tryCompare(gallery,"opened",true);compare(carousel.galleryId,"two")
            carousel.handleCommand(inputManager.back);compare(carousel.selectedId,"__more__")
            compare(JSON.stringify(carousel.collection),JSON.stringify(membership))
            compare(studioFixture.disk(),diskBaseline);launchSpy.target=null
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
            compare(carousel.eligible.length,2);compare(carousel.eligible[0].id,"three");verify(!!carousel.eligible[0].unavailableReason)
            carousel.activate("three");compare(launchSpy.count,1);verify(carousel.message.length>0)
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
            compare(inspector.activeField.control,"stepper");verify(findChild(inspector,"studioScalarStepper").visible)
            settle();if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-history-count.png")
            const increase=findChild(inspector,"studioScalarStep-2"),decrease=findChild(inspector,"studioScalarStep-0")
            verify(increase!==null && decrease!==null);mouseClick(increase,increase.width/2,increase.height/2);compare(inspector.scalarDraft,22)
            mouseClick(decrease,decrease.width/2,decrease.height/2);compare(inspector.scalarDraft,21)
            for(let i=0;i<35;++i)command(inputManager.navigateRight)
            compare(inspector.scalarDraft,50);command(inputManager.navigateRight);compare(inspector.scalarDraft,50)
            for(let i=0;i<55;++i)command(inputManager.navigateLeft)
            compare(inspector.scalarDraft,1);command(inputManager.navigateLeft);compare(inspector.scalarDraft,1)
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
            // Explicit app-opening opt-in is private until Done; Back retains the source list.
            const nativeSession=editor.session
            inspector.session={availableActions:[{name:"Native app",appType:"desktop",action:{type:"launch-app",appId:"native-app"}},{name:"Web app",appType:"web",action:{type:"launch-app",appId:"web-app"}},{name:"Android app",appType:"android",action:{type:"launch-app",appId:"android-app"}}],execute:(op,args,pageId)=>nativeSession.execute(op,args,pageId),document:nativeSession.document,errorString:""}
            inspector.tab=0;inspector.view="fields";settle()
            inspector.selected=inspector.fields.findIndex(f=>f.key==="historyApps");verify(inspector.selected>=0)
            command(inputManager.confirm);compare(inspector.view,"historyApps");settle()
            verify(inspector.appPicker!==null);compare(inspector.appPicker.selectionLimit,50)
            compare(inspector.appPicker.apps.length,3);compare(inspector.appPicker.heading,"App opening sources");verify(inspector.appPicker.description.indexOf("Service viewing history")>=0)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-history-sources.png")
            command(inputManager.navigateDown);command(inputManager.confirm)
            command(inputManager.back);compare(JSON.stringify(editor.session.document),added)
            command(inputManager.confirm);settle()
            {
                const appId=inspector.appPicker.apps[0].action.appId
                command(inputManager.navigateDown);command(inputManager.confirm);command(inputManager.menu)
                command(inputManager.navigateRight);command(inputManager.navigateRight);command(inputManager.confirm)
                compare(editor.current.presentation.historyApps.length,1);compare(editor.current.presentation.historyApps[0],appId)
                verify(editor.session.undo());compare(JSON.stringify(editor.session.document),added)
            }
            inspector.session=nativeSession
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
