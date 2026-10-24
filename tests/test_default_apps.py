import importlib.util
import json
from pathlib import Path
import tempfile
import shutil
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("default_apps", ROOT / "tools/provision-default-apps.py")
apps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(apps)


class DefaultAppsTests(unittest.TestCase):
    def test_isolation_install_and_failed_reload_roll_back(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            with patch.object(apps,"platform"), patch.object(apps.os,"geteuid",return_value=0), patch.object(apps,"run"):
                result=apps.install_native_isolation(root)
            self.assertFalse(result["globalBwrapPolicyChanged"])
            wrapper=root/"usr/lib/isolde-native/bwrap"; profile=root/"etc/apparmor.d/isolde-native"
            previous=(wrapper.read_bytes(),profile.read_bytes())
            self.assertIn(b"flags=(unconfined)",previous[1])
            with patch.object(apps,"platform"), patch.object(apps.os,"geteuid",return_value=0), patch.object(apps,"run",side_effect=[RuntimeError("reload failed"),None]):
                with self.assertRaisesRegex(RuntimeError,"reload failed"): apps.install_native_isolation(root)
            self.assertEqual(previous,(wrapper.read_bytes(),profile.read_bytes()))
    def test_isolation_setup_requires_administrator(self):
        with patch.object(apps,"platform"),patch.object(apps.os,"geteuid",return_value=1000):
            with self.assertRaisesRegex(RuntimeError,"administrator"): apps.install_native_isolation()
    def test_native_wrapper_keeps_supplied_icon_name(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); (root/"applications").mkdir()
            (root/"applications/spotify.desktop").write_text("[Desktop Entry]\nIcon=spotify-client\nExec=never-execute\n")
            self.assertEqual(apps.native_icon("spotify", "spotify", root), "spotify-client")
    def test_steam_sign_in_belongs_to_the_client(self):
        manifest = json.loads((ROOT/"config/packages/steam/manifest.json").read_text())
        self.assertFalse(manifest["setup"]["required"])
    def test_existing_native_clients_do_not_request_privileges_or_downloads(self):
        with patch.object(apps, "platform"), patch.object(apps.os, "geteuid", return_value=1000), patch.object(apps, "native_missing", return_value=[]), patch.object(apps, "run") as execute:
            apps.system(apps.load(ROOT), delegated=True)
            execute.assert_not_called()
    def test_provider_url_cannot_inject_browser_arguments(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); (root/"config").mkdir()
            document = apps.load(ROOT)
            for app_id in document["apps"]:
                source = ROOT/"config/packages"/app_id
                shutil.copytree(source, root/"config/packages"/app_id)
            document["web"]["netflix"]["url"] = "https://www.netflix.com/ --user-data-dir=/outside"
            (root/"config/default-apps.json").write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError, "Unsafe provider URL"): apps.load(root)
    def test_atomic_output_rejects_redirected_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            outside = root/"outside"; outside.mkdir()
            (root/"redirect").symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "redirected"):
                apps.atomic(root/"redirect/changed", b"refused")
            self.assertFalse((outside/"changed").exists())
    def test_requested_set_uses_native_youtube_client_and_steam_library(self):
        document = apps.load(ROOT)
        self.assertEqual(set(document["apps"]), {"kodi", "freetube", "spotify", "cbc-gem", "prime-video", "disney-plus", "paramount-plus", "crunchyroll", "netflix", "peacock", "steam"})
        self.assertEqual(document["gamesBackend"], "steam")
        self.assertEqual(document["native"]["steam"]["arguments"], ["-gamepadui"])
        self.assertNotIn("smarttube", document["apps"])

    def test_missing_native_client_writes_nothing(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            config = base / "config"; config.mkdir()
            state = config / "installed-apps.json"
            state.write_bytes(b'{"installed":["plex"],"packageStates":{}}')
            before = state.read_bytes()
            with patch.object(apps.os, "geteuid", return_value=1000), patch.object(apps.os, "access", return_value=False):
                with self.assertRaisesRegex(RuntimeError, "incomplete"):
                    apps.activate(ROOT, apps.load(ROOT), config, base/"data/Isolde", base/"applications")
            self.assertEqual(state.read_bytes(), before)
            self.assertFalse((base/"applications").exists())

    def test_activation_preserves_existing_state_and_layout(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            config = base / "config"; config.mkdir()
            state = config / "installed-apps.json"
            state.write_text(json.dumps({"installed": ["plex"], "packageStates": {"plex": "failed"}, "failureReasons": {"plex": "Existing error"}}))
            layout = config / "home-layout.json"; layout.write_bytes(b'authored layout unchanged')
            with patch.object(apps.os, "geteuid", return_value=1000), patch.object(apps.os, "access", return_value=True), patch.object(apps, "provider_icon", return_value="/usr/share/icons/provider.ico"):
                result = apps.activate(ROOT, apps.load(ROOT), config, base/"data/Isolde", base/"applications")
            saved = json.loads(state.read_text())
            self.assertEqual(len(saved["installed"]), 12)
            self.assertEqual(saved["packageStates"]["plex"], "failed")
            self.assertEqual(saved["failureReasons"]["plex"], "Existing error")
            self.assertEqual(layout.read_bytes(), b'authored layout unchanged')
            self.assertFalse(result["subscriptionPlaybackVerified"])
            self.assertIn("-gamepadui", (base/"applications/isolde-steam.desktop").read_text())
            self.assertIn("--app=https://www.netflix.com/", (base/"applications/isolde-netflix.desktop").read_text())
            self.assertTrue((config/"default-apps-completed.json").is_file())

    def test_failed_state_commit_keeps_previous_activation(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            config = base/"config"; config.mkdir()
            state = config/"installed-apps.json"; state.write_bytes(b'{"installed":[],"packageStates":{}}')
            before = state.read_bytes()
            write = apps.atomic
            def fail_state(path, *args, **kwargs):
                if path == state: raise OSError("deterministic write failure")
                return write(path, *args, **kwargs)
            with patch.object(apps.os, "geteuid", return_value=1000), patch.object(apps.os, "access", return_value=True), patch.object(apps, "provider_icon", return_value="/usr/share/icons/provider.ico"), patch.object(apps, "atomic", side_effect=fail_state):
                with self.assertRaises(OSError): apps.activate(ROOT, apps.load(ROOT), config, base/"data/Isolde", base/"applications")
            self.assertEqual(state.read_bytes(), before)
            self.assertFalse((config/"default-apps-completed.json").exists())

    def test_native_provisioning_refuses_other_platforms(self):
        with patch.object(apps.sys, "platform", "win32"):
            with self.assertRaisesRegex(RuntimeError, "Ubuntu"): apps.platform()


if __name__ == "__main__": unittest.main()
