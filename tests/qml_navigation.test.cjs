// Execute production QML JavaScript methods with only the render/event boundary
// replaced. These contracts do not claim native/QML or installed verification.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const test = require("node:test");

function method(file, name, context, anchor) {
    let source = fs.readFileSync(path.join(__dirname, "../qml", file), "utf8");
    if (anchor) {
        const offset = source.indexOf(anchor);
        assert.ok(offset >= 0, "scoped method anchor exists");
        source = source.slice(offset);
    }
    const match = new RegExp("^([ \\t]*)function " + name + "\\(", "m").exec(source);
    assert.ok(match, name + " exists in " + file);
    const start = match.index, closing = "\n" + match[1] + "}";
    const end = source.indexOf(closing, start) + closing.length;
    assert.ok(end > start, name + " has its closing brace");
    vm.runInContext(source.slice(start, end), context, {filename: file + ":" + name});
}

test("capture settings arrows change only the selected input row", () => {
    const calls = [], draft = {selectedInput: 1};
    const scope = vm.createContext({
        selectedIndex: 6, draftDirty: false, draft,
        captureController: {selectedInput: 1},
    });
    method("CaptureSettingsView.qml", "setDraftValue", scope);
    method("CaptureSettingsView.qml", "adjust", scope);
    scope.adjust(1);
    assert.equal(scope.draft.selectedInput, 1);
    scope.selectedIndex = 7;
    scope.adjust(-1);
    assert.equal(scope.draft.selectedInput, 8);
    scope.adjust(1);
    assert.equal(scope.draft.selectedInput, 1);
    assert.deepEqual(calls, []);
});

test("capture settings stage changes, apply together, and discard without saving", () => {
    const saved = [], inputManager = {back: 1, navigateLeft: 2, navigateRight: 3, confirm: 4};
    const controller = {
        captureDeviceId: "card-a", videoFormatKey: "fmt-a", audioDeviceId: "audio-a",
        serialPortName: "serial-a", baudRate: 9600, commandTemplate: "{input}\\r", selectedInput: 1,
        inputs: Array.from({length: 8}, (_, index) => ({number: index + 1, name: "HDMI " + (index + 1)})),
        applyConfiguration(configuration) { saved.push(configuration); return this.result; },
        result: false,
        stopPreview() { this.stopped = true; },
    };
    const scope = vm.createContext({
        inputManager, selectedIndex: 0, keyboardMode: 0, draftDirty: false, draft: {},
        captureController: controller,
    });
    for (const name of ["setDraftValue", "resetDraft", "applyDraft", "cancelDraft"])
        method("CaptureSettingsView.qml", name, scope);
    scope.resetDraft();
    scope.setDraftValue("serialPortName", "serial-b");
    assert.equal(scope.draftDirty, true);
    assert.equal(saved.length, 0);
    scope.cancelDraft();
    assert.equal(scope.draft.serialPortName, "serial-a");
    assert.equal(scope.draftDirty, false);
    assert.equal(controller.stopped, true);
    scope.setDraftValue("commandTemplate", "HDMI {input}\\r");
    assert.equal(scope.applyDraft(), false);
    assert.equal(scope.draftDirty, true);
    controller.result = true;
    assert.equal(scope.applyDraft(), true);
    assert.equal(saved.length, 2);
    assert.equal(saved[1].commandTemplate, "HDMI {input}\\r");
    assert.deepEqual(Object.keys(saved[1]).sort(), [
        "audioDeviceId", "baudRate", "captureDeviceId", "commandTemplate",
        "inputLabels", "selectedInput", "serialPortName", "videoFormatKey",
    ]);
    assert.equal(Object.keys(saved[1].inputLabels).length, 8);
    assert.equal(scope.draftDirty, false);
});

test("choosing a different capture card clears an incompatible staged format", () => {
    const scope = vm.createContext({
        selectedIndex: 0, draftDirty: false,
        draft: {captureDeviceId: "card-a", videoFormatKey: "fmt-a"},
        captureController: {
            videoDevices: [{id: "card-a"}, {id: "card-b"}],
            videoFormatsFor: id => id === "card-b" ? [{key: "fmt-b"}] : [{key: "fmt-a"}],
        },
    });
    method("CaptureSettingsView.qml", "setDraftValue", scope);
    method("CaptureSettingsView.qml", "cycleDevice", scope);
    method("CaptureSettingsView.qml", "adjust", scope);
    scope.adjust(1);
    assert.equal(scope.draft.captureDeviceId, "card-b");
    assert.equal(scope.draft.videoFormatKey, "");
    assert.equal(scope.draftDirty, true);
});

