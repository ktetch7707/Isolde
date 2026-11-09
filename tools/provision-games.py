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
import re
import signal
import shutil
import subprocess
import tempfile
import importlib.util
import time

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

_CANCEL_REQUESTED = False


class ProvisioningCancelled(RuntimeError):
    pass


def _request_cancel(_signal, _frame):
    global _CANCEL_REQUESTED
    _CANCEL_REQUESTED = True


def install_cancellation_handlers():
    import signal
    signal.signal(signal.SIGINT, _request_cancel)
    signal.signal(signal.SIGTERM, _request_cancel)


def check_cancelled():
    if _CANCEL_REQUESTED:
        raise ProvisioningCancelled("Stopped at a safe package-operation boundary.")


def run(*args, **options):
    global _CANCEL_REQUESTED
    check_cancelled()
    timeout=options.pop("timeout",60)
    transaction=options.pop("transaction",False)
    check=options.pop("check",True)
    capture_output=options.pop("capture_output",False)
    text=options.pop("text",False)
    options.setdefault("start_new_session",True)
    if capture_output:
        options.setdefault("stdout",subprocess.PIPE);options.setdefault("stderr",subprocess.PIPE)
    process=subprocess.Popen(args,text=text,**options)
    deadline=time.monotonic()+timeout
    while True:
        try:
            output,error=process.communicate(timeout=min(1,max(.01,deadline-time.monotonic())))
            break
        except subprocess.TimeoutExpired as expired:
            timed_out=time.monotonic()>=deadline
            if _CANCEL_REQUESTED and not transaction:
                _stop_process_group(process)
                raise ProvisioningCancelled("Stopped the active download safely.")
            if timed_out and transaction:
                _CANCEL_REQUESTED=True
                output,error=process.communicate()
                if process.returncode and check:
                    raise subprocess.CalledProcessError(process.returncode,args,output,error)
                raise ProvisioningCancelled("The package operation exceeded its deadline but finished safely.")
            if timed_out:
                _stop_process_group(process)
                raise expired
    result=subprocess.CompletedProcess(args,process.returncode,output,error)
    if process.returncode:
        raise subprocess.CalledProcessError(process.returncode,args,output,error)
    check_cancelled()
    return result


def _stop_process_group(process):
    try:os.killpg(process.pid,signal.SIGTERM)
    except ProcessLookupError:return
    try:process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        try:os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError:pass
        process.communicate()


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
    if not verify_runtime_receipt(receipt):
        raise ValueError("Runtime receipt does not match the pinned manifest and installed files")
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
    _,runtimes=runtime_manifest()
    for identity,runtime in runtimes.items():
        if not receipt.get("runtimes",{}).get(identity,{}).get("ready"):continue
        path=directory/("isolde-"+runtime["id"]+".desktop")
        atomic_bytes(path,("[Desktop Entry]\nType=Application\nName="+runtime["name"]+"\nExec=/usr/bin/python3\nTerminal=false\nNoDisplay=true\n").encode())
        written.append(str(path))
    return written


def inspect(packages):
    result = {}
    for name in packages:
        expected, executable = PACKAGES[name]
        query = run("dpkg-query", "-W", "-f=${Status}\n${Version}", name, capture_output=True, text=True, timeout=10,check=False)
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


def runtime_manifest():
    spec=importlib.util.spec_from_file_location("emulator_session",Path(__file__).with_name("emulator-session.py"))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    path=Path(__file__).with_name("game-runtimes.json")
    if not path.exists():path=Path(__file__).parents[1]/"config/game-runtimes.json"
    return path,module.definitions(path)


def file_hash(path):
    path=Path(path)
    if path.is_symlink() or not path.is_file():return None
    with path.open("rb") as stream:return hashlib.file_digest(stream,"sha256").hexdigest()


def inspect_runtimes(packages):
    manifest_path,runtimes=runtime_manifest()
    result={}
    for identity,runtime in runtimes.items():
        required=runtime["packages"]
        binary=Path(runtime["binary"])
        core=Path(runtime["core"]) if runtime.get("core") else None
        ready=(all(packages.get(name,{}).get("ready",False) for name in required)
            and binary.is_file() and not binary.is_symlink() and os.access(binary,os.X_OK)
            and (core is None or core.is_file() and not core.is_symlink()))
        result[identity]={"platforms":runtime["platforms"],"packages":required,"binary":str(binary),
            "binarySha256":file_hash(binary) if ready else None,
            "core":str(core) if core else None,"coreSha256":file_hash(core) if ready and core else None,
            "ready":bool(ready)}
    return hashlib.sha256(manifest_path.read_bytes()).hexdigest(),result


