import QtQuick
import QtTest
import "../../qml" as App

Item {
    width: 1280; height: 720
    TestCase {
        id: tests
        name: "StudioGestures"
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
        function test_alignment_pad_previews_cancels_and_accepts_one_transaction() {
            scenario("arrange-preview", () => {
                verify(studioFixture.reset(false, false, false, true))
                diskBaseline = studioFixture.disk()
                editor = createTemporaryObject(editorComponent, tests.parent)
                verify(editor !== null)
                editor.show("", "cell-0");editor.toolbarFocus = false
                editor.multiSelection = ["cell-0", "cell-1", "cell-2"]
                settle()
                const before = document()
                editor.mode = "layers";editor.choice = 4
                editor.beginArrangement()
                const left = findChild(editor, "studioAlign-left")
                verify(findChild(editor,"studioPreview").freezeData)
                verify(left !== null)
                mouseClick(left)
                verify(editor.arrangementValid)
                compare(document(), before)
                compare(editor.session.changeCount, 0)
                for (const item of editor.page.items) compare(item.placement.x, 0)
                const panel = findChild(editor, "studioArrangement")
                const preview = findChild(editor, "studioPreview")
                const r = preview.placementRectFor(editor.current.placement, "", "")
                const selection = preview.mapToItem(editor, r.x, r.y)
                verify(panel.x >= selection.x + r.width || panel.x + panel.width <= selection.x, "Arrangement controls must leave the selected component visible")
                if (studioFixture.artifacts) grabImage(editor).save(studioFixture.artifacts + "/studio-test-arrange-preview.png")
                command(inputManager.back)
                compare(editor.mode, "layers")
                compare(editor.choice, 4)
                compare(document(), before)
                compare(editor.session.proposalModel, null)
                editor.beginArrangement()
                command(inputManager.confirm) // Left alignment preview.
                verify(editor.arrangementValid)
                editor.choice = 8;command(inputManager.confirm)
                compare(editor.mode, "canvas")
                compare(editor.session.changeCount, 1)
                const remote = document()
                verify(editor.session.undo());compare(document(), before)
                editor.beginArrangement()
                mouseClick(findChild(editor, "studioAlign-left"))
                mouseClick(findChild(editor, "studioArrangeDone"))
                compare(document(), remote)
                verify(editor.session.undo())
                // Invalid candidates cannot leave a stale valid preview to accept.
                verify(editor.session.execute("state", {id: "cell-1", key: "locked", value: true}))
                const locked = document(), count = editor.session.changeCount
                editor.beginArrangement();command(inputManager.confirm)
                verify(!editor.arrangementValid)
                verify(editor.message.length > 0)
                editor.choice = 8;command(inputManager.confirm)
                compare(editor.mode, "arrange")
                compare(document(), locked)
                compare(editor.session.changeCount, count)
                command(inputManager.back)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_group_layout_visual_controls_are_one_private_transaction() {
            scenario("group-layout", () => {
                verify(studioFixture.reset(false, false, false, true))
                diskBaseline=studioFixture.disk()
                editor=createTemporaryObject(editorComponent,tests.parent)
                verify(editor !== null)
                editor.show("", "cell-0");editor.toolbarFocus=false
                verify(editor.session.execute("group",{ids:["cell-0","cell-1","cell-2"]}))
                editor.selectedId=editor.items.find(item=>item.componentType==="container").id
                settle()
                const before=document(), count=editor.session.changeCount
                editor.openInspector(false)
                const inspector=findChild(editor,"studioInspector")
                verify(inspector !== null)
                if(inspector.collapsed)command(inputManager.confirm)
                command(inputManager.navigateDown);command(inputManager.navigateDown);command(inputManager.confirm)
                compare(inspector.view,"fields")
                const layoutIndex=inspector.fields.findIndex(field=>field.type==="layout")
                verify(layoutIndex>=0)
                for(let i=0;i<layoutIndex;++i)command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(editor.mode,"childLayout")
                verify(editor.childLayoutFromInspector)
                verify(findChild(editor,"studioPreview").freezeData)
                command(inputManager.navigateRight) // Grid -> Row.
                verify(editor.childLayoutValid)
                compare(editor.current.presentation.layout.mode,"row")
                compare(document(),before)
                compare(editor.session.changeCount,count)
                const controls=findChild(editor,"studioChildLayoutEditor")
                settle()
                const capacityMinus=findChild(editor,"studioChild-visibleColumns-0")
                verify(capacityMinus !== null && capacityMinus.visible)
                const paddingControl=findChild(editor,"studioChild-padding")
                // The newly visible capacity row must be laid out below the
                // padding control before synthesizing a pointer event.
                tryVerify(()=>capacityMinus.mapToItem(controls,0,0).y>=paddingControl.mapToItem(controls,0,0).y+paddingControl.height)
                mouseClick(capacityMinus)
                compare(editor.current.presentation.layout.visibleColumns,2)
                mouseClick(findChild(editor,"studioChildWrap"))
                compare(editor.current.presentation.layout.wrap,true)
                for(const field of ["gap","padding"]){
                    const slider=findChild(editor,"studioChild-"+field)
                    verify(slider !== null)
                    mouseClick(slider,slider.width-slider.rightPadding-1,slider.height/2)
                    compare(editor.current.presentation.layout[field],100)
                }
                verify(editor.childLayoutValid)
                compare(editor.page.items.filter(item=>item.parentId===editor.selectedId).length,3)
                compare(document(),before)
                compare(editor.session.changeCount,count)
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-group-layout.png")
                command(inputManager.menu);verify(controls.collapsed)
                command(inputManager.confirm);verify(!controls.collapsed)
                command(inputManager.back)
                compare(editor.mode,"inspector")
                compare(inspector.selected,layoutIndex)
                compare(document(),before)
                compare(editor.session.proposalModel,null)
                editor.beginChildLayout()
                mouseClick(findChild(editor,"studioChildMode-row"))
                verify(editor.childLayoutValid)
                mouseClick(findChild(editor,"studioChildDone"))
                compare(editor.mode,"canvas")
                compare(editor.session.changeCount,count+1)
                const pointerResult=document()
                verify(editor.session.undo());compare(document(),before)
                editor.beginChildLayout()
                command(inputManager.navigateRight)
                for(let i=0;i<6;++i)command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(document(),pointerResult)
                verify(editor.session.undo())
                editor.beginChildLayout()
                // Reducing the grid below the authored children's bounds is refused.
                mouseClick(findChild(editor,"studioChild-columns-0"))
                verify(!editor.childLayoutValid)
                mouseClick(findChild(editor,"studioChildDone"))
                compare(editor.mode,"childLayout")
                compare(document(),before)
                command(inputManager.back)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_pointer_threshold_cancel_release_and_remote_parity_data() {
            return [{tag:"full-size",zoom:1}, {tag:"zoomed-out",zoom:0.5}]
        }
        function test_pointer_threshold_cancel_release_and_remote_parity(data) {
            scenario("pointer-transaction-" + data.tag, () => {
                givenPage(false, false)
                const before = document()
                const preview = findChild(editor, "studioPreview")
                preview.parent.zoom=data.zoom
                settle()
                const hit = findChild(editor, "studioHit-cell-0")
                verify(hit !== null)
                verify(waitForRendering(hit))
                const point = hit.mapToItem(editor, hit.width / 2, hit.height / 2)
                mousePress(editor, point.x, point.y)
                mouseMove(editor, point.x + 1, point.y + 1)
                compare(editor.mode, "canvas")
                compare(document(), before)
                mouseRelease(editor, point.x + 1, point.y + 1)
                compare(editor.mode, "context")
                command(inputManager.back)
                const metrics = preview.gridMetricsFor("", "")
                const destination = point.y + (metrics.cellH + metrics.gap) * preview.parent.scale
                mousePress(editor, point.x, point.y)
                mouseMove(editor, point.x, destination)
                compare(editor.mode, "move")
                compare(editor.candidate.y, 1)
                compare(document(), before)
                command(inputManager.back)
                mouseRelease(editor, point.x, destination)
                compare(editor.mode, "canvas")
                compare(document(), before)
                compare(editor.session.changeCount, 0)
                mousePress(editor, point.x, point.y)
                mouseMove(editor, point.x, destination)
                mouseRelease(editor, point.x, destination)
                compare(editor.mode, "canvas")
                compare(editor.current.placement.y, 1)
                compare(editor.session.changeCount, 1)
                const pointerResult = document()
                verify(editor.session.undo())
                compare(document(), before)
                editor.beginGesture(false)
                command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(document(), pointerResult)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_pointer_outside_grid_refuses_stale_drop_and_can_retry() {
            scenario("pointer-boundary", () => {
                givenPage(false, false)
                const before = document(), count = editor.session.changeCount
                const hit = findChild(editor, "studioHit-cell-0")
                const preview = findChild(editor, "studioPreview")
                preview.parent.zoom=0.4
                settle()
                verify(waitForRendering(hit))
                const from = hit.mapToItem(editor, hit.width / 2, hit.height / 2)
                const metrics = preview.gridMetricsFor("", "")
                // First visit a valid target, then move beyond the left boundary.
                // Releasing must not accept that stale valid target.
                mousePress(editor, from.x, from.y)
                mouseMove(editor, from.x, from.y + (metrics.cellH + metrics.gap) * preview.parent.scale)
                compare(editor.candidate.y, 1)
                const outsideX = from.x - 2 * (metrics.cellW + metrics.gap) * preview.parent.scale
                mouseMove(editor, outsideX, from.y)
                compare(editor.mode, "move")
                compare(editor.candidate.x, -2)
                verify(!editor.collisionPreview.valid)
                mouseRelease(editor, outsideX, from.y)
                compare(editor.mode, "move")
                compare(document(), before)
                compare(editor.session.changeCount, count)
                compare(studioFixture.disk(), diskBaseline)
                // The refused proposal stays recoverable with the remote.
                command(inputManager.navigateRight)
                compare(editor.candidate.x, -1)
                command(inputManager.confirm)
                compare(editor.mode, "move")
                compare(document(), before)
                command(inputManager.navigateRight)
                compare(editor.candidate.x, 0)
                command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(editor.mode, "canvas")
                compare(editor.current.placement.y, 1)
                compare(editor.session.changeCount, count + 1)
                verify(editor.session.undo())
                compare(document(), before)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_pointer_edge_growth_is_bounded_and_matches_remote() {
            scenario("pointer-edge", () => {
                givenPage(false,true)
                const before=document(), hit=findChild(editor,"studioHit-cell-0"),preview=findChild(editor,"studioPreview")
                verify(hit !== null)
                verify(waitForRendering(hit))
                const from=hit.mapToItem(editor,hit.width/2,hit.height/2)
                const v=preview.pageViewport,to=preview.mapToItem(editor,preview.width/2,v.y+v.height-2)
                // Stay in the original column while reaching the bottom edge.
                mousePress(editor,from.x,from.y)
                mouseMove(editor,from.x,to.y)
                compare(editor.mode,"move")
                const firstRow=editor.candidate.y
                tryVerify(()=>editor.candidate.y>firstRow)
                tryCompare(hit,"edgeBudget",0)
                const lastRow=editor.candidate.y
                verify(lastRow-firstRow<=editor.grid.rows)
                wait(350) // More than three edge ticks: an idle held pointer must stop.
                compare(editor.candidate.y,lastRow)
                compare(document(),before)
                const composed=findChild(preview,"composedHome")
                verify(composed.scrollOffset>0)
                mouseRelease(editor,from.x,to.y)
                compare(editor.session.changeCount,1)
                const pointerResult=document()
                verify(editor.session.undo());compare(document(),before)
                editor.beginGesture(false)
                for(let i=0;i<lastRow;++i)command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(document(),pointerResult)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_pointer_resize_boundary_refuses_stale_size_data() {
            return [{tag:"too-wide", small:false}, {tag:"nonpositive", small:true}]
        }
        function test_pointer_resize_boundary_refuses_stale_size(data) {
            scenario("resize-boundary-" + data.tag, () => {
                givenPage(false, false)
                const before = document(), count = editor.session.changeCount
                const handle = findChild(editor, "studioResize-cell-0")
                const preview = findChild(editor, "studioPreview")
                preview.parent.zoom=0.4
                settle()
                verify(waitForRendering(handle))
                const from = handle.mapToItem(editor, handle.width / 2, handle.height / 2)
                const metrics = preview.gridMetricsFor("", "")
                const step = (metrics.cellW + metrics.gap) * preview.parent.scale
                mousePress(editor, from.x, from.y)
                mouseMove(editor, from.x + step, from.y)
                compare(editor.mode, "resize")
                compare(editor.candidate.width, 2)
                const to = from.x + (data.small ? -2 : editor.grid.columns) * step
                mouseMove(editor, to, from.y)
                verify(!editor.collisionPreview.valid)
                mouseRelease(editor, to, from.y)
                compare(editor.mode, "resize")
                if(data.small){
                    editor.resizeAxis="width"
                    command(inputManager.navigateUp) // Not an adjustment of this axis.
                    verify(editor.pointerBoundaryError.length>0)
                }
                command(inputManager.confirm)
                compare(editor.mode, "resize")
                compare(document(), before)
                compare(editor.session.changeCount, count)
                compare(studioFixture.disk(), diskBaseline)
                command(inputManager.back)
                settle()
                compare(editor.mode, "canvas")
                compare(editor.pointerBoundaryError, "")
                compare(document(), before)
                // A later valid operation must not inherit the refused target.
                editor.beginGesture(false)
                command(inputManager.navigateDown)
                command(inputManager.confirm)
                compare(editor.mode, "canvas")
                compare(editor.session.changeCount, count + 1)
                verify(editor.session.undo())
                compare(document(), before)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_resize_reflow_preview_and_blocked_drop_stay_in_operation() {
            scenario("resize-reflow", () => {
                givenPage(true, false)
                const before = document()
                editor.beginGesture(true)
                command(inputManager.navigateRight)
                compare(editor.candidate.width, 2)
                compare(editor.collisionPreview.valid, false)
                compare(document(), before)
                command(inputManager.confirm)
                compare(editor.mode, "resize")
                compare(document(), before)
                compare(editor.session.changeCount, 0)
                command(inputManager.back)
                compare(document(), before)
                // Scrolling provides room for the exact same resize/reflow.
                editor.close()
                givenPage(true, true)
                const scrollingBefore = document()
                editor.beginGesture(true)
                command(inputManager.navigateRight)
                verify(editor.collisionPreview.valid)
                verify(editor.collisionPreview.placements.length > 1)
                compare(document(), scrollingBefore)
                command(inputManager.confirm)
                compare(editor.mode, "canvas")
                compare(editor.current.placement.width, 2)
                compare(editor.session.changeCount, 1)
                verify(editor.session.undo())
                compare(document(), scrollingBefore)
                compare(studioFixture.disk(), diskBaseline)
            })
        }
        function test_ordered_pointer_insertion_matches_remote_and_cancels() {
            scenario("ordered-pointer", () => {
                verify(studioFixture.reset(true, false, true))
                diskBaseline = studioFixture.disk()
                editor = createTemporaryObject(editorComponent, tests.parent)
                verify(editor !== null)
                editor.show("", "cell-0")
                editor.toolbarFocus = false
                settle()
                verify(!editor.gridMode)
                const before = document()
                const preview = findChild(editor, "studioPreview")
                const a = preview.editorRectFor("cell-0"), b = preview.editorRectFor("cell-1")
                verify(a !== null && b !== null)
                const from = preview.mapToItem(editor, a.x + a.width / 2, a.y + a.height / 2)
                const to = preview.mapToItem(editor, b.x + b.width / 2, b.y + b.height / 2)
                mousePress(editor, from.x, from.y)
                mouseMove(editor, to.x, to.y)
                compare(editor.mode, "flowMove")
                compare(editor.flowTargetIndex, 1)
                compare(document(), before)
                command(inputManager.back)
                mouseRelease(editor, to.x, to.y)
                compare(document(), before)
                compare(editor.session.changeCount, 0)
                settle()
                mousePress(editor, from.x, from.y)
                mouseMove(editor, to.x, to.y)
                mouseRelease(editor, to.x, to.y)
                compare(editor.mode, "canvas")
                compare(editor.session.changeCount, 1)
                const pointerResult = document()
                verify(editor.session.undo())
                compare(document(), before)
                editor.beginGesture(false)
                command(inputManager.navigateRight)
                command(inputManager.confirm)
                compare(document(), pointerResult)
            })
        }
    }
}
