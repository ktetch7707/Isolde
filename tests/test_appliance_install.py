import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, call
import subprocess
import socket

spec = importlib.util.spec_from_file_location("installer", Path(__file__).resolve().parents[1] / "tools/install-appliance.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)
display_spec = importlib.util.spec_from_file_location("display_waiter", Path(__file__).resolve().parents[1] / "tools/wait-for-display.py")
display_waiter = importlib.util.module_from_spec(display_spec)
display_spec.loader.exec_module(display_waiter)


class InstallationTests(unittest.TestCase):
    def test_media_engines_are_required_without_capture_hardware(self):
        for package in ("kodi", "gstreamer1.0-tools", "gstreamer1.0-plugins-base",
                        "gstreamer1.0-plugins-good", "gstreamer1.0-plugins-bad", "gstreamer1.0-libav"):
            self.assertIn(package, installer.DEPENDENCIES)
        for package in ("qt6-multimedia-dev", "qt6-serialport-dev", "qml6-module-qtmultimedia"):
            self.assertIn(package, installer.DEPENDENCIES)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.payload = self.root / "payload"
        for file in ("bin/isolde", "bin/isolde-native-host", "bin/isolde-recovery", "bin/config/native-session-bus.conf"):
            path = self.payload / file
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("first release")
        self.prefix = self.root / "installed"

    def test_idempotent_install_and_offline_rollback(self):
        first = installer.install_payload(self.payload, self.prefix)
        self.assertEqual(first, installer.install_payload(self.payload, self.prefix))
        self.assertFalse((self.prefix / "previous").exists())
        (self.payload / "bin/isolde").write_text("second release")
        second = installer.install_payload(self.payload, self.prefix)
        self.assertNotEqual(first, second)
        self.assertEqual((self.prefix / "previous").resolve(), first)
        installer.rollback(self.prefix)
        self.assertEqual((self.prefix / "current").resolve(), first)
        self.assertTrue((second / "bin/isolde").exists())

    def test_bad_payload_does_not_replace_current(self):
        first = installer.install_payload(self.payload, self.prefix)
        (self.payload / "bin/isolde-native-host").unlink()
        with self.assertRaises(RuntimeError):
            installer.install_payload(self.payload, self.prefix)
        self.assertEqual((self.prefix / "current").resolve(), first)

    def test_existing_directory_not_overwritten(self):
        (self.prefix / "current").mkdir(parents=True)
        with self.assertRaises(RuntimeError):
            installer.install_payload(self.payload, self.prefix)
        self.assertTrue((self.prefix / "current").is_dir())

    def test_external_link_rejected(self):
        self.prefix.mkdir()
        (self.prefix / "current").symlink_to(self.payload)
        with self.assertRaises(RuntimeError):
            installer.install_payload(self.payload, self.prefix)

    def test_startup_is_bounded_and_does_not_start_during_install(self):
        config = self.root / "config"
        installer.install_payload(self.payload, self.prefix)
        installer.install_startup(self.prefix, config)
        service = (config / "systemd/user/isolde.service").read_text()
        self.assertIn("StartLimitBurst=3", service)
        self.assertIn("KillMode=control-group", service)
        self.assertIn("Restart=always", service)
        self.assertIn("OnFailure=isolde-recovery.service", service)
        self.assertIn("wait-for-display.py", service)
        self.assertTrue((self.prefix / "support/wait-for-display.py").is_file())
        recovery = (config / "systemd/user/isolde-recovery.service").read_text()
        self.assertNotIn("current/bin/isolde-recovery", recovery)
        self.assertIn("Restart=no", recovery)
        self.assertTrue((config / "autostart/isolde.desktop").exists())
        self.assertEqual((self.prefix.parent / "applications/isolde.desktop").read_bytes(),(config / "autostart/isolde.desktop").read_bytes())

    def test_startup_is_enabled_and_verified_without_starting_app(self):
        with patch.object(installer, "run") as run:
            installer.enable_startup()
        self.assertEqual(run.call_args_list, [
            call("systemctl", "--user", "daemon-reload"),
            call("systemctl", "--user", "enable", "isolde.service"),
            call("systemctl", "--user", "is-enabled", "--quiet", "isolde.service"),
        ])

    def test_enablement_failure_is_not_reported_as_success(self):
        for failing_call in (2, 3):
            results = [None] * (failing_call - 1) + [subprocess.CalledProcessError(1, "systemctl")]
            with patch.object(installer, "run", side_effect=results):
                with self.assertRaises(subprocess.CalledProcessError):
                    installer.enable_startup()

    def test_display_waits_for_socket_not_just_environment(self):
        environment = {"XDG_RUNTIME_DIR": str(self.root), "WAYLAND_DISPLAY": "wayland-test"}
        self.assertFalse(display_waiter.display_ready(environment))
        path = self.root / "wayland-test"
        path.write_text("not a socket")
        self.assertFalse(display_waiter.display_ready(environment))
        path.unlink()
        with socket.socket(socket.AF_UNIX) as server:
            server.bind(str(path))
            self.assertTrue(display_waiter.display_ready(environment))

    def test_display_import_only_accepts_display_fields(self):
        result = display_waiter.import_display({"PATH": "/usr/bin"}, 'DISPLAY=:0\nWAYLAND_DISPLAY=wayland-0\nPATH=/bad\nLD_PRELOAD=/bad\n')
        self.assertEqual(result['DISPLAY'], ':0')
        self.assertEqual(result['WAYLAND_DISPLAY'], 'wayland-0')
        self.assertEqual(result['PATH'], '/usr/bin')
        self.assertNotIn('LD_PRELOAD', result)


if __name__ == "__main__":
    unittest.main()
