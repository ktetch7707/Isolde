#!/usr/bin/env python3
"""Build the pinned upstream PSP runtime into a reusable offline Debian payload.

Run only during explicit appliance/guest provisioning, never on the host. The
runtime build is separate from Isolde's incremental CMake tree. No ROM/BIOS.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile

COMMIT = "fa50bb1976065c4f8b1b47af227d367fe9771555"
VERSION = "1.20.4-1"
DEPENDENCIES = ["libsdl2-dev", "libgl-dev", "libglu1-mesa-dev", "libvulkan-dev", "libpng-dev", "zlib1g-dev", "libxt-dev"]


def run(*arguments, **options):
    timeout = options.pop("timeout", 120)
    # A timed-out cmake process can leave Ninja/compiler children running.
    # Own the whole command group so a failed provisioning step releases it.
    capture = options.pop("capture_output", False)
    if capture:
        options.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with subprocess.Popen(arguments, start_new_session=True, **options) as process:
        try:
            stdout, stderr = process.communicate(timeout=timeout)
        except BaseException:
            # Also forward user cancellation (SIGINT/KeyboardInterrupt).
            try: os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError: pass
            process.communicate()
            raise
        result = subprocess.CompletedProcess(arguments, process.returncode, stdout, stderr)
        result.check_returncode()
        return result


def build(cache, provisioner=None):
    # Reuse exactly the platform and redirect guards of the games provisioner.
    if provisioner is None:
        import importlib.util
        spec=importlib.util.spec_from_file_location("games",Path(__file__).with_name("provision-games.py"))
        games=importlib.util.module_from_spec(spec);spec.loader.exec_module(games)
    else:games=provisioner
    games.platform()
    cache=Path(cache).absolute()
    expected_cache=Path.home()/".cache/isolde-emulators/ppsspp-1.20.4"
    if cache!=expected_cache:raise ValueError("Use the managed PSP build cache only")
    if cache.is_symlink() or cache.resolve()!=cache:raise ValueError("Redirected PSP build cache")
    cache.mkdir(parents=True,exist_ok=True)
    package=cache/("isolde-ppsspp_"+VERSION+"_amd64.deb")
    receipt=cache/"payload.json"
    if package.exists() and receipt.exists():
        document=json.loads(receipt.read_text())
        with package.open("rb") as stream:actual=hashlib.file_digest(stream,"sha256").hexdigest()
        if document.get("sourceCommit")!=COMMIT or document.get("sha256")!=actual:raise ValueError("PSP offline payload hash mismatch")
        return package
    required=1.5 if (cache/"source/.git").is_dir() else 3
    if shutil.disk_usage(cache).free<required*1024**3:raise RuntimeError("PSP source build needs "+str(required)+" GiB free; preserve cache and review guest space")
    run("sudo","-n","/usr/bin/apt-get","install","--no-remove","--no-install-recommends","-y",*DEPENDENCIES,timeout=300)
    source=cache/"source"
    if not source.exists():
        run("git","clone","--filter=blob:none","--no-checkout","https://github.com/hrydgard/ppsspp.git",str(source),timeout=240)
    if source.is_symlink() or source.resolve()!=source:raise ValueError("Redirected source tree")
    run("git","-C",str(source),"checkout","--detach",COMMIT)
    head=run("git","-C",str(source),"rev-parse","HEAD",capture_output=True,text=True).stdout.strip()
    if head!=COMMIT:raise ValueError("Unexpected PSP source identity")
    # The desktop SDL build does not consume macOS/iOS bundles, the libretro
    # frontend or PSP autotests. Keep preparation bounded to its dependency tree
    # and preserve completed fetches when a named network deadline is reached.
    paths=["ext","ffmpeg","assets/debugger"]
    run("git","-C",str(source),"submodule","update","--init","--recursive","--depth","1","--",*paths,timeout=600)
    submodules=run("git","-C",str(source),"submodule","status","--recursive","--",*paths,capture_output=True,text=True).stdout
    if any(line[:1] in ("-","+","U") for line in submodules.splitlines()):raise ValueError("PSP submodule identity mismatch")
    output=cache/"build"
    run("cmake","-S",str(source),"-B",str(output),"-G","Ninja","-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=/usr","-DUSING_QT_UI=OFF","-DUSE_SYSTEM_FFMPEG=OFF",
        "-DUSE_WAYLAND_WSI=OFF","-DUNITTEST=OFF","-DHEADLESS=OFF","-DOpenGL_GL_PREFERENCE=GLVND",timeout=120)
    run("cmake","--build",str(output),"--target","PPSSPPSDL","--parallel","2",timeout=1200)
    # Build only our package tree. It is never a host install or a root build.
    with tempfile.TemporaryDirectory(prefix="package-",dir=cache) as temporary:
        tree=Path(temporary)
        (tree/"usr/bin").mkdir(parents=True)
        shutil.copy2(output/"PPSSPPSDL",tree/"usr/bin/PPSSPPSDL")
        shutil.copytree(source/"assets",tree/"usr/share/ppsspp/assets")
        notices=tree/"usr/share/doc/isolde-ppsspp";notices.mkdir(parents=True)
        shutil.copy2(source/"LICENSE.TXT",notices/"copyright")
        (notices/"source.json").write_text(json.dumps({"url":"https://github.com/hrydgard/ppsspp.git","commit":COMMIT,"submodules":submodules},indent=2))
        (tree/"DEBIAN").mkdir()
        (tree/"DEBIAN/control").write_text("Package: isolde-ppsspp\nVersion: "+VERSION+"\nArchitecture: amd64\nMaintainer: Isolde\nDescription: Isolde pinned PPSSPP runtime, no games or firmware\nDepends: libsdl2-2.0-0, libgl1, libglu1-mesa, libvulkan1, libstdc++6, zlib1g\n")
        run("dpkg-deb","--build","--root-owner-group",str(tree),str(package),timeout=90)
    with package.open("rb") as stream:actual=hashlib.file_digest(stream,"sha256").hexdigest()
    games.atomic_receipt(receipt,{"schemaVersion":1,"sourceCommit":COMMIT,"submodules":submodules,"version":VERSION,"sha256":actual})
    if shutil.disk_usage(cache).free<2*1024**3:
        # Once the verified reusable payload exists, only generated compiler
        # output is expendable. Keep source, notices and package/receipt evidence.
        mounts=[Path(line.split()[4].replace("\\040"," ")) for line in Path("/proc/self/mountinfo").read_text().splitlines()]
        if output.resolve()!=output or output.parent!=cache or any(p==output or p.is_relative_to(output) for p in mounts):
            raise ValueError("Unsafe generated PSP build cleanup target")
        games.atomic_receipt(cache/"build-cleanup.json",{"target":str(output),"reason":"Verified offline payload retained; space needed for runtime installation","payloadSha256":actual})
        shutil.rmtree(output)
    return package


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--cache",type=Path,required=True)
    print(build(parser.parse_args().cache))
