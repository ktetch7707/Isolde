# T18 page-first workspace checkpoint — 2026-10-20

Native application code, with the workspace task still in progress. Source snapshot `build/studio-implementation-23`, base `47f0d5c`, archive SHA-256 `d1db5303c2f3e70d2b46ac9231a00be5498bcdec09adb83064d32d9098f30cba`. Guest source/build/evidence: `/home/isolde-dev/isolde-runs/studio-implementation-23/`.

## Implemented

- Collapsible left page menu over the full-sized authored screen: Add page, created pages, Add component, Page appearance, More tools, Review & Apply and Edit page. No permanent top toolbar or required Preview step. Grid guides appear during manipulation; ordinary editing keeps the actual component appearance visible.
- Confirm selects a component and opens Move / Width / Height / Edit component. Move is deliberate; pointer click selects and dragging begins only after the platform drag threshold. Width/Height are independent remote steppers. Existing collision proposals and typed transactions remain the execution path.
- Context windows name the item/page, choose an available side of the selection and restore the edit origin on Back. More contains history, layers, grid/scopes and optional Try interaction. This is not the completed nested/layer/inspector UX.
- New blank pages receive the validated packaged grid immediately. Page contexts support edit, rename, Set as Home and guarded deletion. Page colors update the rendered draft; these are initial numeric/color-entry controls, not the final visual palette/artwork editor.
- Validated profile-local `homePageId`: legacy documents default to the original Home without a read-time write. Designation changes are one undoable draft operation and survive atomic Apply/restart. The designated page cannot be deleted. Incoming links currently refuse deletion with an explanation; they are not silently broken.
- Settings receives the applied designated Home palette; draft colors do not publish early. Main startup and the Home command resolve the designation. Complete navigation and shell theme adoption remain later gates.
- Draft artwork and wallpaper resolve against the active profile's managed asset path. Invalidation clears the path/composition and drops the old clients' component data. Refused edits remain visible in the page menu.

## Verification

Full CTest on frozen snapshot 23: **14/14 passed**, 39.40 seconds. New tests cover remote page creation/designation/protection, atomic designation undo/persistence, button-link deletion refusal and repair, and artwork URL parity/teardown. Updated remote/pointer/inspector replay remains byte-identical with no applied writes. Existing atomic failure/profile suites remain passing.

Staged snapshot 22 also passed 14/14 (54.15 seconds) and the actual graphical page-menu/direct-edit/Add/history/Try interaction/Apply/reopen journey: `/tmp/isolde-components-acceptance-fnr2re5z`, screenshots `build/studio-staged-22/`. These are explicitly staged evidence, not proof of the installed snapshot 23.

Installed release **`d5c568eb056c87d2dc039260`**, previous `d30eff6291f378d0fd37b276`. The installer independently rebuilt/retested before selecting the release; normal user service restarted explicitly. Exact installed executable confirmed through `/proc/<MainPID>/exe`.

Sequential installed graphical evidence, with disposable configurations and durable host copies under `build/studio-installed-23/`:

| Journey | Guest result/config/log directory | Captures |
| --- | --- | --- |
| Direct edit, menu, Add, history, Try interaction, Apply, reopen | `/tmp/isolde-components-acceptance-292d6iun` | `studio/studio-edit.png`, `studio/studio-studio.png`, `studio/studio-applied.png` |
| Dark recreation, saved-skin round trip, reset | `/tmp/isolde-reference-config-b0jvzp34` | `lounge/lounge-custom.png`, `lounge/lounge-default.png` |
| Pastel recreation, saved-skin round trip, reset | `/tmp/isolde-reference-config-rfga7__4` | `pastel/pastel-custom.png`, `pastel/pastel-default.png` |

All journeys passed. Captures explicitly inspected: full-size direct editing with a selection outline and short operation hint; overlaid left menu with no toolbar; both generic reference arrangements preserved and reset to packaged defaults. The two-item Studio fixture is a test arrangement, not a demonstration of the finished catalog or reference reconstruction through Studio. Uncoached user review remains pending.

After all runs: normal Household Home captured/inspected, service active/running, zero restarts, installed executable unchanged. Its previously observed detected-source rows remain intrusive (T25); they were not removed or special-cased. No cleanup of profiles/Kodi/media or offline/hardware qualification is claimed.

## Remaining T18 and later requirements

T18 remains open: original legacy-root page identity/name/deletion must become fully ordinary when another page becomes Home; page duplication/reorder and incoming-link remap need complete remote controls and atomic review. Nested breadcrumbs, focus/empty/hidden/source-error recovery, large-text layout and independently readable configurable Studio chrome need full acceptance. No T18 completion is claimed from one menu capture.

Later tasks still own the complete six-family component catalog, Section/Group authoring, rich content/action/image controls, style/typography/state presets, alignment/distribution, reusable templates/styles, reference-image tools, import/export, full preflight/restore and default-editor cutover. Current Add still exposes Clock/Text only. Button/image links and the reference-reconstruction promise are not claimed implemented by this checkpoint. Native Kodi ownership, offline Generator, hardware and genuine user usability acceptance remain standing gates.
