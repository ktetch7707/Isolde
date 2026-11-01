import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioGrid";when:windowShown
        property var editor:null
        property string diskBaseline:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/)}
        function cleanup(){if(editor){settle();editor.close();settle()}editor=null}
        function settle(){let ready=false;Qt.callLater(()=>Qt.callLater(()=>{ready=true}));tryVerify(()=>ready)}
        function givenPage(flow,arrangement){
            verify(studioFixture.reset(false,false,flow || false,arrangement || false))
            diskBaseline=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,tests.parent);verify(editor !== null)
            editor.show("","cell-0");editor.toolbarFocus=false;settle()
        }
        function command(value){editor.handleCommand(value)}
        function document(){return JSON.stringify(editor.session.document)}
        function scenario(name,body){try{body()}catch(error){if(editor && studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-"+name+".png");throw error}}
        function save(name){if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-"+name+".png")}
        function reviewAndAccept(){editor.choice=6;command(inputManager.confirm);compare(editor.mode,"conversionPreview");tryVerify(()=>!editor.gridGeometryPending);command(inputManager.confirm)}

        function test_existing_locked_group_retains_placement_and_rejects_smaller_grid(){
            scenario("page-grid-group",()=>{
                givenPage(false,true)
                verify(editor.session.execute("group",{ids:["cell-0","cell-1","cell-2"]}))
                const group=editor.items.find(item=>item.componentType==="container")
                verify(editor.session.execute("grid",{value:{columns:8,rows:5}}))
                const proposal=editor.session.placementProposal(group.id,{x:5,y:0,width:3,height:3},"cancel","",{})
                verify(proposal.valid)
                verify(editor.session.execute("place",{placements:proposal.placements}))
                verify(editor.session.execute("state",{id:group.id,key:"locked",value:true}))
                editor.selectedId=group.id;settle()
                const before=document(),count=editor.session.changeCount,components=JSON.stringify(editor.session.document.components)
                editor.beginGrid();editor.choice=1;command(inputManager.navigateLeft)
                compare(editor.gridDraft.columns,7);verify(!editor.gridPreviewValid)
                compare(editor.page.grid.columns,8)
                compare(document(),before);compare(editor.session.changeCount,count)
                command(inputManager.back)
                editor.beginGrid();editor.choice=1;command(inputManager.navigateRight)
                verify(editor.gridPreviewValid)
                reviewAndAccept()
                compare(editor.session.changeCount,count+1)
                compare(JSON.stringify(editor.session.document.components),components)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_legacy_conversion_and_sections_remain_explicit_private_and_reversible(){
            scenario("page-grid-conversion",()=>{
                givenPage(true,false)
                const before=document()
                editor.beginGrid()
                compare(editor.session.proposalModel,null)
                editor.choice=6;command(inputManager.confirm)
                compare(editor.mode,"conversionPreview");compare(document(),before)
                command(inputManager.back);compare(document(),before)
                editor.beginGrid();settle()
                mouseClick(findChild(editor,"studioGridMode-sections"))
                tryVerify(()=>!editor.gridGeometryPending)
                verify(editor.gridPreviewValid)
                compare(editor.page.grid.mode,"sections")
                const height=editor.sectionHeightDraft
                editor.choice=5;command(inputManager.navigateRight)
                tryVerify(()=>!editor.gridGeometryPending)
                compare(editor.sectionHeightDraft,height+20)
                compare(editor.page.composition.sectionHeight,height+20)
                compare(document(),before)
                save("page-grid-sections")
                reviewAndAccept()
                compare(editor.session.changeCount,1)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
    }
}
