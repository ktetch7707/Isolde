# T30 expanded runtime implementation — November 10, 2026

User authorization: implement RPCS3, shadPS4, Vita3K, xemu, Xenia and Cemu;
leave the testing rerun deferred. This closes the known source implementation
gap in BUG-058, **not its verification or T30 acceptance**. No tests, builds,
runtime installations, emulator executions or VM actions were run for this
batch. Existing Studio changes and evidence belong to their separate owner.

## Declared routes

Ubuntu 26.04 amd64 remains the sole managed target. The original ten routes
and their package/version pins remain. Six new routes bring the declaration to
16; the following are bounded software contracts awaiting verification.

| Route | Pinned upstream release | Accepted game input | Setup and limits |
| --- | --- | --- | --- |
| RPCS3 / PlayStation 3 | 0.0.43-20581, source fd027c99b8824e9a9b6f5b2d5ae33f9202e29da3 | Decrypted EBOOT.BIN, ELF/SELF in an approved game folder; unencrypted homebrew PKG | User imports a PS3 PUP. Headless firmware/package preparation, generated VFS and SDL Xbox input profile; Vulkan/no-GUI/fullscreen launch. No claim for encrypted packages requiring additional licenses, pressure-sensitive or motion-only title controls. |
| shadPS4 / PlayStation 4 | SDL release 0.21.0 | Decrypted eboot.bin/ELF in an approved game folder | Explicit private override root and fullscreen boolean. The runtime's SDL positional pad defaults apply. Titles requiring additional system modules, touch or gyro have no universal compatibility claim. |
| Vita3K / PS Vita | Build 4296; source 011f20b049a9c4779ce7f6c77e7161e74d634564 | Unencrypted VPK with eboot.bin and sce_sys/param.sfo | User imports the system PUP, or a ZIP with firmware.pup and optional fonts.pup. Private firmware preparation and SDL frontend install/auto-boot. Pad-only title matrix; touch/rear-touch/motion-only gameplay is not advertised. |
| xemu / Original Xbox | 0.8.139 | XISO with the Xbox volume descriptor at sector 32 | User imports a ZIP with mcpx.bin, flash.bin and hdd.qcow2, optionally eeprom.bin. Exact MCPX 1.0 identity, bounded BIOS sizes and standalone unencrypted QCOW2 are checked. The writable private disk/EEPROM are initialized once and preserved on firmware changes. |
| Xenia Canary / Xbox 360 | Official native Linux artifact 02d2cb5 | Authorized ISO or XEX | Explicit Vulkan/SDL/fullscreen and private storage/content roots. Linux/title/GPU compatibility remains experimental; no firmware, account or ownership bypass. |
| Cemu / Wii U | 2.6, restored official release artifact | Decrypted WUA or RPX in an approved game folder | Generated SDL Xbox-to-GamePad profile, private MLC and initial settings prevent the desktop first-run setup. Back also toggles GamePad view. Touch/gyro-only titles, encrypted WUD/WUX and online accounts are outside this route. |

Every new route exposes only Automatic video: a software renderer is not
invented for these backends. Existing controller/session ownership, stop,
return and profile teardown remain in NativeSandbox/NativeAppHost/SessionManager.
The expanded adapters use a network namespace without network connectivity.
These declarations are neither gameplay passes nor claims that every title
works. Other excluded generations retain their existing open requirements.

## Artifact and installation boundary

`config/game-runtime-artifacts.json` records exact release URLs, byte sizes,
expected SHA-256 values from the official GitHub release API, primary license
URLs/hashes and source/review references. License files were fetched read-only
and hashed. The shadPS4 archive's member name and Xenia archive layout were
inspected. Xenia's archive hash matched its official metadata; its inert ELF
header identifies amd64, and its DT_NEEDED entries were read without execution
to include its non-bundled dependencies. This is artifact inspection, not a
runtime smoke test. The other AppImages have not been built or installed here.

