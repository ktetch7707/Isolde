#!/usr/bin/env python3
"""Explicit Ubuntu appliance provisioning; never run on the Windows host.

System phase downloads native clients. User phase atomically registers verified
clients and private web-app launchers. No account login, game purchase or layout
rewrite is performed. Package data never contains executable commands.
"""
import argparse
import hashlib
import json
import os
import stat
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from urllib.parse import urlparse, urljoin
from html.parser import HTMLParser

LIMIT = 512 * 1024 * 1024


def run(*args, **kw):
    return subprocess.run(args, check=True, timeout=kw.pop("timeout", 600), **kw)


def atomic(path, data, mode=0o600):
    ancestor = path.parent.absolute()
    while not ancestor.exists():
        if ancestor.is_symlink(): raise ValueError("Refusing redirected output")
        ancestor = ancestor.parent
    if ancestor.resolve() != ancestor: raise ValueError("Refusing redirected output")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink(): raise ValueError("Refusing redirected output")
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=".isolde-")
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data); output.flush(); os.fsync(output.fileno())
        os.chmod(name, mode)
        os.replace(name, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try: os.fsync(directory)
        finally: os.close(directory)
    finally:
        if os.path.exists(name): os.unlink(name)


def download(url, destination, limit=LIMIT, timeout=45):
    if urlparse(url).scheme != "https": raise ValueError("HTTPS required")
    request = urllib.request.Request(url, headers={"User-Agent": "Isolde-appliance/1"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        if urlparse(response.url).scheme != "https": raise ValueError("Unsafe download redirect")
        data = response.read(limit+1)
    if not data or len(data) > limit: raise ValueError("Download exceeds its bound")
    atomic(destination, data, 0o644)
    return hashlib.sha256(data).hexdigest()


def load(source):
    path = source / "config/default-apps.json"
    if path.stat().st_size > 64*1024: raise ValueError("Oversized default application set")
    document = json.loads(path.read_text())
    if document.get("schemaVersion") != 1 or set(document) != {"schemaVersion", "apps", "gamesBackend", "native", "web"}:
        raise ValueError("Unsupported default application set")
    ids = document["apps"]
    if not isinstance(ids, list) or not 1 <= len(ids) <= 32 or len(set(ids)) != len(ids): raise ValueError("Invalid defaults")
    if set(ids) != set(document["native"]) | set(document["web"]): raise ValueError("Missing provisioner")
    if set(document["native"]) & set(document["web"]): raise ValueError("Conflicting provisioners")
    for app_id in ids:
        if not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,79}", app_id): raise ValueError("Unsafe app ID")
        manifest = source / "config/packages" / app_id / "manifest.json"
        if manifest.stat().st_size > 1024*1024: raise ValueError("Oversized package")
        package = json.loads(manifest.read_text())
        if package["packageId"] != app_id or package["app"]["id"] != app_id: raise ValueError("Package identity mismatch")
        if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){1,3}(?:-[A-Za-z0-9.-]+)?", package["version"]): raise ValueError("Unsafe package version")
        if not 1 <= len(package["name"]) <= 80 or any(ord(c) < 32 for c in package["name"]): raise ValueError("Unsafe package name")
        desktop_id = document["native"][app_id]["desktopId"] if app_id in document["native"] else "isolde-"+app_id
        if package["app"].get("desktopId") != desktop_id: raise ValueError("Provisioner does not match package launcher")
    backend = document["gamesBackend"]
    if backend not in document["native"] or json.loads((source/"config/packages"/backend/"manifest.json").read_text())["app"].get("presentationMode") != "game-hub": raise ValueError("Unsupported games backend")
    for app in document["native"].values():
        if app["provider"] not in {"ubuntu", "spotify", "freetube"}: raise ValueError("Unknown native provider")
        if app["executable"] not in {"/usr/bin/kodi", "/usr/bin/freetube", "/usr/bin/spotify", "/usr/games/steam"}: raise ValueError("Unsupported executable")
        if app.get("package") not in {None, "kodi", "steam-installer"}: raise ValueError("Unsupported OS package")
        if not re.fullmatch(r"[A-Za-z0-9._-]{1,80}", app["desktopId"]): raise ValueError("Unsafe desktop ID")
        if not isinstance(app["arguments"], list) or len(app["arguments"]) > 8 or any(not isinstance(a, str) or not re.fullmatch(r"[-A-Za-z0-9=]{1,80}", a) for a in app["arguments"]): raise ValueError("Unsafe launch argument")
    for app in document["web"].values():
        for key in ("url", "iconUrl"):
            parsed = urlparse(app[key])
            if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment or not re.fullmatch(r"https://[A-Za-z0-9.-]+/[A-Za-z0-9/._~%-]*", app[key]): raise ValueError("Unsafe provider URL")
    return document


def platform():
    if sys.platform != "linux": raise RuntimeError("Provisioning is restricted to the Ubuntu appliance")
    values = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    if values.get("ID", "").strip('"') != "ubuntu" or values.get("VERSION_ID", "").strip('"') != "26.04": raise RuntimeError("Ubuntu 26.04 required")


def install_native_isolation(root=Path("/")):
    """Explicitly approved native-runtime namespace exception (Isolde-Test)."""
    platform()
    if os.geteuid() != 0: raise RuntimeError("Native sandbox setup requires a guest administrator")
    binary = Path("/usr/bin/bwrap")
    metadata = binary.stat()
    if metadata.st_uid != 0 or metadata.st_mode & (stat.S_IWGRP | stat.S_IWOTH) or binary.is_symlink() or metadata.st_size > 16*1024*1024:
        raise RuntimeError("Use the root-owned Ubuntu bubblewrap package")
    parser = shutil.which("apparmor_parser") or "/usr/sbin/apparmor_parser"
    if not Path(parser).is_file(): raise RuntimeError("Ubuntu AppArmor parser is required")
    wrapper = root/"usr/lib/isolde-native/bwrap"
    profile = root/"etc/apparmor.d/isolde-native"
    policy = b'abi <abi/5.0>,\ninclude <tunables/global>\n\n# Isolde retains private homes, mounts and session buses.\nprofile isolde-native /usr/lib/isolde-native/bwrap flags=(unconfined) {\n  userns,\n}\n'
    for path in (wrapper, profile):
        if path.is_symlink(): raise ValueError("Refusing redirected sandbox installation")
    previous = {p: (p.read_bytes(), stat.S_IMODE(p.stat().st_mode)) if p.exists() else None for p in (wrapper, profile)}
    try:
        atomic(wrapper, binary.read_bytes(), 0o755)
        atomic(profile, policy, 0o644)
        run(parser, "-r", str(profile))
    except Exception:
        for path, saved in previous.items():
            if saved is None: path.unlink(missing_ok=True)
            else: atomic(path, saved[0], saved[1])
        if previous[profile] is not None: run(parser, "-r", str(profile))
        raise
    return {"wrapper": str(wrapper), "profile": str(profile), "globalBwrapPolicyChanged": False}


def system(document, delegated=False):
    platform()
    if os.geteuid() != 0 and not delegated: raise RuntimeError("Only the system phase requires sudo")
    if not native_missing(document):
        print("Default native clients are already present; no package downloads needed.")
        return
    apt = ["sudo", "-n", "/usr/bin/apt-get"] if delegated else ["apt-get"]
    dpkg = ["sudo", "-n", "/usr/bin/dpkg"] if delegated else ["dpkg"]
    if run("dpkg", "--print-architecture", capture_output=True, text=True).stdout.strip() != "amd64": raise RuntimeError("Native default clients currently require amd64")
    with tempfile.TemporaryDirectory(prefix="isolde-default-download-") as temporary:
        scratch = Path(temporary)
        run(*dpkg, "--add-architecture", "i386")
        run(*apt, "update")
        run(*apt, "install", "-y", "ca-certificates", "gnupg", "software-properties-common")
        if not delegated:
            run("add-apt-repository", "-y", "multiverse")
            key = scratch / "spotify.asc"
            download("https://download.spotify.com/debian/pubkey_5384CE82BA52C83A.asc", key, 1024*1024)
            fingerprint = run("gpg", "--show-keys", "--with-colons", str(key), capture_output=True, text=True).stdout
            if not any(line.startswith("fpr:") and line.split(":")[9].endswith("5384CE82BA52C83A") for line in fingerprint.splitlines()): raise RuntimeError("Unexpected Spotify signing key")
            run("gpg", "--batch", "--yes", "--dearmor", "--output", str(scratch / "spotify.gpg"), str(key))
            atomic(Path("/usr/share/keyrings/isolde-spotify.gpg"), (scratch / "spotify.gpg").read_bytes(), 0o644)
            source_files = [Path("/etc/apt/sources.list")] + list(Path("/etc/apt/sources.list.d").glob("*.list")) + list(Path("/etc/apt/sources.list.d").glob("*.sources"))
            existing = any(p.is_file() and p.stat().st_size <= 1024*1024 and "repository.spotify.com" in p.read_text() for p in source_files)
            if not existing:
                atomic(Path("/etc/apt/sources.list.d/isolde-spotify.list"), b"deb [signed-by=/usr/share/keyrings/isolde-spotify.gpg] https://repository.spotify.com stable non-free\n", 0o644)
        run(*apt, "update")
        packages = [a["package"] for a in document["native"].values() if a["provider"] == "ubuntu"]
        run(*apt, "install", "-y", *packages, "spotify-client", timeout=900)
        chrome = scratch / "google-chrome.deb"
        chrome_hash = download("https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb", chrome)
        release_path = scratch / "freetube-release.json"
        download("https://api.github.com/repos/FreeTubeApp/FreeTube/releases/tags/v0.25.3-beta", release_path, 4*1024*1024)
        release = json.loads(release_path.read_text())
        asset = next(a for a in release["assets"] if a["name"] == "freetube_0.25.3_beta_amd64.deb")
        if asset["browser_download_url"] != "https://github.com/FreeTubeApp/FreeTube/releases/download/v0.25.3-beta/freetube_0.25.3_beta_amd64.deb": raise RuntimeError("Unexpected FreeTube download source")
        expected = asset.get("digest", "")
        if not re.fullmatch(r"sha256:[a-f0-9]{64}", expected): raise RuntimeError("Upstream FreeTube checksum is missing")
        freetube = scratch / "freetube.deb"
        actual = download(asset["browser_download_url"], freetube)
        if expected != "sha256:" + actual: raise RuntimeError("FreeTube checksum mismatch")
        run(*apt, "install", "-y", str(chrome), str(freetube), timeout=900)
        audit = Path.home()/".local/share/Isolde/default-app-downloads.json" if delegated else Path("/var/lib/isolde/default-app-downloads.json")
        atomic(audit, json.dumps({"chromeSha256": chrome_hash, "freetubeSha256": actual, "freetubeUpstreamDigest": expected}).encode())


def native_missing(document):
    missing = [app["executable"] for app in document["native"].values() if not os.access(app["executable"], os.X_OK)]
    if document["web"] and not os.access("/usr/bin/google-chrome-stable", os.X_OK): missing.append("/usr/bin/google-chrome-stable")
    return missing


def provider_icon(url, destination, page_url=None):
    # Public provider artwork only. No cookies, browser profiles or account data.
    with tempfile.TemporaryDirectory(prefix="isolde-provider-icon-") as temporary:
        path = Path(temporary) / "icon"
        def fetch(candidate):
            digest = download(candidate, path, 1024*1024, timeout=8)
            content = path.read_bytes()
            suffix = ".ico" if content.startswith(b"\x00\x00\x01\x00") else ".png" if content.startswith(b"\x89PNG\r\n\x1a\n") else ".svg" if b"<svg" in content[:1024] else ""
            if not suffix: raise ValueError("Provider did not return an icon image")
            output = destination / (digest+suffix)
            atomic(output, content, 0o644)
            return str(output)
        try: return fetch(url)
        except (OSError, ValueError) as error:
            if not page_url: raise
            last_error = error
        class Links(HTMLParser):
            def __init__(self): super().__init__(); self.icons = []
            def handle_starttag(self, tag, attributes):
                values = dict(attributes)
                if tag == "link" and "icon" in values.get("rel", "") and values.get("href"):
                    self.icons.append(urljoin(page_url, values["href"]))
        page = Path(temporary)/"page"
        download(page_url, page, 1024*1024, timeout=8)
        links = Links(); links.feed(page.read_text(errors="replace"))
        for candidate in list(dict.fromkeys(links.icons))[:3]:
            try: return fetch(candidate)
            except (OSError, ValueError) as error: last_error = error
        raise ValueError("Provider artwork unavailable: " + str(last_error))


def native_icon(app_id, fallback, root=Path("/usr/share")):
    # Read supplied metadata without executing its launch command. Wrappers
    # retain vendor icon names (for example spotify-client), not guessed IDs.
    entry = root/"applications"/(app_id+".desktop")
    if entry.is_file() and entry.stat().st_size <= 65536:
        group = ""
        for line in entry.read_text().splitlines():
            if line.startswith("["): group = line.strip()
            elif group == "[Desktop Entry]" and line.startswith("Icon="):
                value = line[5:].strip()
                if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", value): return value
                path = Path(value)
                if path.is_absolute() and path.is_file() and path.resolve().is_relative_to(root.resolve()) and path.stat().st_size <= 1024*1024: return str(path)
    return fallback


def activate(source, document, config=None, data=None, applications=None):
    if os.geteuid() == 0: raise RuntimeError("Application activation must run as the regular Isolde user")
    config = config or Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home()/".config"))) / "Isolde"
    data = data or Path(os.environ.get("XDG_DATA_HOME", str(Path.home()/".local/share"))) / "Isolde"
    applications = applications or data.parent / "applications"
    # Complete preflight before publishing any launcher or activation record.
    missing = native_missing(document)
    if missing: raise RuntimeError("Default application downloads are incomplete: " + ", ".join(missing))
    state_path = config / "installed-apps.json"
    state = json.loads(state_path.read_text()) if state_path.exists() else {"installed": [], "packageStates": {}}
    if not isinstance(state.get("installed"), list) or not isinstance(state.get("packageStates", {}), dict): raise ValueError("Invalid installed state; refusing overwrite")
    icon_failures = {}
    for app_id in document["apps"]:
        manifest_path = source / "config/packages" / app_id / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        if app_id in document["native"]:
            app = document["native"][app_id]
            desktop_id, executable, arguments, icon = app["desktopId"], app["executable"], app["arguments"], app["icon"]
            icon = native_icon(app_id, icon)
        else:
            app = document["web"][app_id]
            desktop_id, executable = "isolde-"+app_id, "/usr/bin/google-chrome-stable"
            arguments = ["--no-first-run", "--start-fullscreen", "--class="+desktop_id, "--app="+app["url"]]
            icon = app_id
            try: icon = provider_icon(app["iconUrl"], data.parent / "icons/isolde", app["url"])
            except (OSError, ValueError) as error: icon_failures[app_id] = str(error)
        # These bounded arguments are fixed provisioner output, never a shell.
        command = " ".join([executable] + arguments)
        desktop = f"[Desktop Entry]\nType=Application\nName={manifest['name']}\nExec={command}\nIcon={icon}\nTerminal=false\n"
        atomic(applications / (desktop_id+".desktop"), desktop.encode(), 0o644)
        atomic(data / "packages" / app_id / manifest["version"] / "manifest.json", manifest_path.read_bytes())
        if app_id not in state["installed"]: state["installed"].append(app_id)
        state.setdefault("packageStates", {}).setdefault(app_id, "needs-setup" if manifest.get("setup", {}).get("required") else "ready")
        if state["packageStates"][app_id] == "needs-setup" and not manifest.get("setup", {}).get("required"):
            state["packageStates"][app_id] = "ready"
    atomic(state_path, json.dumps(state, indent=2).encode())
    result = {"registered": document["apps"], "gamesBackend": document["gamesBackend"], "nativeExecutablesVerified": True, "subscriptionPlaybackVerified": False, "iconsUnavailable": icon_failures}
    atomic(config / "default-apps-completed.json", json.dumps(result, indent=2).encode())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["plan", "system", "activate", "isolation"])
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--sudo-apt", action="store_true", help="Use existing delegated apt-get/dpkg privileges; repositories must already be configured")
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    document = load(source)
    if args.action == "plan": print(json.dumps(document, indent=2)); return
    platform()
    if args.action == "isolation":
        import socket
        if socket.gethostname() != "isolde-test": raise RuntimeError("This sandbox exception is authorized only in Isolde-Test")
        print(json.dumps(install_native_isolation())); return
    if args.action == "system": system(document, args.sudo_apt)
    else: print(json.dumps(activate(source, document)))


if __name__ == "__main__":
    try: main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, StopIteration) as error:
        sys.exit("Default application provisioning stopped: " + str(error))
