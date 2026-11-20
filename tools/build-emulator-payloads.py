#!/usr/bin/env python3
"""Package locked upstream Linux runtimes for Isolde's signed offline bundle.

Guest/appliance provisioning only. Downloads never execute, and package trees
contain no maintainer scripts. AppImages are unpacked without FUSE or executing
their runtime. The caller supplies the existing bounded/cancelable runner.
"""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import tarfile
import tempfile
import time
import urllib.request
import zipfile

IDENTITIES = {"rpcs3", "shadps4", "vita3k", "xemu", "xenia", "cemu"}
MAX_EXPANDED = 2 * 1024**3
MAX_FILES = 32768


def locks():
    path = Path(__file__).with_name("game-runtime-artifacts.json")
    if not path.exists():
        path = Path(__file__).parents[1]/"config/game-runtime-artifacts.json"
    if path.is_symlink() or path.stat().st_size > 65536:
        raise ValueError("Invalid runtime artifact lock")
    document = json.loads(path.read_text(encoding="utf-8-sig"))
    if (not isinstance(document,dict) or set(document) != {"schemaVersion", "target", "runtimes"}
            or type(document["schemaVersion"]) is not int or document["schemaVersion"] != 1
            or document["target"] != "ubuntu-26.04-amd64"
            or not isinstance(document["runtimes"],dict) or set(document["runtimes"]) != IDENTITIES):
        raise ValueError("Unsupported runtime artifact lock")
    keys = {"package", "version", "url", "size", "sha256", "release", "source",
            "license", "licenseUrl", "licenseSha256", "format", "member"}
    for identity, row in document["runtimes"].items():
        if (not isinstance(row,dict) or set(row) != keys or row["package"] != "isolde-"+identity
                or not isinstance(row["version"],str) or not re.fullmatch(r"[A-Za-z0-9.+~_-]{1,80}", row["version"])
                or type(row["size"]) is not int or not 0 < row["size"] <= 512*1024**2
                or any(not isinstance(row[k],str) or not re.fullmatch(r"[a-f0-9]{64}", row[k]) for k in ("sha256", "licenseSha256"))
                or any(not isinstance(row[k],str) for k in ("url","licenseUrl","source","release","format"))
                or row["format"] not in {"appimage", "zip-appimage", "tar-xz"}
                or not row["url"].startswith("https://github.com/")
                or "/releases/download/" not in row["url"]
                or not row["licenseUrl"].startswith("https://raw.githubusercontent.com/")
                or not row["source"].startswith("https://github.com/")
                or not row["release"].startswith("https://github.com/")):
            raise ValueError("Invalid pinned runtime artifact")
        relative(row["member"])
    return path, document["runtimes"]


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def relative(name):
    if not isinstance(name,str):raise ValueError("Unsafe path in runtime archive")
    p = PurePosixPath(name)
    if (not name or p.is_absolute() or str(p) != name
            or any(c in name for c in "\\\0\r\n") or ".." in p.parts or ":" in name):
        raise ValueError("Unsafe path in runtime archive")
    return p


def download(url, destination, expected, limit, games, exact_size=None):
    destination = Path(destination)
    if destination.is_symlink():
        raise ValueError("Redirected runtime download")
    if destination.exists():
        if ((exact_size is None or destination.stat().st_size == exact_size)
                and destination.stat().st_size <= limit and digest(destination) == expected):
            return destination
        raise ValueError("Cached upstream runtime hash mismatch; preserve it for review")
    temporary = destination.with_suffix(destination.suffix+".partial")
    if temporary.exists() or temporary.is_symlink():
        raise ValueError("Incomplete runtime download exists; preserve it for review")
    try:
        request = urllib.request.Request(url, headers={"User-Agent": "Isolde-runtime-provisioner"})
        with urllib.request.urlopen(request, timeout=30) as response, temporary.open("xb") as output:
            if not response.url.startswith("https://"):
                raise ValueError("Runtime download was redirected away from HTTPS")
            total = 0
            deadline=time.monotonic()+300
            while True:
                games.check_cancelled()
                if time.monotonic()>deadline:raise TimeoutError("Runtime download exceeded five minutes")
                block = response.read(1024*1024)
                if not block:
                    break
                total += len(block)
                if total > limit:
                    raise ValueError("Runtime download exceeds its byte budget")
                output.write(block)
            output.flush(); os.fsync(output.fileno())
        if (exact_size is not None and total != exact_size) or digest(temporary) != expected:
            raise ValueError("Upstream runtime hash or size mismatch")
        os.replace(temporary, destination)
        return destination
    finally:
        if temporary.exists():
            temporary.unlink()


