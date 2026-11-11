import QtQuick
import QtTest
import "../../qml" as App

Item {
    id:scene;width:1280;height:720;focus:true
    property var target:null
    Keys.onPressed:event=>{
        const commands={};commands[Qt.Key_Up]=inputManager.navigateUp;commands[Qt.Key_Down]=inputManager.navigateDown;commands[Qt.Key_Left]=inputManager.navigateLeft;commands[Qt.Key_Right]=inputManager.navigateRight;commands[Qt.Key_Return]=inputManager.confirm;commands[Qt.Key_Escape]=inputManager.back;commands[Qt.Key_Menu]=inputManager.menu
        if(scene.target && commands[event.key]!==undefined){scene.target.handleCommand(commands[event.key]);event.accepted=true}
    }
    Component {id:centerComponent;App.SourceSettingsView {width:1000;height:650;layoutModel:homeLayout;manager:studioFixture.sources}}
    Component {id:widgetComponent;App.PageSourcesWidget {width:600;height:170;provider:studioFixture.sources;pageId:"";slotId:"test-slot";appearance:homeLayout.appearance;style:homeLayout.studioOptions.componentPresets["detected-sources"].presentation}}
    SignalSpy {id:launchSpy;signalName:"launch"}
    TestCase {
        name:"ContainedSources";when:windowShown
        function press(key){scene.forceActiveFocus();keyClick(key);wait(5)}
        function init(){scene.target=null;launchSpy.target=null;launchSpy.clear();verify(studioFixture.resetSources(8));wait(10)}
        function cleanup(){scene.target=null;launchSpy.target=null}
        function test_custom_elevated_rows_remain_readable(){
            const center=createTemporaryObject(centerComponent,scene,{appearance:{surfaceColor:"#103b38",textColor:"#f5e9cd",elevatedColor:"#f5e9cd",elevatedTextColor:"#121212",elevatedMutedColor:"#303030"}})
            verify(center);wait(30)
            const row=findChild(center,"sourceRow-1"),title=findChild(center,"sourceRowTitle-1")
            verify(row);verify(title);verify(title.text.length>0)
            function luminance(color){
                function linear(value){return value<=0.04045?value/12.92:Math.pow((value+0.055)/1.055,2.4)}
                return 0.2126*linear(color.r)+0.7152*linear(color.g)+0.0722*linear(color.b)
            }
            const foreground=luminance(title.color),background=luminance(row.color)
            verify((Math.max(foreground,background)+0.05)/(Math.min(foreground,background)+0.05)>=4.5,"Unselected source row needs readable text against its actual surface")
            if(studioFixture.artifacts)grabImage(center).save(studioFixture.artifacts+"/studio-test-source-custom-contrast.png")
        }
        function test_overflow_is_bounded_and_menu_opens_source_actions(){
            const widget=createTemporaryObject(widgetComponent,scene);verify(widget);scene.target=widget
            tryCompare(widget,"entries",studioFixture.sources.items)
            const carousel=findChild(widget,"pageAppCarousel");verify(carousel)
            compare(carousel.cards.length,4);verify(carousel.overflow);compare(widget.clip,true)
            compare(widget.width,600);compare(widget.height,170)
            const before=studioFixture.disk();launchSpy.target=widget
            press(Qt.Key_Menu);compare(launchSpy.count,1)
            compare(launchSpy.signalArguments[0][0].type,"settings");compare(launchSpy.signalArguments[0][0].section,2)
            verify(studioFixture.sources.requestedSourceId.length>0);compare(studioFixture.disk(),before)
        }
        function test_policy_cancel_and_save_are_separate_transactions(){
            const center=createTemporaryObject(centerComponent,scene);verify(center);scene.target=center
            const before=studioFixture.disk();press(Qt.Key_Return);compare(center.view,"policy")
            press(Qt.Key_Down);press(Qt.Key_Return);compare(center.view,"options")
            press(Qt.Key_Return);compare(center.draft.mode,"off");compare(studioFixture.sources.policy.mode,"widget")
            press(Qt.Key_Escape);press(Qt.Key_Escape);compare(center.view,"list");compare(studioFixture.disk(),before)
            center.selectedIndex=0;press(Qt.Key_Return);press(Qt.Key_Down);press(Qt.Key_Return)
            press(Qt.Key_Down);press(Qt.Key_Return);compare(center.draft.mode,"compact")
            press(Qt.Key_Escape);center.selectedIndex=center.fields.length;press(Qt.Key_Return)
            compare(studioFixture.sources.policy.mode,"compact");verify(studioFixture.disk()!==before)
        }
        function test_hide_restore_and_disconnected_action_recover_visible_focus(){
            const center=createTemporaryObject(centerComponent,scene);verify(center);scene.target=center
            center.selectedIndex=3;press(Qt.Key_Return);compare(center.view,"actions")
            center.selectedIndex=3;press(Qt.Key_Return);compare(studioFixture.sources.hiddenSources.length,1)
            compare(studioFixture.sources.items.length,7)
            center.selectedIndex=2;press(Qt.Key_Return);compare(center.showHidden,true)
            center.selectedIndex=3;press(Qt.Key_Return);press(Qt.Key_Return)
            compare(studioFixture.sources.hiddenSources.length,0);compare(studioFixture.sources.items.length,8)
            center.showHidden=false;center.selectedIndex=3;press(Qt.Key_Return)
            studioFixture.sourcesConnected(false);wait(5);press(Qt.Key_Return)
            verify(center.message.indexOf("disconnected")>=0);press(Qt.Key_Escape)
            compare(center.view,"list");verify(center.selectedIndex>=0 && center.selectedIndex<center.rows.length)
            compare(center.rows.length,3)
        }
        function test_notice_suppression_and_empty_widget_do_not_write_layout(){
            verify(studioFixture.sources.setPolicy({mode:"compact",duration:0}))
            const style=homeLayout.studioOptions.componentPresets["source-notice"].presentation
            const widget=createTemporaryObject(widgetComponent,scene,{notice:true,style:style});verify(widget)
            const before=studioFixture.disk();compare(widget.entries.length,8)
            widget.workflow={playback:true};compare(widget.entries.length,0)
            widget.workflow={editing:true};compare(widget.entries.length,0)
            widget.workflow={};compare(widget.entries.length,8)
            studioFixture.sourcesConnected(false);compare(widget.entries.length,0)
            compare(widget.width,600);compare(widget.height,170);compare(studioFixture.disk(),before)
            scene.target=widget;launchSpy.target=widget;press(Qt.Key_Return);compare(launchSpy.count,1)
        }
    }
}
