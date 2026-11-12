import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import socket
import stat
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
from contextlib import ExitStack

ROOT = Path(__file__).resolve().parents[1]


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT/"tools"/filename)
    value = importlib.util.module_from_spec(spec); spec.loader.exec_module(value)
    return value


transaction = module("android_transaction_test", "android-store-transaction.py")
setup = module("android_setup_test", "android-setup.py")


class Copied:
    returncode = 0
    def __init__(self, arguments, **kwargs):
        if arguments[:4] != ["/usr/bin/cp", "--archive", "--reflink=auto", "--"]:
            raise AssertionError("Unexpected snapshot command")
        shutil.copytree(arguments[-2], arguments[-1], symlinks=True)
    def poll(self): return self.returncode


class AndroidTransactionTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.root = Path(self.scratch.name).resolve()
        self.activation = self.root/"activation.json"
        (self.root/"data/app").mkdir(parents=True)
        (self.root/"data/app/save").write_text("previous save")
        (self.root/"packages.json").write_text(json.dumps({"example": "old-hash"}))
        self.disk = patch.object(transaction.shutil, "disk_usage", return_value=SimpleNamespace(free=1024**4))
        self.disk.start()

    def tearDown(self):
        self.disk.stop(); self.scratch.cleanup()

    def prepare(self):
        return transaction.prepare(self.root, self.activation, lambda: None, runner=Copied)

    def mutate(self):
        (self.root/"data/app/save").write_text("changed by update")
        (self.root/"packages.json").write_text(json.dumps({"example": "new-hash"}))

    def test_cancel_or_activation_write_failure_restores_data_and_receipt(self):
        self.activation.write_text(json.dumps({"storeTransaction": "previous"}))
        before = self.activation.read_bytes()
        self.prepare(); self.mutate()
        self.assertFalse(transaction.recover(self.root, self.activation))
        self.assertEqual((self.root/"data/app/save").read_text(), "previous save")
        self.assertEqual(json.loads((self.root/"packages.json").read_text()), {"example": "old-hash"})
        self.assertEqual(self.activation.read_bytes(), before)
        self.assertFalse((self.root/".store-transaction").exists())

    def test_commit_token_keeps_new_data_and_recovery_is_repeatable(self):
        token = self.prepare(); self.mutate()
        self.activation.write_text(json.dumps({"storeTransaction": token}))
        self.assertTrue(transaction.recover(self.root, self.activation))
        self.assertEqual((self.root/"data/app/save").read_text(), "changed by update")
        transaction.recover(self.root, self.activation)
        self.assertEqual(json.loads((self.root/"packages.json").read_text()), {"example": "new-hash"})

    @unittest.skipUnless(sys.platform=='linux','Actual Linux archive and IPC inode semantics')
    def test_real_archive_rolls_back_stale_android_socket_and_fifo_without_opening_them(self):
        sock=self.root/'data/app/idle.sock';pipe=self.root/'data/app/idle.pipe'
        endpoint=socket.socket(socket.AF_UNIX);endpoint.bind(str(sock));endpoint.close();os.mkfifo(pipe)
        token=transaction.prepare(self.root,self.activation,lambda:None)
        self.assertTrue(token)
        backup=self.root/'.store-transaction/data/app'
        self.assertTrue(stat.S_ISSOCK((backup/'idle.sock').lstat().st_mode))
        self.assertTrue(stat.S_ISFIFO((backup/'idle.pipe').lstat().st_mode))
        self.mutate()
        self.assertFalse(transaction.recover(self.root,self.activation))
        self.assertEqual((self.root/'data/app/save').read_text(),'previous save')
        self.assertTrue(stat.S_ISSOCK(sock.lstat().st_mode));self.assertTrue(stat.S_ISFIFO(pipe.lstat().st_mode))

    def test_cancel_first_install_removes_only_the_new_android_data(self):
        shutil.rmtree(self.root/"data");(self.root/"packages.json").unlink()
        self.prepare()
        (self.root/"data/new-app").mkdir(parents=True)
        (self.root/"data/new-app/save").write_text("new")
        (self.root/"packages.json").write_text(json.dumps({"example":"new"}))
        self.assertFalse(transaction.recover(self.root,self.activation))
        self.assertFalse((self.root/"data").exists());self.assertFalse((self.root/"packages.json").exists())

    def test_interrupted_rollback_resumes_after_both_directory_renames(self):
        self.prepare(); self.mutate()
        target = self.root/".store-transaction"
        os.replace(self.root/"data", target/"discard")
        os.replace(target/"data", self.root/"data")
        self.assertFalse(transaction.recover(self.root, self.activation))
        self.assertEqual((self.root/"data/app/save").read_text(), "previous save")

    def test_incomplete_backup_cleanup_never_changes_data(self):
        target = self.root/".store-transaction"; target.mkdir(); (target/"partial").write_text("partial")
        transaction.recover(self.root, self.activation)
        self.assertFalse(target.exists())
        self.assertEqual((self.root/"data/app/save").read_text(), "previous save")

    def test_storage_limit_and_cancel_before_snapshot_leave_original_unchanged(self):
        with patch.object(transaction.shutil, "disk_usage", return_value=SimpleNamespace(free=0)):
            with self.assertRaisesRegex(ValueError, "storage"):
                self.prepare()
        with self.assertRaisesRegex(RuntimeError, "cancelled"):
            transaction.prepare(self.root, self.activation, Mock(side_effect=RuntimeError("cancelled")), runner=Copied)
        self.assertEqual((self.root/"data/app/save").read_text(), "previous save")
        self.assertFalse((self.root/".store-transaction").exists())

    def test_invalid_journal_preserves_backup_for_recovery(self):
        self.prepare()
        path = self.root/".store-transaction/transaction.json"
        path.write_text(json.dumps({"token": "../unsafe", "hadData": True}))
        with self.assertRaises(ValueError): transaction.recover(self.root, self.activation)
        self.assertTrue((self.root/".store-transaction/data/app/save").exists())