def verify_runtime_receipt(receipt):
    manifest_path,runtimes=runtime_manifest()
    states=receipt.get("runtimes")
    package_states=receipt.get("packages")
    if (type(receipt.get("schemaVersion")) is not int or receipt["schemaVersion"]!=2
            or receipt.get("manifestSha256")!=hashlib.sha256(manifest_path.read_bytes()).hexdigest()
            or not isinstance(states,dict) or set(states)!=set(runtimes)
            or not isinstance(package_states,dict)):
        return False
    required_packages={package for runtime in runtimes.values() for package in runtime["packages"]}
    if not required_packages.issubset(package_states):return False
    for package in required_packages:
        package_state=package_states.get(package)
        if not isinstance(package_state,dict) or package not in PACKAGES:return False
        expected,executable=PACKAGES[package]
        if (type(package_state.get("ready")) is not bool
                or package_state.get("expectedVersion")!=expected
                or (package_state["ready"] and package_state.get("installedVersion")!=expected)
                or package_state.get("executable") not in (executable,executable.replace("/usr/games/","/usr/bin/"))):
            return False
        if package_state["ready"] and (not isinstance(package_state.get("sha256"),str)
                or not re.fullmatch(r"[a-f0-9]{64}",package_state["sha256"])):
            return False
    for identity,runtime in runtimes.items():
        state=states.get(identity,{})
        if not isinstance(state,dict):return False
        core=runtime.get("core")
        if (state.get("platforms")!=runtime["platforms"] or state.get("packages")!=runtime["packages"]
                or state.get("binary")!=runtime["binary"] or state.get("core")!=core
                or type(state.get("ready")) is not bool):
            return False
        if not state.get("ready"):
            continue
        if not all(package_states[name]["ready"] for name in runtime["packages"]):return False
        binary_hash=state.get("binarySha256")
        if not isinstance(binary_hash,str) or not re.fullmatch(r"[a-f0-9]{64}",binary_hash) or binary_hash!=file_hash(runtime["binary"]):
            return False
        if core:
            core_hash=state.get("coreSha256")
            if not isinstance(core_hash,str) or not re.fullmatch(r"[a-f0-9]{64}",core_hash) or core_hash!=file_hash(core):return False
        if not core and state.get("coreSha256") is not None:
            return False
    return True


def provision(packages, install=False):
    platform()
    if not packages or len(set(packages)) != len(packages) or any(p not in PACKAGES for p in packages):
        raise ValueError("Choose declared runtime packages only")
    before = inspect(packages)
    missing = [p for p in packages if not before[p]["ready"]]
    changed=[p for p in missing if before[p].get("installedVersion")
        and before[p]["installedVersion"]!=PACKAGES[p][0]]
    if install and changed:
        raise RuntimeError("A different emulator package version is installed. Preserve it; use an explicit reviewed update/rollback before retrying: "+", ".join(changed))
    if install and "isolde-ppsspp" in missing:
        spec=importlib.util.spec_from_file_location("ppsspp_builder",Path(__file__).with_name("build-ppsspp.py"))
        builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
        payload=builder.build(Path.home()/".cache/isolde-emulators/ppsspp-1.20.4")
        check_cancelled()
        run("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","/usr/bin/apt-get","install","--no-remove","--no-upgrade","--no-install-recommends",
            "--option=Acquire::Retries=1","--option=Acquire::http::Timeout=60","--option=Acquire::https::Timeout=60",
            "-y",str(payload),timeout=900,transaction=True)
        missing.remove("isolde-ppsspp")
    if install and missing:
        if shutil.disk_usage(Path.home()).free < 2 * 1024**3:
            raise RuntimeError("At least 2 GiB guest space required before runtime installation")
        # Signed Ubuntu repository metadata supplies dependency and payload trust.
        # Refuse silent downgrades or removal; a changed pin needs a new manifest.
        run("sudo", "-n","env","DEBIAN_FRONTEND=noninteractive", "/usr/bin/apt-get", "update","--option=Acquire::Retries=1",
            "--option=Acquire::http::Timeout=60","--option=Acquire::https::Timeout=60",timeout=180)
        run("sudo", "-n","env","DEBIAN_FRONTEND=noninteractive", "/usr/bin/apt-get", "install", "--no-remove", "--no-upgrade", "--no-install-recommends", "-y",
            "--option=Acquire::Retries=1","--option=Acquire::http::Timeout=60","--option=Acquire::https::Timeout=60",
            *[p + "=" + PACKAGES[p][0] for p in missing], timeout=900,transaction=True)
    after = inspect(packages)
    manifest_hash,runtime_state=inspect_runtimes(after)
    package_ready=all(value["ready"] for value in after.values())
    result = {"schemaVersion": 2, "manifestSha256": manifest_hash,"packages": after,"runtimes":runtime_state,"before": before,
        "ready": package_ready and all(value["ready"] for value in runtime_state.values()), "installationRequested": install,
        "gameplayVerified": False, "controllerVerified": False, "firmwareProvided": False}
    if install and not result["ready"]:
        raise RuntimeError("Runtime verification failed; preserve package state and inspect the named package")
    return result


def main():
    install_cancellation_handlers()
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
    try:
        main()
    except ProvisioningCancelled as error:
        print(str(error),file=sys.stderr)
        raise SystemExit(130)
