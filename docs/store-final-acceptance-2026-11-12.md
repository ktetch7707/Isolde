# T29 Store installed software acceptance — 2026-11-12

Candidate: `store-final-20261112-j`, source digest
`79ff9f3de17636cfff622d3bf736206dc379e3ab8912efbe8e282e2023535d0f`,
installed release `3540c89c155eb0c38e750d40`. Full CTest62/62 passes.
Prior full h's isolated498.67ms Studio input stall is retained as BUG-125;
unchanged isolated i passes with31.40/42.03ms maxima, then j full passes.
No threshold was relaxed and no Studio implementation fix is claimed.

## Supported VM matrix and evidence

| Boundary | Production evidence | Limit |
| --- | --- | --- |
| Discovery | Signed official F-Droid browse/search, explicit opt-in, all5 NewPipe matches,4891-app browse, offline/default-OFF tests | Availability depends on signed provider metadata and declared SDK/ABI |
| Android install/update/remove | g actual SmartTube lifecycle; g store-updates attempt113406 real signed NewPipe1014→1015 Store update, progress/default Cancel, unsubscribe/cold reopen, installed base.apk hash and absence checks | Baseline older APK initialization explicitly fixture, actual newer update through UI |
| Atomic failure | g update prepared SIGKILL before activation, production root recover, original saved data restored and new runtime/package receipt absent; native write-failure/hash/provenance/rollback contracts | No actual ENOSPC Android UI claim |
| Profiles | Installed A update/cold state, B fresh absence, stale A request while B active refused with unchanged A receipts; native production private-root canaries and active-profile checks | Controlled account/data fixtures; no personal account login |
| Android setup/repair | j Configure android-store-support: actual fixed root package install/metadata verification and Weston dependency; full native provisioning/setup consent/cancel/failure/path tests | Uses initialized VM images; no repeated image download is claimed |
| Fullscreen | j store attempt150605 and attempt151112: real SmartTube public menus occupy full1280x800 VM, owned-wayland-kiosk event, physical Home restores exact Store card; actual removal | Fullscreen default, per-profile Windowed option tested natively/QML; no target display/GPU claim |
| External content | attempt151112 physical Confirm selects public content; visible loading spinner persists, Home still exits and cleanup/removal completes | Decoded live playback NOT verified; controlled runtime normal-return/error/data-isolation fixture covers Isolde's boundary |
| Chromium | External Chrome physical remote text/Cancel/Done/Menu/scroll/history/rootBack fixture plus embedded WebEngine; actual CBC/Plex menus/Home; j Plex install/open/return/remove | No DRM/entitlement/playback assertion |
| Offline | full-d kernel IPv4+IPv6-denied cold three-process Store15-frame workflow and Web OFF zero-request/native contracts | Network-dependent provider content unavailable offline |

Installed j attempts have14 and16 reviewed captures. Contact sheets are
`build/vm-oracle/round2-20261111-b/review/store-final-j-00/01/02.jpg` and
`store-content-j-00/01/02.jpg`. All reviewed; lifecycle/focus/consent/containment
accepted, spinner excluded from any playback claim. Raw automated receipts keep
visualReview pending; this is the separate agent review record.

Installed result receipts are under j/report/evidence-store-1794524987326079157
and evidence-store-1794525325634069107/isolde-components-acceptance-store-qh155_8t.
Both exact-profile temporary authorization rules pass allow/deny scope probes
and are removed before normal service restore. Layout hashes unchanged; normal
service active/running/zero restarts. No retained normal-profile privilege.

Final controlled runtime fixture and runner self-tests pass in focused o: six Store registrations, store_lifecycle22/22 and oracle self-tests. Original full j remains applicable: only named tests and harness changed after j; no application delta.
The fixture drives production android-session.main with explicit external D-Bus,
process and decoder/session responses. It asserts normal ready/cancel return,
provider-error propagation, owned SIGTERM, display teardown and foreign account
canary preservation in both selected profile roots. It does not decode video.

## Live-service handoff

U01 remains a post-software user-owned check: SmartTube live decoding, account,
region/entitlement and provider changes. The observed spinner is retained as
unverified live content, not silently called playback or attributed to a proven
provider outage. A later reproducible Isolde defect reopens its software gate.
