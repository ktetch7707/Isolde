import QtQuick
import QtTest

Item {
    width:1280;height:720
    StudioColorCase {
        name:"StudioColors"
        when:windowShown
        function test_shared_color_fields_cancel_and_single_accept_data(){
            return [{tag:"light",palette:{backgroundStart:"#ffffff",backgroundEnd:"#eeeeee",surfaceColor:"#f5f5f5",textColor:"#202126"},page:false,scope:"presentation",key:"labelColor"},{tag:"pastel",palette:{backgroundStart:"#f6e5ed",backgroundEnd:"#ccd4ef",surfaceColor:"#efe6f4",textColor:"#514b67"},page:false,scope:"presentation",key:"labelColor"},{tag:"component",page:false,scope:"presentation",key:"labelColor"},{tag:"body",page:false,scope:"presentation",key:"bodyColor"},{tag:"gradient",page:false,scope:"presentation",key:"fillEndColor",kind:"shape"},{tag:"page",page:true,scope:"theme",key:"backgroundEnd"},{tag:"tint",page:true,scope:"background",key:"tintColor"},{tag:"header",page:true,scope:"headerStyle",key:"backgroundColor"},{tag:"chrome",page:true,scope:"studioAppearance",key:"textColor",editor:true}]
        }
        function test_shared_color_fields_cancel_and_single_accept(data){
            givenPage(false,false)
            if(data.palette){verify(editor.session.execute("appearance",{value:data.palette}));settle()}
            if(data.kind){verify(editor.session.execute("addResolved",{type:data.kind,placement:{x:1,y:0,width:2,height:1}}));settle();editor.selectedId=editor.items.find(i=>i.id!=="cell-0").id;verify(editor.session.execute("patch",{id:editor.selectedId,value:{presentation:{fillMode:"gradient"}}}));settle()}
            const before=document(),count=editor.session.changeCount;editor.openInspector(data.page)
            const inspector=findChild(editor,"studioInspector");inspector.editorMode=!!data.editor
                if(inspector.view==="simple")inspector.showDetails()
            const fields=data.editor?homeLayout.studioOptions.inspector.studioFields:data.page?homeLayout.studioOptions.inspector.pageFields:homeLayout.studioOptions.inspector.fields
            const field=fields.find(f=>f.scope===data.scope && f.key===data.key);verify(!!field)
            inspector.activateField(field);const picker=inspector.colorPicker;verify(picker!==null);picker.choose(data.editor?"#ffffff":"#82bce8");settle()
            compare(document(),before);compare(studioFixture.disk(),diskBaseline);verify(inspector.contrast().indexOf("NaN")<0)
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-color-field-"+data.tag+".png")
            command(inputManager.back);compare(document(),before)
            inspector.activateField(field);inspector.colorPicker.choose(data.editor?"#ffffff":"#82bce8");inspector.colorPicker.control=4;command(inputManager.confirm);settle()
            compare(editor.session.changeCount,count+1);compare(inspector.recentColors.length,1)
            verify(editor.session.undo());compare(document(),before);compare(inspector.recentColors.length,0);compare(studioFixture.disk(),diskBaseline)
        }
    }
}