class AndroidSetupTests(unittest.TestCase):
    def test_setup_lock_refuses_an_active_android_operation(self):
        fcntl=SimpleNamespace(LOCK_EX=2,LOCK_NB=4,flock=Mock(side_effect=[None,BlockingIOError()]))
        with tempfile.TemporaryDirectory() as directory, patch.dict(sys.modules,{"fcntl":fcntl}), \
                patch.object(setup.os,"fchown",create=True), \
                patch.object(setup.os,"O_NOFOLLOW",getattr(os,"O_NOFOLLOW",0),create=True):
            runtime=Path(directory);caller=runtime.stat().st_uid
            with setup.android_setup_lock(runtime,caller):
                with self.assertRaisesRegex(RuntimeError,"Close the active Android session"):
                    with setup.android_setup_lock(runtime,caller):pass
    @unittest.skipUnless(sys.platform=="linux", "Linux no-follow directory descriptors")
    def test_backup_anchors_refuse_redirected_or_nonprivate_storage(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();private=root/"private";private.mkdir(mode=0o700)
            fd=setup.owned_directory_anchor(private,os.getuid())
            os.close(fd)
            private.chmod(0o755)
            with self.assertRaisesRegex(ValueError,"private"):
                setup.owned_directory_anchor(private,os.getuid())
            private.chmod(0o700)
            link=root/"redirect";link.symlink_to(private,target_is_directory=True)
            with self.assertRaises(OSError):setup.owned_directory_anchor(link,os.getuid())
    def test_windows_never_reaches_privileged_setup(self):
        with patch.object(sys, "platform", "win32"), patch.object(setup, "install_support") as install:
            with self.assertRaisesRegex(RuntimeError, "Linux appliance"): setup.platform()
            install.assert_not_called()

    def test_support_install_elevates_fixed_dpkg_and_never_user_python(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in setup.FILES:
                path = root/name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"fixture")
            postinst=[]
            def built(arguments,**kwargs):
                postinst.append((Path(arguments[-2])/'DEBIAN/postinst').read_text())
            with patch.object(setup, "trusted_support", side_effect=[ValueError("not installed"), None]), \
                    patch.object(setup, "run_step") as run, patch.object(setup.subprocess,'run',side_effect=built) as package, patch.object(setup, "emit"):
                setup.install_support(root, root)
                self.assertEqual(package.call_args.args[0][:3], ["/usr/bin/dpkg-deb", "--root-owner-group", "--build"])
                self.assertEqual(run.call_args_list[0].args[0][:3], ["/usr/bin/pkexec", "/usr/bin/dpkg", "-i"])
                post=postinst[0]
                self.assertIn('--refresh-store-metadata',post)
                self.assertNotIn(str(root),post)

    def test_root_setup_refuses_missing_pkexec_caller(self):
        with patch.object(setup, "trusted_support"), patch.dict(os.environ, {"PKEXEC_UID": ""}):
            with self.assertRaisesRegex(ValueError, "Store"):
                setup.root_setup(Path("/tmp/not-a-job"), False)

    def test_cancel_waits_for_current_system_step_without_killing_it(self):
        with tempfile.TemporaryDirectory() as directory:
            job = Path(directory)
            process = Mock(); process.returncode = 0
            count = 0
            def poll():
                nonlocal count
                count += 1
                if count==1: (job/"cancel").touch(); return None
                return 0
            process.poll.side_effect = poll
            with patch.object(setup.os, "geteuid", return_value=0, create=True), \
                    patch.object(setup.subprocess, "Popen", return_value=process), patch.object(setup.time, "sleep"):
                with self.assertRaisesRegex(RuntimeError, "safe checkpoint"):
                    setup.run_step(["/usr/bin/dpkg", "--configure", "-a"], job, 10)
            process.kill.assert_not_called(); process.terminate.assert_not_called()


class AndroidInstalledArtifactTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform=='linux','Production Linux session boundary')
    def test_content_session_return_error_and_two_profile_data_boundary(self):
        # This is a controlled external-runtime fixture, not decoded provider video.
        for profile,failed in [('default',False),('6b30896f-2192-47f4-bc93-f71977515e95',True)]:
            with self.subTest(profile=profile),tempfile.TemporaryDirectory() as directory:
                root=Path(directory).resolve();bundle=root/'bundle';(bundle/'config').mkdir(parents=True)
                (bundle/'config/android-runtime.json').write_text('{"schemaVersion":1}')
                shutil.copyfile(ROOT/'tools/android-store-transaction.py',bundle/'android-store-transaction.py')
                config=root/'config';(config/'Isolde').mkdir(parents=True)
                (config/'Isolde/profiles.json').write_text(json.dumps({'active':profile}))
                runtime=root/'runtime';runtime.mkdir(mode=0o700);data=root/'data'
                private=self.session.profile_root(profile,config,data)
                other='default' if profile!='default' else '6b30896f-2192-47f4-bc93-f71977515e95'
                foreign=data/'Isolde/android-profiles'/other;foreign.mkdir(mode=0o700)
                (foreign/'account-canary').write_bytes(b'other profile account fixture')
                events=[];state={};child=Mock(pid=23456);child.poll.side_effect=lambda:None if state else 0
                compositor=Mock();compositor.poll.return_value=None
                compositor_log=Mock();compositor_log.tell.return_value=0
                def spawn(*args,**kwargs):
                    self.assertEqual(kwargs['env']['HOME'],str(private/'home'))
                    self.assertEqual(kwargs['env']['WAYLAND_DISPLAY'],'fixture-kiosk')
                    state.update(state='RUNNING',pid=child.pid,user_id=os.getuid(),waydroid_data=str(private/'data/waydroid/data'))
                    return child
                child.send_signal.side_effect=lambda signal:state.clear()
                manager=Mock();manager.GetSession.side_effect=lambda **kwargs:dict(state)
                dbus=SimpleNamespace(Interface=lambda *args:manager,SystemBus=Mock(),SessionBus=Mock())
                dbus.SessionBus.return_value.name_has_owner.return_value=False
                def command(env,*args,**kwargs):
                    if args==('app','list'):return 'packageName: org.example.video'
                    if args==('app','launch','org.example.video'):
                        if failed:raise RuntimeError('Controlled provider decoder/session failure')
                        return ''
                    raise AssertionError(args)
                def sleep(seconds):raise self.session.Stopped()
                with ExitStack() as stack:
                    stack.enter_context(patch.dict(sys.modules,{'dbus':dbus}))
                    stack.enter_context(patch.dict(os.environ,{'XDG_CONFIG_HOME':str(config),'XDG_DATA_HOME':str(data),'XDG_RUNTIME_DIR':str(runtime),'WAYLAND_DISPLAY':'wayland-0'}))
                    stack.enter_context(patch.object(sys,'argv',['android-session','--profile',profile,'--app','fixture','--store-version','1']))
                    stack.enter_context(patch.object(self.session,'__file__',str(bundle/'android-session.py')))
                    stack.enter_context(patch.object(self.session,'store_package',return_value=({'packageName':'org.example.video','sha256':'fixture'},root/'fixture.apk')))
                    stack.enter_context(patch.object(self.session,'validate_store_apk'))
                    stack.enter_context(patch.object(self.session,'presentation_mode',return_value='fullscreen'))
                    stack.enter_context(patch.object(self.session,'start_compositor',return_value=(compositor,compositor_log,'fixture-kiosk')))
                    stop=stack.enter_context(patch.object(self.session,'stop_compositor'))
                    stack.enter_context(patch.object(self.session.subprocess,'Popen',side_effect=spawn))
                    stack.enter_context(patch.object(self.session,'command',side_effect=command))
                    stack.enter_context(patch.object(self.session.time,'sleep',side_effect=sleep))
                    stack.enter_context(patch.object(self.session,'emit',side_effect=lambda event,**fields:events.append(event)))
                    # Already-installed fixture avoids claiming mocked package installation.
                    (private/'packages.json').write_text('{"fixture":"fixture"}')
                    if failed:
                        with self.assertRaisesRegex(RuntimeError,'Controlled provider'):self.session.main()
                    else:self.session.main()
                    stop.assert_called_once();child.send_signal.assert_called_once()
                self.assertEqual('ready' in events,not failed)
                self.assertEqual((foreign/'account-canary').read_bytes(),b'other profile account fixture')
                self.assertFalse((private/'account-canary').exists());self.assertFalse(state)

    def setUp(self):
        if sys.platform=='linux':
            self.session = module("android_session_artifact_test", "android-session.py")
        else:
            with patch.dict(sys.modules, {"fcntl": SimpleNamespace()}):
                self.session = module("android_session_artifact_test", "android-session.py")

    @unittest.skipUnless(sys.platform=='linux','Linux display socket and ownership')
    def test_owned_compositor_has_private_socket_no_user_config_and_bounded_shutdown(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve();original=self.session.Path.stat;endpoint=None
            def metadata(path,*args,**kwargs):
                if str(path) in ('/usr/bin/weston','/usr/bin','/usr','/'):return SimpleNamespace(st_uid=0,st_mode=0o755)
                return original(path,*args,**kwargs)
            child=Mock();child.poll.return_value=None;child.wait.return_value=0
            def spawn(command,**kwargs):
                nonlocal endpoint
                self.assertIn('--fullscreen',command);self.assertIn('--shell=kiosk-shell.so',command);self.assertIn('--no-config',command)
                name=next(arg.split('=',1)[1] for arg in command if arg.startswith('--socket='))
                endpoint=socket.socket(socket.AF_UNIX);endpoint.bind(str(root/name));return child
            try:
                with patch.object(self.session.Path,'stat',metadata),patch.object(self.session,'emit'):
                    process,log,name=self.session.start_compositor({},root,root,runner=spawn)
                    self.assertTrue(name.startswith('isolde-android-'));self.assertEqual(process,child)
                    self.session.stop_compositor(process,log)
                child.terminate.assert_called_once();child.kill.assert_not_called()
            finally:
                if endpoint:endpoint.close()

    @unittest.skipUnless(sys.platform=='linux','Linux active preference ownership')
    def test_presentation_pref_rejects_unknown_modes_and_redirects(self):
        with tempfile.TemporaryDirectory() as directory:
            state=Path(directory)/'state.json';state.write_text('{"androidContainment":"windowed"}')
            self.assertEqual(self.session.presentation_mode(state),'windowed')
            state.write_text('{"androidContainment":"exec"}')
            with self.assertRaises(ValueError):self.session.presentation_mode(state)
            linked=Path(directory)/'redirect.json';linked.symlink_to(state)
            with self.assertRaises(ValueError):self.session.presentation_mode(linked)

    def test_old_package_presence_does_not_prove_new_apk_is_installed(self):
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory).resolve(); apk = data/"app/~~random/org.example.demo-random/base.apk"
            apk.parent.mkdir(parents=True); apk.write_bytes(b"old version")
            expected = hashlib.sha256(b"new version").hexdigest()
            self.assertFalse(self.session.installed_artifact(data, expected, "org.example.demo"))
            apk.write_bytes(b"new version")
            self.assertTrue(self.session.installed_artifact(data, expected, "org.example.demo"))

    def test_apk_identity_and_version_must_match_signed_metadata(self):
        result = b"package: name='org.example.demo' versionCode='8' versionName='new'\n"
        with patch.object(self.session, "package_tool", return_value=result):
            self.session.validate_store_apk(Path("fixture.apk"), {"packageName": "org.example.demo"}, "8")
            with self.assertRaisesRegex(ValueError, "identity or version"):
                self.session.validate_store_apk(Path("fixture.apk"), {"packageName": "org.example.demo"}, "7")

    def test_missing_root_owned_backup_support_never_elevates_user_python(self):
        with patch.object(self.session.subprocess,"Popen") as process, patch.object(self.session.Path,"exists",return_value=False):
            with self.assertRaisesRegex(RuntimeError,"Set up / repair Android"):
                self.session.administrator_transaction("prepare",Path("profile"),Path("activation"),"default")
            process.assert_not_called()

    def test_package_verification_refuses_oversized_output(self):
        child=Mock();child.returncode=0;child.poll.return_value=0
        def process(arguments,stdout,**unused):
            stdout.write(b"x"*(1024**2+1));return child
        with patch.object(self.session.subprocess,"Popen",side_effect=process):
            with self.assertRaisesRegex(RuntimeError,"verification failed"):
                self.session.package_tool(["/usr/bin/aapt","dump","badging","fixture.apk"])

    def test_missing_verification_tool_has_setup_recovery_message(self):
        with patch.object(self.session.subprocess,"Popen",side_effect=FileNotFoundError()):
            with self.assertRaisesRegex(RuntimeError,"Set up / repair Android"):
                self.session.package_tool(["/usr/bin/apksigner","verify","fixture.apk"])


if __name__=="__main__":
    unittest.main()
