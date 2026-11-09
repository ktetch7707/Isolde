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
import sys
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
    "libretro-desmume": ("0.9.11+git20160819+dfsg1-2build2", "/usr/lib/x86_64-linux-gnu/libretro/desmume_libretro.so"),
    "libretro-beetle-vb": ("0.9.36.1+git20160623-2build2", "/usr/lib/x86_64-linux-gnu/libretro/mednafen_vb_libretro.so"),
}
MAX_BUNDLE_FILES=512
MAX_BUNDLE_BYTES=8*1024**3
MAX_DEB_BYTES=2*1024**3

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
    if process.returncode and check:
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
    runtimes=module.definitions(path)
    declared={name for runtime in runtimes.values() for name in runtime["packages"]}
    pinned=set(PACKAGES)-{"supertux"}
    if declared!=pinned:
        raise ValueError("Runtime manifest package set does not match the pinned provisioning set")
    return path,runtimes


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


def emulator_package_pins():
    return {name:version for name,(version,_) in PACKAGES.items() if name!="supertux"}


def bundle_cache_roots():
    cache=Path(os.environ.get("XDG_CACHE_HOME",str(Path.home()/".cache")))
    if not cache.is_absolute():raise ValueError("Choose an absolute private cache directory")
    root=cache/"isolde/game-runtime-bundles"
    if root.is_symlink() or root.resolve()!=root:raise ValueError("Redirected runtime bundle cache")
    return (root,)


def bundle_signing_paths(create=True):
    state=Path(os.environ.get("XDG_STATE_HOME",str(Path.home()/".local/state")))
    if not state.is_absolute() or state.is_symlink() or state.resolve()!=state:
        raise ValueError("Choose an absolute private state directory")
    root=state/"isolde"
    if root.is_symlink() or root.resolve()!=root:raise ValueError("Redirected runtime signing-key directory")
    root.mkdir(parents=True,exist_ok=True,mode=0o700)
    if root.stat().st_uid!=os.getuid():raise ValueError("Runtime bundle signing-key directory is not user-owned")
    root.chmod(0o700)
    private=root/"runtime-bundle.key"
    public=root/"runtime-bundle.pub"
    if private.is_symlink() or public.is_symlink():raise ValueError("Redirected runtime bundle signing key")
    if private.exists() and public.exists():
        metadata=private.stat()
        if (metadata.st_uid!=os.getuid() or metadata.st_mode & 0o077
                or not public.is_file() or public.stat().st_uid!=os.getuid()):
            raise ValueError("Runtime bundle signing key is not private and user-owned")
        return private,public
    if not create:raise ValueError("No locally trusted runtime bundle key is available")
    if private.exists() or public.exists():raise ValueError("Incomplete runtime bundle signing key pair")
    stage=Path(tempfile.mkdtemp(prefix=".bundle-key-",dir=root))
    try:
        staged_private=stage/"private.pem";staged_public=stage/"public.pem"
        run("openssl","genpkey","-algorithm","ED25519","-out",str(staged_private),timeout=30)
        run("openssl","pkey","-in",str(staged_private),"-pubout","-out",str(staged_public),timeout=30)
        staged_private.chmod(0o600)
        atomic_bytes(private,staged_private.read_bytes())
        atomic_bytes(public,staged_public.read_bytes())
        return private,public
    finally:
        shutil.rmtree(stage)


def sign_document(path, signature_path):
    path=Path(path)
    signature_path=Path(signature_path)
    private,_=bundle_signing_paths()
    fd,temporary=tempfile.mkstemp(prefix=".document-signature-",dir=signature_path.parent)
    os.close(fd)
    try:
        run("openssl","pkeyutl","-sign","-inkey",str(private),"-rawin",
            "-in",str(path),"-out",temporary,timeout=30)
        os.replace(temporary,signature_path)
    finally:
        if os.path.exists(temporary):os.unlink(temporary)


def verify_document_signature(path, signature_path, public=None):
    path=Path(path).absolute()
    signature_path=Path(signature_path).absolute()
    if signature_path.is_symlink() or not signature_path.is_file() or signature_path.stat().st_size!=64:
        raise ValueError("Runtime document lacks a trusted signature")
    if public is None:
        _,public=bundle_signing_paths(create=False)
    public=Path(public).absolute()
    result=run("openssl","pkeyutl","-verify","-pubin","-inkey",str(public),"-rawin",
        "-in",str(path),"-sigfile",str(signature_path),
        capture_output=True,text=True,timeout=30,check=False)
    if result.returncode:
        raise ValueError("Runtime document signature does not match its trusted signing key")


def sign_bundle(path):
    root=Path(path)
    sign_document(root/"bundle.json",root/"bundle.sig")


def verify_bundle_signature(path):
    root=Path(path).absolute()
    packaged=Path(__file__).with_name("game-runtime-bundle").absolute()
    if root.parent==packaged:
        public=Path(__file__).with_name("game-runtime-bundle.pub").absolute()
        if public.is_symlink() or not public.is_file() or public.stat().st_uid!=0 or public.stat().st_mode & 0o022:
            raise ValueError("Packaged runtime bundle has no root-owned trusted signing key")
        verify_document_signature(root/"bundle.json",root/"bundle.sig",public)
    else:
        verify_document_signature(root/"bundle.json",root/"bundle.sig")


