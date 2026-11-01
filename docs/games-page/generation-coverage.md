# Games generation coverage — user expansion, 2026-10-26

The user's completion target is games from **Sony, Microsoft and Nintendo across generations**. The original four emulator tracks are a first implementation batch, not the complete requirement. This ledger remains part of T30.1/T30.6/T30.9. A process launch, installed runtime, catalog entry, compatibility-list entry or captured console picture is not accepted gameplay.

Every delivered route must support TV-remote selection/setup/launch, Xbox-controller gameplay without a mouse, profile-private configuration/saves, safe stop and exact return. Record a real legal playable fixture, tested runtime/source identity, compatibility limits and evidence for each declared platform. A physical console route additionally needs a verified controller bridge; HDMI capture alone does not deliver Xbox-controller input.

| Family | Generations / systems retained | Candidate route and current gap |
| --- | --- | --- |
| Sony | PlayStation | [DuckStation](https://www.duckstation.org/) candidate. Runtime/license/firmware/controller/import/session integration and gameplay are not implemented. |
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
| Nintendo | DS / DSi; 3DS | Separate runtimes, firmware and dual-screen/touch/microphone/controller-only workflows remain open. [melonDS](https://melonds.kuribo64.net/) is a DS candidate, not a 3DS implementation. |

The project must not mark this expanded target complete while required generations have no verified route. Modern-console integration may depend on hardware, owned software/firmware and capabilities outside Isolde's control. Record those constraints explicitly; do not silently reduce the target to the currently integrated systems or imply that every commercial title works.

Primary project pages above were consulted on 2026-10-26. They establish candidate capabilities and limitations, not installed Isolde acceptance. Do not download commercial games, account material or firmware as a substitute for user-owned content.
