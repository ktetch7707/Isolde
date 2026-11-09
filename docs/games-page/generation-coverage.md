# Games generation coverage — user expansion, 2026-10-26

The user's completion target is games from **Sony, Microsoft and Nintendo across generations**. The original four emulator tracks are a first implementation batch, not the complete requirement. This ledger remains part of T30.1/T30.6/T30.9. A process launch, installed runtime, catalog entry, compatibility-list entry or captured console picture is not accepted gameplay.

Every delivered route must support TV-remote selection/setup/launch, Xbox-controller gameplay without a mouse, profile-private configuration/saves, safe stop and exact return. Record a real legal playable fixture, tested runtime/source identity, compatibility limits and evidence for each declared platform. A physical console route additionally needs a verified controller bridge; HDMI capture alone does not deliver Xbox-controller input.

| Family | Generations / systems retained | Candidate route and current gap |
| --- | --- | --- |
| Sony | PlayStation | Beetle PSX software core route supports single CHD images and one user-supplied regional firmware selection. Gameplay, controller and save acceptance remain open; physical discs, CUE and live swaps are not supported. |
| Sony | PlayStation 2 | PCSX2 adapter/setup is implemented in the current batch; actual firmware/gameplay/offline/update acceptance remains open. |
| Sony | PlayStation 3 | [RPCS3](https://rpcs3.net/compatibility) candidate with title-specific compatibility. Provisioning, firmware, directory/package import, renderer/performance, input and gameplay remain open. |
| Sony | PlayStation 4 | [shadPS4](https://www.shadps4.net/compatibility/) is a candidate requiring title-specific assessment. No Linux appliance support or general compatibility is promised by source presence. |
| Sony | PlayStation 5 | No verified emulator route selected. Investigate an owned console or an available PC edition; console control, Xbox-controller bridge, launch/return and hardware acceptance remain unresolved. |
| Sony | PSP | PPSSPP pinned source builder, adapter and settings are implemented in this batch. Actual guest build/install is verified in Configure I; P observes Webfest gameplay with VM keyboard input and return. Xbox-controller, private saves, offline and broader title acceptance remain open. |
| Sony | PS Vita | [Vita3K quickstart](https://vita3k.org/quickstart.html) documents Linux private storage, archive installation and firmware/font setup. Pinning/provisioning, safe package import, firmware setup, touch/controller mapping and actual title acceptance remain open. PSP support does not cover Vita. |
| Microsoft | Original Xbox | [xemu](https://xemu.app/docs/about/) candidate. Its [required-files contract](https://xemu.app/docs/required-files/) needs MCPX, compatible BIOS and a writable hard-disk image; upstream supplies a copyright-free basic disk image, but boot firmware is not bundled. Isolde-Test currently has no firmware. A dedicated validated import/storage/disc adapter, private writable disk, Xbox mappings and actual title acceptance remain open. |
| Microsoft | Xbox 360 | [Xenia](https://xenia.jp/) candidate; upstream [build documentation](https://github.com/xenia-project/xenia/blob/master/docs/building.md#linux) explicitly labels Linux support experimental and incomplete. A pinned Linux artifact, GPU/runtime capabilities, legal homebrew fixture, input/session/save isolation and gameplay need verification before selecting this appliance route. Research-project compatibility is not an Isolde product pass. |
| Microsoft | Xbox One; Xbox Series X/S | No verified emulator route selected. Owned-console or available PC-edition routes require independent assessment, account/offline constraints and Xbox-controller/session ownership proof. |
| Nintendo | NES; SNES | Explicit RetroArch Nestopia and bsnes-mercury-balanced routes in this batch. Pinned guest/core installations are verified in Configure I; per-platform gameplay/save/offline evidence remains open. |
| Nintendo | Nintendo 64 | Separate core/runtime, controller mapping and gameplay acceptance remain open. A generic RetroArch frontend is insufficient. |
| Nintendo | GameCube; Wii | Dolphin adapter and Xbox/GameCube plus Wii Remote/Nunchuk or Classic presets in this batch. Motion, pointer and game-specific compatibility need real acceptance; a preset is not universal Wii support. |
| Nintendo | Wii U | [Cemu](https://cemu.info/) candidate. GamePad second screen/touch/gyro need a controller-only design and title-specific acceptance before support can be claimed. |
| Nintendo | Switch; Switch 2 | Separate current runtime/format/firmware/key/compatibility assessment remains open. No unverified emulator or universal support claim is added. An owned-console route also needs verified input bridging. |
| Nintendo | Game Boy; Game Boy Color; Game Boy Advance | RetroArch Gambatte and mGBA routes in this batch; actual platform fixtures and gameplay/save/offline acceptance remain open. |
| Nintendo | DS / DSi; 3DS | DS has a DeSmuME route with both screens, controller stylus and simulated microphone; actual gameplay/input/save acceptance remains open. DSi and 3DS remain separate, not integrated routes. |
| Nintendo | Virtual Boy | Beetle VB route with two controller D-pad mappings; actual gameplay/input/save acceptance remains open. |

The project must not mark this expanded target complete while required generations have no verified route. Modern-console integration may depend on hardware, owned software/firmware and capabilities outside Isolde's control. Record those constraints explicitly; do not silently reduce the target to the currently integrated systems or imply that every commercial title works.

The N64 route remains excluded until an exact Ubuntu 26.04 core package/version, authenticated package metadata and installed core path are identified and its integration is implemented. The authenticated release index inspected below contains no N64 libretro core; that does not prove a package is unavailable in other pockets or a different verified runtime cannot be used.

## Expanded Sony and Nintendo implementation blockers — 2026-11-09

The following means **not integrated in Isolde**, not that an emulator is
impossible or that every title is unsupported upstream. The existing managed
routes above do not implicitly supply any of these systems.

| Route | Concrete work before declaring an integrated route |
| --- | --- |
| PlayStation | The single-CHD Beetle PSX route is implemented as described above; CUE/multi-file import, direct physical discs, region autodetection before launch and live disc swapping are not. Gameplay, actual controller and save behavior still require verification. |
| PlayStation 3 | Verify a pinned Linux runtime and dependencies; implement firmware setup and safe directory/package import, private storage, controller and session handling. Compatibility and renderer constraints are title-specific. |
| PlayStation 4 | Establish a supportable Linux runtime artifact and dependencies and a bounded title/format/input/storage contract before adding an adapter; upstream compatibility information alone is not an appliance integration. |
| PlayStation 5 | No runtime or owned-console control route has been selected. A console route also needs an Xbox-controller bridge and launch/return design; capture alone is insufficient. |
| PS Vita | Verify a pinned Linux artifact and dependencies; implement firmware/font setup, safe archive installation, private storage, and a controller-only touch/input workflow. |
| Nintendo 64 | Verify an exact core package version and installed core path against authenticated Ubuntu 26.04 amd64 metadata; add the pinned core, format/controller profile, private saves and tests. The installed RetroArch frontend by itself does not provide this route. |
| DSi | Select and verify a pinned runtime/core and dependencies, firmware and formats; implement dual-screen layout, touch and microphone alternatives, private saves and controller/session handling. The DS route does not establish DSi support. |
| 3DS | Select and verify a separate pinned runtime and dependencies; implement its firmware/format requirements, dual-screen/touch input, private storage and session handling. |
| Wii U | Verify a pinned Linux runtime and dependencies; implement format/import and private storage plus a controller-only GamePad second-screen/touch/gyro workflow with explicit title limitations. |
| Switch / Switch 2 | No verified runtime or owned-console route is selected. Each generation needs its own lawful content/firmware and input/session assessment; neither a candidate for one nor HDMI capture establishes the other. |

The isolated development host is Ubuntu 24.04 amd64 and has no cached apt
package-index files; its apt cache cannot establish a 26.04 package pin. On
2026-11-09 the official Ubuntu 26.04 release index was subsequently fetched
read-only and authenticated, enabling the DS and Virtual Boy routes documented
in [the emulator contract](emulator-runtime-contract-2026-11-09.md). The
same index contains the Beetle PSX package, now used for the bounded
single-CHD route. No host package was installed.

To unblock an apt-based route, obtain authenticated **target-release and
architecture** index metadata from the configured Ubuntu archive and verify
the exact package/version, dependency closure and installed executable/core
path. For a non-archive build, establish the upstream source/artifact identity,
license and independently provisioned trust root before pinning. In either
case, the existing signed offline bundle and rollback contract must include
the new artifacts. Then implement the route end-to-end in the runtime manifest,
package catalog/provisioner, validated session and remote setup/import UI,
with focused positive and failure tests. Firmware and games remain
user-supplied; do not download them as test data. VM acceptance remains a
separate later phase, not a reason to invent missing runtime evidence.

## Enforced catalog budgets

The shared `GameResourceLimits.h` contract is surfaced through the Games emulator-coverage API and shown in the remote setup screen. Current bounds are 50,000 catalog entries, 50 entries per page, 128 platforms, 256 collections, 32 editions per game including its primary registration, 1,600,000 retained provenance rows, 32 MiB per catalog archive, 128 MiB of artwork expanded during one restore, and a 512 MiB profile artwork store. These are software/resource ceilings, not performance or target-machine acceptance claims. The catalog code and its tests consume the same constants so the visible matrix does not advertise separate, drift-prone numbers.

Primary project pages above were consulted on 2026-10-26. They establish candidate capabilities and limitations, not installed Isolde acceptance. Do not download commercial games, account material or firmware as a substitute for user-owned content.