def unpack_appimage(archive, destination, games):
    # A type-2 AppImage contains a squashfs superblock appended to its ELF.
    # Locate a plausible superblock, then let unsquashfs validate the format.
    with Path(archive).open("rb") as stream:
        header = stream.read(64)
        if header[:5] != b"\x7fELF\x02" or header[8:11] != b"AI\x02" or struct.unpack_from("<H",header,18)[0]!=62:
            raise ValueError("Expected a type-2 Linux AppImage")
        stream.seek(0); data = stream.read(512*1024**2+1)
    offset = 0
    while True:
        offset = data.find(b"hsqs", offset)
        if offset < 0:
            raise ValueError("AppImage squashfs payload is missing")
        if offset+96 <= len(data):
            major, minor = struct.unpack_from("<HH", data, offset+28)
            used = struct.unpack_from("<Q", data, offset+40)[0]
            if major == 4 and minor == 0 and 96 <= used <= len(data)-offset:
                break
        offset += 4
    games.run("/usr/bin/unsquashfs", "-no-progress", "-o", str(offset), "-d", str(destination), str(archive), timeout=180)


def inspect_tree(root):
    total = count = 0
    for parent, directories, files in os.walk(root, followlinks=False):
        for name in directories+files:
            path = Path(parent)/name; count += 1
            if count > MAX_FILES:
                raise ValueError("Expanded runtime has too many files")
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode):
                if not path.resolve().is_relative_to(root.resolve()):
                    raise ValueError("Runtime payload contains an escaping symbolic link")
            elif stat.S_ISREG(mode):
                total += path.stat().st_size
                if total > MAX_EXPANDED:
                    raise ValueError("Expanded runtime exceeds its storage budget")
                path.chmod(mode & 0o755)  # Never ship setuid/setgid or writable code.
            elif not stat.S_ISDIR(mode):
                raise ValueError("Runtime payload contains a special file")


def launcher_source(installed,entry,payload_hash,package):
    return ("#!/usr/bin/python3\n"
        "import hashlib,json,os,sys\nfrom pathlib import Path\n"
        "def fail(): raise SystemExit('Emulator setup: Runtime files changed. Repair emulators in Isolde.')\n"
        "if os.environ.get('ISOLDE_NATIVE_NAMESPACE')!='1': raise SystemExit('Open this runtime through Isolde Games')\n"
        "root=Path("+repr(installed)+")\n"
        "manifest=Path("+repr("/usr/share/doc/"+package+"/payload.json")+")\n"
        "if root.is_symlink() or root.resolve()!=root or manifest.is_symlink() or manifest.stat().st_size>8*1024**2: fail()\n"
        "data=manifest.read_bytes()\n"
        "if hashlib.sha256(data).hexdigest()!="+repr(payload_hash)+": fail()\n"
        "seen=set();total=0\n"
        "for name,kind,value in json.loads(data):\n"
        " path=root/name;seen.add(name)\n"
        " if not path.parent.resolve().is_relative_to(root): fail()\n"
        " if kind=='link':\n"
        "  if not path.is_symlink() or os.readlink(path)!=value or not path.resolve().is_relative_to(root): fail()\n"
        " else:\n"
        "  if path.is_symlink() or not path.is_file(): fail()\n"
        "  total+=path.stat().st_size\n"
        "  if total>2*1024**3: fail()\n"
        "  with path.open('rb') as stream:\n"
        "   if hashlib.file_digest(stream,'sha256').hexdigest()!=value: fail()\n"
        "actual=set();count=0\n"
        "for parent,dirs,files in os.walk(root,followlinks=False):\n"
        " for name in dirs+files:\n"
        "  count+=1;path=Path(parent)/name\n"
        "  if count>32768: fail()\n"
        "  if path.is_file() or path.is_symlink(): actual.add(path.relative_to(root).as_posix())\n"
        "  elif not path.is_dir(): fail()\n"
        "if actual!=seen: fail()\n"
        "os.environ['APPDIR']="+repr(installed)+"\n"
        "os.chdir(os.environ['HOME'])\n"
        "os.execv("+repr(installed+"/"+entry)+",["+repr(installed+"/"+entry)+"]+sys.argv[1:])\n")


