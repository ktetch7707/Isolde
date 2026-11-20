import importlib.util
from pathlib import Path
import tempfile
import json
import os
import subprocess
import sys
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("games_provision", Path(__file__).parents[1]/"tools/provision-games.py")
games = importlib.util.module_from_spec(spec)
spec.loader.exec_module(games)
adapter_spec=importlib.util.spec_from_file_location("emulator_session",Path(__file__).parents[1]/"tools/emulator-session.py")
adapter=importlib.util.module_from_spec(adapter_spec);adapter_spec.loader.exec_module(adapter)


class GamesProvisioningTests(unittest.TestCase):
    def setUp(self):
        # This class supplies a temporary executable/core boundary. Expanded
        # payload-tree validation has its own real-filesystem regression below.
        self.payload_patch=patch.object(games,"upstream_fingerprint",return_value="b"*64)
        self.payload_patch.start();self.addCleanup(self.payload_patch.stop)
    def launcher_receipt(self,directory):
        source=Path(__file__).parents[1]/"config/game-runtimes.json"
        document=json.loads(source.read_text())
        binary=Path(directory)/"runtime";binary.write_text("verified inert runtime");binary.chmod(0o700)
        core=Path(directory)/"core.so";core.write_text("verified inert core")
        for runtime in document["runtimes"]:
            runtime["binary"]=str(binary)
            if "core" in runtime:runtime["core"]=str(core)
        manifest=Path(directory)/"game-runtimes.json";manifest.write_text(json.dumps(document))
        route_definitions=adapter.definitions(manifest)
        packages={name:{"expectedVersion":value[0],"installedVersion":value[0],"ready":True,
            "executable":value[1],"sha256":"a"*64} for name,value in games.PACKAGES.items()}
        with patch.object(games,"runtime_manifest",return_value=(manifest,route_definitions)):
            manifest_hash,runtimes=games.inspect_runtimes(packages)
        return {"schemaVersion":2,"manifestSha256":manifest_hash,"packages":packages,"runtimes":runtimes},manifest,route_definitions

    def test_no_install_without_explicit_flag(self):
        state={"dolphin-emu":{"ready":False}}
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value=state),patch.object(games,"run") as run:
            result=games.provision(["dolphin-emu"])
            self.assertFalse(result["ready"]);run.assert_not_called()

    def test_repeat_setup_is_idempotent(self):
        routes={name:{"ready":True} for name in adapter.definitions(Path(__file__).parents[1]/"config/game-runtimes.json")}
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value={"pcsx2":{"ready":True}}),patch.object(games,"inspect_runtimes",return_value=("a"*64,routes)),patch.object(games,"run") as run:
            self.assertTrue(games.provision(["pcsx2"],True)["ready"]);run.assert_not_called()

    def test_install_is_pinned_and_failure_is_not_ready(self):
        states=[{"pcsx2":{"ready":False}},{"pcsx2":{"ready":False}}]
        with patch.object(games,"platform"),patch.object(games,"inspect",side_effect=states),\
                patch.object(games,"recover_package_transaction"),patch.object(games,"prepare_bundle",return_value=Path("bundle")),\
                patch.object(games,"apply_bundle",side_effect=RuntimeError("fixture failure")) as apply:
            with self.assertRaises(RuntimeError):games.provision(["pcsx2"],True)
            apply.assert_called_once_with(Path("bundle"),update=False,offline=False)

    def test_repair_reinstalls_damaged_matching_version_without_upgrading(self):
        broken={"isolde-xenia":{"ready":False,"installedVersion":games.PACKAGES["isolde-xenia"][0]}}
        with patch.object(games,"platform"),patch.object(games,"inspect",side_effect=[broken,broken]),\
                patch.object(games,"recover_package_transaction"),patch.object(games,"prepare_bundle",return_value=Path("bundle")),\
                patch.object(games,"apply_bundle",side_effect=RuntimeError("fixture failure")) as apply:
            with self.assertRaises(RuntimeError):games.provision(["isolde-xenia"],True)
            apply.assert_called_once_with(Path("bundle"),update=False,offline=False,repair=["isolde-xenia"])

    def test_changed_installed_runtime_is_preserved_instead_of_silently_downgraded(self):
        state={"pcsx2":{"ready":False,"installedVersion":"2.5.0"}}
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value=state),patch.object(games,"run") as run:
            with self.assertRaisesRegex(RuntimeError,"Preserve it"):
                games.provision(["pcsx2"],True)
            run.assert_not_called()

    @unittest.skipUnless(os.name=="posix","Bounded package-operation signal contract")
    def test_cancel_finishes_active_package_command_before_stopping(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);marker=root/"completed"
            script="""
import importlib.util,os,signal,sys,threading
spec=importlib.util.spec_from_file_location('games',sys.argv[1])
games=importlib.util.module_from_spec(spec);spec.loader.exec_module(games)
games.install_cancellation_handlers()
threading.Timer(.05,lambda:os.kill(os.getpid(),signal.SIGTERM)).start()
try:
    games.run(sys.executable,'-c','import pathlib,sys,time; time.sleep(.25); pathlib.Path(sys.argv[1]).write_text("done")',sys.argv[2],timeout=3,transaction=True)
except games.ProvisioningCancelled:
    print('stopped-at-boundary')
"""
            result=subprocess.run([sys.executable,"-c",script,str(Path(__file__).parents[1]/"tools/provision-games.py"),str(marker)],
                capture_output=True,text=True,timeout=5)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stdout.strip(),"stopped-at-boundary")
            self.assertEqual(marker.read_text(),"done")

    def test_unknown_package_and_redirected_receipt_refused(self):
        with patch.object(games,"platform"):
            with self.assertRaises(ValueError):games.provision(["arbitrary"])
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"original").write_text("preserve")
            (root/"receipt").symlink_to(root/"original")
            with self.assertRaises(ValueError):games.atomic_receipt(root/"receipt",{})
            self.assertEqual((root/"original").read_text(),"preserve")

    def test_launchers_use_only_verified_programs_and_are_repeatable(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);state,manifest,routes=self.launcher_receipt(root)
            with patch.object(games,"runtime_manifest",return_value=(manifest,routes)):
                written=games.register_launchers(state,root)
                self.assertEqual(len(written),len(routes)+1)
                original={p:Path(p).read_bytes() for p in written}
                games.register_launchers(state,root)
                self.assertEqual(original,{p:Path(p).read_bytes() for p in written})
                self.assertIn(b"--fullscreen --renderer sdl",(root/"isolde-supertux.desktop").read_bytes())
                executable=Path(routes["ppsspp"]["binary"]);executable.write_text("changed runtime")
                with self.assertRaisesRegex(ValueError,"receipt"):games.register_launchers(state,root)
                executable.write_text("verified inert runtime")
                state["packages"]["supertux"]["executable"]="/untrusted"
                with self.assertRaises(ValueError):games.register_launchers(state,root)

    def test_launcher_registration_respects_private_xdg_data_home(self):
        with tempfile.TemporaryDirectory() as directory,patch.dict(os.environ,{"XDG_DATA_HOME":directory}):
            root=Path(directory);state,manifest,routes=self.launcher_receipt(root)
            with patch.object(games,"runtime_manifest",return_value=(manifest,routes)):
                written=games.register_launchers(state)
                self.assertEqual(len(written),len(routes)+1)
                self.assertTrue(all(Path(path).parent==root/"applications" for path in written))


class RuntimeBundleTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        self.bundle=self.root/"bundle"
        (self.bundle/"archives").mkdir(parents=True)
        self.entries=[]
        for name,version in games.emulator_package_pins().items():
            filename=name+"_"+version+"_amd64.deb"
            payload=("fixture package "+name+" "+version).encode()
            (self.bundle/"archives"/filename).write_bytes(payload)
            self.entries.append({"package":name,"version":version,"architecture":"amd64","file":filename,
                "size":len(payload),"sha256":__import__("hashlib").sha256(payload).hexdigest()})
        manifest_path,_=games.runtime_manifest()
        self.document={"schemaVersion":1,"target":{"os":"ubuntu","version":"26.04","architecture":"amd64"},
            "runtimeManifestSha256":__import__("hashlib").sha256(manifest_path.read_bytes()).hexdigest(),
            "runtimePackages":games.emulator_package_pins(),"packages":self.entries}
        (self.bundle/"bundle.json").write_text(json.dumps(self.document))

    def fake_runner(self,*args,**kwargs):
        if args[0]=="/usr/bin/dpkg-deb":
            archive=Path(args[2])
            item=next(entry for entry in self.entries if entry["file"]==archive.name)
            if args[-1]=="X-Isolde-Source-Commit":
                return subprocess.CompletedProcess(args,0,"fa50bb1976065c4f8b1b47af227d367fe9771555\n","")
            if args[-1]=="X-Isolde-Artifact-Sha256":
                row=next(row for row in games.UPSTREAM_ARTIFACTS.values() if row["package"]==item["package"])
                return subprocess.CompletedProcess(args,0,row["sha256"]+"\n","")
            return subprocess.CompletedProcess(args,0,
                "\n".join(item[field] for field in ("package","version","architecture"))+"\n","")
        if args[0]=="apt-cache":
            package,version=args[2].split("=",1)
            item=next(entry for entry in self.entries if entry["package"]==package and entry["version"]==version)
            record="Package: "+package+"\nVersion: "+version+"\nArchitecture: "+item["architecture"]+"\nSHA256: "+item["sha256"]+"\n"
            return subprocess.CompletedProcess(args,0,record,"")
        raise AssertionError(args)

    def signed_bundle(self):
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            games.sign_bundle(self.bundle)

    def test_bundle_detects_archive_changes_and_requires_trusted_signature(self):
        self.signed_bundle()
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            doc=games.validate_bundle(self.bundle,self.document["runtimeManifestSha256"],self.fake_runner)
            def no_index_access(*args,**kwargs):
                if args[0]=="apt-cache":raise AssertionError("offline verification must not read or refresh package indexes")
                return self.fake_runner(*args,**kwargs)
            games.authenticate_bundle(self.bundle,doc,no_index_access)
            games.authenticate_bundle(self.bundle,doc,self.fake_runner,verify_repository_indices=True)
            def changed_index(*args,**kwargs):
                result=self.fake_runner(*args,**kwargs)
                if args[0]=="apt-cache":
                    return subprocess.CompletedProcess(args,0,result.stdout.replace("SHA256: ","SHA256: f"),"")
                return result
            with self.assertRaisesRegex(ValueError,"signed Ubuntu package index"):
                games.authenticate_bundle(self.bundle,doc,changed_index,verify_repository_indices=True)
            archive=self.bundle/"archives"/self.entries[0]["file"]
            archive.write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError,"hash or metadata"):
                games.validate_bundle(self.bundle,self.document["runtimeManifestSha256"],self.fake_runner)
            archive.write_bytes(("fixture package "+self.entries[0]["package"]+" "+self.entries[0]["version"]).encode())
            games.sign_bundle(self.bundle)
            with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"other-state")}):
                other_key=games.bundle_signing_paths()
            with patch.object(games,"bundle_signing_paths",return_value=other_key):
                with self.assertRaisesRegex(ValueError,"signature"):
                    games.authenticate_bundle(self.bundle,doc,self.fake_runner)

    def test_failed_install_purges_only_new_packages_and_removes_checkpoint(self):
        self.signed_bundle()
        state={}
        commands=[]
        def runner(*args,**kwargs):
            if args[0] in ("/usr/bin/dpkg-deb","apt-cache"):
                return self.fake_runner(*args,**kwargs)
            if args[0]=="dpkg-query":
                name=args[-1]
                if name in state:
                    return subprocess.CompletedProcess(args,0,"install ok installed\n"+state[name]+"\n","")
                return subprocess.CompletedProcess(args,1,"","")
            if args[0]=="sudo" and "/usr/bin/apt-get" in args:
                operation=next(value for value in ("install","purge") if value in args);commands.append(args)
                if operation=="install":
                    state[self.entries[0]["package"]]=self.entries[0]["version"]
                    state[self.entries[1]["package"]]=self.entries[1]["version"]
                    raise subprocess.CalledProcessError(1,args)
                if operation=="purge":
                    if "--simulate" in args:
                        names=args[args.index("purge")+1:]
                        return subprocess.CompletedProcess(args,0,"".join("Remv "+name+" [1.0]\n" for name in names),"")
                    state.clear()
                    return subprocess.CompletedProcess(args,0,"","")
            raise AssertionError(args)
        checkpoint=self.root/"checkpoint.json"
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            with self.assertRaisesRegex(RuntimeError,"newly added packages were rolled back"):
                games.apply_bundle(self.bundle,runner=runner,checkpoint=checkpoint)
        self.assertFalse(checkpoint.exists())
        simulation=next(command for command in commands if "purge" in command and "--simulate" in command)
        purge=next(command for command in commands if "purge" in command and "--simulate" not in command)
        self.assertIn("--simulate",simulation)
        self.assertNotIn("--no-remove",purge)
        self.assertEqual(state,{})

    def test_offline_bundle_refuses_forged_upstream_artifact_identity(self):
        self.signed_bundle()
        def forged(*args,**kwargs):
            if args[-1]=="X-Isolde-Artifact-Sha256":
                return subprocess.CompletedProcess(args,0,"0"*64+"\n","")
            if args[0]=="apt-cache":raise AssertionError("Offline validation must not use package indexes")
            return self.fake_runner(*args,**kwargs)
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            doc=games.validate_bundle(self.bundle,self.document["runtimeManifestSha256"],self.fake_runner)
            with self.assertRaisesRegex(ValueError,"pinned upstream artifact"):
                games.authenticate_bundle(self.bundle,doc,forged)

    def test_same_version_payload_repair_uses_reinstall_without_removal_or_upgrade(self):
        self.signed_bundle();commands=[]
        before={name:{"status":"install ok installed","version":version}
                for name,version in games.emulator_package_pins().items()}
        def runner(*args,**kwargs):
            if args[0]=="dpkg-query":
                state=before[args[-1]]
                return subprocess.CompletedProcess(args,0,state["status"]+"\n"+state["version"]+"\n","")
            if args[0]=="sudo":
                commands.append(args);return subprocess.CompletedProcess(args,0,"","")
            return self.fake_runner(*args,**kwargs)
        checkpoint=self.root/"repair.json"
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            result=games.apply_bundle(self.bundle,installed=before,runner=runner,
                checkpoint=checkpoint,offline=True,repair=["isolde-xenia"])
        self.assertTrue(result["ready"]);self.assertEqual(result["installed"],before)
        self.assertEqual(len(commands),1)
        for flag in ("--reinstall","--no-download","--no-upgrade","--no-remove"):
            self.assertIn(flag,commands[0])
        xenia=next(item for item in self.entries if item["package"]=="isolde-xenia")
        self.assertEqual(commands[0][-1],str(self.bundle/"archives"/xenia["file"]))
        self.assertFalse(checkpoint.exists())

    def test_clean_offline_install_passes_local_archives_and_absent_dependencies(self):
        name="fixture-runtime-dependency";archive=self.bundle/"archives"/(name+"_1.0_amd64.deb")
        archive.write_bytes(b"inert dependency boundary fixture")
        self.entries.append({"package":name,"version":"1.0","architecture":"amd64","file":archive.name,
            "size":archive.stat().st_size,"sha256":__import__("hashlib").sha256(archive.read_bytes()).hexdigest()})
        (self.bundle/"bundle.json").write_text(json.dumps(self.document));self.signed_bundle()
        state={};commands=[]
        def runner(*args,**kwargs):
            if args[0]=="dpkg-query":
                value=state.get(args[-1])
                return subprocess.CompletedProcess(args,0 if value else 1,
                    "install ok installed\n"+value+"\n" if value else "","")
            if args[0]=="apt-cache":raise AssertionError("A clean offline target has no package indexes")
            if args[0]=="sudo":
                commands.append(args)
                requested=args[args.index("install")+1:]
                self.assertEqual(set(requested),{str(self.bundle/"archives"/entry["file"]) for entry in self.entries})
                state.update({entry["package"]:entry["version"] for entry in self.entries})
                return subprocess.CompletedProcess(args,0,"","")
            return self.fake_runner(*args,**kwargs)
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            result=games.apply_bundle(self.bundle,runner=runner,checkpoint=self.root/"offline.json",offline=True)
        self.assertTrue(result["ready"]);self.assertIn(name,result["added"])
        self.assertEqual(len(commands),1);self.assertIn("--no-download",commands[0])

    def test_interrupted_transaction_is_recovered_before_retry(self):
        self.signed_bundle()
        names={entry["package"] for entry in self.entries}
        before={name:None for name in names}
        installed_name=self.entries[0]["package"]
        state={installed_name:self.entries[0]["version"]}
        checkpoint=self.root/"checkpoint.json"
        checkpoint.write_text(json.dumps({"schemaVersion":1,"phase":"applying","bundle":str(self.bundle),
            "before":before,"requested":{installed_name:self.entries[0]["version"]},"createdAt":1}))
        commands=[]
        def runner(*args,**kwargs):
            if args[0] in ("/usr/bin/dpkg-deb","apt-cache"):
                return self.fake_runner(*args,**kwargs)
            if args[0]=="dpkg-query":
                name=args[-1]
                if name in state:return subprocess.CompletedProcess(args,0,"install ok installed\n"+state[name]+"\n","")
                return subprocess.CompletedProcess(args,1,"","")
            if args[0]=="sudo" and "/usr/bin/apt-get" in args and "purge" in args:
                commands.append(args)
                if "--simulate" in args:
                    names=args[args.index("purge")+1:]
                    return subprocess.CompletedProcess(args,0,"".join("Remv "+name+" [1.0]\n" for name in names),"")
                state.clear()
                return subprocess.CompletedProcess(args,0,"","")
            raise AssertionError(args)
        with patch.dict(os.environ,{"XDG_STATE_HOME":str(self.root/"state")}):
            self.assertTrue(games.recover_package_transaction(checkpoint,runner))
        self.assertFalse(checkpoint.exists())
        self.assertEqual(state,{})
        self.assertIn("--simulate",commands[0])

    def test_rollback_refuses_to_purge_packages_outside_its_additions(self):
        commands=[]
        def runner(*args,**kwargs):
            commands.append(args)
            return subprocess.CompletedProcess(args,0,"Remv isolde-ppsspp [1.0]\nRemv unrelated [1.0]\n","")
        with self.assertRaisesRegex(RuntimeError,"purge plan differs"):
            games.purge_added_packages(["isolde-ppsspp"],runner)
        self.assertEqual(len(commands),1)
        self.assertIn("--simulate",commands[0])

    def test_offline_provision_uses_bundle_without_network_or_index_refresh(self):
        states=[{"pcsx2":{"ready":False}},{"pcsx2":{"ready":True}}]
        with patch.object(games,"platform"),patch.object(games,"inspect",side_effect=states),\
                patch.object(games,"recover_package_transaction"),patch.object(games,"find_bundle",return_value=self.bundle),\
                patch.object(games,"apply_bundle") as apply,patch.object(games,"inspect_runtimes",return_value=("a"*64,{})),\
                patch.object(games,"run") as run:
            result=games.provision(["pcsx2"],True,offline=True)
        self.assertTrue(result["ready"])
        apply.assert_called_once_with(self.bundle,update=False,offline=True)
        run.assert_not_called()


class RuntimeUpdateTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        self.before={name:{"status":"install ok installed","version":"old-"+version}
            for name,version in games.emulator_package_pins().items()}
        self.target={"pcsx2":games.emulator_package_pins()["pcsx2"]}
        self.snapshot_dir=self.root/"state/isolde/runtime-transactions/update-fixture"
        self.snapshot_dir.mkdir(parents=True)
        self.snapshot={"directory":str(self.snapshot_dir),"document":{"schemaVersion":1,
            "before":{"pcsx2":self.before["pcsx2"]["version"]},"packages":[]}}

    def test_update_applies_exact_plan_and_keeps_old_bundle_until_commit(self):
        checkpoint=self.root/"checkpoint.json"
        archive=self.root/"bundle/archives/pcsx2.deb"
        archive.parent.mkdir(parents=True);archive.write_bytes(b"fixture")
        document={"packages":[{"package":"pcsx2","file":"pcsx2.deb"}]}
        (self.root/"bundle/bundle.json").write_text("{}")
        installed={name:dict(state) for name,state in self.before.items()}
        installed["pcsx2"]["version"]=self.target["pcsx2"]
        with patch.object(games,"create_rollback_snapshot",return_value=self.snapshot),\
                patch.object(games,"verify_apt_install_plan") as plan,\
                patch.object(games,"package_status",return_value=installed):
            result=games.apply_runtime_update(self.root/"bundle",document,self.before,self.target,
                lambda *args,**kwargs:subprocess.CompletedProcess(args,0,"",""),
                checkpoint,False)
        self.assertTrue(result["ready"])
        self.assertEqual(result["updated"],["pcsx2"])
        plan.assert_called_once_with([str(archive)],self.target,unittest.mock.ANY,self.before)
        self.assertFalse(checkpoint.exists())
        self.assertFalse(self.snapshot_dir.exists())

    def test_failed_update_restores_snapshot_and_clears_checkpoint(self):
        checkpoint=self.root/"checkpoint.json"
        bundle=self.root/"bundle";(bundle/"archives").mkdir(parents=True)
        archive=bundle/"archives/pcsx2.deb";archive.write_bytes(b"fixture")
        document={"packages":[{"package":"pcsx2","file":"pcsx2.deb"}]}
        commands=[]
        def runner(*args,**kwargs):
            commands.append(args)
            raise subprocess.CalledProcessError(1,args)
        with patch.object(games,"create_rollback_snapshot",return_value=self.snapshot),\
                patch.object(games,"verify_apt_install_plan"),\
                patch.object(games,"restore_runtime_snapshot") as restore:
            with self.assertRaisesRegex(RuntimeError,"prior pinned package versions were restored"):
                games.apply_runtime_update(bundle,document,self.before,self.target,runner,checkpoint,False)
        restore.assert_called_once_with(self.snapshot,runner)
        self.assertFalse(checkpoint.exists())
        self.assertFalse(self.snapshot_dir.exists())
        install=commands[0]
        self.assertIn("--no-remove",install)
        self.assertIn("--allow-downgrades",install)
        self.assertNotIn("--no-upgrade",install)

    def test_interrupted_update_uses_signed_snapshot_recovery_path(self):
        checkpoint=self.root/"checkpoint.json"
        bundle=self.root/"bundle"
        manifest=self.root/"game-runtimes.json"
        manifest.write_text("{}")
        transaction={"schemaVersion":2,"phase":"applying","bundle":str(bundle),
            "before":self.before,"requested":self.target,"rollbackSnapshot":self.snapshot,"createdAt":1}
        checkpoint.write_text(json.dumps(transaction))
        with patch.object(games,"runtime_manifest",return_value=(manifest,{})),\
                patch.object(games,"validate_bundle",return_value={}),\
                patch.object(games,"authenticate_bundle"),\
                patch.object(games,"restore_runtime_snapshot") as restore,\
                patch.object(games,"package_status",return_value=self.before):
            self.assertTrue(games.recover_package_transaction(checkpoint))
        restore.assert_called_once_with(self.snapshot,games.run)
        self.assertFalse(checkpoint.exists())
        self.assertFalse(self.snapshot_dir.exists())

    def test_update_solver_plan_rejects_dependency_changes(self):
        expected={"pcsx2":games.emulator_package_pins()["pcsx2"]}
        line="Inst pcsx2 [2.5.0] ("+expected["pcsx2"]+" Ubuntu:26.04 [amd64])\n"
        runner=lambda *args,**kwargs:subprocess.CompletedProcess(args,0,line,"")
        games.verify_apt_install_plan(["/bundle/pcsx2.deb"],expected,runner)
        with self.assertRaisesRegex(RuntimeError,"changed since the update snapshot"):
            games.verify_apt_install_plan(["/bundle/pcsx2.deb"],expected,runner,
                {"pcsx2":{"status":"install ok installed","version":"2.4.0"}})
        extra=line+"Inst libnew-dependency (1.0 Ubuntu:26.04 [amd64])\n"
        runner=lambda *args,**kwargs:subprocess.CompletedProcess(args,0,extra,"")
        with self.assertRaisesRegex(RuntimeError,"undeclared package"):
            games.verify_apt_install_plan(["/bundle/pcsx2.deb"],expected,runner)

    def test_offline_update_snapshot_uses_prior_signed_bundle_archives(self):
        import hashlib
        cache=self.root/"cache/isolde/game-runtime-bundles"
        bundle=cache/"ubuntu-26.04-amd64-prior"
        archives=bundle/"archives";archives.mkdir(parents=True)
        old_pins=dict(games.emulator_package_pins())
        old_pins["pcsx2"]="2.5.0"
        entries=[]
        for name,version in old_pins.items():
            filename=name+"_"+version+"_amd64.deb"
            payload=("signed prior "+name+" "+version).encode()
            (archives/filename).write_bytes(payload)
            entries.append({"package":name,"version":version,"architecture":"amd64","file":filename,
                "size":len(payload),"sha256":hashlib.sha256(payload).hexdigest()})
        manifest_path,_=games.runtime_manifest()
        document={"schemaVersion":1,"target":{"os":"ubuntu","version":"26.04","architecture":"amd64"},
            "runtimeManifestSha256":hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
            "runtimePackages":old_pins,"packages":entries}
        (bundle/"bundle.json").write_text(json.dumps(document))
        hashes={entry["file"]:entry for entry in entries}
        def metadata(path,runner=games.run):
            entry=hashes[Path(path).name]
            actual=hashlib.sha256(Path(path).read_bytes()).hexdigest()
            return dict(entry,size=Path(path).stat().st_size,sha256=actual)
        with patch.dict(os.environ,{"XDG_CACHE_HOME":str(self.root/"cache"),
                                  "XDG_STATE_HOME":str(self.root/"state")}),\
                patch.object(games,"package_metadata",side_effect=metadata):
            games.sign_bundle(bundle)
            before={"pcsx2":{"status":"install ok installed","version":"2.5.0"}}
            snapshot=games.create_rollback_snapshot(before,{"pcsx2":games.emulator_package_pins()["pcsx2"]},
                runner=lambda *args,**kwargs:self.fail("offline rollback snapshot must not use apt"),
                offline=True)
            root,restored=games.validate_rollback_snapshot(snapshot,runner=lambda *a,**k:None)
            self.assertEqual(restored["before"],{"pcsx2":"2.5.0"})
            archive=root/"archives"/restored["packages"][0]["file"]
            archive.write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError,"does not match the initial installed version"):
                games.validate_rollback_snapshot(snapshot,runner=lambda *a,**k:None)


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
        expanded=adapter.expanded_adapter()
        for identity in self.runtime_definitions:
            runtime,target=self.runtime(identity,self.runtime_definitions[identity]["formats"][0])
            if runtime["firmwareRequired"]:
                with patch.dict(os.environ,{"ISOLDE_NATIVE_NAMESPACE":"1"}),patch.object(adapter,"expanded_adapter",return_value=expanded):
                    with self.assertRaisesRegex(ValueError,"[Ff]irmware.*required"):adapter.command(runtime,target,self.home)
                continue
            with patch.object(adapter,"dolphin_controller",return_value="Xbox Controller"),\
                    patch.dict(os.environ,{"ISOLDE_NATIVE_NAMESPACE":"1"}),\
                    patch.object(adapter,"expanded_adapter",return_value=expanded),\
                    patch.object(expanded,"gamepad",return_value={"name":"Xbox Controller","guid":"a"*32}):
                argv=adapter.command(runtime,target,self.home)
            self.assertEqual(argv[0],str(self.binary))
            self.assertEqual(sum(value in (target,"--target="+target) for value in argv),1)
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

    def test_controller_presets_are_typed_and_dolphin_generates_wii_mapping(self):
        runtime,target=self.runtime("dolphin","iso")
        adapter.atomic(self.home/".config/isolde-emulator/settings.json",json.dumps({
            "schemaVersion":1,"video":"automatic","controller":"xbox-wii-classic","firmware":""}).encode())
        with patch.object(adapter,"dolphin_controller",return_value="Xbox Controller"):
            argv=adapter.command(runtime,target,self.home)
        self.assertIn(target,argv)
        gamecube=(self.home/".local/share/dolphin-emu/Config/GCPadNew.ini").read_text()
        wii=(self.home/".local/share/dolphin-emu/Config/WiimoteNew.ini").read_text()
        self.assertIn("SDL/0/Xbox Controller",gamecube)
        self.assertIn("Extension = Classic",wii)
        runtime,target=self.runtime("retroarch-nes","nes")
        with self.assertRaisesRegex(ValueError,"Dolphin only"):adapter.command(runtime,target,self.home)

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

    def test_ds_and_virtual_boy_are_pinned_catalog_routes(self):
        root=Path(__file__).parents[1]
        catalog=json.loads((root/"config/catalog.json").read_text())
        expected={
            "retroarch-ds":("Nintendo DS",["nds"],"libretro-desmume",
                "0.9.11+git20160819+dfsg1-2build2","desmume_libretro.so"),
            "retroarch-vb":("Virtual Boy",["vb","vboy"],"libretro-beetle-vb",
                "0.9.36.1+git20160623-2build2","mednafen_vb_libretro.so"),
        }
        for identity,(platform,formats,package,version,core) in expected.items():
            with self.subTest(identity=identity):
                route=self.runtime_definitions[identity]
                self.assertEqual(route["platforms"],[platform])
                self.assertEqual(route["formats"],formats)
                self.assertEqual(route["packages"],["retroarch",package])
                self.assertEqual(games.PACKAGES[package],(version,route["core"]))
                self.assertEqual(Path(route["core"]).name,core)
                self.assertFalse(route["firmwareRequired"])
                manifest_path="packages/"+identity+"/manifest.json"
                self.assertIn(manifest_path,catalog["packages"])
                app=json.loads((root/"config"/manifest_path).read_text())["app"]
                self.assertEqual(app["emulatorRuntime"],identity)
                self.assertEqual(app["gameFormats"],formats)
                runtime,target=self.runtime(identity,formats[0])
                argv=adapter.command(runtime,target,self.home)
                self.assertEqual(argv[-3:],["--libretro",runtime["core"],target])
                Path(target).rename(Path(target).with_suffix(".bad"))
                with self.assertRaisesRegex(ValueError,"format"):
                    adapter.command(runtime,str(Path(target).with_suffix(".bad")),self.home)
        coverage=json.loads((root/"config/game-runtimes.json").read_text())["generationCoverage"]
        self.assertIn("Nintendo DSi",coverage["notIntegrated"])
        self.assertNotIn("Nintendo DS",coverage["notIntegrated"])

    def test_playstation_route_binds_chd_and_firmware(self):
        root=Path(__file__).parents[1]
        route=self.runtime_definitions["retroarch-psx"]
        self.assertEqual(route["platforms"],["PlayStation"])
        self.assertEqual(route["formats"],["chd"])
        self.assertTrue(route["firmwareRequired"])
        self.assertEqual(games.PACKAGES["libretro-beetle-psx"],
            ("0.9.38.6+git20260227-1","/usr/lib/x86_64-linux-gnu/libretro/mednafen_psx_libretro.so"))
        cpp=(root/"src/games/GameEmulators.cpp").read_text()
        for digest in adapter.PSX_FIRMWARE:
            self.assertIn(digest,cpp)
        manifest=json.loads((root/"config/packages/retroarch-psx/manifest.json").read_text())
        self.assertEqual(manifest["app"]["gameFormats"],["chd"])
        self.assertEqual(manifest["app"]["emulatorRuntime"],"retroarch-psx")
        runtime,target=self.runtime("retroarch-psx","chd")
        with self.assertRaisesRegex(ValueError,"firmware is required"):
            adapter.command(runtime,target,self.home)
        import hashlib
        data=b"synthetic firmware"+bytes(512*1024-len(b"synthetic firmware"))
        digest=hashlib.sha256(data).hexdigest()
        name=digest+".bin"
        adapter.atomic(self.home/"firmware"/name,data)
        adapter.atomic(self.home/".config/isolde-emulator/settings.json",json.dumps({
            "schemaVersion":1,"video":"automatic","controller":"xbox","firmware":name}).encode())
        with patch.dict(adapter.PSX_FIRMWARE,{hashlib.sha1(data).hexdigest():"scph5501.bin"}):
            argv=adapter.command(runtime,target,self.home)
            self.assertEqual(argv[-3:],["--libretro",runtime["core"],target])
            system=self.home/".config/isolde-emulator/generated"/("psx-"+digest)
            self.assertEqual((system/"scph5501.bin").read_bytes(),data)
            self.assertIn('system_directory = '+json.dumps(str(system)),
                (self.home/".config/isolde-emulator/generated/retroarch.cfg").read_text())
            config=(self.home/".config/isolde-emulator/generated/retroarch.cfg").read_text()
            self.assertIn('auto_overrides_enable = "false"',config)
            self.assertIn('game_specific_options = "false"',config)
            (system/"scph5502.bin").write_bytes(b"unexpected")
            with self.assertRaisesRegex(ValueError,"unexpected files"):
                adapter.command(runtime,target,self.home)
            (system/"scph5502.bin").unlink()
            (self.home/"firmware"/name).write_bytes(bytes(len(data)))
            with self.assertRaisesRegex(ValueError,"changed or is invalid"):
                adapter.command(runtime,target,self.home)

    def test_ds_controller_options_are_private_and_recreated(self):
        runtime,target=self.runtime("retroarch-ds","nds")
        (self.home/"saves").mkdir();save=self.home/"saves/game.dsv";save.write_bytes(b"save")
        adapter.command(runtime,target,self.home)
        base=self.home/".config/isolde-emulator/generated"
        config=(base/"retroarch.cfg").read_text()
        core=base/"desmume-options.cfg"
        self.assertIn('core_options_path = '+json.dumps(str(core)),config)
        self.assertIn('game_specific_options = "false"',config)
        self.assertIn('global_core_options = "true"',config)
        for value in ('desmume_screens_layout = "top/bottom"',
                      'desmume_pointer_device = "r-stick"',
                      'desmume_pointer_mouse = "disable"',
                      'desmume_mic_mode = "internal"'):
            self.assertIn(value,core.read_text())
        core.write_text('desmume_pointer_device = "none"\n')
        adapter.command(runtime,target,self.home)
        self.assertIn('desmume_pointer_device = "r-stick"',core.read_text())
        self.assertEqual(save.read_bytes(),b"save")

    def test_manifest_matrix_is_explicit_and_runtime_packages_are_bound(self):
        manifest=Path(__file__).parents[1]/"config/game-runtimes.json"
        document=json.loads(manifest.read_text())
        self.assertEqual(document["target"]["architectures"],["amd64"])
        self.assertIn("Original Xbox",document["generationCoverage"]["integratedPlatforms"])
        self.assertEqual(set(document["generationCoverage"]["integratedPlatforms"]),
            {platform for runtime in document["runtimes"] for platform in runtime["platforms"]})
        declared={name for runtime in document["runtimes"] for name in runtime["packages"]}
        self.assertEqual(declared,set(games.emulator_package_pins()))
        for runtime in document["runtimes"]:
            self.assertTrue(runtime["packages"])
            self.assertTrue(runtime["formats"])

    def test_manifest_rejects_overlap_and_unknown_route_fields(self):
        manifest=Path(__file__).parents[1]/"config/game-runtimes.json"
        document=json.loads(manifest.read_text())
        document["generationCoverage"]["notIntegrated"].append("PSP")
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"manifest.json";path.write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError,"generation matrix"):adapter.definitions(path)
        document=json.loads(manifest.read_text());document["runtimes"][0]["command"]="untrusted"
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"manifest.json";path.write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError,"capability entry"):adapter.definitions(path)

    def test_manifest_requires_engine_specific_pinned_core_packages(self):
        manifest=Path(__file__).parents[1]/"config/game-runtimes.json"
        original=json.loads(manifest.read_text())
        mutations=(
            lambda doc: doc["runtimes"][3].pop("core"),
            lambda doc: doc["runtimes"][3]["packages"].remove("libretro-nestopia"),
            lambda doc: doc["runtimes"][0].update(core="/usr/lib/untrusted.so"),
            lambda doc: doc["runtimes"][0]["packages"].append("unverified-emulator"),
        )
        for mutate in mutations:
            document=json.loads(json.dumps(original));mutate(document)
            with self.subTest(document=document["runtimes"][0]), tempfile.TemporaryDirectory() as directory:
                path=Path(directory)/"manifest.json";path.write_text(json.dumps(document))
                with self.assertRaisesRegex(ValueError,"route|core"):
                    adapter.definitions(path)

    def test_provisioned_runtime_fingerprint_binds_manifest_executable_and_core(self):
        source=Path(__file__).parents[1]/"config/game-runtimes.json"
        with tempfile.TemporaryDirectory() as directory:
            missing=Path(directory)/"missing-runtime"
            definitions={identity:dict(runtime,binary=str(missing)) for identity,runtime in self.runtime_definitions.items()}
            with patch.object(games,"runtime_manifest",return_value=(source,definitions)):
                manifest_hash,routes=games.inspect_runtimes({name:{"ready":True} for name in games.PACKAGES})
        self.assertRegex(manifest_hash,r"^[a-f0-9]{64}$")
        self.assertEqual(set(routes),set(self.runtime_definitions))
        route=routes["retroarch-gba"]
        self.assertEqual(route["packages"],["retroarch","libretro-mgba"])
        self.assertEqual(route["ready"],False)
        self.assertIsNone(route["binarySha256"])
        self.assertIsNone(route["coreSha256"])

    def test_provisioned_runtime_fingerprint_detects_binary_or_core_change(self):
        source=Path(__file__).parents[1]/"config/game-runtimes.json"
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);binary=root/"runtime";binary.write_text("verified runtime");binary.chmod(0o700)
            core=root/"core.so";core.write_text("verified core")
            routes={identity:dict(runtime,binary=str(binary)) for identity,runtime in self.runtime_definitions.items()}
            for runtime in routes.values():
                if "core" in runtime:runtime["core"]=str(core)
            packages={name:{"expectedVersion":value[0],"installedVersion":value[0],"ready":True,
                "executable":value[1],"sha256":"a"*64} for name,value in games.PACKAGES.items()}
            with patch.object(games,"runtime_manifest",return_value=(source,routes)),\
                    patch.object(games,"upstream_fingerprint",return_value="b"*64):
                manifest_hash,receipt=games.inspect_runtimes(packages)
                self.assertTrue(all(item["ready"] for item in receipt.values()))
                report={"schemaVersion":2,"manifestSha256":manifest_hash,"runtimes":receipt,"packages":packages}
                self.assertTrue(games.verify_runtime_receipt(report))
                core.write_text("changed core")
                self.assertFalse(games.verify_runtime_receipt(report))
                core.write_text("verified core")
                binary.write_text("changed runtime")
                self.assertFalse(games.verify_runtime_receipt(report))


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

    @unittest.skipUnless(os.name=="posix","Bounded PSP build cancellation contract")
    def test_builder_cancel_stops_fetch_or_compile_process_group(self):
        spec=importlib.util.spec_from_file_location("psp_timeout",Path(__file__).parents[1]/"tools/build-ppsspp.py")
        builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
        script="""
import importlib.util,os,signal,sys,threading
base=sys.argv[1]
game_spec=importlib.util.spec_from_file_location('games',base+'/tools/provision-games.py')
games=importlib.util.module_from_spec(game_spec);game_spec.loader.exec_module(games)
build_spec=importlib.util.spec_from_file_location('builder',base+'/tools/build-ppsspp.py')
builder=importlib.util.module_from_spec(build_spec);build_spec.loader.exec_module(builder)
games.install_cancellation_handlers();builder._cancel_check=games.check_cancelled
threading.Timer(.05,lambda:os.kill(os.getpid(),signal.SIGTERM)).start()
try:builder.run(sys.executable,'-c','import time; time.sleep(30)',timeout=60)
except games.ProvisioningCancelled:print('build-stopped')
"""
        result=subprocess.run([sys.executable,"-c",script,str(Path(__file__).parents[1])],
            capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(result.stdout.strip(),"build-stopped")

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


@unittest.skipUnless(os.name=="posix","Expanded routes target the Linux appliance")
class ExpandedRuntimeTests(unittest.TestCase):
    def setUp(self):
        import hashlib
        self.hashlib=hashlib
        self.directory=tempfile.TemporaryDirectory();self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.home=self.root/"profile-home";self.home.mkdir()
        self.binary=self.root/"inert-runtime";self.binary.write_text("external runtime fixture");self.binary.chmod(0o700)
        self.routes=adapter.definitions(Path(__file__).parents[1]/"config/game-runtimes.json")
        self.expanded=adapter.expanded_adapter()
        self.environment=patch.dict(os.environ,{"ISOLDE_NATIVE_NAMESPACE":"1"});self.environment.start();self.addCleanup(self.environment.stop)
        self.loader=patch.object(adapter,"expanded_adapter",return_value=self.expanded);self.loader.start();self.addCleanup(self.loader.stop)
        self.pad=patch.object(self.expanded,"gamepad",return_value={"name":"Xbox & test pad","guid":"a"*32})
        self.pad.start();self.addCleanup(self.pad.stop)

    def route(self,identity,suffix=None):
        route=dict(self.routes[identity],binary=str(self.binary))
        target=self.root/("user game."+(suffix or route["formats"][0]));target.write_bytes(b"owned game fixture")
        return route,target

    def asset(self,content):
        name=self.hashlib.sha256(content).hexdigest()+".bin"
        adapter.atomic(self.home/"firmware"/name,content)
        adapter.atomic(self.home/".config/isolde-emulator/settings.json",json.dumps({
            "schemaVersion":1,"video":"automatic","controller":"xbox","firmware":name}).encode())
        return self.home/"firmware"/name

    def zip_bytes(self,entries):
        import io,zipfile
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,"w",compression=zipfile.ZIP_DEFLATED) as archive:
            for name,content in entries:archive.writestr(name,content)
        return stream.getvalue()

    def firmware_boundary(self,arguments,private,timeout=300):
        # Only the upstream firmware decoder is substituted. Production staging,
        # receipts, hash revalidation, generated config and save paths execute.
        if "--installfw" in arguments:
            folder=private/".config/rpcs3/dev_flash/sys/external"
        elif "--firmware" in arguments:
            folder=private/".local/share/Vita3K/Vita3K/vs0/sys/external"
        else:
            folder=private/".config/rpcs3/dev_hdd0/game/TEST00001/USRDIR"
        folder.mkdir(parents=True)
        name="EBOOT.BIN" if "--installpkg" in arguments else "liblv2.sprx" if "--installfw" in arguments else "libhttp.suprx"
        (folder/name).write_bytes(b"decoded external fixture")

    def test_six_routes_bind_locked_artifacts_and_catalog_entries(self):
        builder=games.artifact_builder();_,locks=builder.locks()
        catalog=json.loads((Path(__file__).parents[1]/"config/catalog.json").read_text())
        self.assertEqual(set(locks),{"rpcs3","shadps4","vita3k","xemu","xenia","cemu"})
        for identity,row in locks.items():
            self.assertEqual(self.routes[identity]["packages"],[row["package"]])
            self.assertEqual(games.PACKAGES[row["package"]],(row["version"],"/usr/bin/isolde-"+identity))
            self.assertIn("packages/"+identity+"/manifest.json",catalog["packages"])
            self.assertRegex(row["sha256"],r"^[a-f0-9]{64}$")

    def test_ps3_firmware_preparation_is_private_reusable_and_tamper_checked(self):
        route,target=self.route("rpcs3");asset=self.asset(b"SCEUF\0\0\0"+b"bounded synthetic PUP")
        with patch.object(self.expanded.Adapter,"run",autospec=True,
                side_effect=lambda _self,*args,**kwargs:self.firmware_boundary(*args,**kwargs)) as decoder:
            argv=adapter.command(route,str(target),self.home)
            self.assertIn("--no-gui",argv);self.assertIn("--fullscreen",argv)
            self.assertNotIn("--no-config",argv);decoder.assert_called_once()
            vfs=json.loads((self.home/".config/rpcs3/vfs.yml").read_text())
            flash=Path(vfs["/dev_flash/"])
            self.assertTrue(flash.is_relative_to(self.home));self.assertTrue(flash.is_dir())
            before=asset.read_bytes();save=self.home/".config/rpcs3/dev_hdd0/home/00000001/savedata/save"
            save.parent.mkdir(parents=True);save.write_bytes(b"existing save")
            adapter.command(route,str(target),self.home);decoder.assert_called_once()
            self.assertEqual(save.read_bytes(),b"existing save");self.assertEqual(asset.read_bytes(),before)
            (flash/"sys/external/liblv2.sprx").write_bytes(b"changed firmware")
            with self.assertRaisesRegex(ValueError,"changed"):adapter.command(route,str(target),self.home)

    def test_ps3_package_installs_at_runtime_boundary_without_modifying_source(self):
        route,target=self.route("rpcs3","pkg");self.asset(b"SCEUF\0\0\0"+b"synthetic PUP")
        with patch.object(self.expanded.Adapter,"run",autospec=True,
                side_effect=lambda _self,*args,**kwargs:self.firmware_boundary(*args,**kwargs)) as decoder:
            argv=adapter.command(route,str(target),self.home)
            self.assertEqual(decoder.call_count,2)
            self.assertTrue(argv[-1].endswith("/TEST00001/USRDIR/EBOOT.BIN"))
            self.assertTrue(Path(argv[-1]).is_relative_to(self.home))
            adapter.command(route,str(target),self.home);self.assertEqual(decoder.call_count,2)
            self.assertEqual(target.read_bytes(),b"owned game fixture")

    def test_vita_archive_and_firmware_use_private_storage_and_sdl_frontend(self):
        route,target=self.route("vita3k")
        target.write_bytes(self.zip_bytes([("eboot.bin",b"homebrew ELF"),("sce_sys/param.sfo",b"metadata")]))
        self.asset(b"SCEUF\0\0\0"+b"synthetic Vita PUP")
        with patch.object(self.expanded.Adapter,"run",autospec=True,
                side_effect=lambda _self,*args,**kwargs:self.firmware_boundary(*args,**kwargs)):
            argv=adapter.command(route,str(target),self.home)
        self.assertEqual(argv[1:3],["--frontend","sdl"])
        config=json.loads((self.home/".config/isolde-emulator/generated/vita.yml").read_text())
        root=Path(config["pref-path"]);self.assertTrue(root.is_relative_to(self.home))
        self.assertTrue((root/"vs0").is_symlink());self.assertTrue((root/"vs0").resolve().is_relative_to(self.home))
        self.assertFalse(config["check-for-updates"]);self.assertTrue(config["initial-setup"])
        target.write_bytes(self.zip_bytes([("../escape",b"bad")]))
        with self.assertRaisesRegex(ValueError,"unsafe"):adapter.command(route,str(target),self.home)
        self.assertFalse((self.root/"escape").exists())
        target.write_bytes(b"PK truncated VPK")
        with self.assertRaisesRegex(ValueError,"archive is invalid"):adapter.command(route,str(target),self.home)

    def test_xbox_qcow_save_and_eeprom_survive_firmware_reselection(self):
        import struct
        route,target=self.route("xemu")
        disc=bytearray(34*2048);disc[32*2048:32*2048+20]=b"MICROSOFT*XBOX*MEDIA";target.write_bytes(disc)
        disk=bytearray(4096);disk[:4]=b"QFI\xfb";struct.pack_into(">I",disk,4,3);struct.pack_into(">I",disk,20,16);struct.pack_into(">Q",disk,24,8*1024**3)
        mcpx=b"M"*512
        archive=self.zip_bytes([("mcpx.bin",mcpx),("flash.bin",bytes(256*1024)),("hdd.qcow2",disk),("eeprom.bin",b"E"*256)])
        self.asset(archive)
        with patch.object(self.expanded,"MCPX_MD5",self.hashlib.md5(mcpx).hexdigest()):
            argv=adapter.command(route,str(target),self.home)
            self.assertIn("-full-screen",argv);self.assertEqual(argv[-2:], ["-dvd_path",str(target)])
            save=self.home/"saves/xemu/hdd.qcow2";save.write_bytes(bytes(disk)+b"saved game")
            eeprom=self.home/"saves/xemu/eeprom.bin";eeprom.write_bytes(b"P"*256)
            adapter.command(route,str(target),self.home)
            self.assertEqual(save.read_bytes(),bytes(disk)+b"saved game");self.assertEqual(eeprom.read_bytes(),b"P"*256)
            bad=bytearray(disk);struct.pack_into(">Q",bad,8,128)
            save.write_bytes(bad)
            with self.assertRaisesRegex(ValueError,"backing"):adapter.command(route,str(target),self.home)

    def test_shad_xenia_and_cemu_use_explicit_private_configuration(self):
        import xml.etree.ElementTree as ET
        for identity in ("shadps4","xenia","cemu"):
            route,target=self.route(identity);argv=adapter.command(route,str(target),self.home)
            if identity=="shadps4":
                self.assertEqual(argv[1:3],["--fullscreen","true"])
                self.assertTrue(Path(argv[4]).is_relative_to(self.home))
            elif identity=="xenia":
                self.assertIn("--target="+str(target),argv)
                self.assertIn("--content_root="+str(self.home/"saves/xenia"),argv)
            else:
                root=ET.fromstring((self.home/".config/Cemu/controllerProfiles/controller0.xml").read_bytes())
                self.assertEqual(root.findtext("type"),"Wii U GamePad")
                self.assertEqual(root.findtext("controller/uuid"),"0_"+"a"*32)
                self.assertEqual(root.findtext("controller/display_name"),"Xbox & test pad")
                self.assertEqual(root.findtext("toggle_display"),"1")
        adapter.atomic(self.home/".config/isolde-emulator/settings.json",json.dumps({
            "schemaVersion":1,"video":"software","controller":"xbox","firmware":""}).encode())
        with self.assertRaisesRegex(ValueError,"no software"):adapter.command(route,str(target),self.home)

    def test_backend_fingerprint_rejects_changed_library_and_unlisted_files(self):
        root=self.root/"payload";root.mkdir();binary=root/"runtime";binary.write_bytes(b"binary")
        library=root/"library.so";library.write_bytes(b"library")
        manifest=self.root/"payload.json"
        manifest.write_text(json.dumps([[p.name,"file",self.hashlib.sha256(p.read_bytes()).hexdigest()] for p in (binary,library)]))
        self.assertTrue(games.upstream_fingerprint("xenia",root,manifest))
        library.write_bytes(b"mutated");self.assertIsNone(games.upstream_fingerprint("xenia",root,manifest))
        library.write_bytes(b"library");(root/"extra").write_bytes(b"unlisted")
        self.assertIsNone(games.upstream_fingerprint("xenia",root,manifest))


if __name__=="__main__":unittest.main()