def package_status(names, runner=run):
    result={}
    for name in sorted(set(names)):
        if not re.fullmatch(r"[a-z0-9][a-z0-9+.-]{0,80}",name):
            raise ValueError("Invalid package name in runtime bundle")
        query=runner("dpkg-query","-W","-f=${Status}\n${Version}",name,
            capture_output=True,text=True,timeout=10,check=False)
        lines=(query.stdout or "").splitlines()
        result[name]={"status":lines[0],"version":lines[1]} if len(lines)==2 else None
    return result


def installed_version(state):
    return state.get("version") if isinstance(state,dict) and state.get("status")=="install ok installed" else None


def package_metadata(path, runner=run):
    path=Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size<=0 or path.stat().st_size>MAX_DEB_BYTES:
        raise ValueError("Invalid or oversized runtime package archive")
    result=runner("/usr/bin/dpkg-deb","-f",str(path),"Package","Version","Architecture",
        capture_output=True,text=True,timeout=30)
    fields=(result.stdout or "").splitlines()
    if len(fields)!=3 or not re.fullmatch(r"[a-z0-9][a-z0-9+.-]{0,80}",fields[0]) \
            or not re.fullmatch(r"[A-Za-z0-9.+:~_-]{1,128}",fields[1]) or fields[2] not in ("amd64","all"):
        raise ValueError("Runtime bundle contains an unsupported Debian package")
    with path.open("rb") as stream:digest=hashlib.file_digest(stream,"sha256").hexdigest()
    return {"package":fields[0],"version":fields[1],"architecture":fields[2],
        "file":path.name,"size":path.stat().st_size,"sha256":digest}


def validate_bundle(path, expected_manifest=None, runner=run, allow_historical=False):
    root=Path(path).absolute()
    if root.is_symlink() or not root.is_dir() or root.resolve()!=root:
        raise ValueError("Runtime dependency bundle is missing or redirected")
    manifest_path=root/"bundle.json"
    if manifest_path.is_symlink() or not manifest_path.is_file() or manifest_path.stat().st_size>1024*1024:
        raise ValueError("Runtime dependency bundle manifest is missing or oversized")
    document=json.loads(manifest_path.read_text())
    if (not isinstance(document,dict)
            or set(document)!={"schemaVersion","target","runtimeManifestSha256","runtimePackages","packages"}
            or type(document["schemaVersion"]) is not int or document["schemaVersion"]!=1
            or document["target"]!={"os":"ubuntu","version":"26.04","architecture":"amd64"}
            or not re.fullmatch(r"[a-f0-9]{64}",document["runtimeManifestSha256"])
            or (expected_manifest and document["runtimeManifestSha256"]!=expected_manifest)
            or not isinstance(document["runtimePackages"],dict) or not isinstance(document["packages"],list)
            or len(document["packages"])==0 or len(document["packages"])>MAX_BUNDLE_FILES):
        raise ValueError("Unsupported or mismatched runtime dependency bundle")
    root_contents={entry.name for entry in root.iterdir()}
    if (root_contents!={"bundle.json","bundle.sig","archives"}
            or any((root/name).is_symlink() for name in root_contents)):
        raise ValueError("Runtime bundle contains unlisted or redirected files")
    if (set(document["runtimePackages"])!=set(emulator_package_pins())
            or any(not isinstance(name,str) or not re.fullmatch(r"[a-z0-9][a-z0-9+.-]{0,80}",name)
                   or not isinstance(version,str) or not re.fullmatch(r"[A-Za-z0-9.+:~_-]{1,128}",version)
                   for name,version in document["runtimePackages"].items())
            or (not allow_historical and document["runtimePackages"]!=emulator_package_pins())):
        raise ValueError("Runtime package pins do not match this Isolde release")
    archives=root/"archives"
    if archives.is_symlink() or not archives.is_dir() or archives.resolve()!=archives:
        raise ValueError("Runtime bundle archive directory is missing or redirected")
    seen=set();total=0;rows=[]
    for item in document["packages"]:
        if (not isinstance(item,dict) or set(item)!={"package","version","architecture","file","size","sha256"}
                or not isinstance(item["file"],str) or not re.fullmatch(r"[A-Za-z0-9.+_%~-]{1,180}\.deb",item["file"])
                or item["file"] in seen or type(item["size"]) is not int or item["size"]<=0
                or item["size"]>MAX_DEB_BYTES or not re.fullmatch(r"[a-f0-9]{64}",item["sha256"])):
            raise ValueError("Invalid package entry in runtime dependency bundle")
        seen.add(item["file"]);total+=item["size"]
        if total>MAX_BUNDLE_BYTES:raise ValueError("Runtime dependency bundle exceeds its storage budget")
        archive=root/"archives"/item["file"]
        metadata=package_metadata(archive,runner)
        if metadata!=item:raise ValueError("Runtime package archive hash or metadata mismatch")
        rows.append(item)
    contents={p.name for p in archives.iterdir()}
    if contents!=seen or any((archives/name).is_symlink() or not (archives/name).is_file() for name in contents):
        raise ValueError("Runtime dependency bundle contains unlisted files")
    available={(item["package"],item["version"]) for item in rows}
    for name,version in document["runtimePackages"].items():
        if (name,version) not in available:raise ValueError("Runtime dependency bundle is missing a pinned package")
    return document


