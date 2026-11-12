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
import uuid
from unittest.mock import patch
from unittest.mock import Mock
from types import SimpleNamespace
from contextlib import redirect_stdout, closing

spec = importlib.util.spec_from_file_location("oracle", Path(__file__).resolve().parents[1] / "tools/vm-oracle.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)


class OracleTests(unittest.TestCase):
    def test_visual_desktop_prompt_inventory_is_bounded_and_dismiss_only(self):
        self.assertEqual(oracle.secret_prompt_paths('/org/freedesktop/secrets\n/org/freedesktop/secrets/collection/login\n/org/freedesktop/secrets/prompt/p1\n'),['/org/freedesktop/secrets/prompt/p1'])
        for tree in ('x'*65537,'/org/freedesktop/secrets/prompt/../../collection/login','\n'.join('/org/freedesktop/secrets/prompt/p'+str(i) for i in range(17))):
            with self.assertRaises(RuntimeError):oracle.secret_prompt_paths(tree)

    def test_reference_default_theme_ignores_object_order_but_rejects_value_changes(self):
        path=Path(__file__).resolve().parents[1]/'tools/home-reference-acceptance.py'
        spec=importlib.util.spec_from_file_location('reference_theme',path)
        reference=importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules,{'fcntl':Mock()}):spec.loader.exec_module(reference)
        expected={'focusColor':'#AABBCC','surfaceThemes':{'search':{'schemaVersion':1,'mode':'origin'},'settings':{'schemaVersion':1,'mode':'home'}}}
        actual={'surfaceThemes':{'settings':{'mode':'home','schemaVersion':1},'search':{'mode':'origin','schemaVersion':1}},'focusColor':'#aabbcc'}
        self.assertTrue(reference.default_theme_matches(actual,expected))
        actual['surfaceThemes']['settings']['mode']='custom'
        self.assertFalse(reference.default_theme_matches(actual,expected))
        actual['surfaceThemes']=expected['surfaceThemes'];actual['focusColor']='#112233'
        self.assertFalse(reference.default_theme_matches(actual,expected))

    def test_artwork_view_reuse_requires_original_full_and_refuses_other_runtime_changes(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder).resolve();base=root/'runs/base';current=root/'runs/focused';base.mkdir(parents=True);current.mkdir()
            values={'qml/StudioImageFraming.qml':b'old','qml/StudioArtworkPicker.qml':b'picker','qml/StudioInspector.qml':b'inspector','qml/PageStudioView.qml':b'editor','qml/ArtworkImage.qml':b'raster','src/home/StudioSession.cpp':b'core','config/studio-defaults.json':b'defaults','tests/studio/tst_colors.qml':b'old color checks'}
            def write(run,data):
                (run/'files.sha256').write_text(''.join(hashlib.sha256(value).hexdigest()+'  '+name+'\n' for name,value in sorted(data.items())))
            write(base,values);changed=dict(values,**{'qml/StudioImageFraming.qml':b'actual card','qml/PageStudioView.qml':b'actual geometry','tests/studio/tst_colors.qml':b'color input regression'})
            write(current,changed);(base/'payload/bin').mkdir(parents=True);(base/'payload/bin/isolde').write_bytes(b'full candidate')
            full={'passed':True,'fullSuite':True,'sourceDigest':oracle.digest(base/'files.sha256'),'payload':oracle.payload_hashes(base/'payload')};oracle.atomic_json(base/'test-result.json',full)
            proof=oracle.focused_qml_coverage(root,current,'base','studio_artwork');self.assertEqual(proof['changedStudioArtworkQml'],['qml/PageStudioView.qml','qml/StudioImageFraming.qml'])
            for name in ('src/home/StudioSession.cpp','config/studio-defaults.json','qml/ArtworkImage.qml','qml/Unexpected.qml'):
                write(current,dict(changed,**{name:b'changed'}))
                with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio_artwork')
            write(current,changed);oracle.atomic_json(base/'test-result.json',dict(full,fullSuite=False))
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio_artwork')
            oracle.atomic_json(base/'test-result.json',full);(base/'payload/bin/isolde').write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio_artwork')

    def test_studio_visuals_reuse_is_bounded_to_the_observed_ui_defects(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder).resolve();base=root/'runs/base';current=root/'runs/focused';base.mkdir(parents=True);current.mkdir()
            values={'qml/PageStudioView.qml':b'editor','qml/CommandCenter.qml':b'search','src/home/StudioSession.cpp':b'core','config/studio-defaults.json':b'data'}
            def write(run,data):
                (run/'files.sha256').write_text(''.join(hashlib.sha256(value).hexdigest()+'  '+name+'\n' for name,value in sorted(data.items())))
            write(base,values);changed=dict(values,**{'qml/PageStudioView.qml':b'heading fix','qml/CommandCenter.qml':b'bounded plain icon'})
            write(current,changed);(base/'payload/bin').mkdir(parents=True);(base/'payload/bin/isolde').write_bytes(b'full candidate')
            result={'passed':True,'fullSuite':True,'sourceDigest':oracle.digest(base/'files.sha256'),'payload':oracle.payload_hashes(base/'payload')};oracle.atomic_json(base/'test-result.json',result)
            proof=oracle.focused_qml_coverage(root,current,'base','studio_visuals')
            self.assertEqual(proof['changedStudioVisualsQml'],['qml/CommandCenter.qml','qml/PageStudioView.qml'])
            for name in ('src/home/StudioSession.cpp','config/studio-defaults.json','qml/Other.qml'):
                write(current,dict(changed,**{name:b'changed'}))
                with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio_visuals')
            write(current,changed);oracle.atomic_json(base/'test-result.json',dict(result,fullSuite=False))
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio_visuals')
            oracle.atomic_json(base/'test-result.json',result);(base/'payload/bin/isolde').write_bytes(b'changed')
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio_visuals')
        icon_targets,icon_pattern=oracle.suite_selection('app_icons')
        self.assertIn('isolde_tests',icon_targets)
        self.assertIsNotNone(re.match(icon_pattern,'studio_artwork'))
        self.assertIsNotNone(re.match(icon_pattern,'studio_selection'))
        self.assertIsNone(re.match(icon_pattern,'games_remote_ui'))
        targets,pattern=oracle.suite_selection('studio_visuals')
        self.assertEqual(targets,['isolde_studio_quick_tests','isolde_home_navigation_tests'])
        for name in ('studio_quick','studio_artwork','studio_shell','home_component_navigation','studio_navigation'):
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIsNone(re.fullmatch(pattern,'studio_performance'))

    def test_studio_search_receipt_requires_denied_network_and_observed_input(self):
        result={'passed':True,'journey':'studio-search','freshProfile':True,'normalProfileUsed':False,'network':{'socketErrnos':[101,101],'unixSocketAvailable':True},'actualInputActions':1,'captureTrace':[{}]*3}
        self.assertTrue(oracle.visual_result_passed('studio-search',result))
        self.assertFalse(oracle.visual_result_passed('studio-search',dict(result,network={})))
        self.assertFalse(oracle.visual_result_passed('studio-search',dict(result,actualInputActions=0)))
        command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),search=True)
        self.assertIn('--search-only',command)
        self.assertIn(str(Path('/frozen')/'tools/home-components-acceptance.py'),command)

    def test_home_acceptance_helper_compiles_without_launching_a_session(self):
        path=Path(__file__).resolve().parents[1]/'tools/home-components-acceptance.py'
        compile(path.read_bytes(),str(path),'exec')

    def test_linked_test_cleanup_preserves_objects_kept_active_and_unknown_targets(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);build=root/'work/build';meta=build/'CMakeFiles';meta.mkdir(parents=True)
            names=['isolde_old_tests','isolde_keep_tests','isolde_active_tests','isolde_unknown_tests']
            for name in names:(build/name).write_bytes(b'\x7fELFgenerated')
            (meta/'TargetDirectories.txt').write_text('\n'.join(str(meta/(name+'.dir')) for name in names[:3]))
            obj=meta/'cached.o';obj.write_bytes(b'compiled object')
            removed=oracle.remove_linked_test_executables(root,active=[build/names[2]],keep=[names[1]],target_free=10**18)
            self.assertEqual(removed,[str(build/names[0])]);self.assertTrue(obj.exists())
            self.assertTrue(all((build/name).exists() for name in names[1:]))
            with self.assertRaises(RuntimeError):oracle.remove_linked_test_executables(root,mounts=[build/names[1]],target_free=10**18)
            with (meta/'TargetDirectories.txt').open('a') as listing:listing.write('\n'+str(meta/'isolde_core.dir'))
            archive=build/'libisolde_core.a';archive.write_bytes(b'!<arch>\ngenerated archive')
            self.assertEqual(oracle.remove_linked_test_executables(root,active=[build/names[2]],keep=[names[1]],target_free=10**18),[str(archive)])
            self.assertEqual(obj.read_bytes(),b'compiled object')
            with (meta/'TargetDirectories.txt').open('a') as listing:listing.write('\n'+str(meta/'isolde.dir'))
            app=build/'isolde';app.write_bytes(b'\x7fELFgenerated app')
            installed=root/'installed/isolde';installed.parent.mkdir();installed.write_bytes(app.read_bytes())
            self.assertEqual(oracle.remove_linked_test_executables(root,active=[build/names[2],app],keep=[names[1]],target_free=10**18),[])
            self.assertEqual(oracle.remove_linked_test_executables(root,active=[build/names[2]],keep=[names[1]],target_free=10**18),[str(app)])
            self.assertTrue(installed.is_file());self.assertTrue(obj.is_file())

    def test_object_cleanup_preserves_production_selected_active_and_metadata(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);meta=root/'work/build/CMakeFiles';meta.mkdir(parents=True)
            names=['isolde_old_tests','isolde_keep_tests','isolde_core','isolde','unknown_tests']
            objects=[]
            for name in names:
                directory=meta/(name+'.dir');directory.mkdir()
                obj=directory/'source.cpp.o';obj.write_bytes(b'\x7fELFobject');objects.append(obj)
            listing=meta/'TargetDirectories.txt';listing.write_text('\n'.join(str(meta/(name+'.dir')) for name in names))
            active=objects[0].with_name('active.cpp.o');active.write_bytes(b'\x7fELFactive')
            generated=objects[0].with_name('moc.cpp');generated.write_text('generated source')
            with self.assertRaisesRegex(RuntimeError,'Mounted'):oracle.remove_unselected_test_objects(root,mounts=[objects[0].parent],keep=[names[1]],target_free=10**18)
            self.assertTrue(objects[0].exists())
            removed=oracle.remove_unselected_test_objects(root,active=[active],keep=[names[1]],target_free=10**18)
            self.assertEqual(removed,[str(objects[0])])
            self.assertTrue(all(p.exists() for p in [*objects[1:],active,generated,listing]))
            objects[0].write_bytes(b'unknown bytes')
            with self.assertRaisesRegex(RuntimeError,'Unknown'):oracle.remove_unselected_test_objects(root,active=[active],keep=[names[1]],target_free=10**18)
            self.assertTrue(objects[0].exists())

    def test_runner_test_cache_preserves_recent_and_rejects_unknown_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);bins=root/'bin';bins.mkdir();driver=bins/'driver.py';driver.write_text('current')
            bundles=[]
            for i in range(5):
                content=('test '+str(i)).encode();entries=hashlib.sha256(content).hexdigest()+'  test.py\n'
                bundle=bins/('maintenance-'+str(i)*64+'-'+hashlib.sha256(entries.encode()).hexdigest());bundle.mkdir()
                (bundle/'test.py').write_bytes(content);(bundle/'files.sha256').write_text(entries);os.utime(bundle,(i+1,i+1));bundles.append(bundle)
            (bundles[0]/'unexpected').write_text('preserve');os.utime(bundles[0],(1,1))
            with self.assertRaises(RuntimeError):oracle.remove_runner_test_bundles(root,driver,target_free=10**18)
            self.assertTrue(bundles[0].exists());self.assertTrue(bundles[4].exists())
            (bundles[0]/'unexpected').unlink();os.utime(bundles[0],(1,1))
            removed=oracle.remove_runner_test_bundles(root,driver,target_free=10**18)
            self.assertIn(str(bundles[0]),removed);self.assertTrue(all(p.exists() for p in bundles[2:]))

    def test_archive_pruning_requires_exact_host_backup_and_preserves_payload_or_source(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);run=root/'runs/old';run.mkdir(parents=True);content=b'archived source';checksum=hashlib.sha256(content).hexdigest()
            (run/'source.tar.gz').write_bytes(content);(run/'archive.sha256').write_text(checksum+'  source.tar.gz')
            (run/'status.json').write_text('{"state":"passed"}')
            receipt=[dict(runId='old',sha256=checksum)]
            with self.assertRaises(RuntimeError):oracle.remove_backed_up_archives(root,receipt,[run/'source.tar.gz'])
            (run/'payload').mkdir();self.assertEqual(oracle.remove_backed_up_archives(root,receipt),[]);(run/'payload').rmdir()
            with self.assertRaises(RuntimeError):oracle.remove_backed_up_archives(root,[dict(runId='old',sha256='0'*64)])
            self.assertEqual(oracle.remove_backed_up_archives(root,receipt),receipt);self.assertFalse((run/'source.tar.gz').exists());self.assertTrue((run/'archive.sha256').exists())

    def test_catalog_result_requires_import_failure_cancel_apply_and_all_frames(self):
        fields=('passed','freshProfile','remoteImport','invalidRefused','privateDraft','framingCancel','atomicGifApply','reopenPreserved','remotePrimitives','independentText','linkedImage','layersSelection')
        result={key:True for key in fields};result['managedGifSha256']='a'*64
        result['captures']=['catalogentry','imagestyles','imagebrowser','imageinvalid','imagepreview','imageplaced','imageinspector','imagemotiondraft','imageframing','imageframingdraft','imageapplied','catalogshape','catalogtext','textkeyboard','textindependent','catalogbutton','catalogcontainer','cataloglayers','imagelink','catalogapplied','imageruntimeone','imageruntimetwo','imagereopened']
        self.assertTrue(oracle.visual_result_passed('catalog',result))
        for key in fields:self.assertFalse(oracle.visual_result_passed('catalog',dict(result,**{key:False})))
        self.assertFalse(oracle.visual_result_passed('catalog',dict(result,captures=result['captures'][:-1])))
        command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),catalog=True)
        self.assertEqual(command[-3:],['--studio','--catalog','--inspect'])

    def test_original_source_archive_restoration_requires_matching_saved_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);candidate=root/'run';bins=root/'bin';candidate.mkdir();bins.mkdir()
            original=b'recoverable original archive';identity=hashlib.sha256(original).hexdigest()
            (candidate/'archive.sha256').write_text(identity+'  source.tar.gz\n')
            incoming=bins/('recovery-'+identity+'.tar.gz');incoming.write_bytes(b'wrong')
            with self.assertRaises(RuntimeError):oracle.restore_source_archive(candidate,bins)
            self.assertFalse((candidate/'source.tar.gz').exists())
            incoming.write_bytes(original);self.assertTrue(oracle.restore_source_archive(candidate,bins))
            self.assertEqual((candidate/'source.tar.gz').read_bytes(),original)
            self.assertFalse(oracle.restore_source_archive(candidate,bins))

    def test_fixture_cache_cleanup_preserves_data_recent_active_and_mounts(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for i in range(6):
                fixture=root/('isolde-tv-acceptance-'+str(i));(fixture/'cache').mkdir(parents=True)
                (fixture/'cache/file').write_text('reproducible');(fixture/'result.json').write_text('evidence')
                os.utime(fixture,(i,i))
            active=root/'isolde-tv-acceptance-1'
            removed=oracle.remove_fixture_caches(root,[],[active/'cache'],target_free=10**18)
            self.assertEqual(set(removed),{str(root/'isolde-tv-acceptance-0/cache'),str(root/'isolde-tv-acceptance-2/cache')})
            self.assertTrue((active/'cache/file').exists())
            self.assertTrue(all((root/('isolde-tv-acceptance-'+str(i))/'result.json').exists() for i in range(6)))
            with self.assertRaises(RuntimeError):oracle.remove_fixture_caches(root,[active/'cache'],[],target_free=10**18)
            remaining={str(p/'cache') for p in root.iterdir() if p!=active and (p/'cache').exists()}
            removed=oracle.remove_fixture_caches(root,[],[active/'cache'],target_free=10**18,preserve_recent=0)
            self.assertEqual(set(removed),remaining)
            self.assertTrue((active/'cache/file').exists())
            self.assertTrue(all((root/('isolde-tv-acceptance-'+str(i))/'result.json').exists() for i in range(6)))
            with self.assertRaises(RuntimeError):oracle.remove_fixture_caches(root,[],[],preserve_recent=-1)

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
        result=dict(passed=True,actualColdReopen=True,explicitManagedStop=True,realTwelveHourOff=False,elapsedOffSeconds=None,
                    offlineNamespace=False,network={'mode':'host-inherited'},actualLocalElapsedSeek=True,committedSevenDaySlotsUnchanged=True,
                    automaticNativeReturn=True,normalProfileUsed=False,localSearchMatches=3,
                    captures=['guidesoakhome','guidesoakguide','guidesoakvideo','guidesoakreturn','guidesoaksearch','guidesoakresults','guidesoakrecovery'])
        self.assertTrue(oracle.visual_result_passed('guide-reopen',result))
        offline=dict(result,offlineSocketFilter=True,network={'mode':'seccomp-af-inet-denied','socketErrnos':[101,101],'unixSocketAvailable':True,'rootOwners':{'/':0,'/usr/lib/isolde-native/bwrap':0}})
        self.assertTrue(oracle.visual_result_passed('guide-reopen',offline))
        self.assertFalse(oracle.visual_result_passed('guide-reopen',dict(offline,explicitManagedStop=False)))
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
            release=run/'installed';oracle.shutil.copytree(payload,release)
            installed_cache=release/generated.relative_to(payload);installed_cache.write_bytes(b'generated installed cache')
            (run/'test-result.json').write_text(json.dumps({'passed':True,'fullSuite':False,'coverageReuse':{'baseRunId':'base'},'payload':expected}))
            self.assertEqual(len(oracle.remove_generated_payload_cache(run,release)),1)
            self.assertEqual(oracle.payload_hashes(release),expected)
            generated.write_bytes(b'cache');source.write_bytes(b'changed code')
            with self.assertRaises(RuntimeError):oracle.remove_generated_payload_cache(run)
            self.assertTrue(generated.exists());source.write_bytes(b'verified module');unknown=payload/'unexpected.py';unknown.write_bytes(b'unreviewed')
            with self.assertRaises(RuntimeError):oracle.remove_generated_payload_cache(run)
            self.assertTrue(unknown.exists());self.assertTrue(generated.exists())

    def test_packaged_psp_cleanup_preserves_source_offline_payload_and_active_build(self):
        with tempfile.TemporaryDirectory() as folder:
            cache=Path(folder);build=cache/'build';(build/'CMakeFiles').mkdir(parents=True)
            (build/'CMakeCache.txt').write_text('CMAKE_HOME_DIRECTORY:INTERNAL='+str(cache/'source')+'\n')
            source=cache/'source';source.mkdir();(source/'source.cpp').write_text('preserve source')
            package=cache/'isolde-ppsspp_1.20.4-1_amd64.deb';package.write_bytes(b'original offline payload')
            receipt=cache/'payload.json';receipt.write_text(json.dumps({'sourceCommit':'fa50bb1976065c4f8b1b47af227d367fe9771555','sha256':oracle.digest(package)}))
            self.assertEqual(oracle.remove_packaged_psp_build(cache,active=[build/'compiler']),[])
            with self.assertRaisesRegex(RuntimeError,'Mounted'):oracle.remove_packaged_psp_build(cache,mounts=[build])
            package.write_bytes(b'changed')
            with self.assertRaisesRegex(RuntimeError,'checksum'):oracle.remove_packaged_psp_build(cache)
            self.assertTrue(build.exists());package.write_bytes(b'original offline payload')
            self.assertEqual(oracle.remove_packaged_psp_build(cache),[str(build)])
            self.assertTrue(source.is_dir());self.assertTrue(package.is_file());self.assertTrue(receipt.is_file())
            with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=0)):
                with patch.object(oracle.subprocess,'run',side_effect=[SimpleNamespace(stdout='fa50bb1976065c4f8b1b47af227d367fe9771555'),SimpleNamespace(stdout=' M local.cpp')]):
                    with self.assertRaisesRegex(RuntimeError,'local'):oracle.remove_packaged_psp_build(cache,prune_source=True)
                self.assertTrue(source.exists())
                with patch.object(oracle.subprocess,'run',side_effect=[SimpleNamespace(stdout='fa50bb1976065c4f8b1b47af227d367fe9771555'),SimpleNamespace(stdout=''),SimpleNamespace(stdout='')]):
                    self.assertEqual(oracle.remove_packaged_psp_build(cache,prune_source=True),[str(source)])
            self.assertTrue(package.is_file());self.assertTrue(receipt.is_file())

    def test_remote_fixture_text_uses_production_keyboard_and_only_dpad(self):
        path=Path(__file__).resolve().parents[1]
        spec=importlib.util.spec_from_file_location('guide_feature_keys',path/'tools/guide_features.py');features=importlib.util.module_from_spec(spec);spec.loader.exec_module(features)
        source=(path/'qml/OnScreenKeyboard.qml').read_text()
        arrays=re.search(r'const letters = shift\s*\?\s*(\[[^\n]+\])\s*:\s*(\[[^\n]+\])',source)
        self.assertIsNotNone(arrays);upper,lower=(json.loads(arrays.group(index)) for index in (1,2))
        prefix=list('1234567890');suffix=['-','_','.','@','#','!','?',',',':','/','{','}','\\','Del','Space','Clear','Done','ABC','123','#+=']
        upper=prefix+upper+suffix;lower=prefix+lower+suffix
        self.assertEqual(features.keyboard_letters(),(upper,lower))
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

    def test_synthetic_media_dedup_preserves_paths_bytes_and_recent_fixtures(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);fixtures=[]
            for n in range(4):
                directory=root/('Isolde-tv-test-'+str(uuid.UUID(int=n+1)))/'Isolde Television Fixture'
                directory.mkdir(parents=True);fixtures.append(directory)
                for name in ('Fixture S00E01.ogv','Fixture S01E01.ogv','Fixture S01E02.ogv'):(directory/name).write_bytes(b'OggS'+b'fixture'*100)
                os.utime(directory.parent,(100+n,100+n))
            records=oracle.deduplicate_fixture_media(root,target_free=2**63)
            self.assertEqual(len(records),2)
            self.assertEqual(len({p.stat().st_ino for p in fixtures[0].glob('*.ogv')}),1)
            self.assertEqual(len({p.stat().st_ino for p in fixtures[-1].glob('*.ogv')}),3)
            for directory in fixtures:
                for path in directory.glob('*.ogv'):self.assertEqual(path.read_bytes(),b'OggS'+b'fixture'*100)
            self.assertEqual(oracle.deduplicate_fixture_media(root,target_free=2**63),[])

    def test_payload_dedup_keeps_verified_candidates_and_refuses_modified_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);files=[]
            for name in ('one','two'):
                run=root/'runs'/name;payload=run/'payload';(payload/'bin').mkdir(parents=True)
                path=payload/'bin/isolde';path.write_bytes(b'fixed tested artifact');files.append(path)
                (run/'test-result.json').write_text(json.dumps({'passed':True,'payload':oracle.payload_hashes(payload)}))
            self.assertEqual(len(oracle.deduplicate_tested_payloads(root,target_free=2**63)),1)
            self.assertEqual(files[0].stat().st_ino,files[1].stat().st_ino)
            self.assertEqual(files[0].read_bytes(),b'fixed tested artifact')
            files[0].write_bytes(b'modified')
            with self.assertRaises(RuntimeError):oracle.deduplicate_tested_payloads(root,target_free=2**63)

    def test_payload_dedup_shares_verified_installed_bytes_without_replacing_release(self):
        with tempfile.TemporaryDirectory() as folder:
            home=Path(folder);root=home/'isolde-oracle';run=root/'runs/passed';payload=run/'payload';(payload/'bin').mkdir(parents=True)
            staged=payload/'bin/isolde';staged.write_bytes(b'fixed installed candidate')
            release=home/'.local/share/isolde-appliance/releases'/('a'*24);(release/'bin').mkdir(parents=True)
            installed=release/'bin/isolde';installed.write_bytes(staged.read_bytes());inode=installed.stat().st_ino
            (run/'test-result.json').write_text(json.dumps(dict(passed=True,payload=oracle.payload_hashes(payload))))
            (run/'deploy-result.json').write_text(json.dumps(dict(release=str(release))))
            with self.assertRaisesRegex(RuntimeError,'Mounted'):oracle.deduplicate_tested_payloads(root,[release],target_free=2**63)
            installed.write_bytes(b'modified deployed candidate')
            with self.assertRaisesRegex(RuntimeError,'Installed payload identity'):oracle.deduplicate_tested_payloads(root,target_free=2**63)
            self.assertEqual(staged.read_bytes(),b'fixed installed candidate');installed.write_bytes(staged.read_bytes())
            records=oracle.deduplicate_tested_payloads(root,target_free=2**63)
            self.assertEqual(len(records),1);self.assertEqual(installed.stat().st_ino,inode)
            self.assertEqual(staged.stat().st_ino,inode);self.assertEqual(installed.read_bytes(),b'fixed installed candidate')

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
        original_read=oracle.Path.read_text
        def read_text(path,*args,**kwargs):
            if path==Path('/proc/self/mountinfo'):return '1 0 0:1 / / rw - ext4 /dev/unit-test rw\n'
            return original_read(path,*args,**kwargs)
        with patch.object(runner,"command",return_value="isolde-test"),patch.object(runner,"stage") as stage,patch.object(oracle.Path,"home",return_value=home),patch.object(oracle.Path,"read_text",read_text),patch.object(oracle.shutil,"disk_usage",return_value=SimpleNamespace(free=7*1024**3)):
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

    def test_studio_helper_preserves_frozen_paths_arguments_and_dependencies(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary).resolve();source=root/'source';folder=source/'tools';folder.mkdir(parents=True)
            frozen=folder/'home-components-acceptance.py';frozen.write_text("raise RuntimeError('frozen helper must remain untouched')")
            (folder/'frozen_dependency.py').write_text("VALUE='frozen dependency'")
            code="import sys,json,frozen_dependency;print(json.dumps({'file':__file__,'args':sys.argv,'dependency':frozen_dependency.VALUE}))"
            expected=hashlib.sha256(code.encode()).hexdigest();helper=root/'bin'/('studio-helper-'+expected+'.py');helper.parent.mkdir();helper.write_bytes(code.encode())
            self.assertEqual(oracle.verified_studio_helper(root,expected),helper)
            executable=root/'payload/bin/isolde candidate'
            command=oracle.studio_helper_command(source,helper,executable);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(frozen),'args':[str(frozen),str(executable),'--studio','--inspect'],'dependency':'frozen dependency'})
            command=oracle.studio_helper_command(source,helper,executable,desktop=True);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(frozen),'args':[str(frozen),str(executable),'--studio','--desktop','--inspect'],'dependency':'frozen dependency'})
            command=oracle.studio_helper_command(source,helper,executable,collections=True);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(frozen),'args':[str(frozen),str(executable),'--studio','--collections','--inspect'],'dependency':'frozen dependency'})
            history_frozen=folder/'tv-library-acceptance.py';history_frozen.write_text("raise RuntimeError('frozen history helper untouched')")
            history_helper=root/'bin'/('history-helper-'+expected+'.py');history_helper.write_bytes(code.encode())
            self.assertEqual(oracle.verified_studio_helper(root,expected,'history'),history_helper)
            command=oracle.history_helper_command(source,history_helper,executable);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(history_frozen),'args':[str(history_frozen),str(executable),'--history','--inspect'],'dependency':'frozen dependency'})
            self.assertIn('untouched',history_frozen.read_text())
            command=oracle.history_helper_command(source,history_helper,executable,pages=True);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(history_frozen),'args':[str(history_frozen),str(executable),'--pages','--inspect'],'dependency':'frozen dependency'})
            command=oracle.history_helper_command(source,history_helper,executable,playlists=True);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(history_frozen),'args':[str(history_frozen),str(executable),'--playlists','--inspect'],'dependency':'frozen dependency'})
            with self.assertRaises(ValueError):oracle.history_helper_command(source,history_helper,executable,pages=True,playlists=True)
            for case,flags in [('games',[]),('emulators',['--emulator'])]:
                command=oracle.history_helper_command(source,history_helper,executable,game_case=case);command[0]=sys.executable
                result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
                game_frozen=folder/'games-acceptance.py'
                self.assertEqual(result,{'file':str(game_frozen),'args':[str(game_frozen),str(executable)]+flags,'dependency':'frozen dependency'})
            with self.assertRaises(ValueError):oracle.history_helper_command(source,history_helper,executable,game_case='foreign')
            reference_frozen=folder/'home-reference-acceptance.py';reference_frozen.write_text("raise RuntimeError('frozen reference helper untouched')")
            reference_helper=root/'bin'/('reference-helper-'+expected+'.py');reference_helper.write_bytes(code.encode())
            self.assertEqual(oracle.verified_studio_helper(root,expected,'reference'),reference_helper)
            flags=['--skin-from-settings','--inspect','--case',str(source/'tests/visual/home-reference')]
            command=oracle.reference_helper_command(source,reference_helper,executable,flags);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'file':str(reference_frozen),'args':[str(reference_frozen),str(executable)]+flags,'dependency':'frozen dependency'})
            self.assertIn('untouched',reference_frozen.read_text())
            (folder/'reference_collections.py').write_text("VALUE='frozen collections'")
            collections_code="VALUE='verified overlay collections'"
            collections_digest=hashlib.sha256(collections_code.encode()).hexdigest()
            collections_helper=root/'bin'/('reference-collections-helper-'+collections_digest+'.py')
            collections_helper.write_text(collections_code)
            self.assertEqual(oracle.verified_studio_helper(root,collections_digest,'reference-collections'),collections_helper)
            reference_helper.write_text("import json,reference_collections;print(json.dumps({'value':reference_collections.VALUE,'file':__file__}))")
            command=oracle.reference_helper_command(source,reference_helper,executable,flags,collections_helper);command[0]=sys.executable
            result=json.loads(subprocess.run(command,check=True,capture_output=True,text=True).stdout)
            self.assertEqual(result,{'value':'verified overlay collections','file':str(reference_frozen)})
            self.assertEqual((folder/'reference_collections.py').read_text(),"VALUE='frozen collections'")
            collections_helper.write_text('tampered')
            with self.assertRaises(RuntimeError):oracle.verified_studio_helper(root,collections_digest,'reference-collections')
            reference_helper.write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):oracle.verified_studio_helper(root,expected,'reference')
            with self.assertRaises(RuntimeError):oracle.verified_studio_helper(root,expected,'unreviewed')
            self.assertIn('must remain untouched',frozen.read_text())
            helper.write_bytes(b'changed')
            with self.assertRaises(RuntimeError):oracle.verified_studio_helper(root,expected)
            with self.assertRaises(RuntimeError):oracle.verified_studio_helper(root,'../bad')

    def test_focused_guide_qml_reuse_requires_unchanged_production_and_original_full_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary).resolve();base=root/'runs/base';current=root/'runs/focused'
            base.mkdir(parents=True);current.mkdir()
            files={'qml/GuideWidget.qml':b'old','src/main.cpp':b'core','CMakeLists.txt':b'build',
                   'tests/guide_ui.cpp':b'tests','docs/review.md':b'notes'}
            def write_manifest(run,values):
                source=run/'source';source.mkdir(exist_ok=True)
                for name,data in values.items():
                    path=source/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
                (run/'files.sha256').write_text(''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in sorted(values.items())))
            write_manifest(base,files);fresh=dict(files,**{'qml/GuideWidget.qml':b'new','tests/guide_ui.cpp':b'regression'})
            write_manifest(current,fresh)
            for run in (base,current):
                executable=run/'payload/bin/isolde';executable.parent.mkdir(parents=True);executable.write_bytes(run.name.encode())
            baseline={'passed':True,'fullSuite':True,'sourceDigest':oracle.digest(base/'files.sha256'),'payload':oracle.payload_hashes(base/'payload')}
            oracle.atomic_json(base/'test-result.json',baseline)
            proof=oracle.guide_qml_coverage(root,current,'base','guide_remote_ui')
            self.assertEqual(proof['changedGuideQml'],['qml/GuideWidget.qml'])
            runner=oracle.Oracle(root,'focused')
            oracle.atomic_json(current/'test-result.json',{'passed':True,'fullSuite':False,'suite':'guide_remote_ui','coverageReuse':proof,'payload':oracle.payload_hashes(current/'payload')})
            self.assertEqual(runner.tested_payload(),current/'payload')
            for suite in ('all','guide','studio'):
                with self.assertRaises(RuntimeError):oracle.guide_qml_coverage(root,current,'base',suite)
            with self.assertRaises(RuntimeError):oracle.guide_qml_coverage(root,current,'../base','guide_remote_ui')
            for name in ('src/main.cpp','CMakeLists.txt','tools/unreviewed-runtime.py'):
                write_manifest(current,dict(fresh,**{name:b'changed'}))
                with self.assertRaises(RuntimeError):oracle.guide_qml_coverage(root,current,'base','guide_remote_ui')
            write_manifest(current,dict(fresh,**{'qml/GuideView.qml':b'added'}))
            with self.assertRaises(RuntimeError):oracle.guide_qml_coverage(root,current,'base','guide_remote_ui')
            write_manifest(current,fresh)
            oracle.atomic_json(base/'test-result.json',dict(baseline,fullSuite=False))
            with self.assertRaises(RuntimeError):oracle.guide_qml_coverage(root,current,'base','guide_remote_ui')
            oracle.atomic_json(base/'test-result.json',baseline)
            (base/'payload/bin/isolde').write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):runner.tested_payload()

    def test_focused_studio_qml_reuse_refuses_config_cpp_and_unverified_payloads(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary).resolve();base=root/'runs/base';current=root/'runs/focused'
            base.mkdir(parents=True);current.mkdir()
            files={'qml/ComposedHome.qml':b'old','qml/PlacedComponents.qml':b'old','qml/AppGallery.qml':b'old',
                   'src/main.cpp':b'core','config/skins/pastel.json':b'config',
                   'tests/tst_home_navigation.cpp':b'tests','CMakeLists.txt':b'build'}
            def write_manifest(run,values):
                source=run/'source';source.mkdir(exist_ok=True)
                for name,data in values.items():
                    path=source/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
                (run/'files.sha256').write_text(''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in sorted(values.items())))
            write_manifest(base,files)
            fresh=dict(files,**{'qml/PlacedComponents.qml':b'focus fix','tests/tst_home_navigation.cpp':b'regression','docs/test-campaign-2026-11-09.md':b'new persistent report'})
            write_manifest(current,fresh)
            for run in (base,current):
                executable=run/'payload/bin/isolde';executable.parent.mkdir(parents=True);executable.write_bytes(run.name.encode())
            baseline={'passed':True,'fullSuite':True,'sourceDigest':oracle.digest(base/'files.sha256'),'payload':oracle.payload_hashes(base/'payload')}
            oracle.atomic_json(base/'test-result.json',baseline)
            proof=oracle.focused_qml_coverage(root,current,'base','home_component_navigation')
            self.assertEqual(proof['changedStudioQml'],['qml/PlacedComponents.qml'])
            write_manifest(current,dict(files,**{'qml/AppGallery.qml':b'bounded app icon fix'}))
            gallery_proof=oracle.focused_qml_coverage(root,current,'base','home_component_navigation')
            self.assertEqual(gallery_proof['changedStudioQml'],['qml/AppGallery.qml'])
            write_manifest(current,fresh)
            runner=oracle.Oracle(root,'focused')
            oracle.atomic_json(current/'test-result.json',{'passed':True,'fullSuite':False,'suite':'home_component_navigation','coverageReuse':proof,'payload':oracle.payload_hashes(current/'payload')})
            self.assertEqual(runner.tested_payload(),current/'payload')
            for name in ('src/main.cpp','config/skins/pastel.json','CMakeLists.txt','qml/GuideWidget.qml','tools/unreviewed.py','docs/unreviewed-new.md'):
                write_manifest(current,dict(fresh,**{name:b'changed'}))
                with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','home_component_navigation')
            write_manifest(current,fresh)
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','studio')
            oracle.atomic_json(base/'test-result.json',dict(baseline,fullSuite=False))
            with self.assertRaises(RuntimeError):runner.tested_payload()
            oracle.atomic_json(base/'test-result.json',baseline)
            (base/'payload/bin/isolde').write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):runner.tested_payload()

    def test_studio_inspector_qml_reuse_is_bounded_to_picker_ui(self):
        root=self.root; base=root/'runs/inspector-base';current=root/'runs/inspector-focused'
        base.mkdir(parents=True);current.mkdir()
        files={'qml/StudioInspector.qml':b'old','qml/StudioAppPicker.qml':b'picker','qml/PageLibraryWidget.qml':b'library','tests/studio/tst_selection.qml':b'tests','src/main.cpp':b'core','config/studio-defaults.json':b'config','qml/PlacedComponents.qml':b'placed'}
        def write_manifest(run,values):
            (run/'files.sha256').write_text(''.join(hashlib.sha256(value).hexdigest()+'  '+name+'\n' for name,value in sorted(values.items())))
        write_manifest(base,files);fresh=dict(files,**{'qml/StudioInspector.qml':b'caption fix','tests/studio/tst_selection.qml':b'actual input regression'});write_manifest(current,fresh)
        (base/'payload/bin').mkdir(parents=True);(base/'payload/bin/isolde').write_bytes(b'full-tested')
        result=dict(passed=True,fullSuite=True,sourceDigest=oracle.digest(base/'files.sha256'),payload=oracle.payload_hashes(base/'payload'));oracle.atomic_json(base/'test-result.json',result)
        proof=oracle.focused_qml_coverage(root,current,'inspector-base','studio_selection')
        self.assertEqual(proof['changedStudioInspectorQml'],['qml/StudioInspector.qml'])
        write_manifest(current,dict(fresh,**{'qml/PageLibraryWidget.qml':b'empty-update focus'}))
        renderer_proof=oracle.focused_qml_coverage(root,current,'inspector-base','studio_selection')
        self.assertEqual(renderer_proof['changedStudioInspectorQml'],['qml/PageLibraryWidget.qml','qml/StudioInspector.qml'])
        for name in ('src/main.cpp','config/studio-defaults.json','qml/PlacedComponents.qml','qml/Unreviewed.qml'):
            write_manifest(current,dict(fresh,**{name:b'changed'}))
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'inspector-base','studio_selection')
        write_manifest(current,fresh);oracle.atomic_json(base/'test-result.json',dict(result,fullSuite=False))
        with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'inspector-base','studio_selection')
        oracle.atomic_json(base/'test-result.json',result);(base/'payload/bin/isolde').write_bytes(b'modified')
        with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'inspector-base','studio_selection')

    def test_theme_journey_fixture_and_required_receipt_boundaries(self):
        phases=['storediscoveryentry','storediscoveryloading','storediscoverybrowse','storediscoverykeyboard','storediscoveryresults','storediscoveryreturn','storediscoverysettingsfocus','storediscoverysettings']
        live=dict(passed=True,journey='store-discovery',freshProfile=True,normalProfileUsed=False,catalogOnly=True,installedAppsChanged=False,provider='fdroid-signed-v2',query='newpipe',matches=['org.schabi.newpipe'],indexSha256='a'*64,entrySha256='b'*64,actualInputActions=30,captures=phases)
        self.assertTrue(oracle.visual_result_passed('store-discovery',live))
        self.assertFalse(oracle.visual_result_passed('store-discovery',dict(live,installedAppsChanged=True)))
        self.assertFalse(oracle.visual_result_passed('store-discovery',dict(live,indexSha256='unsigned')))
        self.assertFalse(oracle.visual_result_passed('store-discovery',dict(live,captures=phases[:-1])))
        self.assertEqual(oracle.CASES['store-discovery'],('store-acceptance.py',['--discovery-only']))
        store_helper=Path(__file__).resolve().parents[1]/'tools/store-acceptance.py'
        compile(store_helper.read_bytes(),str(store_helper),'exec')
        source=Path(__file__).resolve().parents[1]/'tools/home-components-acceptance.py'
        spec=importlib.util.spec_from_file_location('theme_fixture',source)
        helper=importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules,{'fcntl':Mock()}):spec.loader.exec_module(helper)
        response={'movies':[{'title':'Moonlit Harbor'},{'title':'The Glass Garden'}],'limits':{'start':0,'end':2,'total':2}}
        request={'params':{'filter':{'field':'title','operator':'contains','value':'MOONLIT'}}}
        filtered=helper.filter_title_response(request,response)
        self.assertEqual(filtered['movies'],[{'title':'Moonlit Harbor'}])
        self.assertEqual(filtered['limits'],{'start':0,'end':1,'total':1})
        self.assertEqual(len(response['movies']),2)
        request['params']['filter']['value']='settings'
        self.assertEqual(helper.filter_title_response(request,response)['movies'],[])
        self.assertIs(helper.filter_title_response({},response),response)
        request['params']['filter']['operator']='execute'
        with self.assertRaises(ValueError):helper.filter_title_response(request,response)
        for index in range(3):
            doc=helper.theme_document(index)
            self.assertNotEqual(doc['theme'],doc['pages'][0]['layout']['theme'])
            self.assertEqual(doc['components'][0]['action']['type'],'open-page')
            self.assertEqual(doc['homePageId'],'theme-home')
            self.assertEqual(doc['schemaVersion'],2)
            self.assertTrue(all(page['layout']['schemaVersion']==2 and page['layout']['components'] for page in doc['pages']))
            self.assertNotEqual(doc['pages'][0]['layout']['theme'],doc['pages'][1]['layout']['theme'])
            for item in doc['components']+doc['pages'][0]['layout']['components']+doc['pages'][1]['layout']['components']:
                self.assertRegex(item['id'],r'^[A-Za-z0-9_-]+$')
        self.assertEqual(json.loads((Path(__file__).resolve().parents[1]/'tests/fixtures/shell-theme-routes.json').read_text()),[helper.theme_document(i) for i in range(3)])
        self.assertFalse(oracle.visual_result_passed('themes',{'passed':True}))
        phases=('home','origin','search','searchkeyboard','searchreturn','gallery','galleryreturn','movies','details','shows','seasons','episodes','network','networkkeyboard','sources','remotes','profiles','vpn','storepreferences','system','display','audio','ambient','media','folders','import','components','inputs','apps','appdialog','appkeyboard','return')
        receipt=dict(passed=True,journey='themes',freshProfile=True,normalProfileUsed=False,palettes=['dark','pastel','custom'],phases=[tag+phase for tag in ('dark','pastel','custom') for phase in phases]+['custompolicy','customreset','customcancel'],layoutBytesUnchanged=True,nativeKodiBefore={'sources.xml':'kept'},nativeKodiAfter={'sources.xml':'kept'})
        self.assertTrue(oracle.visual_result_passed('themes',receipt))
        receipt['phases'].remove('pastelimport');self.assertFalse(oracle.visual_result_passed('themes',receipt))
        custom=dict(receipt,journey='themecustom',palettes=['custom'],phases=[p for p in receipt['phases'] if p.startswith('custom')])
        self.assertTrue(oracle.visual_result_passed('themecustom',custom))
        custom['phases'].remove('customimport');self.assertFalse(oracle.visual_result_passed('themecustom',custom))

        command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/installed'),themes=True)
        self.assertIn('--themes-only',command)
        self.assertEqual(oracle.CASES['themes'][0],'home-components-acceptance.py')

    def test_original_full_payload_can_verify_exact_retained_release_after_cache_prune(self):
        home=self.root/'home';home.mkdir();base=self.root/'base';base.mkdir()
        releases=home/'.local/share/isolde-appliance/releases'
        release=releases/('a'*24);(release/'bin').mkdir(parents=True);(release/'bin/isolde').write_bytes(b'full tested executable')
        result={'payload':oracle.payload_hashes(release)}
        (base/'deploy-result.json').write_text(json.dumps({'release':str(release)}))
        with patch.object(oracle.Path,'home',return_value=home):
            self.assertTrue(oracle.original_full_payload_matches(base,result))
            (release/'bin/isolde').write_bytes(b'modified')
            self.assertFalse(oracle.original_full_payload_matches(base,result))
            (base/'deploy-result.json').write_text(json.dumps({'release':str(self.root/'outside')}))
            with self.assertRaises(RuntimeError):oracle.original_full_payload_matches(base,result)
            (base/'payload/bin').mkdir(parents=True);(base/'payload/bin/isolde').write_bytes(b'changed staged file')
            self.assertFalse(oracle.original_full_payload_matches(base,result))

    def test_reference_socket_timeout_retains_partial_request_diagnostics(self):
        source=Path(__file__).resolve().parents[1]/'tools/reference_collections.py'
        spec=importlib.util.spec_from_file_location('reference_timeout',source)
        helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
        for partial in (b'',b'{'):
            provider=helper.ReferenceCollections.__new__(helper.ReferenceCollections)
            provider.stopped=threading.Event();provider.requests=[];provider.errors=[];provider.idle_connections=[]
            connection=Mock();connection.recv.side_effect=([partial] if partial else [])+[helper.socket.timeout('timed out')]
            owner=Mock();owner.__enter__=Mock(return_value=connection);owner.__exit__=Mock(return_value=False)
            owner.recv=connection.recv
            provider.socket=Mock();provider.socket.accept.side_effect=[(owner,None),OSError('closed')]
            provider.serve()
            self.assertEqual(provider.errors,[dict(error='timed out',stage='read',receivedBytes=len(partial),method=None)] if partial else [])
            self.assertEqual(provider.idle_connections,[] if partial else [dict(error='timed out',receivedBytes=0)])
            self.assertEqual(provider.requests,[])

    def test_theme_controls_receipt_requires_all_palettes_and_preservation(self):
        phases=[tag+phase for tag in ('dark','pastel','custom') for phase in ('audio','ambient','return')]+['custompolicy','customreset','customcancel']
        receipt=dict(passed=True,journey='themecontrols',freshProfile=True,normalProfileUsed=False,palettes=['dark','pastel','custom'],phases=phases,layoutBytesUnchanged=True,nativeKodiBefore={'sources.xml':'kept'},nativeKodiAfter={'sources.xml':'kept'})
        self.assertTrue(oracle.visual_result_passed('themecontrols',receipt))
        for field,value in [('phases',phases[:-1]),('nativeKodiAfter',{}),('layoutBytesUnchanged',False),('normalProfileUsed',True)]:
            self.assertFalse(oracle.visual_result_passed('themecontrols',dict(receipt,**{field:value})))
        command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),themecontrols=True)
        self.assertIn('--themes-controls-only',command)
        self.assertEqual(oracle.CASES['themecontrols'][0],'home-components-acceptance.py')

    def test_theme_controls_sound_receipt_requires_real_positive_and_silence(self):
        phases=[tag+phase for tag in ('dark','pastel','custom') for phase in ('audio','ambient','return','sounds','soundpreview','sounddisabled')]+['custompolicy','customreset','customcancel']
        observations=[dict(palette=tag,case=kind,bytes=192000,peak=1000 if kind=='preview' else 0,nonzero=1000 if kind=='preview' else 0,sha256='a'*64) for tag in ('dark','pastel','custom') for kind in ('preview','disabled')]
        receipt=dict(passed=True,journey='themecontrols',freshProfile=True,normalProfileUsed=False,palettes=['dark','pastel','custom'],phases=phases,layoutBytesUnchanged=True,nativeKodiBefore={'sources.xml':'kept'},nativeKodiAfter={'sources.xml':'kept'},soundAcceptanceVersion=1,soundOutput=observations)
        self.assertTrue(oracle.visual_result_passed('themecontrols',receipt))
        for field,value in [('soundAcceptanceVersion',True),('soundAcceptanceVersion',2),('soundOutput',[]),('phases',phases[1:])]:
            self.assertFalse(oracle.visual_result_passed('themecontrols',dict(receipt,**{field:value})))
        for index,field,value in [(0,'peak',0),(1,'peak',1),(1,'nonzero',1),(0,'bytes',0),(0,'sha256',''),(0,'nonzero',True)]:
            changed=[dict(item) for item in observations];changed[index][field]=value
            self.assertFalse(oracle.visual_result_passed('themecontrols',dict(receipt,soundOutput=changed)))

    def test_owned_x11_frame_capture_preserves_pixels_and_releases_native_image(self):
        import ast,ctypes
        from PIL import Image
        helper=Path(__file__).resolve().parents[1]/'tools/home-components-acceptance.py'
        function=next(n for n in ast.parse(helper.read_text(encoding='utf-8')).body if isinstance(n,ast.FunctionDef) and n.name=='capture_x11_window')
        namespace={'ctypes':ctypes};exec(compile(ast.Module(body=[function],type_ignores=[]),str(helper),'exec'),namespace)
        pixels=ctypes.create_string_buffer(bytes([0,0,255,0,0,255,0,0]))
        frame=SimpleNamespace(contents=SimpleNamespace(width=2,height=1,byte_order=0,bits_per_pixel=32,red_mask=0xff0000,green_mask=0xff00,blue_mask=0xff,data=ctypes.addressof(pixels),bytes_per_line=8))
        def geometry(*args):args[5]._obj.value=2;args[6]._obj.value=1;return 1
        x11=SimpleNamespace(XGetGeometry=Mock(side_effect=geometry),XGetImage=Mock(return_value=frame),XDestroyImage=Mock())
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'frame.png';namespace['capture_x11_window'](x11,1,0x123,target)
            with Image.open(target) as rendered:self.assertEqual([rendered.getpixel((x,0)) for x in range(2)],[(255,0,0),(0,255,0)])
            self.assertEqual(x11.XGetImage.call_args.args[1],0x123);x11.XDestroyImage.assert_called_once_with(frame)
            frame.contents.bits_per_pixel=16
            with self.assertRaisesRegex(RuntimeError,'pixel format'):namespace['capture_x11_window'](x11,1,0x123,target)
            self.assertEqual(x11.XDestroyImage.call_count,2)

    def test_theme_fault_receipt_requires_loading_error_recovery_and_native_preservation(self):
        phases=[tag+phase for tag in ('dark','pastel','custom') for phase in ('gallery','loading','error','retry','return')]
        receipt=dict(passed=True,journey='themefaults',freshProfile=True,normalProfileUsed=False,palettes=['dark','pastel','custom'],phases=phases,layoutBytesUnchanged=True,nativeKodiBefore={'sources.xml':'kept'},nativeKodiAfter={'sources.xml':'kept'},controlledFaults=3,loadingFrames={tag+'loading-frame.png':'a'*64 for tag in ('dark','pastel','custom')})
        self.assertTrue(oracle.visual_result_passed('themefaults',receipt))
        for field,value in (('controlledFaults',2),('phases',phases[:-1]),('nativeKodiAfter',{'sources.xml':'changed'})):
            self.assertFalse(oracle.visual_result_passed('themefaults',dict(receipt,**{field:value})))
        command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),themefaults=True)
        self.assertIn('--themes-faults-only',command)
        self.assertEqual(oracle.CASES['themefaults'][0],'home-components-acceptance.py')

    def test_shell_keyboard_reuse_refuses_uncovered_runtime_changes(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder).resolve();base=root/'runs/base';current=root/'runs/focused'
            base.mkdir(parents=True);current.mkdir()
            values={'qml/NetworkSettingsView.qml':b'old','qml/SettingsView.qml':b'parent','src/home/HomeLayout.cpp':b'owner','config/studio-defaults.json':b'defaults'}
            def write(run,data):
                (run/'files.sha256').write_text(''.join(hashlib.sha256(value).hexdigest()+'  '+name+'\n' for name,value in sorted(data.items())))
            write(base,values);changed=dict(values,**{'qml/NetworkSettingsView.qml':b'focus ownership','tests/fixtures/shell-theme-routes.json':b'fixture'})
            write(current,changed);(base/'payload/bin').mkdir(parents=True);(base/'payload/bin/isolde').write_bytes(b'full candidate')
            oracle.atomic_json(base/'test-result.json',{'passed':True,'fullSuite':True,'sourceDigest':oracle.digest(base/'files.sha256'),'payload':oracle.payload_hashes(base/'payload')})
            proof=oracle.focused_qml_coverage(root,current,'base','shell_appearance')
            self.assertEqual(proof['changedShellKeyboardQml'],['qml/NetworkSettingsView.qml'])
            for name in ('qml/SettingsView.qml','src/home/HomeLayout.cpp','config/studio-defaults.json','qml/Unexpected.qml'):
                write(current,dict(changed,**{name:b'uncovered'}))
                with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','shell_appearance')
            write(current,changed);(base/'payload/bin/isolde').write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','shell_appearance')

    def test_shell_appearance_batch_selects_all_three_boundaries(self):
        targets,pattern=oracle.suite_selection('shell_appearance')
        self.assertEqual(set(targets),{'isolde_studio_quick_tests','isolde_home_navigation_tests','isolde_surface_appearance_tests'})
        for name in ('studio_selection','studio_shell','surface_appearance_contracts','home_component_navigation'):
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIsNone(re.fullmatch(pattern,'studio_performance'))

    def test_studio_suite_builds_only_matching_test_targets(self):
        targets, pattern = oracle.suite_selection("studio")
        self.assertEqual(targets, ["isolde_studio_quick_tests", "isolde_component_tests", "isolde_home_navigation_tests", "isolde_studio_design_tests"])
        for name in ("studio_quick", "studio_selection", "studio_artwork", "studio_colors", "studio_workflows", "studio_gestures", "studio_grid", "studio_grid_controls", "studio_destinations", "studio_ordered", "studio_policies", "studio_overlays", "studio_unit", "studio_behavior"):
            self.assertIsNotNone(re.fullmatch(pattern, name))
        self.assertIsNone(re.fullmatch(pattern, "home_component_navigation"))

    def test_campaign_repairs_includes_every_failed_registration_without_slow_studio_replay(self):
        targets,pattern=oracle.suite_selection('campaign_repairs')
        for name in ('games_remote_ui','studio_navigation','studio_behavior','audio_backend','games_provisioning','steam_games','optical_exec_adapter','native_profile_isolation'):
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIn('isolde_native_sandbox_tests',targets)
        self.assertIn('isolde_audio_tests',targets)
        self.assertIsNone(re.fullmatch(pattern,'studio_performance'))
        self.assertIsNone(re.fullmatch(pattern,'studio_cutover'))

    def test_runtime_contracts_cover_install_process_and_sandbox_boundaries(self):
        targets,pattern=oracle.suite_selection('runtime_contracts')
        self.assertEqual(targets,['isolde_native_sandbox_tests'])
        for name in ('games_provisioning','steam_games','portable_game_installer','optical_exec_adapter','native_profile_isolation'):
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIsNone(re.fullmatch(pattern,'studio_navigation'))
        self.assertIsNone(re.fullmatch(pattern, "studio_quick_extra"))
        self.assertEqual(oracle.suite_selection("studio_quick"), (["isolde_studio_quick_tests"], "^studio_quick$"))

    def test_safety_partition_is_bounded_and_included_in_studio(self):
        self.assertEqual(oracle.suite_selection("studio_safety"), (["isolde_studio_quick_tests"], "^studio_safety$"))
        pattern = oracle.suite_selection("studio")[1]
        self.assertIsNotNone(re.fullmatch(pattern, "studio_safety"))
        self.assertIsNone(re.fullmatch(pattern, "studio_safety_extra"))

    def test_closure_regressions_cover_all_named_failures_and_performance(self):
        targets,pattern=oracle.suite_selection("studio_closure_regressions")
        self.assertEqual(targets,["isolde_studio_quick_tests","isolde_studio_design_tests","isolde_home_navigation_tests"])
        for name in ["studio_quick","studio_colors","studio_color_interactions","home_component_navigation","studio_design_contracts","studio_performance","studio_cutover"]:
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIsNone(re.fullmatch(pattern,"studio_navigation"))
        self.assertIsNotNone(re.fullmatch(oracle.suite_selection("studio_colors")[1],"studio_color_interactions"))

    def test_performance_suite_includes_placement_and_page_style_regressions(self):
        targets,pattern=oracle.suite_selection("studio_performance")
        self.assertEqual(targets,["isolde_studio_quick_tests","isolde_studio_design_tests","isolde_component_tests"])
        for name in ["studio_performance","studio_design_contracts","component_contracts"]:
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIsNone(re.fullmatch(pattern,"studio_quick"))

    def test_design_journey_requires_observed_input_and_every_supported_workflow(self):
        required=('remoteSharedStyle','sharedImpactPreview','myComponents','linkedInsertDetach','freshIdentities','allFourTemplatesCustomized','currentPagePreservedRoot','namedMilestone','versionPreviewCancelNoWrite','defaultsApplyVersionRestoreExact','remotePortableExportImport','privateReferenceChooseHideRemove','coldReopenPreservedBytes','offlineSocketFilter')
        result={'passed':True,'journey':'studio-designs','freshProfile':True,'normalProfileUsed':False,
                'actualInputActions':1,'captureTrace':[{}]*20,'designs':dict.fromkeys(required,True)}
        self.assertTrue(oracle.visual_result_passed('studio-designs',result))
        for key in required:
            missing=dict(result,designs=dict(result['designs'],**{key:False}))
            self.assertFalse(oracle.visual_result_passed('studio-designs',missing),key)
        self.assertFalse(oracle.visual_result_passed('studio-designs',dict(result,actualInputActions=0)))
        self.assertFalse(oracle.visual_result_passed('studio-designs',dict(result,captureTrace=[])))
        self.assertFalse(oracle.visual_result_passed('studio-designs',{'passed':True}))

    def test_studio_startup_focuses_the_new_window_before_outer_assignment(self):
        import ast
        helper = Path(__file__).resolve().parents[1] / 'tools/home-components-acceptance.py'
        tree = ast.parse(helper.read_text(encoding='utf-8'))
        startup = next(node for node in ast.walk(tree) if isinstance(node, ast.FunctionDef) and node.name == 'start_shell')
        child = SimpleNamespace(pid=123, poll=lambda: None)
        focus = Mock(return_value=0x123)
        namespace = {'offline_module': None, 'executable': Path('/app'), 'env': {}, 'log': None,
                     'subprocess': SimpleNamespace(Popen=Mock(return_value=child)),
                     'run': Mock(return_value='0x123 0 123 isolde'), 'focus_window': focus, 'time': time}
        exec(compile(ast.Module(body=[startup], type_ignores=[]), str(helper), 'exec'), namespace)
        self.assertEqual(namespace['start_shell'](), (child, '0x123', 0x123))
        focus.assert_called_once_with('0x123')

    def test_visual_orphan_recovery_refuses_foreign_process_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);run=root/'run';proc=root/'proc/123';proc.mkdir(parents=True);(run/'payload/bin').mkdir(parents=True)
            (proc/'exe').write_bytes(b'foreign executable');(run/'payload/bin/isolde').write_bytes(b'tested executable')
            with patch.object(oracle.socket,'gethostname',return_value='isolde-test'), patch.object(oracle.os,'getuid',return_value=proc.stat().st_uid,create=True), patch.object(oracle.os,'kill') as kill:
                with self.assertRaisesRegex(RuntimeError,'tested executable'):oracle.recover_visual_orphan(run,123,root/'proc')
                kill.assert_not_called()
            with patch.object(oracle.socket,'gethostname',return_value='isolde-test'), patch.object(oracle.os,'getuid',return_value=proc.stat().st_uid+1,create=True), patch.object(oracle.os,'kill') as kill:
                with self.assertRaisesRegex(RuntimeError,'another user'):oracle.recover_visual_orphan(run,123,root/'proc')
                kill.assert_not_called()

    def test_studio_startup_waits_only_for_transient_empty_x11_client_list(self):
        import ast
        helper=Path(__file__).resolve().parents[1]/'tools/home-components-acceptance.py'
        startup=next(n for n in ast.walk(ast.parse(helper.read_text(encoding='utf-8'))) if isinstance(n,ast.FunctionDef) and n.name=='start_shell')
        child=SimpleNamespace(pid=123,poll=lambda:None,terminate=Mock(),wait=Mock())
        process=SimpleNamespace(Popen=Mock(return_value=child),CalledProcessError=subprocess.CalledProcessError)
        namespace=dict(offline_module=None,executable=Path('/app'),env={},log=None,subprocess=process,
            run=Mock(side_effect=[subprocess.CalledProcessError(1,['wmctrl'],stderr='Cannot get client list properties.'),'0x123 0 123 isolde','']),focus_window=Mock(return_value=0x123),time=SimpleNamespace(sleep=Mock()))
        exec(compile(ast.Module(body=[startup],type_ignores=[]),str(helper),'exec'),namespace)
        self.assertEqual(namespace['start_shell'](),(child,'0x123',0x123))
        namespace['run']=Mock(side_effect=[subprocess.CalledProcessError(1,['wmctrl'],output='',stderr=''),'0x123 0 123 isolde',''])
        self.assertEqual(namespace['start_shell']()[1],'0x123')
        namespace['run']=Mock(side_effect=[subprocess.CalledProcessError(1,['wmctrl'],stderr='BadWindow (invalid Window parameter)\nMajor opcode of failed request: 20 (X_GetProperty)'),'0x123 0 123 isolde',''])
        self.assertEqual(namespace['start_shell']()[1],'0x123')
        namespace['run']=Mock(side_effect=subprocess.CalledProcessError(1,['wmctrl'],stderr='Cannot open display'))
        with self.assertRaises(subprocess.CalledProcessError):namespace['start_shell']()
        child.terminate.assert_called_once()

    def test_studio_startup_failure_stops_its_unassigned_child(self):
        import ast
        helper = Path(__file__).resolve().parents[1] / 'tools/home-components-acceptance.py'
        tree = ast.parse(helper.read_text(encoding='utf-8'))
        startup = next(node for node in ast.walk(tree) if isinstance(node, ast.FunctionDef) and node.name == 'start_shell')
        child = SimpleNamespace(pid=123, poll=lambda: None, terminate=Mock(), wait=Mock())
        namespace = {'offline_module': None, 'executable': Path('/app'), 'env': {}, 'log': None,
                     'subprocess': SimpleNamespace(Popen=Mock(return_value=child)),
                     'run': Mock(return_value='0x123 0 123 isolde'), 'focus_window': Mock(side_effect=RuntimeError('focus failed')), 'time': time}
        exec(compile(ast.Module(body=[startup], type_ignores=[]), str(helper), 'exec'), namespace)
        with self.assertRaisesRegex(RuntimeError,'focus failed'): namespace['start_shell']()
        child.terminate.assert_called_once();child.wait.assert_called_once_with(timeout=10)

    def test_design_journey_uses_the_frozen_source_helper_boundary(self):
        command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),designs=True)
        self.assertIn('--designs-only',command)
        self.assertIn('--studio',command)
        self.assertIn(str(Path('/frozen')/'tools/home-components-acceptance.py'),command)
        self.assertEqual(oracle.CASES['studio-designs'][1],['--studio','--designs-only','--inspect'])

    def test_remaining_suite_reuses_existing_failure_registrations(self):
        targets,pattern=oracle.suite_selection("studio_remaining")
        self.assertEqual(targets,["isolde_studio_quick_tests","isolde_studio_design_tests","isolde_component_tests"])
        for name in ["studio_quick","studio_performance","studio_design_contracts","component_contracts"]:
            self.assertIsNotNone(re.fullmatch(pattern,name))
        self.assertIsNone(re.fullmatch(pattern,"studio_colors"))

    def test_reuse_suite_selects_only_new_native_and_remote_contracts(self):
        targets, pattern = oracle.suite_selection("studio_reuse")
        self.assertEqual(targets,["isolde_studio_quick_tests","isolde_studio_design_tests"])
        self.assertIsNotNone(re.fullmatch(pattern,"studio_reuse"))
        self.assertIsNotNone(re.fullmatch(pattern,"studio_design_contracts"))
        self.assertIsNone(re.fullmatch(pattern,"studio_quick"))

    def test_design_suite_runs_native_and_remote_contracts_together(self):
        self.assertEqual(oracle.suite_selection("studio_design_contracts"), (["isolde_studio_design_tests"], "^studio_design_contracts$"))
        targets, pattern = oracle.suite_selection("studio_designs")
        self.assertEqual(targets, ["isolde_studio_quick_tests", "isolde_studio_design_tests", "isolde_component_tests"])
        self.assertIsNotNone(re.fullmatch(pattern, "studio_designs"))
        self.assertIsNotNone(re.fullmatch(pattern, "studio_design_contracts"))
        self.assertIsNone(re.fullmatch(pattern, "studio_designs_unrelated"))
        for name in ("component_contracts", "studio_quick", "studio_colors", "studio_cutover"):
            self.assertIsNotNone(re.fullmatch(pattern, name))
        for name in ("studio_designs", "studio_design_contracts"):
            self.assertIsNotNone(re.fullmatch(oracle.suite_selection("studio")[1], name))

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

    def test_android_configuration_can_recover_only_an_exact_current_tested_release(self):
        runner = oracle.Oracle(self.root, "full")
        payload = runner.run / "payload"
        (payload / "bin").mkdir(parents=True)
        (payload / "bin/isolde").write_bytes(b"tested executable")
        (payload / "share/data").mkdir(parents=True)
        (payload / "share/data/file").write_bytes(b"tested resource")
        home = self.root / "home"
        release = home / ".local/share/isolde-appliance/releases/tested"
        (release / "bin").mkdir(parents=True)
        (release / "bin/isolde").write_bytes(b"tested executable")
        (release / "share/data").mkdir(parents=True)
        (release / "share/data/file").write_bytes(b"tested resource")
        with patch.object(oracle.Path, "home", return_value=home), patch.object(
            runner, "links", return_value={"current": str(release), "previous": ""}
        ):
            self.assertEqual(runner.deployed_release(payload, "android"), str(release))
            with self.assertRaisesRegex(RuntimeError, "deployed snapshot"):
                runner.deployed_release(payload, "native")
            (release / "share/data/file").write_bytes(b"changed resource")
            with self.assertRaisesRegex(RuntimeError, "exact match"):
                runner.deployed_release(payload, "android")

    def test_android_repository_dns_probe_is_bounded_and_reports_failures(self):
        response = SimpleNamespace(
            returncode=0,
            stdout="192.0.2.1 STREAM repo.waydro.id\n192.0.2.1 DGRAM repo.waydro.id\n",
            stderr="",
        )
        with patch.object(oracle.subprocess, "run", return_value=response) as run:
            result = oracle.repository_dns_probe("repo.waydro.id")
        self.assertEqual(result, {"host": "repo.waydro.id", "resolved": True, "addresses": ["192.0.2.1"], "error": ""})
        self.assertEqual(run.call_args.args[0], ["getent", "ahosts", "repo.waydro.id"])
        self.assertEqual(run.call_args.kwargs["timeout"], 8)

        with patch.object(oracle.subprocess, "run", side_effect=subprocess.TimeoutExpired("getent", 8)):
            self.assertEqual(
                oracle.repository_dns_probe("repo.waydro.id"),
                {"host": "repo.waydro.id", "resolved": False, "error": "DNS lookup timed out"},
            )
        with self.assertRaisesRegex(ValueError, "hostname"):
            oracle.repository_dns_probe("repo.waydro.id; touch /tmp/unsafe")

    def test_resolver_status_is_read_only_bounded_and_reports_missing_tool(self):
        response = SimpleNamespace(returncode=0, stdout="Current DNS Server: 192.0.2.53\n", stderr="")
        with patch.object(oracle.shutil, "which", return_value="/usr/bin/resolvectl"), patch.object(
            oracle.subprocess, "run", return_value=response
        ) as run:
            result = oracle.resolver_status()
        self.assertEqual(
            result,
            {"available": True, "exit": 0, "output": response.stdout, "error": ""},
        )
        self.assertEqual(run.call_args.args[0], ["/usr/bin/resolvectl", "status", "--no-pager"])
        self.assertEqual(run.call_args.kwargs["timeout"], 8)

        with patch.object(oracle.shutil, "which", return_value=None):
            self.assertEqual(
                oracle.resolver_status(),
                {"available": False, "error": "resolvectl is not installed"},
            )

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

    def test_automatic_test_cleanup_skips_healthy_disk_and_records_low_space(self):
        runner=oracle.Oracle(self.root/'oracle','cleanup')
        with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=4*1024**3)),patch.object(runner,'maintain_artifacts') as cleanup:
            runner.automatic_test_cleanup()
            cleanup.assert_not_called()
        with patch.object(oracle.shutil,'disk_usage',side_effect=[SimpleNamespace(free=1024**3),SimpleNamespace(free=int(2.5*1024**3))]),patch.object(runner,'maintain_artifacts') as cleanup:
            runner.automatic_test_cleanup()
            cleanup.assert_called_once_with(minimum_gib=2)
        receipt=json.loads((runner.run/'guest-test-cleanup.json').read_text())
        self.assertTrue(receipt['minimumMet']);self.assertFalse(receipt['targetReached'])
        self.assertGreater(receipt['reclaimedBytes'],0)

    def test_ctest_stops_before_unrelated_work_after_first_failure(self):
        build=self.root/'ctest-fixture';build.mkdir();marker=build/'must-not-run'
        script=build/'later.py';script.write_text('from pathlib import Path\nimport sys\nPath(sys.argv[1]).write_text("ran")\n')
        (build/'CTestTestfile.cmake').write_text(
            f'add_test(first "{Path(sys.executable).as_posix()}" "-c" "raise SystemExit(1)")\n'
            f'add_test(later "{Path(sys.executable).as_posix()}" "{script.as_posix()}" "{marker.as_posix()}")\n')
        result=subprocess.run(oracle.ctest_command(build,build/'results.xml'),capture_output=True,text=True,timeout=15)
        self.assertNotEqual(result.returncode,0);self.assertFalse(marker.exists())
        self.assertIn('first',result.stdout)

    def test_campaign_continuation_retains_failures_and_refuses_unknown_or_repeated_results(self):
        results=[{'name':'first','state':'failed'}]
        self.assertEqual(oracle.campaign_pending(['first','later'],results),['later'])
        self.assertEqual(results,[{'name':'first','state':'failed'}])
        for invalid in (results+results,[{'name':'foreign','state':'passed'}]):
            with self.assertRaises(RuntimeError):oracle.campaign_pending(['first','later'],invalid)
        xml=self.root/'campaign.xml'
        xml.write_text('<testsuite><testcase name="first"><failure/></testcase><testcase name="later"/></testsuite>')
        self.assertEqual(oracle.campaign_results(xml),[{'name':'first','state':'failed'},{'name':'later','state':'passed'}])

    def test_diagnostic_staging_never_claims_pass_or_accepts_incomplete_campaign(self):
        progress={'acceptance':False,'registered':['first'],'results':[{'name':'first','state':'failed'}],
                  'pending':[],'sourceDigest':'source','runs':['base','next']}
        receipt={'acceptance':False,'passed':False,'fullSuite':False,'sourceDigest':'source','runs':['base','next'],'payload':{'bin/isolde':'hash'}}
        self.assertTrue(oracle.diagnostic_campaign_receipt(receipt,progress))
        for field in ('passed','acceptance','fullSuite'):
            with self.assertRaises(RuntimeError):oracle.diagnostic_campaign_receipt(dict(receipt,**{field:True}),progress)
        with self.assertRaises(RuntimeError):oracle.diagnostic_campaign_receipt(receipt,dict(progress,pending=['first']))
        with self.assertRaises(RuntimeError):oracle.diagnostic_campaign_receipt(dict(receipt,sourceDigest='different'),progress)

    def test_source_policy_suite_runs_registered_native_contracts(self):
        targets,pattern=oracle.suite_selection('sources')
        self.assertEqual(targets,['isolde_tests','isolde_component_tests','isolde_studio_quick_tests','isolde_home_navigation_tests'])
        self.assertIsNotNone(re.fullmatch(pattern,'isolde_tests'))
        self.assertIsNotNone(re.fullmatch(pattern,'component_contracts'))
        self.assertIsNotNone(re.fullmatch(pattern,'sources_remote_ui'))
        self.assertIsNotNone(re.fullmatch(pattern,'home_component_navigation'))
        self.assertIsNotNone(re.fullmatch(pattern,'studio_quick'))

    def test_automatic_cleanup_records_failure_without_hiding_it(self):
        runner=oracle.Oracle(self.root/'oracle','cleanup-failure')
        with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=1024**3)),patch.object(runner,'maintain_artifacts',side_effect=RuntimeError('unsafe mount')):
            with self.assertRaisesRegex(RuntimeError,'unsafe mount'):runner.automatic_test_cleanup()
        self.assertFalse(json.loads((runner.run/'guest-test-cleanup.json').read_text())['minimumMet'])

    def test_optional_disk_inventory_timeout_is_recorded(self):
        with patch.object(oracle.subprocess,'run',side_effect=subprocess.TimeoutExpired('du',30)):
            result=oracle.guest_disk_usage_diagnostic([self.root])
        self.assertTrue(result['timedOut'])
        self.assertIn('timed out',result['errors'])
        with patch.object(oracle.subprocess,'run',side_effect=PermissionError('denied')):
            with self.assertRaises(PermissionError):oracle.guest_disk_usage_diagnostic([self.root])

    @unittest.skipUnless(sys.platform=='linux','Guest release symlinks')
    def test_failed_android_registration_restores_release_links_and_service(self):
        spec=importlib.util.spec_from_file_location('rollback_installer',Path(__file__).resolve().parents[1]/'tools/install-appliance.py')
        installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
        home=self.root/'home';prefix=home/'.local/share/isolde-appliance'
        for name in ('old','previous'):
            binary=prefix/'releases'/name/'bin/isolde';binary.parent.mkdir(parents=True);binary.write_bytes(name.encode())
            installer.set_link(prefix/('current' if name=='old' else 'previous'),binary.parent.parent)
        payload=self.root/'payload'
        for name in ('bin/isolde','bin/isolde-native-host','bin/config/native-session-bus.conf'):
            file=payload/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(b'candidate')
        runner=oracle.Oracle(self.root/'oracle','failed-deploy');events=[]
        def stage(name,args,timeout):
            events.append((name,args))
            if name=='android-app-registration':raise RuntimeError('stale Android receipt')
        with patch.object(oracle.Path,'home',return_value=home),patch.object(oracle.os,'geteuid',return_value=1000),patch.object(installer,'ubuntu_supported',return_value=True),patch.object(oracle.importlib.util,'spec_from_file_location',return_value=SimpleNamespace(loader=Mock())),patch.object(oracle.importlib.util,'module_from_spec',return_value=installer),patch.object(runner,'tested_payload',return_value=payload),patch.object(runner,'service',return_value={'ActiveState':'active'}),patch.object(runner,'stage',side_effect=stage):
            with self.assertRaisesRegex(RuntimeError,'stale Android receipt'):runner.deploy()
        self.assertEqual((prefix/'current').resolve(),prefix/'releases/old')
        self.assertEqual((prefix/'previous').resolve(),prefix/'releases/previous')
        self.assertEqual(events[-1],('restore-deploy-service',['systemctl','--user','restart','isolde.service']))
        self.assertTrue(json.loads((runner.run/'deploy-failure.json').read_text())['rollbackVerified'])
        self.assertFalse((runner.run/'deploy-result.json').exists())

    @unittest.skipUnless(sys.platform=='linux','Guest release symlinks')
    def test_release_transaction_restores_absent_links_on_first_install_failure(self):
        spec=importlib.util.spec_from_file_location('first_installer',Path(__file__).resolve().parents[1]/'tools/install-appliance.py')
        installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
        prefix=self.root/'installation';binary=prefix/'releases/new/bin/isolde';binary.parent.mkdir(parents=True);binary.write_bytes(b'new')
        with self.assertRaisesRegex(RuntimeError,'failed'):
            with oracle.release_link_transaction(installer,prefix):
                installer.set_link(prefix/'current',binary.parent.parent)
                raise RuntimeError('failed')
        self.assertFalse((prefix/'current').is_symlink());self.assertFalse((prefix/'previous').exists())

    def test_test_preflight_self_tests_before_cleanup_and_machine_check(self):
        runner=oracle.Oracle(self.root/'oracle','ordered-cleanup');events=[]
        def stop():events.append('check');raise RuntimeError('stop before build')
        with patch.object(oracle,'extract_snapshot',side_effect=lambda run:(events.append('extract') or (run/'source',{}))),patch.object(runner,'stage',side_effect=lambda *args:events.append(args[0])),patch.object(runner,'automatic_test_cleanup',side_effect=lambda:events.append('cleanup')),patch.object(runner,'check',side_effect=stop):
            with self.assertRaisesRegex(RuntimeError,'stop before build'):runner.test('component_contracts','requested-base')
        self.assertEqual(events,['extract','oracle-self-test','cleanup','check'])
        self.assertEqual(runner.maintenance_pins,{runner.root/'runs/requested-base'})

    def test_backup_pruning_preserves_recent_runs_and_stops_at_target(self):
        root=self.root/'oracle';runs=root/'runs';runs.mkdir(parents=True);backups=[]
        for i in range(6):
            run=runs/str(i);run.mkdir();archive=run/'source.tar.gz';archive.write_bytes(b'original')
            checksum=oracle.digest(archive);(run/'archive.sha256').write_text(checksum+'  source.tar.gz')
            (run/'status.json').write_text(json.dumps({'state':'passed'}));os.utime(run,(i,i));os.utime(archive,(i,i))
            backups.append({'runId':str(i),'sha256':checksum})
        with patch.object(oracle.shutil,'disk_usage',side_effect=lambda _:SimpleNamespace(free=(4 if not (runs/'0/source.tar.gz').exists() else 1)*1024**3)):
            removed=oracle.remove_backed_up_archives(root,backups,preserve_recent=True,target_free=3*1024**3)
        self.assertEqual(removed,[backups[0]])
        for i in range(1,6):self.assertTrue((runs/str(i)/'source.tar.gz').exists())
        # No space target: even then the newest three archives stay available.
        oracle.remove_backed_up_archives(root,backups,preserve_recent=True,protected={runs/'1'})
        self.assertTrue((runs/'1/source.tar.gz').exists())
        for i in range(3,6):self.assertTrue((runs/str(i)/'source.tar.gz').exists())

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

    def test_duplicate_source_transport_cleanup_retains_verified_tree_and_payload(self):
        run,runs=self.recovery_snapshot('transport');(run/'status.json').write_text('{"state":"passed"}')
        payload=run/'payload';payload.mkdir();(payload/'isolde').write_bytes(b'retain staged candidate')
        backup=[{'runId':run.name,'sha256':oracle.digest(run/'source.tar.gz')}]
        self.assertEqual(oracle.remove_backed_up_archives(runs.parent,backup),[])
        original=(run/'source.tar.gz').read_bytes()
        (run/'source/src/main.cpp').write_bytes(b'modified')
        with self.assertRaises(RuntimeError):oracle.remove_backed_up_archives(runs.parent,backup,allow_source_copy=True)
        self.assertEqual((run/'source.tar.gz').read_bytes(),original)
        (run/'source/src/main.cpp').write_bytes(b'source bytes\n')
        self.assertEqual(oracle.remove_backed_up_archives(runs.parent,backup,allow_source_copy=True),backup)
        self.assertFalse((run/'source.tar.gz').exists());oracle.verify_tree(run/'source',oracle.manifest(run/'files.sha256'))
        self.assertEqual((payload/'isolde').read_bytes(),b'retain staged candidate')

    def test_failed_source_cleanup_preserves_current_pinned_active_and_evidence(self):
        eligible,runs=self.recovery_snapshot('eligible')
        candidates={'eligible':eligible}
        for name in ('current','pinned','active','running','payload','tested','guide'):
            candidate=runs/name;oracle.shutil.copytree(eligible,candidate);candidates[name]=candidate
        for name,candidate in candidates.items():
            (candidate/'status.json').write_text(json.dumps({'state':'running' if name=='running' else 'failed'}))
            (candidate/'evidence.log').write_text('keep evidence')
        (candidates['payload']/'payload').mkdir()
        (candidates['tested']/'test-result.json').write_text('{}')
        (candidates['guide']/'guide-soak-receipt.json').write_text('{}')
        with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=0)):
            removed=oracle.remove_failed_source_copies(runs.parent,candidates['current'],active=[candidates['active']/'source/src/main.cpp'],protected=[candidates['pinned']])
        self.assertEqual(removed,[str(eligible/'source')])
        for name,candidate in candidates.items():
            self.assertEqual((candidate/'source').exists(),name!='eligible')
            self.assertTrue((candidate/'source.tar.gz').is_file())
            self.assertEqual((candidate/'evidence.log').read_text(),'keep evidence')
        oracle.extract_snapshot(eligible)
        (eligible/'source.tar.gz').write_bytes(b'tampered')
        with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=0)):
            with self.assertRaises(RuntimeError):oracle.remove_failed_source_copies(runs.parent,candidates['current'],protected=list(candidates.values())[1:])
        self.assertTrue((eligible/'source/src/main.cpp').is_file())

    @unittest.skipUnless(sys.platform=='linux','Guest source hardlinks')
    def test_source_dedup_preserves_paths_and_refuses_modified_or_mounted_source(self):
        first,runs=self.recovery_snapshot('first')
        second=runs/'second';oracle.shutil.copytree(first,second)
        for run in (first,second):(run/'status.json').write_text('{"state":"passed"}')
        files=[run/'source/src/main.cpp' for run in (first,second)]
        before=[p.read_bytes() for p in files]
        with self.assertRaisesRegex(RuntimeError,'Mounted'):oracle.deduplicate_source_snapshots(runs.parent,runs/'current',mounts=[first/'source'],target_free=10**18)
        files[0].write_bytes(b'modified')
        with self.assertRaises(RuntimeError):oracle.deduplicate_source_snapshots(runs.parent,runs/'current',target_free=10**18)
        files[0].write_bytes(before[0])
        records=oracle.deduplicate_source_snapshots(runs.parent,runs/'current',target_free=10**18)
        self.assertEqual(len(records),1);self.assertEqual(files[0].stat().st_ino,files[1].stat().st_ino)
        for index,path in enumerate(files):self.assertEqual(path.read_bytes(),before[index])
        for run in (first,second):self.assertTrue((run/'source.tar.gz').is_file());oracle.verify_tree(run/'source',oracle.manifest(run/'files.sha256'))

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

    @unittest.skipUnless(sys.platform=="linux","Guest-only payload maintenance")
    def test_explicit_artifact_cleanup_preserves_newest_payloads_despite_source_mtimes(self):
        home=self.root/'home';home.mkdir();runner=oracle.Oracle(home/'isolde-oracle','current')
        runs=runner.root/'runs';old=[]
        for i in range(1,6):
            run=runs/str(i);(run/'payload/bin').mkdir(parents=True);(run/'payload/bin/isolde').write_text('tested '+str(i))
            (run/'test-result.json').write_text(json.dumps(dict(passed=True,fullSuite=True,payload=oracle.payload_hashes(run/'payload'))))
            (run/'source.log').write_text('retain evidence');os.utime(run/'payload',(i,i));os.utime(run,(20-i,20-i));old.append(run)
        for i,name in enumerate(('recent-a','recent-b'),40):
            recent=runs/name;recent.mkdir();os.utime(recent,(i,i))
        original=oracle.Path.read_text
        def read_text(path,*args,**kwargs):
            if path==Path('/proc/self/mountinfo'):return '1 0 0:1 / / rw - ext4 /dev/unit-test rw\n'
            return original(path,*args,**kwargs)
        (runs/'recent-a/test-result.json').write_text(json.dumps({'coverageReuse':{'baseRunId':'1'}}))
        proof={'baseRunId':'1','verified':True}
        focused=old[1]/'test-result.json'
        tested=json.loads(focused.read_text());tested.update(fullSuite=False,suite='store',coverageReuse=proof);focused.write_text(json.dumps(tested))
        # All generated trees are old by directory mtime, while payload 3–5 are
        # the latest actual staged candidates. Directory cleanup must not unpin them.
        with patch.object(runner,'command',return_value='isolde-test'),patch.object(runner,'stage'),patch.object(oracle.Path,'home',return_value=home),patch.object(oracle.Path,'read_text',read_text),patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=2*1024**3)),patch.object(oracle,'focused_qml_coverage',return_value={'changed':True}):
            with self.assertRaisesRegex(RuntimeError,'coverage changed'):runner.prepare_guest_space(3,prune_payloads=True)
        self.assertTrue((old[1]/'payload').is_dir())
        with patch.object(runner,'command',return_value='isolde-test'),patch.object(runner,'stage'),patch.object(oracle.Path,'home',return_value=home),patch.object(oracle.Path,'read_text',read_text),patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=2*1024**3)),patch.object(oracle,'focused_qml_coverage',return_value=proof):
            runner.prepare_guest_space(3,prune_payloads=True)
        for run in old[2:]:self.assertTrue((run/'payload/bin/isolde').is_file())
        for run in old:self.assertEqual((run/'source.log').read_text(),'retain evidence')
        self.assertTrue((old[0]/'payload/bin/isolde').is_file())
        self.assertFalse((old[1]/'payload').exists());self.assertTrue((old[1]/'test-result.json').is_file())

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
        (run / "evidence-desktop-1.tar.gz").write_bytes(b"bounded desktop result")
        (run / "evidence-studio-designs-1.tar.gz").write_bytes(b"bounded offline design result")
        (run / "evidence-collections-1.tar.gz").write_bytes(b"bounded collection result")
        (run / "evidence-store-discovery-1.tar.gz").write_bytes(b"bounded signed online listing result")
        (run / "evidence-unknown-1.tar.gz").write_bytes(b"not allowlisted")
        oracle.write_report(run)
        with tarfile.open(run / "report.tar.gz") as archive:
            self.assertEqual(set(archive.getnames()), {"status.json", "build.log", "studio-test-move-cancel.png", "evidence-desktop-1.tar.gz", "evidence-studio-designs-1.tar.gz", "evidence-collections-1.tar.gz", "evidence-store-discovery-1.tar.gz"})

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

    def test_collection_result_requires_real_lifecycle_and_complete_captures(self):
        result=dict.fromkeys(("passed","freshProfile","actualNativeInput","normalExit","exactGalleryReturn","buttonReturn","storeRemoveReinstall","layoutUnchanged"),True)
        result.update(physicalHardwareVerified=False,thirdPartyClientVerified=False,captures=["collectionhome","selectedgallery","collectionclient","selectedreturn","collectionrepeat","buttongallery","buttonclient","buttonreturn","buttonrepeat","storeclient","collectionuninstall","allremoved","selectedmissing","missingrefused","allrestored","reinstalledclient","reinstalledreturn"])
        self.assertTrue(oracle.visual_result_passed("collections",result))
        for key in ("actualNativeInput","normalExit","exactGalleryReturn","buttonReturn","storeRemoveReinstall","layoutUnchanged"):
            self.assertFalse(oracle.visual_result_passed("collections",dict(result,**{key:False})))
        self.assertFalse(oracle.visual_result_passed("collections",dict(result,captures=result['captures'][:-1])))
        history=dict(result,appHistoryVerified=True,appHistorySessionOnly=True,appHistoryReopenCycles=2,captures=result['captures']+["historyapp","historyclient","historyreturned","historyrepeat","historyrepeatreturn"])
        self.assertTrue(oracle.visual_result_passed("collections",history))
        self.assertFalse(oracle.visual_result_passed("collections",dict(history,appHistoryReopenCycles=1)))
        self.assertFalse(oracle.visual_result_passed("collections",dict(history,appHistorySessionOnly=False)))
        filtered_phases=list(history['captures'])
        filtered_phases.insert(2,'collectionfilter');filtered_phases.insert(filtered_phases.index('buttongallery'),'collectionfiltercleared')
        filtered=dict(history,localFilterVerified=True,captures=filtered_phases)
        self.assertTrue(oracle.visual_result_passed('collections',filtered))
        self.assertFalse(oracle.visual_result_passed('collections',dict(filtered,captures=history['captures'])))
        authored=dict(filtered,galleryPresentationApplied=True,galleryPresentation={'layout':'list','columns':6},galleryPresentationSha256='a'*64)
        self.assertTrue(oracle.visual_result_passed('collections',authored))
        self.assertFalse(oracle.visual_result_passed('collections',dict(authored,galleryPresentationSha256='wrong')))
        self.assertFalse(oracle.visual_result_passed('collections',dict(authored,galleryPresentationApplied=False)))

    def test_visual_results_use_each_harness_contract(self):
        history=dict.fromkeys(("passed","freshProfile","realKodiPlayback","kodiOwnedLastplayed","historyCardPlayback","exactHistoryReturn","layoutUnchanged","mediaUnchanged"),True)
        history.update(normalProfileUsed=False,independentInstances=[dict(count=5,capacity=3),dict(count=1,capacity=1)],captures=["historylibrary","historyplaying","historyupdated","historyfocused","historyreplaying","historyreturned","historyrepeatreturn"])
        self.assertTrue(oracle.visual_result_passed("history",history))
        for key in ("realKodiPlayback","kodiOwnedLastplayed","historyCardPlayback","exactHistoryReturn","layoutUnchanged"):
            self.assertFalse(oracle.visual_result_passed("history",dict(history,**{key:False})))
        self.assertFalse(oracle.visual_result_passed("history",dict(history,captures=history['captures'][:-1])))
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
        colors=dict(privateCancel=True,atomicApply=True,recentSaved=True,resetCancel=True,studioChrome=True,reopenPreserved=True,captures=['colorpreview','colorapplied','colorrecent','colorreset','chromepreview','chromeapplied','colorreopened'])
        workflows=dict(proportionalSize=True,sizeCancel=True,headerApply=True,headerRecovery=True,cameraNoWrite=True,resetCancel=True,captures=["sizepreview","sizeapplied","headerpreview","headerorder","headerapplied","headerreopened","canvaszoom","resetcancel"])
        self.assertTrue(oracle.visual_result_passed("studio", {"passed":True,"colorPicker":colors,"workflows":workflows}))
        for field in workflows:
            missing=dict(workflows);del missing[field]
            self.assertFalse(oracle.visual_result_passed("studio", {"passed":True,"colorPicker":colors,"workflows":missing}))
        self.assertFalse(oracle.visual_result_passed("studio", {"passed":True}))
        for field in colors:
            missing=dict(colors);del missing[field]
            self.assertFalse(oracle.visual_result_passed("studio", {"passed":True,"colorPicker":missing}))
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
        with patch.object(runner, "prepare_visual_desktop"), \
                patch.object(runner, "tested_payload", return_value=self.root), \
                patch.object(runner, "service", return_value=before), \
                patch.object(runner, "links", return_value={"current": "same"}), \
                patch.object(runner, "stage", side_effect=TimeoutError("timed out")), \
                patch.object(runner, "restore", return_value=before) as restore:
            with self.assertRaises(TimeoutError):
                runner.visual("studio")
            restore.assert_called_once_with(before)
        self.assertFalse((self.root / "service-lease.json").exists())

    def test_visual_desktop_refuses_host_and_only_cancels_prompt(self):
        runner=oracle.Oracle(self.root/'isolde-oracle','desktop')
        with patch.object(runner,'command',return_value='personal-host'),patch.object(oracle.subprocess,'run') as execute:
            with self.assertRaisesRegex(RuntimeError,'Isolde-Test'):runner.prepare_visual_desktop()
            execute.assert_not_called()
        replies=[SimpleNamespace(stdout='/org/freedesktop/secrets/prompt/p1'),SimpleNamespace(stdout='()'),SimpleNamespace(stdout='true'),SimpleNamespace(stdout=''),SimpleNamespace(stdout='/org/freedesktop/secrets')]
        with patch.object(oracle.Path,'home',return_value=self.root),patch.object(oracle,'dismiss_guest_disk_banner',return_value={'identifiedDiskWarning':False,'ignored':False}),patch.object(runner,'command',side_effect=lambda *args:'isolde-test' if args==('hostname',) else ''),patch.object(oracle.subprocess,'run',side_effect=replies) as execute:
            runner.prepare_visual_desktop()
        commands=[call.args[0] for call in execute.call_args_list]
        self.assertEqual(commands[1],['gdbus','call','--session','--dest','org.freedesktop.secrets','--object-path','/org/freedesktop/secrets/prompt/p1','--method','org.freedesktop.Secret.Prompt.Dismiss'])
        self.assertFalse(json.loads((runner.run/'guest-visual-desktop.json').read_text())['credentialsRead'])

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


