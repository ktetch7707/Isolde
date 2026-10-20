# Page Studio implementation and VM evidence — 2026-10-19

This is a working provisional Studio slice, not completion of T17–T24 or user usability acceptance. The legacy editor remains available until cutover gates pass.

## Installed build

- VM: `Isolde-Test`; release `4110917b3cd529d5835ba8bb`.
- Actual running executable: `/home/isolde-dev/.local/share/isolde-appliance/releases/4110917b3cd529d5835ba8bb/bin/isolde`.
- Executable SHA-256: `dd7fffb9b0fec2e595b839ff3cd9bd760455e3bf5f85a76c3f3eb1de723e0145`.
- Frozen source: `/home/isolde-dev/isolde-runs/studio-implementation-12/source`; build: sibling `build`.
- Source archive SHA-256: `c1aba8309e446ae08553731b83db50689f7b8603863d65a053647073dd5e27e9`.
- Host snapshot: `build/studio-implementation-12/` contains HEAD, full dirty patch, per-file hashes and archive hash. Base HEAD was `9de7bf7`; the dirty source bytes, not HEAD alone, identify this build. Application/test files were checked against the manifest after building. Subsequent documentation amendments do not change the installed binary.
- Deployment used `python3 tools/install-appliance.py install --build-dir ../build`, then `systemctl --user restart isolde.service`. The installer repeated all tests before selecting the release.

Open **Settings → Home → Page Studio (preview)**. Remote Left/Right selects existing editor actions; Studio is the final action. Studio's toolbar exposes Add, Pages, Layers, Undo, Redo, Grid, Fit, Preview and Apply. Menu opens selected-component tools. Back cancels gestures/tools, then offers dirty-exit choices.

## Implemented slice

`StudioSession` holds one complete-interface draft with profile invalidation, validated semantic commands, atomic multi-placement/grouped edits, deterministic identities, conflict checks against both the live model and the actual saved file, and bounded history (100 operations / 32 MiB). Overflow refuses the edit until history is explicitly released. Failed operations retain the draft and applied bytes. Apply uses the existing atomic HomeLayout persistence and v1 backup pipeline.

The native QML workspace renders the draft through HomeScreen/ComposedHome. It provides explicit grid-conversion preview, editable grid dimensions, remote pickup/drop, anchored resize, numeric placement, pointer movement, Shift/Swap/Cancel proposals with affected-component overlays, a basic Layers/page picker, Clock/Text insertion, rename/duplicate/remove, text/surface-opacity/corner editing, full-viewport preview and explicit Apply. Runtime actions in preview are intercepted.

The renderer now uses measured content geometry below the header, explicit rectangles that preserve empty cells, and placed-component navigation. The placement layer is a sibling of the flow layout. Qt sequence values no longer incorrectly disable grid rendering.

## Verification and retained evidence

- Full Linux CTest: **14/14 passed**, 48.96 seconds. `../ctest.log`; installer repeated 14/14 successfully in `../install.log`.
- Component contracts: draft/no-write/undo bounds, canonical comparison/identities, live and external-file conflicts, profile teardown including reentrant callbacks, atomic write failure/focus preservation, multi-placement rollback, collision proposals, grouped additions and one-record undo.
- QML: actual scene with visible placed components; matching runtime/editor rectangles at 1280×720, 1920×1080, 3840×2160 and 1280×800; remote Add/history/preview/Apply/reload; rendered zero-opacity and zero-radius controls.
- Installed Studio journey: `/tmp/isolde-components-acceptance-u7lfm2xd`. Original bytes survived opening, draft edits and preview; Apply preserved the original v1 backup; reopening preserved applied bytes. Result JSON identifies the exact installed executable.
- Lounge recreation/reset: `/tmp/isolde-reference-config-enw44obb`.
- Pastel recreation/reset: `/tmp/isolde-reference-config-xx0hwu86`.
- Both fixture runs selected/saved/reapplied skins from Settings and preserved saved skins on reset. Assistant inspection of the four actual captures passes the documented landscape recreation/reset criteria; fixture content remains synthetic and does not establish real playback or provider entitlement.
- Durable host evidence: `build/studio-installed-12/` contains screenshots, copied result/config/shell logs, installation/CTest logs and `visual-review.json`. Images/build outputs are intentionally excluded from commits.
- Final normal service: active/running, `NRestarts=0`, PID 889718; actual `/proc` executable resolved to the release above. `normal-home.png` records restoration. Existing detected-source intrusion on normal Home remains T25, not a closed gate.

Repeat installed Studio acceptance with:

```powershell
./tools/run-visual-acceptance.ps1 -Case studio -SourceRoot /home/isolde-dev/isolde-runs/studio-implementation-12/source -OutputDirectory ./build/NEW-studio-evidence
```

Use [vm-oracle.md](vm-oracle.md) for source freezing, deployment, sequential fixture checks and restoration. The Studio driver uses disposable configuration/data roots and the same exclusive graphical-test lock and capture acknowledgement protocol as the existing harnesses.

## Open requirements

T16.3 replay is now verified in frozen snapshot 13: `/home/isolde-dev/isolde-runs/studio-implementation-13/targeted-tests.log` and `ctest.log` (14/14, 52.73 seconds). A real QTest pointer drag, remote key sequence and numeric inspector replay start from the same identities and produce byte-identical drafts without applied writes. The only application change from installed snapshot 12 is a nonvisual MouseArea objectName used by replay tooling. Archive SHA-256: `f61fc75fe0accc6cf6644e558a3ee8070d4a61ef686651515ca335249ebafd67`.

Scrolling section grids, nested/group editing, complete desktop parity beyond this core move replay, the full catalog/inspector and legacy-operation coverage, templates/tokens/symbols/portable assets, complete route/backup/restore flows, performance/offline acceptance and final editor cutover remain open under T17–T24. Basic preview controls do not close those tasks. Actual user/representative usability review is still pending. No disconnected cold-boot Generator Test, new real-Kodi acceptance or physical hardware qualification is claimed.

Earlier rejected snapshots and logs remain under `build/studio-implementation-*` and the corresponding guest run directories. Screenshot inspection caught the hidden placement layer; an audio test's fixed 100 ms wait also blocked an installer run. Both were corrected without bypassing gates or changing personal profiles. Captures under `studio-installed-04` and the misleadingly named `studio-installed-08` were of release `5263a797a481bb6dcfa6b14b`; they are not acceptance evidence for the final release.
