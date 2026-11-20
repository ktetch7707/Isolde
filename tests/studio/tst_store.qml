import QtQuick
import QtTest
import "../../qml" as App

Item {
    width:1280;height:720
    QtObject {
        id:fixtureCatalog
        property var storeOptions:({platforms:[{type:"android",label:"Android"},{type:"web",label:"Chromium"}],columns:2,spacing:20,cardHeight:142})
        property var entries:[]
        property bool busy:false
        property string errorString:""
        property bool onlineStoreSearch:false
        property int webAdds:0
        property int accessChanges:0
        property var lastWeb:({})
        signal packageInstalled(string appId)
        function setOnlineStoreSearch(value){accessChanges++;onlineStoreSearch=value}
        function addWebApp(name,url,backend){webAdds++;lastWeb={name:name,url:url,backend:backend};entries=[{id:"web-created",name:name,type:"web",storeType:"web",installed:true}];return "web-created"}
    }
    Component {id:storeComponent;App.AppStoreView {width:1280;height:720;catalog:fixtureCatalog;entries:fixtureCatalog.entries;appearance:({surfaceColor:"#202126",textColor:"#f5f1f3",focusColor:"#c994a5"})}}
    TestCase {
        name:"StoreRemote";when:windowShown
        function init(){fixtureCatalog.entries=[];fixtureCatalog.onlineStoreSearch=false;fixtureCatalog.webAdds=0;fixtureCatalog.accessChanges=0;fixtureCatalog.lastWeb={}}
        function makeStore(platform){const store=createTemporaryObject(storeComponent,parent);verify(store!==null);store.choosePlatform(platform);return store}
        function command(store,value){verify(store.handleCommand(value))}
        function test_web_menu_cancel_reopen_keyboard_and_single_save(){
            const store=makeStore(1)
            command(store,inputManager.menu);verify(store.toolsMenuOpen);verify(store.toolsItems[0].indexOf("Add website")>=0)
            command(store,inputManager.confirm);verify(store.addWebOpen);compare(store.webField,0);compare(fixtureCatalog.accessChanges,0)
            command(store,inputManager.back);verify(!store.addWebOpen);compare(fixtureCatalog.webAdds,0);compare(store.area,"tools")
            command(store,inputManager.confirm);verify(store.addWebOpen);compare(store.webField,0)
            command(store,inputManager.confirm);verify(store.searchOpen)
            const keyboard=findChild(store,"storeKeyboard");verify(keyboard!==null);keyboard.accepted("Fixture website")
            compare(store.webName,"Fixture website");verify(!store.searchOpen)
            command(store,inputManager.navigateDown);command(store,inputManager.confirm);verify(store.searchOpen);keyboard.accepted("https://example.org/")
            compare(store.webAddress,"https://example.org/")
            for(let i=0;i<3;i++)command(store,inputManager.navigateDown)
            command(store,inputManager.confirm);compare(fixtureCatalog.webAdds,1);compare(fixtureCatalog.lastWeb.name,"Fixture website");compare(fixtureCatalog.lastWeb.url,"https://example.org/")
            verify(!store.addWebOpen);compare(store.selectedId,"web-created");compare(store.area,"actions");compare(fixtureCatalog.accessChanges,0)
        }
        function test_android_menu_toggles_access_without_web_form(){
            const store=makeStore(0)
            command(store,inputManager.menu);verify(store.toolsItems[0].indexOf("F-Droid")>=0);command(store,inputManager.confirm)
            verify(fixtureCatalog.onlineStoreSearch);compare(fixtureCatalog.accessChanges,1);verify(!store.addWebOpen);compare(fixtureCatalog.webAdds,0)
            command(store,inputManager.menu);command(store,inputManager.confirm);verify(!fixtureCatalog.onlineStoreSearch);compare(fixtureCatalog.accessChanges,2)
        }
        function test_menu_back_restores_origin_without_mutation(){
            const store=makeStore(1);store.area="header";store.headerIndex=3
            command(store,inputManager.menu);command(store,inputManager.navigateDown);command(store,inputManager.back)
            verify(!store.toolsMenuOpen);compare(store.area,"header");compare(store.headerIndex,3);compare(fixtureCatalog.webAdds,0);compare(fixtureCatalog.accessChanges,0);verify(!store.addWebOpen)
        }
    }
}
