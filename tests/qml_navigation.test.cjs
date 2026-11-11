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

test("direct browse filters use the remote chooser and the same next-page query", () => {
    const calls = [];
    const scope = vm.createContext({
        mode: "platformChooser", choice: 2, platformChoices: ["PC"], queryView: "collection",
        activeCollectionId: "saved", search: "", selectedPlatform: "PC", selected: -1,
        selectedId: "", offset: 50, field: 0, message: "", keyboard: {value: "", selectedIndex: 0},
        library: {
            page(...args) { calls.push(args); return args[2] === 0 ? Array.from({length: 50}, (_, i) => ({id: String(i)})) : [{id: "next"}]; },
        },
        pageSize: 50, catalogLimit: 50000, detail: null,
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    method("GamesLibraryWidget.qml", "reload", scope);
    method("GamesLibraryWidget.qml", "submitDirectFilter", scope);
    scope.activateChoice();
    assert.equal(scope.mode, "keyboard");
    assert.equal(scope.field, 22);
    scope.submitDirectFilter("  ");
    assert.equal(scope.mode, "keyboard");
    assert.equal(scope.queryView, "collection");
    scope.submitDirectFilter(" Saga ");
    assert.equal(scope.queryView, "franchise");
    assert.equal(scope.offset, 0);
    assert.equal(scope.activeCollectionId, "");
    assert.deepEqual(calls, [["franchise", "Saga", 0, ""], ["franchise", "Saga", 50, ""]]);
    assert.equal(scope.canNextPage, true);
    scope.mode = "platformChooser"; scope.choice = 3;
    scope.activateChoice();
    assert.equal(scope.field, 23);
    scope.submitDirectFilter("Puzzle");
    assert.equal(scope.queryView, "tag");
    assert.deepEqual(calls.slice(-2), [["tag", "Puzzle", 0, ""], ["tag", "Puzzle", 50, ""]]);
});

test("DS and Virtual Boy setup exposes controller limitations remotely", () => {
    const source = fs.readFileSync(path.join(__dirname, "../qml/GamesLibraryWidget.qml"), "utf8");
    const actions = source.match(/readonly property var emulatorActions:(.*)/)[1];
    for (const [id, hint, controls] of [
        ["retroarch-ds", /right stick moves the stylus/i, /right trigger touches/i],
        ["retroarch-vb", /second D-pad/i, /right trigger left/i],
    ]) {
        const scope = vm.createContext({
            mode: "emulators", choice: 0, message: "", emulatorId: "",
            emulators: [{id, name: id, installed: false, registered: false, firmwareRequired: false}],
            library: {emulatorSettings() { return {schemaVersion: 1, video: "automatic", controller: "xbox"}; }},
        });
        method("GamesLibraryWidget.qml", "activateChoice", scope);
        scope.activateChoice();
        assert.equal(scope.mode, "emulatorSettings");
        assert.match(scope.message, hint);
        const label=vm.runInContext(actions, scope)[1];
        assert.match(label, id==="retroarch-ds"?/Controller help: Xbox · DS/:/Controller help: Xbox · Virtual Boy/);
        assert.doesNotMatch(label,/Wii/);
        scope.choice = 1;
        scope.activateChoice();
        assert.match(scope.message, controls);
        assert.equal(scope.emulatorDraft.controller,"xbox");
    }
});

test("PlayStation firmware selection is reviewed before saving", () => {
    const calls = [];
    const scope = vm.createContext({
        mode: "emulators", choice: 0, message: "", emulatorId: "", firmwareDraft: "",
        keyboard: {selectedIndex: -1, value: ""},
        emulators: [{id:"retroarch-psx", name:"PlayStation", installed:true, registered:true, firmwareRequired:true}],
        library: {
            emulatorSettings() { return {schemaVersion:1, video:"automatic", controller:"xbox", firmware:""}; },
            configureEmulator(...args) { calls.push(args); return true; },
        },
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    scope.activateChoice();
    assert.match(scope.message,/single CHD/);
    assert.match(scope.message,/matching regional/);
    scope.choice=2;scope.activateChoice();
    assert.equal(scope.mode,"keyboard");assert.equal(scope.field,5);assert.equal(calls.length,0);
    scope.firmwareDraft="/media/games/user-firmware.bin";scope.mode="emulatorSettings";scope.choice=3;
    scope.activateChoice();
    assert.equal(calls.length,1);assert.equal(calls[0][0],"retroarch-psx");
    assert.equal(calls[0][2],scope.firmwareDraft);
});

test("pending uninstall cleanup requires the separate remote confirmation", () => {
    const calls = [];
    const inputManager = {back: 1, navigateDown: 2, navigateRight: 3, navigateUp: 4, navigateLeft: 5, confirm: 6};
    const scope = vm.createContext({
        inputManager, readOnly: false, mode: "uninstallRecoveryReview", choice: 0,
        pendingUninstall: {id: "install-id"}, message: "",
        library: {
            resumeOrphanedUninstall(id, confirmed) { calls.push([id, confirmed]); return true; },
            portableInstallStatus: "Checking files",
        },
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    method("GamesLibraryWidget.qml", "handleCommand", scope);
    scope.handleCommand(inputManager.navigateDown);
    scope.handleCommand(inputManager.navigateDown);
    assert.equal(scope.choice, 2);
    assert.deepEqual(calls, []);
    scope.handleCommand(inputManager.confirm);
    assert.deepEqual(calls, [["install-id", true]]);
    assert.equal(scope.mode, "uninstalling");
    assert.equal(scope.message, "Checking files");
});

test("standalone portable install requires remote review and confirmation", () => {
    const calls = [];
    const scope = vm.createContext({
        readOnly: false, mode: "catalogTools", choice: 4, message: "",
        opticalObservationId: "old", opticalDevice: "old", registeringOpticalGame: true,
        runtimes: [{id: "native", name: "Native", file: true}],
        keyboard: {selectedIndex: -1, value: "old"},
        library: {
            confirmPortableInstall() { calls.push("install"); return true; },
            portableInstallStatus: "Installing",
        },
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    scope.activateChoice();
    assert.equal(scope.mode, "installerRuntime");
    assert.equal(scope.registeringOpticalGame, false);
    assert.match(scope.message, /only the standalone file is copied/i);
    scope.activateChoice();
    assert.equal(scope.field, 17);
    assert.equal(scope.mode, "keyboard");
    assert.equal(scope.portableRuntimeId, "native");
    assert.deepEqual(calls, []);
    scope.mode = "installerReview";
    scope.choice = 1;
    scope.activateChoice();
    assert.deepEqual(calls, ["install"]);
    assert.equal(scope.mode, "installerInstalling");
});

test("portable file preflight and confirmation use the same profile media grant", () => {
    const source = fs.readFileSync(path.join(__dirname, "../src/games/GameLibrary.cpp"), "utf8");
    const begin = source.slice(source.indexOf("bool GameLibrary::beginPortableInstall("),
                               source.indexOf("bool GameLibrary::beginPortableDirectoryInstall("));
    assert.match(begin, /opticalObservationId\.isEmpty\(\)\s*&&\s*!approvedMediaDirectory\(m_emulatorProfile,file\.absolutePath\(\)\)/);
});

test("optical portable confirmation uses disc identity rather than media-folder grants", () => {
    const source = fs.readFileSync(path.join(__dirname, "../src/games/GameLibrary.cpp"), "utf8");
    const start = source.indexOf("bool GameLibrary::confirmPortableInstall()");
    const end = source.indexOf("\nvoid GameLibrary::cancelPortableInstall()", start);
    assert.ok(start >= 0 && end > start, "portable confirmation implementation exists");
    const confirmation = source.slice(start, end);
    assert.match(confirmation, /m_portableDirectoryCopy[\s\S]*approvedMediaDirectory\(m_emulatorProfile,m_portableSource\)/);
    assert.match(confirmation, /!m_portableOpticalObservationId\.isEmpty\(\)\s*\|\|\s*approvedMediaDirectory\(m_emulatorProfile,source\.absolutePath\(\)\)/);
    assert.match(confirmation, /revalidateOpticalDisc\(m_portableOpticalObservationId,m_portableOpticalDevice\)/);
    assert.doesNotMatch(confirmation, /MediaGrants::validate\(source\.absolutePath\(\)\)/);
});

test("local game directory copy is an explicit remote review workflow", () => {
    const calls = [];
    const scope = vm.createContext({
        readOnly: false, mode: "catalogTools", choice: 9, message: "",
        opticalObservationId: "", opticalDevice: "", registeringOpticalGame: false,
        runtimes: [{id: "native", name: "Native", file: true}],
        keyboard: {selectedIndex: -1, value: ""},
        library: {
            beginPortableDirectoryInstall(...args) { calls.push(args); return true; },
            portableInstallStatus: "Preflight started",
        },
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    method("GamesLibraryWidget.qml", "submitPortableDirectoryField", scope);
    scope.activateChoice();
    assert.equal(scope.installingDirectory, true);
    assert.equal(scope.mode, "installerRuntime");
    scope.activateChoice();
    assert.equal(scope.field, 20);
    assert.equal(scope.mode, "keyboard");
    scope.submitPortableDirectoryField("/media/approved/Sample Game");
    assert.equal(scope.field, 21);
    assert.deepEqual(calls, []);
    scope.submitPortableDirectoryField("bin/game");
    assert.deepEqual(calls, [["/media/approved/Sample Game", "bin/game", "native"]]);
    assert.equal(scope.mode, "installerPreflight");
});

test("optical remote actions retain the selected media identity and require eject confirmation", () => {
    const calls = [];
    const disc = {observationId: "selected", device: "/dev/sr0", state: "mounted",
        volumeLabel: "Fixture", filesystem: "iso9660", mountRoot: "/media/disc",
        directPlaySupported: true};
    let mediaState = "replaced";
    const scope = vm.createContext({
        readOnly: false, mode: "opticalChooser", choice: 0, message: "", opticalChoices: [disc],
        library: {
            revalidateOpticalDisc(id, device) { calls.push(["revalidate", id, device]); return {state: mediaState, disc}; },
            ejectOpticalDisc(id, device) { calls.push(["eject", id, device]); return false; },
            mountOpticalDisc(id, device) { calls.push(["mount", id, device]); return true; },
            mountedOpticalDiscs() { return []; },
            errorString: "The selected disc or drive changed.",
        },
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    scope.activateChoice();
    assert.equal(scope.mode, "opticalDetails");
    scope.activateChoice();
    assert.match(scope.message, /different media/i);
    assert.deepEqual(calls, [["revalidate", "selected", "/dev/sr0"]]);
    scope.choice = 4;
    scope.activateChoice();
    assert.equal(scope.mode, "opticalEjectReview");
    assert.equal(scope.choice, 0);
    scope.activateChoice();
    assert.equal(calls.length, 1);
    scope.choice = 4;
    scope.activateChoice();
    scope.choice = 1;
    scope.activateChoice();
    assert.deepEqual(calls[1], ["eject", "selected", "/dev/sr0"]);
    assert.match(scope.message, /changed/i);
    scope.opticalSelectedDisc = {...disc, state: "unmounted"};
    scope.choice = 0;
    scope.activateChoice();
    assert.deepEqual(calls[2], ["mount", "selected", "/dev/sr0"]);
    assert.equal(scope.mode, "opticalChooser");
});

test("optical refresh resets selection and displays replacement identity before action", () => {
    let discs = [];
    const scope = vm.createContext({
        opticalDevice: "/dev/sr0", opticalObservationId: "old", choice: 5, message: "",
        opticalSelectedDisc: {state: "mounted", observationId: "old"},
        library: {refreshOpticalDiscs() { return discs; }},
    });
    method("GamesLibraryWidget.qml", "refreshOpticalDetails", scope);
    scope.refreshOpticalDetails();
    assert.equal(scope.choice, 0);
    assert.equal(scope.opticalSelectedDisc.state, "missing");
    discs = [{device:"/dev/sr0", observationId:"new", state:"mounted", directPlaySupported:true,
        volumeLabel:"New disc", filesystem:"udf", mountRoot:"/media/new", mediaId:"new-hash",
        mediaSetId:"new-set", discNumber:2, discCount:2}];
    scope.opticalObservationId = "old";
    scope.choice = 5;
    scope.refreshOpticalDetails();
    assert.equal(scope.choice, 0);
    assert.equal(scope.opticalObservationId, "new");
    assert.match(scope.message, /new-hash/);
    assert.match(scope.message, /Disc 2 of 2/);
    assert.match(scope.message, /Media changed/);
});

test("optical direct play registration clears an earlier directory-copy mode", () => {
    const disc = {observationId:"disc-id", device:"/dev/sr0", state:"mounted", directPlaySupported:true};
    const scope = vm.createContext({
        readOnly:false, mode:"opticalDetails", choice:0, message:"", installingDirectory:true,
        opticalSelectedDisc:disc, opticalObservationId:disc.observationId, opticalDevice:disc.device,
        runtimes:[{id:"native", name:"Native", file:true}], keyboard:{selectedIndex:-1,value:""},
        library:{revalidateOpticalDisc() {return {state:"ready",disc};}},
    });
    method("GamesLibraryWidget.qml", "activateChoice", scope);
    scope.activateChoice();
    assert.equal(scope.registeringOpticalGame,true);
    assert.equal(scope.installingDirectory,false);
    scope.activateChoice();
    assert.equal(scope.field,17);
});

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
        sessionTransition: "fade", transitionDuration: 250, sessionReturn: "origin",
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
        "inputLabels", "selectedInput", "serialPortName", "sessionReturn", "sessionTransition", "transitionDuration", "videoFormatKey",
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
            sessionTransition: "fade", transitionDuration: 250, sessionReturn: "origin"},
        captureController: {
            captureDeviceId: "card", videoFormatKey: "", audioDeviceId: "", serialPortName: "ttyUSB0",
            baudRate: 9600, commandTemplate: "{input}\\r", selectedInput: 2, sessionTransition: "fade", transitionDuration: 250, sessionReturn: "origin",
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

test("capture auto-suggest stages only unambiguous devices without saving", () => {
    const scope = vm.createContext({
        draft: {captureDeviceId: "old", videoFormatKey: "old-format", audioDeviceId: "", serialPortName: ""},
        draftDirty: false,
        captureController: {videoDevices: [{id: "camera"}], audioDevices: [{id: "a"}, {id: "b"}],
            serialPorts: [{id: "serial"}]},
    });
    for (const name of ["setDraftValue", "suggestDevices"]) method("CaptureSettingsView.qml", name, scope);
    scope.suggestDevices();
    assert.equal(scope.draft.captureDeviceId, "camera");
    assert.equal(scope.draft.videoFormatKey, "");
    assert.equal(scope.draft.audioDeviceId, "");
    assert.equal(scope.draft.serialPortName, "serial");
    assert.equal(scope.draftDirty, true);
});

test("capture session Back honors return policy and Home always returns Home", () => {
    const calls = [], inputManager = {back: 1, home: 2};
    const scope = vm.createContext({
        inputManager, captureController: {sessionReturn: "origin"},
        closeSession: home => calls.push(home),
    });
    method("CaptureSessionView.qml", "handleCommand", scope);
    scope.handleCommand(inputManager.back);
    scope.captureController.sessionReturn = "home";
    scope.handleCommand(inputManager.back);
    scope.captureController.sessionReturn = "origin";
    scope.handleCommand(inputManager.home);
    assert.deepEqual(calls, [false, true, true]);
});

test("capture session retries missing signal without sending another switch command", () => {
    const calls = [], inputManager = {back: 1, home: 2, menu: 3, confirm: 4};
    const scope = vm.createContext({
        inputManager, chooseInput: false, selectionPending: false, pendingSwitchConfirmation: true,
        captureController: {state: "no-signal", switchState: "sent",
            stopPreview: () => calls.push("stop"), startPreview: () => calls.push("start"),
            switchInput: () => calls.push("switch")},
    });
    method("CaptureSessionView.qml", "handleCommand", scope);
    scope.handleCommand(inputManager.confirm);
    assert.deepEqual(calls, ["stop", "start"]);
    assert.equal(scope.pendingSwitchConfirmation, true);
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
    scope.captureController.audioState = "error";
    scope.captureController.audioStatus = "Capture audio error";
    assert.equal(scope.videoReady(), true);
    for (const file of ["CaptureSessionView.qml", "CaptureSettingsView.qml"]) {
        const source = fs.readFileSync(path.join(__dirname, "../qml", file), "utf8");
        assert.match(source, /captureController\.audioStatus/);
        assert.match(source, /captureController\.audioState/);
    }
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
        composition: {enabled:false}, legacyHeaderControls: [], legacyMenuEnabled:false,
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

test("discovery uses provider identity and respects the local playback preference", () => {
    const calls=[];
    const title={kind:"onlineTitle",mediaType:"movie",providerId:42,title:"Same title"};
    const scope=vm.createContext({
        movieSearchClient:{busy:false,connected:true,pageStart:0,items:[{movieid:7,title:"Same title",uniqueid:{tmdb:"42"}}]},
        showSearchClient:null,playbackPreferences:{alwaysPlayLocally:true},selectedDiscoveryTitle:null,
        open:true,webSearchEnabled:true,pendingDiscoverySelection:null,
        results:[title],selectedIndex:0,query:"Same title",onlineDiscovery:{loadAvailability(...args){calls.push(["availability",...args]);}},
        closeRequested(){calls.push(["close"]);},mediaRequested(...args){calls.push(["play",...args]);},
    });
    method("CommandCenter.qml","localDiscoveryMatch",scope);
    method("CommandCenter.qml","resolveDiscoverySelection",scope);
    method("CommandCenter.qml","activateSelected",scope);
    scope.activateSelected();
    assert.deepEqual(calls,[["close"],["play","movie",7,"Same title","",0]]);
    calls.length=0;scope.playbackPreferences.alwaysPlayLocally=false;scope.activateSelected();
    assert.deepEqual(calls,[["availability","movie",42,"Same title"]]);
    calls.length=0;scope.playbackPreferences.alwaysPlayLocally=true;
    scope.movieSearchClient.items[0].uniqueid.tmdb="43";scope.activateSelected();
    assert.deepEqual(calls,[["availability","movie",42,"Same title"]]);
    scope.movieSearchClient.items=[{movieid:7,uniqueid:{tmdb:"42"}},{movieid:8,uniqueid:{tmdb:"42"}}];
    assert.equal(scope.localDiscoveryMatch(title),null);
    scope.movieSearchClient.items=[{movieid:7,uniqueid:{tmdb:"42"}}];scope.movieSearchClient.busy=true;
    assert.equal(scope.localDiscoveryMatch(title),null);
    calls.length=0;scope.activateSelected();assert.equal(calls.length,0);
    assert.equal(scope.pendingDiscoverySelection,title);
    scope.open=false;scope.movieSearchClient.busy=false;scope.resolveDiscoverySelection(title);
    assert.equal(calls.length,0);assert.equal(scope.pendingDiscoverySelection,null);
});

test("timed ambient covers internal overlays while leaving playback and external sessions alone", () => {
    const scope=vm.createContext({sessionManager:{active:false,currentAppType:""},captureSession:{open:false},kodiMedia:{playbackActive:false},desktopMode:{active:false,busy:false},appVpn:{busy:false},actionDispatcher:{guideTransitionPending:false}});
    method("Main.qml","ambientIdleAllowed",scope);
    assert.equal(scope.ambientIdleAllowed(),true);
    scope.sessionManager.active=true;
    for(const type of ["settings","store","customize"]){scope.sessionManager.currentAppType=type;assert.equal(scope.ambientIdleAllowed(),true);}
    for(const type of ["desktop","web","android"]){scope.sessionManager.currentAppType=type;assert.equal(scope.ambientIdleAllowed(),false);}
    scope.sessionManager.active=false;
    for(const [object,key] of [[scope.captureSession,"open"],[scope.kodiMedia,"playbackActive"],[scope.desktopMode,"active"],[scope.desktopMode,"busy"],[scope.appVpn,"busy"],[scope.actionDispatcher,"guideTransitionPending"]]){
        object[key]=true;assert.equal(scope.ambientIdleAllowed(),false);object[key]=false;
    }
});

function settingsNavigationScope() {
    const defaults = JSON.parse(fs.readFileSync(path.join(__dirname, '../config/studio-defaults.json'), 'utf8'));
    const scope = vm.createContext({
        selectedSection: 0, inPage: false, route: '', rowSelections: {}, pendingCategoryOrder: [], navigationError: '',
        sections: defaults.settingsNavigation.categories,
        inputManager: {navigateUp: 'up', navigateDown: 'down', navigateLeft: 'left', navigateRight: 'right', confirm: 'ok', back: 'back', menu: 'menu'},
        systemPage: {activePage: -1, captureReturnPage: -1}, profileManager: {editingAllowed: true},
        layoutModel: {composition: {}, errorString: 'Write refused', setComposition: () => true},
        storeRequested() { scope.storeRequests = (scope.storeRequests || 0) + 1; }
    });
    Object.defineProperties(scope, {
        category: {get: () => scope.sections[scope.selectedSection] || {}},
        categoryRows: {get: () => scope.category.rows || []},
        selectedRow: {get: () => scope.rowSelections[scope.category.id] || 0},
        arranging: {get: () => scope.pendingCategoryOrder.length > 0}
    });
    scope.root = scope;
    for (const name of ['selectRow', 'openRoute', 'openCategory', 'openSection', 'backToParent', 'handleCategoryArrangement', 'handleCommand'])
        method('SettingsView.qml', name, scope);
    return scope;
}

test('Settings preserves legacy shortcuts after category reordering and rejects unknown targets', () => {
    const s = settingsNavigationScope();
    s.sections.reverse();
    const routes = ['network', 'studio', 'sources', 'remotes', 'system', 'profiles', 'vpn', 'store-preferences', 'media'];
    routes.forEach((route, index) => { s.openSection(index); assert.equal(s.route, route); });
    const before = [s.selectedSection, s.inPage, s.route];
    for (const invalid of [-1, 9, 1.5, '1']) s.openSection(invalid);
    s.openCategory('network', 'capture');
    assert.deepEqual([s.selectedSection, s.inPage, s.route], before);
});

test('Settings App Store is a destination and Back restores the same Apps row', () => {
    const s = settingsNavigationScope();
    s.openCategory('apps', '');
    s.handleCommand('ok');
    assert.equal(s.storeRequests, 1);
    assert.equal(s.route, '');
    s.selectRow(1); s.handleCommand('ok');
    assert.equal(s.route, 'store-preferences');
    s.handleCommand('back');
    assert.equal(s.route, ''); assert.equal(s.selectedRow, 1); assert.equal(s.inPage, true);
    s.handleCommand('back'); assert.equal(s.inPage, false);
});

test('Settings Back lets an open Wi-Fi keyboard and HDMI draft handle cancellation first', () => {
    const s = settingsNavigationScope();
    const commands = [];
    s.networkPage = {keyboardOpen: true, handleCommand: c => commands.push('network:' + c)};
    s.openSection(0); s.handleCommand('back');
    assert.deepEqual(commands, ['network:back']); assert.equal(s.route, 'network');
    s.networkPage.keyboardOpen = false; s.handleCommand('back'); assert.equal(s.route, '');
    s.openCategory('inputs', 'capture');
    s.systemPage.handleCommand = () => {}; // dirty draft/keyboard still owns Back
    s.handleCommand('back'); assert.equal(s.route, 'capture');
    s.systemPage.handleCommand = () => { s.systemPage.activePage = -1; };
    s.handleCommand('back'); assert.equal(s.route, ''); assert.equal(s.category.id, 'inputs');
});

test('Settings category reorder cancels without writes and retains a failed save for retry', () => {
    const s = settingsNavigationScope();
    let writes = 0;
    s.layoutModel.setComposition = () => { writes++; return false; };
    s.handleCommand('menu'); s.handleCommand('down'); s.handleCommand('back');
    assert.equal(writes, 0); assert.equal(s.pendingCategoryOrder.length, 0);
    s.handleCommand('menu'); s.handleCommand('down'); s.handleCommand('ok');
    assert.equal(writes, 1); assert.equal(s.navigationError, 'Write refused');
    assert.equal(s.pendingCategoryOrder.length, s.sections.length);
    s.layoutModel.setComposition = () => { writes++; return true; };
    s.handleCommand('ok'); assert.equal(writes, 2); assert.equal(s.pendingCategoryOrder.length, 0);
    s.profileManager.editingAllowed = false; s.handleCommand('menu');
    assert.equal(s.pendingCategoryOrder.length, 0);
});

test('App Store return restores Apps with its original origin and cancels across profiles or Home', () => {
    const calls = [], pending = [];
    const s = vm.createContext({
        settingsStoreReturn: {profile:'a', origin:'page-button'}, profileManager:{activeId:'a'},
        sessionManager:{active:false}, allApps:{open:true}, settings:{openAppsSettings:() => calls.push('apps')},
        actionDispatcher:{dispatch:(action, origin) => {calls.push([action.type, action.section, origin]);return true;}},
        Qt:{callLater:fn => pending.push(fn)}
    });
    method('Main.qml', 'restoreSettingsFromStore', s);
    s.restoreSettingsFromStore(); pending.shift()();
    assert.equal(JSON.stringify(calls), JSON.stringify([['settings',-1,'page-button'],'apps']));
    assert.equal(s.allApps.open, false);
    s.settingsStoreReturn={profile:'a',origin:'old'}; s.restoreSettingsFromStore();
    s.profileManager.activeId='b'; pending.shift()(); assert.equal(calls.length,2);
    s.profileManager.activeId='a'; s.restoreSettingsFromStore(); s.settingsStoreReturn=null;
    pending.shift()(); assert.equal(calls.length,2);
});

test('Settings to Store hands off the session without leaving All Apps active', () => {
    const events = [];
    const s = vm.createContext({
        profileManager:{activeId:'a'}, settingsStoreReturn:null, allApps:{open:false}, store:{themeOrigin:'old'},
        sessionManager:{exitCurrent:() => { events.push('exit'); s.allApps.open=true; }},
        actionDispatcher:{currentOrigin:() => 'all-apps', dispatch:(action,origin) => { events.push([action.appId,origin,s.allApps.open]);return false;}},
        restoreSettingsFromStore:() => events.push('restore')
    });
    method('Main.qml','openStoreFromSettings',s);
    s.openStoreFromSettings();
    assert.equal(JSON.stringify(events),JSON.stringify(['exit',['app-store','all-apps',false],'restore']));
    assert.equal(s.settingsStoreReturn.profile,'a'); assert.equal(s.settingsStoreReturn.origin,'all-apps');
});
