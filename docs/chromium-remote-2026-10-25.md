# Chromium remote controls — 2026-10-25

The user requires TV-style movement between highlighted controls, cards, links
and fields. A pointer, mousepad, coordinate click mode or native-keyboard fallback
does not meet that requirement. This clarifies T29.6 without changing completion
credit: T29 remains 13%, Overall remains 32% across 27 active software tasks.

## Implementation

The shared `src/input/web-remote.js` bridge identifies semantic controls, open
shadow-root controls and focusable cards, then selects the closest control in
the requested direction. Confirm activates the selected element. Selection
scrolls the control into view, including inside a scrolling shelf. Disabled,
hidden and inert controls are excluded. Modal dialogs constrain the candidate
set; DOM replacement restores focus to an available control. The highlight and
keyboard join a native dialog's top layer so its backdrop cannot cover them.

Confirm on a text field opens a D-pad keyboard with a local draft. Done applies
input/change events; Cancel/Back leaves the field unchanged. Password previews
are masked. Menu opens highlighted page actions for returning, scrolling and
browser history. Back dismisses an overlay, navigates history or returns to
Isolde. Home remains the existing owned-session exit. There is no cursor mode,
coordinate targeting or mouse-motion event generation.

The embedded WebEngine view injects the bridge in its isolated application world.
External Chrome uses private inherited protocol pipes, fixed script-registration
commands, no debugging listener and a dedicated browser profile within the
selected app/profile's existing private home. Existing browser data is never
read, copied, renamed or moved. The separate profile may require signing in
again. This safer implementation follows automatic review's rejection of an
earlier proposal that also renamed existing browser-profile data.

Appearance comes from the active shared theme (focus/surface/text colors,
typography scale and corners). Add website uses the Store's remote form and
validates HTTPS URLs, rejects credentials and unsafe input, derives stable IDs,
and persists only in the selected profile with atomic activation rollback.

## Verification and limits

`store-online-20261025-e` passes the browser regression but fails the Store
screenshot assertion: the Qt grab-image `save` method returns no Boolean even
though it writes the image. The assertion is corrected; no product behavior is
changed to pass that test. F passes 4/4. Screenshot review then finds the native
dialog covering the custom highlight; G fixes the top-layer placement and adds
a dialog-keyboard regression. G passes 4/4 in 21.42 seconds, source digest
`6905248e3033f02996dd293e7099f66456953b5a2aaa16d1fba86673c37d61da`.

Tests use real Qt key events and an actual Chromium WebEngine page, asserting
directional focus, activation, keyboard cancel/apply, checkbox selection, page
actions, modal scope, focus recovery and nested shelf scrolling. Captures are
under `build/vm-oracle/store-online-20261025-g/report/`. The G dialog and keyboard
captures are explicitly reviewed after the fix. These local fixtures establish
the generic navigation foundation; they do not prove every provider's custom
player, DRM, closed shadow roots or account flow. Provider-specific adapters and
actual external Chrome verification remain T29.6 until evidenced. F-Droid source
verification/installation and SmartTube usable content/playback remain separate
open gates; neither a source toggle nor process launch completes them.

Full run `store-remote-20261025-full-a` passes 32/32 in 281.15 seconds with the
same application source digest as G. Its 29 native captures are byte-identical
to reviewed G captures (13 from online G, 16 from lifecycle G); all online G's
13 images have now been directly viewed. Standing fixture and installed checks
are recorded below when completed.

## Installed checkpoint

Full A's staged Studio `/tmp/isolde-components-acceptance-xi55o067`, lounge
`/tmp/isolde-reference-config-z21j_b79` and Pastel
`/tmp/isolde-reference-config-z7yfx6tx` all pass their functional journeys. All
28 captures are directly reviewed, with both recreation/reset pairs compared
to the two original references. Known Pastel artwork/icon/proportion gaps,
placeholder media and Kodi management shortcuts remain open; this is a
preservation check, not broad design acceptance.

Release `314402c8ad29c6de07ea27ea` is deployed to Isolde-Test. The first `clients`
invocation stops at the intended installed-release guard, with no application
failure. After explicit Deploy, `Visual -Cases clients -Installed` passes at
`/tmp/isolde-reference-config-tqivq6qk`; all eight frames in
`visual/clients/attempt-20261025-232417-812/` are directly reviewed. Two bounded
physical guest Right presses select the actual CBC Gem Browse control in
external Chrome; the shared remote hint is visible and no cursor is used.
Physical Home closes the owned Chrome session and restores CBC Gem's exact
card focus. FreeTube launch/Home return also passes, independently of this
Chromium bridge. No account or subscription playback is exercised.

The legacy `appsoverflow` phase captures a Store removal confirmation rather
than the More apps gallery; it is not accepted as gallery evidence. This does
not replace the separate gallery regressions or the missing broader T26 gates.
Normal layout hashes remain unchanged and the service restores active with
zero restarts. Host browser/input and personal browser data are untouched.

Full A's installed Store journey subsequently fails at Plex removal:
`/tmp/isolde-components-acceptance-store-zfzd27kn`. All 14 captured frames
(including failure, not a removed frame) are reviewed. Android install/open/
return/remove succeeds; Plex opens its genuine UI with the bridge highlight,
then returns to the selected Store card, but the hidden WebEngine retains
keyboard focus and Store no longer receives D-pad keys. This is a regression,
not accepted lifecycle evidence. B adds guarded focus restoration whenever
Store becomes visible, prevents a queued web-focus callback after hide, and
tests actual active-focus restoration when the Store reappears. Full B and
the failed lifecycle journey must pass before completion.

## Verified focus correction

Full B passes 32/32 in 262.34 seconds, source digest
`300b30763e9d9d5fbde2ed674be122f011fa8beb4dbb20d1aaa8e816c88c16dd`.
All 29 native captures are byte-identical to reviewed A. B changes only Store
visibility focus restoration, the guarded queued web-focus callback and its
regression (plus documentation). A's reference and external Chrome evidence
remains applicable: their renderer/data/pipe/input script bytes are unchanged;
the two visibility handlers do not change the reference surfaces' presentation.

Installed release `2c9bbf97600df0a864e9d6f5` passes the repeated Store journey at
`/tmp/isolde-components-acceptance-store-9z38tg6t`. All 14 frames in
`build/vm-oracle/store-remote-20261025-full-b/visual/store/attempt-20261025-233922-517/`
are directly reviewed. SmartTube and Plex install/open/Home-return/uninstall;
Plex's removal confirmation now responds to D-pad input after return. Android
package absence and both removed manifests/activation records are verified.
SmartTube still spins with a network notice; Plex's captured current page is
loading. Neither screenshot proves playback. Normal layout hashes are preserved,
service active/running, zero restarts. No broad T29 gate or percentage is raised.

Remaining work includes verified live F-Droid catalog/install, complete actual
Add-by-URL launch/remove acceptance, provider/player adapters and Back/history
edge cases, SmartTube networking/playback, two-profile account acceptance and
Android containment. The standalone native diagnostic caller also needs to
supply the new shared appearance contract before testing a web-kiosk app;
the actual Isolde launch path supplies it and is verified above.
