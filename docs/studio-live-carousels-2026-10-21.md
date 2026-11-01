# Live pastel carousels — installed checkpoint

User-authorized T21.6/T21.7/T21.9 slice, 2026-10-21–22. The normal Studio Demo Home now displays Pastel sky with registry-backed apps and seeded Kodi viewing history. The complete interaction acceptance gate remains open: native Spotify launches, but its remote Home return has not passed. No broader task-completion credit is claimed.

## Implemented

- One app-bar root persists `appCollection: {mode: all|selected, ids: [...]}`. Actual desktop/web registry names/symbols are rendered; mock entries are excluded. All/Selected membership, up to 128 selected identities, visible capacity and scoped More apps share the same collection. Legacy placed app bars remain readable; explicit membership editing converts them atomically.
- Kodi `watched` reads completed and partial history, excludes never-watched items and sorts by lastplayed. `recent` remains dateadded. Shows + watched reads episode history. The bridge admits only canonical calendar dates for the narrow lastplayed/after filter; no write method/listener was added.
- `libraryChrome` exposes clean carousel vs browser controls through a shared inspector switch. Playback return refreshes profile-owned clients, restoring the media identity when history reorders.
- The v2 pastel preset contains ten launchers, one app collection and one watched shelf, with matching validated source bindings and managed wallpaper. It replaces authored app/history illustration buttons only on explicit application; no profile document is rewritten on startup.
- Pending appearance sliders, real Home-shaped Studio selection and genuine switches are included. The switch regression exposed a genuine ListView model-reset focus bug; the selected field is now restored after model replacement.

## Verification and installed evidence

`pastel-live-20261022-full-c` passes **18/18 CTest registrations** in 149.64 seconds. Archive SHA256: `b4eb2141a09b1e52107a1aba5efcb0419588f1ab267764c541ab4f1e3769b6ab`. Source digest: `40af4d675d7acbd271606dd9d09ce7dc53e51373102ce42b79087f89223e0bb5`. Evidence: `build/vm-oracle/pastel-live-20261022-full-c/report/`. All seven control/selection captures were opened and reviewed.

Both lounge/pastel save/reapply/reset journeys pass using that exact staged executable. All four captures were opened and reviewed under `build/vm-oracle/pastel-live-20261022-full-c/visual/`. Lounge retains the static reference regression. Pastel intentionally uses live collections: its disposable fixture profile has one real web entry and an honest missing-Kodi state, rather than illustrated history. This is not a claim that the old static pastel fixture has been rebuilt as a full editable live-data composition; T21.9 remains open. Both normal layout hashes stayed unchanged during the harness.

The runner deployed release **`a72852697c1f50e138161a63`**, retaining `6d558525d9ead04d09f31aa9` as previous. Studio Demo (`8bdf6077-4613-4b52-9b20-d1ec7931dfcf`) explicitly selected Pastel sky through Studio → More tools → Saved skins → Apply. Persisted layout hash: `38ca8d4fd778d33fdf9b192a9c918c22c623572c88a2e12cb561f83b43315409`. Household remained `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`.

`build/studio-demo/installed-kodi-verification-new.log` passes all six viewed movies, eight artwork responses, episode states and Household preservation against that exact release. The verifier's new explicit `--expected-release ID` supports intentional deployments without changing the original seed manifest; its original unchanged-release default remains strict. The first post-deploy probe correctly failed the old unchanged-release assertion before this option was added.

Reviewed installed captures in `build/studio-demo/`:

- `installed-pastel-home.png`: normal Home, three real registry entries (YouTube/Kodi/Spotify), live seeded movies.
- `installed-history-sixth.png`: D-pad reaches The Last Lighthouse, the sixth watched movie.
- `installed-playback.png` and `installed-playback-return.png`: actual synthetic OGV playback; Back returns to the same movie after it moves to the front. Lastplayed order now differs from dateadded without layout editing.
- `installed-spotify-launch.png`: actual Spotify desktop login interface, no sign-in performed. The initial Kodi launch opened its setup route; its existing seeded-source setup was then completed through the app store.

## Issues found during installed checks

1. Native Spotify's Home key did not return to Isolde. No passing native remote-return/focus claim is made. Closing/restarting during the test exposed a virtual-terminal capture trap, not a surviving Spotify process: `/proc` confirmed no Spotify process after the restart. The old frame persisted while tty2 was inactive. Separated Alt+F2 VM events restored the confirmed tty2 session (`Active=yes`). Record this as native-input acceptance pending, not successful remote app return.
2. The applied layout survives restart, but its shelf can keep a startup connection error even after the real Kodi bridge is available. `restarted-kodi-probe.log` proves six actual viewed movies after restart. The source now retries failed page clients once when the owning player transitions to connected, without reviving invalidated profiles. A regression injects an initial provider error and checks recovery plus teardown isolation. **This follow-up is awaiting verification/deployment.**

## Guest maintenance and machine boundary

The user authorized ordinary guest-only maintenance without repeated approvals; see `AGENTS.md` and `vm-oracle.md`. Host writes stay within the authorized workspace/requested outputs. No host packages, services, settings or unrelated files were changed.

Earlier cleanup logs: `approved-cleanup.log`, `approved-cleanup-five.log`. `standing-approved-cleanup-20261022.log` records four inspected obsolete staging payloads removed under locks, resolved-path and mount checks: `studio-entry-final-20261020`, `studio-reflow-release-20261021`, `studio-scrolling-delivery-20261021`, `studio-slider-release-20261020`. Installed releases, profiles, snapshots and reports were retained; free space then 2,256,568,320 bytes. No remaining approval loop applies to routine guest cleanup.

## Remaining gates

Verify/deploy the startup recovery follow-up; recapture normal Home after restart. Preserve the separate native Home-return defect as an open installed-acceptance item. Collection editing/More apps are covered by focused native/QML tests, but the complete independent uncoached installed journey, streaming history providers, Button carousel and broader fixture reconstruction remain open. T21 stays 36%; Overall round(849/24) = 35% across 24 active tasks. Exclude unrelated `docs/components.md`, `docs/design/page-studio/feature-map.md`, `src/home/PageData.h` and `evidence/` from commits.
