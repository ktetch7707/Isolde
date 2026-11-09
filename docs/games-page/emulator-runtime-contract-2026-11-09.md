# T30 emulator integration contract — 2026-11-09

This implementation checkpoint narrows emulator integration to the declared Linux appliance matrix. A runtime being installable or launchable is **not** a game-compatibility, controller, performance, or gameplay acceptance claim.

## Bounded platform matrix

The version-2 `config/game-runtimes.json` manifest is the source of truth for the supported host and every integrated route:

| Runtime route | Platforms | Declared formats | Package set |
| --- | --- | --- | --- |
| PPSSPP | PSP | ISO, CSO, CHD, PBP, ELF | `isolde-ppsspp` |
| Dolphin | GameCube, Wii | ISO, GCM, RVZ, GCZ, WBFS, DOL, ELF | `dolphin-emu` |
| PCSX2 | PlayStation 2 | ISO, CHD, CSO, BIN, ELF | `pcsx2` |
| RetroArch / Nestopia | NES | NES, UNF, UNIF | RetroArch + Nestopia |
| RetroArch / Gambatte | Game Boy, Game Boy Color | GB, GBC | RetroArch + Gambatte |
| RetroArch / mGBA | Game Boy Advance | GBA | RetroArch + mGBA |
| RetroArch / bsnes-mercury-balanced | SNES | SFC, SMC | RetroArch + bsnes-mercury-balanced |

The managed provisioning target is Ubuntu 26.04 amd64 only. PlayStation, PS3/4/5, Vita, N64, DS/3DS, Wii U, Switch generations, Original Xbox, Xbox 360 and Xbox One/Series are explicitly not integrated; no generic frontend or candidate emulator is presented as support for them. Extending the matrix requires a separately pinned runtime/core and formats, bounded remote setup, safe firmware/input/session contracts, and focused tests.

## Production contracts added

- The runtime manifest is versioned and validated in both the trusted emulator adapter and C++ registry. The generation ledger must exactly match the manifest's route platform declarations; invalid versions, duplicate routes, overlapping integrated/excluded rows, unsafe formats, and unknown runtime fields are refused.
- Package requirements are declared beside each runtime route rather than inferred from a generic emulator package. Provisioning receipts include the manifest SHA-256, pinned package versions, installed executable/core paths and hashes, and per-route package/platform readiness. Isolde registers runtime launch entries only after the receipt matches the current manifest and installed executable/core hashes. Gameplay and controller verification remain explicitly false.
- Runtime installation refuses a changed installed package version, and apt receives both `--no-upgrade` and `--no-remove`; a refreshed pin cannot silently downgrade or replace an existing runtime. Stop requests signal only the provisioner; active apt package operations finish before cancellation is honored, while downloads/index refreshes remain bounded and interruptible.
- Video and controller choices remain typed profile settings, not command strings. Dolphin's Xbox-to-Wii Classic/Nunchuk mappings are generated into its private configuration; the Classic preset is refused for other engines. Existing save files are kept outside generated configuration and survive regeneration.
- PS2 firmware remains user-provided, size-bounded, checked for expected directory markers, copied content-addressably into the active profile, and SHA-256 checked again immediately before launch. Replacing an imported image retires only stale, validated Isolde-owned firmware objects after the new settings commit; malformed or unrelated files are not deleted. An import refuses a firmware directory with more than eight entries. Firmware is never downloaded.
- The Games setup screen states the host boundary, lists integrated and explicitly non-integrated generations, and warns that runtime availability does not establish title compatibility, controller success, or performance.

## Focused evidence and residuals

`python3 tests/test_games_provisioning.py` covers the manifest boundary, explicit generation exclusions, runtime/package association, binary/core fingerprint changes, registration refusal after artifact mutation, identity-only launch arguments, typed controller configuration, private saves, and firmware hash rejection. Related available checks: `python3 tests/test_portable_game_installer.py`, `python3 tests/test_steam_games.py`, Python byte-compilation, JSON parsing, and `git diff --check`.

Native C++/QML tests could not run in this environment because CMake/Qt tooling is unavailable. VM acceptance is intentionally deferred. Actual controller gameplay, per-runtime gameplay/return, save-flush/relaunch/crash behavior, disconnected cold boot, package-update rollback, and offline dependency-bundle completeness remain unverified. Package changes are install-only and cancellation waits for the active package command, but an apt/dpkg failure can still leave newly added system packages requiring package-manager repair; transactional rollback is not implemented. No route outside the matrix is claimed.
