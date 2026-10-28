import importlib.util
from pathlib import Path
import tempfile
import json
import os
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("games_provision", Path(__file__).parents[1]/"tools/provision-games.py")
games = importlib.util.module_from_spec(spec)
spec.loader.exec_module(games)
adapter_spec=importlib.util.spec_from_file_location("emulator_session",Path(__file__).parents[1]/"tools/emulator-session.py")
adapter=importlib.util.module_from_spec(adapter_spec);adapter_spec.loader.exec_module(adapter)


class GamesProvisioningTests(unittest.TestCase):
    def test_no_install_without_explicit_flag(self):
        state={"dolphin-emu":{"ready":False}}
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value=state),patch.object(games,"run") as run:
            result=games.provision(["dolphin-emu"])
            self.assertFalse(result["ready"]);run.assert_not_called()

    def test_repeat_setup_is_idempotent(self):
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value={"pcsx2":{"ready":True}}),patch.object(games,"run") as run:
            self.assertTrue(games.provision(["pcsx2"],True)["ready"]);run.assert_not_called()

    def test_install_is_pinned_and_failure_is_not_ready(self):
        states=[{"pcsx2":{"ready":False}},{"pcsx2":{"ready":False}}]
        with patch.object(games,"platform"),patch.object(games,"inspect",side_effect=states),patch.object(games.shutil,"disk_usage") as disk,patch.object(games,"run") as run:
            disk.return_value.free=8*1024**3
            with self.assertRaises(RuntimeError):games.provision(["pcsx2"],True)
            command=run.call_args.args
            self.assertIn("pcsx2="+games.PACKAGES["pcsx2"][0],command)
            self.assertIn("--no-remove",command)

    def test_unknown_package_and_redirected_receipt_refused(self):
        with patch.object(games,"platform"):
            with self.assertRaises(ValueError):games.provision(["arbitrary"])
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"original").write_text("preserve")
            (root/"receipt").symlink_to(root/"original")
            with self.assertRaises(ValueError):games.atomic_receipt(root/"receipt",{})
            self.assertEqual((root/"original").read_text(),"preserve")

    def test_launchers_use_only_verified_programs_and_are_repeatable(self):
        state={"packages":{name:{"ready":True,"executable":value[1]} for name,value in games.PACKAGES.items()}}
        with tempfile.TemporaryDirectory() as directory:
            written=games.register_launchers(state,Path(directory))
            self.assertEqual(len(written),8)
            original={p:Path(p).read_bytes() for p in written}
            games.register_launchers(state,Path(directory))
            self.assertEqual(original,{p:Path(p).read_bytes() for p in written})
            self.assertIn(b"--fullscreen --renderer sdl",(Path(directory)/"isolde-supertux.desktop").read_bytes())
            state["packages"]["supertux"]["executable"]="/tmp/untrusted"
            with self.assertRaises(ValueError):games.register_launchers(state,Path(directory))

    def test_launcher_registration_respects_private_xdg_data_home(self):
        state={"packages":{name:{"ready":True,"executable":value[1]} for name,value in games.PACKAGES.items()}}
        with tempfile.TemporaryDirectory() as directory,patch.dict(os.environ,{"XDG_DATA_HOME":directory}):
            written=games.register_launchers(state)
            self.assertEqual(len(written),8)
            self.assertTrue(all(Path(path).parent==Path(directory)/"applications" for path in written))


class EmulatorAdapterTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory();self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.home=self.root/"private";self.home.mkdir()
        self.binary=self.root/"runtime";self.binary.write_text("inert adapter test");self.binary.chmod(0o700)
        self.runtime_definitions=adapter.definitions(Path(__file__).parents[1]/"config/game-runtimes.json")

    def runtime(self,identity,suffix):
        value=dict(self.runtime_definitions[identity],binary=str(self.binary))
        target=self.root/("game with spaces."+suffix);target.write_text("inert validation fixture")
        if "core" in value:
            core=self.root/"core.so";core.write_text("inert core fixture");value["core"]=str(core)
        return value,str(target)

    def test_all_declared_routes_build_identity_only_commands(self):
        for identity in self.runtime_definitions:
            runtime,target=self.runtime(identity,self.runtime_definitions[identity]["formats"][0])
            if identity=="pcsx2":
                with self.assertRaisesRegex(ValueError,"firmware is required"):adapter.command(runtime,target,self.home)
                continue
            with patch.object(adapter,"dolphin_controller",return_value="Xbox Controller"):
                argv=adapter.command(runtime,target,self.home)
            self.assertEqual(argv[0],str(self.binary));self.assertIn(target,argv)
            self.assertEqual(argv.count(target),1)
        self.assertFalse((self.home/".config/isolde-emulator/settings.json").exists())

    def test_unknown_settings_preserved_and_source_never_written(self):
        runtime,target=self.runtime("ppsspp","iso");before=Path(target).read_bytes()
        path=self.home/".config/isolde-emulator/settings.json"
        adapter.atomic(path,b'{"schemaVersion":99}')
        with self.assertRaises(ValueError):adapter.command(runtime,target,self.home)
        self.assertEqual(path.read_bytes(),b'{"schemaVersion":99}');self.assertEqual(Path(target).read_bytes(),before)

    def test_redirected_storage_and_wrong_format_refused(self):
        runtime,target=self.runtime("ppsspp","nes")
        with self.assertRaisesRegex(ValueError,"format"):adapter.command(runtime,target,self.home)
        runtime,target=self.runtime("ppsspp","iso")
        (self.home/".config").symlink_to(self.root,target_is_directory=True)
        with self.assertRaisesRegex(ValueError,"redirected"):adapter.command(runtime,target,self.home)

    def test_firmware_hash_checked_and_private_save_paths(self):
        runtime,target=self.runtime("pcsx2","iso")
        data=b"ROMDIR ROMVER"+bytes(4*1024**2-len(b"ROMDIR ROMVER"))
        import hashlib
        name=hashlib.sha256(data).hexdigest()+".bin"
        adapter.atomic(self.home/"firmware"/name,data)
        adapter.atomic(self.home/".config/isolde-emulator/settings.json",json.dumps({"schemaVersion":1,"video":"software","controller":"xbox","firmware":name}).encode())
        argv=adapter.command(runtime,target,self.home)
        self.assertNotIn("-settings",argv);self.assertIn("-nogui",argv)
        config=(self.home/".config/PCSX2/inis/PCSX2.ini").read_text()
        self.assertIn(str(self.home/"saves"),config);self.assertIn("SDL-0/A",config)
        (self.home/"firmware"/name).write_bytes(bytes(len(data)))
        with self.assertRaisesRegex(ValueError,"changed"):adapter.command(runtime,target,self.home)

    def test_existing_save_bytes_survive_config_regeneration(self):
        runtime,target=self.runtime("retroarch-gba","gba")
        (self.home/"saves").mkdir();save=self.home/"saves/game.srm";save.write_bytes(b"private save")
        adapter.command(runtime,target,self.home);adapter.command(runtime,target,self.home)
        self.assertEqual(save.read_bytes(),b"private save")
        config=(self.home/".config/isolde-emulator/generated/retroarch.cfg").read_text()
        self.assertIn('config_save_on_exit = "false"',config)
        self.assertIn(str(self.home/"saves"),config)


class GameFixtureTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "posix", "Guest process-group contract")
    def test_builder_timeout_terminates_descendants(self):
        import subprocess, sys, time
        spec=importlib.util.spec_from_file_location("psp_timeout",Path(__file__).parents[1]/"tools/build-ppsspp.py")
        builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
        with tempfile.TemporaryDirectory() as temporary:
            marker=Path(temporary)/"child"
            script="import subprocess,sys,time; p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)']); open(sys.argv[1],'w').write(str(p.pid)); time.sleep(30)"
            with self.assertRaises(subprocess.TimeoutExpired):
                builder.run(sys.executable,"-c",script,str(marker),timeout=1)
            child=Path("/proc")/marker.read_text()/"stat"
            # SIGKILL delivery to a descendant can settle after the leader's
            # wait completes. Require it to stop within a bounded grace period;
            # reaping an orphan is init's responsibility, so Z also means stopped.
            deadline=time.monotonic()+2
            while True:
                try:
                    child_state=child.read_text().split()[2]
                except (FileNotFoundError,ProcessLookupError):
                    break
                if child_state=="Z":break
                if time.monotonic()>=deadline:
                    self.fail("Timed-out builder left a live descendant: "+child_state)
                time.sleep(.02)

    def test_archive_pin_and_traversal_refused_without_extraction(self):
        spec=importlib.util.spec_from_file_location("fixture_fetch",Path(__file__).parents[1]/"tools/fetch-game-fixtures.py")
        fixtures=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixtures)
        with self.assertRaisesRegex(ValueError,"hash mismatch"):fixtures.members(b"untrusted")
        import hashlib,io,zipfile
        output=io.BytesIO()
        with zipfile.ZipFile(output,"w") as archive:
            archive.writestr("webfest/EBOOT.PBP",b"inert")
            archive.writestr("webfest/../../outside",b"never write")
        payload=output.getvalue()
        with patch.object(fixtures,"SHA256",hashlib.sha256(payload).hexdigest()):
            with self.assertRaisesRegex(ValueError,"Unsafe"):fixtures.members(payload)

    def test_offline_psp_payload_is_reused_only_with_pinned_source_and_hash(self):
        spec=importlib.util.spec_from_file_location("psp_build",Path(__file__).parents[1]/"tools/build-ppsspp.py")
        builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
        import hashlib,types
        with tempfile.TemporaryDirectory() as temporary,patch.object(builder.Path,"home",return_value=Path(temporary)),patch.object(builder,"run") as run:
            cache=Path(temporary)/".cache/isolde-emulators/ppsspp-1.20.4";cache.mkdir(parents=True)
            package=cache/("isolde-ppsspp_"+builder.VERSION+"_amd64.deb");package.write_bytes(b"inert offline payload")
            (cache/"payload.json").write_text(json.dumps({"sourceCommit":builder.COMMIT,"sha256":hashlib.sha256(package.read_bytes()).hexdigest()}))
            from unittest.mock import Mock
            provisioner=types.SimpleNamespace(platform=Mock())
            self.assertEqual(builder.build(cache,provisioner),package);run.assert_not_called()
            package.write_bytes(b"changed payload")
            with self.assertRaisesRegex(ValueError,"hash mismatch"):builder.build(cache,provisioner)
            run.assert_not_called()


if __name__=="__main__":unittest.main()
