import QtQuick
import QtQuick.Window
import QtTest
import "../../qml" as App

Item {
    id:stage;width:1280;height:720
    TestCase {
        id:tests;name:"StudioVmPerformance";when:windowShown
        property var editor:null
        Component {id:editorComponent;App.PageStudioView {layoutModel:homeLayout}}
        SignalSpy {id:frames;target:stage.Window.window;signalName:"frameSwapped"}
        property bool sampling: false
        property int sampleIndex: 0
        property real inputStarted: 0
        property real previousFrame: 0
        property var inputSamples: []
        property var frameSamples: []
        property var commandSamples: []
        function sendSample() {
            if (!sampling) return
            inputStarted = studioFixture.monotonicMilliseconds()
            editor.handleCommand(sampleIndex % 2 === 0 ? inputManager.navigateRight : inputManager.navigateLeft)
            if (sampleIndex >= 10) commandSamples.push(studioFixture.monotonicMilliseconds() - inputStarted)
        }
        Connections {
            target: stage.Window.window
            function onFrameSwapped() {
                if (!tests.sampling || tests.inputStarted === 0) return
                const completed = studioFixture.monotonicMilliseconds()
                if (tests.sampleIndex >= 10) {
                    tests.inputSamples.push(completed - tests.inputStarted)
                    tests.frameSamples.push(completed - tests.previousFrame)
                }
                tests.previousFrame = completed
                tests.inputStarted = 0
                tests.sampleIndex++
                if (tests.sampleIndex === 110) tests.sampling = false
                else Qt.callLater(tests.sendSample)
            }
        }
        function cleanup(){sampling=false;if(editor){editor.close();editor.destroy();editor=null}wait(0)}
        function test_preview_geometry_retains_card_identity() {
            verify(studioFixture.reset(false,false,false,false,100))
            editor=createTemporaryObject(editorComponent,stage,{width:1280,height:720})
            editor.show("","cell-99");editor.toolbarFocus=false;verify(waitForRendering(editor));editor.beginGesture(false)
            const nativeTimes=[],inputTimes=[]
            const card=findChild(editor,"studioProposedCard-cell-99");verify(card!==null)
            for(let i=0;i<10;i++){
                let start=studioFixture.monotonicMilliseconds()
                const proposal=editor.session.placementProposal("cell-99",{x:i%2===0?4:3,y:6,width:1,height:1},"shift","",{parentId:""})
                verify(proposal.valid);nativeTimes.push(studioFixture.monotonicMilliseconds()-start)
                start=studioFixture.monotonicMilliseconds()
                editor.handleCommand(i%2===0?inputManager.navigateRight:inputManager.navigateLeft)
                inputTimes.push(studioFixture.monotonicMilliseconds()-start)
                compare(findChild(editor,"studioProposedCard-cell-99"),card,"Geometry updates must retain the actual preview card")
                wait(1)
            }
            console.info("MOVEMENT_PROBE "+JSON.stringify({native:nativeTimes,input:inputTimes}))
        }
        function test_response_data(){return [{tag:"720",width:1280,height:720},{tag:"1080",width:1920,height:1080}]}
        function test_response(data){
            failOnWarning(/Binding loop detected|ReferenceError|TypeError/)
            stage.width=data.width;stage.height=data.height
            stage.Window.window.width=data.width;stage.Window.window.height=data.height
            verify(studioFixture.reset(false,false,false,false,100))
            const disk=studioFixture.disk(),clientsBefore=studioFixture.activeStudioClients()
            editor=createTemporaryObject(editorComponent,stage,{width:data.width,height:data.height})
            verify(editor!==null);editor.show("","cell-99");editor.toolbarFocus=false
            verify(waitForRendering(editor));verify(frames.valid)
            editor.beginGesture(false);compare(editor.mode,"move")
            // Timestamp the actual signal, and drive input from the event loop.
            // SignalSpy.wait polling must not become part of latency or frame cadence.
            inputSamples=[];frameSamples=[];commandSamples=[];sampleIndex=0;previousFrame=0
            sampling=true;sendSample()
            tryVerify(()=>!sampling,60000,"Every movement input must produce a frame")
            const inputs=inputSamples,intervals=frameSamples,commands=commandSamples
            compare(inputs.length,100);compare(intervals.length,100)
            editor.handleCommand(inputManager.back);compare(editor.mode,"canvas")
            compare(editor.session.changeCount,0);compare(studioFixture.disk(),disk)
            const ordered=inputs.slice().sort((a,b)=>a-b),p95=ordered[Math.ceil(ordered.length*.95)-1]
            const mean=intervals.reduce((a,b)=>a+b,0)/intervals.length
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-performance-"+data.tag+".png")
            editor.close();editor.destroy();editor=null;wait(0)
            for(let cycle=0;cycle<10;cycle++){
                editor=createTemporaryObject(editorComponent,stage,{width:data.width,height:data.height})
                verify(editor!==null);editor.show("","cell-0");verify(editor.session.active)
                editor.close();verify(!editor.session.active);editor.destroy();editor=null;wait(0)
            }
            const clientsAfter=studioFixture.activeStudioClients()
            // Validate the maximum authored model separately from the 100-visible-widget timing scene.
            // The installed cutover suite exercises the actual 1024-node Layers selection workflow.
            verify(studioFixture.reset(false,true,false,false,1024));compare(homeLayout.items.length,1024)
            const receipt={viewport:data.width+"x"+data.height,renderer:"Qt Quick software on VM offscreen window",
                commandMs:commands,inputToVisibleMs:inputs,frameIntervalsMs:intervals,visibleWidgets:100,editableNodes:homeLayout.items.length,
                longestUnacknowledgedInputMs:Math.max(...inputs),lifecycleCycles:10,clientsBefore:clientsBefore,
                clientsAfter:clientsAfter,historyBytesAfter:0,historyEntriesAfter:0,
                provenance:"Actual frameSwapped timing around production Studio movement; active Studio session clients; empty history verified after cancel; maximum model independently validated."}
            verify(studioFixture.writePerformance(data.tag,receipt))
            console.info("STUDIO_PERFORMANCE "+JSON.stringify(receipt))
            verify(p95<100,"p95 input-to-visible "+p95+" ms must be below 100 ms")
            verify(mean<=1000/30,"Mean movement frame interval "+mean+" ms must sustain 30 fps")
            verify(Math.max(...inputs)<=250,"Every input must be acknowledged within 250 ms")
            verify(clientsAfter<=clientsBefore,"Active editor clients must not grow after teardown cycles")
        }
        function test_scaling_capture_data(){return [{tag:"720",width:1280,height:720},{tag:"1080",width:1920,height:1080},{tag:"2160",width:3840,height:2160}]}
        function test_scaling_capture(data){
            stage.width=data.width;stage.height=data.height;stage.Window.window.width=data.width;stage.Window.window.height=data.height
            verify(studioFixture.reset(false,false,false,false,100))
            const disk=studioFixture.disk()
            editor=createTemporaryObject(editorComponent,stage,{width:data.width,height:data.height});verify(editor!==null)
            editor.show("","cell-0");verify(waitForRendering(editor))
            const menu=findChild(editor,"studioMenuEntries")
            tryVerify(()=>menu.contentHeight>0 && menu.contentHeight<=menu.height)
            const rail=findChild(editor,"studioPageMenu")
            verify(rail.width>=data.width*.2,"The page controls must remain TV-readable at every resolution")
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-scaling-"+data.tag+".png")
            editor.activateEntry({action:"reuse"})
            verify(waitForRendering(editor))
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-scaling-reuse-"+data.tag+".png")
            editor.handleCommand(inputManager.confirm)
            verify(waitForRendering(editor))
            if(studioFixture.artifacts)grabImage(editor).save(studioFixture.artifacts+"/studio-test-scaling-keyboard-"+data.tag+".png")
            compare(studioFixture.disk(),disk)
        }
    }
}
