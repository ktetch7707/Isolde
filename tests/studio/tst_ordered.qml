import QtQuick
import QtQuick.Window
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioOrdered";when:windowShown
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
        function givenOrdered(column,capacity){
            verify(studioFixture.reset(false,false));diskBaseline=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,tests.parent);verify(editor !== null)
            editor.show("","cell-0");editor.toolbarFocus=false
            const layout={schemaVersion:1,mode:column?"column":"row",columns:column?1:4,rows:column?4:1,gap:12,padding:0}
            if(!column)layout.visibleColumns=capacity || 4
            const config={name:column?"Reading column":"Favorite buttons",presentation:{layout:layout}}
            if(!column)config.buttons=[{name:"Alpha"},{name:"Beta"},{name:"Gamma"}]
            const proposal=editor.session.additionProposal(column?"container":"carousel",{x:0,y:1,width:4,height:2},"cancel","","","",config,false)
            verify(proposal.valid,proposal.reason);verify(editor.session.execute("batch",{commands:proposal.commands}))
            destinationId=proposal.id
            if(column)for(let i=0;i<3;++i){const child=editor.session.additionProposal("text",{x:0,y:i,width:1,height:1},"cancel","","",destinationId,{name:["Alpha","Beta","Gamma"][i]},false);verify(child.valid,child.reason);verify(editor.session.execute("batch",{commands:child.commands}))}
            const children=editor.items.filter(item=>item.parentId===destinationId)
            editor.selectedId=children[0].id;settle()
            return children.map(item=>item.id)
        }
        function test_ordered_insertion_data(){return [{tag:"button-carousel",column:false},{tag:"nested-column",column:true}]}
        function test_ordered_insertion(data){
            scenario("ordered-"+data.tag,()=>{
                const ids=givenOrdered(data.column,4),before=document(),count=editor.session.changeCount
                const layout=JSON.stringify(editor.parentGroup.presentation.layout),preview=findChild(editor,"studioPreview")
                const axis=data.column?"y":"x",direction=data.column?inputManager.navigateDown:inputManager.navigateRight
                beginMoveOrdered();command(direction);command(direction)
                verify(editor.flowPreviewValid);compare(editor.flowTargetIndex,2)
                compare(editor.page.items.find(item=>item.id===ids[0]).placement[axis],2)
                compare(editor.page.items.find(item=>item.id===ids[1]).placement[axis],0)
                compare(document(),before);compare(editor.session.changeCount,count)
                command(inputManager.back);settle();compare(document(),before)
                beginMoveOrdered();command(direction);command(direction);command(inputManager.confirm)
                compare(editor.session.changeCount,count+1);const remote=document()
                verify(editor.session.undo());compare(document(),before);settle()
                const a=preview.placementRectFor(editor.current.placement,"",destinationId),b=preview.placementRectFor(editor.items.find(item=>item.id===ids[2]).placement,"",destinationId)
                const from=preview.mapToItem(editor,a.x+a.width/2,a.y+a.height/2),to=preview.mapToItem(editor,b.x+b.width/2,b.y+b.height/2)
                mousePress(editor,from.x,from.y);mouseMove(editor,to.x,to.y)
                compare(editor.mode,"flowMove");compare(editor.flowTargetIndex,2)
                settle()
                const panel=findChild(editor,"studioMoveActions")
                tryVerify(()=>panel.visible && Qt.colorEqual(grabImage(editor).pixel(Math.round(panel.x+panel.width/2),Math.round(panel.y+panel.height-5)),editor.palette.surfaceColor))
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-ordered-"+data.tag+".png")
                mouseRelease(editor,to.x,to.y);compare(editor.mode,"canvas");compare(document(),remote)
                compare(JSON.stringify(editor.parentGroup.presentation.layout),layout)
                verify(editor.session.undo());compare(document(),before)
                settle();command(inputManager.confirm);compare(editor.mode,"context")
                tryVerify(()=>findChild(editor,"studioDialogChoice-0") !== null);settle()
                const moveChoice=findChild(editor,"studioDialogChoice-0")
                tryVerify(()=>moveChoice.visible && moveChoice.enabled);verify(waitForPolish(editor.Window.window))
                mouseClick(moveChoice);tryCompare(editor,"mode","flowMove");settle()
                const target=findChild(editor,"studioFlowTarget-2")
                verify(waitForPolish(editor.Window.window));mouseClick(target);tryCompare(editor,"flowTargetIndex",2)
                compare(document(),before)
                const place=findChild(editor,"studioMovePlace")
                verify(waitForPolish(editor.Window.window));mouseClick(place);tryVerify(()=>document()===remote)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function beginMoveOrdered(){command(inputManager.confirm);compare(editor.mode,"context");command(inputManager.confirm);compare(editor.mode,"flowMove");settle()}
        function test_ordered_locked_refusal_and_mixed_spans_preserve_gaps(){
            scenario("ordered-refusal",()=>{
                const ids=givenOrdered(false,4)
                verify(editor.session.execute("state",{id:ids[1],key:"locked",value:true}))
                const locked=document(),count=editor.session.changeCount
                beginMoveOrdered();command(inputManager.navigateRight);verify(!editor.flowPreviewValid)
                command(inputManager.confirm);compare(editor.mode,"flowMove");compare(document(),locked);compare(editor.session.changeCount,count)
                command(inputManager.back);verify(editor.session.undo());settle()
                const layout=Object.assign({},editor.parentGroup.presentation.layout,{columns:6})
                verify(editor.session.execute("childLayout",{id:destinationId,value:layout}))
                verify(editor.session.execute("place",{placements:ids.map((id,index)=>({id:id,placement:{x:[0,3,5][index],y:0,width:index===0?2:1,height:1},parentId:destinationId,categoryId:""}))}))
                const before=document(),components=editor.items.filter(item=>ids.indexOf(item.id)>=0)
                beginMoveOrdered();command(inputManager.navigateRight);command(inputManager.navigateRight)
                verify(editor.flowPreviewValid)
                for(let i=0;i<3;++i){const node=editor.page.items.find(item=>item.id===ids[i]);compare(node.placement.x,[4,0,2][i]);compare(node.placement.width,components[i].placement.width)}
                command(inputManager.confirm);verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_ordered_overflow_edge_preview_is_bounded_and_cancel_restores_viewport(){
            scenario("ordered-overflow",()=>{
                const ids=givenOrdered(false,2),before=document(),count=editor.session.changeCount,preview=findChild(editor,"studioPreview")
                const viewport=JSON.stringify(preview.editorViewport())
                beginMoveOrdered();command(inputManager.navigateRight);command(inputManager.navigateRight);settle()
                verify(preview.editorViewport().groups[destinationId].x>0)
                command(inputManager.confirm);const remote=document();verify(editor.session.undo());settle()
                preview.restoreEditorViewport(JSON.parse(viewport));settle()
                const a=preview.placementRectFor(editor.current.placement,"",destinationId),scope=preview.scopeViewport(destinationId)
                const from=preview.mapToItem(editor,a.x+a.width/2,a.y+a.height/2),edge=preview.mapToItem(editor,scope.x+scope.width-2,scope.y+scope.height/2)
                mousePress(editor,from.x,from.y);mouseMove(editor,edge.x,edge.y)
                tryCompare(editor,"flowTargetIndex",2)
                tryVerify(()=>findChild(editor,"studioHit-"+ids[0]).edgeBudget===0)
                compare(editor.flowIds.length,3);compare(document(),before);compare(editor.session.changeCount,count)
                command(inputManager.back);mouseRelease(editor,edge.x,edge.y);settle()
                compare(document(),before);compare(JSON.stringify(preview.editorViewport()),viewport)
                mousePress(editor,from.x,from.y);mouseMove(editor,edge.x,edge.y);tryCompare(editor,"flowTargetIndex",2)
                mouseRelease(editor,edge.x,edge.y);compare(document(),remote)
                verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
            })
        }
    }
}
