# Portable game installer support boundary

The Games page supports one deliberately narrow independent local-install format: an inert ZIP bundle containing `isolde-game.json` with `schemaVersion`, `title`, `platform` and a relative `entrypoint`. The entry point must be a Linux ELF file. Preflight inspects metadata and the ELF header without executing archive contents. Install requires a separate confirmation showing the selected installed file runtime and new profile-private destination.

The helper enforces a 512 MiB archive limit, 10,000-member limit and 2 GiB total expanded-size limit; rejects encrypted entries, duplicate/non-canonical paths, symlinks, unsupported special files and unsafe entry points; writes only into a new staging directory; and atomically publishes the completed package. Existing installations and saves are not overwritten or removed. Cancellation terminates the bounded helper and removes an uncommitted staging directory where possible. Failed catalog registration preserves the installed private folder for recovery rather than silently deleting it.

Play resolves the registered relative entry point through the selected installed native file runtime. The package tree is mounted read-only in the game session; profile storage remains isolated. The title still runs as user-supplied executable code after the explicit install and Play actions, so the user must trust its source.

Not supported by this installer: arbitrary `.exe`/Windows installers, scripts, self-extracting installers, network installers, compatibility-runtime setup, dependency resolution, DRM/activation bypass, disk imaging, or automatic installer execution. A general third-party installer flow and original-disc install/play lifecycle remain T30.7/T30.8 coding gaps. No universal compatibility, gameplay, remote-only third-party setup, native build or physical-disc qualification is claimed here.

Focused executable validation: `python3 tests/test_portable_game_installer.py`, plus the existing Games provisioning and Steam adapter tests. Native C++/QML integration still requires the repository's Qt/CMake test environment.
