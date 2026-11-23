#!/usr/bin/env python3
"""Download, verify, and install an Isolde appliance update.

This tool performs the trusted update path for per-user appliance installs:

1. Download the release tarball, update-manifest.json, and update-manifest.sig
   from a GitHub release.
2. Verify the manifest signature against the shipped public key.
3. Verify the tarball filename, size, and SHA-256 against the manifest.
4. Extract the tarball and verify the payload signature against the public key.
5. Stage the payload as a content-addressed release and atomically switch the
   current/previous release links.
6. Optionally enable startup for the new release.

The public key is shipped with Isolde at share/isolde/isolde-signing-key.pub
and never leaves the device. No unsigned artifact is ever activated.
"""

import argparse
import fcntl
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path


def run(*args, check=True, **kwargs):
    return subprocess.run(args, check=check, **kwargs)


def http_get(url, output):
    """Download a URL to a file, raising on failure."""
    req = urllib.request.Request(url, headers={"User-Agent": "Isolde-Update"})
    with urllib.request.urlopen(req, timeout=60) as response:
        data = response.read()
    output.write_bytes(data)
    return len(data)


def payload_digest(payload: Path) -> str:
    """Content-addressed SHA-256 of every regular file in the payload tree."""
    digest = hashlib.sha256()
    for path in sorted(payload.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(payload)).encode() + b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def verify_signature(public_key, data_file, signature_file):
    """Verify a 64-byte Ed25519 signature over raw file contents."""
    if signature_file.is_symlink() or not signature_file.is_file() or signature_file.stat().st_size != 64:
        raise RuntimeError("Signature must be a regular 64-byte file")
    if public_key.is_symlink() or not public_key.is_file():
        raise RuntimeError("Trusted signing key is missing")
    result = run(
        "openssl", "pkeyutl", "-verify", "-pubin", "-inkey", str(public_key),
        "-rawin", "-in", str(data_file), "-sigfile", str(signature_file),
        capture_output=True, text=True, timeout=30, check=False,
    )
    if result.returncode:
        raise RuntimeError("Signature does not match trusted signing key")


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


def set_link(path, target):
    with tempfile.TemporaryDirectory(dir=path.parent, prefix=".link-") as scratch:
        temporary = Path(scratch) / "link"
        temporary.symlink_to(target)
        os.replace(temporary, path)


def install_payload(payload, prefix, signature, public_key):
    """Stage and activate a verified payload. Returns the release directory."""
    for relative in ("bin/isolde", "bin/isolde-native-host", "bin/isolde-recovery",
                     "bin/config/native-session-bus.conf"):
        if not (payload / relative).is_file():
            raise RuntimeError(f"Incomplete payload: missing {relative}")
    files = sorted(payload.rglob("*"))
    if any(path.is_symlink() for path in files):
        raise RuntimeError("Release payload must not contain symlinks")
    digest = payload_digest(payload)
    with tempfile.TemporaryDirectory(dir=payload.parent, prefix=".digest-") as scratch:
        digest_file = Path(scratch) / "digest.txt"
        digest_file.write_text(digest)
        verify_signature(public_key, digest_file, signature)
    prefix.mkdir(parents=True, exist_ok=True)
    current = checked_release(prefix, "current")
    previous = checked_release(prefix, "previous")
    releases = prefix / "releases"
    releases.mkdir(exist_ok=True)
    release = releases / digest[:24]
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
        raise RuntimeError("No previous release is available.")
    set_link(prefix / "current", previous)
    set_link(prefix / "previous", current)
    return previous


