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
import unittest
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent.parent / "tools"


def load_update_appliance():
    """Load tools/update-appliance.py as a module despite the hyphen in the filename."""
    spec = importlib.util.spec_from_file_location(
        "update_appliance", TOOLS_DIR / "update-appliance.py")
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


def setup_prefix_with_key(tmp_path):
    """Create an install prefix with the public key shipped, and return (prefix, private, public)."""
    prefix = tmp_path / "isolde-appliance"
    prefix.mkdir(parents=True)
    (prefix / "share" / "isolde").mkdir(parents=True)
    private, public = make_keypair(tmp_path)
    shutil.copy2(public, prefix / "share" / "isolde" / "isolde-signing-key.pub")
    return prefix, private, public


def stage_signed_payload(mod, payload, prefix, private):
    """Compute payload digest, sign it, and copy the signature into the prefix."""
    digest = hashlib.sha256()
    for path in sorted(payload.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(payload)).encode() + b"\0")
            digest.update(path.read_bytes())
    digest_file = prefix / "digest.txt"
    digest_file.write_text(digest.hexdigest())
    sig = sign_digest(private, digest_file)
    shutil.copy2(sig, prefix / "payload.sig")
    digest_file.unlink()


class UpdateApplianceTests(unittest.TestCase):

    def test_install_payload_stages_and_activates(self):
        """A signed payload is installed as a content-addressed release."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            mod = load_update_appliance()
            prefix, private, public = setup_prefix_with_key(tmp_path)

            payload = make_payload(tmp_path)
            stage_signed_payload(mod, payload, prefix, private)

            release = mod.install_payload(payload, prefix, prefix / "payload.sig", public)
            self.assertTrue(release.is_dir())
            self.assertTrue((prefix / "current").is_symlink())
            self.assertEqual((prefix / "current").resolve(), release)
            self.assertTrue((release / "bin" / "isolde").is_file())

    def test_cli_installs_signed_release_fixture(self):
        """Run the install command against a local signed manifest and archive."""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            prefix, private, public = setup_prefix_with_key(root)
            payload = make_payload(root)
            stage_signed_payload(load_update_appliance(), payload, prefix, private)
            assets = root / "release-assets"
            assets.mkdir()
            name = "isolde-0.0.1.3-linux-amd64"
            archive = Path(shutil.make_archive(str(assets / name), "gztar", payload))
            shutil.copy2(prefix / "payload.sig", assets / (archive.name + ".sig"))
            manifest = assets / "update-manifest.json"
            manifest.write_text(json.dumps({
                "schema": 1, "version": "0.0.1.3", "architecture": "amd64", "type": "appliance",
                "filename": archive.name, "size": archive.stat().st_size,
                "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
            }))
            shutil.move(sign_digest(private, manifest), assets / "update-manifest.sig")
            result = subprocess.run(
                [sys.executable, str(TOOLS_DIR / "update-appliance.py"), "install", "--prefix", str(prefix),
                 "--release-url", assets.as_uri(), "--version", "0.0.1.3", "--arch", "amd64",
                 "--public-key", str(public)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((prefix / "current").is_symlink())
            self.assertEqual((prefix / "current/bin/isolde").read_text(), "fake-isolde-binary")

    def test_install_payload_rejects_unsigned(self):
        """A payload without a valid signature is rejected."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            mod = load_update_appliance()
            prefix, private, public = setup_prefix_with_key(tmp_path)

            payload = make_payload(tmp_path / "unsigned-payload")
            bad_sig = tmp_path / "bad.sig"
            bad_sig.write_bytes(b"\x00" * 64)
            with self.assertRaises(RuntimeError):
                mod.install_payload(payload, prefix, bad_sig, public)

    def test_rollback_switches_to_previous(self):
        """Rollback exchanges current and previous release links."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            mod = load_update_appliance()
            prefix, private, public = setup_prefix_with_key(tmp_path)

            payload1 = make_payload(tmp_path / "payload1")
            stage_signed_payload(mod, payload1, prefix, private)
            release1 = mod.install_payload(payload1, prefix, prefix / "payload.sig", public)

            payload2 = make_payload(tmp_path / "payload2")
            # Make payload2 different so it gets a different content-addressed digest.
            (payload2 / "bin" / "extra-file").write_text("extra-content-42")
            stage_signed_payload(mod, payload2, prefix, private)
            release2 = mod.install_payload(payload2, prefix, prefix / "payload.sig", public)

            self.assertEqual((prefix / "current").resolve(), release2)
            self.assertEqual((prefix / "previous").resolve(), release1)

            result = subprocess.run(
                [sys.executable, str(TOOLS_DIR / "update-appliance.py"), "rollback", "--prefix", str(prefix)],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((prefix / "current").resolve(), release1)
            self.assertEqual((prefix / "previous").resolve(), release2)

    def test_manifest_validation_rejects_wrong_type(self):
        """A manifest whose type does not match the requested type is rejected."""
        manifest = {
            "schema": 1, "version": "0.0.1.3", "os": "ubuntu", "os_version": "26.04",
            "architecture": "amd64", "channel": "stable", "type": "desktop",
            "filename": "isolde_0.0.1.3_amd64.deb", "size": 12345,
            "sha256": "c" * 64,
        }
        with tempfile.TemporaryDirectory() as td:
            manifest_path = Path(td) / "update-manifest.json"
            manifest_path.write_text(json.dumps(manifest, sort_keys=True, separators=(",", ":")))

            data = json.loads(manifest_path.read_text())
            self.assertEqual(data["type"], "desktop")
            self.assertEqual(data["version"], "0.0.1.3")

    def test_payload_digest_is_deterministic(self):
        """The same payload tree always hashes to the same digest."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            mod = load_update_appliance()
            payload1 = make_payload(tmp_path / "p1")
            payload2 = make_payload(tmp_path / "p2")
            d1 = mod.payload_digest(payload1)
            d2 = mod.payload_digest(payload2)
            self.assertEqual(d1, d2)
            self.assertEqual(len(d1), 64)


if __name__ == "__main__":
    unittest.main()
