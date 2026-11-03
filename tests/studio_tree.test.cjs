const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const test = require("node:test");

const source = fs.readFileSync(path.join(__dirname, "../qml/StudioTree.js"), "utf8");
const tree = vm.createContext({});
vm.runInContext(source.replace(/^\.pragma library\s*/, ""), tree);
const outline = rows => Array.from(rows, row => [row.item.id, row.depth]);

test("reading order and component identities survive nested expansion", () => {
    const nodes = [
        {id: "first"},
        {id: "child", parentId: "group", hidden: true, locked: true},
        {id: "group", componentType: "container"},
        {id: "nested", parentId: "group", componentType: "container"},
        {id: "grandchild", parentId: "nested"},
        {id: "last"},
    ];
    const rows = tree.layerRows(nodes, {group: true, nested: true});
    assert.deepEqual(outline(rows), [
        ["first", 0], ["group", 0], ["child", 1],
        ["nested", 1], ["grandchild", 2], ["last", 0],
    ]);
    assert.equal(rows[2].item, nodes[1]);
    assert.equal(rows[2].item.hidden, true);
    assert.equal(rows[2].item.locked, true);
});

test("collapsed groups retain their named row and omit descendants", () => {
    const nodes = [
        {id: "group", componentType: "container"},
        {id: "child", parentId: "group"},
        {id: "last"},
    ];
    assert.deepEqual(outline(tree.layerRows(nodes, {})), [["group", 0], ["last", 0]]);
    assert.deepEqual(outline(tree.layerRows([], {})), []);
});

test("1024-node expanded document indexes each parent exactly once", () => {
    let parentReads = 0;
    const nodes = [], expanded = {};
    for (let index = 0; index < 1024; ++index) {
        const group = index % 4 === 0;
        const parent = group ? "" : "node-" + (index - index % 4);
        const node = {id: "node-" + index, componentType: group ? "container" : "text"};
        Object.defineProperty(node, "parentId", {get() { ++parentReads; return parent; }});
        nodes.push(node);
        if (group) expanded[node.id] = true;
    }
    const rows = tree.layerRows(nodes, expanded);
    assert.equal(rows.length, 1024);
    assert.equal(parentReads, 1024);
    assert.deepEqual(outline(rows).slice(-4), [
        ["node-1020", 0], ["node-1021", 1], ["node-1022", 1], ["node-1023", 1],
    ]);
});

test("prototype-like identities do not collide in the child index", () => {
    const nodes = [
        {id: "__proto__", componentType: "container"},
        {id: "constructor", parentId: "__proto__"},
    ];
    const expanded = Object.create(null);
    expanded.__proto__ = true;
    assert.deepEqual(outline(tree.layerRows(nodes, expanded)), [["__proto__", 0], ["constructor", 1]]);
    assert.deepEqual(outline(tree.layerRows(nodes, {})), [["__proto__", 0]]);
});
