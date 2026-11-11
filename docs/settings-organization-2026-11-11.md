# Settings organization — T12 / T29

User direction: organize Settings to a professional smart-TV standard. App Store
may appear in both All Apps and Settings → Apps; its preferences must be clearly
distinguished from the destination. This user-directed UI slice does not resume
the unrelated backlog or lift the existing test-rerun deferral.

## Implemented hierarchy

| Category | Destinations |
|---|---|
| Network & Internet | Network connection; VPN |
| Picture & Sound | Display & accessibility; Sound |
| Home & Appearance | Edit Home & pages; Ambient mode |
| Apps | App Store; Store preferences |
| Inputs & Sources | HDMI inputs; Detected sources |
| Remotes & Controllers | Existing remote/controller controls |
| Profiles | Existing people and permissions controls |
| Media & Playback | Existing Kodi library, import and playback controls |
| System & Support | Setup guide; Diagnostics; About; explicitly unavailable Power |

The new menu is bounded declarative data in `studio-defaults.json`, validated
against native destination IDs. App Store remains the existing shared Store
destination and retains the All Apps link. Settings' former “Online Store” page
is now accurately titled “Store preferences.” Duplicate Media Library controls
are removed from the System menu; existing backend pages retain stable IDs.

## Interaction contract

- Up/Down selects a category; Confirm/Right enters it. Within grouped categories,
  Confirm opens a child, Back restores the exact row, and another Back restores
  category selection. Left from a category menu also returns to categories.
- Only the active pane has a focus outline. Category descriptions wrap instead
  of disappearing into ellipses; both category and child lists scroll.
- Child editors retain first refusal of Back for Wi-Fi keyboard/prompts, VPN
  edits, Studio drafts, profile text, source editing, audio/capture drafts,
  screensaver pickers and nested media controls.
- App Store opened from Apps returns to Apps on Back, retaining the original
  outside-Settings origin. Home cancels this return; queued returns cannot cross
  profile boundaries or replace another active session. Store app launch/return
  continues through the existing Store workflow.
- Existing saved section actions 0–8 are translated to semantic destinations;
  category reordering cannot reinterpret a saved VPN/Profiles/Media shortcut.
- Menu on the category list starts a reorder preview. Up/Down moves the selected
  category; Confirm atomically saves the complete permutation in the active
  profile's existing composition; Back cancels without a write. A write failure
  keeps the preview for retry. Restricted profiles cannot reorder. No categories
  can be dropped, duplicated or renamed into executable destinations.
- Missing/invalid menu defaults inherit the packaged navigation in memory.
  Reading older layouts does not migrate or rewrite them.

## Source-review findings addressed

1. “Online Store” named a catalog-preference page as though it were the store.
2. System mixed display/audio/ambient/media/input workflows with support tools.
3. Root category selection and child selection both looked actively focused.
4. Settings intercepted Back before the network on-screen keyboard could close.
5. Diagnostics → capture setup returned to the System list instead of Diagnostics.
6. A Settings → Store transition can reopen an All Apps origin during session
   teardown; the transition now closes that gallery so it cannot consume Store
   input. The return also preserves the outside-Settings origin.

These are source-reviewed corrections, not newly verified runtime bug closures.

## Verification and handoff

Round-two authorization supersedes the historical deferral below. Full VM
`round2-20261111-b` passes61/61, including prepared native/navigation checks;
both installed Lounge/Pastel journeys pass with48 reviewed screenshots and
unchanged normal layouts. Settings category and final System-row captures are
reviewed in both themes. Power is still unimplemented (BUG-078), and broader
system acceptance remains open. [Campaign evidence](test-campaign-2026-11-11.md).

Respect the standing user deferral: **no VM, native, CTest, remote journey or
visual rerun was performed**. No deployment or acceptance credit is claimed.
Prepared native regressions cover menu/schema bounds, malformed routes and
versions, complete permutations, persistence, profile separation, no write on
read and deterministic save rollback. Prepared navigation regressions cover
legacy shortcuts after reorder, nested Back, Store return/profile/Home guards,
reorder cancel/retry and restricted profiles.

The reference and shell-theme helpers are adapted to navigate the new categories
using packaged menu data; frozen older candidates still require their matching
frozen helpers. Future verification must review the actual Settings and Apps
screens at TV size, enlarged text and both dark/pastel palettes; execute the
prepared regressions, one task-closure full suite, and both reference journeys.

Static checks only: defaults JSON parsing; JavaScript test-file syntax; Python
helper compilation without execution; diff whitespace inspection; progress-table
reconciliation. Acceptance remains T12 0%, T29 13%, overall 1563/25 = 63%.

Round-two update supersedes the original test deferral: native61/61 passes,
both installed reference pairs and actual Audio/Ambient/sound routes are
reviewed. Combined three-palette inventory has90 reviewed frames but times
out before completion; source contrast, capture overflow and clipped VPN
header are recorded in docs/test-campaign-2026-11-11.md. Sources palette repair
is prepared, VM-unverified. Overall68%; no broad Settings gate is closed.
