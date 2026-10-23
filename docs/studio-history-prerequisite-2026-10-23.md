# T21.9 — genuine collection prerequisite

The user authorized the T21.9 prerequisite on 2026-10-23 while T20 stays open. This is a sequencing exception, not a waived visual or safety gate.

## Implementation

- Register Recently viewed in the normal Studio catalog as a distinct library family. Its canonical source is `recently-viewed`, with local Kodi `history/watched` semantics. Generic Library controls cannot silently turn it into Recently added.
- Read movies and episodes through the existing profile-scoped Kodi bridge, at most 50 per request. Merge by valid Kodi `lastplayed`, then stable media kind/identity, and retain at most 50 results. Missing/invalid dates are excluded. A failed second request does not publish partial history. Kodi remains the owner of metadata, watched/resume state and playback.
- Expose separate remote slider controls for Result count and Visible cards. Reuse private preview/cancel, validated transactions and Undo. Repair selected index when the displayed result count shrinks. Multiple instances reuse independent PageData clients; teardown invalidates them.
- Update the opt-in packaged Pastel history source. Existing stored layouts are never rewritten on read. Rebuild the Pastel fixture from the same packaged layout, with one app collection and one Recently viewed component rather than separately positioned illustrated items.
- Acceptance-only deterministic providers restore real packaged catalog manifests into a disposable XDG tree and serve local movie/episode/artwork bridge responses. Original geometric test artwork is generated separately from the reference. No application fixture switches, special-case renderer branches, Kodi database writes, external requests or real-playback claims are introduced.

## Verification

Focused VM runs:

- `t21-history-20261023-a`: `kodi_media_client` passes bounded mixed-history ordering, missing dates, limits, empty state and invalidation.
- `t21-history-20261023-b`: fails because Studio defaults required exactly five presets/six catalog entries. Replaced exact counts with bounded registered-entry validation; `-c` passes `component_contracts`.
- `-d`: remote test used obsolete toolbar selection; corrected it to use the page menu. `-e`: test omitted the existing Show controls step; corrected the actual remote journey. `-f` passes `studio_selection`; all four captured control screenshots reviewed. These sparse controls do not prove populated Home acceptance.
- `t21-history-20261023-full-a`: stops at oracle self-test because importing the acceptance script as a module could not resolve the sibling fixture module. Moved that import into the executable journey. No application build/test pass is claimed for this run.
- `t21-history-20261023-full-b`: 23/25 registrations pass; navigation exposes a loading-focus regression and obsolete individually-authored fixture/catalog assumptions. Fixed focus repair to wait for nonempty loaded data, supplied normal grid/placement inputs, and updated collection/tab/container expectations. Focused `-g` retains two fixture-test failures; `-h` passes both navigation registrations in 56.53 seconds.
- `t21-history-20261023-full-c`: 24/25 pass; the four-scenario `studio_grid` suite times out at 30 seconds (previous full-b took 28.66 seconds). Split its existing scenarios between `studio_grid` and `studio_grid_controls`, keeping every assertion and the 30-second bound. Focused `-i` passes both registrations.
- **`t21-history-20261023-full-d`: 26/26 pass in 127.74 seconds**, source digest `1626462b7e9fe6d4da8c39368ff1487a82fa8d6cfb0ed344793a7bc2c8a04e37`. The exact staged payload passes Studio and both Settings-selected reference save/reapply/reset journeys. All 18 native control captures and all 23 graphical captures reviewed. Earlier b/c/g/h/i captures are byte-identical to the reviewed full-d captures.

Populated Pastel evidence: `build/vm-oracle/t21-history-20261023-full-d/visual/pastel/attempt-20261023-164617-560/pastel-custom.png`; saved structure and request evidence: `report/evidence-pastel-1792799228584192448/isolde-reference-config-dzxrc__g/` (guest `/tmp/isolde-reference-config-dzxrc__g`). This proves a real mixed-history renderer reading deterministic local providers. Five registered launchable apps appear; the other restored catalog entries are mock apps and correctly excluded, so graphical app overflow is **not** proven. The existing QML membership/gallery test covers bounded overflow separately.

Visual verdict: populated composition now works, but app artwork remains letter placeholders and recent art is too shallow relative to the reference. No professional visual acceptance or T21.9 completion is claimed. The user subsequently requested six concrete repairs: supplied app icons, real Games/Music pages, a Guide destination instead of Settings, a top-right search Web switch, and real Desktop handoff instead of a mock session. These are authorized next implementation work, preserving the prerequisite and standing gates.

Evidence is under `build/vm-oracle/<run-id>/report/` and the corresponding guest oracle run. Installed release remains `d87a5f1475b036da6b5e55a8`; normal layout hashes are unchanged (Household `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`, Studio Demo `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`). Normal service restored active/running, zero restarts, PID2053527. No deployment in this slice.

## Open limits

No T21.6, T21.7 or T21.9 checkbox is closed yet. Streaming/app-history adapters, unsupported-provider UX, installed acceptance and professional populated visual acceptance remain separate gates. Fixture history proves the production read/render path against deterministic inputs; it does not establish real Kodi playback or third-party history permissions. T20 remains 80%, T21 36%, Overall round(866/24) = 36% across 24 active tasks.
