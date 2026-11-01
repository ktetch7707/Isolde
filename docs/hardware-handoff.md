# User checks after software reaches 100%

This is a handoff checklist, **not** part of the software percentage. The user will move the finished software to the actual hardware and decide how to test it. No result below is required to finish an active T-task or S1. Definition: [VM software completion](vm-software-completion.md).

Current state: software is still in development (see current backlog/calculator); no final artifact or physical compatibility pass is claimed. T31 will replace the development notes below with the exact tested artifact, VM evidence and residual limitations. Do not install anything on the user's host as part of preparing this handoff.

## Final artifact record — filled by T31

- Source revision, artifact filename/location and SHA-256; how the user checks the checksum.
- Tested VM OS/virtual hardware, install/update/recovery versions and offline resources.
- Path to the illustrated shipped guide and recovery image; what install/reset/rollback preserves or erases.
- Supported app/game/emulator/provider matrix: real VM runtime tested, boundary fixture tested, and target-only assertions untested. Include known external limitations without calling untested routes compatible.
- Concise software evidence and any disclosed nonblocking uncertainty. Known failing required software behavior must be repaired before S1.

## Equipment and personal checks

| Checklist | What the user may check on their equipment |
| --- | --- |
| H01 display/audio/input | Real TV/GPU modes, safe area and viewing-distance readability; audio devices; remote/CEC/button mapping, key repeat, suspend/resume/hotplug, response and animation performance |
| H02 games/controllers | Remote-to-controller handoff, actual Xbox/other controller feel/disconnect, per-runtime lawful gameplay/save/relaunch/return, target GPU/firmware dependencies and performance |
| H03 optical | Actual CD/DVD recognition, required/multiple/wrong/missing/damaged media, install/play, changed mount, eject/busy/unplug and preservation of game/save/source data |
| H04 capture/switch/audio | Actual capture formats, HDMI/RS-232 switching, routing, device disconnect/reconnect, sound/video sync and latency |
| H05 install/recovery | The user's USB preparation/firmware/boot menu, correct target-disk choice, first run and real reboot/power-loss/recovery/rollback behavior; follow the shipped backup/erasure warnings |
| U01 accounts/services | Owned subscriptions/DRM, SmartTube/live provider content, actual VPN location and region availability, CBC/other service playback, per-account behavior and service changes outside Isolde |
| U02 personal usability | Preferred colors/layout/density, comfort at actual TV distance, whether the workflow labels and guide make sense to the user; no prior participant sign-off was required for S1 |

Record the artifact/version, steps, expected and actual result, and useful diagnostics for any finding. A concrete application defect becomes a normal software regression; do not interpret a past VM pass as proof that the user's observation is wrong. This checklist imposes no formal release/qualification campaign or requirement to publish.
