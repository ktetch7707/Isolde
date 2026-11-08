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
