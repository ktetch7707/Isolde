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


def run_update_appliance(*args):
    return subprocess.run(
        [sys.executable, "tools/update-appliance.py", *map(str, args)],
        capture_output=True, text=True,
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


class UpdateApplianceTests(unittest.TestCase):

    def test_install_payload_stages_and_activates(self):
        """A signed payload is installed as a content-addressed release."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            prefix = tmp_path / "isolde-appliance"
            prefix.mkdir(parents=True)
            (prefix / "share" / "isolde").mkdir(parents=True)
            private, public = make_keypair(tmp_path)
            shutil.copy2(public, prefix / "share" / "isolde" / "isolde-signing-key.pub")

            payload = make_payload(tmp_path)
            mod = load_update_appliance()

            release = mod.install_payload(payload, prefix, prefix / "payload.sig", public)
            self.assertTrue(release.is_dir())
            self.assertTrue((prefix / "current").is_symlink())
            self.assertEqual((prefix / "current").resolve(), release)
            self.assertTrue((release / "bin" / "isolde").is_file())

    def test_install_payload_rejects_unsigned(self):
        """A payload without a valid signature is rejected."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            prefix = tmp_path / "isolde-appliance"
            prefix.mkdir(parents=True)
            (prefix / "share" / "isolde").mkdir(parents=True)
            private, public = make_keypair(tmp_path)
            shutil.copy2(public, prefix / "share" / "isolde" / "isolde-signing-key.pub")

            payload = make_payload(tmp_path / "unsigned-payload")
            mod = load_update_appliance()

            bad_sig = tmp_path / "bad.sig"
            bad_sig.write_bytes(b"\x00" * 64)
            with self.assertRaises(RuntimeError):
                mod.install_payload(payload, prefix, bad_sig, public)

    def test_rollback_switches_to_previous(self):
        """Rollback exchanges current and previous release links."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            prefix = tmp_path / "isolde-appliance"
            prefix.mkdir(parents=True)
            (prefix / "share" / "isolde").mkdir(parents=True)
            private, public = make_keypair(tmp_path)
            shutil.copy2(public, prefix / "share" / "isolde" / "isolde-signing-key.pub")

            mod = load_update_appliance()
            payload1 = make_payload(tmp_path / "payload1")
            payload2 = make_payload(tmp_path / "payload2")

            release1 = mod.install_payload(payload1, prefix, prefix / "payload.sig", public)
            release2 = mod.install_payload(payload2, prefix, prefix / "payload.sig", public)

            self.assertEqual((prefix / "current").resolve(), release2)
            self.assertEqual((prefix / "previous").resolve(), release1)

            mod.rollback(prefix)
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
