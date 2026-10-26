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
import signal
import threading
import unittest
from unittest.mock import patch
from unittest.mock import Mock
from types import SimpleNamespace
from contextlib import redirect_stdout

spec = importlib.util.spec_from_file_location("oracle", Path(__file__).resolve().parents[1] / "tools/vm-oracle.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)


class OracleTests(unittest.TestCase):
    def test_guide_probe_does_not_search_other_profiles_or_claim_playback(self):
        with tempfile.TemporaryDirectory() as folder:
            home=Path(folder)
            result=oracle.guide_backend_probe(home)
            self.assertFalse(result["available"]);self.assertFalse(result["playbackVerified"])
            profiles=home/".config/Isolde/profiles.json";profiles.parent.mkdir(parents=True)
            profiles.write_text(json.dumps({"active":"../other"}))
            result=oracle.guide_backend_probe(home)
            self.assertFalse(result["available"]);self.assertNotIn("activeProfile",result)
            profiles.write_text(json.dumps({"active":"default"}))
            result=oracle.guide_backend_probe(home)
            self.assertFalse(result["available"]);self.assertEqual(result["activeProfile"],"default")
            self.assertFalse(result["playbackVerified"])

    @unittest.skipUnless(os.name=="posix","Linux active-profile local socket")
    def test_guide_probe_is_read_only_paged_and_active_profile_scoped(self):
        import socket
        with tempfile.TemporaryDirectory() as folder:
            home=Path(folder);profiles=home/".config/Isolde/profiles.json";profiles.parent.mkdir(parents=True)
            profiles.write_text(json.dumps({"active":"default"}))
            directory=home/".local/share/Isolde/native-profiles/default/kodi/home/.kodi/userdata/isolde";directory.mkdir(parents=True)
            descriptor=os.open(directory,os.O_RDONLY|os.O_DIRECTORY)
            calls=[];errors=[]
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as server:
                server.bind("/proc/self/fd/"+str(descriptor)+"/rpc.sock");server.listen();server.settimeout(3)
                def serve():
                    try:
                        for _ in range(2):
                            stream,_=server.accept()
                            with stream:
                                request=json.loads(stream.recv(32768));calls.append(request)
                                result={"version":{"major":13,"minor":5,"patch":0}} if request["method"]=="JSONRPC.Version" else {"channels":[{"channelid":7,"broadcastnow":{"title":"Fixture"}}],"limits":{"start":0,"end":1,"total":1}}
                                stream.sendall(json.dumps({"jsonrpc":"2.0","id":1,"result":result}).encode()+b"\n")
                    except Exception as error:errors.append(error)
                thread=threading.Thread(target=serve);thread.start()
                result=oracle.guide_backend_probe(home);thread.join(4)
            os.close(descriptor)
            self.assertEqual(errors,[]);self.assertFalse(thread.is_alive())
            self.assertTrue(result["available"]);self.assertEqual(result["totalChannels"],1)
            self.assertFalse(result["playbackVerified"]);self.assertEqual(result["channelsWithNow"],1)
            self.assertEqual([r["method"] for r in calls],["JSONRPC.Version","PVR.GetChannels"])
            self.assertEqual(calls[1]["params"]["limits"],{"start":0,"end":50})

    def test_android_probe_does_not_claim_product_acceptance(self):
        result = {key: True for key in ("passed", "compatibilityProbeOnly", "userdataBindingVerified", "packageInstalled", "launchRequested", "ownedSessionStopped")}
        result.update({key: False for key in ("isoldeIntegrationVerified", "playbackVerified", "remoteVerified")})
        self.assertTrue(oracle.visual_result_passed("android", result))
        for key in result:
            bad = dict(result); bad.pop(key)
            self.assertFalse(oracle.visual_result_passed("android", bad))

    def test_android_setup_error_is_recorded_without_an_enter_prompt(self):
        process=Mock();process.stdout=["Installer failed before initialization\n"];process.wait.return_value=7
        fake=SimpleNamespace(Popen=Mock(return_value=process),PIPE=-1,STDOUT=-2)
        script=oracle.android_admin_launcher(["/usr/bin/python3","/tested/setup.py"],self.root,"Android setup")
        with patch.dict(sys.modules,{"subprocess":fake}),redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit) as result:exec(compile(script,"android-admin-terminal.py","exec"),{})
        self.assertEqual(result.exception.code,7)
        self.assertEqual(json.loads((self.root/"android-admin-exit.json").read_text()),{"exit":7})
        self.assertIn("failed before initialization",(self.root/"android-admin.log").read_text())
        self.assertEqual(fake.Popen.call_args.args[0],["pkexec","/usr/bin/python3","/tested/setup.py"])

    @unittest.skipUnless(sys.platform=="linux","Guest-only cleanup")
    def test_android_space_preparation_preserves_recent_payloads_and_profile_data(self):
        home=self.root/"home";home.mkdir()
        runner=oracle.Oracle(home/"oracle","current")
        for i in range(1,5):
            old=runner.root/"runs"/str(i);(old/"payload").mkdir(parents=True)
            (old/"payload/bin").write_text("obsolete payload")
            (old/"source.log").write_text("preserve evidence")
            os.utime(old,(i,i))
        profile=home/"profile.json";profile.write_text("preserve user data")
        with patch.object(runner,"command",return_value="isolde-test"),patch.object(oracle.Path,"home",return_value=home),patch.object(oracle.shutil,"disk_usage",return_value=SimpleNamespace(free=7*1024**3)):
            runner.prepare_guest_space(8)
        self.assertFalse((runner.root/"runs/1/payload").exists())
        self.assertFalse((runner.root/"runs/2/payload").exists())
        self.assertTrue((runner.root/"runs/3/payload").exists())
        self.assertTrue((runner.root/"runs/4/payload").exists())
        self.assertTrue((runner.root/"runs/1/source.log").is_file())
        self.assertEqual(profile.read_text(),"preserve user data")

    def test_cancellation_unwinds_a_waiting_selector_and_owned_child(self):
        previous=signal.signal(signal.SIGTERM,oracle.interrupted)
        timer=threading.Timer(.2,lambda:os.kill(os.getpid(),signal.SIGTERM))
        started=time.monotonic()
        try:
            timer.start()
            runner=oracle.Oracle(self.root,"cancel-select")
            with self.assertRaisesRegex(RuntimeError,"interrupted by signal"):
                runner.stage("wait",[sys.executable,"-c","import time; time.sleep(30)"],35)
            self.assertLess(time.monotonic()-started,3)
        finally:
            timer.cancel();signal.signal(signal.SIGTERM,previous)
    def test_guest_build_cleanup_preserves_source_and_rejects_redirects(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run = root/"studio-implementation-1"; run.mkdir()
            build = run/"build"; build.mkdir()
            (build/"CMakeCache.txt").write_text("Generated cache")
            (build/"CMakeFiles").mkdir()
            (run/"source").mkdir(); (run/"evidence.log").write_text("retain")
            with self.assertRaises(RuntimeError): oracle.remove_guest_build(build, root, [build/"shared"])
            self.assertTrue(build.exists())
            oracle.remove_guest_build(build, root, [])
            self.assertTrue((run/"source").exists()); self.assertTrue((run/"evidence.log").exists())
            outside = root/"outside"; outside.mkdir()
            build.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(RuntimeError): oracle.remove_guest_build(build, root, [])
            self.assertTrue(outside.exists())

    def test_reference_round_trip_does_not_claim_collection_acceptance(self):
        path = Path(__file__).resolve().parents[1] / "tools/home-reference-acceptance.py"
        module_spec = importlib.util.spec_from_file_location("reference_acceptance", path)
        reference = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(reference)
        document = {"components": [{"appCollection": {"mode": "all"}}], "pages": [
            {"layout": {"components": [{"componentType": "library"}]}}]}
        review = reference.collection_review(document)
        self.assertEqual(review["required"], ["apps", "media"])
        self.assertFalse(review["population_verified"])
        self.assertFalse(review["behavior_verified"])
        self.assertEqual(len(review["missing"]), 2)
        legacy = reference.collection_review({"items": [{"kind": "action"}]})
        self.assertEqual(legacy["required"], [])
        self.assertIsNone(legacy["population_verified"])
        functional = dict(functional_journey_passed=True, reset_verified=True,
                          settings_skin_round_trip=True, saved_skin_survives_reset=True,
                          component_support=None, missing=review["missing"],
                          semantic_acceptance="not_verified", visual_acceptance="pending")
        self.assertTrue(oracle.visual_result_passed("pastel", functional))
        functional["functional_journey_passed"] = False
        self.assertFalse(oracle.visual_result_passed("pastel", functional))

    def test_studio_suite_builds_only_matching_test_targets(self):
        targets, pattern = oracle.suite_selection("studio")
        self.assertEqual(targets, ["isolde_studio_quick_tests", "isolde_component_tests", "isolde_home_navigation_tests"])
        for name in ("studio_quick", "studio_selection", "studio_gestures", "studio_grid", "studio_grid_controls", "studio_destinations", "studio_ordered", "studio_policies", "studio_overlays", "studio_unit", "studio_behavior"):
            self.assertIsNotNone(re.fullmatch(pattern, name))
        self.assertIsNone(re.fullmatch(pattern, "home_component_navigation"))
        self.assertIsNone(re.fullmatch(pattern, "studio_quick_extra"))
        self.assertEqual(oracle.suite_selection("studio_quick"), (["isolde_studio_quick_tests"], "^studio_quick$"))

    def test_vpn_suite_selects_backend_and_remote_launch_contracts(self):
        targets, pattern = oracle.suite_selection("vpn")
        self.assertEqual(targets, ["isolde_vpn_tests"])
        self.assertIsNotNone(re.fullmatch(pattern, "vpn_setup"))
        self.assertIsNotNone(re.fullmatch(pattern, "vpn_launch"))
        self.assertIsNone(re.fullmatch(pattern, "vpn_unrelated"))

    def test_targeted_studio_run_cannot_be_used_as_deploy_payload(self):
        runner = oracle.Oracle(self.root, "targeted")
        (runner.run / "test-result.json").write_text(json.dumps({"passed": True, "fullSuite": False, "suite": "studio"}))
        with self.assertRaisesRegex(RuntimeError, "no passing full-suite payload"):
            runner.tested_payload()

    def test_split_navigation_runs_every_native_scenario_once(self):
        repo = Path(__file__).resolve().parents[1]
        source = (repo / "tests/tst_home_navigation.cpp").read_text()
        slots = re.search(r"private slots:(.*?)\n};", source, re.S).group(1)
        expected = {name for name in re.findall(r"void (\w+)\(\);", slots) if not name.endswith("_data")}
        cmake = (repo / "CMakeLists.txt").read_text()
        selected = []
        for group in ("RUNTIME", "STUDIO"):
            selected.extend(re.search(r"set\(ISOLDE_" + group + r"_NAVIGATION_CASES\s+(.*?)\)", cmake, re.S).group(1).split())
        self.assertEqual(set(selected), expected)
        self.assertEqual(len(selected), len(expected))
        targets, pattern = oracle.suite_selection("home_component_navigation")
        self.assertEqual(targets, ["isolde_home_navigation_tests"])
        for name in ("home_component_navigation", "studio_navigation"):
            self.assertIsNotNone(re.fullmatch(pattern, name))
        self.assertIsNone(re.fullmatch(pattern, "studio_behavior"))

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
        emulators=dict(passed=True,freshProfile=True,remoteRuntimeSetup=True,remoteRegistration=True,nativeGameStarted=True,
            returned=True,sourceUnchanged=True,runtime="ppsspp",gameplayVerified=False,physicalControllerVerified=False,
            captures=["emulatorsempty","emulatorssetup","emulatorsinstallreview","emulatorsready","emulatorsadd","emulatorsreview",
                "emulatorsdetails","emulatorspermission","emulatorsmenu","emulatorsplay","emulatorsaction","emulatorsreturn","emulatorsrestored"])
        self.assertTrue(oracle.visual_result_passed("emulators",emulators))
        for key in emulators:
            incomplete=dict(emulators);incomplete.pop(key)
            self.assertFalse(oracle.visual_result_passed("emulators",incomplete))
        claimed=dict(emulators,gameplayVerified=True)
        self.assertFalse(oracle.visual_result_passed("emulators",claimed))
        independent = {"passed": True, "native_clients": True, "steam_client_downloaded": False, "subscriptionPlaybackVerified": False, "household_unchanged": True}
        self.assertTrue(oracle.visual_result_passed("clients", independent))
        self.assertFalse(oracle.visual_result_passed("apps", independent))
        clients = dict(passed=True, native_clients=True, steam_client_downloaded=True, household_unchanged=True, subscriptionPlaybackVerified=False)
        self.assertTrue(oracle.visual_result_passed("apps", clients))
        self.assertFalse(oracle.visual_result_passed("apps", {"passed": True}))
        clients["subscriptionPlaybackVerified"] = True
        self.assertFalse(oracle.visual_result_passed("apps", clients))
        repaired = dict(passed=True, preserving_update=True, household_unchanged=True, real_compositor_return=True)
        self.assertTrue(oracle.visual_result_passed("repairs", repaired))
        for key in repaired:
            incomplete = dict(repaired); incomplete[key] = False
            self.assertFalse(oracle.visual_result_passed("repairs", incomplete))
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

    @unittest.skipUnless(os.name=="posix","Linux advisory lock")
    def test_bounded_wait_preserves_owner_and_acquires_after_release(self):
        lock=self.root/"queued-lock";ready=threading.Event()
        def owner():
            with oracle.exclusive_lock(lock):ready.set();time.sleep(.15)
        thread=threading.Thread(target=owner);thread.start();self.assertTrue(ready.wait(1))
        with oracle.exclusive_lock(lock,.8):self.assertFalse(thread.is_alive())
        thread.join(1);self.assertTrue(lock.exists())
        with oracle.exclusive_lock(lock):
            started=time.monotonic()
            with self.assertRaisesRegex(RuntimeError,"BUSY"):
                with oracle.exclusive_lock(lock,.05):self.fail("Interrupted an active owner")
            self.assertLess(time.monotonic()-started,.5)


if __name__ == "__main__":
    unittest.main()
