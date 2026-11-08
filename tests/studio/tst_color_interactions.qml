import QtQuick
import QtTest

Item {
    width:1280;height:720
    StudioColorCase {
        name:"StudioColorInteractions"
        when:windowShown
        function test_brightness_slider_remote_pointer_match_one_transaction(){
            givenPage(false,false);const before=document()
            editor.openInspector(true);const inspector=findChild(editor,"studioInspector"),field=homeLayout.studioOptions.inspector.pageFields.find(f=>f.key==="backgroundEnd")
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
            editor.openInspector(false);const inspector=findChild(editor,"studioInspector");inspector.selectedIds=["cell-0",second]
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
