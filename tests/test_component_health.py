import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import subprocess

spec = importlib.util.spec_from_file_location("health", Path(__file__).resolve().parents[1] / "tools/component-health.py")
health = importlib.util.module_from_spec(spec)
spec.loader.exec_module(health)


class ComponentTests(unittest.TestCase):
    def test_missing_engines_and_no_hardware_is_a_report_not_exception(self):
        with tempfile.TemporaryDirectory() as folder:
            report = health.inspect(find=lambda _: None, video_root=Path(folder))
        self.assertFalse(report["captureConfigured"])
        self.assertIn("Missing", report["components"][0]["status"])
        self.assertIn("No video devices", report["components"][2]["status"])

    def test_installed_runtime_does_not_claim_working_capture(self):
        commands = []
        def run(args):
            commands.append(args)
            return True, "1.28.2"
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "video0").mkdir()
            (root / "video0/name").write_text("Fixture capture device")
            report = health.inspect(find=lambda name: "/usr/bin/" + name, run=run, video_root=root)
        self.assertFalse(report["captureConfigured"])
        self.assertEqual(report["missingElements"], [])
        self.assertIn("not tested", report["components"][1]["status"])
        self.assertIn("Fixture capture device", report["components"][2]["status"])
        self.assertTrue(all(args[0].endswith("gst-inspect-1.0") or args[0] == "dpkg-query" for args in commands))

    def test_incomplete_plugins(self):
        with tempfile.TemporaryDirectory() as folder:
            report = health.inspect(find=lambda name: name,
                                    run=lambda args: (args[-1] != "v4l2src", "version"), video_root=Path(folder))
        self.assertEqual(report["missingElements"], ["v4l2src"])

    def test_process_failure_and_timeout(self):
        for error in (FileNotFoundError(), subprocess.TimeoutExpired("probe", 2)):
            with patch.object(health.subprocess, "run", side_effect=error):
                self.assertEqual(health.probe(["probe"]), (False, ""))


if __name__ == "__main__":
    unittest.main()
