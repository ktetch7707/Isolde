import QtQuick
import QtTest
import "../../qml" as App

Item {
    width: 1280; height: 720
    TestCase {
        id: tests
        name: "StudioControls"
        when: windowShown
        property var editor: null
        property string diskBaseline: ""
        Component { id: editorComponent; App.PageStudioView { width: 1280; height: 720; layoutModel: homeLayout } }
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
        function test_move_cancel_restores_document_history_and_viewport() {
            scenario("move-cancel", () => {
                givenPage(false, true)
                const before = document()
                const preview = findChild(editor, "studioPreview")
                editor.beginGesture(false)
                for (let i = 0; i < 12; ++i) command(inputManager.navigateDown)
                const composed = findChild(preview, "composedHome")
                tryVerify(() => composed.scrollOffset > 0)
                compare(document(), before)
                command(inputManager.back)
                compare(document(), before)
                compare(editor.session.changeCount, 0)
                tryCompare(composed, "scrollOffset", 0)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_size_slider_remote_cancel_and_pointer_commit() {
            scenario("size-slider", () => {
                givenPage(false, false)
                const before = document()
                editor.inspectorOperation("geometry")
                compare(editor.mode, "size")
                const slider = findChild(editor, "studioSize-width")
                verify(slider !== null)
                compare(slider.from, 1)
                compare(slider.to, 4)
                command(inputManager.navigateRight)
                compare(editor.candidate.width, 2)
                compare(document(), before)
                command(inputManager.back)
                compare(document(), before)
                editor.inspectorOperation("geometry")
                // Pointer drives the real Slider; the session still validates
                // and accepts through the same Done operation as remote input.
                const pointerSlider = findChild(editor, "studioSize-width")
                verify(waitForRendering(pointerSlider))
                mouseClick(pointerSlider, pointerSlider.width - pointerSlider.rightPadding - 2, pointerSlider.height / 2)
                compare(editor.candidate.width, 4)
                editor.choice = 2
                command(inputManager.confirm)
                compare(editor.session.changeCount, 1)
                compare(editor.current.placement.width, 4)
                compare(studioFixture.disk(), diskBaseline)
                verify(editor.session.undo())
                compare(document(), before)
            })
        }
        function test_no_room_enable_and_add_is_one_undoable_edit() {
            scenario("no-room-add", () => {
                givenPage(true, false)
                const before = document()
                editor.startAddition("text", {name: "Retained label"})
                compare(editor.mode, "noRoom")
                command(inputManager.confirm)
                compare(editor.mode, "move")
                verify(editor.pendingScrollEnable)
                compare(editor.addingConfiguration.name, "Retained label")
                compare(document(), before)
                command(inputManager.confirm)
                compare(editor.session.changeCount, 1)
                verify(editor.session.model.composition.verticalScrolling)
                compare(editor.session.model.items.length, 13)
                verify(editor.session.undo())
                compare(document(), before)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_scrolling_checkbox_pointer_preview_cancel() {
            scenario("scroll-checkbox", () => {
                givenPage(false, false)
                const before = document()
                editor.inspectorOperation("scrolling")
                compare(editor.mode, "scrolling")
                const toggle = findChild(editor, "studioScrollingToggle")
                verify(toggle !== null)
                compare(toggle.checked, false)
                mouseClick(toggle)
                tryCompare(toggle, "checked", true)
                compare(document(), before)
                compare(editor.session.changeCount, 0)
                command(inputManager.back)
                compare(document(), before)
                verify(editor.session.proposalModel === null)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_move_reveals_actual_rendered_component() {
            scenario("rendered-visibility", () => {
                givenPage(false, true)
                const before = document()
                const preview = findChild(editor, "studioPreview")
                editor.beginGesture(false)
                for (let i = 0; i < 12; ++i) command(inputManager.navigateDown)
                function targetVisible() {
                    const card = findChild(editor, "studioProposal-cell-0")
                    if (!card || !card.visible) return false
                    const p = card.mapToItem(preview, 0, 0), v = preview.pageViewport
                    return p.y >= v.y - 0.1 && p.y + card.height <= v.y + v.height + 0.1
                }
                tryVerify(targetVisible, 2000, "Actual proposal must fit inside the viewport")
                compare(document(), before)
                command(inputManager.confirm)
                compare(editor.current.placement.y, 12)
                compare(editor.session.changeCount, 1)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_locked_component_cannot_start_moving() {
            scenario("locked-move", () => {
                givenPage(false, false)
                verify(editor.session.execute("state", {id: "cell-0", key: "locked", value: true}))
                const before = document(), history = editor.session.changeCount
                editor.beginGesture(false)
                compare(editor.mode, "canvas")
                verify(editor.message.indexOf("locked") >= 0)
                compare(document(), before)
                compare(editor.session.changeCount, history)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
    }
}
