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
import importlib.util

PACKAGES = {
    "isolde-ppsspp": ("1.20.4-1", "/usr/bin/PPSSPPSDL"),
    "dolphin-emu": ("2512+dfsg-3", "/usr/games/dolphin-emu"),
    "pcsx2": ("2.6.3+dfsg-1build1", "/usr/games/pcsx2-qt"),
    "supertux": ("0.6.3-5", "/usr/games/supertux2"),
    "retroarch": ("1.22.2+dfsg-2ubuntu1", "/usr/bin/retroarch"),
    "libretro-nestopia": ("1.53.2+20260228.gitc0ae3bc-1", "/usr/lib/x86_64-linux-gnu/libretro/nestopia_libretro.so"),
    "libretro-gambatte": ("0.5.0+git20160522+dfsg1-3build1", "/usr/lib/x86_64-linux-gnu/libretro/gambatte_libretro.so"),
    "libretro-mgba": ("0.10.5+dfsg-3build1", "/usr/lib/x86_64-linux-gnu/libretro/mgba_libretro.so"),
    "libretro-bsnes-mercury-balanced": ("094+git20220807-8build2", "/usr/lib/x86_64-linux-gnu/libretro/bsnes_mercury_balanced_libretro.so"),
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
    directory = Path(directory) if directory else Path(os.environ.get("XDG_DATA_HOME",str(Path.home()/".local/share")))/"applications"
    if not directory.is_absolute():raise ValueError("Choose an absolute private application-data directory")
    definitions = {
        "supertux": ("supertux", "SuperTux", ["--fullscreen", "--renderer", "sdl"]),
    }
    written = []
    for package, state in receipt["packages"].items():
        if package not in definitions: continue
        if not state["ready"]: continue
        app_id, name, arguments = definitions[package]
        executable = state["executable"]
        if executable not in (PACKAGES[package][1], PACKAGES[package][1].replace("/usr/games/", "/usr/bin/")):
            raise ValueError("Runtime executable does not match the verified manifest")
        entry = "[Desktop Entry]\nType=Application\nName="+name+"\nExec="+" ".join([executable]+arguments)+"\nTerminal=false\nNoDisplay=true\n"
        path = directory/("isolde-"+app_id+".desktop")
        atomic_bytes(path, entry.encode()); written.append(str(path))
    manifest=Path(__file__).with_name("game-runtimes.json")
    if not manifest.exists():manifest=Path(__file__).parents[1]/"config/game-runtimes.json"
    runtime_document = json.loads(manifest.read_text())
    for runtime in runtime_document["runtimes"]:
        # Core presence and the frontend package must both be verified. PSP is
        # supplied by the separately pinned, locally built Debian payload.
        required = {"ppsspp":["isolde-ppsspp"],"dolphin":["dolphin-emu"],"pcsx2":["pcsx2"],
            "retroarch-nes":["retroarch","libretro-nestopia"],"retroarch-gb":["retroarch","libretro-gambatte"],
            "retroarch-gba":["retroarch","libretro-mgba"],"retroarch-snes":["retroarch","libretro-bsnes-mercury-balanced"]}[runtime["id"]]
        if not all(receipt["packages"].get(name,{}).get("ready") for name in required): continue
        path=directory/("isolde-"+runtime["id"]+".desktop")
        atomic_bytes(path,("[Desktop Entry]\nType=Application\nName="+runtime["name"]+"\nExec=/usr/bin/python3\nTerminal=false\nNoDisplay=true\n").encode())
        written.append(str(path))
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
        binary_hash=None
        if binary:
            with binary.open("rb") as stream:binary_hash=hashlib.file_digest(stream,"sha256").hexdigest()
        result[name] = {"expectedVersion": expected, "installedVersion": lines[1] if installed else None,
            "ready": installed and lines[1] == expected and binary is not None,
            "executable": str(binary) if binary else None,
            "sha256": binary_hash,
            "copyright": str(Path("/usr/share/doc") / name / "copyright") if installed else None}
    return result


def provision(packages, install=False):
    platform()
    if not packages or len(set(packages)) != len(packages) or any(p not in PACKAGES for p in packages):
        raise ValueError("Choose declared runtime packages only")
    before = inspect(packages)
    missing = [p for p in packages if not before[p]["ready"]]
    if install and "isolde-ppsspp" in missing:
        spec=importlib.util.spec_from_file_location("ppsspp_builder",Path(__file__).with_name("build-ppsspp.py"))
        builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
        payload=builder.build(Path.home()/".cache/isolde-emulators/ppsspp-1.20.4")
        run("sudo","-n","/usr/bin/apt-get","install","--no-remove","--no-install-recommends","-y",str(payload),timeout=180)
        missing.remove("isolde-ppsspp")
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
    packages = [name for name in PACKAGES if name!="supertux"] + (["supertux"] if args.fixture else [])
    result = provision(packages, args.install)
    if args.fixture and args.install:
        spec=importlib.util.spec_from_file_location("game_fixtures",Path(__file__).with_name("fetch-game-fixtures.py"))
        fixtures=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixtures)
        result["fixtures"]=[fixtures.fetch()]
    if result["ready"] and args.install: result["launchers"] = register_launchers(result)
    atomic_receipt(args.receipt, result)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
