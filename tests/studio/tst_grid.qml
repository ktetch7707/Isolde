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

        function test_page_inspector_grid_controls_preview_cancel_and_pointer_remote_parity(){
            scenario("page-grid",()=>{
                givenPage(false,false)
                const before=document()
                editor.openInspector(true)
                command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
                const inspector=findChild(editor,"studioInspector")
                compare(inspector.view,"fields")
                const index=inspector.fields.findIndex(field=>field.type==="grid")
                verify(index>=0)
                for(let i=0;i<index;++i)command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(editor.mode,"convert")
                compare(editor.session.proposalModel,null)
                compare(document(),before)
                editor.choice=1;command(inputManager.navigateRight)
                verify(editor.gridPreviewValid)
                compare(editor.page.grid.columns,5)
                compare(document(),before);compare(editor.session.changeCount,0)
                verify(findChild(editor,"studioPreview").freezeData)
                settle();save("page-grid")
                command(inputManager.back)
                compare(editor.mode,"inspector");compare(inspector.selected,index)
                compare(editor.session.proposalModel,null);compare(document(),before)
                editor.beginGrid();editor.choice=1;command(inputManager.navigateRight)
                reviewAndAccept()
                compare(editor.mode,"canvas");compare(editor.session.changeCount,1)
                const remote=document()
                verify(editor.session.undo());compare(document(),before)
                editor.beginGrid();settle()
                mouseClick(findChild(editor,"studioGrid-columns-2"))
                compare(editor.page.grid.columns,5)
                mouseClick(findChild(editor,"studioGridPreview"))
                compare(editor.mode,"conversionPreview");command(inputManager.confirm)
                compare(document(),remote)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_grid_sliders_bounds_exact_values_and_refused_physical_geometry(){
            scenario("page-grid-bounds",()=>{
                givenPage(false,false)
                const before=document()
                editor.beginGrid();settle()
                for(const field of ["gap","margin"]){
                    const slider=findChild(editor,"studioGrid-"+field);verify(slider !== null)
                    mouseClick(slider,slider.width-slider.rightPadding-1,slider.height/2)
                    compare(editor.gridDraft[field],100);verify(editor.gridPreviewValid)
                }
                compare(document(),before);compare(editor.session.changeCount,0)
                command(inputManager.back);compare(document(),before)
                editor.beginGrid();editor.choice=1
                for(let i=0;i<20;++i)command(inputManager.navigateRight)
                compare(editor.gridDraft.columns,16)
                editor.choice=2
                for(let i=0;i<20;++i)command(inputManager.navigateRight)
                compare(editor.gridDraft.rows,10)
                editor.choice=3
                // Dense grids cannot fit the largest gap at this viewport.
                const slider=findChild(editor,"studioGrid-gap")
                mouseClick(slider,slider.width-slider.rightPadding-1,slider.height/2)
                compare(editor.gridDraft.gap,100);verify(!editor.gridPreviewValid)
                compare(editor.session.proposalModel,null)
                editor.choice=6;command(inputManager.confirm)
                compare(editor.mode,"convert");verify(editor.message.length>0)
                compare(document(),before);compare(editor.session.changeCount,0)
                command(inputManager.back)
                editor.beginGrid();editor.choice=8;command(inputManager.confirm)
                compare(editor.mode,"gridExact");command(inputManager.confirm)
                compare(editor.mode,"keyboard")
                const keyboard=findChild(editor,"studioKeyboard")
                keyboard.accepted("3");compare(editor.mode,"keyboard")
                compare(document(),before)
                keyboard.accepted("6");compare(editor.mode,"gridExact")
                compare(editor.page.grid.columns,6)
                compare(document(),before)
                command(inputManager.navigateDown);command(inputManager.confirm)
                compare(editor.mode,"keyboard");compare(editor.page.grid.columns,6)
                verify(findChild(editor,"studioPreview").freezeData)
                keyboard.canceled();compare(editor.mode,"gridExact")
                command(inputManager.back);compare(editor.mode,"convert")
                command(inputManager.back);compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
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
