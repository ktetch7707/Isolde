# Isolde install path and user instructions

For the existing Oracle VirtualBox test machine, follow the [VM oracle runbook](vm-oracle.md) for isolation, release selection and restoration.

Isolde should eventually be installable by a normal user without touching a Linux desktop after the initial OS image or installer boots.

## Dedicated-machine requirement and current installer foundation

The consumer target is a computer dedicated to Isolde, not a general desktop with an extra app. Users should eventually download one image, explicitly choose the target disk, install, and complete the TV setup guide. No Linux knowledge should be required. Disk replacement must always be explicit; the current tooling never formats disks.

Implemented foundation (0.0.1.2): `tools/install-appliance.py`, currently targeting **Ubuntu 26.04 only**. This is developer/pilot installation tooling, not a finished consumer installer or signed distribution.

Startup hardening: the enabled user service runs `wait-for-display.py` before launching Isolde. It waits up to 30 seconds for a local Wayland/X11 socket, refreshes only the allowlisted graphical environment from the user service manager, then replaces itself with Isolde. This avoids an observed early-login crash caused by launching Qt before Wayland exists. The wait is bounded and retains the service's crash-limit/recovery behavior. The installer does not enable OS automatic login; the test VM already has that separately configured.

October 15 acceptance: the full installer with `--startup` passed all eight suites. The first cold boot exposed the early-display race; after adding the readiness gate, a second cold boot started Isolde automatically with `NRestarts=0` and Household Home visually confirmed, without any manual service-start command. This verifies the existing Ubuntu/GNOME test VM, not blank-machine provisioning or every compositor.

From a downloaded source tree on Ubuntu:

```sh
python3 tools/install-appliance.py plan
python3 tools/install-appliance.py check
sudo python3 tools/install-appliance.py dependencies
python3 tools/install-appliance.py install --startup
```

Only the dependency action needs administrator privileges. The installation action configures CMake, builds, runs all tests, stages a complete payload, and then atomically selects a content-addressed application release. A failed build or test does not activate a release. It preserves existing Isolde profiles/settings and does not stop a running instance. Internet/package repositories are needed for missing dependencies; existing local capabilities do not require internet afterward.

Default location: `~/.local/share/isolde-appliance`. The executable is `current/bin/isolde`. Optional `--startup` installs a regular-user systemd service and desktop autostart entry. It starts at **graphical login**, not before login, and restarts a failed/exited interface after five seconds. Three starts within a minute is the limit, preventing an endless rapid crash loop. Repeated failure opens the independent **Isolde Recovery** screen: Retry clears the restart limit and starts Isolde; Use previous release requires a second confirmation before switching and starting it. Missing/invalid previous releases are disabled. The service stops its entire process group on shutdown. Logs: `journalctl --user -u isolde.service` and `journalctl --user -u isolde-recovery.service`.

Recovery uses a separate Qt Quick executable without loading Isolde's profiles, app catalog, sessions, or Chromium. Its executable is pinned to the release installed with startup configuration, so rolling back to an older app without recovery support does not remove the recovery UI. Actions share the installer lock and reject release switching while Isolde is running. Recovery itself does not auto-restart indefinitely. This covers application failures, not compositor/OS crashes, missing shared Qt libraries, a broken recovery executable, or storage failure. Those still require an OS recovery path. It is not a factory reset or data-migration rollback.

For the already-installed VM, use `systemctl --user restart isolde.service` rather than launching a second independent copy. To pause supervised testing: `systemctl --user stop isolde.service`. The graphical session supplies display variables; no fixed display number or user id is encoded in the installer.

Offline application rollback (after at least two different deployments):

```sh
systemctl --user stop isolde.service
python3 tools/install-appliance.py rollback
systemctl --user start isolde.service
```

Rollback changes the application release only, not profiles, package dependencies, firmware, or the OS. Future incompatible data migrations require backups and migration-aware rollback before production updates. Old releases are retained; automatic disk-space retention is not implemented. The installer is not a security boundary against the same OS user modifying their files.

First run now opens a seven-step remote/keyboard guide: welcome, network, TV safe area/scale, audio, remote, profiles, and finish. Continue works offline. Progress is saved atomically in `device-setup.json`, outside individual profile folders. Failed saves remain visible and do not advance the guide. Existing settings are not reset. Repeat it with **Settings → System → Setup guide** or `isolde --setup`.

