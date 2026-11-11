# T30 emulator integration contract — 2026-11-09

November 10 source continuation: [six expanded runtime implementations](expanded-runtimes-2026-11-10.md)
extend the declared matrix to sixteen routes. Their tests, actual provisioning
and installed/runtime evidence are explicitly deferred. This is not acceptance.

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
| RetroArch / Beetle PSX | PlayStation | CHD only | RetroArch + Beetle PSX |
| RetroArch / DeSmuME | Nintendo DS (not DSi) | NDS | RetroArch + DeSmuME |
| RetroArch / Beetle VB | Virtual Boy | VB, VBOY | RetroArch + Beetle VB |
| RPCS3 | PlayStation 3 | ELF, SELF, BIN; unencrypted homebrew PKG | `isolde-rpcs3` |
| shadPS4 SDL | PlayStation 4 | Decrypted BIN, ELF | `isolde-shadps4` |
| Vita3K SDL | PS Vita | Unencrypted VPK | `isolde-vita3k` |
| xemu | Original Xbox | XISO | `isolde-xemu` |
| Xenia Canary native Linux | Xbox 360 | ISO, XEX | `isolde-xenia` |
| Cemu | Wii U | Decrypted WUA, RPX | `isolde-cemu` |

The managed provisioning target is Ubuntu 26.04 amd64 only. PS5, N64, DSi/3DS, Switch generations and Xbox One/Series remain explicitly not integrated. The six new rows have source implementation only; neither this declaration nor a generic frontend establishes title compatibility or a verification pass. See the November 10 record for exact artifact pins, setup/input/storage boundaries and pending checks.

## PlayStation CHD route

The same authenticated release index identifies `libretro-beetle-psx` version `0.9.38.6+git20260227-1` and SHA-256 `f6bfa15de69ae45c50210ddf7bb82b749fd2a0dd00866282c023fa3e87df1df1`. The downloaded deb matched and contains `/usr/lib/x86_64-linux-gnu/libretro/mednafen_psx_libretro.so`, with package copyright records under `/usr/share/doc/libretro-beetle-psx/copyright`. Its authenticated source archive SHA-256 is `69d2104c7f4697edf4e11c67603d50258fa6f0cfc2ffa8d5bb8c076aa19c67fb`; the shipped core supports CHD and selects a regional BIOS based on the disc. The existing bundle builder resolves and authenticates dependencies including `libchdr0` on the target, but a new offline bundle has not been built or installed.

The remote setup permits one 512 KiB user-supplied BIOS at a time: SCPH-5500 (Japan), 5501 (North America) or 5502 (Europe). Firmware must be in a profile-approved media folder. Save explicitly copies it privately; known SHA-1 values from the pinned core identify the supported region, and a content-addressed SHA-256 guards the stored copy. Both Games launch preflight and the session adapter recheck it; missing, changed, redirected or unsupported firmware is refused. The session generates only the selected region's expected filename in a private directory and refuses unexpected files there. Older generated private firmware copies can remain; saves are never deleted. Hash matching identifies bytes, not ownership or licensing. Isolde supplies neither games nor BIOS.

Register or import a user-owned single `.chd` image through Games; the existing source-root grant and sandbox mount route it read-only to the selected core. Catalog disc identity is the approved locator, **not** a content hash: replacing the CHD in place is not detected. The UI cannot determine the image's region before launch; select matching firmware, or the core will report missing regional firmware. Profile-private RetroArch save and state directories and standard SDL Xbox mappings are used. CUE and other multi-file images, direct physical PlayStation discs, playlists, live disc swaps, CHD conversion and universal game compatibility are not supported. Changing regions requires another firmware import. Actual gameplay, region mismatch presentation, save flush, input and offline bundle installation are not yet verified.

## Verified DS and Virtual Boy package selection

