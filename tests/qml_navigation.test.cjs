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

test("app overflow opens All Apps while button overflow retains shortcuts", () => {
    const calls = [];
    const scope = vm.createContext({
        readOnly: false, message: "", globalGalleryRoute: true, buttons: false,
        pendingGalleryRestore: true, originId: "carousel", galleryApps: [{id: "shortcut"}],
        gallery: {open() { calls.push("shortcuts"); }},
        allAppsRequested(origin) { calls.push(origin); },
    });
    method("PageAppCarousel.qml", "activate", scope);
    scope.activate("__more__");
    assert.deepEqual(calls, ["carousel"]);
    assert.equal(scope.pendingGalleryRestore, false);
    scope.buttons = true;
    scope.activate("__more__");
    assert.deepEqual(calls, ["carousel", "shortcuts"]);
    assert.equal(scope.galleryId, "shortcut");
});

test("app overflow saves carousel focus before opening All Apps", () => {
    const source = fs.readFileSync(path.join(__dirname, "../qml/PlacedComponents.qml"), "utf8");
    const match = source.match(/onAllAppsRequested:origin=>(\{[^\n]+\})/);
    assert.ok(match, "carousel gallery handler exists");
    const calls = [], saved = {selectedId: "__more__", galleryOpen: false};
    const scope = vm.createContext({
        node: {modelData: {item: {id: "carousel"}}},
        focusState: () => saved,
        surface: {host: {
            rememberAppFocus(id, state) { calls.push(["save", id, state]); },
            allAppsRequested(origin) { calls.push(["open", origin]); },
        }},
    });
    vm.runInContext("(origin => " + match[1] + ")", scope)("carousel");
    assert.deepEqual(calls, [["save", "carousel", saved], ["open", "carousel"]]);
});

test("gallery survives synchronous and deferred VPN launch failure", () => {
    const queue = [];
    const scope = vm.createContext({
        Qt: {callLater: fn => queue.push(fn)}, launchGeneration: 0,
        launchPending: false, launchAttemptAppId: "", open: true,
    });
    scope.resumeAfterApp = () => { scope.open = true; };
    for (const name of ["dispatchFromGallery", "clearLaunchAttempt", "restoreAfterAction", "vpnLaunchFailed"])
        method("AppGallery.qml", name, scope);
    const synchronous = {dispatch(action, origin) {
        assert.equal(origin, "all-apps");
        scope.restoreAfterAction();
        assert.equal(scope.vpnLaunchFailed(action.appId), true);
        return true;
    }};
    assert.equal(scope.dispatchFromGallery(synchronous, {appId: "sample"}), true);
    assert.equal(scope.open, true);
    const deferred = {dispatch: () => true};
    scope.dispatchFromGallery(deferred, {appId: "sample"});
    assert.equal(scope.open, false);
    while (queue.length) queue.shift()();
    assert.equal(scope.launchAttemptAppId, "sample");
    scope.restoreAfterAction();
    assert.equal(scope.vpnLaunchFailed("sample"), true);
    assert.equal(scope.open, true);
    scope.dispatchFromGallery(deferred, {appId: "sample"});
    while (queue.length) queue.shift()();
    assert.equal(scope.launchAttemptAppId, "sample");
    scope.restoreAfterAction();
    assert.equal(scope.vpnLaunchFailed("sample"), true);
    scope.dispatchFromGallery(deferred, {type: "settings", section: 6});
    scope.restoreAfterAction();
    assert.equal(scope.open, true);
    while (queue.length) queue.shift()();
    assert.equal(scope.vpnLaunchFailed("sample"), false);
});

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
        sessionTransition: "fade", transitionDuration: 250,
        inputs: Array.from({length: 8}, (_, index) => ({number: index + 1, name: "HDMI " + (index + 1)})),
        result: false,
    };
    const scope = vm.createContext({
        inputManager, selectedIndex: 0, keyboardMode: 0, draftDirty: false, draft: {},
        captureController: controller,
        actionDispatcher: {
            dispatchEditorAction(action) {
                saved.push(action);
                return action.type === "capture-preview-stop" || controller.result;
            },
        },
    });
    for (const name of ["setDraftValue", "resetDraft", "configurationDraft", "applyDraft", "cancelDraft"])
        method("CaptureSettingsView.qml", name, scope);
    scope.resetDraft();
    scope.setDraftValue("serialPortName", "serial-b");
    assert.equal(scope.draftDirty, true);
    assert.equal(saved.length, 0);
    scope.cancelDraft();
    assert.equal(scope.draft.serialPortName, "serial-a");
    assert.equal(scope.draftDirty, false);
    assert.equal(JSON.stringify(saved[0]), JSON.stringify({type: "capture-preview-stop"}));
    scope.setDraftValue("commandTemplate", "HDMI {input}\\r");
    assert.equal(scope.applyDraft(), false);
    assert.equal(scope.draftDirty, true);
    controller.result = true;
    assert.equal(scope.applyDraft(), true);
    assert.equal(saved.length, 3);
    assert.equal(saved[2].type, "capture-apply");
    assert.equal(saved[2].configuration.commandTemplate, "HDMI {input}\\r");
    assert.deepEqual(Object.keys(saved[2].configuration).sort(), [
        "audioDeviceId", "baudRate", "captureDeviceId", "commandTemplate",
        "inputLabels", "selectedInput", "serialPortName", "sessionTransition", "transitionDuration", "videoFormatKey",
    ]);
    assert.equal(Object.keys(saved[2].configuration.inputLabels).length, 8);
    assert.equal(scope.draftDirty, false);
});