def authenticate_bundle(path, document, runner=run, verify_repository_indices=False):
    """Verify the trusted bundle signature and optionally its repository provenance."""
    verify_bundle_signature(path)
    for item in document["packages"]:
        if item["package"]=="isolde-ppsspp":
            # This one package is built locally from the pinned upstream commit;
            # its control metadata binds the payload to that declared source.
            result=runner("/usr/bin/dpkg-deb","-f",str(Path(path)/"archives"/item["file"]),
                "X-Isolde-Source-Commit",capture_output=True,text=True,timeout=30)
            if (result.stdout or "").strip()!="fa50bb1976065c4f8b1b47af227d367fe9771555":
                raise ValueError("Local PSP payload is not bound to the pinned upstream source")
            continue
        if not verify_repository_indices:
            continue
        result=runner("apt-cache","show",item["package"]+"="+item["version"],
            capture_output=True,text=True,timeout=30)
        records=(result.stdout or "").strip().split("\n\n")
        expected=None
        for record in records:
            fields={}
            for line in record.splitlines():
                if ": " in line:
                    key,value=line.split(": ",1);fields[key]=value
            if (fields.get("Package")==item["package"] and fields.get("Version")==item["version"]
                    and fields.get("Architecture") in (item["architecture"],"all")
                    and re.fullmatch(r"[a-f0-9]{64}",fields.get("SHA256",""))):
                expected=fields["SHA256"]
                break
        if expected!=item["sha256"]:
            raise ValueError("Runtime package does not match the cached signed Ubuntu package index: "+item["package"])


def indexed_package_hash(name,version,architecture,runner=run):
    result=runner("apt-cache","show",name+"="+version,capture_output=True,text=True,timeout=30)
    for record in (result.stdout or "").strip().split("\n\n"):
        fields={}
        for line in record.splitlines():
            if ": " in line:
                key,value=line.split(": ",1);fields[key]=value
        if (fields.get("Package")==name and fields.get("Version")==version
                and fields.get("Architecture") in (architecture,"all")
                and re.fullmatch(r"[a-f0-9]{64}",fields.get("SHA256",""))):
            return fields["SHA256"]
    raise RuntimeError("No authenticated Ubuntu package index entry is available for rollback version "+name+"="+version)


def _copy_archive(source,destination):
    source=Path(source);destination=Path(destination)
    if source.is_symlink() or not source.is_file():raise ValueError("Refusing redirected package input")
    if destination.exists():raise ValueError("Duplicate package archive in runtime bundle")
    shutil.copyfile(source,destination)


def signed_historical_archive(name,version,runner=run):
    for cache_root in bundle_cache_roots():
        if not cache_root.is_dir() or cache_root.is_symlink():continue
        for candidate in sorted(cache_root.iterdir()):
            if candidate.is_symlink() or not candidate.is_dir():continue
            try:
                old=validate_bundle(candidate,runner=runner,allow_historical=True)
                verify_bundle_signature(candidate)
            except (OSError,ValueError,json.JSONDecodeError,subprocess.CalledProcessError):
                continue
            if old["runtimePackages"].get(name)!=version:continue
            for entry in old["packages"]:
                if entry["package"]==name and entry["version"]==version:
                    return candidate/"archives"/entry["file"],entry
    return None,None


def create_rollback_snapshot(before, requested, runner=run, offline=False):
    versions={name:installed_version(before.get(name)) for name in requested}
    versions={name:version for name,version in versions.items() if version is not None}
    if not versions: return None
    state=Path(os.environ.get("XDG_STATE_HOME",str(Path.home()/".local/state")))
    if not state.is_absolute() or state.is_symlink() or state.resolve()!=state:
        raise ValueError("Choose an absolute private runtime transaction state directory")
    root=state/"isolde/runtime-transactions"
    if root.is_symlink() or root.resolve()!=root:raise ValueError("Redirected runtime transaction directory")
    root.mkdir(parents=True,exist_ok=True,mode=0o700);root.chmod(0o700)
    stage=Path(tempfile.mkdtemp(prefix="update-",dir=root));(stage/"archives").mkdir(mode=0o700)
    entries=[]
    try:
        for name,version in sorted(versions.items()):
            source,entry=signed_historical_archive(name,version,runner)
            if source is not None:
                _copy_archive(source,stage/"archives"/entry["file"])
                metadata=package_metadata(stage/"archives"/entry["file"],runner)
                if metadata!=entry:raise ValueError("Prior signed runtime rollback archive changed")
            elif name=="isolde-ppsspp" or offline:
                raise RuntimeError("A signed prior runtime package bundle is required for a reversible offline update: "+name)
            else:
                expected=indexed_package_hash(name,version,"amd64",runner)
                runner("apt-get","download",name+"="+version,cwd=str(stage/"archives"),timeout=300)
                matches=[]
                for archive in (stage/"archives").glob("*.deb"):
                    metadata=package_metadata(archive,runner)
                    if metadata["package"]==name and metadata["version"]==version and metadata["sha256"]==expected:
                        matches.append(archive)
                if len(matches)!=1:
                    raise RuntimeError("Could not retrieve exactly one authenticated rollback archive for "+name+"="+version)
                matches[0].rename(stage/"archives"/(name+"_"+version+"_amd64.deb"))
                metadata=package_metadata(stage/"archives"/(name+"_"+version+"_amd64.deb"),runner)
                entries.append(metadata)
                continue
            entries.append(metadata)
        document={"schemaVersion":1,"before":versions,"packages":entries}
        atomic_bytes(stage/"rollback.json",json.dumps(document,indent=2).encode())
        sign_document(stage/"rollback.json",stage/"rollback.sig")
        verify_document_signature(stage/"rollback.json",stage/"rollback.sig")
        final=root/("update-"+str(int(time.time()))+"-"+os.urandom(8).hex())
        os.replace(stage,final)
        return {"directory":str(final),"document":document}
    finally:
        if stage.exists():shutil.rmtree(stage)


