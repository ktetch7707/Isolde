import QtQuick
import QtTest
import "../../qml" as App

Item {
    width: 1280
    height: 720
    TestCase {
        name: "StudioCutover"
        when: windowShown
        property var host: null
        Component {
            id: customizeComponent
            App.CustomizeHomeView {
                width: 1280
                height: 720
                layoutModel: homeLayout
                initialComponentId: "cell-0"
            }
        }
        Component {
            id: pageComponent
            App.PageSurface {
                width: 1280
                height: 720
                layoutModel: homeLayout
                provider: null
                availableApps: []
                artwork: []
                themes: []
            }
        }
        function init() {
            failOnWarning(/Binding loop detected|ReferenceError|TypeError/)
            verify(studioFixture.reset())
            host = createTemporaryObject(customizeComponent, parent)
            verify(host !== null)
            tryVerify(() => host.editing)
        }
        function cleanup() {
            if (host) host.visible = false
            host = null
        }
        function send(command) {
            host.handleCommand(command)
        }
        function test_selected_origin_and_home_cannot_discard_dirty_draft() {
            const editor = findChild(host, "pageStudio")
            verify(editor !== null)
            compare(editor.selectedId, "cell-0")
            const disk = studioFixture.disk()
            verify(editor.session.execute("patch", {id: "cell-0", value: {name: "Draft name"}}))
            verify(editor.session.dirty)
            send(inputManager.home)
            compare(editor.mode, "dirty")
            verify(editor.open)
            compare(studioFixture.disk(), disk)
            send(inputManager.confirm) // Continue editing is the safe default.
            compare(editor.mode, "canvas")
            verify(editor.session.dirty)
            compare(studioFixture.disk(), disk)
        }
        function test_home_cancels_move_without_writing_or_history() {
            const editor = findChild(host, "pageStudio")
            const disk = studioFixture.disk()
            editor.toolbarFocus = false
            editor.beginGesture(false)
            compare(editor.mode, "move")
            const count = editor.session.changeCount
            send(inputManager.navigateRight)
            send(inputManager.home)
            compare(editor.mode, "canvas")
            compare(editor.selectedId, "cell-0")
            compare(editor.session.changeCount, count)
            compare(studioFixture.disk(), disk)
        }
        function test_teardown_invalidates_client_and_reopens_same_origin() {
            const editor = findChild(host, "pageStudio")
            const session = editor.session
            const disk = studioFixture.disk()
            host.visible = false
            verify(!session.active)
            compare(studioFixture.disk(), disk)
            host.visible = true
            tryVerify(() => host.editing)
            compare(editor.selectedId, "cell-0")
            verify(editor.session.active)
            compare(studioFixture.disk(), disk)
        }
        function test_named_page_entry_and_clean_exit_restore_component() {
            host.visible = false
            const pageId = homeLayout.createPage("Cutover page", "blank")
            verify(pageId.length > 0)
            const model = homeLayout.pageModel(pageId)
            const itemId = model.addWidget("clock")
            verify(itemId.length > 0)
            host = createTemporaryObject(pageComponent, parent)
            verify(host !== null)
            host.showPage(pageId, "home-origin", false)
            const content = findChild(host, "pageContent")
            verify(content !== null)
            content.restoreFocus(itemId)
            compare(host.focusedComponentId, itemId)
            const disk = studioFixture.disk()
            host.beginEditing()
            const editor = findChild(host, "pageStudio")
            tryVerify(() => editor.open)
            compare(editor.pageId, pageId)
            compare(editor.selectedId, itemId)
            send(inputManager.home)
            tryVerify(() => !host.editing)
            tryCompare(content, "focusedItemId", itemId)
            compare(host.activeId, pageId)
            compare(studioFixture.disk(), disk)
            host.beginEditing()
            tryVerify(() => editor.open)
            verify(editor.session.execute("removePage", {id: pageId}))
            verify(editor.session.apply())
            verify(host.editing)
            verify(editor.open) // Publish must not tear down its own result UI.
            send(inputManager.home)
            tryVerify(() => !host.editing)
            compare(host.activeId, "")
        }
    }
}