The Cemu pin is the currently restored official artifact, not either compromised
hash documented in the [upstream incident](https://github.com/cemu-project/Cemu/issues/1911).
Vita's continuous-release URL is hash-locked: a replaced upstream download is
refused, never accepted as an implicit update. Previously prepared signed
offline bundles remain usable without contacting upstream.

`build-emulator-payloads.py` runs only after the existing provisioner confirms
the Ubuntu guest target. It downloads with byte/time/cancel bounds and checks
the pinned hashes. Type-2 AppImages are extracted with unsquashfs rather than
executing their embedded launcher or requiring FUSE. Archive membership,
amd64 headers, expansion limits, special files and escaping links are checked.
Six local `isolde-*` Debian packages contain the upstream runtime tree,
copyright/source notices and a fixed namespace-only launcher. There are no
maintainer scripts in these six local packages. A writable cached package receipt alone is not
trusted; packaging is recreated from its locked upstream archive.

The existing authenticated apt dependency resolver and signed offline bundle
contain the local packages and their dependency closure. Local package control
metadata binds each package to its upstream artifact hash.
Exact signed archive paths, including absent dependencies, are supplied to apt
so a clean offline target does not need an index for the local package names.
Existing dependency versions are preserved rather than replaced implicitly.
The old version
refusal, explicit update/rollback, no-remove/no-upgrade and offline no-download
contracts remain. Historical signed bundles can contain the previous subset
of runtime packages for rollback. A damaged matching-version runtime is
eligible for an explicit pinned `--reinstall`, rather than being mistaken for
a healthy installation. Both provisioning receipts and the installed launchers
check the complete runtime file/link manifest, including supporting libraries.
Remote setup provides online install/repair and reviewed updates, plus explicit
offline install/repair/update choices that never request a download. Cancel
returns to the exact originating emulator-list action. Installation and data
rebuild refuse while an owned game session is active.

## Firmware, imports and recovery

The PS2 profile-grant repair in BUG-075 also protects these imports. Firmware
must be inside the active profile's approved media folder. C++ copies it with
bounded streaming and two matching hashes before atomically saving its
content-addressed selection. A changed or redirected source is refused;
unknown setup fields, renderer values and firmware contracts are rejected.
No startup read migrates or rewrites settings.

The native helper verifies the selected private asset again. Firmware decoders
run inside the existing game namespace, with an isolated temporary HOME and
bounded output/time. Only a complete prepared tree with the expected system
modules is published under its source hash. Its receipt and all files are
verified before reuse. RPCS3 VFS points at immutable prepared firmware; Vita's
two firmware links are constrained to those prepared profile-owned trees.
Mutable PS3/Vita game data, Cemu MLC and Xenia content stay outside firmware
staging. PS3 PKG preparation is private and cached by the package's content
hash; it must produce exactly one playable EBOOT.BIN before launch.

ZIP validation refuses traversal, encrypted entries, case-colliding names,
file/directory collisions, special files and oversized expansions. Xbox QCOW2
validation rejects backing files, encryption and external-data features.
Its private disk and EEPROM are never replaced by a subsequent boot bundle.
Source game, firmware and save bytes are not deleted during preparation.

Remote setup retains Preview/Cancel behavior and adds **Rebuild imported data**
with Cancel selected by default. Confirm retains the old prepared directory
under a unique recovery name and recreates prepared data on next Play. It
does not remove original imports, private saves, settings or the other
profile's data. A shared nonblocking preparation lock prevents a simultaneous
preparation/repair. Redirected roots and excessive retained copies are refused.

## Deferred verification

Only Python AST parsing, JSON parsing, diff review and upstream artifact/source
inspection were performed. No product Python module or test suite was executed.
Prepared coverage extends the existing games provisioning, native component
and Games remote suites: locked package metadata, complete backend fingerprint,
matching-version repair, actual generated command/config files, private
firmware decoder boundaries, cache reuse/tamper, unsafe VPK/ZIP, Xbox save/disk
preservation, renderer refusal and remote cancel/rebuild origin.

The firmware/runtime decoders in those prepared fixtures are external-boundary
substitutions. They do not prove firmware decryption or gameplay. The later
authorized campaign must build/install the full signed bundle, exercise native
and QML tests, inspect actual runtime output and safe stop/return, verify cold
offline/profile/error/update recovery, and retain the common lawful VM gameplay
positive fixture. Per-runtime GPU/firmware/title/controller assertions use the
VM substitutions and honest user handoff in vm-software-completion.md.

T30 acceptance remains 0%; project acceptance remains 1563/25 = 63%. The
pre-campaign Coding% headline is not new verification. No acceptance milestone
is awarded from this implementation or its unrun regression code.
