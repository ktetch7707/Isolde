# T17 geometry checkpoint — 2026-10-20

Implemented native code, not a design-only update. Source snapshot: `build/studio-implementation-18`, base `82249d1`, archive SHA-256 `aae05fd1d9dcc9877878543074309a32e4b1717558fe3068265c8dcdc17b6d69`. Guest source/build/logs: `/home/isolde-dev/isolde-runs/studio-implementation-18/`. Installed release: `d30eff6291f378d0fd37b276` (previous `47330f1ab7ab5a2f1bd5a517`).

## Changes and evidence

- Shared `PlacedComponents.qml` renders measured rectangles for whole-screen and per-section grids. Runtime, editor overlays, hit testing and remote placement use the same section metrics. Empty explicit grids remain editable; invalid physical cell geometry surfaces an error rather than rendering negative sizes.
- Explicit screen/scrolling-section modes, bounded section grid overrides and packaged editor grid defaults. Section coordinates are independent; collision/overflow checks validate the complete proposed layout. Conversion is explicit and undoable; library/section-action layouts cannot be flattened silently. Existing legacy flow pages remain unchanged on read/cancel.
- Studio camera now fills the actual viewport; tool visibility no longer scales or reflows the page. The old toolbar remains provisional and can obscure content; T18 owns its replacement with the contextual page-first shell. This checkpoint is not the accepted final editor.
- Addition resolves registered component defaults and collisions as one draft transaction. Corrected canonical component normalization and a mutating optional-field lookup found by regression tests. Page mutation rollback now snapshots before changing the document.
- New section-isolation/atomic-add/undo/persistence tests and actual QQuickWindow section geometry tests at 1280x720, 1920x1080, 3840x2160 and 1280x800. Existing remote/pointer/inspector parity and deterministic write-failure suites remain passing.

Full CTest: **14/14 passed**, 56.63 seconds; `build/studio-installed-18/ctest.log`. Installer independently rebuilt/retested before selecting the release; retained `install.log`. Failed snapshots 15–17 remain retained with their diagnostics; they are not acceptance evidence.

Installed sequential graphical runs, all using disposable configurations:

| Journey | Guest evidence | Host captures/results |
| --- | --- | --- |
| Dark recreation + reset | `/tmp/isolde-reference-config-0o8netsv` | `build/studio-installed-18/lounge/` plus copied guest directory |
| Pastel recreation + reset | `/tmp/isolde-reference-config-p7l8bker` | `build/studio-installed-18/pastel/` plus copied guest directory |
| Studio draft/history/Apply/reopen | `/tmp/isolde-components-acceptance-0gxx5svk` | `build/studio-installed-18/studio/` plus copied guest directory |

Visual inspection: dark hero/media/source/guide arrangement and pastel independent launcher circles/labels/frosted panels/recent row remain intact through the generic renderer; both reset captures show packaged default Home. Studio capture confirms full-sized canvas and also confirms the old overlay toolbar is not final UX. Fixture media/progress are synthetic, as documented by their READMEs; these runs do not establish real playback, unavailable services, GPU blur qualification or user usability acceptance.

Service restoration checked after graphical acceptance: active/running, zero restarts; no profile cleanup or Kodi mutation. Current normal-Home capture and copied result JSON are retained with the host evidence. Normal Household still exhibits the previously observed intrusive detected-source rows (T25), unlike the isolated fixtures; this is not claimed fixed. Offline cold-boot Generator, hardware, full nested editor, new catalog, Home designation and finished Studio usability remain their owning backlog gates.