def validate_rollback_snapshot(snapshot,runner=run):
    if not isinstance(snapshot,dict) or set(snapshot)!={"directory","document"}:
        raise ValueError("Update recovery snapshot reference is invalid")
    root=Path(snapshot["directory"]).absolute()
    state=Path(os.environ.get("XDG_STATE_HOME",str(Path.home()/".local/state")))
    expected=(state/"isolde/runtime-transactions").absolute()
    if (root.is_symlink() or not root.is_dir() or root.resolve()!=root
            or root.parent!=expected or not root.name.startswith("update-")):
        raise ValueError("Update recovery snapshot path is redirected")
    manifest=root/"rollback.json"
    if manifest.is_symlink() or not manifest.is_file() or manifest.stat().st_size>128*1024:
        raise ValueError("Update recovery snapshot is missing or oversized")
    verify_document_signature(manifest,root/"rollback.sig")
    document=json.loads(manifest.read_text())
    if not isinstance(document,dict) or document!=snapshot["document"] \
            or set(document)!={"schemaVersion","before","packages"} \
            or document["schemaVersion"]!=1 or not isinstance(document["before"],dict) \
            or not isinstance(document["packages"],list) or not document["packages"]:
        raise ValueError("Update recovery snapshot does not match its signed manifest")
    expected_names=set(document["before"])
    if (set(document["before"])!=expected_names or len(expected_names)!=len(document["packages"])
            or any(name not in emulator_package_pins() or not isinstance(version,str)
                   or not re.fullmatch(r"[A-Za-z0-9.+:~_-]{1,128}",version)
                   for name,version in document["before"].items())):
        raise ValueError("Update recovery snapshot package set is invalid")
    archive_dir=root/"archives"
    if archive_dir.is_symlink() or not archive_dir.is_dir() or archive_dir.resolve()!=archive_dir:
        raise ValueError("Update rollback archive directory is redirected")
    observed={}
    for entry in document["packages"]:
        if not isinstance(entry,dict) or set(entry)!={"package","version","architecture","file","size","sha256"}:
            raise ValueError("Update rollback package entry is invalid")
        if (not isinstance(entry["package"],str) or entry["package"] not in expected_names
                or entry["version"]!=document["before"].get(entry["package"])
                or entry["architecture"] not in ("amd64","all")
                or not isinstance(entry["file"],str)
                or not re.fullmatch(r"[A-Za-z0-9.+_%~-]{1,180}\.deb",entry["file"])
                or Path(entry["file"]).name!=entry["file"]
                or type(entry["size"]) is not int or entry["size"]<=0 or entry["size"]>MAX_DEB_BYTES
                or not isinstance(entry["sha256"],str) or not re.fullmatch(r"[a-f0-9]{64}",entry["sha256"])):
            raise ValueError("Update rollback package entry has unsafe metadata")
        metadata=package_metadata(archive_dir/entry["file"],runner)
        if metadata!=entry or document["before"].get(entry["package"])!=entry["version"]:
            raise ValueError("Update rollback archive does not match the initial installed version")
        if entry["package"] in observed:raise ValueError("Duplicate update rollback package")
        observed[entry["package"]]=entry
    if set(observed)!=expected_names or {p.name for p in archive_dir.iterdir()}!={entry["file"] for entry in observed.values()}:
        raise ValueError("Update rollback archive set is incomplete")
    return root,document


def verify_apt_install_plan(archives,expected,runner=run,installed=None):
    prefix=("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","LC_ALL=C","/usr/bin/apt-get")
    paths=[str(Path(path).absolute()) for path in archives]
    simulation=runner(*prefix,"--simulate","--no-download","--no-remove","--allow-downgrades",
        "--no-install-recommends","install",*paths,capture_output=True,text=True,timeout=300)
    changed={}
    for line in (simulation.stdout or "").splitlines():
        if line.startswith("Remv "):
            raise RuntimeError("Runtime update plan would remove a package; transaction refused.")
        match=re.match(r"^Inst ([a-z0-9][a-z0-9+.-]*(?::[a-z0-9]+)?)(?: \[([^]]+)\])? \(([^ )]+)",line)
        if not match:continue
        name=match.group(1).split(":",1)[0]
        old,new=match.group(2),match.group(3)
        if name not in expected:
            raise RuntimeError("Runtime update plan changes an undeclared package: "+name)
        if installed is not None:
            current=installed_version(installed.get(name))
            if old!=current:
                raise RuntimeError("Runtime package state changed since the update snapshot: "+name)
        if new!=expected[name]:
            raise RuntimeError("Runtime update plan selected an unexpected package version: "+name)
        changed[name]=new
    if changed!=expected:
        raise RuntimeError("Runtime update plan does not contain every exact requested package version.")