def find_public_key(prefix):
    """Locate the trusted public key shipped with the installed release."""
    candidates = [
        prefix / "share" / "isolde" / "isolde-signing-key.pub",
        Path(__file__).resolve().parent / "isolde-signing-key.pub",
    ]
    for candidate in candidates:
        if candidate.is_file() and not candidate.is_symlink():
            return candidate
    raise RuntimeError("Trusted signing key not found. Cannot verify updates.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["install", "rollback"])
    parser.add_argument("--prefix", type=Path, default=Path.home() / ".local/share/isolde-appliance")
    parser.add_argument("--release-url", required=True,
                        help="GitHub release download URL base (e.g. https://github.com/ktetch7707/Isolde/releases/download/v0.0.1.3)")
    parser.add_argument("--version", required=True, help="Release version (e.g. 0.0.1.3)")
    parser.add_argument("--arch", choices=["amd64", "arm64"], required=True, help="Target architecture")
    parser.add_argument("--type", choices=["appliance", "desktop"], default="appliance",
                        help="Package type: appliance (per-user tarball) or desktop (system .deb)")
    parser.add_argument("--startup", action="store_true",
                        help="Enable graphical-login startup for the installed release")
    parser.add_argument("--public-key", type=Path, help="Trusted signing public key (default: shipped key)")
    args = parser.parse_args()

    if sys.platform != "linux":
        raise RuntimeError("This tool supports Linux only.")
    prefix = args.prefix.expanduser().absolute()
    public_key = args.public_key or find_public_key(prefix)

    if args.action == "rollback":
        if args.type != "appliance":
            raise RuntimeError("Rollback is only supported for appliance installs.")
        import fcntl
        prefix.mkdir(parents=True, exist_ok=True)
        lock = (prefix / ".install.lock").open("a")
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError("Another installation is already using this destination.")
        previous = rollback(prefix)
        print(f"Rolled back to {previous}. Restart Isolde to use it; profile data is unchanged.")
        return

    import fcntl
    prefix.mkdir(parents=True, exist_ok=True)
    lock = (prefix / ".install.lock").open("a")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError("Another installation is already using this destination.")

    version = args.version
    arch = args.arch

    with tempfile.TemporaryDirectory(prefix="isolde-update-") as directory:
        work = Path(directory)
        manifest_path = work / "update-manifest.json"
        manifest_sig_path = work / "update-manifest.sig"

        # 1. Download manifest and signature first (small files).
        http_get(f"{args.release_url}/update-manifest.json", manifest_path)
        http_get(f"{args.release_url}/update-manifest.sig", manifest_sig_path)

        # 2. Verify manifest signature against the shipped public key.
        verify_signature(public_key, manifest_path, manifest_sig_path)

        # 3. Parse manifest and validate fields.
        manifest = json.loads(manifest_path.read_text())
        if manifest.get("schema") != 1:
            raise RuntimeError("Unsupported manifest schema version")
        if manifest.get("version") != version:
            raise RuntimeError(f"Manifest version {manifest.get('version')} does not match requested {version}")
        if manifest.get("architecture") != arch:
            raise RuntimeError(f"Manifest architecture {manifest.get('architecture')} does not match {arch}")
        if manifest.get("type") != args.type:
            raise RuntimeError(f"Manifest type {manifest.get('type')} does not match requested type {args.type}")
        expected_filename = manifest.get("filename")
        expected_size = manifest.get("size")
        expected_sha256 = manifest.get("sha256")
        if not expected_filename or not expected_size or not expected_sha256:
            raise RuntimeError("Manifest missing filename, size, or sha256")

        if args.type == "appliance":
            tarball = expected_filename
            tarball_sig = f"{tarball}.sig"

            # 4. Download the tarball and verify size + hash.
            http_get(f"{args.release_url}/{tarball}", work / tarball)
            tarball_path = work / tarball
            actual_size = tarball_path.stat().st_size
            if actual_size != expected_size:
                raise RuntimeError(f"Tarball size {actual_size} does not match manifest {expected_size}")
            actual_sha256 = hashlib.sha256(tarball_path.read_bytes()).hexdigest()
            if actual_sha256 != expected_sha256:
                raise RuntimeError("Tarball SHA-256 does not match manifest")

            # 5. Download the payload signature and verify it.
            http_get(f"{args.release_url}/{tarball_sig}", work / tarball_sig)
            with tempfile.TemporaryDirectory(dir=work, prefix=".extract-") as scratch:
                extract_dir = Path(scratch) / "payload"
                shutil.unpack_archive(str(tarball_path), str(extract_dir))
                release = install_payload(extract_dir, prefix, work / tarball_sig, public_key)

            if args.startup:
                config = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config")))
                spec = importlib.util.spec_from_file_location(
                    "installer", Path(__file__).with_name("install-appliance.py"))
                installer = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(installer)
                installer.install_startup(release, config)
                installer.enable_startup()

            print(f"Installed update {version} -> {release}. Restart Isolde to use it.")

        elif args.type == "desktop":
            deb = expected_filename
            if not deb.endswith(".deb"):
                raise RuntimeError(f"Desktop manifest filename {deb} is not a .deb package")

            # 4. Download the .deb and verify size + hash.
            http_get(f"{args.release_url}/{deb}", work / deb)
            deb_path = work / deb
            actual_size = deb_path.stat().st_size
            if actual_size != expected_size:
                raise RuntimeError(f".deb size {actual_size} does not match manifest {expected_size}")
            actual_sha256 = hashlib.sha256(deb_path.read_bytes()).hexdigest()
            if actual_sha256 != expected_sha256:
                raise RuntimeError(".deb SHA-256 does not match manifest")

            # 5. Install the .deb using the package manager. Requires administrator.
            if os.geteuid() != 0:
                raise RuntimeError("Desktop package installation requires administrator privileges (sudo).")
            run("dpkg", "--install", str(deb_path), check=True)
            print(f"Installed desktop update {version} from {deb}.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError, urllib.error.URLError, json.JSONDecodeError) as error:
        print(f"update-appliance: {error}", file=sys.stderr)
        sys.exit(1)
