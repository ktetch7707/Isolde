#!/usr/bin/env python3
"""Sign an Isolde release payload and/or update manifest with an Ed25519 key.

The private key is never stored in the repository. It is provided by the release
maintainer through a protected channel (offline file, HSM, or CI secret).

Usage:

  # Sign a staged appliance payload digest
  python3 tools/sign-release.py --payload <payload-dir> --private-key <key.pem> \
      --output-signature <sig.bin>

  # Build and sign an update manifest JSON
  python3 tools/sign-release.py --build-manifest --version 0.0.1.3 --arch amd64 \
      --filename isolde-0.0.1.3-linux-amd64.tar.gz --size 12345 --sha256 <hex> \
      --private-key <key.pem> --output-manifest update-manifest.json \
      --output-signature update-manifest.sig

  # Verify a signature against a public key
  python3 tools/sign-release.py --verify --public-key tools/isolde-signing-key.pub \
      --data <file> --signature <sig.bin>
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(*args, check=True, **kwargs):
    return subprocess.run(args, check=check, **kwargs)


def payload_digest(payload: Path) -> str:
    """Content-addressed SHA-256 of every regular file in the payload tree."""
    digest = hashlib.sha256()
    for path in sorted(payload.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(payload)).encode() + b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def sign_raw(private_key: Path, data_file: Path) -> bytes:
    """Sign raw file contents with Ed25519. Returns the signature bytes."""
    descriptor, name = tempfile.mkstemp(prefix=".isolde-sign-", dir=data_file.parent)
    os.close(descriptor)
    sig_file = Path(name)
    try:
        result = run(
            "openssl", "pkeyutl", "-sign", "-inkey", str(private_key),
            "-rawin", "-in", str(data_file), "-out", str(sig_file),
            capture_output=True, text=True, check=False,
        )
        if result.returncode:
            raise RuntimeError(f"Signing failed: {result.stderr.strip()}")
        return sig_file.read_bytes()
    finally:
        sig_file.unlink(missing_ok=True)


def verify_raw(public_key: Path, data_file: Path, signature_file: Path) -> None:
    """Verify an Ed25519 signature over raw file contents."""
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


def canonical_manifest(manifest: dict) -> bytes:
    """Stable JSON bytes for signing: sorted keys, no whitespace variation."""
    return json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode("utf-8")


def build_manifest_dict(version, os_name, os_version, arch, channel, filename,
                        size, sha256, package_type):
    return {
        "schema": 1,
        "version": version,
        "os": os_name,
        "os_version": os_version,
        "architecture": arch,
        "channel": channel,
        "type": package_type,
        "filename": filename,
        "size": size,
        "sha256": sha256,
    }


def write_bytes_atomic(path: Path, data: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(dir=path.parent, prefix=".isolde-write-")
    os.close(descriptor)
    tmp = Path(name)
    try:
        tmp.write_bytes(data)
        os.replace(tmp, path)
    finally:
        if tmp.exists():
            tmp.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--payload", type=Path, help="Staged appliance payload directory")
    parser.add_argument("--manifest", type=Path, help="Existing update manifest JSON to sign")
    parser.add_argument("--build-manifest", action="store_true",
                        help="Build update-manifest.json from options")
    parser.add_argument("--verify", action="store_true", help="Verify a signature instead of signing")
    parser.add_argument("--private-key", type=Path, help="Ed25519 private key (never committed)")
    parser.add_argument("--public-key", type=Path, help="Ed25519 public key (for verification)")
    parser.add_argument("--data", type=Path, help="File whose signature to verify (with --verify)")
    parser.add_argument("--signature", type=Path, help="Signature file to verify (with --verify)")
    parser.add_argument("--output-signature", type=Path, help="Output signature file")
    parser.add_argument("--output-manifest", type=Path, help="Output manifest JSON (with --build-manifest)")
    parser.add_argument("--version", help="Release version (e.g. 0.0.1.3)")
    parser.add_argument("--os", default="ubuntu", help="Target OS")
    parser.add_argument("--os-version", default="26.04", help="Target OS version")
    parser.add_argument("--arch", choices=["amd64", "arm64"], help="Target architecture")
    parser.add_argument("--channel", default="stable", help="Release channel")
    parser.add_argument("--filename", help="Release artifact filename")
    parser.add_argument("--size", type=int, help="Artifact size in bytes")
    parser.add_argument("--sha256", help="Artifact SHA-256 hex digest")
    parser.add_argument("--type", choices=["appliance", "desktop"], default="appliance",
                        help="Package type")
    args = parser.parse_args()

    if args.verify:
        if not all([args.public_key, args.data, args.signature]):
            parser.error("--verify requires --public-key, --data, --signature")
        verify_raw(args.public_key, args.data, args.signature)
        print(f"Signature verified: {args.signature}")
        return

    if not args.private_key:
        parser.error("--private-key is required for signing")

    if args.payload:
        if not args.payload.is_dir():
            parser.error(f"--payload must be a directory: {args.payload}")
        digest = payload_digest(args.payload)
        descriptor, name = tempfile.mkstemp(prefix=".isolde-payload-digest-", dir=args.payload.parent)
        os.close(descriptor)
        digest_file = Path(name)
        try:
            digest_file.write_text(digest)
            signature = sign_raw(args.private_key, digest_file)
        finally:
            pass
        out_sig = args.output_signature or Path("payload.sig")
        out_sig.parent.mkdir(parents=True, exist_ok=True)
        write_bytes_atomic(out_sig, signature)
        print(f"Signed payload digest -> {out_sig} ({len(signature)} bytes)")
        if args.public_key:
            vsig_path = Path(tempfile.mktemp(prefix=".isolde-verify-payload-", dir=args.payload.parent) + ".sig")
            vsig_path.write_bytes(signature)
            try:
                verify_raw(args.public_key, digest_file, vsig_path)
                print("Payload signature verified against public key.")
            finally:
                vsig_path.unlink(missing_ok=True)
        digest_file.unlink(missing_ok=True)

    if args.build_manifest:
        missing = [n for n, v in (
            ("--version", args.version), ("--arch", args.arch),
            ("--filename", args.filename), ("--size", args.size), ("--sha256", args.sha256))
                   if v is None]
        if missing:
            parser.error(f"--build-manifest requires: {', '.join(missing)}")
        manifest = build_manifest_dict(
            args.version, args.os, args.os_version, args.arch, args.channel,
            args.filename, args.size, args.sha256, args.type,
        )
        manifest_bytes = canonical_manifest(manifest)
        descriptor, name = tempfile.mkstemp(prefix=".isolde-manifest-", dir=Path(tempfile.gettempdir()))
        os.close(descriptor)
        manifest_file = Path(name)
        try:
            manifest_file.write_bytes(manifest_bytes)
            signature = sign_raw(args.private_key, manifest_file)
        finally:
            manifest_file.unlink(missing_ok=True)
        out_manifest = args.output_manifest or Path("update-manifest.json")
        out_sig = args.output_signature or Path("update-manifest.sig")
        write_bytes_atomic(out_manifest, manifest_bytes)
        write_bytes_atomic(out_sig, signature)
        print(f"Wrote manifest -> {out_manifest}")
        print(f"Signed manifest -> {out_sig} ({len(signature)} bytes)")
        if args.public_key:
            descriptor, name = tempfile.mkstemp(prefix=".isolde-verify-manifest-", dir=out_manifest.parent)
            os.close(descriptor)
            verify_file = Path(name)
            try:
                verify_file.write_bytes(manifest_bytes)
                verify_raw(args.public_key, verify_file, out_sig)
                print("Manifest signature verified against public key.")
            finally:
                verify_file.unlink(missing_ok=True)

    if args.manifest:
        if not args.manifest.is_file():
            parser.error(f"--manifest must be a file: {args.manifest}")
        signature = sign_raw(args.private_key, args.manifest)
        out_sig = args.output_signature or Path("manifest.sig")
        out_sig.parent.mkdir(parents=True, exist_ok=True)
        write_bytes_atomic(out_sig, signature)
        print(f"Signed manifest -> {out_sig} ({len(signature)} bytes)")
        if args.public_key:
            verify_raw(args.public_key, args.manifest, out_sig)
            print("Manifest signature verified against public key.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f"sign-release: {error}", file=sys.stderr)
        sys.exit(1)
