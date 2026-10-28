# T21.9 — registered Pastel buttons

The packaged opt-in Pastel layout and its recreation fixture now use ten registered editable Button components for shortcuts/inputs, alongside their existing single App carousel and single genuine Kodi Recently viewed component. Legacy launcher nodes are no longer used in this packaged design. Generic category/panel presentation remains shared; no fixture-specific renderer branch or flattened interface image is introduced.

This changes only `kind`, `widgetType`, `componentType` and the canonical derived `source` for those ten nodes. Stable IDs, actions, placements, categories, every presentation value, collection settings, pages and artwork are unchanged. Source data parity passes, and [the retained host result](quality/studio-pastel-buttons-host-20261027.json) records both file hashes and the field-equivalence check. `node build/studio-pointer-boundary-20261027/host-smoke.cjs` also passes packaged/fixture equality and production app/history structure checks.

The new native `studioPastelPresetUsesEditableButtons` case verifies complete fixture parity, registered button/source contracts, one app collection, one real history source and whole-document loading without a profile write. It is added to `studio_unit` but has not run on this host.

No existing user's saved document is rewritten or converted on read. The updated design is adopted by explicitly applying the packaged preset. Existing backup/rollback behavior remains in the skin operation.

VM testing is delegated by the user. Both generic recreation/reset pairs, actual button Content/Appearance/Actions editing, component replacement, collection overflow/editing, focus return and atomic rollback must still be run through the bounded runner and explicitly reviewed. This source/data revision does not supersede old captures with an invented visual pass. T20.5/T21.9 and task percentages remain open/unchanged.
