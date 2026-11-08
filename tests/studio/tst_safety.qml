import QtQuick
import QtTest
import "../../qml" as App

Item {
    width: 1280; height: 720
    TestCase {
        id: tests
        name: "StudioSafety"
        when: windowShown
        property var editor: null
        property string diskBaseline: ""
        Component { id: editorComponent; App.PageStudioView { width: 1280; height: 720; layoutModel: homeLayout } }
        SignalSpy { id: skinsSpy; signalName: "skinsRequested" }
        function init() {
            failOnWarning(/Binding loop detected|ReferenceError|TypeError/)
        }
        function cleanup() {
            if (editor) {
                settle()
                editor.close()
                settle()
            }
            editor = null
        }
        function settle() {
            let settled = false
            Qt.callLater(() => Qt.callLater(() => { settled = true }))
            tryVerify(() => settled)
        }
        function givenPage(full, scrolling) {
            verify(studioFixture.reset(full || false, scrolling || false))
            diskBaseline = studioFixture.disk()
            editor = createTemporaryObject(editorComponent, tests.parent)
            verify(editor !== null)
            editor.show("", "cell-0")
            editor.toolbarFocus = false
            tryVerify(() => editor.current !== null)
        }
        function command(value) { editor.handleCommand(value) }
        function document() { return JSON.stringify(editor.session.document) }
        function scenario(name, body) {
            try { body() } catch (error) {
                if (editor && studioFixture.artifacts)
                    grabImage(editor).save(studioFixture.artifacts + "/studio-test-" + name + ".png")
                throw error
            }
        }
        function test_t23_interaction_navigates_only_draft_and_restores_edit_origin() {
            givenPage(false,true)
            verify(editor.session.execute("newPage",{name:"Draft destination"}))
            const target=editor.session.document.pages[0].id
            const before=document(),origin=editor.editSnapshot()
            editor.beginInteraction();compare(editor.mode,"preview")
            const preview=findChild(editor,"studioPreview")
            compare(preview.editMode,false);compare(preview.previewOnly,true)
            editor.interactionAction({type:"open-page",pageId:target},editor.selectedId)
            compare(editor.pageId,target)
            editor.interactionAction({type:"launch-app",appId:"movies"},"")
            verify(editor.message.indexOf("Would invoke")===0)
            editor.interactionAction({type:"settings",section:0},"")
            verify(editor.message.indexOf("Would invoke")===0)
            command(inputManager.back);settle()
            compare(editor.mode,"canvas");compare(editor.pageId,origin.pageId)
            compare(editor.selectedId,origin.selectedId);compare(editor.scopeId,origin.scopeId)
            compare(editor.parentScopeId,origin.parentScopeId);compare(editor.toolbarFocus,origin.toolbarFocus)
            compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_t23_stress_scenarios_are_private_and_editor_lock_blocks_commands() {
            givenPage(false,true)
            const before=document(),selected=editor.selectedId
            for(let i=1;i<=4;++i){
                editor.mode="stress";editor.choice=i;editor.activateChoice();settle()
                compare(editor.samplePreview,true);compare(document(),before)
                compare(studioFixture.disk(),diskBaseline);compare(editor.selectedId,selected)
            }
            editor.mode="stress";editor.choice=0;editor.activateChoice();compare(editor.stressScenario,"real")
            editor.session.setEditorLocked(true)
            verify(!editor.session.execute("rename",{id:selected,name:"Blocked"},editor.pageId))
            verify(!editor.session.apply(selected));compare(document(),before)
            editor.session.setEditorLocked(false)
            verify(editor.session.execute("rename",{id:selected,name:"Allowed"},editor.pageId))
            verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_t23_whole_interface_defaults_preview_cancel_accept_and_undo() {
            givenPage(false,false)
            verify(editor.session.execute("newPage",{name:"Authored destination"}))
            const before=document(),origin=editor.selectedId
            editor.mode="restoreDefaults";editor.choice=2;editor.activateChoice();settle()
            compare(editor.mode,"defaultsReview");compare(editor.choice,0)
            compare(document(),before);verify(editor.defaultRemovedPages.length>0)
            command(inputManager.back);settle();compare(editor.mode,"restoreDefaults");compare(editor.selectedId,origin)
            compare(document(),before);compare(studioFixture.disk(),diskBaseline)
            editor.choice=2;editor.activateChoice();editor.choice=1;editor.activateChoice()
            compare(editor.mode,"canvas");compare(editor.session.document.pages.length,0)
            verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_t23_health_routes_to_component_and_versions_cancel_restore_selection() {
            givenPage(false,false)
            const origin=editor.selectedId,before=document()
            editor.focusFinding({pageId:"",componentId:origin,property:"name",message:"Review label"})
            compare(editor.selectedId,origin);compare(editor.mode,"inspector")
            editor.mode="canvas"
            verify(editor.session.saveVersion("UI milestone"))
            editor.versionEntries=editor.session.versions()
            editor.mode="versions";editor.choice=3;editor.activateChoice()
            compare(editor.mode,"versionReview");compare(document(),before)
            command(inputManager.back);settle();compare(editor.mode,"versions");compare(editor.selectedId,origin)
            compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
    }
}