class SourcesJourneyContracts(unittest.TestCase):
    def test_store_transport_reuse_refuses_unrelated_source_and_changed_full_payload(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);base=root/'runs/base';current=root/'runs/current'
            files={name:b'original' for name in ('tools/fdroid-store.py','tests/test_fdroid_store.py','src/main.cpp','src/app/AppCatalog.cpp','qml/AppStoreView.qml','qml/Main.qml','config/catalog.json')}
            def write(run,values):
                for name,data in values.items():
                    path=run/'source'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
                (run/'files.sha256').write_text(''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in sorted(values.items())))
            write(base,files)
            executable=base/'payload/bin/isolde';executable.parent.mkdir(parents=True);executable.write_bytes(b'original full executable')
            oracle.atomic_json(base/'test-result.json',dict(passed=True,fullSuite=True,sourceDigest=oracle.digest(base/'files.sha256'),payload=oracle.payload_hashes(base/'payload')))
            changed=dict(files,**{'tools/fdroid-store.py':b'bounded compressed transport','tests/test_fdroid_store.py':b'signature and expansion regressions'})
            write(current,changed)
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','store')['changedStoreFiles'],['tools/fdroid-store.py'])
            changed['qml/AppStoreView.qml']=b'preserve authoritative provider matches'
            write(current,changed)
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','store')['changedStoreFiles'],['qml/AppStoreView.qml','tools/fdroid-store.py'])
            for name in ('tools/android-setup.py','tools/provision-android.py'):
                files[name]=b'original';changed[name]=b'bounded Android metadata/support correction'
            write(base,files);base_receipt=json.loads((base/'test-result.json').read_text());base_receipt['sourceDigest']=oracle.digest(base/'files.sha256');oracle.atomic_json(base/'test-result.json',base_receipt)
            write(current,changed)
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','store')['changedStoreFiles'],['qml/AppStoreView.qml','tools/android-setup.py','tools/fdroid-store.py','tools/provision-android.py'])
            targets,pattern=oracle.suite_selection('store')
            self.assertTrue(all(re.fullmatch(pattern,name) for name in ('fdroid_store','store_lifecycle','isolde_tests','studio_selection','android_provisioning','web_remote')))
            for name in ('src/main.cpp','src/app/AppCatalog.cpp','qml/Main.qml','config/catalog.json'):
                write(current,dict(changed,**{name:b'outside transport scope'}))
                with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','store')
            write(current,changed);executable.write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','store')

    def test_current_acceptance_helper_parses_before_vm_journeys(self):
        helper=Path(__file__).resolve().parents[1]/'tools/home-components-acceptance.py'
        compile(helper.read_bytes(),str(helper),'exec')

    def test_native_gallery_evidence_is_bound_to_a_passing_run_and_exact_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            run=Path(temporary);path=run/'studio-gallery-presentation.json';path.write_text('{"layout":"list","columns":6}')
            result=dict(passed=True,galleryPresentationEvidence=dict(file=path.name,sha256=oracle.digest(path)))
            self.assertEqual(oracle.verified_gallery_evidence(run,result),(path,oracle.digest(path)))
            command=oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),collections=True,gallery_evidence=path,gallery_digest=oracle.digest(path))
            self.assertIn('--gallery-presentation-evidence',command)
            with self.assertRaises(RuntimeError):oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),gallery_evidence=path,gallery_digest=oracle.digest(path))
            with self.assertRaises(RuntimeError):oracle.verified_gallery_evidence(run,dict(result,passed=False))
            with self.assertRaises(RuntimeError):oracle.verified_gallery_evidence(run,dict(result,galleryPresentationEvidence=dict(file='../outside',sha256=oracle.digest(path))))
            path.write_text('{"layout":"grid"}')
            with self.assertRaises(RuntimeError):oracle.verified_gallery_evidence(run,result)

    def test_installed_search_matrix_requires_profile_cold_save_failure_and_origin_proof(self):
        phases=['searchhome','searchgallery','offlinemedia','offlinesearch','offlinewebon','offlineweboff','offlinewebpersisted','offlineprofilefresh','offlineprofileoff','offlineprofileon','offlinedefaulton','offlinewebsavefailed','offlinewebretry','offlinewebrestored','searchgalleryreturn','offlinerecovery']
        fields=('webToggleVerified','coldPreferenceVerified','profileSwitchVerified','preferenceWriteFailureVerified','localKodiBoundaryVerified','galleryOriginVerified','layoutBytesUnchanged')
        result=dict.fromkeys(fields,True)
        surfaces=[dict(pid=11,window='0x100'),dict(pid=12,window='0x100'),dict(pid=12,window='0x200'),dict(pid=12,window='0x300')]
        result.update(passed=True,journey='studio-search',freshProfile=True,normalProfileUsed=False,network=dict(socketErrnos=[101,101],unixSocketAvailable=True),actualInputActions=1,captureTrace=[dict(phase=p) for p in phases],phases=phases,searchMatrixVersion=2,nativeKodiBefore={'sources.xml':'unchanged'},nativeKodiAfter={'sources.xml':'unchanged'},kodiBoundary='controlled-unix-rpc',ownedRuntimeWindows=surfaces)
        self.assertTrue(oracle.visual_result_passed('studio-search',result))
        for field in fields:self.assertFalse(oracle.visual_result_passed('studio-search',dict(result,**{field:False})))
        self.assertFalse(oracle.visual_result_passed('studio-search',dict(result,phases=phases[:-1])))
        self.assertFalse(oracle.visual_result_passed('studio-search',dict(result,ownedRuntimeWindows=surfaces[:3]+[surfaces[0]])))
        self.assertFalse(oracle.visual_result_passed('studio-search',dict(result,nativeKodiAfter={'sources.xml':'rewritten'})))

    def test_sources_reuse_accepts_only_its_registered_widget_and_regressions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);base=root/'runs/base';current=root/'runs/current'
            files={'qml/PageStudioView.qml':b'original gallery controls','qml/Main.qml':b'original routing','qml/SourceSettingsView.qml':b'original guidance','qml/HomeScreen.qml':b'original Home','qml/PageSourcesWidget.qml':b'original','tests/studio/tst_sources.qml':b'original tests','src/main.cpp':b'unchanged','config/studio-defaults.json':b'unchanged'}
            def write(run,values):
                for name,data in values.items():
                    path=run/'source'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
                (run/'files.sha256').write_text(''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in sorted(values.items())))
            write(base,files)
            executable=base/'payload/bin/isolde';executable.parent.mkdir(parents=True);executable.write_bytes(b'original full executable')
            baseline=dict(passed=True,fullSuite=True,sourceDigest=oracle.digest(base/'files.sha256'),payload=oracle.payload_hashes(base/'payload'))
            oracle.atomic_json(base/'test-result.json',baseline)
            changed=dict(files,**{'qml/PageSourcesWidget.qml':b'details fix','tests/studio/tst_sources.qml':b'visible text regression'})
            write(current,changed)
            proof=oracle.focused_qml_coverage(root,current,'base','sources')
            self.assertEqual(proof['changedSourcesQml'],['qml/PageSourcesWidget.qml'])
            write(current,dict(changed,**{'docs/acceptance-reconciliation-2026-11-11.md':b'new evidence report'}))
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','sources')['changedSourcesQml'],['qml/PageSourcesWidget.qml'])
            write(current,dict(changed,**{'qml/HomeScreen.qml':b'remove legacy source banner'}))
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','sources')['changedSourcesQml'],['qml/HomeScreen.qml','qml/PageSourcesWidget.qml'])
            write(current,dict(changed,**{'qml/Main.qml':b'source Menu route','qml/SourceSettingsView.qml':b'explicit action guidance'}))
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','sources')['changedSourcesQml'],['qml/Main.qml','qml/PageSourcesWidget.qml','qml/SourceSettingsView.qml'])
            write(current,dict(changed,**{'qml/PageStudioView.qml':b'gallery Done/Reset origin fix'}))
            self.assertEqual(oracle.focused_qml_coverage(root,current,'base','sources')['changedSourcesQml'],['qml/PageSourcesWidget.qml','qml/PageStudioView.qml'])
            for path in ('src/main.cpp','config/studio-defaults.json'):
                write(current,dict(changed,**{path:b'outside scope'}))
                with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','sources')
            write(current,changed);executable.write_bytes(b'tampered')
            with self.assertRaises(RuntimeError):oracle.focused_qml_coverage(root,current,'base','sources')

    def test_report_cleanup_requires_backups_and_preserves_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);runs=root/'runs';runs.mkdir();backups=[]
            for index in range(7):
                run=runs/str(index);run.mkdir();archive=run/'report.tar.gz';archive.write_bytes(b'verified report '+str(index).encode())
                os.utime(archive,(index,index));(run/'status.json').write_text(json.dumps(dict(state='running' if index==2 else 'passed')))
                (run/'raw-evidence.log').write_text('retained')
                backups.append(dict(runId=str(index),sha256=oracle.digest(archive)))
            with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=0)):
                with self.assertRaisesRegex(RuntimeError,'checksum'):
                    oracle.remove_backed_up_reports(root,[dict(backups[0],sha256='0'*64)])
                with self.assertRaisesRegex(RuntimeError,'Mounted'):
                    oracle.remove_backed_up_reports(root,[backups[0]],[runs/'0'])
                removed=oracle.remove_backed_up_reports(root,backups,protected={runs/'1',runs/'3'})
            self.assertEqual(removed,[backups[0]])
            for index in range(7):
                self.assertEqual((runs/str(index)/'raw-evidence.log').read_text(),'retained')
                self.assertEqual((runs/str(index)/'report.tar.gz').exists(),index!=0)
            payload=runs/'5/payload/bin/isolde';payload.parent.mkdir(parents=True);payload.write_bytes(b'pinned tested executable')
            with patch.object(oracle.shutil,'disk_usage',return_value=SimpleNamespace(free=0)):
                with self.assertRaisesRegex(RuntimeError,'checksum'):
                    oracle.remove_backed_up_reports(root,[dict(backups[4],sha256='0'*64)],preserve_recent=False)
                removed=oracle.remove_backed_up_reports(root,backups,protected={runs/'1',runs/'3'},preserve_recent=False)
            self.assertEqual(removed,backups[4:])
            self.assertEqual(payload.read_bytes(),b'pinned tested executable')
            self.assertTrue(all((runs/str(index)/'raw-evidence.log').exists() for index in range(7)))
            self.assertTrue((runs/'2/report.tar.gz').exists())

    def test_sources_requires_complete_receipt_and_verified_helper_flag(self):
        phases=["sourcesdefault","sourceslist","sourcesdraft","sourceshidden","sourcesrestored","sourceshandlers","sourcesdisconnected","sourcesretry","sourcespolicy","sourcespolicysaved","sourceskeep","sourceskept","darksourcesbounded","darksourcesactions","darksourcesnotice","pastelsourcesbounded","pastelsourcesactions","pastelsourcesnotice"]
        receipt=dict(passed=True,journey="sources",freshProfile=True,normalProfileUsed=False,phases=phases,noUnintendedLayoutWrites=True,explicitPolicySave=True,remoteHandlerSaved=True,remoteKeepSaved=True,fixtureOnly=True,remoteHideRestore=True,policyCancelNoWrite=True)
        self.assertTrue(oracle.visual_result_passed("sources",receipt))
        for field in ("passed","freshProfile","noUnintendedLayoutWrites","explicitPolicySave","remoteHandlerSaved","remoteKeepSaved","fixtureOnly","remoteHideRestore","policyCancelNoWrite"):
            self.assertFalse(oracle.visual_result_passed("sources",dict(receipt,**{field:False})))
        self.assertFalse(oracle.visual_result_passed("sources",dict(receipt,phases=phases[:-1])))
        self.assertFalse(oracle.visual_result_passed("sources",dict(receipt,normalProfileUsed=True)))
        self.assertIn("--sources-only",oracle.studio_helper_command(Path('/frozen'),Path('/helper'),Path('/app'),sources=True))

if __name__ == "__main__":
    unittest.main()
