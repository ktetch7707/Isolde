# T22/T23/T24 installed software acceptance — November 4, 2026

**Accepted: T22, T23 and T24 are 100%; deferred T15.4 is complete.** The source-bound auditor passes all34 named records with `evidenceComplete: true`, zero errors. All58 final installed captures are individually reviewed. Final normal service is active/running with zero unexpected restarts; both normal-layout hashes match before/after.

## Candidate and reproducible evidence

One integrated candidate, `studio-batch-final-navigation-20261104-a`, passes **55/55 CTests**, zero failed/skipped, **431 seconds** from the CTest XML. It is installed as release `6c359ceb7a5b813e5e6952b7`.

| Identity | SHA256 |
| --- | --- |
| Source digest | `fb5fd9c480fece358ae83f9cfd6e6d1e6d5ae852c513612f5ff4807b197bc0ed` |
| Source archive | `18b6b19b4879abdacfa4797ba2dbdf083e040da605e9850933ee21a155c16742` |
| Installed executable | `e26739523c9f4de5d7b5d85dd8a18b248fe1cbccb79f08deaa5ce65cdcf09721` |

Evidence root: `build/vm-oracle/studio-batch-final-navigation-20261104-a/`. Frozen, hash-verified source: `build/studio-batch-final-navigation-source/`; source receipt: `build/studio-batch-final-navigation-audit/source-receipt.json`. The candidate was built from the shared working tree, including identifiable concurrent Guide/Android/appearance work; it is not represented as an isolated clean-HEAD build. Unrelated staged changes remain owned separately.

The source-bound acceptance audit is `build/studio-batch-final-navigation-audit/report.json`; every referenced artifact has a SHA256 and explicit review. Native review ledger: `build/studio-batch-final-navigation-native-review.json` (77 PNGs byte-identical to individually reviewed W/X/Y3 images). Final installed review ledger: `build/studio-batch-final-installed-review.json`.

## Implemented and verified behavior

T22 delivers four editable templates, explicit page/interface scope, responsive rules, private reference imagery, Home/page/component inheritance, named shared styles, appearance-only copy/paste, independent and linked reusable components, impact preview/Detach, and bounded portable packages with explicit missing-source remapping. Native production tests cover malformed versions/actions/paths/art, definition cycles, collisions/locked nodes, cross-profile artwork ownership and whole-document rollback. Export excludes credentials, accounts, private paths, media/Kodi data and editor-private references.

T23 delivers the actual full-viewport draft renderer, Try interaction with external effects intercepted, changed-page summaries and actionable findings, conflict/failure/Retry handling, bounded named versions and scoped defaults, and exact navigation/recovery. Installed root tasks induce a real private-archive filesystem failure, preserve saved bytes/draft, then repair and Retry. Delete/Undo, color/wheel cancellation, reflow, size, nested scopes, scrolling, header recovery and Apply are reviewed through visible controls.

T24 makes Studio the ordinary editor behind Edit Home/Edit page/context/Settings, retires the duplicate packaged editors, and reconciles every historical operation in [the feature map](design/page-studio/feature-map.md). The final correction restores stored left/top/none page-menu rendering using the existing PageNavigation in the shared runtime/Studio geometry. The explicit editable-menu operation now selects an existing component or starts normal private placement; Cancel/Undo preserve the original. No startup migration or silent rewrite was added.

## Evidence reuse and supported matrix

Root Home core and desktop journeys reuse Y3 only after comparing every production file: the final differences are HomeScreen, PageNavigation and PageStudioView. The new menu branch requires a nonempty named-page ID and stays inactive throughout those root/nested/header/scroll journeys. The editable-menu operation is not invoked there. Exact proof: `root-journey-applicability.json` in the audit directory. Core: **1578 inputs / 370.8555s / 34 reviewed captures**. Desktop: **1057 inputs / 206.2223s / 10 reviewed captures**; equivalent remote/pointer operations produce identical saved bytes, including nested movement and held-drag cancellation. These are measured scripted-agent journeys, not uncoached participant scores.