def build(identity, games):
    games.platform()
    _, rows = locks()
    if identity not in rows:
        raise ValueError("Unknown upstream runtime")
    row = rows[identity]
    cache = Path.home()/".cache/isolde-emulators"/(identity+"-"+row["sha256"][:16])
    if cache.resolve() != cache or cache.is_symlink():
        raise ValueError("Redirected runtime packaging cache")
    cache.mkdir(parents=True, exist_ok=True)
    package = cache/(row["package"]+"_"+row["version"]+"_amd64.deb")
    receipt = cache/"payload.json"
    if package.is_symlink() or receipt.is_symlink() or (receipt.exists() and receipt.stat().st_size>65536):
        raise ValueError("Redirected or invalid runtime packaging receipt")
    if package.exists() and receipt.exists():
        record = json.loads(receipt.read_text())
        if record != {"artifactSha256": row["sha256"], "packageSha256": digest(package)}:
            raise ValueError("Runtime package receipt mismatch")
        # Recreate local debs from their locked source archive. A writable cache
        # receipt alone is not authority to execute a cached package as root.
    if shutil.disk_usage(cache).free < 2*1024**3:
        raise RuntimeError("At least 2 GiB free space is required to package a runtime")
    archive = download(row["url"], cache/"upstream.archive", row["sha256"], row["size"], games, row["size"])
    license_file = download(row["licenseUrl"], cache/"copyright", row["licenseSha256"], 1024*1024, games)
    with tempfile.TemporaryDirectory(prefix="package-", dir=cache) as temporary:
        tree = Path(temporary); root = tree/"usr/lib/isolde-emulators"/identity
        root.parent.mkdir(parents=True)
        if row["format"] == "zip-appimage":
            with zipfile.ZipFile(archive) as z:
                if z.namelist() != [row["member"]] or z.getinfo(row["member"]).file_size > 512*1024**2:
                    raise ValueError("Unexpected upstream AppImage archive members")
                image = Path(temporary)/"upstream.AppImage"
                with z.open(row["member"]) as source, image.open("xb") as output:
                    shutil.copyfileobj(source, output)
            unpack_appimage(image, root, games); image.unlink()
        elif row["format"] == "appimage":
            unpack_appimage(archive, root, games)
        else:
            root.mkdir()
            with tarfile.open(archive, "r:xz") as t:
                members = t.getmembers()
                if len(members)!=2 or {m.name for m in members} != {row["member"], "LICENSE"}:
                    raise ValueError("Unexpected upstream runtime archive members")
                for member in members:
                    relative(member.name)
                    if not member.isfile() or member.size > MAX_EXPANDED:
                        raise ValueError("Unsafe upstream runtime archive entry")
                    destination = root/("runtime" if member.name == row["member"] else "LICENSE")
                    with t.extractfile(member) as source, destination.open("xb") as output:
                        shutil.copyfileobj(source, output)
                    destination.chmod(0o755 if destination.name == "runtime" else 0o644)
        inspect_tree(root)
        entry = "runtime" if row["format"] == "tar-xz" else "AppRun"
        if not (root/entry).is_file():
            raise ValueError("Upstream runtime entry point is missing")
        wrapper = tree/"usr/bin"/("isolde-"+identity); wrapper.parent.mkdir(parents=True)
        installed = "/usr/lib/isolde-emulators/"+identity
        notices = tree/"usr/share/doc"/row["package"]; notices.mkdir(parents=True)
        shutil.copyfile(license_file, notices/"copyright")
        (notices/"upstream.json").write_text(json.dumps(row, indent=2))
        files=[]
        for parent,directories,names in os.walk(root,followlinks=False):
            for name in sorted(directories+names):
                path=Path(parent)/name;relative_name=path.relative_to(root).as_posix()
                if path.is_symlink():files.append([relative_name,"link",os.readlink(path)])
                elif path.is_file():files.append([relative_name,"file",digest(path)])
        payload_manifest=notices/"payload.json"
        payload_manifest.write_text(json.dumps(sorted(files),separators=(",",":")))
        wrapper.write_text(launcher_source(installed,entry,digest(payload_manifest),row["package"]))
        wrapper.chmod(0o755)
        dependencies = "python3, libc6, libstdc++6, libgl1, libegl1, libvulkan1, libudev1, libdbus-1-3, libx11-6, libx11-xcb1, libfontconfig1, liblz4-1, libxrandr2, libxcursor1, libxi6, libxinerama1, libxkbcommon0, libpulse0, libasound2t64, libsdl2-2.0-0, libsdl3-0, libgtk-3-0t64, libgdk-pixbuf-2.0-0, libxcb-cursor0, libxcb-icccm4, libxcb-keysyms1, libxcb-render-util0, libxcb-xinerama0"
        (tree/"DEBIAN").mkdir()
        (tree/"DEBIAN/control").write_text("Package: "+row["package"]+"\nVersion: "+row["version"]
            +"\nArchitecture: amd64\nMaintainer: Isolde\nX-Isolde-Artifact-Sha256: "+row["sha256"]
            +"\nDescription: Isolde pinned "+identity+" runtime; no games, accounts or firmware\nDepends: "+dependencies+"\n")
        games.run("dpkg-deb", "--build", "--root-owner-group", str(tree), str(package), timeout=180)
    games.atomic_receipt(receipt, {"artifactSha256": row["sha256"], "packageSha256": digest(package)})
    return package