test("capture editor routes apply, test, preview, refresh, and cancel through typed actions", () => {
    const actions = [], inputManager = {back: 1, navigateLeft: 2, navigateRight: 3, confirm: 4};
    const scope = vm.createContext({
        selectedIndex: 9, keyboardMode: 0, draftDirty: true,
        draft: {captureDeviceId: "card", videoFormatKey: "", audioDeviceId: "", serialPortName: "ttyUSB0",
            baudRate: 9600, commandTemplate: "{input}\\r", selectedInput: 2, inputLabels: {"1":"HDMI 1"},
            sessionTransition: "fade", transitionDuration: 250},
        captureController: {
            captureDeviceId: "card", videoFormatKey: "", audioDeviceId: "", serialPortName: "ttyUSB0",
            baudRate: 9600, commandTemplate: "{input}\\r", selectedInput: 2, sessionTransition: "fade", transitionDuration: 250,
            inputs: Array.from({length: 8}, (_, index) => ({number: index + 1, name: "HDMI " + (index + 1)})),
        },
        actionDispatcher: {dispatchEditorAction: action => { actions.push(action); return true; }},
        inputManager,
    });
    for (const name of ["configurationDraft", "resetDraft", "applyDraft", "cancelDraft", "activateSelected", "handleCommand"])
        method("CaptureSettingsView.qml", name, scope);
    scope.activateSelected();
    assert.deepEqual(actions.map(action => action.type), ["capture-apply", "capture-switch-input"]);
    assert.equal(actions[1].input, 2);
    scope.selectedIndex = 10;
    scope.activateSelected();
    assert.equal(actions[2].captureDeviceId, "card");
    assert.equal(actions[2].videoFormatKey, "");
    assert.equal(actions[2].audioDeviceId, "");
    scope.selectedIndex = 11;
    scope.activateSelected();
    scope.selectedIndex = 14;
    scope.activateSelected();
    scope.cancelDraft();
    assert.deepEqual(actions.slice(2).map(action => action.type), [
        "capture-preview-draft", "capture-preview-stop", "capture-refresh", "capture-preview-stop",
    ]);
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

test("capture settings keep remote focus on screen through the last editor rows", () => {
    const calls = [], scope = vm.createContext({
        selectedIndex: 16, ListView: {Contain: "contain"},
        optionList: {positionViewAtIndex: (...args) => calls.push(args)},
    });
    method("CaptureSettingsView.qml", "keepSelectedRowVisible", scope);
    scope.keepSelectedRowVisible();
    assert.deepEqual(calls, [[16, "contain"]]);
});

test("capture session appearance choices are staged and cancel restores saved choices", () => {
    const scope = vm.createContext({
        selectedIndex: 15, draftDirty: false, draft: {sessionTransition: "fade", transitionDuration: 250},
        captureController: {
            sessionTransition: "fade", transitionDuration: 250,
            inputs: Array.from({length: 8}, (_, i) => ({number: i + 1, name: "HDMI " + (i + 1)})),
        },
        actionDispatcher: {dispatchEditorAction: () => true},
    });
    for (const name of ["setDraftValue", "adjust", "resetDraft", "cancelDraft"])
        method("CaptureSettingsView.qml", name, scope);
    scope.adjust(1);
    assert.equal(scope.draft.sessionTransition, "slide");
    scope.selectedIndex = 16;
    scope.adjust(-1);
    assert.equal(scope.draft.transitionDuration, 200);
    assert.equal(scope.draftDirty, true);
    scope.cancelDraft();
    assert.equal(scope.draft.sessionTransition, "fade");
    assert.equal(scope.draft.transitionDuration, 250);
    assert.equal(scope.draftDirty, false);
});

test("capture fallback art belongs only to the matching HDMI shortcut", () => {
    const scope = vm.createContext({
        pageSurface: {open: false},
        homeLayout: {
            captureArtworkSource: () => "",
            components: [{id: "tile", action: {type: "switch-input", input: 2}, customLogo: "artwork/a.png"},
                {id: "other", action: {type: "switch-input", input: 3}, customLogo: "artwork/b.png"}],
            resolveArtwork: asset => "managed:" + asset,
        },
    });
    method("Main.qml", "captureFallbackArtwork", scope);
    assert.equal(scope.captureFallbackArtwork(2, "tile"), "managed:artwork/a.png");
    assert.equal(scope.captureFallbackArtwork(3, "tile"), "managed:artwork/b.png");
    assert.equal(scope.captureFallbackArtwork(4, "tile"), "");
    scope.pageSurface = {open: true, model: {
        items: [{id: "page", action: {type: "switch-input", input: 2}, customLogo: "artwork/page.png"}],
    }};
    assert.equal(scope.captureFallbackArtwork(2, "page"), "managed:artwork/page.png");
    assert.equal(scope.captureFallbackArtwork(2, "tile"), "managed:artwork/a.png");
    scope.homeLayout.captureArtworkSource = input => input === 2 ? "configured:hdmi-2" : "";
    assert.equal(scope.captureFallbackArtwork(2, "tile"), "configured:hdmi-2");
});

test("capture artwork editor stages independent input choices and cancels or applies atomically", () => {
    const calls = [], inputManager = {back: 1, navigateUp: 2, navigateDown: 3, navigateLeft: 4, navigateRight: 5, confirm: 6};
    let succeeds = false;
    const scope = vm.createContext({
        artworkEditing: false, artworkDraft: {}, artworkChoices: [], artworkRow: 0, artworkMessage: "",
        inputManager,
        homeLayout: {composition: {captureInputArtwork: {"1": "artwork/a.png"}},
            captureArtworkChoices: () => [{id: "artwork/a.png", name: "A"}, {id: "artwork/b.png", name: "B"}]},
        actionDispatcher: {dispatchEditorAction: action => {calls.push(JSON.parse(JSON.stringify(action))); return succeeds;}},
    });
    for (const name of ["openArtworkEditor", "artworkName", "adjustArtwork", "handleArtworkCommand"])
        method("CaptureSettingsView.qml", name, scope);
    scope.openArtworkEditor();
    scope.handleArtworkCommand(inputManager.navigateRight);
    assert.equal(scope.artworkDraft["1"], "artwork/b.png");
    assert.equal(scope.homeLayout.composition.captureInputArtwork["1"], "artwork/a.png");
    scope.handleArtworkCommand(inputManager.back);
    assert.equal(scope.artworkEditing, false);
    assert.equal(calls.length, 0);
    scope.openArtworkEditor();
    scope.artworkRow = 1;
    scope.handleArtworkCommand(inputManager.navigateRight);
    scope.artworkRow = 8;
    scope.handleArtworkCommand(inputManager.confirm);
    assert.equal(scope.artworkEditing, true);
    assert.equal(scope.artworkDraft["2"], "artwork/a.png");
    succeeds = true;
    scope.handleArtworkCommand(inputManager.confirm);
    assert.equal(scope.artworkEditing, false);
    assert.deepEqual(calls, [
        {type: "capture-artwork-apply", artwork: {"1": "artwork/a.png", "2": "artwork/a.png"}},
        {type: "capture-artwork-apply", artwork: {"1": "artwork/a.png", "2": "artwork/a.png"}},
    ]);
});

test("capture expand uses the originating home or page rectangle when available", () => {
    const calls = [], scope = vm.createContext({
        pageSurface: {open: false, sourceRect: (id, target) => { calls.push(id); return {x: 4, y: 5, width: 40, height: 50}; }},
        captureSession: {},
        home: {
            editorRectFor: id => id === "tile" ? {x: 10, y: 20, width: 100, height: 80} : null,
            mapToItem: (_target, x, y) => ({x: x + 3, y: y + 4}),
        },
    });
    method("Main.qml", "captureOriginRect", scope);
    assert.equal(JSON.stringify(scope.captureOriginRect("tile")), JSON.stringify({x: 13, y: 24, width: 100, height: 80}));
    assert.equal(scope.captureOriginRect("missing"), null);
    scope.pageSurface.open = true;
    assert.equal(scope.captureOriginRect("page-tile").width, 40);
    assert.deepEqual(calls, ["page-tile"]);
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

test("capture session masks the picture until post-write frames are available", () => {
    const scope = vm.createContext({
        pendingSwitchConfirmation: true,
        captureController: {state: "receiving-frames", switchState: "sending"},
    });
    method("CaptureSessionView.qml", "videoReady", scope);
    assert.equal(scope.videoReady(), false);
    scope.captureController.switchState = "sent";
    scope.captureController.state = "awaiting-frames";
    assert.equal(scope.videoReady(), false);
    scope.captureController.state = "receiving-frames";
    assert.equal(scope.videoReady(), true);
    scope.captureController.state = "no-signal";
    assert.equal(scope.videoReady(), false);
    scope.captureController.state = "receiving-frames";
    scope.pendingSwitchConfirmation = false;
    assert.equal(scope.videoReady(), true);
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

test("capture session Menu releases capture before opening configuration", () => {
    const calls = [], inputManager = {back: 1, home: 2, menu: 3};
    const scope = vm.createContext({
        inputManager, open: true, originId: "source", chooseInput: false,
        captureController: {
            cancelSwitch() { calls.push("cancel"); },
            stopPreview() { calls.push("stop"); },
        },
        returned: origin => calls.push(origin),
        configureRequested: origin => calls.push("configure:" + origin),
    });
    method("CaptureSessionView.qml", "closeSession", scope);
    method("CaptureSessionView.qml", "handleCommand", scope);
    scope.handleCommand(inputManager.menu);
    assert.deepEqual(calls, ["cancel", "stop", "source", "configure:source"]);
});

test("audio output arrows preview a route, Back cancels, and Confirm applies", () => {
    const inputManager = {back: 1, navigateLeft: 2, navigateRight: 3, confirm: 4};
    const actions = [];
    const audioSettings = {
        outputs: [{id: "10", name: "TV", active: true}, {id: "11", name: "Speakers", active: false}],
        outputStatus: "Choose an output.",
    };
    const scope = vm.createContext({
        inputManager, audioSettings, selectedIndex: 10, outputDraftId: "", outputDraftActive: false,
        outputApplyPending: false,
        actionDispatcher: {
            dispatchEditorAction(action) {
                actions.push(action);
                audioSettings.outputStatus = "Switching output…";
                return true;
            },
        },
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
    assert.equal(JSON.stringify(actions), JSON.stringify([{type: "audio-output-select", outputId: "11"}]));
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
