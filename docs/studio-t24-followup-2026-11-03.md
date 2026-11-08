# T24 navigation, large layouts and literal labels

This continues the user-authorized coding-only T24 exception. VM verification
remains deferred. It corrects concrete software gaps found after the initial
cutover commit; it is not a claim that T24's installed acceptance passed.

## Behavior changes

- Page navigation saves the complete transient origin: stable component, page and
  nested-group viewport, authored tab selection, recovery control and app/button
  collection state. Opening another page clears that page's transient state;
  Back restores its own snapshot. Removed or subsequently restricted history
  entries are skipped so Back can reach an existing origin or Home recovery.
- Deferred collection engagement and scroll reveals carry a navigation generation.
  An earlier page cannot restore focus or scroll into a later page with the same
  component ID. Viewport restoration runs after queued focus reveals. An origin
  removed/hidden by Apply uses reachable fallback focus rather than restoring an
  unrelated old scroll position.
- Inline Studio uses the same complete snapshot on exit. A late page request is
  refused while Studio is active, preserving its draft. Runtime history is bounded
  to 64 entries; further forward navigation is explicitly refused, with Back/Home
  recovery, instead of silently discarding an origin or growing indefinitely.
- Error overlays receive input before the editor underneath. A generic error has
  no hidden Right-key action to launch the Store. Existing named-app retry/Store
  behavior remains available for actual app launch errors.
- The Layers tree indexes children once and then traverses expanded groups.
  Reading order, component identities, depth, lock/hidden visibility and collapse
  semantics remain unchanged. A 1,024-node document no longer rescans the complete
  document for every expanded parent. Prototype-like IDs cannot expand a group
  through inherited object properties.
- Studio dialog choices wrap full labels and size their rows to contain the text.
  The existing virtualized ListView remains bounded by the available viewport.
  Component/page names and user-derived status labels render as plain text rather
  than interpreting markup. Shared palette, typography and spacing remain the
  presentation authorities.

No document schema, stored-layout migration, media database, dependency, service or
provider was added. Navigation snapshots are transient and never enter Apply or
portable exports. The existing native permission predicate is respected where the
current backend exposes it; older backends without that API retain their existing
page-access behavior. T22/T23 work already present in the shared checkout was
preserved rather than included wholesale in this patch.

## Focused verification

`node --test tests/studio_tree.test.cjs tests/qml_navigation.test.cjs` passes
**11/11**. These tests execute the production JavaScript helper and production QML
JavaScript method bodies with only their rendering/event/native boundary supplied
by test adapters. They verify reading order, collapse, 1,024-node linear indexing,
prototype-like IDs, removed/restricted history, recovery, generation cancellation,
current-page collection restoration, draft/history refusal and error action policy.
They do not simulate a passing installed Studio journey or claim Qt rendering.
Log: `build/t24-followup/node-tests.log`.

The `studio_cutover` QML suite now additionally exercises long scrolling-page
Back/collection restoration, removed-page/stale-callback recovery, long literal
labels and selection of component 1,024 through virtualized Layers. Its scale
fixture uses the real schema/session/renderer and remains bounded to 1,024 nodes;
the earlier reset scenarios retain their existing defaults. These QML checks are
**prepared but not run** on this host.

Qt/CMake/CTest, qmlformat and clang-format remain unavailable. No host toolchain
was installed and no VM action was launched. `git diff --check` and the progress
calculator remain the available static checks. Local JavaScript tests do not
award installed or performance acceptance credit. T24 stays at 0%; Overall stays
42% (sum 1147, 27 active tasks).

## Remaining verification

Run the focused QML cutover suite on the complete integrated source before the
task-closure suite and installed acceptance. Inspect long labels at the declared
viewports, actual scroll/focus after app/media/page returns, full-history refusal,
late routes during dirty editing and error-overlay recovery. Measure the declared
input/frame/lifecycle budgets with actual VM observations; the linear-index test
does not prove the 100 ms/30 fps targets. Complete the original source-tied 34-task
report, both reference/reset reviews and T15.4/T24 walkthrough. Reconcile concurrent
T22/T23 source and evidence before freezing that candidate. Any failures remain
software work; this record does not assert that no further code changes are needed.
