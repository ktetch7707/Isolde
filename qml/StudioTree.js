.pragma library

// Input order is the document's reading order. Index children once so expanded
// groups do not rescan the complete 1024-component document for every parent.
function layerRows(items, expanded) {
    const children = new Map()
    for (const item of items) {
        const parent = item.parentId || ""
        if (!children.has(parent)) children.set(parent, [])
        children.get(parent).push(item)
    }
    const rows = []
    function append(parent, depth) {
        for (const item of children.get(parent) || []) {
            rows.push({item: item, depth: depth})
            if (item.componentType === "container" && Object.prototype.hasOwnProperty.call(expanded, item.id) && expanded[item.id]) append(item.id, depth + 1)
        }
    }
    append("", 0)
    return rows
}
