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
        function pageColor(key) {
            editor.openInspector(true)
            const inspector=findChild(editor,"studioInspector")
            inspector.activateField(homeLayout.studioOptions.inspector.pageFields.find(f=>f.key===key))
            compare(inspector.view,"colorWheel")
            return findChild(inspector,"studioColorPicker")
        }
        function test_page_color_swatches_preview_cancel_and_accept_once() {
            scenario("color-swatches",()=>{
                givenPage(false,false)
                const before=document(), picker=pageColor("backgroundStart")
                verify(waitForRendering(picker))
                mouseClick(findChild(picker,"studioColorSwatch-2"))
                // Preview replaces the model object; observe the current
                // editor binding rather than retaining the prior model.
                tryVerify(()=>editor.page.backgroundStart==="#e89ab0")
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-color-presets.png")
                compare(document(),before)
                compare(editor.session.changeCount,0)
                compare(studioFixture.disk(),diskBaseline)
                command(inputManager.back)
                compare(document(),before)
                const again=pageColor("backgroundStart")
                again.choose("#82bce8")
                again.choose("#91d4b8")
                again.control=4;command(inputManager.confirm)
                compare(editor.session.changeCount,1)
                compare(editor.session.document.theme.backgroundStart,"#91d4b8")
                compare(findChild(editor,"studioInspector").recentColors.length,1)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_color_wheel_pointer_remote_parity_and_cancel() {
            scenario("color-wheel",()=>{
                givenPage(false,false)
                const before=document(),picker=pageColor("backgroundStart")
                picker.updateHsv(0,1,1)
                verify(waitForRendering(picker))
                mouseClick(findChild(picker,"studioColorCategory-3"))
                compare(picker.category,3)
                const wheel=findChild(picker,"studioColorWheel")
                verify(waitForRendering(wheel))
                mouseClick(wheel,wheel.width-4,wheel.height/2)
                compare(picker.hex(picker.sample),"#ff0000")
                mouseClick(wheel,4,wheel.height/2)
                compare(picker.hex(picker.sample),"#00ffff")
                tryVerify(()=>editor.page.backgroundStart==="#00ffff")
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-color-wheel.png")
                picker.control=4;command(inputManager.confirm)
                const pointerDocument=document()
                verify(editor.session.undo());compare(document(),before)
                const remote=pageColor("backgroundStart")
                remote.updateHsv(0,1,1);remote.category=3;remote.control=1
                command(inputManager.confirm)
                for(let step=0;step<36;++step)command(inputManager.navigateRight)
                compare(remote.hex(remote.sample),"#00ffff")
                command(inputManager.back) // Leave two-axis adjustment, keep picker.
                compare(findChild(editor,"studioInspector").view,"colorWheel")
                remote.control=4;command(inputManager.confirm)
                compare(document(),pointerDocument)
                verify(editor.session.undo());compare(document(),before)
                pageColor("backgroundStart").choose("#f2ce75")
                command(inputManager.back)
                compare(document(),before)
                compare(findChild(editor,"studioInspector").recentColors.length,1)
            })
        }
        function test_page_color_reset_removes_override_and_is_undoable() {
            scenario("color-reset",()=>{
                givenPage(false,false)
                let picker=pageColor("textColor")
                picker.choose("#f2ce75");picker.control=4;command(inputManager.confirm)
                const custom=document(),history=editor.session.changeCount
                picker=pageColor("textColor")
                picker.control=3;command(inputManager.confirm)
                compare(document(),custom)
                compare(editor.session.changeCount,history)
                picker.control=4;command(inputManager.confirm)
                verify(editor.session.document.theme.textColor===undefined)
                compare(editor.session.changeCount,history+1)
                verify(editor.session.undo());compare(document(),custom)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_exact_color_validation_keeps_preview_private() {
            scenario("color-exact",()=>{
                givenPage(false,false)
                const before=document(),picker=pageColor("backgroundEnd")
                // A pending wheel/swatch preview must remain a preview while
                // the optional exact-input subdialog is open.
                picker.choose("#e89ab0")
                picker.control=6;command(inputManager.confirm)
                const inspector=findChild(editor,"studioInspector"),keyboard=findChild(inspector,"inspectorKeyboard")
                compare(inspector.view,"keyboard")
                settle();compare(document(),before);compare(editor.session.changeCount,0)
                keyboard.accepted("#zzzzzz")
                compare(inspector.view,"keyboard");compare(document(),before)
                keyboard.accepted("#123456")
                compare(inspector.view,"colorWheel")
                compare(picker.hex(picker.sample),"#123456")
                compare(document(),before);compare(editor.session.changeCount,0)
                command(inputManager.back)
                compare(document(),before);compare(studioFixture.disk(),diskBaseline)
            })
        }
        function scalarField(key,asPage) {
            editor.openInspector(asPage)
            const inspector=findChild(editor,"studioInspector")
            const fields=asPage?homeLayout.studioOptions.inspector.pageFields:homeLayout.studioOptions.inspector.fields
            inspector.activateField(fields.find(f=>f.key===key))
            compare(inspector.view,"scalar")
            return inspector
        }
        function test_scalar_remote_cancel_pointer_commit_and_parity() {
            scenario("scalar-opacity",()=>{
                givenPage(false,false)
                const before=document()
                let inspector=scalarField("surfaceOpacity",false)
                compare(inspector.scalarText(inspector.scalarDraft),"100%")
                for(let i=0;i<20;++i)command(inputManager.navigateLeft)
                compare(inspector.scalarDraft,0)
                settle();compare(document(),before);compare(editor.session.changeCount,0)
                command(inputManager.back);settle();compare(document(),before)
                inspector=scalarField("surfaceOpacity",false)
                const slider=findChild(inspector,"studioScalarSlider")
                verify(waitForRendering(slider));mouseClick(slider,slider.leftPadding+1,slider.height/2)
                compare(inspector.scalarDraft,0)
                inspector.scalarControl=2;command(inputManager.confirm)
                const pointer=document();compare(editor.session.changeCount,1)
                verify(editor.session.undo());compare(document(),before)
                inspector=scalarField("surfaceOpacity",false)
                for(let i=0;i<20;++i)command(inputManager.navigateLeft)
                inspector.scalarControl=2;command(inputManager.confirm)
                compare(document(),pointer)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_scalar_exact_percent_preview_and_reset() {
            scenario("scalar-dimming",()=>{
                givenPage(false,false)
                const before=document(),inspector=scalarField("dim",true)
                inspector.changeScalar(.5,false)
                inspector.scalarControl=4;command(inputManager.confirm)
                settle();compare(document(),before);compare(editor.session.changeCount,0)
                const keyboard=findChild(inspector,"inspectorKeyboard")
                keyboard.accepted("120");compare(inspector.view,"keyboard")
                command(inputManager.back);compare(inspector.view,"scalar");compare(inspector.message,"")
                inspector.scalarControl=4;command(inputManager.confirm)
                keyboard.accepted("25");compare(inspector.view,"scalar")
                compare(inspector.scalarDraft,.25);compare(inspector.scalarText(inspector.scalarDraft),"25%")
                settle();compare(document(),before)
                const panel=findChild(inspector,"studioScalarPanel")
                tryVerify(()=>panel.width>0 && panel.width<editor.width && panel.height>0 && panel.height<editor.height && panel.x>=0 && panel.x+panel.width<=editor.width)
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-scalar-dimming.png")
                inspector.scalarControl=2;command(inputManager.confirm)
                compare(editor.session.changeCount,1)
                compare(editor.session.document.composition.background.dim,.25)
                const custom=document()
                scalarField("dim",true).changeScalar(.25,true)
                command(inputManager.back);settle();compare(document(),custom)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
        function test_scalar_corner_presets_and_custom_value_preservation() {
            scenario("scalar-corners",()=>{
                givenPage(false,false)
                verify(editor.session.execute("patch",{id:"cell-0",value:{presentation:{radius:23}}}))
                const before=document(),history=editor.session.changeCount
                const inspector=scalarField("radius",false)
                compare(inspector.scalarDraft,23) // Opening never snaps an existing custom value.
                compare(findChild(inspector,"studioScalarSlider").value,23)
                compare(document(),before)
                command(inputManager.navigateDown);command(inputManager.confirm)
                compare(inspector.scalarDraft,0)
                command(inputManager.navigateRight);command(inputManager.confirm)
                compare(inspector.scalarDraft,22)
                command(inputManager.navigateRight);command(inputManager.confirm)
                compare(inspector.scalarDraft,100)
                settle();compare(document(),before)
                const panel=findChild(inspector,"studioScalarPanel")
                tryVerify(()=>panel.width>0 && panel.width<editor.width && panel.height>0 && panel.height<editor.height && panel.x>=0 && panel.x+panel.width<=editor.width)
                if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-scalar-corners.png")
                inspector.scalarControl=3;command(inputManager.confirm)
                compare(editor.session.changeCount,history+1)
                compare(editor.current.presentation.radius,100)
                verify(editor.session.undo());compare(document(),before)
                compare(studioFixture.disk(),diskBaseline)
            })
        }
    }
}
