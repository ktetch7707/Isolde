# Isolde install, release, and update — Ubuntu 26.04

This guide covers the full path for putting Isolde on an Ubuntu 26.04 machine,
creating the first release, and keeping it updated. It is written for the
desktop package target (installed under `/opt/isolde`, not a disk-replacing
image). The appliance image path is described in `docs/user-setup.md`.

## 1. Install on Ubuntu 26.04

### For non-technical users (recommended)

Download the signed `.deb` from the GitHub release page
(https://github.com/ktetch7707/Isolde/releases), then install it:

```sh
sudo apt install ./isolde_0.0.1.3_amd64.deb
```

You can also double-click the `.deb` file in the file manager to open it in
Ubuntu Software Center and click Install.

After installation:

1. **Launch Isolde** from the applications menu ("Isolde") or run
   `/opt/isolde/bin/isolde --windowed`
2. **Setup wizard opens automatically** — seven steps using remote or keyboard:
   - Welcome → Network → Display → Audio → Remote → Profiles → First library → Start Isolde
3. **Optionally enable autostart** on the final setup step ("Start Isolde
   automatically at login"). Isolde will then start at every graphical login.
4. **Done** — Isolde is ready to use.

Profiles, layouts, and settings are stored in your home directory and survive
updates.

### For developers (from source)

Install build dependencies:

```sh
sudo apt update
sudo apt install -y cmake ninja-build g++ \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  qt6-multimedia-dev qt6-serialport-dev qt6-webengine-dev qt6-webchannel-dev \
  libqt6sql6-sqlite \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtqml-workerscript qml6-module-qtwebengine qml6-module-qtwebchannel \
  qml6-module-qtquick-dialogs qml6-module-qtmultimedia \
  bubblewrap dbus-daemon libglib2.0-bin python3 pkexec \
  network-manager pulseaudio-utils wireplumber pipewire-pulse wmctrl \
  kodi gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-libav gstreamer1.0-gl
```

Clone the repo, build, test, and run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/isolde --windowed
```

Isolde starts fullscreen by default; `--windowed` is for development.

---

## 2. Push a release (maintainer)

Releases are published from the `ktetch7707/Isolde` repository by pushing a
signed tag. The GitHub Actions workflow `.github/workflows/release.yml` builds,
tests, signs, and publishes automatically.

### Prerequisites for the first release

1. **Generate a signing key pair** (offline, never commit the private key):

   ```sh
   openssl genpkey -algorithm ED25519 -out isolde-signing-key.pem
   openssl pkey -in isolde-signing-key.pem -pubout -out tools/isolde-signing-key.pub
   ```

   Commit `tools/isolde-signing-key.pub` to the repository. Keep
   `isolde-signing-key.pem` offline; it is the trust anchor for all updates.

2. **Add the private key as a GitHub secret** named `ISOLDE_SIGNING_KEY` in the
   repository settings (Settings → Secrets and variables → Actions). The release
   workflow reads it to sign artifacts. Never paste it into chat, issues, or
   logs.

3. **Protect release tags** in GitHub: require approval before publishing,
   restrict who can push `v*` tags.

### Cutting a release

```sh
git checkout -b release-0.0.1.3
echo "0.0.1.3" > VERSION
git commit -am "Prepare 0.0.1.3"
git tag v0.0.1.3
git push origin v0.0.1.3
```

The release workflow:

1. Verifies Ubuntu 26.04 (self-hosted runner required until GitHub-hosted
   runners support 26.04)
2. Installs build dependencies
3. Configures, builds, runs the full CTest suite (a failed build/test blocks
   the release)
4. Builds `isolde-0.0.1.3-linux-amd64.tar.gz` (appliance payload)
5. Builds `isolde_0.0.1.3_amd64.deb` (desktop package)
6. Signs the payload digest and the `update-manifest.json` with the private key
7. Creates the GitHub release with assets:
   - `isolde-0.0.1.3-linux-amd64.tar.gz` + `.sig`
   - `isolde_0.0.1.3_amd64.deb`
   - `update-manifest.json` + `update-manifest.sig`

The manifest binds version, OS, architecture, filename, size, SHA-256, and
channel, and is signature-verified before any device installs it.

---

## 3. Update on that machine

### Desktop package update

When a new release is published, Isolde (with Web ON) checks GitHub for the
latest stable release. If a newer, signature-verified `.deb` exists for this
device's architecture, **Settings → System & Support → Software Updates**
shows "A verified update is ready to install."

Select **Install update**. Isolde:

1. Downloads `update-manifest.json` and `update-manifest.sig`
2. Verifies the manifest signature against the shipped public key
   (`share/isolde/isolde-signing-key.pub`)
3. Downloads the `.deb` and verifies its size and SHA-256 against the manifest
4. Installs the `.deb` with `dpkg --install` (requires administrator
   authorization via `pkexec`)

The running Isolde is not interrupted. Restart Isolde to use the new version.
Profiles, layouts, media library, and settings are preserved.

### Appliance (per-user) update

For per-user installs under `~/.local/share/isolde-appliance`, the same
verified flow installs the tarball as a content-addressed release and atomically
switches the `current` link. The previous release is retained as `previous`.

### Rollback

If an update causes problems, roll back to the previous release:

```sh
# Appliance install
systemctl --user stop isolde.service
python3 tools/install-appliance.py rollback
systemctl --user start isolde.service

# Or from the recovery screen: "Use previous release" → "Confirm previous release"
```

Rollback changes only the application release, not profiles, settings, or
media.

### Web OFF

With Web OFF, Isolde issues zero external requests and does not check for
updates. Turn Web ON in Settings to receive update notifications.

---

## Version note

This guide describes Isolde **0.0.1.3+** with the signed release/update path.
Check **Settings → System → About** for your exact installed version.
