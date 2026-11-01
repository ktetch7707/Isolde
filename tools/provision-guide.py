#!/usr/bin/env python3
"""Explicit, guest-only IPTV Simple package setup; no Kodi profile writes.

Invoked only by the bounded oracle Configure guide action after full Test.
The actual isolated M3U/XMLTV channel is created by visual acceptance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import xml.etree.ElementTree as ET

PACKAGE = "kodi-pvr-iptvsimple"
VERSION = "21.11.0+ds-1"


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, timeout=kwargs.pop("timeout", 30), **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "posix" or socket.gethostname() != "isolde-test":
        raise RuntimeError("PVR fixture provisioning is restricted to Isolde-Test")
    release = Path("/etc/os-release").read_text()
    if 'ID=ubuntu' not in release or 'VERSION_ID="26.04"' not in release:
        raise RuntimeError("This PVR package manifest requires Ubuntu 26.04")
    query = subprocess.run(["dpkg-query", "-W", "-f=${Status}\n${Version}", PACKAGE], capture_output=True, text=True, timeout=10)
    before = query.stdout.splitlines()
    installed = query.returncode == 0 and before == ["install ok installed", VERSION]
    if not installed:
        policy = run("apt-cache", "policy", PACKAGE).stdout
        if "Candidate: " + VERSION not in policy:
            raise RuntimeError("The reviewed IPTV Simple package version is unavailable; preserve package state and review apt policy")
        # Existing signed Ubuntu repositories only; never add a repository,
        # upgrade Kodi, remove packages or enable a user's PVR configuration.
        run("sudo", "-n", "/usr/bin/apt-get", "install", "--no-remove", "--no-install-recommends", "-y", PACKAGE + "=" + VERSION, timeout=240)
    after = run("dpkg-query", "-W", "-f=${Status}\n${Version}", PACKAGE).stdout.splitlines()
    if after != ["install ok installed", VERSION]:
        raise RuntimeError("IPTV Simple package verification failed")
    files = run("dpkg-query", "-L", PACKAGE).stdout.splitlines()
    addon = Path("/usr/share/kodi/addons/pvr.iptvsimple/addon.xml")
    if str(addon) not in files or ET.parse(addon).getroot().get("id") != "pvr.iptvsimple":
        raise RuntimeError("The package does not expose the expected Kodi addon")
    libraries = [Path(p) for p in files if p.startswith("/usr/lib/") and "/pvr.iptvsimple/" in p and ".so." in Path(p).name and Path(p).is_file() and not Path(p).is_symlink()]
    if not libraries:
        raise RuntimeError("IPTV Simple's native binary was not found in the verified package")
    for library in libraries:
        if "not found" in run("ldd", str(library)).stdout:
            raise RuntimeError("IPTV Simple has an unresolved runtime dependency")
    receipt = {"schemaVersion": 1, "package": PACKAGE, "version": VERSION, "installedNow": not installed,
               "addonId": "pvr.iptvsimple", "addonXmlSha256": hashlib.sha256(addon.read_bytes()).hexdigest(),
               "libraries": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in libraries},
               "normalProfileModified": False, "channelCreated": False, "playbackVerified": False}
    path = args.receipt.absolute()
    if path.parent.resolve() != path.parent or path.is_symlink():
        raise RuntimeError("Unsafe provisioning receipt destination")
    fd, temporary = tempfile.mkstemp(prefix=".guide-", dir=path.parent)
    try:
        with os.fdopen(fd, "w") as output:
            json.dump(receipt, output, indent=2); output.flush(); os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)
    print(json.dumps(receipt))


if __name__ == "__main__":
    main()
