import QtQuick
import QtTest

Item {
    width:1280;height:720
    StudioColorCase {
        name:"StudioColorInteractions"
        when:windowShown
        function test_five_color_rows_swatches_overrides_reconnect_and_cancel(){
            givenPage(false,false);const before=document()
            editor.openInspector(true)
            const inspector=findChild(editor,"studioInspector")
            compare(inspector.view,"simple");command(inputManager.confirm);compare(inspector.view,"theme")
            compare(inspector.themeGroups.map(g=>g.label).join("|"),"Background|Cards|Text|Highlight|Accent")
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-five-color-theme.png")
            inspector.selected=5;command(inputManager.confirm);compare(inspector.view,"schemes")
            inspector.selected=2 // Choose Ocean; Midnight already matches fresh defaults.
            command(inputManager.confirm);settle();compare(editor.session.changeCount,1)
            const scheme=document();compare(studioFixture.disk(),diskBaseline)
            inspector.selected=2;command(inputManager.confirm);compare(inspector.view,"themeRoles")
            inspector.selected=1;command(inputManager.confirm);compare(inspector.view,"colorWheel")
            inspector.colorPicker.choose("#ffeedd");settle();compare(document(),scheme)
            inspector.colorPicker.control=4;command(inputManager.confirm);settle()
            compare(inspector.view,"themeRoles");compare(editor.page.appearance.headingColor,"#ffeedd")
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-individual-theme-colors.png")
            command(inputManager.back);compare(inspector.view,"theme");compare(inspector.selected,2)
            command(inputManager.navigateRight);command(inputManager.confirm);compare(inspector.view,"colorWheel")
            inspector.colorPicker.choose("#ffffff");settle();inspector.colorPicker.control=4;command(inputManager.confirm);settle()
            compare(inspector.view,"theme");compare(editor.page.appearance.headingColor,"#ffeedd")
            compare(editor.page.appearance.bodyColor,"#ffffff");compare(editor.page.appearance.labelColor,"#ffffff")
            const changed=document(),count=editor.session.changeCount
            command(inputManager.confirm);compare(inspector.view,"colorWheel")
            inspector.colorPicker.choose("#000000");settle();command(inputManager.back);settle()
            compare(document(),changed);compare(editor.session.changeCount,count)
            command(inputManager.navigateLeft);command(inputManager.confirm);compare(inspector.view,"themeRoles")
            inspector.selected=1;command(inputManager.confirm);inspector.colorPicker.control=3;command(inputManager.confirm);settle()
            inspector.colorPicker.control=4;command(inputManager.confirm);settle();compare(editor.page.appearance.headingColor,"#ffffff")
            compare(studioFixture.disk(),diskBaseline)
            while(editor.session.canUndo)verify(editor.session.undo())
            compare(document(),before)
        }
        function test_theme_pointer_swatch_and_remote_swatch_produce_same_draft(){
            givenPage(false,false);editor.openInspector(true)
            const inspector=findChild(editor,"studioInspector");command(inputManager.confirm)
            inspector.selected=0;command(inputManager.navigateRight);command(inputManager.confirm)
            inspector.colorPicker.choose("#152637");settle();inspector.colorPicker.control=4;command(inputManager.confirm);settle()
            const remote=document();verify(editor.session.undo());settle()
            const swatch=findChild(inspector,"studioThemeSwatch-0");verify(swatch!==null);tryVerify(()=>swatch.visible && swatch.width>0 && swatch.height>0)
            mouseClick(swatch,swatch.width/2,swatch.height/2);compare(inspector.view,"colorWheel")
            inspector.colorPicker.choose("#152637");settle();inspector.colorPicker.control=4;command(inputManager.confirm);settle()
            compare(document(),remote);compare(studioFixture.disk(),diskBaseline)
        }
        function test_brightness_slider_remote_pointer_match_one_transaction(){
            givenPage(false,false);const before=document()
            editor.openInspector(true);findChild(editor,"studioInspector").showDetails();const inspector=findChild(editor,"studioInspector"),field=homeLayout.studioOptions.inspector.pageFields.find(f=>f.key==="backgroundEnd")
                if(inspector.view==="simple")inspector.showDetails()
            inspector.activateField(field);let picker=inspector.colorPicker;picker.choose("#ffffff");settle()
            const slider=findChild(picker,"studioColorBrightness")
            verify(slider!==null,"the brightness slider was not created")
            verify(slider.visible && slider.width>0 && slider.height>0,"the brightness slider has no visible geometry")
            const panel=findChild(picker,"studioColorPanel")
            verify(panel!==null && panel.visible,"the color panel was not visible")
            mouseClick(slider,slider.width/2,slider.height/2);settle()
            compare(picker.hex(picker.sample),"#808080");compare(document(),before);picker.control=4;command(inputManager.confirm);const pointer=document();compare(editor.session.changeCount,1)
            verify(editor.session.undo());compare(document(),before)
            inspector.activateField(field);picker=inspector.colorPicker;picker.choose("#ffffff");picker.control=2
            for(let i=0;i<10;++i)command(inputManager.navigateLeft)
            compare(picker.hex(picker.sample),"#808080");picker.control=4;command(inputManager.confirm);compare(document(),pointer);compare(editor.session.changeCount,1)
            verify(editor.session.undo());compare(document(),before);compare(studioFixture.disk(),diskBaseline)
        }
        function test_mixed_color_preview_cancel_reset_and_recent_reopen(){
            givenPage(false,false)
            verify(editor.session.execute("addResolved",{type:"text",placement:{x:1,y:0,width:2,height:1}}));settle()
            const second=editor.items.find(i=>i.id!=="cell-0").id
            verify(editor.session.execute("patch",{id:second,value:{presentation:{bodyColor:"#ffffff"}}}));settle()
            editor.openInspector(false);findChild(editor,"studioInspector").showDetails();const inspector=findChild(editor,"studioInspector");inspector.selectedIds=["cell-0",second]
                if(inspector.view==="simple")inspector.showDetails()
            const field=homeLayout.studioOptions.inspector.fields.find(f=>f.key==="bodyColor"),before=document(),count=editor.session.changeCount
            verify(inspector.mixed(field));inspector.activateField(field);verify(inspector.colorPicker.mixedOriginal)
            inspector.colorPicker.choose("#82bce8");settle();verify(inspector.colorPicker.mixedOriginal);command(inputManager.back);compare(document(),before)
            inspector.activateField(field);inspector.colorPicker.choose("#82bce8");inspector.colorPicker.control=4;command(inputManager.confirm);settle();compare(editor.session.changeCount,count+1)
            for(const id of inspector.selectedIds)compare(editor.items.find(i=>i.id===id).presentation.bodyColor,"#82bce8")
            const accepted=document();inspector.activateField(field);inspector.colorPicker.category=2;compare(inspector.colorPicker.swatches[0].value,"#82bce8");command(inputManager.back)
            verify(editor.session.undo());compare(document(),before)
            inspector.selectedIds=[second];verify(editor.session.execute("appearance",{value:{backgroundStart:"#ffffff",backgroundEnd:"#ffffff"}}));verify(editor.session.execute("patch",{id:second,value:{presentation:{surfaceColor:"#000000",surfaceOpacity:0,bodyColor:"#ffffff"}}}));settle();inspector.activeField=field;verify(inspector.contrast().indexOf("1.0:1")>=0)
            verify(editor.session.execute("patch",{id:second,value:{presentation:{bodyColor:"#000000"}}}));settle();verify(inspector.contrast().indexOf("21.0:1")>=0)
            verify(editor.session.execute("patch",{id:second,value:{presentation:{surfaceColor:"#ffffff",surfaceOpacity:1,fillMode:"solid",fillEndColor:"#000000"}}}));settle();verify(inspector.contrast().indexOf("21.0:1")>=0)
        }
    }
}
