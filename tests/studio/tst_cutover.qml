import QtQuick
import QtTest
import "../../qml" as App
import "../../qml/StudioTree.js" as StudioTree

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
            verify(model.addWidget("clock"))
            const itemId = model.items[model.items.length - 1].id
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
        function scrollingPage(name) {
            const id = homeLayout.createPage(name, "blank")
            verify(id.length > 0)
            const session = homeLayout.beginStudio()
            verify(session.execute("pagePatch", {scope:"composition", value:{enabled:true,
                verticalScrolling:true, showClock:false,
                grid:{columns:4, rows:3, mode:"screen"}}}, id))
            verify(session.execute("addResolved", {type:"text",
                placement:{x:0, y:0, width:1, height:1}}, id))
            verify(session.execute("addResolved", {type:"text",
                placement:{x:0, y:9, width:1, height:1}}, id))
            verify(session.apply())
            session.invalidate()
            return id
        }
        function runtimeHost() {
            host.visible = false
            host = createTemporaryObject(pageComponent, parent)
            verify(host !== null)
            return findChild(host, "pageContent")
        }
        function test_back_restores_page_scroll_and_collection_state() {
            host.visible = false
            const first = scrollingPage("Long first"), second = scrollingPage("Long second")
            const content = runtimeHost()
            host.showPage(first, "home-origin", false)
            const focus = homeLayout.pageModel(first).items[0].id
            content.restoreFocus(focus)
            verify(waitForRendering(content))
            tryVerify(() => content.editorGeometryReady)
            const viewport = content.editorViewport()
            viewport.composed.y = 800
            content.restoreEditorViewport(viewport)
            tryCompare(findChild(content, "composedHome"), "scrollOffset", 800)
            content.rememberAppFocus(focus, {selectedId:"app-one"})
            const before = content.editorViewport(), disk = studioFixture.disk()
            host.showPage(second, focus, false)
            compare(Object.keys(content.appCollectionFocus).length, 0)
            compare(content.editorViewport().composed.y, 0)
            host.back()
            compare(host.activeId, first)
            tryCompare(content, "focusedItemId", focus)
            tryVerify(() => content.editorViewport().composed.y === before.composed.y)
            compare(content.appCollectionFocus[focus].selectedId, "app-one")
            compare(studioFixture.disk(), disk)
        }
        function test_back_skips_removed_page_and_stale_scroll_callback() {
            host.visible = false
            const first = scrollingPage("Long first"), removed = scrollingPage("Removed"), last = scrollingPage("Last")
            const content = runtimeHost()
            host.showPage(first, "home-origin", false)
            host.showPage(removed, "", false)
            host.showPage(last, "", false)
            verify(homeLayout.removePage(removed))
            host.back()
            compare(host.activeId, first)
            // Supersede the queued restoration before it can apply to Last.
            host.showPage(last, "", true)
            tryVerify(() => content.editorViewport().composed.y === 0)
            compare(host.activeId, last)
        }
        function test_layers_reading_order_and_hidden_locked_descendants() {
            const nodes = [{id:"group", componentType:"container"},
                {id:"child", parentId:"group", hidden:true, locked:true}, {id:"last"}]
            const rows = StudioTree.layerRows(nodes, {group:true})
            compare(rows.length, 3)
            compare(rows[1].item.id, "child")
            compare(rows[1].depth, 1)
            verify(rows[1].item.hidden && rows[1].item.locked)
            compare(StudioTree.layerRows(nodes, {}).length, 2)
        }
        function test_long_layer_names_wrap_inside_the_focus_outline() {
            const editor = findChild(host, "pageStudio")
            const name = "<b>A long component name for a shared television interface</b> ".repeat(2)
            verify(editor.session.execute("patch", {id:"cell-0", value:{name:name}}))
            editor.mode = "layers"
            editor.choice = 0
            const list = findChild(editor, "studioDialogList")
            verify(list !== null)
            tryVerify(() => list.currentItem !== null)
            const label = findChild(list.currentItem, "studioDialogLabel-0")
            verify(label !== null)
            compare(label.wrapMode, Text.Wrap)
            compare(label.textFormat, Text.PlainText)
            verify(label.text.indexOf(name) >= 0)
            tryVerify(() => label.implicitHeight > label.font.pixelSize * 1.5)
            verify(list.currentItem.height >= label.implicitHeight + 16)
        }
        function test_1024_components_remain_editable_through_virtualized_layers() {
            const started=studioFixture.monotonicMilliseconds()
            function phase(name){console.info("CUTOVER_SCALE "+name+" "+(studioFixture.monotonicMilliseconds()-started)+"ms")}
            phase("begin")
            host.visible = false
            phase("hidden")
            verify(studioFixture.reset(false, true, false, false, 1024))
            phase("reset")
            host = createTemporaryObject(customizeComponent, parent)
            phase("created")
            verify(host !== null)
            const editor = findChild(host, "pageStudio")
            tryVerify(() => editor.open)
            phase("open")
            const composed=findChild(editor,"composedHome")
            verify(composed!==null)
            verify(composed.viewportItems.length<=(homeLayout.grid.rows+6)*homeLayout.grid.columns,"Rendered rows must remain bounded by the viewport")
            const targets=findChild(editor,"studioPointerTargets")
            verify(targets!==null)
            verify(targets.count<=(homeLayout.grid.rows+6)*homeLayout.grid.columns+1,"Pointer targets must remain bounded by the viewport")
            compare(editor.layerRows.length, 1024)
            const disk = studioFixture.disk()
            editor.mode = "layers"
            editor.choice = 1023
            phase("selected-last-layer")
            const list = findChild(editor, "studioDialogList")
            tryVerify(() => list.currentItem !== null && list.currentItem.objectName === "studioDialogChoice-1023")
            verify(list.contentItem.children.length < 100)
            send(inputManager.confirm)
            phase("layer-confirm")
            compare(editor.selectedId, "cell-1023")
            compare(editor.mode, "layerItem")
            send(inputManager.confirm) // Find on page is the explicit default action.
            phase("find-on-page")
            compare(editor.mode, "canvas")
            tryVerify(()=>composed.viewportItems.some(entry=>entry.item.id==="cell-1023"))
            phase("selected-visible")
            compare(studioFixture.disk(), disk)
            phase("disk-unchanged")
        }
    }
}
