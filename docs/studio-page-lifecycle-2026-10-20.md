# T18 page lifecycle and selection — 2026-10-20

This record extends the earlier workspace checkpoint. It does not claim completion of the component catalog, nested/group editing, reusable styles, reference tools or editor cutover.

## Data and user operations

The original page now has optional root-level `rootPage` metadata: name, visibility and position among named pages. Legacy documents load without writing this metadata. Its empty stable page ID is retained for compatibility. A visible original page counts toward the 64-page profile limit. The designated Home remains protected; after another page becomes Home the original page can be renamed, reordered, duplicated or deleted through the same page context.

Page duplication allocates fresh local component/category/navigation identities, remaps local category and tab references and retains existing app/media/page destinations. Deleting a page refuses incoming links unless the user explicitly chooses an existing replacement. The confirmation names the removal and redirect. Removal plus repair is one completely validated draft operation and one undo record. Root-page links have a validated `root-page` dispatcher action; they are rejected when the original page no longer exists. Home/startup and page navigation resolve the applied designation.

Single-page presets preserve other pages and original-page metadata. With a named Home, the preset applies to that page, including after deleting the original page. Legacy preset contents are canonicalized before embedding so the destination remains renderable.

Studio uses independent, profile-local appearance overrides over packaged readable colors. Page color experiments cannot make its controls unreadable. Studio overrides must retain 4.5:1 text contrast on normal/selected surfaces and 3:1 focus contrast. The More menu provides Studio appearance and reset. Accessibility scale/font/motion values remain inherited from the profile.

Components accept validated boolean hidden/locked state. Hidden components remain in draft data and Layers; the selected hidden component is revealed for editing and omitted by Try interaction/runtime. Locking refuses movement and resizing, including collision proposals. Failed state/movement requests do not alter the draft. These are foundations for the later complete Layers/group workflow.

The shared PageNavigation component reserves identical left/top/no-navigation page viewports in Studio and runtime. Studio tools overlay that viewport. Legacy flow components register their actual rendered geometry for a named selection outline and pointer selection; selecting them does not dispatch their runtime action. Back retains the exact selected component. Page selection remembers each page's origin and falls back to the designated Home when history removes the editing page.

## Verification ledger

- Snapshot 24: archive `027b009f493257502acc1214bde2fd0b273a77fd2a7a4a9833ea65e8d76693b9`; all 14 CTest suites passed, 57.25 seconds. First page lifecycle and independent-chrome coverage.
- Snapshot 25: archive `b4952ce7a5907755da4f2006a90995210f48c2cac774328ea6f075a5a726f154`; all 14 suites passed, 57.01 seconds. Added hidden/locked recovery and extended native remote page journeys. This predates the shared navigation frame and legacy selection work.
- Snapshot 26: build failed because a new test called the private `documentData()` method. No installation occurred. Corrected the test to inspect public committed bytes.
- Snapshot 27: frozen but not built; superseded by the same test correction in snapshot 28.
- Snapshot 28: archive `d9663463c6c71cc4aee05fc85a1cd442a59d22c1a9886427a4a0e30dc8f6b60c`; compiled successfully, but the new legacy pointer-selection assertion failed (the context window did not open). Other native navigation cases, including page viewport parity, passed. No deployment occurred.
- Snapshot 29: archive `56f61366fdc99f391b2e29e0c2346d665a69e3fe2468e8ece67ccba4c4ea83e4`; failed the legacy pointer-selection assertion and exposed a dangling QJsonValueRef in the new preset test. No deployment occurred. Adds explicit visible ancestry and decorative-card edit selection, closes the page rail before the pointer test, double-stroke selection contrast and a duplicate-identity boundary correction. Source/build/logs: `/home/isolde-dev/isolde-runs/studio-implementation-29/`; host manifests: `build/studio-implementation-29/`.
- Snapshot 30: archive `c2602bd4b2e18f961b46dcb9032e6734cc2d9b5c2114766bd06064bb8de0cea5`; both previously failing regressions pass (page lifecycle/preset test: 292 ms; actual legacy pointer selection/no action dispatch/outline parity: 1.465 seconds). The outline now subscribes to ancestor geometry changes. The preset test retains a QJsonValue instead of a reference into a temporary object. Full application build and all 14 CTest suites passed in 57.62 seconds. Installation and all three graphical journeys passed; see the installed evidence below. Source/build/logs: `/home/isolde-dev/isolde-runs/studio-implementation-30/`.

No profile/Kodi/media cleanup, offline cold-boot qualification, physical HDMI validation or genuine uncoached user acceptance is implied by these automated tests. The developer entry remains until the later cutover gate.

## Installed evidence and restoration

Installed release `d29207a40cc6296737bc757f`, previous `d5c568eb056c87d2dc039260`. The installer independently rebuilt/retested, selected the complete payload, and the existing service was restarted explicitly without changing startup configuration. `/proc/<MainPID>/exe` resolves to the new release. Source archive identity is snapshot 30 above; these documentation edits were made after its freeze.

| Journey | Guest evidence | Durable host directory |
| --- | --- | --- |
| Studio direct edit, history, optional Try interaction, Apply/reopen | `/tmp/isolde-components-acceptance-fv1_g0gz` | `build/studio-installed-30/studio/` |
| Dark recreation, saved skin, reset | `/tmp/isolde-reference-config-1g6u9fkz` | `build/studio-installed-30/lounge/` |
| Pastel recreation, saved skin, reset | `/tmp/isolde-reference-config-h6gprdyl` | `build/studio-installed-30/pastel/` |

Exact guest configs/results/logs were copied alongside host captures. All seven captures were inspected: the Studio viewport remains full size with overlay tools and a visible named selection; its applied page menu is readable; both reference arrangements retain their geometry/artwork/labels/focus and reset to packaged defaults. The Studio two-item fixture demonstrates interaction, not the finished catalog or Studio-authored reference reconstruction.

After the final run the normal Household profile was restored and captured in `build/studio-installed-30/normal-home.png`: service active/running, NRestarts=0, current/previous links and actual executable verified in `restoration.log`. Existing intrusive Detected Sources rows remain visibly present (T25); they were not hidden by modifying the normal profile or deleting media/test folders. Full-product offline Generator and genuine user/cutover acceptance remain open later gates.