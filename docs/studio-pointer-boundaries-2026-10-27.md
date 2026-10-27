# T20.5 — refused pointer targets and remote recovery

Implementation batch, 2026-10-27. The user explicitly assigns VM testing to another agent; this batch starts no VM operation and does not close a visual, native or usability acceptance gate.

## Changes

Pointer movement previously discarded an out-of-bounds target. Releasing then accepted the last valid candidate, despite showing a boundary error. Pointer resizing had the same problem, including a drag below the minimum size.

Movement also compared screen-space pointer distances with unscaled grid cells. The conversion now accounts for the canvas scale, so zoom changes the view without changing the cell reached by an equivalent drag. The ordinary threshold/cancel/drop/undo/remote-parity journey now runs at full size and zoomed out. Boundary tests use a zoomed-out canvas to keep refused targets inside the test window while crossing the actual page boundary.

Movement now retains the actual refused candidate and lets the existing `StudioSession::placementProposal` boundary refuse its acceptance. Positive resize rectangles use that same path. Nonpositive resize rectangles retain an explicit refusal, including through release and Confirm, because they have no renderable rectangle. A deliberate valid adjustment clears the refusal. Beginning, canceling, closing or reopening editing clears transient refusal state.

Remote input can move a refused pointer candidate several cells back into bounds. Each step must reduce its boundary violation until it is valid; it cannot silently snap to a different location. Ordinary remote navigation at a valid grid edge still retains the previous valid candidate. Preview document extent remains bounded by the configured scrolling limit even when a refused pointer target is outside it. All accepted changes still use the existing atomic placement command, with no persistence during preview.

`tests/studio/tst_gestures.qml` adds actual-input regressions for a valid target followed by an out-of-bounds release, multi-cell remote recovery, Confirm refusal, oversized and nonpositive resizing, cancel, retry, one accepted history entry, undo and unchanged applied bytes.

The prerequisite collection review also found that both `StudioCatalog` and `StudioInspector` excluded Android apps although `PageAppCarousel` includes them. Both pickers now admit the same desktop/web/Android types as the production collection. `studio_selection` adds a remote-input case for Android selection in Add and Edit, preserving section filtering and explicit membership across registry removal, excluding mock/Store entries, and leaving applied layout bytes unchanged. This is picker behavior using deterministic fixture actions, not Android installation, entitlement, history or playback evidence.

## Local verification and testing handoff

`build/studio-pointer-boundary-20261027/host-smoke.json` records passing host JavaScript smoke checks against the actual QML methods and drag expressions (scales 0.4, 0.5, 1 and 1.5), plus packaged Pastel/fixture document equality, nine-slot app capacity and the production Kodi-history source. The source digest is recorded in the report. These checks stub the native boundary and do **not** execute Qt, native C++, graphical rendering or the QML input suite. `git diff --check` passes for the edited files. The host has no discovered CMake/Qt test toolchain; no host dependency or setting is changed.

The testing agent should run the existing bounded runner's full Test on the completed source batch, then Visual for `studio`, `pastel` and `lounge` against that same passing source. In particular, review `studio_gestures` (the three new boundary cases and zoom parity), `studio_selection` (Android picker input), `studio_policies`, `studio_destinations`, ordered movement, reduced-motion feedback, both reference recreation/reset pairs, and exact focus/viewport restoration. Preserve any other agent's healthy guardian; this implementation starts no competing build.

## Remaining gates

The Pastel preset already contains one real registry-backed app collection and one Kodi viewing-history component, rather than individually placed imitation app/history cards. Its saved structure matches the recreation fixture. This batch does not add fabricated provider content or fetch replacement artwork.

The existing artwork/design/provider limitations in `studio-pastel-overflow-2026-10-26.md` and pending real-artwork journey in `kodi-movie-artwork-2026-10-26.md` remain open. Real app-provider viewing-history acceptance requires a permitted working adapter; installed-app status alone cannot supply it. The remaining T21 inspector/header/tool organization, T22 templates, T23 apply/restore and T24 uncoached cutover gates are not completed by these boundary fixes. No checkbox or percentage credit: T20 80%, T21 36%, Overall 32% across 27 active tasks.