Not yet implemented/validated: blank-machine provisioning, dedicated compositor session, automatic OS login, signed package/image delivery, driver/hardware qualification, consumer USB creation, OS update rollback, region/privacy onboarding, and fresh-install/reboot acceptance. The existing VM desktop and login policy are deliberately unchanged. GDM automatic login requires an explicit appliance provisioning step following the [GNOME administration guidance](https://help.gnome.org/system-admin-guide/login-automatic.html); a dedicated multi-app session can be evaluated using [labwc](https://labwc.github.io/getting-started.html).

Recovery verification: five automated suites pass. In the running VM, repeated deliberate SIGKILL failures reached on-screen recovery; keyboard Retry returned to Isolde; a second crash loop exercised the confirmation and actual previous-release switch, returning to Home. The current release was restored afterward. This does not validate reboot recovery or a fresh OS install. Ubuntu desktop notifications remain possible until the dedicated session is provisioned.

Installer regression tests cover repeat deployment, previous-release selection, incomplete-payload rejection, unsafe release-link rejection, preservation of existing directories, and bounded startup configuration. `install --startup` explicitly runs user-service daemon-reload, enables `isolde.service`, and verifies it with `is-enabled --quiet`; an enablement or verification failure fails the install command instead of reporting success. Enablement does not start or interrupt the current interface. Regression tests assert this command sequence and failure propagation. The desktop autostart launcher is retained for graphical-environment import. Installing without `--startup` remains an explicit developer/non-autostart option and does not disable an already enabled service. These are staging tests, not a substitute for a clean-machine install test.

## Target install experiences

The professional install path should have three tiers:

1. **Appliance image**: a bootable USB/image for a small PC, mini PC, or dedicated box. This is the preferred consumer path.
2. **Linux package**: a signed `.deb`/repository install for Ubuntu/Debian users who already manage a Linux host.
3. **Developer/VM setup**: a reproducible path for testing, screenshots, and safe experiments without endangering the host computer.

## First-run onboarding

After install, Isolde should launch into a TV-first setup wizard:

1. Choose language and region.
2. Configure display safe area and UI scale.
3. Configure audio: set TV reference level, choose volume owner, test volume/mute.
4. Pair or test a remote in the first-class Remotes setup flow.
5. Test HDMI-CEC TV power/input behavior when supported by the hardware.
6. Connect to network.
7. Choose privacy/update defaults.
8. Add first apps/services such as YouTube, Spotify, Kodi, Plex, Home Assistant, Steam, or local folders.
9. Confirm Home layout and finish.

The wizard must remain usable with only a keyboard during development and with only a remote in production.

## Current developer setup

For source builds, use:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/isolde
```

Expected Linux dependencies include CMake, Ninja, a C++17 compiler, and Qt 6 modules for Core, Gui, Qml, Quick, Test, WebEngineQuick, and WebChannel.

Native profile separation additionally requires `bubblewrap`, `dbus-run-session` (typically supplied by `dbus-daemon`), working unprivileged namespaces, and the built `isolde-native-host` executable alongside Isolde. Install `config/native-session-bus.conf` in the adjacent `config` folder; CMake installs both automatically. The native integration tests also use `gdbus` (typically `libglib2.0-bin`). If the host cannot create the sandbox, native apps are refused rather than launched with shared credentials. No host AppArmor or global user-namespace policy changes are needed on the current test VM.

## Packaging roadmap

Before any public-friendly installer, Isolde needs:

- a reproducible release build;
- signed packages or signed appliance images;
- autostart/kiosk session integration;
- a rollback/recovery path;
- dependency checks with friendly errors;
- a first-run wizard;
- safe update/restart separation;
- logs and diagnostics available from Settings;
- documentation for TV setup: set TV to Isolde input on power-up, set TV volume to reference level, and let Isolde own normal navigation and volume.
# Managed media engines

October 15 master-plan foundation: the explicit Ubuntu dependency step now includes Kodi and GStreamer tools, base/good/bad plugins, libav and GL support. It does not depend on capture hardware being attached. Existing per-user build/install still does not elevate itself or silently install system packages. The online apt bootstrap is **not** the planned offline installation/recovery image; tested package locks, integrity manifests and offline media are still required.

Settings → System → Components runs an offline read-only check of installed Kodi package version, GStreamer version and selected capture/audio pipeline elements, plus Linux video-device names. It does not launch Kodi, open a capture device, send network requests, repair packages or certify signal/latency. `tools/component-health.py` provides the same JSON diagnostics. The VM reports Kodi 21.3 and GStreamer 1.28.2 with the currently checked elements; no video device is present. Some additional dependency groups are not installed in the VM yet; the check reports runtime elements rather than claiming the full future pipeline is qualified.

Settings → System → HDMI Inputs configures the USB capture card, optional HDMI audio input, serial switch port, baud rate, eight friendly input labels, and the switch's model-specific command template. Full connection instructions and limitations are in [HDMI capture setup](capture.md). A successful serial write or incoming video frames do not prove that the expected source was selected; verify the picture on the intended card/switch hardware.

