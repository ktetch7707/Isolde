import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

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
            with patch.object(setup, "trusted_support", side_effect=[ValueError("not installed"), None]), \
                    patch.object(setup, "run_step") as run, patch.object(setup, "emit"):
                setup.install_support(root, root)
                self.assertEqual(run.call_args_list[0].args[0][:3], ["/usr/bin/dpkg-deb", "--root-owner-group", "--build"])
                self.assertEqual(run.call_args_list[1].args[0][:3], ["/usr/bin/pkexec", "/usr/bin/dpkg", "-i"])

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
    def setUp(self):
        with patch.dict(sys.modules, {"fcntl": SimpleNamespace()}):
            self.session = module("android_session_artifact_test", "android-session.py")

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
