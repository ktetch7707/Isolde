import base64
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import ssl
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch
from contextlib import redirect_stdout
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("fdroid_store", ROOT/"tools/fdroid-store.py")
store = importlib.util.module_from_spec(spec)
spec.loader.exec_module(store)
TARGET = {"sdk": 30, "abis": ["x86", "x86_64"], "features": []}


def fixture(count=1):
    packages = {}
    for i in range(count):
        identity = f"org.example.app{i:03}"
        packages[identity] = {"metadata": {"name": {"en-US": f"App {i:03}"},
            "summary": {"en-US": "A local demo"}, "categories": ["Games"]}, "versions": {
            "a"*64: {"file": {"name": f"/{identity}_7.apk", "sha256": "a"*64, "size": 123},
                "manifest": {"versionCode": 7, "versionName": "1.7", "usesSdk": {"minSdkVersion": 21},
                             "nativecode": ["x86_64"]}}}}
    data = json.dumps({"repo": {"address": store.REPOSITORY}, "packages": packages}).encode()
    entry = {"version": 30000, "timestamp": int(time.time()*1000), "index": {"name": "/index-v2.json",
             "sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}}
    return entry, data


class FdroidBrowsingTests(unittest.TestCase):
    def test_update_check_includes_only_requested_subscriptions(self):
        entry, raw = fixture(103)
        doc=store.verified_index(entry,raw)
        packages=["org.example.app002","org.example.app099"]
        results=store.browse(doc,"",0,TARGET,packages)["results"]
        self.assertEqual([r["packageName"] for r in results],packages)
        with self.assertRaises(ValueError):store.browse(doc,"",0,TARGET,["../unsafe"])

    def test_invalid_update_subscription_is_rejected_before_network(self):
        with patch("sys.argv",["fdroid-store","--cache","unused","--packages","../unsafe"]), \
                patch.object(store,"download") as download:
            with self.assertRaises(ValueError):store.main()
            download.assert_not_called()
    def test_search_pages_and_metadata_for_installer(self):
        entry, raw = fixture(103)
        doc = store.verified_index(entry, raw)
        first = store.browse(doc, "", 0, TARGET)
        last = store.browse(doc, "", 2, TARGET)
        self.assertEqual(len(first["results"]), 50)
        self.assertTrue(first["hasNext"])
        self.assertFalse(first["hasPrevious"])
        self.assertEqual(len(last["results"]), 3)
        self.assertFalse(last["hasNext"])
        self.assertTrue(last["hasPrevious"])
        found = store.browse(doc, "APP 042", 0, TARGET)["results"]
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["storeId"], "org.example.app042")
        self.assertEqual(found[0]["versionCode"], "7")
        self.assertTrue(found[0]["compatible"])
        self.assertTrue(found[0]["metadataVerified"])
        self.assertEqual(found[0]["downloadUrl"], store.REPOSITORY+"org.example.app042_7.apk")

    def test_index_hash_and_size_mismatch_rejected(self):
        entry, raw = fixture()
        for altered in (raw+b" ", raw.replace(b"App 000", b"App 111")):
            with self.assertRaisesRegex(ValueError, "SHA-256 or size"):
                store.verified_index(entry, altered)

    def test_abi_sdk_feature_and_missing_runtime_refuse_install(self):
        entry, raw = fixture()
        doc = store.verified_index(entry, raw)
        for target in (None, {**TARGET, "sdk": 19}, {**TARGET, "abis": ["arm64-v8a"]}):
            row = store.browse(doc, "", 0, target)["results"][0]
            self.assertFalse(row["compatible"])
            self.assertTrue(row["compatibilityReason"])
        package = doc["packages"]["org.example.app000"][0]
        package["features"] = ["android.hardware.telephony"]
        self.assertFalse(store.browse(doc, "", 0, TARGET)["results"][0]["compatible"])
        package["features"] = []
        package["maxSdkVersion"] = 29
        self.assertFalse(store.browse(doc, "", 0, TARGET)["results"][0]["compatible"])

    def test_unsafe_artifacts_and_prereleases_are_not_offered(self):
        entry, raw = fixture()
        original = json.loads(raw)
        for field, value in (("name", "/../escape.apk"), ("sha256", "not-a-hash"), ("size", 129*1024**2)):
            doc = copy.deepcopy(original)
            doc["packages"]["org.example.app000"]["versions"]["a"*64]["file"][field] = value
            payload = json.dumps(doc).encode()
            descriptor = {"index": {"sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}}
            self.assertEqual(store.browse(store.verified_index(descriptor, payload), "", 0, TARGET)["total"], 0)
        doc = store.verified_index(entry, raw)
        with self.assertRaises(ValueError): store.browse(doc, "x"*129, 0, TARGET)
        with self.assertRaises(ValueError): store.browse(doc, "", -1, TARGET)

    def test_preferred_signer_and_release_safety(self):
        _, raw = fixture()
        for change in ({"releaseChannels": ["Beta"]}, {"antiFeatures": {"KnownVuln": {}}},
                       {"obbMainFile": {"name": "/expansion.obb"}}):
            doc = json.loads(raw)
            doc["packages"]["org.example.app000"]["versions"]["a"*64].update(change)
            payload = json.dumps(doc).encode()
            descriptor = {"index": {"sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}}
            self.assertEqual(store.browse(store.verified_index(descriptor, payload), "", 0, TARGET)["total"], 0)
        doc = json.loads(raw)
        doc["packages"]["org.example.app000"]["metadata"]["preferredSigner"] = "b"*64
        payload = json.dumps(doc).encode()
        descriptor = {"index": {"sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}}
        self.assertEqual(store.browse(store.verified_index(descriptor, payload), "", 0, TARGET)["total"], 0)

    def test_redirect_outside_official_repository_rejected(self):
        for url in ("http://f-droid.org/repo/entry.jar", "https://other.example/repo/entry.jar"):
            with self.assertRaises(ValueError):
                store.RepositoryRedirect().redirect_request(None, None, 302, "", {}, url)

    def test_manifest_duplicate_attributes_rejected(self):
        with self.assertRaises(ValueError): store.sections(b"Name: a\r\nName: b\r\n")
        self.assertEqual(store.sections(b"Name: abc\r\n def\r\n")[0]["name"], "abcdef")


@unittest.skipUnless(shutil.which("openssl"), "OpenSSL is supplied by appliance Android setup")
class FdroidSignatureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = tempfile.TemporaryDirectory()
        cls.root = Path(cls.scratch.name)
        cls.openssl = shutil.which("openssl")
        subprocess.run([cls.openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256", "-days", "1",
            "-subj", "/CN=Isolde test fixture", "-keyout", str(cls.root/"key.pem"), "-out", str(cls.root/"cert.pem")],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        cls.signer = hashlib.sha256(ssl.PEM_cert_to_DER_cert((cls.root/"cert.pem").read_text())).hexdigest()

    @classmethod
    def tearDownClass(cls):
        cls.scratch.cleanup()

    def signed(self, entry=None, tamper_index=False, tamper_manifest=False):
        entry = fixture()[0] if entry is None else entry
        data = json.dumps(entry).encode()
        digest = lambda b: base64.b64encode(hashlib.sha256(b).digest())
        manifest = b"Manifest-Version: 1.0\r\n\r\nName: entry.json\r\nSHA-256-Digest: "+digest(data)+b"\r\n\r\n"
        sf = b"Signature-Version: 1.0\r\nSHA-256-Digest-Manifest: "+digest(manifest)+b"\r\n\r\n"
        (self.root/"signature.sf").write_bytes(sf)
        subprocess.run([self.openssl, "smime", "-sign", "-binary", "-md", "sha256", "-in", str(self.root/"signature.sf"),
            "-signer", str(self.root/"cert.pem"), "-inkey", str(self.root/"key.pem"), "-outform", "DER",
            "-out", str(self.root/"signature.der")], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w") as jar:
            jar.writestr("entry.json", data+b" " if tamper_index else data)
            jar.writestr("META-INF/MANIFEST.MF", manifest+b" " if tamper_manifest else manifest)
            jar.writestr("META-INF/TEST.SF", sf)
            jar.writestr("META-INF/TEST.RSA", (self.root/"signature.der").read_bytes())
        return archive.getvalue()

    def test_actual_signature_and_pinned_signer(self):
        archive = self.signed()
        self.assertEqual(store.verified_entry(archive, self.signer)["index"]["name"], "/index-v2.json")
        with self.assertRaisesRegex(ValueError, "pinned certificate"):
            store.verified_entry(archive)  # A valid signature from an untrusted signer is insufficient.

    def test_tampering_with_manifest_or_index_is_rejected(self):
        for archive in (self.signed(tamper_index=True), self.signed(tamper_manifest=True)):
            with self.assertRaisesRegex(ValueError, "digest mismatch"):
                store.verified_entry(archive, self.signer)

    def test_invalid_cryptographic_signature_is_rejected(self):
        original = zipfile.ZipFile(io.BytesIO(self.signed()))
        changed = io.BytesIO()
        with zipfile.ZipFile(changed, "w") as jar:
            for name in original.namelist():
                payload = original.read(name)
                if name.endswith(".RSA"):
                    payload = payload[:-1]+bytes([payload[-1] ^ 1])
                jar.writestr(name, payload)
        with self.assertRaisesRegex(ValueError, "signature is invalid"):
            store.verified_entry(changed.getvalue(), self.signer)

    def test_stale_future_and_bad_descriptor_rejected(self):
        for stamp in (0, int((time.time()+2*86400)*1000)):
            entry = fixture()[0]; entry["timestamp"] = stamp
            with self.assertRaisesRegex(ValueError, "expired or dated"):
                store.verified_entry(self.signed(entry), self.signer)
        entry = fixture()[0]; entry["index"]["name"] = "/../other.json"
        with self.assertRaisesRegex(ValueError, "descriptor"):
            store.verified_entry(self.signed(entry), self.signer)
        entry = fixture()[0]; entry["version"] = 99999
        with self.assertRaisesRegex(ValueError, "protocol version"):
            store.verified_entry(self.signed(entry), self.signer)

    def test_profile_cache_reverified_and_corrupt_payload_refetched(self):
        entry, data = fixture()
        signed = self.signed(entry)
        cache = self.root/"profile-cache"
        original_verify = store.verified_entry
        downloads = lambda name, maximum: signed if name == "entry.jar" else data
        with patch("sys.argv", ["fdroid-store", "--cache", str(cache), "--query", "local"]), \
                patch.object(store, "runtime_target", return_value=TARGET), \
                patch.object(store, "verified_entry", side_effect=lambda raw: original_verify(raw, self.signer)):
            with patch.object(store, "download", side_effect=downloads) as network, redirect_stdout(io.StringIO()):
                store.main()
                self.assertEqual(network.call_count, 2)
            with patch.object(store, "download", side_effect=AssertionError("Unexpected external request")), redirect_stdout(io.StringIO()):
                store.main()
            (cache/"index-v2.json").write_bytes(b"corrupt")
            with patch.object(store, "download", side_effect=downloads) as network, redirect_stdout(io.StringIO()):
                store.main()
                network.assert_called_once_with("index-v2.json", store.MAX_INDEX)


if __name__ == "__main__":
    unittest.main()
