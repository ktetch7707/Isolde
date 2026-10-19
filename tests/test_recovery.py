import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("recovery", Path(__file__).resolve().parents[1] / "tools/recovery-controller.py")
recovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recovery)


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.prefix = Path(self.temp.name)
        for name in ("a", "b"):
            binary = self.prefix / "releases" / name / "bin/isolde"
            binary.parent.mkdir(parents=True)
            binary.write_text(name)
        (self.prefix / "current").symlink_to(self.prefix / "releases/b")
        (self.prefix / "previous").symlink_to(self.prefix / "releases/a")
        self.calls = []

    def control(self, *args):
        self.calls.append(args)
        return "failed" if args[0] == "show" else ""

    def test_retry_resets_rate_limit_without_switching_release(self):
        recovery.recover(self.prefix, 10, self.control)
        self.assertEqual((self.prefix / "current").resolve().name, "b")
        self.assertEqual(self.calls[-2:], [("reset-failed", "isolde.service"), ("start", "isolde.service")])

    def test_confirmed_rollback_preserves_data(self):
        sentinel = self.prefix / "profile-sentinel"
        sentinel.write_text("unchanged")
        recovery.recover(self.prefix, 11, self.control)
        self.assertEqual((self.prefix / "current").resolve().name, "a")
        self.assertEqual(sentinel.read_text(), "unchanged")

    def test_no_previous_is_not_available(self):
        (self.prefix / "previous").unlink()
        self.assertFalse(recovery.previous_available(self.prefix))
        with self.assertRaises(RuntimeError):
            recovery.recover(self.prefix, 11, self.control)
        self.assertEqual(len(self.calls), 1)

    def test_cannot_switch_under_running_app(self):
        with self.assertRaises(RuntimeError):
            recovery.recover(self.prefix, 11, lambda *args: "active")
        self.assertEqual((self.prefix / "current").resolve().name, "b")

    def test_install_lock_blocks_recovery_mutations(self):
        import fcntl
        with (self.prefix / ".install.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaises(BlockingIOError):
                recovery.recover(self.prefix, 11, self.control)
        self.assertEqual((self.prefix / "current").resolve().name, "b")


if __name__ == "__main__":
    unittest.main()