Named-page/templates/reference journeys are rerun on the final installed executable because their visuals are affected by the menu correction. The real-Kodi pages workflow uses a fresh UUID profile, native Kodi fixture import, local clip playback/pause/stop/return and ordinary remote Studio controls. It verifies left/top/none, direct Movies destination and saved-design page inclusion. It does not claim an injected Home tab; native tab contracts and installed reference navigation supplement this direct-destination proof. Provider fixtures are labelled; they do not claim third-party streaming acceptance.

Disconnected evidence denies AF_INET/AF_INET6 with errno101 while permitting local UNIX sockets. It proves Studio edit/Apply/local managed art/portable round trip and actual stopped-process cold reopen, plus local Search with Web Off and exact Back focus. This is a socket-denial fixture, not a network namespace. Full appliance Generator/clean-VM integration remains T31.


| Final installed journey | Actual measured result |
| --- | --- |
| Isolated pages/Kodi | 913 input events, 161.770s journey; 172s driver; nine reviewed captures |
| Saved designs/offline | 2143 input events, 504.029s journey; 498s last driver interval; 31 reviewed captures |
| Lounge recreation/reset | 101s driver; four reviewed captures |
| Pastel recreation/reset | 172s driver; 14 reviewed captures |

The reference helper does not record per-key usability metrics. Audit records use zero *additional agent review* keys/time for those comparisons, explicitly; driver durations above are actual elapsed intervals. Successful final journeys have no observed unintended launch, rearrangement or lost focus. Earlier failed helper attempts are excluded, retained in the diagnosis record. Scripted key counts include bounded menu traversal and keyboard input and are not human usability timing.

The raw reference receipts intentionally leave automatic visual/semantic approval pending. Their snapshot-only collection summary also states that save/reapply/reset alone is insufficient. This acceptance adds explicit image review plus the actual overflow/See All/album→tracks workflow assertions and `pastel-provider-final.json` boundary-call trace; it does not relabel the raw conservative summary or claim live provider playback.

Final restoration receipt: `build/studio-batch-final-navigation-audit/evidence/restoration-final.json`, MainPID3589271. Root normal layout SHA `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Household profile SHA `85865ee3f03c6301cb2b3153bacc1bde8fba01b2c005279f7cd86263311cb4cf`, both unchanged. The bounded runner exited0 and no VM action remains active.

## Actual VM measurements and limits

VirtualBox Linux, four configured vCPUs, 8192MiB, VMSVGA; Qt Quick software renderer. Fresh final receipts contain 100 input/frame samples at each viewport.

| Viewport | Input-to-visible p95 | Mean frame interval | Longest unacknowledged input |
| --- | --- | --- | --- |
| 1280×720 | 22.443ms | 14.257ms | 47.014ms |
| 1920×1080 | 31.962ms | 16.180ms | 42.668ms |

Both cover 100 visible widgets, independently validated 1024 editable nodes and ten lifecycle cycles, clients 0→0 and no retained history after Cancel. Actual 720p/1080p/4K fields/menu/keyboard/reuse captures show readable proportions, visible focus and bounded scrolling; reduced-motion, contrast, long-label/missing-art and empty/large collection regressions pass. VM measurements do not qualify physical GPU/TV/remote/HDMI latency.

Known failed attempts remain failures in the diagnosis record. Helper route corrections reused the passing application; the shared menu renderer change justified one replacement full suite before launch. No extra full run was made solely for documentation, accounting or helper fixes. Focused host helper/hash/path/tamper/cache contracts and Python compilation passed. The Windows full oracle invocation terminated at its SIGTERM test and is not claimed passed; the actual Linux oracle registration passes within 55/55. clang-format, qmlformat and Ruff were unavailable on the host; no formatter-clean claim or host installation is made.

Physical devices, live paid accounts/region/DRM and personal usability remain the user-owned post-100% handoff under [VM software completion](vm-software-completion.md). T25/T26/T27/T28 and final T31 retain their separate software scope. This batch does not declare the whole product complete.

Integrity receipts: final audit SHA256 `af085b03399e20a8517abfb29ae8a411eb1859fe8b6c0e6ef8d83f3a7383ad7d`; final installed review ledger SHA256 `60a0bae400f6f8efbe4f6a90eea987fabb86c90513fbdaa649fbd36ccaaee850`. `tools/backlog-progress.ps1 -CheckTable` passes: sum1448, Overall54%,27 ACTIVE tasks.
