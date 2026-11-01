import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioPolicies";when:windowShown
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
            if(displaySettings.reduceMotion!==reduced)displaySettings.toggleReduceMotion()
            editor=createTemporaryObject(editorComponent,tests.parent);verify(editor !== null)
            editor.show("","cell-0");editor.toolbarFocus=false
            if(mixed){verify(editor.session.execute("remove",{id:"cell-1"}));verify(editor.session.execute("place",{placements:[{id:"cell-0",placement:{x:0,y:0,width:2,height:1}}]}))}
            verify(editor.session.execute("appearance",{value:{motionEnabled:!reduced}}))
            settle();compare(editor.page.appearance.motionEnabled,!reduced);compare(editor.reduceMotion,reduced)
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
        function test_full_and_mixed_input_parity_data(){return [{tag:"full",mixed:false,reduced:false},{tag:"mixed",mixed:true,reduced:false},{tag:"full-reduced",mixed:false,reduced:true},{tag:"mixed-reduced",mixed:true,reduced:true}]}
        function test_full_and_mixed_input_parity(data){
            scenario("policy-"+data.tag,()=>{
                givenPage(true,data.mixed,data.reduced)
                const before=document(),count=editor.session.changeCount,span=editor.current.placement.width
                const preview=findChild(editor,"studioPreview"),viewport=JSON.stringify(preview.editorViewport())
                beginMove();command(inputManager.navigateRight);verify(editor.collisionPreview.valid)
                compare(editor.collisionPreview.placements.length,2);compare(document(),before);compare(editor.session.changeCount,count)
                const origin=findChild(editor,"studioOrigin")
                verify(origin.visible);compare(origin.border.width,data.reduced?3:2)
                if(data.reduced){
                    verify(Qt.colorEqual(origin.border.color,editor.palette.focusColor))
                    displaySettings.toggleReduceMotion();compare(editor.reduceMotion,false);compare(origin.border.width,2)
                    compare(document(),before);compare(editor.session.changeCount,count)
                    displaySettings.toggleReduceMotion();compare(editor.reduceMotion,true);compare(origin.border.width,3)
                    if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-global-reduced-motion-"+data.tag+".png")
                }
                verify(preview.freezeData)
                command(inputManager.back);settle();compare(document(),before);compare(JSON.stringify(preview.editorViewport()),viewport)
                beginMove();command(inputManager.navigateRight);command(inputManager.confirm)
                compare(editor.current.placement.width,span);compare(editor.session.changeCount,count+1)
                const remote=document();verify(editor.session.undo());compare(document(),before);settle()
                dragRight(true);compare(editor.mode,"canvas");compare(document(),remote)
                verify(editor.session.undo());compare(document(),before);settle()
                beginMove();mouseClick(findChild(editor,"studioMoveCell-1"));compare(editor.candidate.x,1)
                mouseClick(findChild(editor,"studioMovePlace"));compare(document(),remote)
                verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_locked_full_grid_refuses_and_pointer_cancel_restores(){
            scenario("policy-locked",()=>{
                givenPage(true,false,true)
                verify(editor.session.execute("state",{id:"cell-1",key:"locked",value:true}))
                const before=document(),count=editor.session.changeCount,preview=findChild(editor,"studioPreview"),viewport=JSON.stringify(preview.editorViewport())
                beginMove();command(inputManager.navigateRight);verify(!editor.collisionPreview.valid)
                const ghost=findChild(editor,"studioGhost")
                verify(ghost.visible);compare(ghost.border.width,2);verify(Qt.colorEqual(ghost.border.color,editor.palette.focusColor))
                command(inputManager.confirm);compare(editor.mode,"move");verify(editor.message.indexOf("locked")>=0)
                command(inputManager.back);settle();compare(document(),before)
                dragRight(true);compare(editor.mode,"move");verify(!editor.collisionPreview.valid)
                compare(document(),before);compare(editor.session.changeCount,count)
                mouseClick(findChild(editor,"studioMoveCancel"));settle();compare(editor.mode,"canvas")
                compare(JSON.stringify(preview.editorViewport()),viewport);compare(document(),before);compare(studioFixture.disk(),diskBaseline)
            })
        }
    }
}
