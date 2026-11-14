#!/usr/bin/env python3
"""OnFailure handler. UI is separate from the main application's data/runtime."""
import argparse
import fcntl
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import sys
import os
import tempfile

spec = importlib.util.spec_from_file_location("installer", Path(__file__).with_name("install-appliance.py"))
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


def systemctl(*args):
    return subprocess.run(["systemctl", "--user", *args], check=True, text=True, capture_output=True).stdout.strip()


def release_digest(release_dir):
    digest = hashlib.sha256()
    for path in sorted(release_dir.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(release_dir)).encode() + b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def verify_release_signature(release_dir, signature_path, public_key):
    release_dir = Path(release_dir).resolve()
    signature_path = Path(signature_path).absolute()
    if signature_path.is_symlink() or not signature_path.is_file() or signature_path.stat().st_size != 64:
        raise RuntimeError("Release lacks a trusted signature")
    public_key = Path(public_key).absolute()
    if public_key.is_symlink() or not public_key.is_file():
        raise RuntimeError("Trusted signing key is missing")
    descriptor, name = tempfile.mkstemp(prefix=".isolde-recovery-digest-", dir=release_dir.parent)
    os.close(descriptor)
    digest_file = Path(name)
    try:
        digest_file.write_text(release_digest(release_dir))
        result = run("openssl", "pkeyutl", "-verify", "-pubin", "-inkey", str(public_key),
            "-rawin", "-in", str(digest_file), "-sigfile", str(signature_path),
            capture_output=True, text=True, timeout=30, check=False)
        if result.returncode:
            raise RuntimeError("Release signature does not match trusted signing key")
    finally:
        digest_file.unlink(missing_ok=True)


def run(*args, check=True, **kwargs):
    return subprocess.run(args, check=check, **kwargs)


def previous_available(prefix):
    try:
        previous = installer.checked_release(prefix, "previous")
        return previous is not None and previous != installer.checked_release(prefix, "current")
    except (RuntimeError, OSError):
        return False


def recover(prefix, choice, control=systemctl, signature=None, public_key=None):
    if choice not in (10, 11):
        raise RuntimeError("Unknown recovery action")
    # Never change a release underneath an app started elsewhere while UI was open.
    if control("show", "isolde.service", "--property=ActiveState", "--value") not in ("failed", "inactive"):
        raise RuntimeError("Isolde is already starting or running")
    with (prefix / ".install.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if choice == 11:
            if not previous_available(prefix):
                raise RuntimeError("No previous release available")
            previous = installer.checked_release(prefix, "previous")
            if signature is not None:
                verify_release_signature(previous, signature, public_key or Path(__file__).with_name("isolde-signing-key.pub"))
            installer.rollback(prefix)
        control("reset-failed", "isolde.service")
        control("start", "isolde.service")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--ui", required=True, type=Path)
    parser.add_argument("--signature", type=Path, help="Trusted release signature file (64-byte raw signature)")
    parser.add_argument("--public-key", type=Path, help="Trusted signing public key (defaults to packaged isolde-signing-key.pub)")
    args = parser.parse_args()
    # OnFailure can be queued during another successful start. Do not cover it.
    if systemctl("show", "isolde.service", "--property=ActiveState", "--value") != "failed":
        return
    failed = False
    while True:
        command = [str(args.ui)]
        if previous_available(args.prefix):
            command.append("--previous")
        if failed:
            command.append("--error")
        choice = subprocess.run(command).returncode
        if choice not in (10, 11):
            raise RuntimeError("Recovery interface exited unexpectedly; inspect the recovery service journal")
        try:
            recover(args.prefix, choice, signature=args.signature, public_key=args.public_key)
            return
        except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
            print(f"Recovery action failed: {error}", file=sys.stderr, flush=True)
            # Another successful launch should remove recovery rather than cover it.
            if systemctl("show", "isolde.service", "--property=ActiveState", "--value") == "active":
                return
            failed = True


if __name__ == "__main__":
    main()
