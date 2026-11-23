"""Tests for the Isolde release signing tool (tools/sign-release.py).

These tests generate a throwaway Ed25519 key pair in a temporary directory,
sign payloads and manifests, and verify the signatures against the public key.
No real private key is ever committed or transmitted.
"""

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


def run_sign_release(*args):
    return subprocess.run(
        [sys.executable, "tools/sign-release.py", *map(str, args)],
        capture_output=True, text=True,
    )


def make_keypair(tmp_path):
    """Generate a throwaway Ed25519 key pair for the test."""
    private = tmp_path / "test-signing-key.pem"
    public = tmp_path / "test-signing-key.pub"
    subprocess.run(
        ["openssl", "genpkey", "-algorithm", "ED25519", "-out", str(private)],
        check=True, capture_output=True,
    )
    subprocess.run(
        ["openssl", "pkey", "-in", str(private), "-pubout", "-out", str(public)],
        check=True, capture_output=True,
    )
    return private, public


class SignReleaseTests(unittest.TestCase):

    def test_sign_and_verify_payload_digest(self):
        """Signing a payload digest produces a 64-byte signature that verifies."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            private, public = make_keypair(tmp_path)
            payload = tmp_path / "payload"
            (payload / "bin" / "isolde").parent.mkdir(parents=True)
            (payload / "bin" / "isolde").write_text("fake-binary")
            (payload / "share" / "isolde" / "isolde-signing-key.pub").parent.mkdir(parents=True)
            (payload / "share" / "isolde" / "isolde-signing-key.pub").write_bytes(public.read_bytes())

            sig = tmp_path / "payload.sig"
            result = run_sign_release("--payload", payload, "--private-key", private,
                                      "--output-signature", sig, "--public-key", public)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(sig.is_file())
            self.assertEqual(sig.stat().st_size, 64)

    def test_sign_and_verify_manifest(self):
        """Building and signing a manifest produces verifiable JSON + signature."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            private, public = make_keypair(tmp_path)
            manifest = tmp_path / "update-manifest.json"
            sig = tmp_path / "update-manifest.sig"
            result = run_sign_release(
                "--build-manifest",
                "--version", "0.0.1.3",
                "--arch", "amd64",
                "--filename", "isolde-0.0.1.3-linux-amd64.tar.gz",
                "--size", "12345",
                "--sha256", "a" * 64,
                "--type", "appliance",
                "--private-key", private,
                "--output-manifest", manifest,
                "--output-signature", sig,
                "--public-key", public,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(manifest.is_file())
            self.assertTrue(sig.is_file())
            self.assertEqual(sig.stat().st_size, 64)
            data = json.loads(manifest.read_text())
            self.assertEqual(data["version"], "0.0.1.3")
            self.assertEqual(data["architecture"], "amd64")
            self.assertEqual(data["type"], "appliance")
            self.assertEqual(data["schema"], 1)

    def test_wrong_public_key_rejects_signature(self):
        """A signature made with key A must not verify against key B."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            private_a, public_a = make_keypair(tmp_path)
            private_b = tmp_path / "b.pem"
            public_b = tmp_path / "b.pub"
            subprocess.run(["openssl", "genpkey", "-algorithm", "ED25519", "-out", str(private_b)],
                           check=True, capture_output=True)
            subprocess.run(["openssl", "pkey", "-in", str(private_b), "-pubout", "-out", str(public_b)],
                           check=True, capture_output=True)

            data = tmp_path / "data.bin"
            data.write_text("some data to sign")

            sig = tmp_path / "data.sig"
            subprocess.run(
                ["openssl", "pkeyutl", "-sign", "-inkey", str(private_a),
                 "-rawin", "-in", str(data), "-out", str(sig)],
                check=True, capture_output=True,
            )

            result = run_sign_release("--verify", "--public-key", public_b,
                                      "--data", data, "--signature", sig)
            self.assertNotEqual(result.returncode, 0)

    def test_manifest_is_deterministic(self):
        """The same manifest inputs always produce the same signed bytes."""
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            private, public = make_keypair(tmp_path)
            args = [
                "--build-manifest",
                "--version", "0.0.1.3",
                "--arch", "amd64",
                "--filename", "isolde-0.0.1.3-linux-amd64.tar.gz",
                "--size", "12345",
                "--sha256", "b" * 64,
                "--type", "appliance",
                "--private-key", private,
            ]
            r1 = run_sign_release(*args, "--output-manifest", tmp_path / "m1.json",
                                  "--output-signature", tmp_path / "s1.sig")
            r2 = run_sign_release(*args, "--output-manifest", tmp_path / "m2.json",
                                  "--output-signature", tmp_path / "s2.sig")
            self.assertEqual(r1.returncode, 0, r1.stderr)
            self.assertEqual(r2.returncode, 0, r2.stderr)
            self.assertEqual((tmp_path / "m1.json").read_bytes(),
                             (tmp_path / "m2.json").read_bytes())
            self.assertEqual((tmp_path / "s1.sig").read_bytes(),
                             (tmp_path / "s2.sig").read_bytes())


if __name__ == "__main__":
    unittest.main()
