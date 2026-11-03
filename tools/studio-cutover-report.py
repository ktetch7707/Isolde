#!/usr/bin/env python3
"""Prepare and audit T24 evidence locally; never starts or contacts the VM.

Initialize before freezing the acceptance candidate. Fill the resulting records
from bounded-runner output and agent review. A complete report means the recorded
evidence was checked, not that this tool independently exercised the product.
"""

import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path


# Each record is a named installed task, not credit for source presence.
TASKS = {
    "blank": ("T24.1", "Blank page: add Clock/apps, move, size, preview, Apply, cold reopen"),
    "existing": ("T24.1", "Existing and multiple pages: edit, switch, cancel and preserve routes"),
    "nested": ("T24.1", "Nested scope, group selection, 32-node bound and atomic group movement"),
    "collision": ("T24.1", "Mixed-size/locked neighbors, complete reflow, blocked/full-grid recovery"),
    "cancel": ("T24.1", "Move/resize preview then Back: identical bytes and unchanged history"),
    "parity": ("T24.1", "Remote/pointer/inspector equivalent edits produce identical canonical bytes"),
    "history": ("T24.1", "Undo/redo, bounded history, named versions and reversible restore"),
    "profiles": ("T24.1", "Profile isolation, teardown and late callbacks; no draft leakage"),
    "invalid": ("T24.1", "Malformed stored/imported data falls back without rewriting original"),
    "repair": ("T24.1", "Missing asset/source/internal route: actionable repair and cancel"),
    "atomic": ("T24.1", "Induced write failure, revision conflict, retry and whole-document rollback"),
    "scroll": ("T24.1", "Three-viewports page: add/move/resize, disable/cancel, exact return/scroll"),
    "reuse": ("T24.1", "Template roles, scope, linked styles/components, detach and portable remap"),
    "legacy": ("T24.2", "Legacy read/cancel unchanged; explicit conversion preserves IDs/assets/routes"),
    "lounge-recreation": ("T24.2", "Review actual Home reference recreation on frozen installed source"),
    "lounge-reset": ("T24.2", "Review actual Home reset; packaged defaults and no override"),
    "pastel-recreation": ("T24.2", "Review actual Pastel recreation, components, navigation and artwork"),
    "pastel-reset": ("T24.2", "Review actual Pastel reset; packaged defaults and no override"),
    "scaling": ("T24.2", "720p/1080p/4K: safe area, geometry, readable fields and visible focus"),
    "kodi": ("T24.2", "Isolated real-Kodi page source/play/return; personal library unmodified"),
    "color": ("T24.3", "Choose a named color/wheel without typing a code; cancel and Apply"),
    "occupied-move": ("T24.3", "Discover Move, visibly adjust neighbors, accept once and cancel fully"),
    "size": ("T24.3", "Find Size inside Edit component; change size without numeric keyboard"),
    "delete-undo": ("T24.3", "Delete through original component context, confirm consequences, Undo"),
    "controls": ("T24.3", "Frame image, choose/reorder apps, spacing/type/corners/capacity without keyboard"),
    "header": ("T24.3", "Title/clock, Search/Settings styling/hiding; permanent recovery remains"),
    "discovery": ("T24.3", "Find Undo, Layers, Layout, Saved designs and Settings inheritance"),
    "accessibility": ("T24.3", "Contrast, reduced motion, long labels, missing art, empty/large collections"),
    "offline": ("T24.3", "Disconnected Generator/local art/search/Apply/recovery; Web OFF zero requests"),
    "performance": ("T24.3", "VM input/frame budgets, 100 visible widgets, 1024 nodes, lifecycle bounds"),
    "entry": ("T24.4", "Normal Settings/Edit Home/Edit page/context entrances use Studio without flag"),
    "recovery": ("T24.4", "Empty/hidden header, Home/Back/dirty exit, lock and exact launch/dialog return"),
    "parity-map": ("T24.4", "Review every legacy feature-map row against its shipped Studio destination"),
    "restoration": ("T24.5", "Normal service restored, zero unexpected restarts, profile hashes unchanged"),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_manifest(root):
    """Hash the complete production payload, including newly added source files."""
    cmake = root / "CMakeLists.txt"
    paths = {cmake}
    # Production helper scripts are part of the shipped payload too. Harness
    # and report edits alone do not invalidate an application candidate.
    for name in re.findall(r"tools/[A-Za-z0-9_.-]+\.(?:py|sh|json)", cmake.read_text(encoding="utf-8")):
        paths.add(root / name)
    for name in ("src", "qml", "config", "resources", "packaging"):
        folder = root / name
        if folder.exists():
            paths.update(path for path in folder.rglob("*") if path.is_file())
    result = {}
    for path in sorted(paths):
        if path.is_symlink() or not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Source escapes workspace: {path}")
        result[path.relative_to(root).as_posix()] = digest(path)
    return result


def template(root):
    return {
        "schemaVersion": 1,
        "sourceFiles": source_manifest(root),
        "candidate": {"runId": "", "releaseId": "", "executableSha256": ""},
        "vm": {"description": "", "renderer": "", "cpu": "", "memoryMiB": 0},
        "fullSuite": {"passed": 0, "failed": 0, "skipped": 0, "evidence": []},
        "tasks": {
            key: {
                "gate": gate,
                "task": description,
                "status": "pending",
                "input": "",
                "expected": "",
                "observed": "",
                "focusBefore": "",
                "focusAfter": "",
                "elapsedSeconds": None,
                "actions": None,
                "wrongTurns": None,
                "helpNeeded": None,
                "accidentalLaunches": None,
                "unexpectedRearrangements": None,
                "lostFocus": None,
                "evidence": [],
            }
            for key, (gate, description) in TASKS.items()
        },
        "performance": {viewport: {
            "inputToVisibleMs": [],
            "frameIntervalsMs": [],
            "visibleWidgets": 0,
            "editableNodes": 0,
            "longestUnacknowledgedInputMs": None,
            "lifecycleCycles": 0,
            "clientsBefore": None,
            "clientsAfter": None,
            "historyBytesAfter": None,
            "historyEntriesAfter": None,
        } for viewport in ("1280x720", "1920x1080")},
        "review": {"agent": "", "limitations": "", "notes": ""},
    }


def evidence_errors(base, references):
    if not isinstance(references, list) or not references:
        return ["missing evidence"]
    errors = []
    for ref in references:
        if not isinstance(ref, dict):
            errors.append("invalid evidence reference")
            continue
        name = ref.get("path", "")
        path = base / name if isinstance(name, str) else base
        if (
            not name
            or Path(name).is_absolute()
            or not path.resolve().is_relative_to(base.resolve())
            or not path.is_file()
            or path.stat().st_size == 0
        ):
            errors.append(f"missing/unsafe/empty evidence: {name}")
        elif ref.get("sha256") != digest(path):
            errors.append(f"evidence hash mismatch: {name}")
        if not isinstance(ref.get("review"), str) or not ref["review"].strip():
            errors.append(f"missing explicit review: {name}")
    return errors


def number(value, minimum=0):
    return type(value) in (int, float) and math.isfinite(value) and value >= minimum


def capture_dimensions(base, references):
    dimensions = set()
    for ref in references if isinstance(references, list) else []:
        if not isinstance(ref, dict) or not isinstance(ref.get("path"), str):
            continue
        path = base / ref["path"]
        if not path.resolve().is_relative_to(base.resolve()) or not path.is_file():
            continue
        with path.open("rb") as capture:
            header = capture.read(24)
        if len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR":
            dimensions.add(struct.unpack(">II", header[16:24]))
    return dimensions


def audit(report, root, base):
    errors = []
    if report.get("schemaVersion") != 1:
        errors.append("unsupported report schema")
    if report.get("sourceFiles") != source_manifest(root):
        errors.append("candidate production source differs from frozen manifest")
    candidate = report.get("candidate", {})
    for field in ("runId", "releaseId"):
        if not isinstance(candidate.get(field), str) or not candidate[field].strip():
            errors.append(f"missing candidate {field}")
    sha = candidate.get("executableSha256", "")
    if not isinstance(sha, str) or len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
        errors.append("missing/invalid installed executable hash")
    vm = report.get("vm", {})
    if any(not isinstance(vm.get(k), str) or not vm[k].strip() for k in ("description", "renderer", "cpu")) or not number(vm.get("memoryMiB"), 1):
        errors.append("missing VM performance baseline")
    suite = report.get("fullSuite", {})
    if not number(suite.get("passed"), 1) or suite.get("failed") != 0 or suite.get("skipped") != 0:
        errors.append("full suite not passed without failures/skips")
    errors.extend("fullSuite: " + error for error in evidence_errors(base, suite.get("evidence")))
    tasks = report.get("tasks", {})
    if set(tasks) != set(TASKS):
        errors.append("installed task inventory differs from T24 contract")
    for key, (gate, _) in TASKS.items():
        record = tasks.get(key, {})
        issues = []
        if record.get("status") != "passed" or record.get("gate") != gate:
            issues.append("not passed at owning gate")
        for field in ("input", "expected", "observed", "focusBefore", "focusAfter"):
            if not isinstance(record.get(field), str) or not record[field].strip():
                issues.append(f"missing {field}")
        for field in ("elapsedSeconds", "actions", "wrongTurns", "helpNeeded"):
            if not number(record.get(field)):
                issues.append(f"missing/invalid {field}")
        for field in ("accidentalLaunches", "unexpectedRearrangements", "lostFocus"):
            if type(record.get(field)) is not int or record[field] != 0:
                issues.append(f"unresolved {field}")
        issues.extend(evidence_errors(base, record.get("evidence")))
        captures = capture_dimensions(base, record.get("evidence"))
        if key in {"lounge-recreation", "lounge-reset", "pastel-recreation", "pastel-reset"} and not captures:
            issues.append("missing actual PNG capture for explicit reference/reset review")
        if key == "scaling" and not {(1280, 720), (1920, 1080), (3840, 2160)}.issubset(captures):
            issues.append("missing 720p/1080p/4K PNG captures")
        errors.extend(f"{key}: {issue}" for issue in issues)
    performance = report.get("performance", {})
    for viewport in ("1280x720", "1920x1080"):
        errors.extend(f"{viewport}: {error}" for error in performance_errors(performance.get(viewport, {})))
    review = report.get("review", {})
    for field in ("agent", "limitations", "notes"):
        if not isinstance(review.get(field), str) or not review[field].strip():
            errors.append(f"missing agent review {field}")
    return errors


def performance_errors(performance):
    errors = []
    for field in ("inputToVisibleMs", "frameIntervalsMs"):
        samples = performance.get(field, [])
        if not isinstance(samples, list) or len(samples) < 100 or not all(number(v, 0.001) for v in samples):
            errors.append(f"performance: need at least 100 positive finite {field} samples")
        elif field == "inputToVisibleMs" and sorted(samples)[math.ceil(len(samples) * 0.95) - 1] >= 100:
            errors.append("performance: p95 input-to-visible must be below 100 ms")
        elif field == "frameIntervalsMs" and sum(samples) / len(samples) > 1000 / 30:
            errors.append("performance: movement below 30 fps")
    for field, minimum in (("visibleWidgets", 100), ("editableNodes", 1024), ("lifecycleCycles", 10)):
        if not number(performance.get(field), minimum):
            errors.append(f"performance: {field} below {minimum}")
    for field, maximum in (("longestUnacknowledgedInputMs", 250), ("historyBytesAfter", 32 * 1024 * 1024), ("historyEntriesAfter", 100)):
        if not number(performance.get(field)) or performance[field] > maximum:
            errors.append(f"performance: {field} missing/over budget")
    if not number(performance.get("clientsBefore")) or not number(performance.get("clientsAfter")) or performance.get("clientsAfter", math.inf) > performance.get("clientsBefore", -1):
        errors.append("performance: client count grew after teardown cycles")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("init", "check"))
    parser.add_argument("report", type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.operation == "init":
            args.report.parent.mkdir(parents=True, exist_ok=True)
            with args.report.open("x", encoding="utf-8") as output:
                json.dump(template(args.source_root.resolve(strict=True)), output, indent=2)
                output.write("\n")
            print(f"Prepared {len(TASKS)} pending tasks; no application checks run: {args.report}")
            return 0
        report = json.loads(args.report.read_text(encoding="utf-8"))
        errors = audit(report, args.source_root.resolve(strict=True), args.report.parent)
        print(json.dumps({"evidenceComplete": not errors, "errors": errors}, indent=2))
        return 1 if errors else 0
    except (OSError, ValueError, TypeError, AttributeError) as error:
        print(json.dumps({"evidenceComplete": False, "errors": [str(error)]}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
