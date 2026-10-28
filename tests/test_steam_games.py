"""Steam adapter contracts with inert manifests/processes, not owned gameplay."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock

spec=importlib.util.spec_from_file_location("steam_games",Path(__file__).parents[1]/"tools/steam-games.py")
steam=importlib.util.module_from_spec(spec);spec.loader.exec_module(steam)


class SteamGamesTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.home=Path(self.temp.name)/"private-home";self.root=self.home/".local/share/Steam/steamapps"
        (self.root/"common/Fixture").mkdir(parents=True)
        self.manifest=self.root/"appmanifest_42.acf"
        self.text='"AppState" { "appid" "42" "name" "Inert Steam fixture" "StateFlags" "4" "installdir" "Fixture" }'
        self.manifest.write_text(self.text)

    def test_discovery_reads_installed_metadata_without_writes(self):
        before={str(p):p.read_bytes() for p in self.home.rglob("*") if p.is_file()}
        result=steam.discover(self.home)
        self.assertEqual(result["games"],[dict(schemaVersion=1,title="Inert Steam fixture",platform="Steam",runtimeId="steam",root="",target="42")])
        self.assertEqual({str(p):p.read_bytes() for p in self.home.rglob("*") if p.is_file()},before)
        self.assertEqual(steam.discover(self.home.parent/"different-profile")["games"],[])

    def test_invalid_id_duplicate_keys_depth_and_size_refused(self):
        for value in ("0","01","-1","42 -bad","steam://run/42","4294967296",True):
            with self.assertRaises(ValueError):steam.app_id(value)
        for document in ('"x" "one" "x" "two"','"x" {','"x" bad','"x" {'*10+'"y" "z"'+'}'*10):
            with self.assertRaises(ValueError):steam.vdf(document)
        self.manifest.write_bytes(b"x"*65537)
        with self.assertRaises(ValueError):steam.discover(self.home)

    def test_comments_unicode_and_escaped_quotes(self):
        title='Fixture '+chr(92)+'"quoted'+chr(92)+'" café'
        self.manifest.write_text(self.text.replace('Inert Steam fixture',title)+' // comment\n')
        self.assertEqual(steam.discover(self.home)["games"][0]["title"],'Fixture "quoted" café')

    def test_uninstalled_updating_removed_and_changed_identity_do_not_launch(self):
        for flags in ("0","2","128","1024"):
            self.manifest.write_text(self.text.replace('"4"','"'+flags+'"'))
            self.assertEqual(steam.discover(self.home)["games"],[])
            with self.assertRaises(ValueError):steam.verify_installed(self.home,"42")
        self.manifest.write_text(self.text.replace('"appid" "42"','"appid" "43"'))
        with self.assertRaises(ValueError):steam.verify_installed(self.home,"42")
        self.manifest.unlink()
        with self.assertRaises(ValueError):steam.verify_installed(self.home,"42")

    def test_external_library_is_not_read_or_granted(self):
        external=self.home.parent/"another-account";outside=external/"steamapps"
        outside.mkdir(parents=True);(outside/"appmanifest_90.acf").write_text("unreadable and never parsed")
        (self.root/"libraryfolders.vdf").write_text('"libraryfolders" { "0" { "path" '+__import__('json').dumps(str(external))+' } }')
        result=steam.discover(self.home)
        self.assertEqual(len(result["games"]),1);self.assertTrue(result["warnings"])

    def test_redirected_game_manifest_and_profile_refused(self):
        self.manifest.write_text(self.text.replace('"Fixture"','"../outside"'))
        with self.assertRaises(ValueError):steam.discover(self.home)
        self.manifest.unlink();outside=self.home.parent/"outside.acf";outside.write_text(self.text)
        self.manifest.symlink_to(outside)
        with self.assertRaises(ValueError):steam.discover(self.home)
        redirected=self.home.parent/"alias";redirected.symlink_to(self.home, target_is_directory=True)
        with self.assertRaises(ValueError):steam.discover(redirected)

    def test_bootstrap_exit_zero_is_not_game_start_and_timeout_is_failure(self):
        elapsed=[0]
        def sleep(seconds):elapsed[0]+=seconds
        launcher=Mock();launcher.poll.return_value=0
        def processes(identity):return [100] if elapsed[0]>=1 else []
        steam.wait_for_game("42",launcher,timeout=2,clock=lambda:elapsed[0],sleep=sleep,processes=processes)
        self.assertGreaterEqual(elapsed[0],1)
        with self.assertRaisesRegex(RuntimeError,"did not start"):
            steam.wait_for_game("42",launcher,timeout=1,clock=lambda:elapsed[0],sleep=sleep,processes=lambda _:[])
        launcher.poll.return_value=1
        with self.assertRaisesRegex(RuntimeError,"could not start"):
            steam.wait_for_game("42",launcher,clock=lambda:0,processes=lambda _:[])

    @unittest.skipUnless(Path("/proc/self/environ").exists(),"Linux process environment")
    def test_process_probe_matches_only_requested_game_identity(self):
        children=[]
        try:
            for identity in ("42","43"):
                child=subprocess.Popen([sys.executable,"-c","import time; time.sleep(10)"],env=dict(os.environ,SteamAppId=identity))
                children.append(child)
            deadline=time.monotonic()+2
            while time.monotonic()<deadline and children[0].pid not in steam.game_processes("42"):time.sleep(.02)
            self.assertIn(children[0].pid,steam.game_processes("42"))
            self.assertNotIn(children[1].pid,steam.game_processes("42"))
            self.assertNotIn(os.getpid(),steam.game_processes("42"))
        finally:
            for child in children:child.terminate();child.wait(timeout=3)

    def test_launch_outside_namespace_refused_before_starting_client(self):
        from unittest.mock import patch
        with patch.dict(os.environ,{"ISOLDE_NATIVE_NAMESPACE":"0"}),patch.object(steam.subprocess,"Popen") as start:
            with self.assertRaisesRegex(ValueError,"isolated"):steam.launch("42")
            start.assert_not_called()

    def test_fifo_metadata_refused_without_blocking(self):
        self.manifest.unlink();os.mkfifo(self.manifest)
        started=time.monotonic()
        with self.assertRaisesRegex(ValueError,"regular file"):steam.discover(self.home)
        self.assertLess(time.monotonic()-started,1)

    @unittest.skipUnless(Path("/proc/self/environ").exists(),"Linux owned processes")
    def test_shutdown_stops_only_this_launch_token(self):
        import uuid
        token=uuid.uuid4().hex;children=[]
        try:
            for value in (token,uuid.uuid4().hex):
                children.append(subprocess.Popen([sys.executable,"-c","import time; time.sleep(20)"],env=dict(os.environ,ISOLDE_STEAM_SESSION=value)))
            deadline=time.monotonic()+2
            while time.monotonic()<deadline and children[0].pid not in steam.matching_processes("ISOLDE_STEAM_SESSION",token):time.sleep(.02)
            steam.stop_owned_session(token)
            children[0].wait(timeout=3)
            self.assertIsNone(children[1].poll())
        finally:
            for child in children:
                if child.poll() is None:child.terminate()
                child.wait(timeout=3)


if __name__=="__main__":unittest.main()