test("disconnected saved capture devices can be cleared or replaced", () => {
    const scope = vm.createContext({
        selectedIndex: 3, draftDirty: false, draft: {serialPortName: "gone"},
        captureController: {serialPorts: [{id: "ttyUSB0"}]},
    });
    method("CaptureSettingsView.qml", "setDraftValue", scope);
    method("CaptureSettingsView.qml", "cycleDevice", scope);
    method("CaptureSettingsView.qml", "adjust", scope);
    scope.adjust(1);
    assert.equal(scope.draft.serialPortName, "");
    scope.adjust(-1);
    assert.equal(scope.draft.serialPortName, "ttyUSB0");
});

test("capture session preserves selection for retry after a failed switch", () => {
    const calls = [], inputManager = {back: 1, home: 2, navigateLeft: 3, navigateRight: 4, confirm: 5};
    let succeeds = false;
    const scope = vm.createContext({
        inputManager, chooseInput: false, selectionPending: true, pendingSwitchConfirmation: false,
        captureController: {
            selectedInput: 4, state: "receiving-frames",
            switchInput: input => { calls.push(input); return succeeds; },
        },
    });
    method("CaptureSessionView.qml", "handleCommand", scope);
    scope.handleCommand(inputManager.confirm);
    assert.equal(scope.selectionPending, true);
    assert.equal(scope.pendingSwitchConfirmation, false);
    succeeds = true;
    scope.handleCommand(inputManager.confirm);
    assert.equal(scope.selectionPending, false);
    assert.equal(scope.pendingSwitchConfirmation, true);
    assert.deepEqual(calls, [4, 4]);
});

test("capture session retries an asynchronous switch failure and waits for serial completion", () => {
    const calls = [], inputManager = {back: 1, home: 2, navigateLeft: 3, navigateRight: 4, confirm: 5};
    const scope = vm.createContext({
        inputManager, chooseInput: false, selectionPending: false, pendingSwitchConfirmation: true,
        captureController: {
            selectedInput: 4, state: "receiving-frames", switchState: "sending",
            switchInput(input) { calls.push(input); this.switchState = "sending"; return true; },
        },
    });
    method("CaptureSessionView.qml", "handleCommand", scope);
    scope.handleCommand(inputManager.confirm);
    assert.equal(scope.pendingSwitchConfirmation, true);
    assert.deepEqual(calls, []);
    scope.captureController.switchState = "error";
    scope.handleCommand(inputManager.confirm);
    assert.deepEqual(calls, [4]);
    assert.equal(scope.pendingSwitchConfirmation, true);
    scope.captureController.switchState = "sent";
    scope.captureController.state = "awaiting-frames";
    scope.handleCommand(inputManager.confirm);
    assert.equal(scope.pendingSwitchConfirmation, true);
    scope.captureController.state = "receiving-frames";
    scope.handleCommand(inputManager.confirm);
    assert.equal(scope.pendingSwitchConfirmation, false);
});

test("closing capture session cancels pending serial output before releasing preview", () => {
    const calls = [], scope = vm.createContext({
        open: true, originId: "home:tile:hdmi", chooseInput: false,
        captureController: {
            cancelSwitch() { calls.push("cancel"); },
            stopPreview() { calls.push("stop"); },
        },
        returned: origin => calls.push(origin),
    });
    method("CaptureSessionView.qml", "closeSession", scope);
    scope.closeSession();
    assert.equal(scope.open, false);
    assert.deepEqual(calls, ["cancel", "stop", "home:tile:hdmi"]);
});

test("audio output arrows preview a route, Back cancels, and Confirm applies", () => {
    const inputManager = {back: 1, navigateLeft: 2, navigateRight: 3, confirm: 4};
    const audioSettings = {
        outputs: [{id: "10", name: "TV", active: true}, {id: "11", name: "Speakers", active: false}],
        outputStatus: "Choose an output.",
        selectOutput(id) { this.requested = id; this.outputStatus = "Switching output…"; return true; },
    };
    const scope = vm.createContext({
        inputManager, audioSettings, selectedIndex: 10, outputDraftId: "", outputDraftActive: false,
        outputApplyPending: false,
    });
    Object.defineProperty(scope, "candidateOutput", {get: () => scope.findOutput(scope.outputDraftId)});
    for (const name of ["findOutput", "beginOutputDraft", "adjustOutputDraft", "cancelOutputDraft",
        "reconcileOutputDraft", "handleCommand"])
        method("AudioSettingsView.qml", name, scope);
    scope.handleCommand(inputManager.navigateRight);
    assert.equal(scope.candidateOutput.id, "11");
    assert.equal(scope.outputDraftActive, true);
    scope.handleCommand(inputManager.back);
    assert.equal(scope.outputDraftActive, false);
    assert.equal(scope.outputDraftId, "10");
    scope.handleCommand(inputManager.navigateRight);
    scope.handleCommand(inputManager.confirm);
    assert.equal(audioSettings.requested, "11");
    assert.equal(scope.outputDraftActive, true);
    assert.equal(scope.outputApplyPending, true);
    scope.handleCommand(inputManager.back);
    assert.equal(scope.outputDraftActive, true);
    assert.equal(scope.outputDraftId, "11");
    audioSettings.outputs[0].active = false;
    audioSettings.outputs[1].active = true;
    audioSettings.outputStatus = "Choose with arrows, Enter applies.";
    scope.reconcileOutputDraft();
    assert.equal(scope.outputDraftActive, false);
});