def restore_runtime_snapshot(snapshot,runner=run):
    root,document=validate_rollback_snapshot(snapshot,runner)
    archives=[str(root/"archives"/entry["file"]) for entry in document["packages"]]
    expected=document["before"]
    current=package_status(expected.keys(),runner)
    verify_apt_install_plan(archives,expected,runner,current)
    prefix=("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","LC_ALL=C","/usr/bin/apt-get")
    runner(*prefix,"--no-download","--no-remove","--allow-downgrades","--yes",
        "--no-install-recommends","install",*archives,timeout=900,transaction=True)
    current=package_status(expected,runner)
    if any(installed_version(current.get(name))!=version for name,version in expected.items()):
        raise RuntimeError("Restored runtime package versions do not match the pre-update snapshot.")


def apply_runtime_update(root,document,before,updates,runner,checkpoint,offline):
    if not updates:return {"ready":True,"installed":before,"updated":[]}
    if any(installed_version(before.get(name)) is None for name in emulator_package_pins()):
        raise RuntimeError("Install all declared runtime packages before requesting a reversible update.")
    entries={item["package"]:item for item in document["packages"]}
    if any(name not in entries for name in updates):
        raise ValueError("Runtime update bundle is missing a requested pinned package")
    archives=[str(root/"archives"/entries[name]["file"]) for name in updates]
    verify_apt_install_plan(archives,updates,runner,before)
    if checkpoint is None:
        checkpoint=Path(os.environ.get("XDG_STATE_HOME",str(Path.home()/".local/state")))/"isolde/emulator-provisioning.json"
    checkpoint=Path(checkpoint).absolute()
    if checkpoint.exists():raise RuntimeError("An interrupted emulator package transaction needs recovery before retrying.")
    snapshot=create_rollback_snapshot(before,updates,runner,offline)
    transaction={"schemaVersion":2,"phase":"applying","bundle":str(root),"before":before,
        "requested":updates,"rollbackSnapshot":snapshot,"createdAt":int(time.time())}
    atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
    prefix=("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","LC_ALL=C","/usr/bin/apt-get")
    try:
        runner(*prefix,"--no-download","--no-remove","--allow-downgrades","--yes","--no-install-recommends",
            "install",*archives,timeout=1800,transaction=True)
        after=package_status(before.keys(),runner)
        invalid={name:after.get(name) for name,version in updates.items()
            if installed_version(after.get(name))!=version}
        if invalid:raise RuntimeError("Updated runtime packages did not match their pinned versions.")
        transaction["phase"]="committed";transaction["after"]=after
        atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
        checkpoint.unlink()
        shutil.rmtree(snapshot["directory"])
        return {"ready":True,"installed":after,"updated":sorted(updates),
            "bundleSha256":hashlib.sha256((root/"bundle.json").read_bytes()).hexdigest()}
    except Exception as error:
        try:
            restore_runtime_snapshot(snapshot,runner)
        except Exception as rollback_error:
            transaction.update({"phase":"recovery-required","failure":str(error)[:1000],
                "rollbackFailure":str(rollback_error)[:1000]})
            atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
            raise RuntimeError("Runtime update failed and prior packages could not be fully restored; keep the checkpoint and retry recovery. "+str(rollback_error)) from error
        checkpoint.unlink(missing_ok=True)
        shutil.rmtree(snapshot["directory"],ignore_errors=True)
        raise RuntimeError("Runtime update failed; the prior pinned package versions were restored. "+str(error)) from error


def purge_added_packages(names,runner=run):
    packages=sorted(set(names))
    if not packages or any(not re.fullmatch(r"[a-z0-9][a-z0-9+.-]{0,80}",name) for name in packages):
        raise ValueError("Invalid package set for transaction rollback")
    prefix=("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","LC_ALL=C","/usr/bin/apt-get")
    simulation=runner(*prefix,"--simulate","--no-download","purge",*packages,
        capture_output=True,text=True,timeout=300)
    planned=set()
    for name in re.findall(r"^Remv ([a-z0-9][a-z0-9+.-]*(?::[a-z0-9]+)?)(?:\s|$)",
                           simulation.stdout or "",re.MULTILINE):
        planned.add(name.split(":",1)[0])
    if planned!=set(packages):
        raise RuntimeError("Rollback purge plan differs from the packages newly added by this transaction.")
    runner(*prefix,"--no-download","--yes","--no-install-recommends","purge",*packages,
        timeout=900,transaction=True)