On 2026-11-09, the [Ubuntu resolute release InRelease](https://archive.ubuntu.com/ubuntu/dists/resolute/InRelease) was verified with the locally installed Ubuntu archive keyring: `gpgv` reported a good signature by Ubuntu Archive Automatic Signing Key (2018), fingerprint `F6ECB3762474EDA9D21B7022871920D1991BC93C`. Its signed SHA-256 for [universe amd64 Packages.xz](https://archive.ubuntu.com/ubuntu/dists/resolute/universe/binary-amd64/Packages.xz) is `1587be86d66d38542325215e0e109f75bd695c8f24d793ace2762038a6ad581e`; the downloaded index matched. The signed release source index also matched SHA-256 `9c732321580ae2824f7d8cb936e12cca435a60a252d0654b8cc2a0011dc82903`. This is release-pocket evidence, not a claim about all updates or every package in the archive.

The [resolute-updates InRelease](https://archive.ubuntu.com/ubuntu/dists/resolute-updates/InRelease) dated 2026-11-09 also verified with that key; its universe amd64 index matched signed SHA-256 `22492d94a328f31ab0b9e7129d58aa780dd3b7963cb539c59067ac67a0e368f7` and contained no `libretro-*` packages. Other pockets and separately sourced runtimes were not surveyed.

The index pins `libretro-desmume` `0.9.11+git20160819+dfsg1-2build2` (deb SHA-256 `7f239fdeacdd991fee8871541f5bb6fc0504dc6e67e2593a280710e57bbf9d18`) and `libretro-beetle-vb` `0.9.36.1+git20160623-2build2` (deb SHA-256 `b31190250cbd86117372b52c691ab5a8cebae3586f54881e9ea67492ae02fa47`). Downloaded debs matched those hashes; their file lists establish the exact core paths in the manifest and their copyright files identify GPL-2.0-or-later licensing. The normal provisioner resolves the complete dependency closure using authenticated target indexes, binds it into the signed offline bundle, and refuses a missing or changed pin; this research did not install the packages or construct a bundle.

The packages include components under additional terms; distributors must review the full `/usr/share/doc/libretro-desmume/copyright` and `/usr/share/doc/libretro-beetle-vb/copyright`, rather than treating the primary GPL label as exhaustive.

Source archives authenticated through the release source index were inspected for the exact core version. DeSmuME creates dummy firmware, accepts NDS, and maps the right analog stick to the stylus when `desmume_pointer_device = "r-stick"`; R2 presses it, L3 enables internal microphone noise and L2 closes the lid. Isolde writes these options and top/bottom screen layout to profile-private generated configuration, disabling automatic game overrides/remaps for this route. Some titles require unsupported microphone behavior or precise touch interactions. Beetle VB accepts VB/VBOY; its second D-pad uses L2 (up), R2 (left), L3 (down) and R3 (right). Those mappings are disclosed in remote setup, but have not been tested with a controller or gameplay. Both routes use the existing private save/session boundaries; neither route claims full title compatibility, physical input or VM acceptance. Games are user-supplied.

## Production contracts added

- The runtime manifest is versioned and validated in both the trusted emulator adapter and C++ registry. The generation ledger must exactly match the manifest's route platform declarations; invalid versions, duplicate routes, overlapping integrated/excluded rows, unsafe formats, and unknown runtime fields are refused.
- Engine declarations are now cross-checked against their pinned package roles: direct engines accept only their own runtime package, and RetroArch routes require the frontend, exactly one `libretro-*` package, and a core path. Provisioning also requires the manifest package set to exactly match its package/version pin allowlist. A malformed, incomplete, or unpinned route is rejected by the runtime validator before it can appear installable or launch.
- Package requirements are declared beside each runtime route rather than inferred from a generic emulator package. Provisioning receipts include the manifest SHA-256, pinned package versions, installed executable/core paths and hashes, and per-route package/platform readiness. Isolde registers runtime launch entries only after the receipt matches the current manifest and installed executable/core hashes. Gameplay and controller verification remain explicitly false.
- Runtime installation refuses a changed installed package version, and apt receives both `--no-upgrade` and `--no-remove`; a refreshed pin cannot silently downgrade or replace an existing runtime. Stop requests signal only the provisioner; active apt package operations finish before cancellation is honored, while downloads/index refreshes remain bounded and interruptible. `--prepare-bundle` stages the complete pinned amd64 runtime dependency closure in a bounded cache, verifies Ubuntu package SHA256 values against apt's authenticated signed indexes, then signs the manifest with an Ed25519 key held outside the bundle. CMake can include a prepared bundle and its trusted public key in the appliance artifact with `-DISOLDE_GAME_RUNTIME_BUNDLE=/absolute/path/to/bundle -DISOLDE_GAME_RUNTIME_BUNDLE_PUBLIC_KEY=/absolute/path/to/bundle.pub`; the key must be provisioned through the trusted artifact build, not copied from the bundle. `--install --offline` never refreshes package indexes or downloads packages: it verifies the signature and archive hashes locally and needs no cached apt indexes on the target. PPSSPP is the sole locally built payload, bound to the pinned upstream commit, and protected by the signed bundle manifest.
- A package transaction writes a durable recovery checkpoint before apt/dpkg changes. On command or post-install verification failure it simulates the purge plan and proceeds only when the removal set exactly matches packages absent before the transaction; interrupted transactions use the same check before recovery. Install operations use apt's `--no-remove` safeguard. `--install --update` enables exact pinned-version changes only after the solver plan proves that no undeclared package or dependency changes; it requires signed prior archives for every replaced package, writes a signed rollback snapshot, and automatically restores that snapshot on failure or interrupted-transaction recovery. Online updates can retrieve old Ubuntu packages only when the authenticated apt index still publishes them; offline updates require a matching signed older bundle in the cache. If either the old archive or a no-extra-changes solver plan is unavailable, the update is refused before dpkg changes. If exact package restoration fails, the checkpoint remains and provisioning refuses to proceed.
- Video and controller choices remain typed profile settings, not command strings. Dolphin's Xbox-to-Wii Classic/Nunchuk mappings are generated into its private configuration; the Classic preset is refused for other engines. Existing save files are kept outside generated configuration and survive regeneration.
- PS2 firmware remains user-provided, size-bounded, checked for expected directory markers, copied content-addressably into the active profile, and SHA-256 checked again immediately before launch. Replacing an imported image retires only stale, validated Isolde-owned firmware objects after the new settings commit; malformed or unrelated files are not deleted. An import refuses a firmware directory with more than eight entries. Firmware is never downloaded.
- The Games setup screen states the host boundary, lists integrated and explicitly non-integrated generations, and warns that runtime availability does not establish title compatibility, controller success, or performance.

## Focused evidence and residuals

`python3 tests/test_games_provisioning.py` covers the manifest boundary, explicit generation exclusions, runtime/package association, binary/core fingerprint changes, registration refusal after artifact mutation, identity-only launch arguments, typed controller configuration, private saves, firmware hash rejection, offline bundle integrity/authentication, and install/update rollback/recovery. Related available checks: `python3 tests/test_portable_game_installer.py`, `python3 tests/test_steam_games.py`, Python byte-compilation, JSON parsing, and `git diff --check`.

Native C++/QML tests could not run in this environment because CMake/Qt tooling is unavailable. VM acceptance is intentionally deferred. Actual controller gameplay, per-runtime gameplay/return, save-flush/relaunch/crash behavior, disconnected cold boot, and end-to-end package-update rollback remain unverified. The bounded dependency closure and signature paths have fixture coverage, but still need an end-to-end Ubuntu 26.04 bundle build/install/update and a disconnected appliance test. No route outside the matrix is claimed.

The requested additional Nintendo 64 route was not added: the authenticated Ubuntu 26.04 release and updates universe amd64 indexes inspected above contain no N64 libretro core. Other pockets and separately sourced runtimes were not established by this check. No package name, version, path, firmware, or compatibility claim has been inferred. N64 remains explicitly not integrated pending a verified runtime and its input/save/session implementation.
