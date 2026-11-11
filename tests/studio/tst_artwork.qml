import QtQuick
import QtTest
import "../../qml" as App
Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioArtwork";when:windowShown
        property var editor:null
        property string diskBaseline:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        Component {id:shapeCardComponent;App.HomeCard {width:240;height:120;appearance:homeLayout.appearance}}
        Component {id:menuHomeComponent;App.HomeScreen {width:1280;height:720;sections:[];brandVisible:false;backgroundStart:"black";backgroundEnd:"black";tileRoundness:0.2;internetAvailable:false;localNetworkAvailable:false;hasContextualSources:false}}
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/);verify(studioFixture.reset(false,false));diskBaseline=studioFixture.disk();editor=createTemporaryObject(editorComponent,tests.parent);verify(editor!==null);editor.show("","cell-0");editor.toolbarFocus=false;tryVerify(()=>editor.current!==null)}
        function cleanup(){if(editor)editor.close();editor=null}
        function settle(){let ready=false;Qt.callLater(()=>Qt.callLater(()=>ready=true));tryVerify(()=>ready)}
        Component {id:artworkPickerComponent;App.StudioArtworkPicker {width:1280;height:720}}
        SignalSpy {id:artworkPickedSpy;signalName:"picked"}
        function test_catalog_addition_clears_header_origin_before_component_edit(){
            editor.headerSelection="title";editor.toolbarFocus=true
            editor.startAddition("text",{});settle();compare(editor.mode,"move");compare(editor.headerSelection,"")
            editor.handleCommand(inputManager.confirm);settle();compare(editor.mode,"canvas");compare(editor.current.componentType,"text")
            const id=editor.selectedId;editor.handleCommand(inputManager.confirm);compare(editor.mode,"context")
            editor.handleCommand(inputManager.navigateDown);editor.handleCommand(inputManager.confirm);settle();compare(editor.mode,"inspector")
            const inspector=findChild(editor,"studioInspector");verify(inspector!==null);compare(inspector.pageMode,false);compare(editor.selectedId,id)
                if(inspector.view==="simple")inspector.showDetails()
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_container_move_preview_preserves_the_authored_heading(){
            const proposal=editor.session.additionProposal("container",{x:0,y:1,width:2,height:2},"cancel","","","",{name:"Container heading",presentation:{showHeading:true,layout:{schemaVersion:1,mode:"grid",columns:2,rows:1,gap:12,padding:0}}},false)
            verify(proposal.valid,proposal.reason)
            verify(editor.session.execute("batch",{commands:proposal.commands}),editor.session.errorString)
            editor.selectedId=proposal.id;settle()
            const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            editor.beginGesture(false);settle();verify(editor.collisionPreview.valid)
            const label=findChild(editor,"studioProposalName-"+proposal.id)
            verify(label!==null);compare(label.visible,false)
            verify(editor.footerText.indexOf("Container heading")>=0)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-container-heading-preview.png")
            editor.handleCommand(inputManager.back);settle()
            compare(JSON.stringify(editor.session.document),before);compare(editor.session.changeCount,count)
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_empty_artwork_folder_keeps_a_visible_recovery_focus(){
            const emptySession={errorString:"",browseArtwork:function(){return {directory:"/fixture",parent:"/fixture",home:"/fixture",pictures:"/fixture",entries:[],page:0,pages:1}}}
            const picker=createTemporaryObject(artworkPickerComponent,tests.parent,{session:emptySession,appearance:homeLayout.studioAppearance});verify(picker!==null)
            compare(picker.entries.length,0);compare(picker.selected,0)
            const list=findChild(picker,"studioArtworkFiles");verify(list!==null);tryVerify(()=>list.currentItem!==null);compare(list.currentIndex,0)
            picker.handleCommand(inputManager.confirm);compare(picker.selected,0);compare(studioFixture.disk(),diskBaseline)
        }
        function test_artwork_browser_remote_pointer_import_and_cancel(){
            const source=studioFixture.artworkFixture(),folder=source.slice(0,source.lastIndexOf("/")),before=JSON.stringify(editor.session.document)
            const picker=createTemporaryObject(artworkPickerComponent,tests.parent,{session:editor.session,appearance:homeLayout.studioAppearance});verify(picker!==null)
            editor.visible=false;artworkPickedSpy.target=picker;artworkPickedSpy.clear();verify(picker.browse(folder,0));compare(picker.entries.length,1)
            picker.handleCommand(inputManager.confirm);verify(picker.candidate.endsWith("animation.gif"));verify(picker.preview.startsWith("data:image/png;base64,"))
            picker.handleCommand(inputManager.back);compare(picker.candidate,"");compare(artworkPickedSpy.count,0)
            picker.handleCommand(inputManager.confirm);picker.handleCommand(inputManager.confirm);compare(artworkPickedSpy.count,1);compare(artworkPickedSpy.signalArguments[0][0],source)
            verify(picker.browse(folder,0));picker.handleCommand(inputManager.confirm);wait(30)
            const list=findChild(picker,"studioArtworkFiles");verify(list!==null);verify(list.currentItem!==null)
            mouseClick(list.currentItem,list.currentItem.width/2,list.currentItem.height/2);compare(artworkPickedSpy.count,2);compare(artworkPickedSpy.signalArguments[1][0],source)
            compare(JSON.stringify(editor.session.document),before);compare(studioFixture.disk(),diskBaseline)
        }
        Component {id:realMotionImageComponent;App.ArtworkImage {width:160;height:100;z:20000}}
        Component {id:iconStylesComponent;Rectangle {
            width:1120;height:580;z:20000;color:"#e8ced8"
            property var apps:studioFixture.iconAppsFixture()
            property var palette:({backgroundStart:"#e8ced8",surfaceColor:"#f8eaf0",textColor:"#352b3a",focusColor:"#80526e",iconColor:"#80526e",fontFamily:"DejaVu Sans",typographyScale:1})
            Repeater {model:["signature","tiles","rail"]
                delegate:App.PageAppCarousel {required property string modelData;required property int index;x:24;y:24+index*175;width:1072;height:modelData==="signature"?150:100;registry:({apps:parent.apps});highlighted:true;appearance:parent.palette;style:({appIconStyle:modelData,logoMode:"cutout",layout:{visibleColumns:3},cardGap:24})}
            }
        }}
        function test_three_icon_styles_render_and_keep_selected_glow(){
            editor.visible=false;const scene=createTemporaryObject(iconStylesComponent,tests.parent);verify(scene!==null);settle();wait(160);verify(waitForRendering(scene))
            if(studioFixture.artifacts)grabImage(scene).save(studioFixture.artifacts+"/studio-test-icon-styles-pastel.png")
            scene.color="#17191f";scene.palette={backgroundStart:"#17191f",surfaceColor:"#252831",textColor:"#f5f0f4",focusColor:"#efbfd8",iconColor:"#efbfd8",fontFamily:"DejaVu Sans",typographyScale:1};settle();wait(160);verify(waitForRendering(scene))
            if(studioFixture.artifacts)grabImage(scene).save(studioFixture.artifacts+"/studio-test-icon-styles-dark.png")
            compare(studioFixture.disk(),diskBaseline)
        }
        Component {id:tintedIconComponent;Rectangle {width:80;height:80;color:"#112233";z:20000
            App.ArtworkImage {objectName:"tintedIcon";anchors.fill:parent;fit:"contain";tintColor:"#00ff00";outlineWidth:4;outlineColor:"#ffffff"
                source:"data:image/svg+xml,"+encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="80" height="80"><path fill="red" fill-rule="evenodd" d="M16 16H64V64H16Z M32 32H48V48H32Z"/></svg>')}
        }}
        function test_monochrome_preserves_holes_edges_and_focus_colour(){
            editor.visible=false
            const card=createTemporaryObject(tintedIconComponent,tests.parent);verify(card!==null)
            const image=findChild(card,"tintedIcon");tryCompare(image,"status",Image.Ready);tryVerify(()=>image.isImageLoaded(image.displaySource));image.requestPaint();wait(80);verify(waitForRendering(image))
            const first=grabImage(card)
            compare(first.pixel(4,40),Qt.rgba(17/255,34/255,51/255,1))
            compare(first.pixel(40,40),first.pixel(4,40))
            compare(first.pixel(24,40),Qt.rgba(0,1,0,1))
            compare(first.pixel(14,40),Qt.rgba(1,1,1,1))
            image.tintColor="#0000ff";image.requestPaint();wait(80);verify(waitForRendering(image))
            const second=grabImage(card);compare(second.pixel(24,40),Qt.rgba(0,0,1,1));compare(second.pixel(14,40),first.pixel(14,40));compare(second.pixel(40,40),first.pixel(40,40))
            image.glowWidth=8;image.requestPaint();wait(80);verify(waitForRendering(image));const glowing=grabImage(card)
            verify(glowing.pixel(8,40)!==second.pixel(8,40),"Selected glow must extend beyond the hard contour")
            image.glowWidth=0;image.requestPaint();wait(80);verify(waitForRendering(image));compare(grabImage(card).pixel(8,40),second.pixel(8,40))
            if(studioFixture.artifacts)second.save(studioFixture.artifacts+"/studio-test-monochrome-alpha-focus.png")
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_icon_cleanup_is_remote_reversible_and_private(){
            const source=studioFixture.iconArtworkFixture(),folder=source.slice(0,source.lastIndexOf("/")),before=JSON.stringify(editor.session.document)
            editor.visible=false
            const picker=createTemporaryObject(artworkPickerComponent,tests.parent,{session:editor.session,appearance:homeLayout.studioAppearance});verify(picker!==null)
            artworkPickedSpy.target=picker;artworkPickedSpy.clear();verify(picker.browse(folder,0));compare(picker.entries.length,1)
            picker.handleCommand(inputManager.confirm);compare(picker.candidate,source);const original=picker.preview
            picker.handleCommand(inputManager.navigateDown);picker.handleCommand(inputManager.navigateDown);picker.handleCommand(inputManager.confirm)
            compare(picker.cleanEnabled,true);verify(picker.cleanedSource.startsWith("file:"));verify(picker.preview!==original);compare(picker.previewSurfaces.length,3)
            compare(artworkPickedSpy.count,0);compare(JSON.stringify(editor.session.document),before);compare(studioFixture.disk(),diskBaseline)
            if(studioFixture.artifacts)grabImage(picker).save(studioFixture.artifacts+"/studio-test-icon-cleanup-preview.png")
            picker.handleCommand(inputManager.confirm);compare(picker.cleanEnabled,false);compare(picker.preview,original)
            picker.handleCommand(inputManager.back);compare(picker.candidate,"");compare(artworkPickedSpy.count,0);compare(studioFixture.disk(),diskBaseline)
        }
        Component {id:framingComponent;App.StudioImageFraming {width:1280;height:720}}
        SignalSpy {id:framingSpy;signalName:"previewed"}
        function test_text_body_style_is_independent_plain_and_inherited(){
            const item={id:"text-style",name:"Editor name",componentType:"text",widgetType:"text",presentation:{bodyText:"<b>Plain body</b> and words",bodySize:30,bodyBold:true,bodyAlign:"right",bodyWrap:false,bodyLines:3,bodyPadding:8,bodyLineHeight:1.5,visibleLabel:"Separate caption",labelSize:12,labelBold:false,labelPosition:"below",labelColor:"#00ff00"}}
            const card=createTemporaryObject(shapeCardComponent,tests.parent,{item:item,appearance:{textColor:"#123456",fontFamily:"DejaVu Sans",typographyScale:1}});verify(card!==null)
            const body=findChild(card,"componentBodyText");compare(body.text,item.presentation.bodyText);compare(body.textFormat,Text.PlainText)
            compare(body.font.pixelSize,30);compare(body.font.bold,true);compare(body.horizontalAlignment,Text.AlignRight);compare(body.wrapMode,Text.NoWrap);compare(body.maximumLineCount,3);compare(body.lineHeight,1.5);compare(body.color,"#123456");compare(body.x,8)
            compare(card.title,"Separate caption");compare(card.interactive,false)
            card.appearance={textColor:"#654321",fontFamily:"DejaVu Sans",typographyScale:1};compare(body.color,"#654321")
            card.item=Object.assign({},item,{presentation:Object.assign({},item.presentation,{bodyColor:"#abcdef",bodyWrap:true,bodyAlign:"center"})})
            compare(body.color,"#abcdef");compare(body.wrapMode,Text.WordWrap);compare(body.horizontalAlignment,Text.AlignHCenter)
            const host=createTemporaryObject(menuHomeComponent,tests.parent,{items:[item]});compare(host.focusable(item),false);host.items=[item,{id:"reachable",componentType:"button",name:"Home",action:{type:"root-page"}}];host.restoreFocus(item.id);compare(host.focusedItemId,"reachable");compare(host.recoveryFocused,false);host.editMode=true;compare(host.focusable(item),true)
        }
        function test_real_gif_import_browser_framing_render_and_cancel(){
            const path=studioFixture.artworkFixture();verify(path.endsWith(".gif"));const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            const relative=editor.session.stageArtwork(path);verify(relative.endsWith(".gif"),editor.session.errorString)
            const resolver=editor.session.model,still=resolver.resolveArtwork(relative);verify(still.startsWith("data:image/png;base64,"))
            editor.visible=false // Capture the actual canvas, without the editor sibling covering its window region.
            const image=createTemporaryObject(realMotionImageComponent,tests.parent,{source:still,managedAsset:relative,animationResolver:resolver});verify(image!==null)
            tryCompare(image,"status",Image.Ready);tryVerify(()=>image.isImageLoaded(image.displaySource));image.requestPaint();wait(80);verify(waitForRendering(image));compare(image.animationAttempted,false)
            const firstCanvas=image.toDataURL();const first=grabImage(image);image.motionEnabled=true;image.advanceAnimation();compare(image.animation.frames.length,2)
            verify(image.animation.frames[0]!==image.animation.frames[1],"Fixture decodes to two different PNG frames");image.advanceAnimation();compare(image.animationFrame,1);tryVerify(()=>image.isImageLoaded(image.displaySource));image.requestPaint();wait(80);verify(waitForRendering(image));const second=grabImage(image)
            if(studioFixture.artifacts){first.save(studioFixture.artifacts+"/studio-test-gif-first.png");second.save(studioFixture.artifacts+"/studio-test-gif-second.png")}
            verify(firstCanvas!==image.toDataURL(),"Canvas backing image must change between real GIF frames");verify(!first.equals(second),"Real decoded GIF frames render different pixels")
            image.reducedMotion=true;compare(image.animation.frames,undefined);compare(image.animationFrame,0)
            image.reducedMotion=false;image.x=10000;image.advanceAnimation();compare(image.leasedResolver,null);image.x=0;image.advanceAnimation();verify(image.leasedResolver!==null);image.visible=false;compare(image.leasedResolver,null)
            const frame=createTemporaryObject(framingComponent,tests.parent,{z:20001,source:still,appearance:homeLayout.studioAppearance,original:{artFit:"cover",artFocalX:.5,artFocalY:.5,artZoom:1}});verify(frame!==null)
            const output=findChild(frame,"studioFramingPreview");tryCompare(output,"status",Image.Ready);tryVerify(()=>output.isImageLoaded(output.displaySource));output.requestPaint();wait(80);verify(waitForRendering(frame))
            framingSpy.target=frame;framingSpy.clear();frame.handleCommand(inputManager.navigateRight);compare(frame.draft.artFocalX,.55)
            frame.control=4;frame.handleCommand(inputManager.navigateRight);compare(frame.draft.artZoom,1.1);verify(framingSpy.count>=2)
            frame.control=5;frame.handleCommand(inputManager.confirm);compare(frame.draft.artAspectLocked,false)
            frame.begin();const target=findChild(frame,"studioFocalTarget");verify(target!==null);const targetParent=target.parent;mouseClick(target,(target.width-targetParent.renderedWidth)/2+targetParent.renderedWidth*.55,target.height/2);compare(frame.draft.artFocalX,.55);compare(frame.draft.artFocalY,.5)
            wait(30);verify(waitForRendering(frame));const fit=findChild(frame,"studioFramingFit"),fill=findChild(frame,"studioFramingFill");verify(fit!==null && fill!==null);verify(fit.visible);mouseClick(fit,fit.width/2,fit.height/2);compare(frame.draft.artFit,"contain");frame.control=1;frame.handleCommand(inputManager.navigateLeft);compare(frame.draft.artFit,"cover");mouseClick(fill,fill.width/2,fill.height/2);compare(frame.draft.artFit,"cover")
            compare(JSON.stringify(editor.session.document),before);compare(editor.session.changeCount,count);compare(studioFixture.disk(),diskBaseline)
            if(studioFixture.artifacts)grabImage(frame).save(studioFixture.artifacts+"/studio-test-image-framing.png")
        }
        function test_framing_uses_real_component_geometry_label_and_opacity(){
            const relative=editor.session.stageArtwork(studioFixture.artworkFixture()),source=editor.session.model.resolveArtwork(relative)
            const item={id:"portrait",componentType:"image",widgetType:"image",name:"Editor identity",opacity:.6,logoSource:source,presentation:{variant:"image",visibleLabel:"Poster caption",labelPosition:"below",radius:24,labelSize:20,artFit:"cover"}}
            editor.visible=false
            const frame=createTemporaryObject(framingComponent,tests.parent,{z:20001,source:source,original:item.presentation,componentItem:item,componentAppearance:homeLayout.appearance,targetGeometry:{width:240,height:360,unit:.9},appearance:homeLayout.studioAppearance});verify(frame!==null)
            const card=findChild(frame,"studioFramingCard");verify(card!==null)
            const actual=createTemporaryObject(shapeCardComponent,tests.parent,{visible:false,item:item,width:240,height:360,unit:.9,appearance:homeLayout.appearance});verify(actual!==null)
            compare(card.width,actual.width);compare(card.height,actual.height);compare(card.unit,actual.unit);compare(card.title,actual.title);compare(card.bodyHeight,actual.bodyHeight);compare(card.corner,actual.corner);compare(card.opacity,.6)
            tryCompare(frame,"sourceReady",true);frame.control=4;frame.handleCommand(inputManager.navigateRight);compare(frame.draft.artZoom,1.1);tryVerify(()=>card.style.artZoom===1.1)
            verify(card.bodyHeight<card.height,"The independent label consumes the same real component space")
            wait(80);verify(waitForRendering(frame));if(studioFixture.artifacts)grabImage(frame).save(studioFixture.artifacts+"/studio-test-framing-portrait.png")
            compare(studioFixture.disk(),diskBaseline)
        }
        function test_inspector_image_framing_is_one_transaction_and_cancel(){
            verify(editor.session.execute("addResolved",{type:"image",placement:{x:1,y:0,width:3,height:2}}),editor.session.errorString)
            const added=editor.items.find(i=>i.componentType==="image");verify(added!==undefined);editor.selectedId=added.id;settle();editor.openInspector(false);findChild(editor,"studioInspector").showDetails();settle()
            const inspector=findChild(editor,"studioInspector");verify(inspector!==null)
                if(inspector.view==="simple")inspector.showDetails()
            const relative=editor.session.stageArtwork(studioFixture.artworkFixture());verify(relative.endsWith(".gif"));verify(editor.session.execute("patch",{id:added.id,value:{customLogo:relative}}));settle()
            const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            const crop=homeLayout.studioOptions.inspector.fields.find(f=>f.type==="crop");verify(!!crop)
            inspector.activateField(crop);compare(inspector.view,"framing");settle()
            inspector.acceptFraming();compare(inspector.view,"fields");compare(editor.session.changeCount,count);inspector.activateField(crop);settle()
            inspector.framingPreview({artFocalX:.25,artZoom:1.5});verify(editor.session.proposalModel!==null);compare(editor.session.changeCount,count)
            inspector.handleCommand(inputManager.back);settle();compare(JSON.stringify(editor.session.document),before);compare(editor.session.changeCount,count)
            inspector.activateField(crop);settle();inspector.framingPreview({artFocalX:.25,artZoom:1.5});inspector.acceptFraming();settle()
            compare(editor.session.changeCount,count+1);compare(editor.current.presentation.artZoom,1.5);verify(editor.session.undo());settle();compare(JSON.stringify(editor.session.document),before);compare(studioFixture.disk(),diskBaseline)
        }
    }
}
