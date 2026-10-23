"""Oracle safety/cache/timeout regressions; stdlib only, no VM or user service needed."""
import hashlib
import importlib.util
import io
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("oracle", Path(__file__).resolve().parents[1] / "tools/vm-oracle.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)


class OracleTests(unittest.TestCase):
    def test_studio_suite_builds_only_matching_test_targets(self):
        targets, pattern = oracle.suite_selection("studio")
        self.assertEqual(targets, ["isolde_studio_quick_tests", "isolde_component_tests", "isolde_home_navigation_tests"])
        for name in ("studio_quick", "studio_selection", "studio_unit", "studio_behavior"):
            self.assertIsNotNone(re.fullmatch(pattern, name))
        self.assertIsNone(re.fullmatch(pattern, "home_component_navigation"))
        self.assertIsNone(re.fullmatch(pattern, "studio_quick_extra"))
        self.assertEqual(oracle.suite_selection("studio_quick"), (["isolde_studio_quick_tests"], "^studio_quick$"))

    def test_targeted_studio_run_cannot_be_used_as_deploy_payload(self):
        runner = oracle.Oracle(self.root, "targeted")
        (runner.run / "test-result.json").write_text(json.dumps({"passed": True, "fullSuite": False, "suite": "studio"}))
        with self.assertRaisesRegex(RuntimeError, "no passing full-suite payload"):
            runner.tested_payload()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ.get("ISOLDE_ORACLE_TEST_TMP"))
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def snapshot(self, name="src/main.cpp", member_type=None):
        run = self.root / "snapshot"
        run.mkdir()
        content = b"source bytes\n"
        with tarfile.open(run / "source.tar.gz", "w:gz") as archive:
            entry = tarfile.TarInfo(name)
            entry.size = len(content)
            if member_type:
                entry.type = member_type
                entry.linkname = "/etc/passwd"
            archive.addfile(entry, io.BytesIO(content) if not member_type else None)
        (run / "files.sha256").write_text(hashlib.sha256(content).hexdigest() + "  " + name + "\n")
        (run / "archive.sha256").write_text(oracle.digest(run / "source.tar.gz") + "  source.tar.gz\n")
        return run

    def test_archive_verifies_and_is_immutable(self):
        run = self.snapshot()
        source, entries = oracle.extract_snapshot(run)
        oracle.verify_tree(source, entries)
        with self.assertRaisesRegex(RuntimeError, "already extracted"):
            oracle.extract_snapshot(run)

    def test_corrupt_archive_never_extracts(self):
        run = self.snapshot()
        with (run / "source.tar.gz").open("ab") as output:
            output.write(b"tamper")
        with self.assertRaisesRegex(RuntimeError, "checksum"):
            oracle.extract_snapshot(run)
        self.assertFalse((run / "source").exists())

    def test_archive_traversal_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "Unsafe"):
            oracle.extract_snapshot(self.snapshot("../outside"))
        self.assertFalse((self.root / "outside").exists())

    def test_archive_symlink_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "non-regular"):
            oracle.extract_snapshot(self.snapshot(member_type=tarfile.SYMTYPE))

    def test_incremental_sync_keeps_unchanged_mtime(self):
        source, entries = oracle.extract_snapshot(self.snapshot())
        destination = self.root / "work"
        self.assertEqual(oracle.sync_workspace(source, destination, entries, {}), 1)
        first = (destination / "src/main.cpp").stat().st_mtime_ns
        self.assertEqual(oracle.sync_workspace(source, destination, entries, entries), 0)
        self.assertEqual((destination / "src/main.cpp").stat().st_mtime_ns, first)

    def test_incremental_sync_updates_only_changed_and_previously_owned_files(self):
        source, entries = oracle.extract_snapshot(self.snapshot())
        destination = self.root / "work"
        oracle.sync_workspace(source, destination, entries, {})
        (destination / "unowned.txt").write_text("keep")
        (source / "src/main.cpp").write_text("changed")
        changed = {"src/main.cpp": oracle.digest(source / "src/main.cpp")}
        self.assertEqual(oracle.sync_workspace(source, destination, changed, entries), 1)
        oracle.sync_workspace(source, destination, {}, changed)
        self.assertFalse((destination / "src/main.cpp").exists())
        self.assertEqual((destination / "unowned.txt").read_text(), "keep")

    def test_incremental_sync_refuses_unsafe_previous_manifest(self):
        with self.assertRaisesRegex(RuntimeError, "Unsafe"):
            oracle.sync_workspace(self.root, self.root / "work", {}, {"../outside": "0" * 64})

    def test_result_bundle_excludes_caches_cookies_and_payload(self):
        run = self.root / "run"
        run.mkdir()
        (run / "status.json").write_text('{"state":"failed"}')
        (run / "build.log").write_text("error")
        (run / "cookie").write_text("test-only-placeholder")
        (run / "studio-test-move-cancel.png").write_bytes(b"test fixture")
        (run / "personal-photo.png").write_bytes(b"excluded")
        (run / "source.tar.gz").write_bytes(b"not evidence")
        oracle.write_report(run)
        with tarfile.open(run / "report.tar.gz") as archive:
            self.assertEqual(set(archive.getnames()), {"status.json", "build.log", "studio-test-move-cancel.png"})

    @unittest.skipUnless(os.name == "posix", "Linux /tmp evidence paths")
    def test_visual_evidence_is_allowlisted(self):
        evidence = Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-"))
        self.addCleanup(lambda: __import__("shutil").rmtree(evidence))
        (evidence / "result.json").write_text('{"passed":true}')
        (evidence / "cache").mkdir()
        (evidence / "cache/cookie").write_text("test-only-placeholder")
        oracle.collect_evidence(self.root, "studio", {evidence})
        with tarfile.open(next(self.root.glob("evidence-studio-*.tar.gz"))) as archive:
            self.assertEqual(len(archive.getnames()), 1)
            self.assertTrue(archive.getnames()[0].endswith("result.json"))

    def test_visual_results_use_each_harness_contract(self):
        reference = dict(component_support=True, reset_verified=True,
                         settings_skin_round_trip=True, saved_skin_survives_reset=True, missing=[])
        for case in ("lounge", "pastel"):
            self.assertTrue(oracle.visual_result_passed(case, reference))
            for field in reference:
                incomplete = dict(reference)
                del incomplete[field]
                self.assertFalse(oracle.visual_result_passed(case, incomplete))
            self.assertFalse(oracle.visual_result_passed(case, {"passed": True}))
        self.assertTrue(oracle.visual_result_passed("studio", {"passed": True}))
        self.assertFalse(oracle.visual_result_passed("studio", reference))
        self.assertFalse(oracle.visual_result_passed("pages", {"passed": False}))

    def test_targeted_pass_cannot_deploy_or_render_a_stale_payload(self):
        runner = oracle.Oracle(self.root, "targeted")
        oracle.atomic_json(runner.run / "test-result.json", {"passed": True, "fullSuite": False})
        with self.assertRaisesRegex(RuntimeError, "full-suite"):
            runner.tested_payload()

    @unittest.skipUnless(os.name == "posix", "Linux process identity")
    def test_cancel_refuses_to_signal_foreign_process(self):
        runner = oracle.Oracle(self.root, "foreign")
        runner.update()
        with self.assertRaisesRegex(RuntimeError, "ownership"):
            oracle.cancel_owned(self.root, "foreign")

    def test_cancel_unknown_run_does_not_create_a_lease(self):
        self.assertFalse(oracle.cancel_owned(self.root, "unknown")["cancelled"])
        self.assertFalse((self.root / "service-lease.json").exists())

    def test_visual_failure_still_restores_service(self):
        runner = oracle.Oracle(self.root, "visual")
        before = {"ActiveState": "active"}
        with patch.object(runner, "tested_payload", return_value=self.root), \
                patch.object(runner, "service", return_value=before), \
                patch.object(runner, "links", return_value={"current": "same"}), \
                patch.object(runner, "stage", side_effect=TimeoutError("timed out")), \
                patch.object(runner, "restore", return_value=before) as restore:
            with self.assertRaises(TimeoutError):
                runner.visual("studio")
            restore.assert_called_once_with(before)
        self.assertFalse((self.root / "service-lease.json").exists())

    @unittest.skipUnless(os.name == "posix", "Linux process groups/selector pipes")
    def test_silent_process_timeout_is_bounded(self):
        runner = oracle.Oracle(self.root, "timeout")
        started = time.monotonic()
        with self.assertRaises(TimeoutError):
            runner.stage("silent", [sys.executable, "-c", "import time;time.sleep(60)"], .3)
        self.assertLess(time.monotonic() - started, 3)

    @unittest.skipUnless(os.name == "posix", "Linux process groups/selector pipes")
    def test_stage_keeps_partial_lines_and_nonzero_exit_is_not_success(self):
        runner = oracle.Oracle(self.root, "output")
        lines = []
        runner.stage("partial", [sys.executable, "-c", "print('READY',end='')"], 5, on_line=lines.append)
        self.assertEqual(lines, ["READY"])
        with self.assertRaisesRegex(RuntimeError, "exited 7"):
            runner.stage("failed", [sys.executable, "-c", "raise SystemExit(7)"], 5)

    @unittest.skipUnless(os.name == "posix", "Linux advisory lock")
    def test_concurrent_runner_fails_immediately_without_removing_lock(self):
        lock = self.root / "lock"
        with oracle.exclusive_lock(lock):
            with self.assertRaisesRegex(RuntimeError, "BUSY"):
                with oracle.exclusive_lock(lock):
                    self.fail("Second process acquired owned lock")
        self.assertTrue(lock.exists())


if __name__ == "__main__":
    unittest.main()