test("a failed direct capture launch opens the input picker for retry", () => {
    const calls = [];
    const scope = vm.createContext({
        videoOutput: {}, originId: "", chooseInput: false, pendingSwitchConfirmation: false,
        selectionPending: true, open: false,
        captureController: {
            setVideoOutput: () => {}, selectInput: input => calls.push(input),
            switchInput: () => false,
        },
    });
    method("CaptureSessionView.qml", "openSession", scope);
    scope.openSession(3, "origin");
    assert.equal(scope.open, true);
    assert.equal(scope.chooseInput, true);
    assert.equal(scope.originId, "origin");
    assert.deepEqual(calls, [3]);
});

test("keyboard modes, shift, and completion work with remote commands", () => {
    const accepted = [], inputManager = {navigateLeft: 1, navigateRight: 2, navigateUp: 3, navigateDown: 4, back: 5, confirm: 6};
    const scope = vm.createContext({
        inputManager, mode: "letters", shift: false, selectedIndex: 0, value: "",
        focusRestorerId: "", accepted: value => accepted.push(value), canceled() {},
    });
    method("OnScreenKeyboard.qml", "keyList", scope);
    method("OnScreenKeyboard.qml", "press", scope);
    method("OnScreenKeyboard.qml", "handleCommand", scope);
    Object.defineProperty(scope, "keys", {get: () => scope.keyList()});
    const confirmKey = key => {
        const index = scope.keys.indexOf(key);
        assert.ok(index >= 0, `${key} is reachable in ${scope.mode}`);
        while (scope.selectedIndex < index) scope.handleCommand(inputManager.navigateRight);
        while (scope.selectedIndex > index) scope.handleCommand(inputManager.navigateLeft);
        scope.handleCommand(inputManager.confirm);
    };
    confirmKey("Shift");
    confirmKey("Q");
    assert.equal(scope.value, "Q");
    assert.equal(scope.shift, false);
    confirmKey("#+=");
    confirmKey("\\");
    confirmKey("123");
    confirmKey("2");
    confirmKey("ABC");
    confirmKey("Done");
    assert.deepEqual(accepted, ["Q\\2"]);
});

test("page Back restores its saved state and skips a removed or restricted page", () => {
    const restored = [], pages = new Set(["first", "restricted", "last"]);
    const scope = vm.createContext({
        history: [
            {id: "first", state: {focus: "origin", viewport: {legacyY: 800}}},
            {id: "removed", state: {}}, {id: "restricted", state: {}},
        ],
        activeId: "last", editing: false,
        layoutModel: {pageModel: id => pages.has(id), pageAllowed: id => id !== "restricted"},
        content: {
            resetNavigationState: state => restored.push(state),
            navigationState: () => { throw new Error("Back must not push history"); },
        },
        closeAll() { throw new Error("Valid origin must remain reachable"); },
    });
    method("PageSurface.qml", "showPage", scope);
    method("PageSurface.qml", "back", scope);
    scope.back();
    assert.equal(scope.activeId, "first");
    assert.equal(scope.history.length, 0);
    assert.equal(restored.at(-1).focus, "origin");
    assert.equal(restored.at(-1).viewport.legacyY, 800);
});

test("an exhausted history returns to permanent recovery", () => {
    let closed = 0;
    const scope = vm.createContext({
        editing: false, history: [{id: "removed", state: {}}],
        layoutModel: {pageModel: () => false}, closeAll() { ++closed; },
    });
    method("PageSurface.qml", "back", scope);
    scope.back();
    assert.equal(closed, 1);
    assert.equal(scope.history.length, 0);
});

