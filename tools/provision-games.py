#!/usr/bin/env python3
"""T30 trusted Ubuntu runtime provisioner. Never installs on a Windows host.

No ROMs, firmware, accounts, game libraries or layouts are inspected/modified.
The native SuperTux package is an optional, freely distributed gameplay fixture.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

PACKAGES = {
    "dolphin-emu": ("2512+dfsg-3", "/usr/games/dolphin-emu"),
    "pcsx2": ("2.6.3+dfsg-1build1", "/usr/games/pcsx2-qt"),
    "supertux": ("0.6.3-5", "/usr/games/supertux2"),
}


def run(*args, **options):
    return subprocess.run(args, check=True, timeout=options.pop("timeout", 60), **options)


def platform():
    if os.name != "posix" or not Path("/etc/os-release").is_file():
        raise RuntimeError("Games provisioning requires the Ubuntu guest/appliance")
    values = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    if values.get("ID", "").strip('"') != "ubuntu" or values.get("VERSION_ID", "").strip('"') != "26.04":
        raise RuntimeError("This runtime manifest supports Ubuntu 26.04 only")
    if run("dpkg", "--print-architecture", capture_output=True, text=True).stdout.strip() != "amd64":
        raise RuntimeError("This runtime manifest supports amd64 only")


def atomic_bytes(path, data):
    path = Path(path).absolute()
    if path.is_symlink() or path.parent.resolve() != path.parent:
        raise ValueError("Refusing redirected receipt destination")
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".games-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data); output.flush(); os.fsync(output.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_DIRECTORY)
        try: os.fsync(directory)
        finally: os.close(directory)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)


def atomic_receipt(path, document):
    atomic_bytes(path, json.dumps(document, indent=2).encode())


def register_launchers(receipt, directory=None):
    directory = Path(directory) if directory else Path.home()/".local/share/applications"
    definitions = {
        "dolphin-emu": ("dolphin", "Dolphin", []),
        "pcsx2": ("pcsx2", "PCSX2", ["-bigpicture"]),
        "supertux": ("supertux", "SuperTux", ["--fullscreen", "--renderer", "sdl"]),
    }
    written = []
    for package, state in receipt["packages"].items():
        if not state["ready"]: continue
        app_id, name, arguments = definitions[package]
        executable = state["executable"]
        if executable not in (PACKAGES[package][1], PACKAGES[package][1].replace("/usr/games/", "/usr/bin/")):
            raise ValueError("Runtime executable does not match the verified manifest")
        entry = "[Desktop Entry]\nType=Application\nName="+name+"\nExec="+" ".join([executable]+arguments)+"\nTerminal=false\nNoDisplay=true\n"
        path = directory/("isolde-"+app_id+".desktop")
        atomic_bytes(path, entry.encode()); written.append(str(path))
    return written


def inspect(packages):
    result = {}
    for name in packages:
        expected, executable = PACKAGES[name]
        query = subprocess.run(["dpkg-query", "-W", "-f=${Status}\n${Version}", name], capture_output=True, text=True, timeout=10)
        lines = query.stdout.splitlines()
        installed = query.returncode == 0 and len(lines) == 2 and lines[0] == "install ok installed"
        # Use the package's file list, never an untrusted PATH wrapper.
        candidates = []
        if installed:
            files = run("dpkg-query", "-L", name, capture_output=True, text=True).stdout.splitlines()
            candidates = [Path(p) for p in files if p in (executable, executable.replace("/usr/games/", "/usr/bin/"))]
        binary = next((p for p in candidates if p.is_file() and not p.is_symlink()), None)
        result[name] = {"expectedVersion": expected, "installedVersion": lines[1] if installed else None,
            "ready": installed and lines[1] == expected and binary is not None,
            "executable": str(binary) if binary else None,
            "sha256": hashlib.file_digest(binary.open("rb"), "sha256").hexdigest() if binary else None,
            "copyright": str(Path("/usr/share/doc") / name / "copyright") if installed else None}
    return result


def provision(packages, install=False):
    platform()
    if not packages or len(set(packages)) != len(packages) or any(p not in PACKAGES for p in packages):
        raise ValueError("Choose declared runtime packages only")
    before = inspect(packages)
    missing = [p for p in packages if not before[p]["ready"]]
    if install and missing:
        if shutil.disk_usage(Path.home()).free < 2 * 1024**3:
            raise RuntimeError("At least 2 GiB guest space required before runtime installation")
        # Signed Ubuntu repository metadata supplies dependency and payload trust.
        # Refuse silent downgrades or removal; a changed pin needs a new manifest.
        run("sudo", "-n", "/usr/bin/apt-get", "update", timeout=180)
        run("sudo", "-n", "/usr/bin/apt-get", "install", "--no-remove", "--no-install-recommends", "-y",
            *[p + "=" + PACKAGES[p][0] for p in missing], timeout=900)
    after = inspect(packages)
    result = {"schemaVersion": 1, "packages": after, "before": before,
        "ready": all(value["ready"] for value in after.values()), "installationRequested": install,
        "gameplayVerified": False, "controllerVerified": False, "firmwareProvided": False}
    if install and not result["ready"]:
        raise RuntimeError("Runtime verification failed; preserve package state and inspect the named package")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--fixture", action="store_true")
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    packages = ["dolphin-emu", "pcsx2"] + (["supertux"] if args.fixture else [])
    result = provision(packages, args.install)
    if result["ready"] and args.install: result["launchers"] = register_launchers(result)
    atomic_receipt(args.receipt, result)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
