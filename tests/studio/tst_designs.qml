import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    TestCase {
        id:tests;name:"StudioDesigns";when:windowShown
        property var editor:null
        property string disk:""
        Component {id:editorComponent;App.PageStudioView {width:1280;height:720;layoutModel:homeLayout}}
        function init(){failOnWarning(/Binding loop detected|ReferenceError|TypeError/);verify(studioFixture.reset(false,false));disk=studioFixture.disk();editor=createTemporaryObject(editorComponent,tests.parent);verify(editor!==null);editor.show("","cell-0");editor.toolbarFocus=false;tryVerify(()=>editor.current!==null)}
        function cleanup(){if(editor)editor.close();editor=null}
        function command(value){editor.handleCommand(value)}
        function designTools(){editor.activateEntry({action:"designs"});compare(editor.mode,"designTools");const panel=findChild(editor,"studioDesignTools");verify(panel!==null);panel.selected=panel.designs.length;command(inputManager.confirm);compare(panel.view,"menu");panel.selected=0;return panel}
        function test_remote_preview_cancel_and_accept_are_one_draft_transaction(){
            verify(editor.session.execute("rename",{id:"cell-0",name:"My existing draft"}))
            const baseline=JSON.stringify(editor.session.document),count=editor.session.changeCount,selected=editor.selectedId
            let panel=designTools();command(inputManager.confirm);compare(panel.view,"designs")
            const target=panel.designs.findIndex(d=>d.id==="builtin-minimal-pastel");verify(target>=0)
            for(let i=0;i<target;++i)command(inputManager.navigateDown)
            command(inputManager.confirm);compare(panel.scope,"page");compare(panel.view,"preview");compare(panel.selected,1);verify(panel.previewing)
            compare(JSON.stringify(editor.session.document),baseline);compare(studioFixture.disk(),disk)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-design-preview.png")
            command(inputManager.back);compare(editor.session.proposalModel,null)
            command(inputManager.back);compare(editor.mode,"canvas");compare(editor.selectedId,selected);compare(JSON.stringify(editor.session.document),baseline)
            panel=designTools();command(inputManager.confirm);panel.selected=target;command(inputManager.confirm)
            command(inputManager.navigateUp);command(inputManager.confirm);compare(editor.mode,"canvas")
            compare(editor.session.changeCount,count+1);compare(studioFixture.disk(),disk)
            verify(editor.session.undo());compare(JSON.stringify(editor.session.document),baseline)
        }
        function test_pointer_and_remote_accept_produce_identical_layouts(){
            let panel=designTools();command(inputManager.confirm);panel.selected=panel.designs.findIndex(d=>d.id==="builtin-compact-launcher");verify(panel.selected>=0);command(inputManager.confirm);command(inputManager.navigateUp);command(inputManager.confirm)
            const remote=JSON.stringify(editor.session.document);verify(editor.session.undo())
            panel=designTools();panel.selected=0;panel.activate();panel.selected=panel.designs.findIndex(d=>d.id==="builtin-compact-launcher");panel.activate();panel.selected=0;panel.activate()
            compare(JSON.stringify(editor.session.document),remote);compare(studioFixture.disk(),disk)
        }
        function test_reference_controls_and_file_browser_cancel_do_not_change_draft(){
            const baseline=JSON.stringify(editor.session.document),selected=editor.selectedId
            const panel=designTools();panel.selected=3;command(inputManager.confirm);compare(panel.view,"reference")
            command(inputManager.navigateDown);const opacity=panel.referenceOpacity;command(inputManager.navigateLeft);verify(panel.referenceOpacity<opacity)
            command(inputManager.navigateDown);command(inputManager.confirm);compare(panel.referenceVisible,false)
            command(inputManager.back);panel.view="menu";panel.selected=1;command(inputManager.confirm);compare(panel.view,"browser")
            command(inputManager.back);command(inputManager.back);compare(editor.mode,"canvas");compare(editor.selectedId,selected)
            compare(JSON.stringify(editor.session.document),baseline);compare(studioFixture.disk(),disk)
        }
        function test_reference_image_and_portable_file_controls_use_private_state(){
            const baseline=JSON.stringify(editor.session.document),reference=studioFixture.referenceFixture()
            verify(reference.length>0)
            const directory=reference.slice(0,reference.lastIndexOf("/")),exportPath=directory+"/portable-design.json"
            let panel=designTools();panel.selected=3;command(inputManager.confirm);command(inputManager.confirm);compare(panel.view,"browser")
            panel.selected=1;command(inputManager.confirm);compare(panel.view,"folderPath")
            let keyboard=findChild(panel,"studioDesignPathKeyboard");verify(keyboard!==null);keyboard.accepted(directory);compare(panel.view,"browser")
            const imageIndex=panel.listing.entries.findIndex(e=>e.path===reference);verify(imageIndex>=0)
            panel.selected=imageIndex+2;command(inputManager.confirm);compare(panel.view,"reference")
            const overlay=findChild(editor,"studioReferenceImage");verify(overlay!==null);tryCompare(overlay,"status",Image.Ready);verify(overlay.visible);verify(overlay.width>0 && overlay.height>0)
            command(inputManager.navigateDown);command(inputManager.navigateLeft);compare(overlay.opacity,panel.referenceOpacity)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-private-reference.png")
            command(inputManager.back);panel.view="menu";panel.selected=2;command(inputManager.confirm);compare(panel.view,"scope")
            command(inputManager.confirm);compare(panel.view,"exportPath");keyboard.accepted(exportPath);compare(panel.view,"menu")
            compare(JSON.stringify(editor.session.document),baseline);compare(studioFixture.disk(),disk)
            panel.selected=1;command(inputManager.confirm);panel.selected=1;command(inputManager.confirm);keyboard.accepted(directory)
            const fileIndex=panel.listing.entries.findIndex(e=>e.path===exportPath);verify(fileIndex>=0)
            panel.selected=fileIndex+2;command(inputManager.confirm);compare(panel.view,"scope");command(inputManager.confirm);compare(panel.view,"preview")
            command(inputManager.confirm);compare(panel.view,"scope");compare(JSON.stringify(editor.session.document),baseline)
            command(inputManager.back);panel.view="menu";panel.selected=3;command(inputManager.confirm);panel.selected=3;command(inputManager.confirm)
            compare(editor.session.referenceSource,"");tryVerify(()=>!overlay.visible);compare(studioFixture.disk(),disk)
        }
    }
}
