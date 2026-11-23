#!/usr/bin/env python3
"""Build an experimental Ubuntu desktop package from an already tested build."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent.parent
PREFIX = "/opt/isolde"
RUNTIME_DEPENDENCIES = (
    "bubblewrap", "dbus-daemon", "libglib2.0-bin", "libqt6sql6-sqlite",
    "python3", "pkexec", "network-manager", "pulseaudio-utils", "wireplumber",
    "pipewire-pulse", "wmctrl", "kodi", "gstreamer1.0-tools",
    "gstreamer1.0-plugins-base", "gstreamer1.0-plugins-good",
    "gstreamer1.0-plugins-bad", "gstreamer1.0-libav", "gstreamer1.0-gl",
    "qml6-module-qtquick", "qml6-module-qtquick-window",
    "qml6-module-qtquick-controls", "qml6-module-qtquick-layouts",
    "qml6-module-qtquick-templates", "qml6-module-qtqml-workerscript",
    "qml6-module-qtwebengine", "qml6-module-qtwebchannel",
    "qml6-module-qtquick-dialogs", "qml6-module-qtmultimedia",
)


def check_host():
    """Reject unsupported builders so dependencies match the target release."""
    values = {}
    for line in Path("/etc/os-release").read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value.strip('"')
    if values.get("ID") != "ubuntu" or values.get("VERSION_ID") != "26.04":
        raise RuntimeError("Build this package on Ubuntu 26.04.")
    architecture = subprocess.check_output(
        ["dpkg", "--print-architecture"], text=True
    ).strip()
    if architecture not in ("amd64", "arm64"):
        raise RuntimeError(f"Unsupported package architecture: {architecture}")
    return architecture


def check_build(build_dir):
    """Require the configured build and package metadata to use the same version."""
    version = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", version):
        raise RuntimeError("VERSION must contain four numeric components.")
    cache = (build_dir / "CMakeCache.txt").read_text()
    match = re.search(r"^CMAKE_PROJECT_VERSION:STATIC=(.+)$", cache, re.MULTILINE)
    if not match or match.group(1).strip() != version:
        raise RuntimeError("The configured build version does not match VERSION.")
    source = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache, re.MULTILINE)
    if not source or Path(source.group(1).strip()).resolve() != ROOT:
        raise RuntimeError("The configured build does not belong to this source tree.")
    return version


def build_package(build_dir, output_dir):
    """Stage a build privately and derive binary dependencies on the target OS."""
    architecture = check_host()
    build_dir = build_dir.resolve()
    output_dir = output_dir.resolve()
    version = check_build(build_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"isolde_{version}_{architecture}.deb"
    if output.exists():
        raise RuntimeError(f"Refusing to overwrite {output}")

    with tempfile.TemporaryDirectory(prefix="isolde-deb-") as directory:
        work = Path(directory)
        debian = work / "debian"
        debian.mkdir()
        stage = debian / "isolde"
        stage.mkdir()
        stage.chmod(0o755)
        env = os.environ.copy()
        env["DESTDIR"] = str(stage)
        subprocess.run(
            ["cmake", "--install", str(build_dir), "--prefix", PREFIX],
            env=env, check=True,
        )
        for path in stage.rglob("*"):
            if path.is_symlink():
                raise RuntimeError(f"Installed payload contains a symlink: {path}")
            if path.is_dir():
                path.chmod(0o755)

        payload = stage / PREFIX.lstrip("/")
        binaries = [payload / "bin" / name for name in
                    ("isolde", "isolde-native-host", "isolde-recovery")]
        required = binaries + [payload / "bin/config/native-session-bus.conf"]
        missing = [str(path.relative_to(stage)) for path in required if not path.is_file()]
        if missing:
            raise RuntimeError("Incomplete installed build: " + ", ".join(missing))

        desktop = stage / "usr/share/applications/org.isolde.Isolde.Desktop.desktop"
        desktop.parent.mkdir(parents=True)
        for directory in (stage / "usr", stage / "usr/share", desktop.parent):
            directory.chmod(0o755)
        desktop.write_text(
            "[Desktop Entry]\nType=Application\nName=Isolde (Desktop)\n"
            "Comment=Media and home interface\n"
            "Exec=/opt/isolde/bin/isolde --windowed\n"
            "TryExec=/opt/isolde/bin/isolde\n"
            "Terminal=false\nCategories=AudioVideo;Player;\n"
        )
        desktop.chmod(0o644)
        (debian / "control").write_text(
            "Source: isolde\nSection: misc\nPriority: optional\n"
            "Maintainer: Isolde contributors\nStandards-Version: 4.6.2\n\n"
            "Package: isolde\nArchitecture: any\nDescription: Isolde media interface\n"
        )
        control = stage / "DEBIAN"
        control.mkdir()
        control.chmod(0o755)
        result = subprocess.run(
            ["dpkg-shlibdeps", "-O", *(str(path.relative_to(work)) for path in binaries)],
            cwd=work, text=True, stdout=subprocess.PIPE, check=True,
        )
        match = re.search(r"^shlibs:Depends=(.+)$", result.stdout, re.MULTILINE)
        if not match:
            raise RuntimeError("Could not determine shared-library dependencies.")
        dependencies = ", ".join((match.group(1), *RUNTIME_DEPENDENCIES))
        (control / "control").write_text(
            f"Package: isolde\nVersion: {version}\nArchitecture: {architecture}\n"
            "Maintainer: Isolde contributors\nSection: misc\nPriority: optional\n"
            f"Depends: {dependencies}\n"
            "Description: Isolde media and home interface\n"
            " A windowed desktop entry for the Isolde media interface.\n"
        )
        (control / "control").chmod(0o644)
        postinst = ROOT / "debian" / "postinst"
        if postinst.is_file():
            (control / "postinst").write_bytes(postinst.read_bytes())
            (control / "postinst").chmod(0o755)
        temporary_package = work / "isolde.deb"
        subprocess.run(
            ["dpkg-deb", "--root-owner-group", "--build", str(stage), str(temporary_package)],
            check=True,
        )
        with tempfile.NamedTemporaryFile(dir=output_dir, prefix=".isolde-deb-") as target:
            with temporary_package.open("rb") as source:
                shutil.copyfileobj(source, target)
            target.flush()
            os.fchmod(target.fileno(), 0o644)
            os.fsync(target.fileno())
            os.link(target.name, output)
    return output


def main():
    """Expose packaging separately from installation and system modification."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(build_package(args.build_dir, args.output_dir))
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Package build failed: {error}\n")


if __name__ == "__main__":
    main()
