import QtQuick
import QtTest
import "../../qml" as App

TestCase {
    id: tests
    property var editor: null
    property string diskBaseline: ""

    Component {
        id: editorComponent
        App.PageStudioView {
            width: 1280
            height: 720
            layoutModel: homeLayout
        }
    }

    // Fail on runtime warnings so silent UI regressions do not pass.
    function init() {
        failOnWarning(/Binding loop detected|ReferenceError|TypeError/)
    }
    // Close the editor between cases to keep draft state isolated.
    function cleanup() {
        if (editor) {
            settle()
            editor.close()
            settle()
        }
        editor = null
    }
    // Wait for queued UI updates before asserting the draft.
    function settle() {
        let settled = false
        Qt.callLater(() => Qt.callLater(() => { settled = true }))
        tryVerify(() => settled)
    }
    // Start each case from the requested fixture and retain the disk baseline.
    function givenPage(full, scrolling) {
        verify(studioFixture.reset(full || false, scrolling || false))
        diskBaseline = studioFixture.disk()
        editor = createTemporaryObject(editorComponent, tests.parent)
        verify(editor !== null)
        editor.show("", "cell-0")
        editor.toolbarFocus = false
        tryVerify(() => editor.current !== null)
    }
    // Send the same commands a remote control would generate.
    function command(value) { editor.handleCommand(value) }
    // Compare serialized draft data rather than only the preview.
    function document() { return JSON.stringify(editor.session.document) }
    // Preserve visual diagnostics if an interaction fails.
    function scenario(name, body) {
        try {
            body()
        } catch (error) {
            if (editor && studioFixture.artifacts)
                grabImage(editor).save(studioFixture.artifacts + "/studio-test-" + name + ".png")
            throw error
        }
    }
}
