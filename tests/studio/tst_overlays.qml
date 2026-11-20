import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioOverlays";when:windowShown
        property var editor:null
        property string diskBaseline:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/)}
        function cleanup(){if(editor){settle();editor.close();settle()}editor=null}
        function settle(){let ready=false;Qt.callLater(()=>Qt.callLater(()=>{ready=true}));tryVerify(()=>ready)}
        function command(value){editor.handleCommand(value)}
        function document(){return JSON.stringify(editor.session.document)}
        function scenario(name,body){try{body()}catch(error){if(editor && studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-"+name+".png");throw error}}
        function givenPage(full,mixed,reduced){
            verify(studioFixture.reset(full || false,false));diskBaseline=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,tests.parent);verify(editor !== null)
            editor.show("","cell-0");editor.toolbarFocus=false
            if(mixed){verify(editor.session.execute("remove",{id:"cell-1"}));verify(editor.session.execute("place",{placements:[{id:"cell-0",placement:{x:0,y:0,width:2,height:1}}]}))}
            verify(editor.session.execute("appearance",{value:{motionEnabled:!reduced}}))
            settle();compare(editor.page.appearance.motionEnabled,!reduced)
        }
        function beginMove(){command(inputManager.confirm);compare(editor.mode,"context");command(inputManager.confirm);compare(editor.mode,"move");settle()}
        function dragRight(release){
            const hit=findChild(editor,"studioHit-cell-0"),preview=findChild(editor,"studioPreview"),metrics=preview.gridMetricsFor("","")
            const from=hit.mapToItem(editor,hit.width/2,hit.height/2),to=Qt.point(from.x+metrics.cellW+metrics.gap,from.y)
            mousePress(editor,from.x,from.y);mouseMove(editor,to.x,to.y)
            compare(editor.mode,"move");compare(editor.candidate.x,1)
            if(release)mouseRelease(editor,to.x,to.y)
            return to
        }
        function acceptGrid(){editor.choice=6;command(inputManager.confirm);compare(editor.mode,"conversionPreview");tryVerify(()=>!editor.gridGeometryPending);command(inputManager.confirm);compare(editor.mode,"canvas")}
        function test_overlap_control_is_private_cancelable_and_pointer_remote_equivalent(){
            scenario("policy-overlap-control",()=>{
                givenPage(false,false,false)
                const before=document(),count=editor.session.changeCount
                editor.beginGrid();command(inputManager.navigateUp);compare(editor.choice,9)
                command(inputManager.confirm);verify(editor.gridDraft.overlap);verify(editor.page.grid.overlap)
                compare(document(),before);compare(editor.session.changeCount,count)
                settle();if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-policy-overlap-control.png")
                command(inputManager.back);compare(document(),before)
                editor.beginGrid();command(inputManager.navigateUp);command(inputManager.confirm);acceptGrid()
                compare(editor.session.changeCount,count+1);const remote=document();verify(editor.session.undo());compare(document(),before)
                editor.beginGrid();settle();mouseClick(findChild(editor,"studioGridOverlap"));verify(editor.page.grid.overlap)
                mouseClick(findChild(editor,"studioGridPreview"));tryVerify(()=>!editor.gridGeometryPending);command(inputManager.confirm)
                compare(document(),remote);verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_overlay_move_keeps_locked_background_and_refuses_unsafe_disable(){
            scenario("policy-overlay",()=>{
                givenPage(false,false,true)
                verify(editor.session.execute("grid",{value:{overlap:true}}))
                const panel=editor.session.additionProposal("container",{x:0,y:0,width:2,height:2},"cancel","","","",{name:"Background panel",presentation:{showHeading:false,surfaceColor:"#445566",surfaceOpacity:0.3}},false)
                verify(panel.valid,panel.reason);verify(editor.session.execute("batch",{commands:panel.commands}))
                verify(editor.session.execute("orderRelative",{id:panel.id,relativeId:"cell-0",before:true}))
                verify(editor.session.execute("state",{id:panel.id,key:"locked",value:true}))
                const before=document(),count=editor.session.changeCount,background=JSON.stringify(editor.items.find(item=>item.id===panel.id))
                beginMove();command(inputManager.navigateRight);verify(editor.collisionPreview.valid)
                compare(editor.collisionPreview.placements.length,1);verify(editor.footerText.indexOf("layer")>=0)
                compare(document(),before);command(inputManager.confirm);compare(editor.session.changeCount,count+1)
                compare(JSON.stringify(editor.items.find(item=>item.id===panel.id)),background)
                const remote=document();verify(editor.session.undo());compare(document(),before);settle()
                dragRight(true);compare(document(),remote);verify(editor.session.undo());compare(document(),before)
                editor.beginGrid();command(inputManager.navigateUp);command(inputManager.confirm)
                verify(!editor.gridPreviewValid);compare(editor.session.proposalModel,null)
                editor.choice=6;command(inputManager.confirm);compare(editor.mode,"convert")
                command(inputManager.back);compare(document(),before);compare(editor.session.changeCount,count)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
    }
}
