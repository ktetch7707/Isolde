"""Production Kodi snapshot/import using only disposable filesystem roots."""
import importlib.util
import json
import os
from pathlib import Path
import sqlite3
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('kodi_import',ROOT/'tools/kodi-import.py')
module=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(module)


@unittest.skipUnless(sys.platform=='linux','Linux owned storage and durable rename contracts')
class KodiImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name).resolve();self.source=self.root/'desktop/.kodi'
        self.config=self.root/'config/Isolde';self.data=self.root/'data/Isolde'
        self.operation=module.Import('default',self.source,self.config,self.data)
        self.write(self.source/'userdata/guisettings.xml',b'<settings><setting id="lookandfeel.skin">skin.fixture</setting></settings>')
        self.write(self.source/'userdata/sources.xml',b'<sources><video><source><name>Nested original media</name><path>/media/library/</path></source></video></sources>')
        self.write(self.source/'userdata/keymaps/nested/remote.xml',b'original keymap')
        self.write(self.source/'userdata/addon_data/plugin.fixture/settings.xml',b'private addon settings')
        self.write(self.source/'addons/skin.fixture/addon.xml',b'<addon id="skin.fixture"/>')
        self.write(self.source/'userdata/playlists/video/Keep.xsp',b'original playlist')
        database=self.source/'userdata/Database/MyVideos133.db';database.parent.mkdir()
        with sqlite3.connect(database) as db:db.execute('CREATE TABLE watch_state (resume INTEGER)');db.execute('INSERT INTO watch_state VALUES (42)')
        self.write(self.operation.target/'userdata/guisettings.xml',b'previous Isolde skin')
        self.write(self.operation.target/'userdata/nested/previous.conf',b'previous profile setting')
        self.original=module.inventory(self.source);self.previous=module.inventory(self.operation.target)
        self.source_mode=self.source.stat().st_mode
        self.media=self.root/'media/never-move.ogv';self.write(self.media,b'original media')

    def write(self,path,data):
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)

    def assert_preserved(self):
        self.assertEqual(module.inventory(self.source),self.original)
        self.assertEqual(self.source.stat().st_mode,self.source_mode)
        self.assertEqual(self.media.read_bytes(),b'original media')

    def test_inventory_uses_real_skin_source_addons_and_versioned_database(self):
        text=self.operation.scan()
        for expected in ('skin.fixture','Media sources: 1','User add-ons: 1','Watch/resume databases: 1'):self.assertIn(expected,text)
        self.assert_preserved();self.assertFalse(self.operation.receipt.exists())

    def test_recursive_backup_import_cold_reopen_and_rollback(self):
        foreign=self.data/'native-profiles/6b30896f-2192-47f4-bc93-f71977515e95/kodi/home/.kodi/userdata/keep'
        self.write(foreign,b'other profile')
        self.operation.backup();state=self.operation.load()
        self.assertEqual(self.data.stat().st_mode&0o077,0)
        self.assertEqual(module.inventory(self.operation.backup_path(state)/'desktop'),self.original)
        self.assertFalse(self.operation.status()['canRollback'])
        self.operation.install();self.assertEqual(module.inventory(self.operation.target),self.original)
        cold=module.Import('default',self.source,self.config,self.data)
        self.assertTrue(cold.status()['canRollback']);cold.rollback()
        self.assertEqual(module.inventory(self.operation.target),self.previous)
        self.assertFalse(cold.status()['canRollback']);self.assertEqual(foreign.read_bytes(),b'other profile');self.assert_preserved()

    def test_first_import_rollback_removes_only_new_private_configuration(self):
        self.operation.remove(self.operation.target)
        self.operation.backup();self.operation.install();self.operation.rollback()
        self.assertFalse(self.operation.target.exists());self.assert_preserved()

    def test_import_requires_backup_and_rejects_changed_profile(self):
        with self.assertRaisesRegex(ValueError,'backup'):self.operation.install()
        self.operation.backup();self.write(self.operation.target/'new-setting',b'saved after backup')
        with self.assertRaisesRegex(ValueError,'changed after backup'):self.operation.install()
        self.assertEqual((self.operation.target/'new-setting').read_bytes(),b'saved after backup');self.assert_preserved()

    def test_tampered_snapshot_never_changes_target(self):
        self.operation.backup();state=self.operation.load()
        self.write(self.operation.backup_path(state)/'desktop/userdata/guisettings.xml',b'tampered')
        with self.assertRaisesRegex(ValueError,'checksum'):self.operation.install()
        self.assertEqual(module.inventory(self.operation.target),self.previous);self.assert_preserved()

    def test_write_failures_roll_back_complete_target_and_receipt(self):
        self.operation.backup()
        for point in ('prepared','old-renamed','target-replaced','before-import-receipt'):
            with self.subTest(point=point):
                def fail(actual):
                    if actual==point:raise OSError('controlled storage failure')
                self.operation.fault=fail
                with self.assertRaisesRegex(OSError,'controlled'):self.operation.install()
                self.assertEqual(module.inventory(self.operation.target),self.previous)
                self.assertFalse(self.operation.status()['canRollback']);self.assertFalse(self.operation.journal.exists());self.assert_preserved()
        self.operation.fault=lambda point:None;self.operation.install()
        for point in ('old-renamed','target-replaced','before-import-receipt'):
            with self.subTest(rollback=point):
                self.operation.fault=lambda actual:(_ for _ in ()).throw(OSError('rollback failure')) if actual==point else None
                with self.assertRaises(OSError):self.operation.rollback()
                self.assertEqual(module.inventory(self.operation.target),self.original)
                self.assertTrue(self.operation.status()['canRollback']);self.assert_preserved()

    def test_actual_process_crash_recovers_uncommitted_and_committed_renames(self):
        self.operation.backup()
        script="import importlib.util,os,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('crash',sys.argv[1]);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);o=m.Import('default',Path(sys.argv[2]),Path(sys.argv[3]),Path(sys.argv[4]),fault=lambda point:os._exit(91) if point==sys.argv[5] else None);o.install()"
        for point in ('prepared','old-renamed','target-replaced','before-import-receipt','after-import-receipt'):
            with self.subTest(point=point):
                result=subprocess.run([sys.executable,'-c',script,str(ROOT/'tools/kodi-import.py'),str(self.source),str(self.config),str(self.data),point],capture_output=True,timeout=10)
                self.assertEqual(result.returncode,91,result.stderr)
                self.assertTrue(self.operation.journal.exists());self.operation.recover()
                committed=point=='after-import-receipt'
                self.assertEqual(module.inventory(self.operation.target),self.original if committed else self.previous)
                self.assertEqual(self.operation.status()['canRollback'],committed);self.assert_preserved()
                if committed:self.operation.rollback()

    def test_backup_write_failure_retains_existing_recovery_snapshot(self):
        self.operation.backup();before=self.operation.receipt.read_bytes()
        self.operation.fault=lambda point:(_ for _ in ()).throw(OSError('receipt unavailable'))
        with self.assertRaises(OSError):self.operation.backup()
        self.assertEqual(self.operation.receipt.read_bytes(),before)
        self.assertEqual(module.inventory(self.operation.target),self.previous);self.assert_preserved()

    def test_links_and_declared_mounts_are_refused_without_following(self):
        external=self.root/'outside';self.write(external/'keep',b'outside data')
        link=self.source/'userdata/redirect';link.symlink_to(external,target_is_directory=True)
        with self.assertRaises(ValueError):self.operation.backup()
        self.assertEqual((external/'keep').read_bytes(),b'outside data');link.unlink()
        mounts=self.root/'mountinfo';mounts.write_text('1 0 8:1 / '+str(self.source/'temp/shared')+' rw - ext4 /dev/sda rw\n')
        with self.assertRaisesRegex(ValueError,'mounted'):module.reject_mounts(self.source,mounts)
        bad=self.root/'redirect-source';bad.symlink_to(self.source,target_is_directory=True)
        with self.assertRaises(ValueError):module.Import('default',bad,self.config,self.data)

    def test_limits_and_unknown_profile_or_receipt_never_mutate_source(self):
        with patch.object(module,'MAX_BYTES',1):
            with self.assertRaisesRegex(ValueError,'limit'):self.operation.backup()
        with self.assertRaises(ValueError):module.Import('../other',self.source,self.config,self.data)
        with self.assertRaises(ValueError):module.Import('default',self.source,self.source/'nested-config',self.data)
        self.operation.backup();state=self.operation.load();state['profile']='other';module.atomic_json(self.operation.receipt,state)
        with self.assertRaisesRegex(ValueError,'another profile'):self.operation.install()
        self.assertEqual(module.inventory(self.operation.target),self.previous);self.assert_preserved()

    def test_xml_entities_and_bad_skin_identity_are_refused(self):
        self.write(self.source/'userdata/guisettings.xml',b'<!DOCTYPE settings [<!ENTITY e "text">]><settings/>')
        with self.assertRaisesRegex(ValueError,'entities'):self.operation.scan()
        self.write(self.source/'userdata/guisettings.xml',b'<settings><setting id="lookandfeel.skin">bad&lt;b&gt;</setting></settings>')
        with self.assertRaisesRegex(ValueError,'skin identity'):self.operation.scan()

    def test_cleaning_private_snapshot_unlinks_links_without_touching_targets(self):
        self.operation.ensure_storage();folder=self.operation.base/'6b30896f-2192-47f4-bc93-f71977515e95';folder.mkdir()
        external=self.root/'external';self.write(external/'keep',b'keep')
        (folder/'redirect').symlink_to(external,target_is_directory=True)
        self.operation.remove(folder);self.assertEqual((external/'keep').read_bytes(),b'keep')
        with self.assertRaises(ValueError):self.operation.remove(external)

    def test_cancellation_at_rename_boundaries_restores_previous_profile(self):
        self.operation.backup()
        for point in ('prepared','old-renamed','target-replaced','before-import-receipt'):
            with self.subTest(point=point),patch.object(module,'stopping',False):
                def cancel(actual):
                    if actual==point:module.stopping=True
                self.operation.fault=cancel
                with self.assertRaisesRegex(RuntimeError,'cancelled'):self.operation.install()
            self.assertEqual(module.inventory(self.operation.target),self.previous)
            self.assertFalse(self.operation.status()['canRollback']);self.assert_preserved()

    def test_stopped_runtime_socket_and_fifo_are_not_configuration(self):
        ipc=self.operation.target/'userdata/isolde';ipc.mkdir()
        endpoint=socket.socket(socket.AF_UNIX);self.addCleanup(endpoint.close);endpoint.bind(str(ipc/'rpc.sock'))
        os.mkfifo(ipc/'notification.fifo')
        self.operation.backup();self.operation.install();self.operation.rollback()
        self.assertEqual(module.inventory(self.operation.target),self.previous)
        self.assertFalse((self.operation.target/'userdata/isolde').exists());self.assert_preserved()


if __name__=='__main__':unittest.main()