def prepare_bundle(destination=None):
    platform()
    manifest_path,_=runtime_manifest()
    manifest_hash=hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    if destination is None:
        packaged=Path(__file__).with_name("game-runtime-bundle").absolute()
        for candidate in (packaged,*bundle_cache_roots()):
            final=candidate/("ubuntu-26.04-amd64-"+manifest_hash[:16])
            if final.exists():
                document=validate_bundle(final,manifest_hash)
                authenticate_bundle(final,document,verify_repository_indices=True)
                return final
        base=bundle_cache_roots()[0]
    else:base=Path(destination).absolute()
    if not base.is_absolute() or base.is_symlink():raise ValueError("Choose a safe absolute runtime bundle destination")
    base.mkdir(parents=True,exist_ok=True)
    if base.resolve()!=base:raise ValueError("Runtime bundle destination is redirected")
    final=base/("ubuntu-26.04-amd64-"+manifest_hash[:16])
    if final.exists():
        document=validate_bundle(final,manifest_hash)
        authenticate_bundle(final,document,verify_repository_indices=True)
        return final
    stage=Path(tempfile.mkdtemp(prefix=".runtime-bundle-",dir=base))
    try:
        archives=stage/"archives";archives.mkdir(mode=0o700)
        builder_spec=importlib.util.spec_from_file_location("ppsspp_builder",Path(__file__).with_name("build-ppsspp.py"))
        builder=importlib.util.module_from_spec(builder_spec);builder_spec.loader.exec_module(builder)
        ppsspp=builder.build(Path.home()/".cache/isolde-emulators/ppsspp-1.20.4")
        _copy_archive(ppsspp,archives/ppsspp.name)
        if shutil.disk_usage(stage).free<2*1024**3:
            raise RuntimeError("At least 2 GiB free space is required to prepare offline runtime dependencies")
        run("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","/usr/bin/apt-get","update",
            "--option=Acquire::Retries=1","--option=Acquire::http::Timeout=60",
            "--option=Acquire::https::Timeout=60",timeout=180)
        options=["--option=Dir::Cache::archives="+str(archives)+"/",
            "--option=Dir::Cache::archives::partial="+str(archives)+"/partial/",
            "--option=Dir::State::status="+str(stage/"empty-status")]
        (stage/"empty-status").write_bytes(b"")
        run("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","/usr/bin/apt-get","--download-only",
            "--no-install-recommends","--no-upgrade","--no-remove","-y",*options,
            "--option=Acquire::Retries=1","--option=Acquire::http::Timeout=60",
            "--option=Acquire::https::Timeout=60","install",
            *[name+"="+version for name,version in emulator_package_pins().items() if name!="isolde-ppsspp"],
            str(ppsspp),timeout=1800)
        if (archives/"partial").exists():shutil.rmtree(archives/"partial")
        (stage/"empty-status").unlink(missing_ok=True)
        entries=[package_metadata(archive) for archive in sorted(archives.iterdir())]
        if len(entries)>MAX_BUNDLE_FILES or sum(item["size"] for item in entries)>MAX_BUNDLE_BYTES:
            raise ValueError("Prepared emulator dependency closure exceeds its explicit bundle budget")
        document={"schemaVersion":1,"target":{"os":"ubuntu","version":"26.04","architecture":"amd64"},
            "runtimeManifestSha256":manifest_hash,"runtimePackages":emulator_package_pins(),"packages":entries}
        atomic_bytes(stage/"bundle.json",json.dumps(document,indent=2).encode())
        sign_bundle(stage)
        validate_bundle(stage,manifest_hash)
        authenticate_bundle(stage,document,verify_repository_indices=True)
        os.replace(stage,final)
        return final
    finally:
        if stage.exists():shutil.rmtree(stage)


