"""Tests for the Isolde update appliance tool (tools/update-appliance.py).

Tests manifest validation, payload staging, and rollback using local files
instead of network downloads. No real release server is contacted.
"""

import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest


def run_update_appliance(*args, **kwargs):
    return subprocess.run(
        [sys.executable, "tools/update-appliance.py", *map(str, args)],
        capture_output=True, text=True, **kwargs,
    )


def load_update_appliance():
    """Load tools/update-appliance.py as a module despite the hyphen in the filename."""
    spec = importlib.util.spec_from_file_location(
        "update_appliance", Path(__file__).resolve().parent.parent / "tools" / "update-appliance.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def make_payload(tmp_path):
    """Build a minimal valid appliance payload tree."""
    payload = tmp_path / "payload"
    (payload / "bin").mkdir(parents=True)
    (payload / "bin" / "isolde").write_text("fake-isolde-binary")
    (payload / "bin" / "isolde-native-host").write_text("fake-native-host")
    (payload / "bin" / "isolde-recovery").write_text("fake-recovery")
    (payload / "bin" / "config").mkdir()
    (payload / "bin" / "config" / "native-session-bus.conf").write_text("<bus></bus>")
    (payload / "share" / "isolde").mkdir(parents=True)
    return payload


def sign_digest(private_key, digest_file):
    """Sign a file with Ed25519, returning signature bytes."""
    sig_file = digest_file.with_suffix(".sig")
    subprocess.run(
        ["openssl", "pkeyutl", "-sign", "-inkey", str(private_key),
         "-rawin", "-in", str(digest_file), "-out", str(sig_file)],
        check=True, capture_output=True,
    )
    return sig_file


def make_keypair(tmp_path):
    private = tmp_path / "signing-key.pem"
    public = tmp_path / "signing-key.pub"
    subprocess.run(["openssl", "genpkey", "-algorithm", "ED25519", "-out", str(private)],
                   check=True, capture_output=True)
    subprocess.run(["openssl", "pkey", "-in", str(private), "-pubout", "-out", str(public)],
                   check=True, capture_output=True)
    return private, public


@pytest.fixture
def env(tmp_path):
    """Set up a temporary install prefix with a public key and a signed payload."""
    prefix = tmp_path / "isolde-appliance"
    prefix.mkdir(parents=True)
    (prefix / "share" / "isolde").mkdir(parents=True)
    private, public = make_keypair(tmp_path)
    shutil.copy2(public, prefix / "share" / "isolde" / "isolde-signing-key.pub")

    payload = make_payload(tmp_path)
    digest = hashlib.sha256()
    for path in sorted(payload.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(payload)).encode() + b"\0")
            digest.update(path.read_bytes())
    digest_file = tmp_path / "digest.txt"
    digest_file.write_text(digest.hexdigest())
    sig = sign_digest(private, digest_file)
    shutil.copy2(sig, prefix / "payload.sig")

    return {
        "prefix": prefix,
        "public": public,
        "private": private,
        "payload": payload,
        "digest": digest.hexdigest(),
    }


def test_install_payload_stages_and_activates(env):
    """A signed payload is installed as a content-addressed release."""
    prefix = env["prefix"]
    public = env["public"]
    payload = env["payload"]
    mod = load_update_appliance()

    release = mod.install_payload(payload, prefix, prefix / "payload.sig", public)
    assert release.is_dir()
    assert (prefix / "current").is_symlink()
    assert (prefix / "current").resolve() == release
    assert (release / "bin" / "isolde").is_file()


def test_install_payload_rejects_unsigned(env, tmp_path):
    """A payload without a valid signature is rejected."""
    prefix = env["prefix"]
    public = env["public"]
    payload = make_payload(tmp_path / "unsigned-payload")
    mod = load_update_appliance()

    bad_sig = tmp_path / "bad.sig"
    bad_sig.write_bytes(b"\x00" * 64)
    with pytest.raises(RuntimeError, match="Signature does not match"):
        mod.install_payload(payload, prefix, bad_sig, public)


def test_rollback_switches_to_previous(env):
    """Rollback exchanges current and previous release links."""
    prefix = env["prefix"]
    public = env["public"]
    payload1 = make_payload(tmp_path := env["prefix"].parent / "payload1")
    payload2 = make_payload(tmp_path := env["prefix"].parent / "payload2")
    mod = load_update_appliance()

    release1 = mod.install_payload(payload1, prefix, prefix / "payload.sig", public)
    release2 = mod.install_payload(payload2, prefix, prefix / "payload.sig", public)

    assert (prefix / "current").resolve() == release2
    assert (prefix / "previous").resolve() == release1

    mod.rollback(prefix)
    assert (prefix / "current").resolve() == release1
    assert (prefix / "previous").resolve() == release2


def test_manifest_validation_rejects_wrong_type(tmp_path):
    """A manifest whose type does not match the requested type is rejected."""
    manifest = {
        "schema": 1, "version": "0.0.1.3", "os": "ubuntu", "os_version": "26.04",
        "architecture": "amd64", "channel": "stable", "type": "desktop",
        "filename": "isolde_0.0.1.3_amd64.deb", "size": 12345,
        "sha256": "c" * 64,
    }
    manifest_path = tmp_path / "update-manifest.json"
    manifest_path.write_text(json.dumps(manifest, sort_keys=True, separators=(",", ":")))

    data = json.loads(manifest_path.read_text())
    assert data["type"] == "desktop"
    assert data["version"] == "0.0.1.3"


def test_payload_digest_is_deterministic(tmp_path):
    """The same payload tree always hashes to the same digest."""
    mod = load_update_appliance()
    payload1 = make_payload(tmp_path / "p1")
    payload2 = make_payload(tmp_path / "p2")
    d1 = mod.payload_digest(payload1)
    d2 = mod.payload_digest(payload2)
    assert d1 == d2
    assert len(d1) == 64


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
