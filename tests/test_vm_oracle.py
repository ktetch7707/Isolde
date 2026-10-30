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
from contextlib import redirect_stdout, closing

spec = importlib.util.spec_from_file_location("oracle", Path(__file__).resolve().parents[1] / "tools/vm-oracle.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)


class OracleTests(unittest.TestCase):
    def test_cold_readers_close_every_connection_even_when_queries_fail(self):
        import sqlite3
        from contextlib import closing
        path=Path(__file__).resolve().parents[1]/'tools'
        specification=importlib.util.spec_from_file_location('cold_readers',path/'guide-soak-acceptance.py')
        module=importlib.util.module_from_spec(specification)
        sys.path.insert(0,str(path))
        try:specification.loader.exec_module(module)
        finally:sys.path.pop(0)
        active=[];connect=sqlite3.connect
        class Tracked(sqlite3.Connection):
            def __init__(self,*args,**kwargs):super().__init__(*args,**kwargs);active.append(self)
            def close(self):
                super().close()
                if self in active:active.remove(self)
        with tempfile.TemporaryDirectory() as folder:
            database=Path(folder)/'guide.sqlite'
            with closing(connect(database)) as connection:
                connection.execute('CREATE TABLE slots(id,channel,media_type,media_id,title,start_utc,end_utc,status)')
                now=time.time();connection.execute('INSERT INTO slots VALUES(?,?,?,?,?,?,?,?)',('s','c','episode',1,'Fixture',now-10,now+120,'ready'));connection.commit()
            with patch.object(module.sqlite3,'connect',side_effect=lambda *a,**k:connect(*a,**k,factory=Tracked)):
                for _ in range(500):
                    self.assertEqual(module.current(database,'c')[0],'s');self.assertEqual(len(module.slots(database)),1)
                self.assertEqual(active,[])
                with closing(connect(database)) as connection:connection.execute('DROP TABLE slots');connection.commit()
                with self.assertRaises(sqlite3.OperationalError):module.current(database,'c')
                self.assertEqual(active,[])

    def test_cold_reopen_never_counts_as_real_twelve_hour_soak(self):
        result=dict(passed=True,actualColdReopen=True,realTwelveHourOff=False,elapsedOffSeconds=None,
                    offlineNamespace=False,network={'mode':'host-inherited'},actualLocalElapsedSeek=True,committedSevenDaySlotsUnchanged=True,
                    automaticNativeReturn=True,normalProfileUsed=False,localSearchMatches=3,
                    captures=['guidesoakhome','guidesoakguide','guidesoakvideo','guidesoakreturn','guidesoaksearch','guidesoakresults','guidesoakrecovery'])
        self.assertTrue(oracle.visual_result_passed('guide-reopen',result))
        offline=dict(result,offlineSocketFilter=True,network={'mode':'seccomp-af-inet-denied','socketErrnos':[101,101],'unixSocketAvailable':True,'rootOwners':{'/':0,'/usr/lib/isolde-native/bwrap':0}})
        self.assertTrue(oracle.visual_result_passed('guide-reopen',offline))
        self.assertFalse(oracle.visual_result_passed('guide-soak',offline))
        for network in ({},dict(offline['network'],socketErrnos=[0,101]),dict(offline['network'],unixSocketAvailable=False),dict(offline['network'],rootOwners={'/':65534,'/usr/lib/isolde-native/bwrap':65534})):
            self.assertFalse(oracle.visual_result_passed('guide-reopen',dict(offline,network=network)))
        self.assertFalse(oracle.visual_result_passed('guide-soak',result))
        result['realTwelveHourOff']=True
        self.assertFalse(oracle.visual_result_passed('guide-soak',result))
        self.assertFalse(oracle.visual_result_passed('guide-reopen',result))
        result['elapsedOffSeconds']=43200
        result['offlineNamespace']=True
        self.assertTrue(oracle.visual_result_passed('guide-soak',result))

    def test_cold_ready_window_accounts_for_unavailable_airings_without_changing_slots(self):
        path=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('guide_ready_fixture',path/'tools/guide-soak-acceptance.py');module=importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules,{'guide_lifecycle':SimpleNamespace(position=None),'guide_features':SimpleNamespace(remote_text=None)}):spec.loader.exec_module(module)
        import sqlite3
        with tempfile.TemporaryDirectory() as folder:
            database=Path(folder)/'guide.sqlite'
            with closing(sqlite3.connect(database)) as connection:
                connection.execute('CREATE TABLE slots(channel TEXT,start_utc INTEGER,end_utc INTEGER,status TEXT)')
                connection.executemany('INSERT INTO slots VALUES(?,?,?,?)',[('local',1000,1120,'unavailable'),('local',1120,1240,'unavailable'),('local',1240,1360,'ready'),('pvr',1000,2000,'ready')])
                connection.commit()
            channels=[{'id':'pvr','owner':'kodi-pvr'},{'id':'local'}]
            self.assertEqual(module.ready_window(database,channels,1005),(1245,channels[1]))
            self.assertEqual(module.ready_window(database,channels,1250),(1250,channels[1]))
            with self.assertRaises(RuntimeError):module.ready_window(database,channels,800)
            with closing(sqlite3.connect(database)) as connection:self.assertEqual(connection.execute('SELECT COUNT(*) FROM slots WHERE status="unavailable"').fetchone()[0],2)

    def test_reopen_helper_must_match_exact_file_digest(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder).resolve();(root/'bin').mkdir();value=b'# reviewed helper'
            checksum=hashlib.sha256(value).hexdigest();path=root/'bin'/('guide-reopen-'+checksum+'.py')
            path.write_bytes(value)
            self.assertEqual(oracle.verified_reopen_helper(root,checksum),path)
            path.write_bytes(b'# modified helper')
            with self.assertRaises(RuntimeError):oracle.verified_reopen_helper(root,checksum)
            with self.assertRaises(RuntimeError):oracle.verified_reopen_helper(root,'../../other')

    @unittest.skipUnless(sys.platform=='linux' and __import__('platform').machine()=='x86_64','Linux x86-64 socket fixture')
    def test_offline_socket_filter_denies_internet_but_preserves_unix_and_root_identity(self):
        path=Path(__file__).resolve().parents[1]
        spec=importlib.util.spec_from_file_location('guide_socket_fixture',path/'tools/guide-soak-acceptance.py');module=importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules,{'guide_lifecycle':SimpleNamespace(position=None),'guide_features':SimpleNamespace(remote_text=None)}):spec.loader.exec_module(module)
        program="import os,socket,errno;assert os.stat('/').st_uid==0;unix=socket.socket(socket.AF_UNIX);unix.close();\nfor family in (socket.AF_INET,socket.AF_INET6):\n try: socket.socket(family)\n except OSError as error: assert error.errno==errno.ENETUNREACH\n else: raise AssertionError('Internet socket allowed')"
        subprocess.run([sys.executable,'-c',program],preexec_fn=module.install_socket_filter,check=True,timeout=5)

    def test_soak_receipt_uses_frozen_manifest_without_test_process_state(self):
        with tempfile.TemporaryDirectory() as folder:
            run=Path(folder);evidence=run/'evidence';evidence.mkdir();executable=run/'isolde'
            executable.write_bytes(b'frozen executable');(run/'files.sha256').write_text('frozen source manifest')
            with self.assertRaises(RuntimeError):oracle.write_soak_receipt(run,{evidence},executable)
            self.assertFalse((run/'guide-soak-receipt.json').exists())
            (evidence/'guide-soak-ready.json').write_text('{}')
            oracle.write_soak_receipt(run,{evidence},executable)
            receipt=json.loads((run/'guide-soak-receipt.json').read_text())
            self.assertEqual(receipt,{'receipt':str(evidence/'guide-soak-ready.json'),'sourceDigest':oracle.digest(run/'files.sha256'),'executableSha256':oracle.digest(executable)})
            other=run/'other';other.mkdir();(other/'guide-soak-ready.json').write_text('{}')
            with self.assertRaises(RuntimeError):oracle.write_soak_receipt(run,{evidence,other},executable)
            self.assertEqual(json.loads((run/'guide-soak-receipt.json').read_text()),receipt)

    def test_feature_helper_override_preserves_frozen_script_arguments_and_dependencies(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder).resolve();source=root/'source';scripts=source/'tools';scripts.mkdir(parents=True);(root/'bin').mkdir()
            (scripts/'guide_features.py').write_text("raise RuntimeError('stale feature helper imported')")
            (scripts/'fixture_dependency.py').write_text("VALUE='frozen dependency'")
            value=b"from fixture_dependency import VALUE\n"
            checksum=hashlib.sha256(value).hexdigest();helper=root/'bin'/('guide-features-'+checksum+'.py');helper.write_bytes(value)
            (scripts/'tv-library-acceptance.py').write_text("import guide_features,json,sys;print(json.dumps([guide_features.VALUE,sys.argv[1:]]))")
            verified=oracle.verified_guide_helper(root,checksum,'features')
            command=oracle.feature_helper_command(source,verified,root/'payload'/'isolde',['--guide-features','--inspect'])
            result=subprocess.run([sys.executable]+command[1:],capture_output=True,text=True,timeout=5,check=True)
            self.assertEqual(json.loads(result.stdout),['frozen dependency',[str(root/'payload'/'isolde'),'--guide-features','--inspect']])
            helper.write_bytes(b'modified')
            with self.assertRaises(RuntimeError):oracle.verified_guide_helper(root,checksum,'features')
            with self.assertRaises(RuntimeError):oracle.verified_guide_helper(root,checksum,'other')

    def test_native_media_fixture_serves_exact_http_ranges_and_honest_failures(self):
        spec=importlib.util.spec_from_file_location('guide_http_fixture',Path(__file__).resolve().parents[1]/'tools/guide_features.py')
        features=importlib.util.module_from_spec(spec);spec.loader.exec_module(features)
        requests=[];handler=features.stream_handler(b'0123456789',requests)
        def response(header='',path='/stream.ogv',head=False):
            request=object.__new__(handler);request.connection=Mock();request.command='HEAD' if head else 'GET';request.path=path;request.headers={'Range':header};request.wfile=io.BytesIO()
            headers={};codes=[];request.send_response=codes.append;request.send_header=lambda key,value:headers.update({key:value});request.end_headers=lambda:None
            request.serve(not head)
            return codes[0],headers,request.wfile.getvalue()
        code,headers,body=response();self.assertEqual((code,headers['Content-Length'],body),(200,'10',b'0123456789'))
        for header,expected,content_range in [('bytes=0-',b'0123456789','bytes 0-9/10'),('bytes=7-',b'789','bytes 7-9/10'),('bytes=2-4',b'234','bytes 2-4/10'),('bytes=-3',b'789','bytes 7-9/10'),('bytes=8-99',b'89','bytes 8-9/10')]:
            code,headers,body=response(header);self.assertEqual((code,body,headers['Content-Range'],headers['Content-Length']),(206,expected,content_range,str(len(expected))))
            code,headers,body=response(header,head=True);self.assertEqual((code,body,headers['Content-Range']),(206,b'',content_range))
        for header in ('bytes=10-','bytes=5-2','bytes=-0','bytes=-','bytes=0-1,4-5','invalid','bytes='+('9'*200)+'-'):
            code,headers,body=response(header);self.assertEqual((code,headers['Content-Range'],body),(416,'bytes */10',b''))
        self.assertEqual(response(path='/auth')[0],401);self.assertEqual(response(path='/missing')[0],404)
        for _ in range(150):response()
        self.assertEqual(len(requests),128)
        with self.assertRaises(ValueError):features.stream_handler(b'',[])

    def test_payload_cache_cleanup_restores_hashes_and_refuses_changed_or_unknown_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            run=Path(folder).resolve();payload=run/'payload';(payload/'bin').mkdir(parents=True);(payload/'bin/isolde').write_bytes(b'executable')
            source=payload/'share/isolde/helper.py';source.parent.mkdir(parents=True);source.write_bytes(b'verified module')
            expected=oracle.payload_hashes(payload);(run/'test-result.json').write_text(json.dumps({'passed':True,'fullSuite':True,'payload':expected}))
            cache=source.parent/'__pycache__';cache.mkdir();generated=cache/'helper.cpython-314.pyc';generated.write_bytes(b'generated runtime cache')
            removed=oracle.remove_generated_payload_cache(run);self.assertEqual(len(removed),1);self.assertFalse(generated.exists());self.assertEqual(oracle.payload_hashes(payload),expected)
            generated.write_bytes(b'cache');source.write_bytes(b'changed code')
            with self.assertRaises(RuntimeError):oracle.remove_generated_payload_cache(run)
            self.assertTrue(generated.exists());source.write_bytes(b'verified module');unknown=payload/'unexpected.py';unknown.write_bytes(b'unreviewed')
            with self.assertRaises(RuntimeError):oracle.remove_generated_payload_cache(run)
            self.assertTrue(unknown.exists());self.assertTrue(generated.exists())

    def test_remote_fixture_text_uses_production_keyboard_and_only_dpad(self):
        path=Path(__file__).resolve().parents[1]
        spec=importlib.util.spec_from_file_location('guide_feature_keys',path/'tools/guide_features.py');features=importlib.util.module_from_spec(spec);spec.loader.exec_module(features)
        source=(path/'qml/OnScreenKeyboard.qml').read_text()
        arrays=re.search(r'readonly property var keys: shift\s*\?\s*(\[[\s\S]*?\])\s*:\s*(\[[\s\S]*?\])',source)
        self.assertIsNotNone(arrays);upper,lower=(json.loads(arrays.group(index)) for index in (1,2))
        for text in ('/tmp/isolde-tv-acceptance-test/channel.json','/home/isolde-dev/Videos/Isolde-Acceptance/Poster.png','saved tv','http://127.0.0.1:12345/stream.ogv'):
            at=0;shift=False;value='previous draft';accepted=[];events=[]
            def key(command):
                nonlocal at,shift,value
                self.assertIn(command,('Up','Down','Left','Right','Return'));events.append(command);keys=upper if shift else lower
                if command=='Up':at=max(0,at-10)
                elif command=='Down':at=min(len(keys)-1,at+10)
                elif command=='Left':at=max(0,at-1)
                elif command=='Right':at=min(len(keys)-1,at+1)
                else:
                    item=keys[at]
                    if item=='Shift':shift=not shift
                    elif item=='Clear':value=''
                    elif item=='Done':accepted.append(value)
                    elif item=='Del':value=value[:-1]
                    elif item=='Space':value+=' '
                    else:value+=item;shift=False
            features.remote_text(key,text);self.assertEqual(accepted,[text]);self.assertLess(len(events),1000)
    @unittest.skipUnless(os.name == "posix", "Linux process groups/selector pipes")
    def test_failed_stage_diagnostics_do_not_replay_capture_commands(self):
        with tempfile.TemporaryDirectory() as folder:
            runner=oracle.Oracle.__new__(oracle.Oracle);runner.run=Path(folder);runner.state={};runner.update=lambda **fields:runner.state.update(fields)
            output=io.StringIO();received=[]
            def live(line):
                received.append(line);print(line)
            with redirect_stdout(output),self.assertRaises(RuntimeError):
                runner.stage("capture-failure",[sys.executable,"-c","print('CUSTOM_READY /tmp/isolde-reference-config-test');raise SystemExit(7)"],5,on_line=live)
            self.assertEqual(received,["CUSTOM_READY /tmp/isolde-reference-config-test"])
            self.assertEqual(output.getvalue().splitlines().count(received[0]),1)
            self.assertIn("LOG "+received[0],output.getvalue())

    def test_reference_guide_snapshot_reads_live_wal_without_mutating_it(self):
        import sqlite3
        path=Path(__file__).resolve().parents[1]/"tools/home-reference-acceptance.py"
        specification=importlib.util.spec_from_file_location("reference_guide_snapshot",path)
        reference=importlib.util.module_from_spec(specification);specification.loader.exec_module(reference)
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            with self.assertRaises(RuntimeError):reference.guide_cache_snapshot(root)
            with sqlite3.connect(root/"guide.sqlite") as database:
                database.execute("PRAGMA journal_mode=WAL")
                database.execute("CREATE TABLE channels(id,number,definition,candidates,cursor,revision,hidden,favorite,position)")
                database.execute("CREATE TABLE slots(id,channel,media_type,media_id,title,start_utc,end_utc,status)")
                database.execute("INSERT INTO channels VALUES('c',1,'{}','[]',0,1,0,0,0)")
                database.execute("INSERT INTO slots VALUES('s','c','episode',1,'Program',10,20,'ready')")
                database.commit()
                before=list(database.execute("SELECT * FROM slots"))
                channels,programs=reference.guide_cache_snapshot(root)
                self.assertEqual(len(channels),1);self.assertEqual(programs,before)
                self.assertEqual(list(database.execute("SELECT * FROM slots")),before)

    def test_store_and_audio_focused_suites_select_only_owned_tests(self):
        self.assertEqual(oracle.suite_selection("store_remote_ui"),(["isolde_studio_quick_tests"],"^store_remote_ui$"))
        self.assertEqual(oracle.suite_selection("audio_backend"),(["isolde_audio_tests"],"^audio_backend$"))

    def test_storage_inventory_is_read_only_and_excludes_linked_runs(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);payload=root/"runs/old/payload";payload.mkdir(parents=True)
            (payload/"binary").write_bytes(b"evidence payload")
            (payload.parent/"status.json").write_text(json.dumps({"state":"passed"}))
            result=oracle.oracle_storage_inventory(root)
            self.assertEqual(result["stagedPayloads"][0]["runId"],"old")
            self.assertGreater(result["stagedPayloads"][0]["payloadBytes"],0)
            self.assertEqual((payload/"binary").read_bytes(),b"evidence payload")

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
        runner=oracle.Oracle(home/"isolde-oracle","current")
        for i in range(1,5):
            old=runner.root/"runs"/str(i);(old/"payload").mkdir(parents=True)
            (old/"payload/bin").write_text("obsolete payload")
            (old/"source.log").write_text("preserve evidence")
            os.utime(old,(i,i))
        profile=home/"profile.json";profile.write_text("preserve user data")
        (runner.root/'runs/1/guide-soak-receipt.json').write_text('{}');os.utime(runner.root/'runs/1',(1,1))
        with patch.object(runner,"command",return_value="isolde-test"),patch.object(runner,"stage") as stage,patch.object(oracle.Path,"home",return_value=home),patch.object(oracle.shutil,"disk_usage",return_value=SimpleNamespace(free=7*1024**3)):
            runner.prepare_guest_space(8)
        stage.assert_called_once_with("guest-apt-cache-clean",["sudo","-n","apt-get","clean"],60)
        receipt=json.loads((runner.run/"guest-package-cache-cleanup.json").read_text())
        self.assertFalse(receipt["installedPackagesRemoved"])
        self.assertEqual(receipt["target"],"/var/cache/apt/archives")
        self.assertTrue((runner.root/"runs/1/payload").exists())
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

    def recovery_snapshot(self, name):
        runs=self.root/"runs";runs.mkdir(exist_ok=True)
        run=self.snapshot().rename(runs/name)
        oracle.extract_snapshot(run)
        return run,runs

    def test_source_cleanup_preserves_verified_recovery_and_reextracts(self):
        run,runs=self.recovery_snapshot("old")
        preserved={name:(run/name).read_bytes() for name in ("source.tar.gz","archive.sha256","files.sha256")}
        (run/"evidence.log").write_text("preserve acceptance")
        cache=run/"source/src/__pycache__";cache.mkdir();(cache/"main.cpython-313.pyc").write_bytes(b"disposable cache")
        oracle.remove_guest_source_snapshot(run,runs,[])
        self.assertFalse((run/"source").exists());self.assertEqual((run/"evidence.log").read_text(),"preserve acceptance")
        for name,content in preserved.items():self.assertEqual((run/name).read_bytes(),content)
        source,entries=oracle.extract_snapshot(run);oracle.verify_tree(source,entries)
        self.assertEqual((source/"src/main.cpp").read_bytes(),b"source bytes\n")

    @unittest.skipUnless(sys.platform=="linux","Guest-only maintenance orchestration")
    def test_source_maintenance_preserves_unrecoverable_payload_and_recent_runs(self):
        home=self.root/"home";home.mkdir();runner=oracle.Oracle(home/"isolde-oracle","current");runs=runner.root/"runs"
        valid=self.snapshot().rename(runs/"valid");oracle.extract_snapshot(valid)
        missing=runs/"missing";payload=runs/"payload-run";recent=[runs/"recent-a",runs/"recent-b"]
        for run in (missing,payload,*recent):
            (run/"source").mkdir(parents=True);(run/"source/retained.txt").write_text("retain this source")
        (payload/"payload").mkdir();(payload/"payload/isolde").write_text("last passing payload")
        for i,run in enumerate((missing,valid,payload,*recent),1):
            (run/"status.json").write_text(json.dumps({"state":"passed"}));os.utime(run,(i,i))
        def usage(_):return SimpleNamespace(free=(2 if (valid/"source").exists() else 4)*1024**3)
        original_read=oracle.Path.read_text
        def read_text(path,*args,**kwargs):
            if path==Path("/proc/self/mountinfo"):return "1 0 0:1 / / rw - ext4 /dev/unit-test rw\n"
            return original_read(path,*args,**kwargs)
        with patch.object(runner,"command",return_value="isolde-test"),patch.object(runner,"stage"),patch.object(oracle.Path,"home",return_value=home),patch.object(oracle.Path,"read_text",read_text),patch.object(oracle.shutil,"disk_usage",side_effect=usage):
            runner.prepare_guest_space(3,prune_sources=True)
        self.assertFalse((valid/"source").exists());self.assertTrue((valid/"source.tar.gz").is_file())
        for run in (missing,payload,*recent):self.assertEqual((run/"source/retained.txt").read_text(),"retain this source")
        self.assertEqual((payload/"payload/isolde").read_text(),"last passing payload")
        records=json.loads((runner.run/"guest-source-cleanup.json").read_text())["targets"]
        self.assertEqual({r["target"]:r["state"] for r in records},{str(missing/"source"):"preserved-missing-recovery-evidence",str(valid/"source"):"removed"})

    def test_source_cleanup_refuses_corruption_unknown_files_and_mounts(self):
        for change in ("source","archive","manifest","extra","unowned-pyc","mount","submount","ancestor-mount"):
            with self.subTest(change=change):
                run,runs=self.recovery_snapshot(change);source=run/"source";mounts=[]
                if change=="source":(source/"src/main.cpp").write_bytes(b"uncommitted valuable edits")
                elif change=="archive":
                    with (run/"source.tar.gz").open("ab") as output:output.write(b"corruption")
                elif change=="manifest":(run/"files.sha256").write_text("0"*64+"  src/main.cpp\n")
                elif change=="extra":(source/"unknown.txt").write_text("preserve unknown data")
                elif change=="unowned-pyc":(source/"unknown.pyc").write_bytes(b"not a known cache directory")
                else:mounts=[source if change=="mount" else source/"src" if change=="submount" else runs]
                with self.assertRaises(RuntimeError):oracle.remove_guest_source_snapshot(run,runs,mounts)
                self.assertTrue(source.exists());self.assertTrue((run/"source.tar.gz").exists())

    @unittest.skipUnless(os.name=="posix","POSIX source symlink boundaries")
    def test_source_cleanup_refuses_linked_sources_and_recovery_files(self):
        for target in ("source","source/src/foreign","archive.sha256"):
            with self.subTest(target=target):
                run,runs=self.recovery_snapshot(target.replace("/","-"));outside=self.root/(run.name+"-outside")
                path=run/target
                if target=="source":path.rename(outside);path.symlink_to(outside,target_is_directory=True)
                elif target=="archive.sha256":path.rename(outside);path.symlink_to(outside)
                else:outside.write_text("personal bytes");path.symlink_to(outside)
                with self.assertRaises(RuntimeError):oracle.remove_guest_source_snapshot(run,runs,[])
                self.assertTrue(outside.exists());self.assertTrue((run/"source").exists())

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

    @unittest.skipUnless(os.name == "posix", "Linux /tmp evidence paths")
    def test_guide_evidence_keeps_source_and_cache_but_refuses_private_or_oversized_files(self):
        evidence=Path(tempfile.mkdtemp(prefix="isolde-tv-acceptance-"))
        self.addCleanup(lambda:__import__("shutil").rmtree(evidence))
        (evidence/"pvr-source.json").write_text('{"channel":{"channelid":7,"uniqueid":101}}')
        (evidence/"guide-surfaces-cache.json").write_text('{"rows":[]}')
        (evidence/"shell.log").write_text("fixture log")
        secret=self.root/"private-token";secret.write_text("excluded secret")
        (evidence/"credentials.json").write_text("excluded")
        (evidence/"cache").mkdir();(evidence/"cache/cookie").write_text("excluded")
        oracle.collect_evidence(self.root,"guide-surfaces",{evidence})
        first=next(self.root.glob("evidence-guide-surfaces-*.tar.gz"))
        with tarfile.open(first) as archive:
            self.assertEqual({Path(name).name for name in archive.getnames()},{"pvr-source.json","guide-surfaces-cache.json","shell.log"})
        (evidence/"pvr-source.json").unlink();(evidence/"pvr-source.json").symlink_to(secret)
        (evidence/"guide-surfaces-cache.json").write_bytes(b"x"*(8*1024**2+1))
        oracle.collect_evidence(self.root,"guide-surfaces",{evidence})
        second=next(path for path in self.root.glob("evidence-guide-surfaces-*.tar.gz") if path!=first)
        with tarfile.open(second) as archive:self.assertEqual({Path(name).name for name in archive.getnames()},{"shell.log"})
        (evidence/"pvr-source.json").unlink();(evidence/"pvr-source.json").write_text("{}")
        (evidence/"guide-surfaces-cache.json").write_text("{}")
        oracle.collect_evidence(self.root,"studio",{evidence})
        with tarfile.open(next(self.root.glob("evidence-studio-*.tar.gz"))) as archive:self.assertEqual({Path(name).name for name in archive.getnames()},{"shell.log"})

    def test_visual_results_use_each_harness_contract(self):
        guide=dict(passed=True,freshProfile=True,normalProfileUsed=False,pvrAddon="pvr.iptvsimple",sourceChannels=1,sourceListings=50,
                   remoteImport=True,actualPvrPlayback=True,returned=True,automaticNativeReturn=True,mediaUnchanged=True,syntheticSource=True,providerSubscriptionVerified=False,
                   captures=["guidestartup","guideorigin","guidehome","guideempty","guidesource","guidepvr","guidevideo","guidereturn"])
        self.assertTrue(oracle.visual_result_passed("guide",guide))
        for claimed in (None,False,1,"true"):
            incomplete=dict(guide);incomplete["automaticNativeReturn"]=claimed
            self.assertFalse(oracle.visual_result_passed("guide",incomplete))
        incomplete=dict(guide);del incomplete["automaticNativeReturn"];self.assertFalse(oracle.visual_result_passed("guide",incomplete))
        surfaces=dict(guide,nativeChannelsCreated=2,cachedChannelCount=3,surfaceJourneyInputAcknowledged=True,committedSlotsUnchanged=True,sourceCachePreserved=True,actualLocalElapsedSeek=True,automaticLocalReturn=True,
                      captures=guide["captures"]+["guidelocaldraft","guidelocalpreview","guidelocalreview","guidelocalcreated","guidelocalvideo","guidelocalosd","guidelocalreturn","guideduplicatedraft","guidelineup","guidewidgetentry","guidecompact","guideoverlay","guideexpanded","guideoverlayreturn","guidecompactreturn"])
        self.assertTrue(oracle.visual_result_passed("guide-surfaces",surfaces))
        self.assertFalse(oracle.visual_result_passed("guide-surfaces",guide))
        for key in ("surfaceJourneyInputAcknowledged","committedSlotsUnchanged","sourceCachePreserved","actualPvrPlayback","automaticNativeReturn","actualLocalElapsedSeek","automaticLocalReturn"):
            incomplete=dict(surfaces);incomplete[key]=False;self.assertFalse(oracle.visual_result_passed("guide-surfaces",incomplete))
        for key in ("nativeChannelsCreated","cachedChannelCount"):
            incomplete=dict(surfaces);incomplete[key]=True;self.assertFalse(oracle.visual_result_passed("guide-surfaces",incomplete))
        for index in range(len(surfaces["captures"])):
            incomplete=dict(surfaces);incomplete["captures"]=surfaces["captures"][:index]+surfaces["captures"][index+1:];self.assertFalse(oracle.visual_result_passed("guide-surfaces",incomplete))
        self.assertEqual(oracle.CASES["guide-surfaces"],("tv-library-acceptance.py",["--guide","--guide-surfaces","--inspect"]))
        spec=importlib.util.spec_from_file_location('guide_feature_contract',Path(__file__).resolve().parents[1]/'tools/guide_features.py')
        features=importlib.util.module_from_spec(spec);spec.loader.exec_module(features)
        feature_flags=('nativeFeedImport','sourceTimesRetained','explicitEpgMapping','actualNoEpgPlayback','authenticationFailureVisible','sourceOffPreservesCache','managedStationLogo','remoteColorWheel','portableArtwork','portableSchedule','savedRuleTemplate','remoteCalendarPolicy','cartoonNetworkOneSave','mixedNestedRules','directSourceImport','failedFeedRefreshPreservesCache')
        feature_result=dict(surfaces,captures=surfaces['captures']+features.PHASES,cachedChannelCount=10,**dict.fromkeys(feature_flags,True))
        self.assertTrue(oracle.visual_result_passed('guide-features',feature_result))
        for phase in features.PHASES:
            incomplete=dict(feature_result,captures=[item for item in feature_result['captures'] if item!=phase])
            self.assertFalse(oracle.visual_result_passed('guide-features',incomplete),phase)
        for flag in feature_flags:
            self.assertFalse(oracle.visual_result_passed('guide-features',dict(feature_result,**{flag:False})),flag)
        lifecycle_phases=['guidecontinuousready','guidecontinuousvideo','guidecontinuousnext','guidesurfed','guidesurfreturn','guidedeletedready','guidedeletedfile','guidedeletedreturn','guiderepairselect','guiderepaircontext','guiderepairreview','guiderepairconfirm','guiderepaired']
        lifecycle=dict(surfaces,captures=surfaces['captures']+lifecycle_phases,continuousTransition=True,remoteSurf=True,actualDeletedFile=True,targetedRepair=True,unaffectedSlotsPreserved=True,cachedFirstPaintMs=[40,80])
        self.assertTrue(oracle.visual_result_passed('guide-lifecycle',lifecycle))
        for key in ('continuousTransition','remoteSurf','actualDeletedFile','targetedRepair','unaffectedSlotsPreserved'):
            invalid=dict(lifecycle);invalid[key]=False;self.assertFalse(oracle.visual_result_passed('guide-lifecycle',invalid))
        for timing in ([],[1001],[True],[0.5]):
            invalid=dict(lifecycle,cachedFirstPaintMs=timing);self.assertFalse(oracle.visual_result_passed('guide-lifecycle',invalid))
        soak=dict(passed=True,realTwelveHourOff=True,offlineNamespace=True,actualLocalElapsedSeek=True,committedSevenDaySlotsUnchanged=True,automaticNativeReturn=True,normalProfileUsed=False,elapsedOffSeconds=43201,localSearchMatches=3,captures=['guidesoakhome','guidesoakguide','guidesoakvideo','guidesoakreturn','guidesoaksearch','guidesoakresults','guidesoakrecovery'])
        self.assertTrue(oracle.visual_result_passed('guide-soak',soak))
        for elapsed in (True,43199,'43201'):
            self.assertFalse(oracle.visual_result_passed('guide-soak',dict(soak,elapsedOffSeconds=elapsed)))
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