def apply_bundle(path, installed=None, runner=run, checkpoint=None, update=False, offline=False):
    manifest_path,_=runtime_manifest()
    document=validate_bundle(path,hashlib.sha256(manifest_path.read_bytes()).hexdigest(),runner)
    authenticate_bundle(path,document,runner)
    root=Path(path).absolute()
    required=emulator_package_pins()
    before=installed if installed is not None else package_status(
        [item["package"] for item in document["packages"]],runner)
    for name,version in required.items():
        current=before.get(name)
        if current_version:=installed_version(current):
            if current_version!=version and not update:
                raise RuntimeError("Installed runtime version differs from this bundle; preserve it and use an explicit reviewed update: "+name)
    missing={name:version for name,version in required.items() if installed_version(before.get(name))!=version}
    if update:
        return apply_runtime_update(root,document,before,missing,runner,checkpoint,offline)
    if not missing:return {"ready":True,"installed":before,"added":[],"bundleSha256":hashlib.sha256((root/"bundle.json").read_bytes()).hexdigest()}
    if checkpoint is None:
        checkpoint=Path(os.environ.get("XDG_STATE_HOME",str(Path.home()/".local/state")))/"isolde/emulator-provisioning.json"
    checkpoint=Path(checkpoint).absolute()
    if checkpoint.exists():raise RuntimeError("An interrupted emulator package transaction needs recovery before retrying.")
    transaction={"schemaVersion":1,"phase":"applying","bundle":str(root),
        "before":before,"requested":missing,"createdAt":int(time.time())}
    atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
    archive_option="--option=Dir::Cache::archives="+str(root/"archives")+"/"
    try:
        runner("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","/usr/bin/apt-get","--no-download",
            "--no-upgrade","--no-remove","--no-install-recommends","-y",archive_option,
            "install",*[name+"="+version for name,version in missing.items()],timeout=1800,transaction=True)
        after=package_status(before.keys(),runner)
        invalid={name:after.get(name) for name,version in required.items() if installed_version(after.get(name))!=version}
        if invalid:raise RuntimeError("Installed packages did not match the reviewed bundle: "+", ".join(invalid))
        transaction["phase"]="committed";transaction["after"]=after
        atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
        checkpoint.unlink()
        return {"ready":True,"installed":after,"added":[name for name in missing if before.get(name) is None],
            "bundleSha256":hashlib.sha256((root/"bundle.json").read_bytes()).hexdigest()}
    except Exception as error:
        try:after=package_status(before.keys(),runner)
        except Exception as status_error:
            transaction.update({"phase":"recovery-required","failure":str(error)[:1000],
                "rollbackFailure":"Could not inspect package state: "+str(status_error)[:800]})
            atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
            raise RuntimeError("Emulator package installation state is unknown; preserve Games data and retry recovery.") from error
        added=sorted(name for name,state in after.items() if installed_version(before.get(name)) is None
            and isinstance(state,dict) and state.get("status","").startswith("install "))
        transaction.update({"phase":"rolling-back","failure":str(error)[:1000],"added":added})
        atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
        if added:
            try:
                purge_added_packages(added,runner)
                restored=package_status(before.keys(),runner)
                if any(restored.get(name)!=state for name,state in before.items()):
                    raise RuntimeError("Package rollback did not restore the initial installed state.")
            except Exception as rollback_error:
                transaction["phase"]="recovery-required"
                transaction["rollbackFailure"]=str(rollback_error)[:1000]
                atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
                raise RuntimeError("Emulator package installation failed and rollback needs repair; preserve Games data. "+str(rollback_error)) from error
        checkpoint.unlink(missing_ok=True)
        raise RuntimeError("Emulator package installation failed; newly added packages were rolled back. "+str(error)) from error


def recover_package_transaction(checkpoint=None,runner=run):
    if checkpoint is None:
        checkpoint=Path(os.environ.get("XDG_STATE_HOME",str(Path.home()/".local/state")))/"isolde/emulator-provisioning.json"
    checkpoint=Path(checkpoint).absolute()
    if not checkpoint.exists():return False
    if checkpoint.is_symlink() or checkpoint.stat().st_size>128*1024:
        raise RuntimeError("Emulator recovery record is redirected or oversized; preserve it for review.")
    transaction=json.loads(checkpoint.read_text())
    if not isinstance(transaction,dict):
        raise RuntimeError("Emulator recovery record is invalid; preserve it for review.")
    if transaction.get("schemaVersion")==2:
        if (set(transaction)-{"schemaVersion","phase","bundle","before","requested","rollbackSnapshot",
                              "createdAt","after","failure","rollbackFailure"}
                or transaction.get("phase") not in
                ("applying","recovery-required","committed")):
            raise RuntimeError("Runtime update recovery record is invalid; preserve it for review.")
        snapshot=transaction.get("rollbackSnapshot")
        if transaction["phase"]=="committed":
            validate_rollback_snapshot(snapshot,runner)
            shutil.rmtree(snapshot["directory"])
            checkpoint.unlink()
            return True
        manifest_path,_=runtime_manifest()
        bundle=Path(transaction.get("bundle",""))
        document=validate_bundle(bundle,hashlib.sha256(manifest_path.read_bytes()).hexdigest(),runner)
        authenticate_bundle(bundle,document,runner)
        requested=transaction.get("requested")
        before=transaction.get("before")
        if (not isinstance(requested,dict) or not requested
                or any(name not in emulator_package_pins() or version!=emulator_package_pins()[name]
                       for name,version in requested.items())
                or not isinstance(before,dict)):
            raise RuntimeError("Runtime update recovery record does not match the pinned bundle.")
        restore_runtime_snapshot(snapshot,runner)
        restored=package_status(before.keys(),runner)
        if any(restored.get(name)!=state for name,state in before.items()):
            transaction.update({"phase":"recovery-required",
                "rollbackFailure":"Installed package inventory still differs after update recovery."})
            atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
            raise RuntimeError("Runtime update rollback is incomplete; preserve the recovery record.")
        shutil.rmtree(snapshot["directory"])
        checkpoint.unlink()
        return True
    if (set(transaction)-{"schemaVersion","phase","bundle","before","requested","createdAt","failure","added","rollbackFailure"}
            or transaction.get("schemaVersion")!=1 or transaction.get("phase") not in
            ("applying","rolling-back","recovery-required","committed")):
        raise RuntimeError("Emulator recovery record is invalid; preserve it for review.")
    if transaction["phase"]=="committed":
        checkpoint.unlink()
        return True
    manifest_path,_=runtime_manifest()
    bundle=Path(transaction.get("bundle",""))
    document=validate_bundle(bundle,hashlib.sha256(manifest_path.read_bytes()).hexdigest(),runner)
    authenticate_bundle(bundle,document,runner)
    names={item["package"] for item in document["packages"]}
    before=transaction.get("before")
    requested=transaction.get("requested")
    if (not isinstance(before,dict) or set(before)!=names or not isinstance(requested,dict)
            or not requested or any(name not in emulator_package_pins() or version!=emulator_package_pins()[name]
                                    for name,version in requested.items())):
        raise RuntimeError("Emulator recovery record does not match the pinned bundle; preserve it for review.")
    current=package_status(names,runner)
    added=sorted(name for name,state in current.items() if installed_version(before.get(name)) is None
        and isinstance(state,dict) and state.get("status","").startswith("install "))
    if added:
        purge_added_packages(added,runner)
    restored=package_status(names,runner)
    if any(restored.get(name)!=state for name,state in before.items()):
        transaction.update({"phase":"recovery-required","added":added,
            "rollbackFailure":"Installed package inventory still differs after recovery."})
        atomic_bytes(checkpoint,json.dumps(transaction,indent=2).encode())
        raise RuntimeError("Emulator package rollback is incomplete; preserve the recovery record and Games data.")
    checkpoint.unlink()
    return True


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


