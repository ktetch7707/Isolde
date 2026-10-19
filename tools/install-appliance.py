#!/usr/bin/env python3
"""Non-destructive Ubuntu bootstrap and per-user release installation.

No disk formatting, autologin changes, sudoers rules or security-policy changes.
System dependencies are an explicit, separately privileged operation.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

DEPENDENCIES = [
    "build-essential", "cmake", "ninja-build", "qt6-base-dev", "qt6-multimedia-dev", "qt6-serialport-dev",
    "qt6-declarative-dev", "qt6-webengine-dev", "qt6-webchannel-dev",
    "qml6-module-qtquick", "qml6-module-qtquick-window", "qml6-module-qtquick-controls",
    "qml6-module-qtquick-layouts", "qml6-module-qtquick-templates", "qml6-module-qtqml-workerscript",
    "qml6-module-qtwebengine", "qml6-module-qtwebchannel", "qml6-module-qtquick-dialogs", "qml6-module-qtmultimedia",
    "bubblewrap", "dbus-daemon", "libglib2.0-bin", "python3",
    "network-manager", "pulseaudio-utils", "wireplumber", "pipewire-pulse", "wmctrl",
    "kodi", "gstreamer1.0-tools", "gstreamer1.0-plugins-base", "gstreamer1.0-plugins-good",
    "gstreamer1.0-plugins-bad", "gstreamer1.0-libav", "gstreamer1.0-gl",
]


def run(*args, **kwargs):
    subprocess.run(args, check=True, **kwargs)


def ubuntu_supported():
    values = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    return values.get("ID", "").strip('"') == "ubuntu" and values.get("VERSION_ID", "").strip('"') == "26.04"


def atomic_text(path, text, mode=0o600):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=".isolde-")
    try:
        with os.fdopen(fd, "w") as output:
            output.write(text)
        os.chmod(name, mode)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def set_link(path, target):
    # Unique temp name; never unlink an existing user's current/previous directory.
    with tempfile.TemporaryDirectory(dir=path.parent, prefix=".link-") as scratch:
        temporary = Path(scratch) / "link"
        temporary.symlink_to(target)
        os.replace(temporary, path)


def checked_release(prefix, name):
    link = prefix / name
    if not link.is_symlink():
        if link.exists():
            raise RuntimeError(f"Refusing to replace non-symlink: {link}")
        return None
    resolved = link.resolve(strict=True)
    if resolved.parent != (prefix / "releases").resolve() or not (resolved / "bin/isolde").is_file():
        raise RuntimeError(f"Invalid release link: {link}")
    return resolved


def install_payload(payload, prefix):
    for relative in ("bin/isolde", "bin/isolde-native-host", "bin/config/native-session-bus.conf"):
        if not (payload / relative).is_file():
            raise RuntimeError(f"Incomplete build: missing {relative}")
    files = sorted(payload.rglob("*"))
    if any(path.is_symlink() for path in files):
        raise RuntimeError("Release payload must not contain symlinks")
    digest = hashlib.sha256()
    for path in files:
        if path.is_file():
            digest.update(str(path.relative_to(payload)).encode() + b"\0")
            digest.update(path.read_bytes())
    prefix.mkdir(parents=True, exist_ok=True)
    current = checked_release(prefix, "current")
    checked_release(prefix, "previous")
    releases = prefix / "releases"
    releases.mkdir(exist_ok=True)
    release = releases / digest.hexdigest()[:24]
    if not release.exists():
        with tempfile.TemporaryDirectory(dir=releases, prefix=".staging-") as scratch:
            staged = Path(scratch) / "payload"
            shutil.copytree(payload, staged)
            staged.rename(release)
    if current != release:
        if current:
            set_link(prefix / "previous", current)
        set_link(prefix / "current", release)
    return release


def rollback(prefix):
    previous = checked_release(prefix, "previous")
    current = checked_release(prefix, "current")
    if previous is None or current is None:
        raise RuntimeError("No previous release is available. Profiles have not been changed.")
    set_link(prefix / "current", previous)
    set_link(prefix / "previous", current)


def install_startup(prefix, config):
    release = checked_release(prefix, "current")
    if release is None or not (release / "bin/isolde-recovery").is_file():
        raise RuntimeError("Install a release with the recovery executable before enabling startup")
    # systemd specifier expansion must not reinterpret user path characters.
    executable = str(prefix / "current/bin/isolde").replace("%", "%%").replace("\\", "\\\\").replace('"', '\\"')
    display_waiter = str(prefix / "support/wait-for-display.py").replace("%", "%%").replace("\\", "\\\\").replace('"', '\\"')
    unit = f'''[Unit]
Description=Isolde living-room interface
PartOf=graphical-session.target
StartLimitIntervalSec=60
StartLimitBurst=3
OnFailure=isolde-recovery.service

[Service]
Type=simple
ExecStart=/usr/bin/python3 "{display_waiter}" --executable "{executable}"
Restart=always
RestartSec=5
TimeoutStopSec=10
KillMode=control-group

[Install]
WantedBy=graphical-session.target
'''
    atomic_text(config / "systemd/user/isolde.service", unit)
    support = prefix / "support"
    for filename in ("install-appliance.py", "recovery-controller.py", "wait-for-display.py"):
        atomic_text(support / filename, Path(__file__).with_name(filename).read_text())
    def unit_quote(path):
        return '"' + str(path).replace("%", "%%").replace("\\", "\\\\").replace('"', '\\"') + '"'
    # Pin recovery to the installed release, not 'current': rollback must not
    # remove the recovery executable when the previous release predates it.
    recovery_unit = f'''[Unit]
Description=Isolde on-screen recovery
PartOf=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/bin/python3 {unit_quote(support / 'recovery-controller.py')} --prefix {unit_quote(prefix)} --ui {unit_quote(release / 'bin/isolde-recovery')}
Restart=no
TimeoutStopSec=5
KillMode=control-group
'''
    atomic_text(config / "systemd/user/isolde-recovery.service", recovery_unit)
    # Import the real graphical session, never hard-code a display or uid.
    launcher = prefix / "start-session.sh"
    atomic_text(launcher, '''#!/bin/sh
set -eu
systemctl --user stop isolde-recovery.service
systemctl --user import-environment DISPLAY WAYLAND_DISPLAY XAUTHORITY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP
systemctl --user reset-failed isolde.service
exec systemctl --user start isolde.service
''', 0o700)
    desktop_path = str(launcher).replace("\\", "\\\\").replace('"', '\\"').replace("`", "\\`").replace("$", "\\$").replace("%", "%%")
    atomic_text(config / "autostart/isolde.desktop", f'''[Desktop Entry]
Type=Application
Name=Isolde
Exec="{desktop_path}"
Terminal=false
X-GNOME-Autostart-enabled=true
''')


def enable_startup():
    # Enable for future graphical logins without interrupting a running app.
    # Keep the desktop launcher as an environment-import fallback as well.
    run("systemctl", "--user", "daemon-reload")
    run("systemctl", "--user", "enable", "isolde.service")
    run("systemctl", "--user", "is-enabled", "--quiet", "isolde.service")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["plan", "check", "dependencies", "install", "rollback"])
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--prefix", type=Path, default=Path.home() / ".local/share/isolde-appliance")
    parser.add_argument("--startup", action="store_true", help="Start at graphical login and recover from crashes")
    parser.add_argument("--build-dir", type=Path, help="Reuse a CMake build directory (default: build-appliance)")
    args = parser.parse_args()
    if args.action == "plan":
        print("Target: Ubuntu 26.04; existing desktop and user preserved.")
        print("Dependencies: " + " ".join(DEPENDENCIES))
        print(f"Build, test, stage release, atomically activate under {args.prefix}.")
        print("Optional --startup: user service + graphical autostart. No automatic login, disk erase or reboot.")
        return
    if sys.platform != "linux" or not ubuntu_supported():
        raise RuntimeError("This installer currently supports Ubuntu 26.04 only.")
    if args.action == "check":
        run("apt-get", "-s", "install", *DEPENDENCIES)
        return
    if args.action == "dependencies":
        if os.geteuid() != 0:
            raise RuntimeError("Run only the dependencies action with sudo.")
        run("apt-get", "update")
        run("apt-get", "install", "-y", *DEPENDENCIES)
        return
    if os.geteuid() == 0:
        raise RuntimeError("Build/install as the regular Isolde user, not root.")
    prefix = args.prefix.expanduser().absolute()
    import fcntl
    prefix.mkdir(parents=True, exist_ok=True)
    lock = (prefix / ".install.lock").open("a")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError("Another installation is already using this destination.")
    if args.action == "rollback":
        rollback(prefix)
        print("Previous application release selected. Restart Isolde to use it; profile data is unchanged.")
        return
    source = args.source.resolve(strict=True)
    build = args.build_dir.resolve() if args.build_dir else source / "build-appliance"
    configure = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", "-DBUILD_TESTING=ON"]
    if not args.build_dir:
        configure.append("-DCMAKE_BUILD_TYPE=Release")
    run(*configure)
    run("cmake", "--build", str(build), "--parallel", "2")
    test_env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
    run("ctest", "--test-dir", str(build), "--output-on-failure", env=test_env)
    with tempfile.TemporaryDirectory(prefix="isolde-install-") as directory:
        payload = Path(directory)
        run("cmake", "--install", str(build), "--prefix", str(payload))
        release = install_payload(payload, prefix)
    if args.startup:
        config = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config")))
        install_startup(prefix, config)
        enable_startup()
    print(f"Installed and tested: {release}. Existing running app was not interrupted.")
    print(f"Launch: {prefix / 'current/bin/isolde'}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(f"Isolde installation stopped: {error}")
