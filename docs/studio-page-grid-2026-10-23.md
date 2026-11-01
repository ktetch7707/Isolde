# T20 page-layout controls — 2026-10-23

Page Layout now provides layout choices, a diagram using the actual renderer's metrics, column/row steppers, gap/margin sliders and section-height control. Optional exact values use the same validated proposal. Controls sit opposite the selection and can be hidden; overflowing controls scroll and remote focus reveals its field.

An existing placed layout retains every component, group and lock when its grid changes. Legacy layouts and explicit layout-mode changes use the existing conversion proposal. Opening controls changes nothing; candidate edits stay private until confirmed, Cancel restores the origin, and acceptance creates one undoable transaction without saving to disk.

Section previews wait for real surface geometry before deciding whether cells fit. Replaced section delegates cannot remove the replacement's geometry registration. Invalid dimensions and physically impossible spacing refuse with the original draft intact.

## Verification

- `t20-page-grid-20261023-a` exposed transient section geometry being mistaken for an invalid layout. Fixed renderer readiness and delegate identity rather than weakening validation.
- `t20-page-grid-20261023-b`: four page-grid scenarios pass in 27.25 seconds.
- `t20-page-grid-20261023-c`: six focused Studio registrations pass in 97.28 seconds; page-grid registration passes in 26.16 seconds. Source digest `54475801d29f0433c1102b92f387e3a5d7c5c76838306d347590b041c4416df6`. Reports: `build/vm-oracle/t20-page-grid-20261023-c/report/`.
- Opened both page-grid captures from run c. Controls are readable at 1280×720 and the selected fixture item remains visible. Section controls scroll to the optional exact-values action. These sparse synthetic scenes prove control visibility, not the appearance of a populated Home.
- `t20-page-grid-20261023-full-d`: all **20 CTest registrations pass in 191.30 seconds**. Source digest matches run c above; source archive SHA256 `ad604964418e9e51cd48aa651ff4d993443321902c32acf7d97aa72b1fb78578`. All eleven offscreen screenshots are byte-identical to run c's opened/reviewed images. Full graphical journeys are checked separately below.

## Visual acceptance remains open

Reopened the populated installed-checkpoint captures `build/studio-four-fixes/runtime-movies-focus2.png` and `studio-live-carousels.png`. They show both collections, but three apps with fallback initials and unusually shallow history art are not evidence of matching the supplied reference. These older captures do not prove current-source visual behavior.

The isolated Pastel journey has unavailable history and insufficient app data. Its save/reset pass cannot count as complete reference recreation. Production collection semantics and deterministic fixture providers remain required under T21.9. No task checkbox, deployment or visual acceptance is claimed by a focused test pass. T20 stays 80%; Overall stays round(866/24) = 36% across 24 active tasks.

## Full-build graphical review

The exact full-d staged executable passes all three functional journeys. Opened all **23** graphical captures under `build/vm-oracle/t20-page-grid-20261023-full-d/visual/`:

- Studio attempt `20261023-020814-802`, 15 images, guest evidence `/tmp/isolde-components-acceptance-1p0zokjo`: movement/reflow, resizing, nested eight-input arrangement, Inspector, Apply and scrolling work. These are sparse diagnostic scenes inside a desktop window; they do not establish TV polish. Existing Inspector prose, mixed tools and generic catalog still need their owning gates.
- Lounge attempt `20261023-021004-817`, four images, `/tmp/isolde-reference-config-jd2nuctg`: landscape adaptation retains navigation, banner, three independent art/title/progress cards, four sources and guide. Reviewed against the unchanged reference; composition and reset pass. Cropped fixture artwork remains illustrative.
- Pastel attempt `20261023-021058-644`, four images, `/tmp/isolde-reference-config-hiaah6x0`: wallpaper, launcher rows and panels render, but one initial-based YouTube entry and unavailable history fail the full reference composition. Save/reapply/reset passes; visual recreation remains incomplete. Default reset removes the wallpaper and preserves the added pages.

Both normal layout hashes are unchanged: Household `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Studio Demo `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`. Service is restored active/running, PID 1873252, zero automatic restarts. Installed release remains `fc815f2341cb375a0b809118`; no deployment or host maintenance occurred.

Remaining T20.5 work includes explicit named cross-container Move targets and the complete input/policy matrix, followed by installed desktop/TV acceptance. The Pastel visual gap remains a standing gate; a functional runner pass does not close it.