def provision(packages, install=False, offline=False, bundle_output=None, update=False):
    platform()
    if not packages or len(set(packages)) != len(packages) or any(p not in PACKAGES for p in packages):
        raise ValueError("Choose declared runtime packages only")
    if offline and not install:raise ValueError("Offline mode requires an explicit --install request")
    if update and not install:raise ValueError("Reversible runtime updates require an explicit --install request")
    before = inspect(packages)
    missing = [p for p in packages if not before[p]["ready"]]
    changed=[p for p in missing if before[p].get("installedVersion")
        and before[p]["installedVersion"]!=PACKAGES[p][0]]
    if update and "supertux" in changed:
        raise RuntimeError("The optional SuperTux gameplay fixture is not part of the reversible emulator runtime update set.")
    if install and changed and not update:
        raise RuntimeError("A different emulator package version is installed. Preserve it; use an explicit reviewed update/rollback before retrying: "+", ".join(changed))
    if install:
        recover_package_transaction()
        runtime_missing=[name for name in missing if name!="supertux"]
        if runtime_missing:
            bundle=find_bundle() if offline else prepare_bundle(bundle_output)
            apply_bundle(bundle,update=update,offline=offline)
        if "supertux" in missing:
            if offline:raise RuntimeError("The optional SuperTux fixture is not included in the offline emulator bundle")
            run("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","/usr/bin/apt-get","update",
                "--option=Acquire::Retries=1","--option=Acquire::http::Timeout=60",
                "--option=Acquire::https::Timeout=60",timeout=180)
            run("sudo","-n","env","DEBIAN_FRONTEND=noninteractive","/usr/bin/apt-get","install",
                "--no-remove","--no-upgrade","--no-install-recommends","-y",
                "supertux="+PACKAGES["supertux"][0],timeout=900,transaction=True)
    after = inspect(packages)
    manifest_hash,runtime_state=inspect_runtimes(after)
    package_ready=all(value["ready"] for value in after.values())
    result = {"schemaVersion": 2, "manifestSha256": manifest_hash,"packages": after,"runtimes":runtime_state,"before": before,
        "ready": package_ready and all(value["ready"] for value in runtime_state.values()), "installationRequested": install,
        "gameplayVerified": False, "controllerVerified": False, "firmwareProvided": False}
    if install and not result["ready"]:
        raise RuntimeError("Runtime verification failed; preserve package state and inspect the named package")
    return result


def find_bundle():
    manifest_path,_=runtime_manifest()
    manifest_hash=hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    packaged=Path(__file__).with_name("game-runtime-bundle").absolute()
    for candidate in (packaged,*bundle_cache_roots()):
        root=candidate/("ubuntu-26.04-amd64-"+manifest_hash[:16])
        if root.exists():
            document=validate_bundle(root,manifest_hash)
            authenticate_bundle(root,document)
            return root
    raise RuntimeError("No matching complete offline emulator bundle is available; run --prepare-bundle while online.")


def main():
    install_cancellation_handlers()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--update", action="store_true",help="Apply exact pinned runtime versions with a verified prior-package rollback snapshot")
    parser.add_argument("--offline", action="store_true",help="Install only from a previously prepared authenticated bundle")
    parser.add_argument("--prepare-bundle", action="store_true",help="Download and verify the complete pinned runtime dependency bundle")
    parser.add_argument("--bundle-output",type=Path,help="Absolute bundle-cache directory for --prepare-bundle")
    parser.add_argument("--fixture", action="store_true")
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    if args.prepare_bundle and (args.install or args.offline or args.update):
        parser.error("--prepare-bundle cannot be combined with --install, --offline, or --update")
    if args.bundle_output and not args.prepare_bundle:
        parser.error("--bundle-output requires --prepare-bundle")
    if args.update and not args.install:
        parser.error("--update requires --install")
    packages = [name for name in PACKAGES if name!="supertux"] + (["supertux"] if args.fixture else [])
    result = provision(packages, args.install,args.offline,update=args.update)
    if args.prepare_bundle:
        bundle=prepare_bundle(args.bundle_output)
        result["bundlePrepared"]=str(bundle)
        result["bundleSha256"]=hashlib.sha256((bundle/"bundle.json").read_bytes()).hexdigest()
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
