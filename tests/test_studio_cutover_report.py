"""Evidence-auditor contracts; synthetic records are never product acceptance."""

import copy
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


SPEC = importlib.util.spec_from_file_location(
    "studio_cutover_report", Path(__file__).parents[1] / "tools/studio-cutover-report.py"
)
reporter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(reporter)


class CutoverReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "CMakeLists.txt").write_text("project(fixture)")
        (self.root / "qml").mkdir()
        (self.root / "qml/Main.qml").write_text("Item {}")
        self.base = self.root / "evidence"
        self.base.mkdir()
        artifact = self.base / "review.txt"
        artifact.write_text("Synthetic test evidence, not an application pass")
        self.ref = {
            "path": "review.txt",
            "sha256": reporter.digest(artifact),
            "review": "Synthetic fixture for validator unit tests only",
        }
        self.captures = []
        for width, height in ((1280, 720), (1920, 1080), (3840, 2160)):
            image = self.base / f"header-{width}.png"
            image.write_bytes(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", width, height))
            self.captures.append(dict(self.ref, path=image.name, sha256=reporter.digest(image)))

    def complete(self):
        result = reporter.template(self.root)
        result["candidate"] = {"runId": "fixture", "releaseId": "fixture", "executableSha256": "a" * 64}
        result["vm"] = {"description": "synthetic", "renderer": "fixture", "cpu": "fixture", "memoryMiB": 4096}
        result["fullSuite"] = {"passed": 1, "failed": 0, "skipped": 0, "evidence": [self.ref]}
        for task in result["tasks"].values():
            task.update(
                status="passed", input="fixture", expected="fixture", observed="fixture",
                focusBefore="fixture", focusAfter="fixture", elapsedSeconds=1, actions=1,
                wrongTurns=0, helpNeeded=0, accidentalLaunches=0, unexpectedRearrangements=0, lostFocus=0,
                evidence=[self.ref] + self.captures,
            )
        result["performance"]["1280x720"].update(
            inputToVisibleMs=[50] * 100, frameIntervalsMs=[20] * 100, visibleWidgets=100,
            editableNodes=1024, longestUnacknowledgedInputMs=200, lifecycleCycles=10,
            clientsBefore=0, clientsAfter=0, historyBytesAfter=1024, historyEntriesAfter=10,
        )
        result["performance"]["1920x1080"] = copy.deepcopy(result["performance"]["1280x720"])
        result["review"] = {"agent": "unit-test", "limitations": "synthetic", "notes": "fixture only"}
        return result

    def errors(self, report):
        return reporter.audit(report, self.root, self.base)

    def test_pending_template_cannot_pass(self):
        self.assertTrue(self.errors(reporter.template(self.root)))

    def test_complete_fixture_checks_inventory(self):
        self.assertEqual(self.errors(self.complete()), [])

    def test_changed_or_added_production_source_invalidates_candidate(self):
        result = self.complete()
        (self.root / "qml/New.qml").write_text("Item {}")
        self.assertIn("candidate production source differs from frozen manifest", self.errors(result))

    def test_missing_required_task_cannot_pass(self):
        result = self.complete()
        del result["tasks"]["atomic"]
        self.assertTrue(self.errors(result))

    def test_stale_evidence_hash_cannot_pass(self):
        result = self.complete()
        (self.base / "review.txt").write_text("changed")
        self.assertTrue(any("hash mismatch" in issue for issue in self.errors(result)))

    def test_evidence_cannot_escape_report_directory(self):
        outside = self.root / "outside.txt"
        outside.write_text("outside")
        for name in ("../outside.txt", str(outside)):
            with self.subTest(name=name):
                ref = dict(self.ref, path=name, sha256=reporter.digest(outside))
                self.assertTrue(reporter.evidence_errors(self.base, [ref]))

    def test_review_required_for_each_artifact(self):
        self.assertTrue(reporter.evidence_errors(self.base, [dict(self.ref, review="")]))

    def test_missing_samples_and_nonfinite_values_are_rejected(self):
        for samples in ([], [1] * 99, [float("nan")] * 100, [float("inf")] * 100, [True] * 100):
            result = self.complete()
            result["performance"]["1280x720"]["inputToVisibleMs"] = samples
            self.assertTrue(self.errors(result))

    def test_p95_and_frame_budgets(self):
        for field, samples in (("inputToVisibleMs", [100] * 100), ("frameIntervalsMs", [34] * 100)):
            result = self.complete()
            result["performance"]["1280x720"][field] = samples
            self.assertTrue(self.errors(result))

    def test_lifecycle_bounds_and_unacknowledged_waits(self):
        for field, value in (("clientsAfter", 1), ("historyEntriesAfter", 101), ("historyBytesAfter", 32 * 1024 * 1024 + 1), ("longestUnacknowledgedInputMs", 251)):
            result = self.complete()
            result["performance"]["1280x720"][field] = value
            self.assertTrue(self.errors(result))

    def test_failed_skipped_or_absent_full_suite(self):
        for field, value in (("failed", 1), ("skipped", 1), ("passed", 0)):
            result = self.complete()
            result["fullSuite"][field] = value
            self.assertTrue(self.errors(result))

    def test_accidental_launch_or_focus_loss_requires_repair(self):
        for field in ("accidentalLaunches", "unexpectedRearrangements", "lostFocus"):
            result = self.complete()
            result["tasks"]["entry"][field] = 1
            self.assertTrue(self.errors(result))

    def test_unknown_schema_and_missing_vm_baseline(self):
        result = self.complete()
        result["schemaVersion"] = 2
        self.assertTrue(self.errors(result))
        result = self.complete()
        result["vm"] = {}
        self.assertTrue(self.errors(result))

    def test_report_checks_do_not_mutate_input(self):
        result = self.complete()
        before = copy.deepcopy(result)
        self.errors(result)
        self.assertEqual(before, result)

    def test_logs_alone_cannot_replace_reference_captures(self):
        result = self.complete()
        result["tasks"]["pastel-reset"]["evidence"] = [self.ref]
        self.assertTrue(self.errors(result))

    def test_scaling_needs_all_three_viewports(self):
        result = self.complete()
        result["tasks"]["scaling"]["evidence"] = self.captures[:2]
        self.assertTrue(self.errors(result))

    def test_both_performance_viewports_are_required(self):
        result = self.complete()
        del result["performance"]["1920x1080"]
        self.assertTrue(self.errors(result))


if __name__ == "__main__":
    unittest.main()
