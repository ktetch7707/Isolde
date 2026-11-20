import QtQuick
import QtQuick.Window
import QtWebEngine

Window {
    id:root;width:1000;height:720;visible:true
    title:"Isolde web remote test"
    function evaluate(source,world){web.runJavaScript(source,world,result=>remoteTest.result(result))}
    WebEngineView {
        id:web;anchors.fill:parent;focus:true
        profile:WebEngineProfile {offTheRecord:true}
        userScripts.collection:[{name:"isolde-remote",injectionPoint:WebEngineScript.DocumentReady,worldId:WebEngineScript.ApplicationWorld,sourceCode:inputManager.webRemoteScript({focusColor:"#a9326e",surfaceColor:"#f7edf4",textColor:"#332c44",cornerRadius:12,typographyScale:1})}]
        Component.onCompleted:loadHtml('<!doctype html><html><head><style>body{font:24px sans-serif;background:#f8f6fb;color:#332c44}#one,#two{position:absolute;top:70px;width:160px;height:60px;font:24px sans-serif}#one{left:40px}#two{left:230px}#entry{position:absolute;left:230px;top:170px;width:160px;height:48px;font:24px sans-serif}#choice{position:absolute;left:270px;top:290px;width:35px;height:35px}#off{display:none}</style></head><body><button id="one">First</button><button id="two" onclick="this.dataset.clicked=\'yes\'">Second</button><input id="entry" aria-label="Test text"><input type="checkbox" id="choice" aria-label="Choice"><button id="off">Hidden</button></body></html>')
        onLoadingChanged:request=>{if(request.status===WebEngineView.LoadSucceededStatus){forceActiveFocus();root.requestActivate();remoteTest.ready()}}
    }
}
