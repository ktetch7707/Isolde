import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioDestinations";when:windowShown
        property var editor:null
        property string diskBaseline:""
        property string sourceId:""
        property string destinationId:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/)}
        function cleanup(){if(editor){settle();editor.close();settle()}editor=null}
        function settle(){let ready=false;Qt.callLater(()=>Qt.callLater(()=>{ready=true}));tryVerify(()=>ready)}
        function command(value){editor.handleCommand(value)}
        function document(){return JSON.stringify(editor.session.document)}
        function scenario(name,body){try{body()}catch(error){if(editor && studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-"+name+".png");throw error}}
        function addGroup(name,parent,x,y,width,height,columns,rows){
            const proposal=editor.session.additionProposal("container",{x:x,y:y,width:width,height:height},"cancel","","",parent || "",{name:name,presentation:{layout:{schemaVersion:1,mode:"grid",columns:columns || 4,rows:rows || 2,gap:12,padding:0}}},false)
            verify(proposal.valid,proposal.reason)
            verify(editor.session.execute("batch",{commands:proposal.commands}),editor.session.errorString)
            return proposal.id
        }
        function givenGroups(){
            verify(studioFixture.reset(false,false,false,true));diskBaseline=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,tests.parent);verify(editor !== null)
            editor.show("","cell-0");editor.toolbarFocus=false
            verify(editor.session.execute("group",{ids:["cell-0","cell-1"]}))
            sourceId=editor.items.find(item=>item.componentType==="container").id
            destinationId=addGroup("Reading room","",2,0,2,2,4,2)
            editor.selectedId="cell-0";settle()
            compare(editor.parentScopeId,sourceId)
        }
        function beginMove(){command(inputManager.confirm);compare(editor.mode,"context");command(inputManager.confirm);compare(editor.mode,"move");settle()}
        function destinations(pointer){
            if(pointer)mouseClick(findChild(editor,"studioMoveOptions"))
            else command(inputManager.menu)
            compare(editor.mode,"moveOptions")
            compare(editor.dialogLabels()[5],"Move into group…")
            tryVerify(()=>findChild(editor,"studioDialogChoice-5") !== null)
            settle()
            if(pointer)mouseClick(findChild(editor,"studioDialogChoice-5"))
            else {for(let i=0;i<5;++i)command(inputManager.navigateDown);command(inputManager.confirm)}
            compare(editor.mode,"groupDestination");settle()
        }
        function choose(id,pointer){
            const index=editor.destinationGroups.findIndex(group=>group.id===id);verify(index>=0)
            verify(editor.dialogLabels()[index].indexOf("Move into ")===0 || !id)
            if(pointer)mouseClick(findChild(editor,"studioDialogChoice-"+index))
            else {for(let i=0;i<index;++i)command(inputManager.navigateDown);command(inputManager.confirm)}
            compare(editor.mode,"move");settle()
        }
        function test_named_group_move_remote_pointer_parity_and_exact_cancel(){
            scenario("named-group-move",()=>{
                givenGroups()
                const before=document(),count=editor.session.changeCount
                const viewport=JSON.stringify(findChild(editor,"studioPreview").editorViewport())
                beginMove();destinations(false);choose(destinationId,false)
                compare(editor.parentScopeId,destinationId);verify(editor.collisionPreview.valid)
                verify(editor.message.indexOf("Reading room")>=0)
                compare(document(),before);compare(editor.session.changeCount,count)
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-named-group-move.png")
                command(inputManager.back);settle()
                compare(editor.parentScopeId,sourceId);compare(editor.selectedId,"cell-0")
                compare(document(),before);compare(editor.session.changeCount,count)
                compare(JSON.stringify(findChild(editor,"studioPreview").editorViewport()),viewport)
                beginMove();destinations(false);choose(destinationId,false);command(inputManager.navigateRight);command(inputManager.confirm)
                compare(editor.mode,"canvas");compare(editor.session.changeCount,count+1)
                compare(editor.items.find(item=>item.id==="cell-0").parentId,destinationId)
                const remote=document()
                verify(editor.session.undo());compare(document(),before);settle()
                beginMove();destinations(true);choose(destinationId,true)
                mouseClick(findChild(editor,"studioMoveCell-1"));compare(editor.candidate.x,1)
                compare(document(),before)
                mouseClick(findChild(editor,"studioMovePlace"));compare(editor.mode,"canvas")
                compare(document(),remote)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_locked_hidden_and_descendant_targets_are_excluded(){
            scenario("group-target-refusal",()=>{
                givenGroups()
                const child=addGroup("Nested room",destinationId,0,0,1,1,2,2)
                editor.selectedId=destinationId;settle()
                verify(!editor.destinationGroups.some(group=>group.id===destinationId || group.id===child))
                const cycle=editor.session.placementProposal(destinationId,{x:0,y:0,width:1,height:1},"shift","",{parentId:child,categoryId:""})
                verify(!cycle.valid)
                editor.selectedId="cell-0";settle()
                verify(editor.session.execute("state",{id:destinationId,key:"locked",value:true}))
                verify(!editor.destinationGroups.some(group=>group.id===destinationId || group.id===child))
                verify(!editor.session.placementProposal("cell-0",{x:0,y:0,width:1,height:1},"shift","",{parentId:child,categoryId:""}).valid)
                verify(editor.session.execute("state",{id:destinationId,key:"locked",value:false}))
                verify(editor.session.execute("state",{id:destinationId,key:"hidden",value:true}))
                verify(!editor.destinationGroups.some(group=>group.id===destinationId || group.id===child))
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_full_destination_refuses_without_reparenting_and_pointer_cancel(){
            scenario("group-target-full",()=>{
                givenGroups()
                addGroup("Occupant",destinationId,0,0,4,2,4,2)
                const before=document(),count=editor.session.changeCount
                beginMove();destinations(true);choose(destinationId,true)
                verify(!editor.collisionPreview.valid)
                command(inputManager.confirm);compare(editor.mode,"move");verify(editor.message.length>0)
                compare(editor.items.find(item=>item.id==="cell-0").parentId,sourceId)
                compare(document(),before);compare(editor.session.changeCount,count)
                mouseClick(findChild(editor,"studioMoveCancel"));compare(editor.mode,"canvas")
                compare(editor.parentScopeId,sourceId);compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
    }
}
