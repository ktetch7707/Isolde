# Store design and lifecycle acceptance — 2026-10-25

User request: a professionally designed, easy-to-use Store with Android/Chromium selection at the top right, plus repeatable tests that install, open, close and uninstall an app of each type.

## Delivery gates

- [ ] Native themed Store, top-right platform selector, visible remote focus, search, details and separate Install/Open/Uninstall actions.
- [ ] Remote GUI regressions exercise both platform views, confirmations, cancellation and selected-card preservation.
- [ ] Fresh-profile VM journey installs, opens, closes and uninstalls SmartTube through the Store; Android package absence is verified before metadata removal.
- [ ] The same journey installs, opens, closes and uninstalls the Plex Chromium package through the Store.
- [ ] Current-profile activation writes, legacy read compatibility, preserved normal profiles and both standing recreation/reset fixtures.
- [ ] Full regressions, reviewed screenshots and commit/push evidence.

## Implementation

AppStoreView is now a native QML surface using the shared appearance model. Validated catalog presentation data supplies platform labels/types, column count, spacing, card height and details width. Device apps remain separately accessible so existing native packages are not misclassified as Chromium. Install and removal require explicit confirmation, default to Cancel and preserve the selected app. Open returns to the same Store card on app close. Chromium navigates to about:blank when hidden to release the closed page.

AppCatalog installs/removes Android packages asynchronously through the existing bounded android-session helper. It requires successful completion, matching operation, verified package presence/absence and successful owned-container shutdown before changing activation. The helper uses only the selected profile's userdata; no root grant, arbitrary executable or shell recipe is added. The app list and package manifests are written beneath the active profile. A legacy shared activation is read for compatibility without startup migration; subsequent edits write a private profile snapshot, including Household.

The VM Store case uses a new UUID, separate config/data/cache, no account sign-in and disabled automatic sources/Kodi startup. Remote keys operate the actual Store. Only the two opened-app phases ask the host wrapper to send Home, restricted to Isolde-Test. The test asserts both activation records and manifests are gone and that the Android install receipt no longer contains SmartTube. Playback and provider online-content availability are separate acceptance requirements.

## Verification ledger

store-20261025-a passes the focused studio_selection suite. Its initial grab images captured an unsettled editor frame and are rejected as Store visual proof; the test now hides the editor and waits for rendering. B passes 31/31; the two corrected native Store grab images are reviewed. C fails because its version validator wrongly requires three numeric components and rejects Kodi 21.3; the validator is corrected to accept one through four safe numeric components.

D passes 31/31 and the actual Store journey for both apps in /tmp/isolde-components-acceptance-store-5smudja_. All 14 frames in build/vm-oracle/store-20261025-d/visual/store/attempt-20261025-191513-099/ are reviewed. Plex reaches its real provider interface; remote Home returns to the same Store card and uninstall removes activation/manifests. SmartTube launches, returns and is removed with package-absence confirmation, but online content still spins and Android emits a network notice; playback is not accepted. Both normal layout hashes are unchanged and Isolde restores active with zero restarts.

E corrects availability wording, groups desktop Chromium wrappers under Chromium, names operations and scrolls details to keep large-text remote actions visible. E passes 31/31 (267.24-second CTest log), including the new large-text/classification regression; its two native Store captures are reviewed and the other 20 native captures match D byte for byte. The actual journey fails at Android launch after successful install: no ready event is logged. Its three captured Store frames are reviewed; it is not counted as passing lifecycle evidence. The normal service and both layout hashes are preserved. The failure UI was not captured by the first harness; F adds bounded failure capture and explicitly restores Isolde window/input focus after an Android Store operation. This addresses a possible focus handoff; verification must establish the outcome before acceptance.

F passes 31/31 but its lifecycle test fails at installation. All three frames are reviewed, including store-storefailure.png: the UI says Android could not start. Bounded service diagnostics identify OSError 24 (too many open files) in Waydroid's container manager, matching the upstream foreground_pipe selector leak reported at https://github.com/waydroid/waydroid/issues/2357. This supersedes the tentative focus diagnosis. No failure is counted as acceptance. Service and layouts are preserved. The repeated phase warning is a secondary runner error after its failed guardian replays the tail; the primary failure is retained in visual-store.log/status.json.

The installer now applies a bounded AST-checked selector cleanup to the pinned runtime, keeps a root-private backup, rejects unknown source shapes and repeats without rewriting an already repaired helper. The normal Android installer includes it; the narrow android-repair Configure target uses the existing Ubuntu administrator prompt, adds no persistent privilege, refuses an active Android session and restarts only its idle daemon. It then executes 128 actual helper commands and verifies the descriptor count is unchanged. Local provisioning tests pass 10 with four Linux-only skips; authoritative Linux/full/GUI verification follows. Store failure capture remains as a diagnostic improvement.

## Reconciled later checkpoint — 2026-10-25

Retained G reports now record 31/31 full tests, deployed release `014674ec29d7dfb787fd8ca9`, and a successful functional Store journey through both Android and Plex install/open/close/remove phases. `build/vm-oracle/store-20261025-g/report/status.json` records unchanged normal layouts, service active/running with zero restarts and `visualPassed: true`, while `visualReview`/`visualAcceptance` remain pending and `semanticAcceptance` is `not_verified`. This audit read the reports/logs; it does not newly review the 14 captured frames or mark these delivery boxes complete. The retained reports take precedence over the earlier pending-run wording, but a broad accepted lifecycle still requires the missing review and full T29.8 cases.

The later `store-online-20261025-c/report/status.json` records three passing focused Store registrations with `fullSuite: false`; it is not a full-regression or real-source/remote-bridge acceptance result. Current source work continues separately. See [scope/evidence audit](first-product-software-scope.md) and T29/T31 in [the backlog](agent-backlog.md).