test("deferred collection restoration cannot engage an identically named widget on another page", () => {
    const queue = [];
    let engaged = 0;
    const scope = vm.createContext({
        Qt: {callLater: callback => queue.push(callback)},
        navigationGeneration: 0, config: {}, tabs: [], activeTab: 0,
        host: {items: [{id: "same"}], appCollectionFocus: {same: {selectedId: "old-app"}}},
        widgets: {same: {restoreFocusState() { ++engaged; }}},
        scrollingPage: false, focusEntries: () => [{id: "same"}],
        content: {cancelFlick() {}}, navigationStrip: {cancelFlick() {}},
    });
    scope.root = scope;
    method("ComposedHome.qml", "restoreFocus", scope);
    method("ComposedHome.qml", "resetNavigationState", scope);
    scope.restoreFocus("same");
    scope.resetNavigationState();
    while (queue.length) queue.shift()();
    assert.equal(engaged, 0);
    assert.equal(scope.activeWidget, "");
});

test("a current-page collection callback still restores the real selection state", () => {
    const queue = [], saved = {selectedId: "app-one"};
    let received;
    const scope = vm.createContext({
        Qt: {callLater: callback => queue.push(callback)}, navigationGeneration: 1,
        host: {items: [{id: "same"}], appCollectionFocus: {same: saved}},
        widgets: {same: {restoreFocusState(state) { received = state; }}},
        scrollingPage: false, focusEntries: () => [{id: "same"}],
    });
    scope.root = scope;
    method("ComposedHome.qml", "restoreFocus", scope);
    scope.restoreFocus("same");
    while (queue.length) queue.shift()();
    assert.equal(scope.activeWidget, "same");
    assert.equal(received, saved);
});

test("a newer route cancels both phases of deferred viewport restoration", () => {
    const queue = [], restored = [];
    const scope = vm.createContext({
        Qt: {callLater: callback => queue.push(callback)}, navigationGeneration: 0,
        appCollectionFocus: {}, items: [{id: "same"}],
        composed: {resetNavigationState() {}}, sectionFlick: {cancelFlick() {}},
        repairFocus() {}, restoreFocus() {}, focusable: () => true,
        restoreEditorViewport: state => restored.push(state),
    });
    scope.root = scope;
    method("HomeScreen.qml", "resetNavigationState", scope);
    scope.resetNavigationState({focus: "same", viewport: {legacyY: 800}});
    queue.shift()(); // First deferred phase has already run.
    scope.resetNavigationState();
    while (queue.length) queue.shift()();
    assert.equal(restored.length, 0);
    scope.resetNavigationState({focus: "same", viewport: {legacyY: 400}});
    while (queue.length) queue.shift()();
    assert.equal(restored[0].legacyY, 400);
});

test("external routes cannot discard an active Studio draft or overflow history", () => {
    const rejected = [];
    let resets = 0;
    const scope = vm.createContext({
        editing: true, activeId: "first", history: [], historyLimit: 64,
        navigationRejected: reason => rejected.push(reason),
        layoutModel: {pageModel: () => true, pageAllowed: () => true},
        content: {resetNavigationState() { ++resets; }},
    });
    method("PageSurface.qml", "showPage", scope);
    assert.equal(scope.showPage("second", "", false), false);
    assert.equal(scope.editing, true);
    scope.editing = false;
    scope.history = Array.from({length: 64}, () => ({id: "first", state: {}}));
    assert.equal(scope.showPage("second", "", false), false);
    assert.equal(scope.history.length, 64);
    assert.equal(scope.activeId, "first");
    assert.equal(resets, 0);
    assert.equal(rejected.length, 2);
});

test("generic navigation errors cannot launch the Store through an unlabeled Right action", () => {
    const launches = [];
    const scope = vm.createContext({
        window: {launchErrorAppId: "", launchErrorText: "Page history is full", launchErrorVpn: false},
        inputManager: {back: 1, home: 2, confirm: 3, navigateRight: 4, navigateLeft: 5},
        sessionManager: {launch: id => launches.push(id)},
        settings: {openSection: index => launches.push("section-" + index)},
    });
    method("Main.qml", "handleCommand", scope, "id: launchErrorOverlay");
    scope.handleCommand(4);
    assert.deepEqual(launches, []);
    assert.equal(scope.window.launchErrorText, "Page history is full");
    scope.handleCommand(1);
    assert.equal(scope.window.launchErrorText, "");
    scope.window.launchErrorAppId = "missing-app";
    scope.handleCommand(4);
    assert.deepEqual(launches, ["app-store"]);
    scope.window.launchErrorVpn = true;
    scope.handleCommand(4);
    assert.deepEqual(launches, ["app-store", "settings", "section-6"]);
});
