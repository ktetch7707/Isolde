import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioReuse";when:windowShown
        property var editor:null
        property string disk:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/);verify(studioFixture.reset());disk=studioFixture.disk();editor=createTemporaryObject(editorComponent,tests.parent);verify(editor!==null);editor.show("","cell-0");tryVerify(()=>editor.current!==null)}
        function cleanup(){if(editor)editor.close();editor=null}
        function panel(){editor.activateEntry({action:"reuse"});compare(editor.mode,"reuseTools");const value=findChild(editor,"studioReusePanel");verify(value!==null);return value}
        function command(value){editor.handleCommand(value)}
        function test_grouped_reuse_back_restores_the_exact_menu_row(){
            const tools=panel(),before=JSON.stringify(editor.session.document)
            tools.selected=1;command(inputManager.confirm);compare(tools.view,"componentMenu")
            tools.selected=3;command(inputManager.confirm);compare(tools.view,"manageComponents")
            command(inputManager.back);compare(tools.view,"componentMenu");compare(tools.selected,3)
            command(inputManager.back);compare(tools.view,"overview");compare(tools.selected,1)
            tools.selected=2;command(inputManager.confirm);compare(tools.view,"overview");compare(tools.selected,2)
            compare(JSON.stringify(editor.session.document),before);compare(studioFixture.disk(),disk)
        }
        function test_remote_style_review_cancel_accept_and_undo(){
            const baseline=JSON.stringify(editor.session.document),count=editor.session.changeCount
            const tools=panel()
            tools.propose("saveStyle",{id:"cell-0",name:"TV ink"})
            compare(tools.view,"review",tools.message);compare(tools.selected,1)
            command(inputManager.confirm);compare(JSON.stringify(editor.session.document),baseline)
            tools.propose("saveStyle",{id:"cell-0",name:"TV ink"})
            command(inputManager.navigateUp);command(inputManager.confirm)
            compare(editor.session.reusableChoices("styles").length,1)
            compare(editor.session.changeCount,count+1)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),baseline)
            compare(studioFixture.disk(),disk)
        }
        function test_remote_pointer_saved_copy_parity_and_no_overwrite(){
            const tools=panel()
            tools.propose("saveComponent",{id:"cell-0",name:"My label"});tools.selected=0;tools.activate()
            const before=JSON.stringify(editor.session.document),count=editor.session.changeCount
            tools.activateLegacy(8);compare(tools.view,"components")
            command(inputManager.confirm);compare(tools.view,"review",tools.message);compare(tools.selected,1)
            command(inputManager.navigateUp);command(inputManager.confirm)
            const remote=JSON.stringify(editor.session.document)
            verify(remote!==before);compare(editor.session.changeCount,count+1);verify(editor.session.undo())
            tools.activateLegacy(8);tools.selected=0;tools.activate();tools.selected=0;tools.activate()
            compare(JSON.stringify(editor.session.document),remote);compare(studioFixture.disk(),disk)
        }
        function test_responsive_policies_render_without_authoring_geometry_data(){
            return ["wrap","scroll","shrink","stack"].map(policy=>({tag:policy,policy:policy}))
        }
        function test_responsive_policies_render_without_authoring_geometry(data){
            const document=JSON.parse(JSON.stringify(editor.session.document))
            document.composition={schemaVersion:1,enabled:true,margin:24,columns:1}
            document.categories=[{id:"flow",name:"Flow",presentation:{columns:4,height:800,padding:0,cardGap:16}}]
            const node=document.components[0];delete node.placement;node.categoryId="flow"
            const second=JSON.parse(JSON.stringify(node));second.id="cell-1"
            document.components=[node,second]
            verify(editor.session.pushEdit(JSON.stringify(document)),editor.session.errorString)
            const before=JSON.stringify(editor.session.document),nodes=JSON.stringify(editor.session.document.components)
            const tools=panel()
            tools.propose("responsive",{categoryId:"flow",rule:{schemaVersion:1,policy:data.policy,minimumWidth:120,minimumHeight:16,aspect:4}})
            compare(tools.view,"review",tools.message);command(inputManager.navigateUp);command(inputManager.confirm)
            command(inputManager.back)
            tryVerify(()=>findChild(editor,"responsiveSection-flow")!==null)
            const scroll=findChild(editor,"responsiveSection-flow")
            compare(scroll.interactive,data.policy==="scroll")
            const card=findChild(editor,"composedCard-cell-0");verify(card!==null)
            compare(card.parent.columns,data.policy==="stack"?1:4)
            verify(waitForRendering(editor))
            tryVerify(()=>card.width>0 && card.height>0,5000,"Responsive card must have nonzero rendered geometry")
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-responsive-"+data.policy+"-wide.png")
            editor.alternateViewportRatio=.5
            verify(waitForRendering(editor))
            // Responsive section replacement legitimately creates new cards.
            // Inspect the current rendered delegate, not its destroyed predecessor.
            function currentCard(){return findChild(editor,"composedCard-cell-0")}
            tryVerify(()=>!!currentCard() && currentCard().width>0 && currentCard().height>0)
            if(data.policy==="wrap")tryVerify(()=>currentCard().parent && currentCard().parent.columns>=1 && currentCard().parent.columns<4)
            if(data.policy==="stack")compare(currentCard().parent.columns,1)
            const currentScroll=findChild(editor,"responsiveSection-flow")
            if(data.policy!=="scroll")verify(currentScroll.contentHeight<=currentScroll.height+1,"Every responsive row must fit the declared non-scrolling section")
            compare(findChild(editor,"studioPreview").layoutError,"")
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-responsive-"+data.policy+"-tall.png")
            compare(JSON.stringify(editor.session.document.components),nodes)
            compare(studioFixture.disk(),disk)
            if(data.policy==="stack") {
                editor.alternateViewportRatio=0
                const overflow=JSON.parse(JSON.stringify(editor.session.document)),source=overflow.components.slice()
                const category=overflow.categories[0]
                category.presentation.responsive.minimumWidth=800
                overflow.categories=[];overflow.components=[]
                for(let group=0;group<3;++group) {
                    const copy=JSON.parse(JSON.stringify(category));copy.id="overflow-"+group;overflow.categories.push(copy)
                    source.forEach((value,index)=>{const node=JSON.parse(JSON.stringify(value));node.id="overflow-"+group+"-"+index;node.categoryId=copy.id;overflow.components.push(node)})
                }
                verify(editor.session.pushEdit(JSON.stringify(overflow)),editor.session.errorString)
                tryVerify(()=>findChild(editor,"studioPreview").layoutError.indexOf("Responsive")>=0)
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-responsive-overflow.png")
                editor.activateMenu(editor.menuEntries.findIndex(e=>e.action==="apply"));command(inputManager.confirm)
                compare(editor.mode,"applyFailed");verify(editor.message.indexOf("Responsive")>=0);compare(studioFixture.disk(),disk)
                command(inputManager.back);verify(editor.session.undo())
                tryCompare(findChild(editor,"studioPreview"),"layoutError","")
            }
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),before)
        }
        function test_viewport_preview_and_cancel_preserve_geometry(){
            const baseline=JSON.stringify(editor.session.document),tools=panel()
            tools.activateLegacy(12);compare(tools.view,"responsive")
            tools.selected=5;command(inputManager.confirm);compare(editor.alternateViewportRatio,1)
            compare(JSON.stringify(editor.session.document),baseline)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-reuse-square.png")
            command(inputManager.back);compare(editor.alternateViewportRatio,0)
            command(inputManager.back);compare(editor.mode,"canvas")
            compare(editor.selectedId,"cell-0");compare(studioFixture.disk(),disk)
        }
    }
}
