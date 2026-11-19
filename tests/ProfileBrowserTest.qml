import QtQuick
import QtQuick.Window
import QtWebEngine
import "../qml"

Window {
    width: 640; height: 480; visible: true
    property string person: testPhase.split("-")[0]
    property bool writing: testPhase.endsWith("write")
    WebEngineView {
        id: browser
        anchors.fill: parent
        profile: IsoldeWebProfile {
            storageName: "isolde-test-" + person
            persistentStoragePath: testRoot + "/" + person
            cachePath: testRoot + "/cache-" + person
        }
        Component.onCompleted: loadHtml("<!doctype html><title>Profile isolation test</title>", "http://isolde.test/")
        onLoadingChanged: request => {
            if (request.status !== WebEngineView.LoadSucceededStatus) return
            const script = `(function() {
                const person = '${person}', writing = ${writing};
                const expected = writing ? null : person;
                function fail(message) { window.testResult = message; }
                if (localStorage.getItem('identity') !== expected) return fail('localStorage leaked or was lost');
                const cookie = document.cookie.match(/(?:^|; )identity=([^;]+)/);
                if ((cookie ? cookie[1] : null) !== expected) return fail('cookie leaked or was lost');
                if (sessionStorage.getItem('identity') !== null) return fail('old live session survived');
                const request = indexedDB.open('identity', 1);
                request.onupgradeneeded = () => request.result.createObjectStore('data');
                request.onerror = () => fail('IndexedDB open failed');
                request.onsuccess = () => {
                    const db = request.result;
                    const tx = db.transaction('data', 'readwrite');
                    const store = tx.objectStore('data');
                    const read = store.get('person');
                    let ok = true;
                    read.onsuccess = () => {
                        if ((read.result || null) !== expected) { ok = false; fail('IndexedDB leaked or was lost'); return; }
                        if (writing) store.put(person, 'person');
                    };
                    tx.onerror = () => fail('IndexedDB transaction failed');
                    tx.oncomplete = () => {
                        db.close();
                        if (!ok) return;
                        if (writing) {
                            localStorage.setItem('identity', person);
                            sessionStorage.setItem('identity', person);
                            document.cookie = 'identity=' + person + '; path=/; SameSite=Lax';
                        }
                        window.testResult = 'PASS';
                    };
                };
            })()`
            browser.runJavaScript(script)
            poll.start()
        }
    }
    Timer {
        id: poll; interval: 100; repeat: true
        onTriggered: browser.runJavaScript("window.testResult || ''", result => {
            if (!result) return
            poll.stop()
            console.log(testPhase + ": " + result)
            testCompletion.complete(result === "PASS" ? 0 : 1)
        })
    }
}
