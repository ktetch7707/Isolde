import copy
import importlib.util
import json
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
import sys
from unittest.mock import patch
from unittest.mock import Mock
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("android_provisioner", ROOT/"tools/provision-android.py")
android = importlib.util.module_from_spec(spec)
spec.loader.exec_module(android)
install_spec = importlib.util.spec_from_file_location("android_appliance_installer", ROOT/"tools/install-appliance.py")
installer = importlib.util.module_from_spec(install_spec)
install_spec.loader.exec_module(installer)


class AndroidProvisioningTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "linux", "Linux package ownership")
    def test_store_package_checks_hash_identity_profile_and_redirects(self):
        spec = importlib.util.spec_from_file_location("store_android_session", ROOT/"tools/android-session.py")
        session = importlib.util.module_from_spec(spec); spec.loader.exec_module(session)
        package = "org.example.demo"
        app_id = "fdroid-"+hashlib.sha256(package.encode()).hexdigest()[:24]
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            base = data/"Isolde/packages/profiles/default"/app_id/"7"
            base.mkdir(parents=True)
            apk = base/"app.apk"; apk.write_bytes(b"verified test payload")
            manifest = {"schemaVersion": 1, "kind": "fdroid", "id": app_id, "version": "7",
                        "packageName": package, "sha256": hashlib.sha256(apk.read_bytes()).hexdigest()}
            (base/"manifest.json").write_text(json.dumps(manifest))
            self.assertEqual(session.store_package(app_id, "7", "default", data)[0], manifest)
            with self.assertRaises(ValueError): session.store_package(app_id, "../7", "default", data)
            with self.assertRaises(ValueError): session.store_package(app_id, "7", "other", data)
            apk.write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "verification"): session.store_package(app_id, "7", "default", data)
            apk.unlink(); apk.symlink_to(base/"manifest.json")
            with self.assertRaisesRegex(ValueError, "unsafe"): session.store_package(app_id, "7", "default", data)

    def test_selector_repair_closes_on_success_and_exception_and_is_repeatable(self):
        before=b'''def foreground_pipe(args, cmd):
    process = args
    sel = selectors.DefaultSelector()
    sel.register(process.stdout, selectors.EVENT_READ)
    if cmd:
        raise RuntimeError("operation failed")
    return (process.returncode, "")
'''
        after=android.selector_cleanup_source(before)
        self.assertEqual(android.selector_cleanup_source(after),after)
        selector=Mock();factory=Mock(return_value=selector)
        scope={"selectors":SimpleNamespace(DefaultSelector=factory,EVENT_READ=1)}
        exec(compile(after,"test-selector-repair","exec"),scope)
        for failure in (False,True):
            stream=Mock();process=SimpleNamespace(stdout=stream,returncode=0);selector.reset_mock()
            if failure:
                with self.assertRaisesRegex(RuntimeError,"operation failed"):scope["foreground_pipe"](process,True)
            else:self.assertEqual(scope["foreground_pipe"](process,False),(0,""))
            selector.close.assert_called_once_with();stream.close.assert_called_once_with()
        selector.reset_mock();selector.register.side_effect=RuntimeError("registration failed")
        with self.assertRaisesRegex(RuntimeError,"registration failed"):scope["foreground_pipe"](process,False)
        selector.close.assert_called_once_with()
        with self.assertRaisesRegex(RuntimeError,"allocation"):android.selector_cleanup_source(before.replace(b"sel = selectors.DefaultSelector()",b"sel = another_factory()"))
        with self.assertRaisesRegex(RuntimeError,"Oversized"):android.selector_cleanup_source(b" "*(128*1024+1))

    @unittest.skipUnless(sys.platform == "linux", "Linux atomic activation with directory fsync")
    def test_activation_preserves_existing_apps_and_is_repeatable(self):
        receipt = {"configurationSha256": hashlib.sha256((ROOT/"config/android-runtime.json").read_bytes()).hexdigest()}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); config = root/"config"; config.mkdir()
            state = config/"Isolde/installed-apps.json"; state.parent.mkdir()
            original = {"installed": ["freetube"], "packageStates": {"freetube": "failed"}, "failureReasons": {"freetube": "preserve"}}
            state.write_text(json.dumps(original))
            env = {"XDG_CONFIG_HOME": str(config), "XDG_DATA_HOME": str(root/"data")}
            with patch.dict(os.environ, env), patch.object(android.os, "geteuid", return_value=1000, create=True), patch.object(android, "system_receipt", return_value=receipt):
                self.assertTrue(android.activate(ROOT)); self.assertTrue(android.activate(ROOT))
                value = json.loads(state.read_text())
                self.assertEqual(value["installed"], ["freetube", "smarttube"])
                self.assertEqual(value["failureReasons"], original["failureReasons"])
                self.assertEqual(value["packageStates"]["freetube"], "failed")
                self.assertTrue((root/"data/Isolde/packages/smarttube/32.68.0/manifest.json").is_file())
                before = state.read_bytes()
                with patch.object(android.shared, "atomic", side_effect=OSError("disk full")):
                    with self.assertRaises(OSError): android.activate(ROOT)
                self.assertEqual(state.read_bytes(), before)

    @unittest.skipUnless(sys.platform == "linux", "Linux Android profile storage")
    def test_android_runtime_refuses_wrong_profile_and_redirected_storage(self):
        spec = importlib.util.spec_from_file_location("android_session", ROOT/"tools/android-session.py")
        session = importlib.util.module_from_spec(spec); spec.loader.exec_module(session)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); config = root/"config"; (config/"Isolde").mkdir(parents=True)
            selected = config/"Isolde/profiles.json"; selected.write_text(json.dumps({"active": "default"}))
            with self.assertRaises(ValueError): session.profile_root("../other", config, root/"data")
            other = "6b30896f-2192-47f4-bc93-f71977515e95"
            with self.assertRaisesRegex(ValueError, "changed"): session.profile_root(other, config, root/"data")
            self.assertFalse((root/"data").exists())
            first = session.profile_root("default", config, root/"data")
            (first/"sentinel").write_text("private")
            selected.write_text(json.dumps({"active": other}))
            second = session.profile_root(other, config, root/"data")
            self.assertNotEqual(first, second); self.assertFalse((second/"sentinel").exists())
            self.assertEqual(first.stat().st_mode & 0o777, 0o700)
            redirected = root/"redirect"; redirected.symlink_to(root/"data", target_is_directory=True)
            with self.assertRaisesRegex(RuntimeError, "redirected"): session.profile_root(other, config, redirected)

    @unittest.skipUnless(sys.platform == "linux", "Guest-only Android probe")
    def test_asynchronous_package_completion_and_lost_ownership(self):
        probe_spec = importlib.util.spec_from_file_location("android_probe", ROOT/"tools/android-acceptance.py")
        probe = importlib.util.module_from_spec(probe_spec); probe_spec.loader.exec_module(probe)
        listings = iter(["packageName: com.android.settings", "packageName: org.smarttube.stable"])
        verify = Mock(); sleep = Mock()
        probe.await_package(lambda: next(listings), verify, clock=lambda: 0, sleep=sleep)
        self.assertEqual(verify.call_count, 2); sleep.assert_called_once_with(1)
        listing = Mock()
        with self.assertRaisesRegex(RuntimeError, "ownership lost"):
            probe.await_package(listing, Mock(side_effect=RuntimeError("ownership lost")), clock=lambda: 0)
        listing.assert_not_called()
        ticks = iter([0, 60])
        with self.assertRaisesRegex(RuntimeError, "deadline"):
            probe.await_package(lambda: "", Mock(), clock=lambda: next(ticks), sleep=Mock())

    def test_uninitialized_daemon_is_not_queried_and_active_session_is_refused(self):
        manager = Mock()
        bus = Mock(); bus.name_has_owner.return_value = True
        dbus = SimpleNamespace(SystemBus=Mock(return_value=bus), Interface=Mock(return_value=manager))
        with patch.dict(sys.modules, {"dbus": dbus}):
            android.require_idle_runtime(False)
            dbus.SystemBus.assert_not_called()
            manager.GetSession.return_value = {}
            android.require_idle_runtime(True)
            manager.GetSession.assert_called_once_with(timeout=5)
            manager.GetSession.return_value = {"state": "RUNNING", "waydroid_data": "/another/profile"}
            with self.assertRaisesRegex(RuntimeError, "Close the active Android session"):
                android.require_idle_runtime(True)

    def test_appliance_android_opt_in_and_invalid_flag_refusal(self):
        args = ["install-appliance.py", "dependencies", "--source", str(ROOT), "--without-default-apps", "--android", "--android-software-rendering"]
        with patch.object(sys, "argv", args), patch.object(sys, "platform", "linux"), patch.object(installer, "ubuntu_supported", return_value=True), patch.object(installer.os, "geteuid", return_value=0, create=True), patch.object(installer, "run") as run:
            installer.main()
            self.assertEqual(run.call_args.args, (sys.executable, str(ROOT/"tools/provision-android.py"), "--source", str(ROOT), "--software-rendering"))
        with patch.object(sys, "argv", ["installer", "dependencies", "--android-software-rendering"]), patch.object(installer, "run") as run:
            with self.assertRaises(SystemExit): installer.main()
            run.assert_not_called()

    def test_pinned_configuration_and_rejection_boundaries(self):
        original = android.load(ROOT)
        self.assertEqual(original["apps"]["smarttube"]["packageName"], "org.smarttube.stable")
        changes = [lambda d: d.update(schemaVersion=2),
                   lambda d: d.update(systemType="GAPPS"),
                   lambda d: d.update(runtimeVersion="1; touch /tmp/injected"),
                   lambda d: d["repository"].update(url="https://example.invalid/"),
                   lambda d: d["repository"].update(suite="../noble"),
                   lambda d: d["apps"]["smarttube"].update(packageName="foreign.package"),
                   lambda d: d["apps"]["smarttube"].update(url="https://example.invalid/app.apk"),
                   lambda d: d["apps"]["smarttube"].update(sha256=""),
                   lambda d: d["apps"]["smarttube"].update(abi="arm64-v8a")]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory); (source/"config").mkdir()
            for change in changes:
                data = copy.deepcopy(original); change(data)
                (source/"config/android-runtime.json").write_text(json.dumps(data))
                with self.assertRaises(ValueError): android.load(source)

    def test_checksum_failure_removes_only_new_download(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)/"app.apk"
            def download(url, path, limit):
                path.write_bytes(b"bad download")
                return "0"*64
            with patch.object(android.shared, "download", side_effect=download) as fetch:
                with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                    android.verified_download("https://example.invalid/", target, "1"*64, 128)
                self.assertFalse(target.exists())
                target.write_bytes(b"existing saved package")
                with self.assertRaisesRegex(ValueError, "must be new"):
                    android.verified_download("https://example.invalid/", target, "1"*64, 128)
                self.assertEqual(target.read_bytes(), b"existing saved package")
                self.assertEqual(fetch.call_count, 1)

    def test_platform_failure_precedes_any_system_change(self):
        with patch.object(android.shared, "platform", side_effect=RuntimeError("unsupported platform")), patch.object(android.shared, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "unsupported platform"): android.system(ROOT)
            run.assert_not_called()

    @unittest.skipUnless(__import__("sys").platform == "linux", "Linux atomic writer")
    def test_rendering_configuration_rolls_back_and_repeats_without_upgrade(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"waydroid.cfg"
            before = b"[waydroid]\narch=x86_64\n[properties]\ncustom.setting=preserve\n"
            path.write_bytes(before)
            path.chmod(0o644) # Waydroid's unprivileged app client must read this metadata.
            with patch.object(android.shared, "run", side_effect=RuntimeError("regeneration failed")):
                with self.assertRaisesRegex(RuntimeError, "regeneration failed"): android.configure_software_rendering(path)
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(path.stat().st_mode & 0o777, 0o644)
            with patch.object(android.shared, "run") as run:
                self.assertTrue(android.configure_software_rendering(path))
                saved = path.read_bytes()
                self.assertFalse(android.configure_software_rendering(path))
                self.assertEqual(path.read_bytes(), saved)
                self.assertEqual(path.stat().st_mode & 0o777, 0o644)
                self.assertEqual(run.call_count, 1)
                self.assertIn(b"custom.setting = preserve", saved)


if __name__ == "__main__": unittest.main()
