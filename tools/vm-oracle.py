#!/usr/bin/env python3
"""Bounded Linux runner: tests, explicit guest provisioning and isolated acceptance."""
import argparse
import contextlib
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import selectors
import shutil
import signal
import subprocess
import sys
import tarfile
import time
import xml.etree.ElementTree as ET

SUITES = {"all": None, "component_contracts": "isolde_component_tests",
          "profile_browser_isolation": "isolde_profile_browser_tests",
          "home_component_navigation": "isolde_home_navigation_tests",
          "studio_navigation": "isolde_home_navigation_tests",
          "kodi_media_client": "isolde_kodi_media_tests",
          "desktop_mode": "isolde_desktop_mode_tests",
          "vpn": "isolde_vpn_tests",
          "studio_quick": "isolde_studio_quick_tests",
          "studio_selection": "isolde_studio_quick_tests",
          "studio_gestures": "isolde_studio_quick_tests",
          "studio_grid": "isolde_studio_quick_tests",
          "studio_destinations": "isolde_studio_quick_tests",
          "studio_ordered": "isolde_studio_quick_tests",
          "studio_policies": "isolde_studio_quick_tests",
          "studio_overlays": "isolde_studio_quick_tests",
          "studio_unit": "isolde_component_tests",
          "studio_behavior": "isolde_home_navigation_tests",
          "studio": ("isolde_studio_quick_tests", "isolde_component_tests", "isolde_home_navigation_tests")}


def suite_selection(suite):
    """One allowlisted selection authority for build targets and CTest names."""
    targets = SUITES[suite]
    targets = list(targets) if isinstance(targets, tuple) else [targets] if targets else []
    pattern = "^(studio_quick|studio_selection|studio_gestures|studio_grid|studio_grid_controls|studio_destinations|studio_ordered|studio_policies|studio_overlays|studio_unit|studio_behavior)$" if suite == "studio" else "^" + re.escape(suite) + "$"
    if suite == "studio_grid":
        pattern = "^(studio_grid|studio_grid_controls)$"
    if suite == "vpn":
        pattern = "^(vpn_setup|vpn_launch)$"
    if suite == "home_component_navigation":
        pattern = "^(home_component_navigation|studio_navigation)$"
    return targets, pattern
CASES = {"studio": ("home-components-acceptance.py", ["--studio", "--inspect"]),
         "desktop": ("home-components-acceptance.py", ["--studio", "--desktop", "--inspect"]),
         "lounge": ("home-reference-acceptance.py", ["--skin-from-settings", "--inspect"]),
         "pastel": ("home-reference-acceptance.py", ["--skin-from-settings", "--inspect"]),
         "repairs": ("home-reference-acceptance.py", ["--existing-profile", "8bdf6077-4613-4b52-9b20-d1ec7931dfcf", "--inspect"]),
         "apps": ("home-reference-acceptance.py", ["--existing-profile", "8bdf6077-4613-4b52-9b20-d1ec7931dfcf", "--verify-default-apps", "--inspect"]),
         "clients": ("home-reference-acceptance.py", ["--existing-profile", "8bdf6077-4613-4b52-9b20-d1ec7931dfcf", "--verify-default-apps", "--skip-steam", "--inspect"]),
         "pages": ("tv-library-acceptance.py", ["--pages", "--inspect"]),
         "vpn": ("vpn-acceptance.py", []),
         "shell": ("shell-acceptance.py", []),
         "playlists": ("tv-library-acceptance.py", ["--playlists", "--inspect"]),
         "android": ("android-acceptance.py", [])}


def visual_result_passed(case, result):
    if case == "android":
        return (all(result.get(k) is True for k in ("passed", "compatibilityProbeOnly", "userdataBindingVerified", "packageInstalled", "launchRequested", "ownedSessionStopped"))
                and all(result.get(k) is False for k in ("isoldeIntegrationVerified", "playbackVerified", "remoteVerified")))
    if case == "playlists":
        return (all(result.get(key) is True for key in ("passed","real_kodi_saved_playlist","playlist_creation_ui_verified","playlist_cancel_no_write","play_pause_stop","media_unchanged"))
                and result.get("normal_profile_used") is False and result.get("picker_ui_verified") is False and result.get("episodes")==3)
    if case == "shell":
        return (result.get("passed") is True and result.get("reviewSequenceCaptured")==["shellnavigation","shellgallery","shellstore","shellreturn","shellambient","shellwake"]
                and result.get("hostUiAutomationUsed") is False and result.get("layoutWrites") is False and result.get("semanticAcceptance")=="not_verified")
    if case == "vpn":
        return (all(result.get(k) is True for k in ("passed", "defaultProvidersInstalled", "uiAuthoredRule", "countryVerified", "nativeClientOpened"))
                and result.get("app")=="cbc-gem" and result.get("country")=="CA"
                and result.get("browserUsedForSetup") is False and result.get("hostUiAutomationUsed") is False)
    if case == "clients":
        return all(result.get(key) is True for key in ("passed", "native_clients", "household_unchanged")) and result.get("steam_client_downloaded") is False and result.get("subscriptionPlaybackVerified") is False
    if case == "apps":
        return all(result.get(key) is True for key in ("passed", "native_clients", "steam_client_downloaded", "household_unchanged")) and result.get("subscriptionPlaybackVerified") is False
    if case == "repairs":
        return all(result.get(key) is True for key in ("passed", "preserving_update", "household_unchanged", "real_compositor_return"))
    if case in ("lounge", "pastel"):
        functional = (result.get("functional_journey_passed") is True
                      if "functional_journey_passed" in result else
                      result.get("component_support") is True and result.get("missing") == [])
        return (functional
                and result.get("reset_verified") is True
                and result.get("settings_skin_round_trip") is True
                and result.get("saved_skin_survives_reset") is True)
    return result.get("passed") is True


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def android_admin_launcher(command, run, title):
    return ("import subprocess,json\nprint("+repr(title+" — enter the VM administrator password only here.")+",flush=True)\n"
        +"p=subprocess.Popen("+repr(["pkexec"]+command)+",stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)\n"
        +"total=0\nwith open("+repr(str(run/"android-admin.log"))+",'w') as log:\n"
        +" for line in p.stdout:\n  print(line[:8192],end='',flush=True)\n  if total<8*1024*1024: log.write(line[:8192]);log.flush();total+=len(line[:8192])\n"
        +"code=p.wait()\nwith open("+repr(str(run/"android-admin-exit.json"))+",'w') as result:json.dump({'exit':code},result)\nraise SystemExit(code)\n")


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def safe_relative(name):
    p = PurePosixPath(name)
    if not name or "\\" in name or p.is_absolute() or any(v in (".", "..") for v in name.split("/")):
        raise RuntimeError("Unsafe snapshot path: " + name)
    return p


def manifest(path):
    entries = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not re.fullmatch(r"[0-9a-f]{64}  .+", line):
            raise RuntimeError("Invalid source hash manifest")
        name = line[66:]
        safe_relative(name)
        if name in entries:
            raise RuntimeError("Duplicate snapshot path: " + name)
        entries[name] = line[:64]
    if not entries:
        raise RuntimeError("Empty source manifest")
    return entries


def verify_tree(root, entries):
    for name, expected in entries.items():
        path = root / name
        if path.is_symlink() or not path.is_file() or digest(path) != expected:
            raise RuntimeError("Source changed or missing: " + name)


def extract_snapshot(run):
    expected = (run / "archive.sha256").read_text().split()[0]
    if digest(run / "source.tar.gz") != expected:
        raise RuntimeError("Archive checksum mismatch; do not build")
    entries = manifest(run / "files.sha256")
    source = run / "source"
    if source.exists():
        raise RuntimeError("Snapshot already extracted; use Status, Visual or Deploy for this run")
    source.mkdir()
    with tarfile.open(run / "source.tar.gz", "r:gz") as archive:
        seen = set()
        for member in archive:
            name = str(safe_relative(member.name))
            if name not in entries or name in seen or not member.isfile():
                raise RuntimeError("Unlisted, duplicate or non-regular archive entry: " + name)
            seen.add(name)
            target = source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(member) as incoming, target.open("wb") as output:
                shutil.copyfileobj(incoming, output)
            target.chmod(member.mode & 0o777)
        if seen != set(entries):
            raise RuntimeError("Archive and manifest disagree")
    verify_tree(source, entries)
    return source, entries


def sync_workspace(source, destination, entries, previous):
    """Keep timestamps on identical files so Ninja can reuse compiled objects."""
    destination.mkdir(parents=True, exist_ok=True)
    base = destination.resolve()
    changed = 0
    for name in set(previous) - set(entries):
        target = destination / safe_relative(name)
        if not target.resolve().is_relative_to(base) or target.is_symlink():
            raise RuntimeError("Unsafe stale workspace file: " + name)
        if target.is_file():
            target.unlink()  # Only paths from our previous verified manifest; never recursive.
    for name, expected in entries.items():
        target = destination / name
        if not target.resolve().is_relative_to(base) or target.is_symlink():
            raise RuntimeError("Unsafe workspace destination: " + name)
        if target.is_file() and digest(target) == expected:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        os.utime(target, None)  # Changed files must be newer than cached objects.
        changed += 1
    verify_tree(destination, entries)
    return changed


def payload_hashes(payload):
    result = {}
    for path in sorted(payload.rglob("*")):
        if path.is_symlink():
            raise RuntimeError("Payload contains a symlink")
        if path.is_file():
            result[str(path.relative_to(payload))] = digest(path)
    if "bin/isolde" not in result:
        raise RuntimeError("Missing staged executable")
    return result


def stop_owned(process):
    if process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=8)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


class Oracle:
    def __init__(self, root, run_id):
        self.root = root
        self.run = root / "runs" / run_id
        self.run.mkdir(parents=True, exist_ok=True)
        self.state = {"runId": run_id, "state": "running", "stage": "starting",
                      "pid": os.getpid(), "started": time.time(), "logs": str(self.run)}
        self.state["driverDigest"] = digest(Path(__file__))
        self.state["processStartTicks"] = Path("/proc/self/stat").read_text().split()[21] if os.name == "posix" else None

    def update(self, **values):
        self.state.update(values, updated=time.time())
        atomic_json(self.run / "status.json", self.state)
        atomic_json(self.root / "latest.json", self.state)

    def stage(self, name, argv, timeout=120, env=None, on_line=None):
        self.update(stage=name)
        print("STAGE " + name, flush=True)
        log = self.run / (name + ".log")
        if log.exists():
            log = self.run / (name + "-" + str(time.time_ns()) + ".log")
        self.update(stageLog=str(log))
        process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   env=env, start_new_session=True, bufsize=0)
        start = last_heartbeat = time.monotonic()
        pending = b""
        tail = ""
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        try:
            with log.open("wb") as output:
                while selector.get_map():
                    now = time.monotonic()
                    if now - start > timeout:
                        raise TimeoutError(f"{name} exceeded {timeout}s; see {log}")
                    if now - last_heartbeat >= 10:
                        self.update(elapsedSeconds=round(now - start), lastOutput=tail[-300:])
                        print(f"PROGRESS {name} {int(now-start)}s {tail[-180:]}", flush=True)
                        last_heartbeat = now
                    for key, _ in selector.select(0.25):
                        data = os.read(key.fd, 65536)
                        if not data:
                            selector.unregister(key.fileobj)
                            continue
                        output.write(data)
                        output.flush()
                        pending += data
                        while b"\n" in pending:
                            line, pending = pending.split(b"\n", 1)
                            tail = line.decode("utf-8", "replace").strip()
                            if on_line:
                                on_line(tail)
                code = process.wait(timeout=5)
                if pending:
                    tail = pending.decode("utf-8", "replace")
                    if on_line:
                        on_line(tail)
            if code:
                lines = log.read_text(errors="replace").splitlines()[-25:]
                print("\n".join(lines), flush=True)
                raise RuntimeError(f"{name} exited {code}; see {log}")
        finally:
            selector.close()
            stop_owned(process)

    def command(self, *argv):
        return subprocess.check_output(argv, text=True, timeout=15).strip()

    def service(self):
        text = self.command("systemctl", "--user", "show", "isolde.service", "-p", "ActiveState",
                            "-p", "SubState", "-p", "NRestarts", "-p", "MainPID")
        return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)

    def links(self):
        prefix = Path.home() / ".local/share/isolde-appliance"
        return {name: str((prefix / name).resolve()) for name in ("current", "previous")}

    def restore(self, before):
        expected = before["ActiveState"] == "active"
        active = self.service()["ActiveState"] == "active"
        if active != expected:
            self.command("systemctl", "--user", "start" if expected else "stop", "isolde.service")
        after = self.service()
        if (after["ActiveState"] == "active") != expected:
            raise RuntimeError("Normal service restoration failed")
        return after

    def check(self):
        for tool in ("cmake", "ninja", "ctest", "python3", "tar"):
            if not shutil.which(tool):
                raise RuntimeError("Required guest tool missing: " + tool)
        free = shutil.disk_usage(self.root).free
        if free < 2 * 1024**3:
            raise RuntimeError("Less than 2 GiB free; stop and review space. No automatic deletion.")
        self.update(hostname=self.command("hostname"), freeGiB=round(free / 1024**3, 2),
                    service=self.service(), releases=self.links())
        environment = dict(os.environ)
        for line in self.command("systemctl", "--user", "show-environment").splitlines():
            key, _, value = line.partition("=")
            if key in {"DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR", "DISPLAY", "XAUTHORITY"}: environment[key] = value
        portal = subprocess.run(["gdbus", "introspect", "--session", "--dest", "org.freedesktop.portal.Desktop", "--object-path", "/org/freedesktop/portal/desktop"], env=environment, capture_output=True, text=True, timeout=15)
        app_metadata = {}
        for app_id in ("spotify", "steam", "freetube"):
            entries = {}
            for root in (Path.home()/".local/share", Path("/usr/share")):
                for desktop_id in (app_id, "isolde-"+app_id):
                    entry = root/"applications"/(desktop_id+".desktop")
                    if entry.is_file() and entry.stat().st_size <= 16384: entries[str(entry)] = entry.read_text()[:16384]
            icons = []
            for pattern in ("icons/hicolor/*/apps/"+app_id+"*", "pixmaps/"+app_id+"*", app_id+"/icons/*"):
                icons.extend(str(p) for p in Path("/usr/share").glob(pattern) if p.is_file())
            app_metadata[app_id] = {"entries": entries, "icons": sorted(icons)[:64]}
        atomic_json(self.run / "desktop-platform.json", {"portalExit": portal.returncode, "appMetadata": app_metadata,
            "globalShortcuts": "org.freedesktop.portal.GlobalShortcuts" in portal.stdout,
            "portalInterfaces": portal.stdout[:65536], "error": portal.stderr[:2048],
            "desktopIdentity": (Path.home()/".local/share/applications/org.isolde.Isolde.desktop").is_file()})
        locations = [Path.home()/"isolde-runs", Path.home(), Path("/var/cache/apt"), Path("/var/lib/systemd/coredump")]
        usage = subprocess.run(["du", "-x", "-B1", "--max-depth=2", *[str(p) for p in locations if p.is_dir()]], capture_output=True, text=True, timeout=30)
        atomic_json(self.run / "guest-disk-usage.json", {"guestHome": str(Path.home()), "usage": usage.stdout[:65536], "errors": usage.stderr[:2048]})
        privileges = subprocess.run(["sudo", "-n", "-l"], capture_output=True, text=True, timeout=10)
        atomic_json(self.run / "guest-privileges.json", {"exit": privileges.returncode, "policy": privileges.stdout[:8192], "error": privileges.stderr[:2048]})
        atomic_json(self.run/"native-admin-tools.json", {name:shutil.which(name) for name in ("pkexec","gnome-terminal","kgx","ptyxis","systemd-run")})
        terminal_help=subprocess.run(["ptyxis","--help"],env=environment,capture_output=True,text=True,timeout=10)
        atomic_json(self.run/"native-terminal-help.json",{"exit":terminal_help.returncode,"output":terminal_help.stdout[:16384],"error":terminal_help.stderr[:2048]})
        native_setup={}
        for filename in ("/usr/lib/isolde-native/bwrap","/etc/apparmor.d/isolde-native"):
            file=Path(filename)
            native_setup[filename]={"exists":file.is_file(),"symlink":file.is_symlink()}
            if file.is_file(): native_setup[filename].update(uid=file.stat().st_uid,mode=oct(file.stat().st_mode & 0o7777),sha256=digest(file))
        if Path("/usr/lib/isolde-native/bwrap").is_file():
            native_setup["ancestors"]=[{"path":str(p),"symlink":p.is_symlink(),"uid":p.stat().st_uid,"mode":oct(p.stat().st_mode & 0o7777)} for p in Path("/usr/lib/isolde-native/bwrap").parents]
            label=subprocess.run(["/usr/lib/isolde-native/bwrap","--ro-bind","/","/","--proc","/proc","--","/usr/bin/cat","/proc/self/attr/current"],capture_output=True,text=True,timeout=10)
            native_setup["loadedLabel"]={"exit":label.returncode,"output":label.stdout[:2048],"error":label.stderr[:2048]}
        atomic_json(self.run/"native-setup-state.json",native_setup)
        native_home = Path.home()/".local/share/Isolde/native-profiles/8bdf6077-4613-4b52-9b20-d1ec7931dfcf/steam/home"
        native_logs = {}
        if native_home.is_dir() and not native_home.is_symlink():
            for directory, children, files in os.walk(native_home, followlinks=False):
                children[:] = [c for c in children if not (Path(directory)/c).is_symlink()]
                if len(Path(directory).relative_to(native_home).parts)>7: children[:]=[]; continue
                for name in files:
                    file = Path(directory)/name
                    if name in {"bootstrap_log.txt", "console-linux.txt", "steam-runtime-check-requirements.log"} and not file.is_symlink() and file.stat().st_size <= 4*1024*1024:
                        native_logs[str(file.relative_to(native_home))] = file.read_text(errors="replace")[-16384:]
                if len(native_logs)>=8: break
        atomic_json(self.run/"native-bootstrap-diagnostics.json", native_logs)
        namespace_policy = {}
        for filename in ("/proc/sys/kernel/unprivileged_userns_clone", "/proc/sys/kernel/apparmor_restrict_unprivileged_userns", "/proc/sys/user/max_user_namespaces", "/etc/apparmor.d/bwrap", "/etc/apparmor.d/bubblewrap", "/etc/apparmor.d/steam", "/proc/self/attr/current"):
            file = Path(filename)
            try: namespace_policy[filename] = file.read_text()[:16384]
            except OSError as error: namespace_policy[filename] = str(error)
        atomic_json(self.run/"native-namespace-policy.json", namespace_policy)
        denials = subprocess.run(["journalctl", "-k", "--since=-20min", "--no-pager", "-g", "userns|apparmor"], capture_output=True, text=True, timeout=15)
        atomic_json(self.run/"native-namespace-denials.json", {"exit": denials.returncode, "output": denials.stdout[-16384:], "error": denials.stderr[:2048]})
        matching_profiles = {}
        for file in sorted(Path("/etc/apparmor.d").glob("*"))[:256]:
            if file.is_file() and not file.is_symlink() and file.stat().st_size<=65536:
                content=file.read_text(errors="replace")
                if "unpriv_bwrap" in content: matching_profiles[file.name]=content
        atomic_json(self.run/"native-bwrap-profile.json", matching_profiles)
        # Read-only Android runtime preflight. Never load modules, install a
        # runtime, change graphics or inspect Android/account data here.
        kernel = os.uname().release
        android = {"kernel": kernel, "machine": os.uname().machine,
                   "tools": {name: shutil.which(name) for name in ("waydroid", "weston", "adb")},
                   "binderDevices": [str(p) for p in (Path("/dev/binder"), Path("/dev/hwbinder"), Path("/dev/vndbinder"), Path("/dev/binderfs")) if p.exists()]}
        config = Path("/boot") / ("config-" + kernel)
        android["kernelConfig"] = [line for line in config.read_text().splitlines() if "ANDROID_BINDER" in line or "MEMFD_CREATE=" in line] if config.is_file() and config.stat().st_size < 1024*1024 else []
        android["binderFilesystemAvailable"] = "binder" in Path("/proc/filesystems").read_text()
        for label, command in (("binderModule", ["modinfo", "binder_linux"]),
                               ("packages", ["apt-cache", "policy", "waydroid", "weston"]),
                               ("runtimeService", ["systemctl", "show", "waydroid-container.service", "-p", "LoadState", "-p", "ActiveState"]),
                               ("graphics", ["lspci", "-nn"])):
            if not shutil.which(command[0]):
                android[label] = {"available": False}
                continue
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            output = result.stdout
            if label == "graphics": output = "\n".join(line for line in output.splitlines() if any(word in line.lower() for word in ("vga", "display", "3d controller")))
            android[label] = {"exit": result.returncode, "output": output[:8192], "error": result.stderr[:2048]}
        android["session"] = {key: value for line in self.command("systemctl", "--user", "show-environment").splitlines()
                              for key, separator, value in [line.partition("=")] if separator and key in {"XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "WAYLAND_DISPLAY", "DISPLAY"}}
        atomic_json(self.run/"android-capabilities.json", android)

    def test(self, suite):
        self.check()
        service_before, releases_before = self.state["service"], self.state["releases"]
        source, entries = extract_snapshot(self.run)
        self.stage("oracle-self-test", ["python3", str(source / "tests/test_vm_oracle.py")], 30)
        work = self.root / "work"
        old = json.loads((work / "manifest.json").read_text()) if (work / "manifest.json").exists() else {}
        changed = sync_workspace(source, work / "source", entries, old)
        atomic_json(work / "manifest.json", entries)
        self.update(changedFiles=changed, suite=suite, sourceDigest=digest(self.run / "files.sha256"))
        build = work / "build"
        self.stage("configure", ["cmake", "-S", str(work / "source"), "-B", str(build), "-G", "Ninja",
                                 "-DBUILD_TESTING=ON", "-DCMAKE_BUILD_TYPE=Debug"], 120)
        command = ["cmake", "--build", str(build), "--parallel", "2"]
        targets, pattern = suite_selection(suite)
        if targets:
            command += ["--target"] + targets
        self.stage("build", command, 1200)
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                   ISOLDE_TEST_ARTIFACTS=str(self.run))
        command = ["ctest", "--test-dir", str(build), "--output-on-failure", "--timeout", "120",
                   "--output-junit", str(self.run / "ctest.xml")]
        if suite != "all":
            command += ["-R", pattern]
        self.stage("ctest", command, 360, env)
        xml = ET.parse(self.run / "ctest.xml").getroot()
        cases = list(xml.iter("testcase"))
        if suite != "all":
            expected = {"studio_quick", "studio_selection", "studio_gestures", "studio_grid", "studio_grid_controls", "studio_destinations", "studio_ordered", "studio_policies", "studio_overlays", "studio_unit", "studio_behavior"} if suite == "studio" else {suite}
            if suite == "studio_grid":
                expected = {"studio_grid", "studio_grid_controls"}
            if suite == "home_component_navigation":
                expected = {"home_component_navigation", "studio_navigation"}
            if suite == "vpn":
                expected = {"vpn_setup", "vpn_launch"}
            if {c.get("name") for c in cases} != expected:
                raise RuntimeError("CTest did not execute exactly the selected suites")
        if not cases or any(c.find("failure") is not None or c.find("error") is not None for c in cases):
            raise RuntimeError("CTest report contains no tests or failures")
        if any(c.find("skipped") is not None for c in cases):
            raise RuntimeError("CTest skipped tests; inspect the report before claiming a pass")
        last = build / "Testing/Temporary/LastTest.log"
        if last.is_file():
            shutil.copy2(last, self.run / "LastTest.log")
        verify_tree(work / "source", entries)
        service_after, releases_after = self.service(), self.links()
        self.update(service=service_after, releases=releases_after)
        if service_after != service_before or releases_after != releases_before:
            raise RuntimeError("Installed service or release changed during offscreen Test; inspect evidence, do not deploy")
        result = {"passed": True, "fullSuite": suite == "all", "suite": suite,
                  "tests": len(cases), "sourceDigest": digest(self.run / "files.sha256"),
                  "serviceBefore": service_before, "serviceAfter": service_after,
                  "releasesBefore": releases_before, "releasesAfter": releases_after}
        if suite == "all":
            self.stage("stage-payload", ["cmake", "--install", str(build), "--prefix", str(self.run / "payload")], 120)
            result["payload"] = payload_hashes(self.run / "payload")
        atomic_json(self.run / "test-result.json", result)
        self.update(tests=len(cases), fullSuite=result["fullSuite"])

    def tested_payload(self):
        result = json.loads((self.run / "test-result.json").read_text())
        if not result.get("passed") or not result.get("fullSuite"):
            raise RuntimeError("This run has no passing full-suite payload; run Test -Suite all")
        if payload_hashes(self.run / "payload") != result["payload"]:
            raise RuntimeError("Staged payload changed after testing; refuse use")
        verify_tree(self.run / "source", manifest(self.run / "files.sha256"))
        return self.run / "payload"

    def configure_native(self, target="native"):
        if target not in {"native","surfshark","android"}: raise ValueError("Unsupported guest configuration target")
        payload=self.tested_payload()
        deployed=json.loads((self.run/"deploy-result.json").read_text())
        if self.command("hostname")!="isolde-test" or self.links()["current"]!=deployed["release"] or digest(Path(deployed["release"])/"bin/isolde")!=digest(payload/"bin/isolde"):
            raise RuntimeError("Native configuration requires this deployed snapshot in Isolde-Test")
        if target=="android":self.prepare_guest_space(8)
        script=self.run/"source/tools/provision-default-apps.py"
        command=["/usr/bin/python3",str(script),"isolation","--source",str(self.run/"source")]
        # Older tested installers inherited the administrator's group-writable
        # umask. Tighten only the exact approved directory after installation.
        command=["/usr/bin/python3","-c","import os,runpy,sys; sys.argv="+repr(command[1:])+"; runpy.run_path(sys.argv[0],run_name='__main__'); os.chmod('/usr/lib/isolde-native',0o755)"]
        if target=="surfshark":command=["/usr/bin/python3",str(script),"surfshark","--source",str(self.run/"source")]
        if target=="android":command=["/usr/bin/python3",str(self.run/"source/tools/provision-android.py"),"--source",str(self.run/"source"),"--software-rendering"]
        launcher=self.run/"native-admin-terminal.py"
        title="Isolde Android setup" if target=="android" else "Isolde Surfshark setup" if target=="surfshark" else "Isolde native setup"
        launcher.write_text("import subprocess\nprint("+repr(title+" — enter the VM administrator password only here.")+", flush=True)\nr = subprocess.run("+repr(["pkexec"]+command)+")\nprint('Setup finished with exit code', r.returncode, flush=True)\n"+("if r.returncode: input('Press Enter to close this window. ')\n" if target in {"surfshark","android"} else "input('Press Enter to close this window. ')\n")+"raise SystemExit(r.returncode)\n")
        if target=="android":
            # Capture installer output, never the administrator authentication
            # dialog. Errors close automatically and produce a structured exit.
            (self.run/"android-admin-exit.json").unlink(missing_ok=True)
            launcher.write_text(android_admin_launcher(command,self.run,title))
        environment=dict(os.environ)
        for line in self.command("systemctl","--user","show-environment").splitlines():
            key,_,value=line.partition("=")
            if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS"}: environment[key]=value
        before,links,layouts=self.service(),self.links(),self.layout_hashes()
        atomic_json(self.root/"service-lease.json",{"service":before,"releases":links,"runId":self.state["runId"],"pid":os.getpid()})
        try:
            self.stage("native-configure-stop",["systemctl","--user","stop","isolde.service"],30)
            print("VM_AUTH_REQUIRED: authenticate in the "+title+" terminal inside Isolde-Test; never send a password here.",flush=True)
            self.stage("native-configure-policy",["ptyxis","--standalone","--title",title,"--","/usr/bin/python3",str(launcher)],3600 if target=="android" else 900 if target=="surfshark" else 180,env=environment)
            if target=="android":
                outcome=self.run/"android-admin-exit.json"
                if not outcome.is_file() or json.loads(outcome.read_text()).get("exit")!=0:raise RuntimeError("Android administrator setup failed; see android-admin.log")
                receipt=Path("/var/lib/isolde/android-install.json")
                if receipt.is_symlink() or not receipt.is_file() or receipt.stat().st_size>65536 or receipt.stat().st_uid!=0:raise RuntimeError("Android administrator setup did not produce a valid receipt")
                result=json.loads(receipt.read_text())
                expected=digest(self.run/"source/config/android-runtime.json")
                if result.get("configurationSha256")!=expected or result.get("apkStaged") is not True or result.get("profileAppInstalled") is not False or result.get("launchVerified") is not False:raise RuntimeError("Android setup receipt does not match this tested configuration")
                atomic_json(self.run/"android-install-result.json",result)
            elif target=="surfshark":
                receipt=Path("/var/lib/isolde/surfshark-install.json")
                if not receipt.is_file():raise RuntimeError("Surfshark administrator setup did not complete")
                result=json.loads(receipt.read_text())
                if not result.get("installed"):raise RuntimeError("Surfshark installation was not confirmed")
                atomic_json(self.run/"surfshark-install-result.json",result)
            elif not Path("/usr/lib/isolde-native/bwrap").is_file() or not Path("/etc/apparmor.d/isolde-native").is_file():
                raise RuntimeError("Administrator setup did not complete; no native sandbox exception installed")
            if target=="native":self.stage("native-configure-isolation-test",["ctest","--test-dir",str(self.root/"work/build"),"-R","^native_profile_isolation$","--output-on-failure"],90)
        finally:
            self.update(service=self.restore(before),normalLayoutsBefore=layouts,normalLayoutsAfter=self.layout_hashes())
            (self.root/"service-lease.json").unlink(missing_ok=True)
            if self.links()!=links or self.layout_hashes()!=layouts: raise RuntimeError("Native configuration changed a release or saved layout")
        if target=="surfshark":
            # The VPN control app runs in the guest desktop, not a private media
            # app sandbox. Its system daemon must manage this guest's network.
            self.stage("surfshark-open",["/usr/bin/python3","-c","import subprocess; subprocess.Popen(['gtk-launch','surfshark'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)"],15,env=environment)

    def diagnose(self, app_id="freetube"):
        if app_id not in {"freetube", "cbc-gem", "steam"}: raise ValueError("Unsupported native diagnostic")
        self.tested_payload()
        profile_id = "8bdf6077-4613-4b52-9b20-d1ec7931dfcf"
        profiles = json.loads((Path.home()/".config/Isolde/profiles.json").read_text())
        if profiles.get("active") != profile_id or not any(p.get("id")==profile_id and p.get("name")=="Studio Demo" for p in profiles.get("profiles", [])):
            raise RuntimeError("Select the explicitly authorized Studio Demo profile before native diagnostics")
        work = self.root/"work"
        if json.loads((work/"manifest.json").read_text()) != manifest(self.run/"files.sha256"):
            raise RuntimeError("Native diagnostic helper does not match this tested source")
        executable = work/"build/isolde_native_smoke"
        if not executable.is_file(): raise RuntimeError("Tested native diagnostic helper is unavailable")
        environment=dict(os.environ)
        for line in self.command("systemctl","--user","show-environment").splitlines():
            key,_,value=line.partition("=")
            if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS"}: environment[key]=value
        if not environment.get("DISPLAY"): raise RuntimeError("Native diagnostics require the logged-in graphical session")
        before, links, layouts = self.service(), self.links(), self.layout_hashes()
        atomic_json(self.root/"service-lease.json", {"service": before, "releases": links, "runId": self.state["runId"], "pid": os.getpid()})
        try:
            self.stage("native-diagnostic-stop", ["systemctl", "--user", "stop", "isolde.service"], 30)
            # Existing production-bridge helper, same explicitly authorized
            # Studio Demo profile. No shell, shared home, login or policy change.
            seconds=30 if app_id=="steam" else 5
            self.stage("native-"+app_id+"-diagnostic", [str(executable), app_id, profile_id, str(seconds)], seconds+15, env=environment)
        finally:
            self.update(service=self.restore(before), normalLayoutsBefore=layouts, normalLayoutsAfter=self.layout_hashes(), diagnosticOnly=True)
            (self.root/"service-lease.json").unlink(missing_ok=True)
            if self.links()!=links or self.layout_hashes()!=layouts:
                raise RuntimeError("Native diagnostic changed a release or saved layout")

    def visual(self, case, installed=False):
        if case in ("repairs", "apps", "clients", "vpn", "shell") and not installed:
            raise RuntimeError("The explicit Studio Demo repair journey requires the verified installed release")
        payload = self.tested_payload()
        if installed:
            deployed = json.loads((self.run / "deploy-result.json").read_text())
            executable = Path(deployed["release"]) / "bin/isolde"
            if self.links()["current"] != deployed["release"] or digest(executable) != digest(payload / "bin/isolde"):
                raise RuntimeError("Installed release does not match this tested run")
        else:
            executable = payload / "bin/isolde"
        print("EXECUTABLE " + str(executable), flush=True)
        source = self.run / "source"
        script, flags = CASES[case]
        command = ["python3", str(source / "tools" / script), str(executable)] + flags
        if case in ("lounge", "pastel", "repairs", "apps", "clients"):
            fixture = "home-reference" if case == "lounge" else "home-pastel-reference"
            command += ["--case", str(source / "tests/visual" / fixture)]
        before, links = self.service(), self.links()
        layouts = self.layout_hashes()
        lease = {"service": before, "releases": links, "runId": self.state["runId"], "pid": os.getpid()}
        atomic_json(self.root / "service-lease.json", lease)
        evidence = set()

        def line_output(line):
            print(line, flush=True)
            match = re.search(r"/tmp/isolde-(?:components-acceptance|reference-config|tv-acceptance)-[A-Za-z0-9_-]+", line)
            if match:
                evidence.add(Path(match.group()))
        try:
            # The existing harness lock is still authoritative for the graphical session.
            self.stage("visual-" + case, command, 900 if case in ("pages","playlists","android") else 480, on_line=line_output)
            results = [json.loads((p / "result.json").read_text()) for p in evidence if (p / "result.json").is_file()]
            if not results or not all(visual_result_passed(case, r) for r in results):
                raise RuntimeError("Harness exited without a passing result.json")
            self.update(visualCase=case, visualPassed=True, visualReview="pending",
                        visualAcceptance="pending", semanticAcceptance="not_verified", executable=str(executable))
        finally:
            # This guard also runs on timeouts/SIGHUP when a harness cannot reach its own finally.
            restored = self.restore(before)
            self.update(service=restored, normalLayoutsBefore=layouts, normalLayoutsAfter=self.layout_hashes())
            if self.links() != links:
                raise RuntimeError("Release links changed during graphical testing")
            (self.root / "service-lease.json").unlink(missing_ok=True)
            collect_evidence(self.run, case, evidence)
            if self.layout_hashes() != layouts:
                raise RuntimeError("Normal profile layout changed during an isolated visual test")

    def layout_hashes(self):
        config = Path.home() / ".config/Isolde"
        return {str(p.relative_to(config)): digest(p) for p in config.rglob("home-layout.json")
                if p.is_file() and not p.is_symlink()}

    def deploy(self):
        payload = self.tested_payload()
        source = self.run / "source"
        spec = importlib.util.spec_from_file_location("oracle_installer", source / "tools/install-appliance.py")
        installer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(installer)
        if os.geteuid() == 0 or not installer.ubuntu_supported():
            raise RuntimeError("Deploy requires the regular user on supported Ubuntu")
        prefix = Path.home() / ".local/share/isolde-appliance"
        prefix.mkdir(parents=True, exist_ok=True)
        before = self.links()
        with exclusive_lock(prefix / ".install.lock"):
            release = installer.install_payload(payload, prefix)
        # Reuse the installer activation contract. No repeat compilation/tests or startup edits.
        self.stage("restart", ["systemctl", "--user", "restart", "isolde.service"], 30)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            service = self.service()
            pid = service.get("MainPID", "0")
            if service["ActiveState"] == "active" and Path(f"/proc/{pid}/exe").resolve() == release / "bin/isolde":
                result = {"release": str(release), "before": before, "after": self.links(), "service": service}
                atomic_json(self.run / "deploy-result.json", result)
                self.update(**result)
                return
            time.sleep(0.5)
        raise RuntimeError("Selected release failed to start; inspect restart.log and journal. No automatic rollback.")

    def prepare_guest_space(self, target_gib=5):
        if self.command("hostname") != "isolde-test":
            raise RuntimeError("Guest provisioning is restricted to Isolde-Test")
        # Only obsolete generated payloads; source, logs, screenshots, releases,
        # native build cache and all profile/account trees are retained.
        runs = self.root / "runs"
        candidates = sorted((p for p in runs.iterdir() if p.is_dir() and not p.is_symlink()), key=lambda p:p.stat().st_mtime, reverse=True)
        protected = {self.run, *candidates[:3]}
        mounts = [Path(line.split()[4].replace("\\040", " ")) for line in Path("/proc/self/mountinfo").read_text().splitlines()]
        cleanup_path = self.run / "guest-payload-cleanup.json"
        removed = json.loads(cleanup_path.read_text()).get("targets", []) if cleanup_path.exists() else []
        for candidate in reversed(candidates):
            if shutil.disk_usage(self.root).free >= target_gib * 1024**3: break
            target = candidate / "payload"
            if candidate in protected or not target.is_dir() or target.is_symlink(): continue
            if target.resolve() != target or not target.resolve().is_relative_to(runs.resolve()): raise RuntimeError("Unsafe guest artifact path")
            if any(m == target or m.is_relative_to(target) for m in mounts): raise RuntimeError("Refusing mounted guest artifact")
            removed.append(str(target))
            atomic_json(self.run / "guest-payload-cleanup.json", {"guest": "isolde-test", "targets": removed})
            shutil.rmtree(target)
        legacy = Path.home()/"isolde-runs"
        if legacy.is_dir() and not legacy.is_symlink() and legacy.resolve() == legacy:
            old_runs = sorted((p for p in legacy.iterdir() if p.is_dir() and not p.is_symlink() and re.fullmatch(r"studio-implementation-[0-9]+", p.name)), key=lambda p:int(p.name.rsplit("-", 1)[1]))
            active = []
            for process in Path("/proc").iterdir():
                if process.name.isdigit():
                    for name in ("cwd", "exe"):
                        try: active.append((process/name).resolve(strict=True))
                        except (OSError, RuntimeError): pass
            for old in old_runs[:-3][:128]:
                if shutil.disk_usage(self.root).free >= 8 * 1024**3: break
                target = old/"build"
                if not target.is_dir() or any(p == target or p.is_relative_to(target) for p in active): continue
                # Record the checked absolute target before deleting generated
                # artifacts. The helper rejects links, mounts and unknown trees.
                removed.append(str(target))
                atomic_json(cleanup_path, {"guest": "isolde-test", "targets": removed})
                remove_guest_build(target, legacy, mounts)
    def provision(self):
        self.tested_payload()
        deployment = json.loads((self.run / "deploy-result.json").read_text())
        if self.links()["current"] != deployment["release"]:
            raise RuntimeError("Provisioning requires this run's deployed release")
        self.prepare_guest_space()
        self.check()
        if shutil.disk_usage(self.root).free < 4 * 1024**3: raise RuntimeError("Default clients need at least 4 GiB free after bounded artifact cleanup")
        before, links, layouts = self.service(), self.links(), self.layout_hashes()
        atomic_json(self.root / "service-lease.json", {"service": before, "releases": links, "runId": self.state["runId"], "pid": os.getpid()})
        source = self.run / "source"
        script = source / "tools/provision-default-apps.py"
        try:
            self.stage("default-apps-stop", ["systemctl", "--user", "stop", "isolde.service"], 30)
            self.stage("default-apps-system", ["python3", str(script), "system", "--sudo-apt", "--source", str(source)], 1800)
            self.stage("default-apps-activate", ["python3", str(script), "activate", "--source", str(source)], 480)
            receipt = Path.home()/".config/Isolde/default-apps-completed.json"
            atomic_json(self.run / "default-apps-result.json", json.loads(receipt.read_text()))
            if self.layout_hashes() != layouts: raise RuntimeError("Application provisioning changed a saved layout")
        finally:
            self.update(service=self.restore(before), normalLayoutsBefore=layouts, normalLayoutsAfter=self.layout_hashes())
            (self.root / "service-lease.json").unlink(missing_ok=True)


def remove_guest_build(target, boundary, mounts):
    """Generated legacy build only, with no redirected or mounted descendants."""
    if target.name != "build" or target.is_symlink() or target.resolve() != target.absolute() or not target.resolve().is_relative_to(boundary.resolve()):
        raise RuntimeError("Unsafe legacy guest build target")
    if not (target/"CMakeCache.txt").is_file() or not (target/"CMakeFiles").is_dir():
        raise RuntimeError("Legacy target is not a generated CMake build")
    if any(m == target or m.is_relative_to(target) for m in mounts): raise RuntimeError("Refusing mounted legacy guest build")
    shutil.rmtree(target)


def collect_evidence(run, case, directories):
    """Do not copy WebEngine caches, PulseAudio cookies or personal profile trees."""
    with tarfile.open(run / ("evidence-" + case + "-" + str(time.time_ns()) + ".tar.gz"), "w:gz") as archive:
        for directory in sorted(directories):
            if not re.fullmatch(r"/tmp/isolde-(?:components-acceptance|reference-config|tv-acceptance)-[A-Za-z0-9_-]+", str(directory)):
                raise RuntimeError("Unexpected evidence root")
            paths = [directory / name for name in ("result.json", "shell.log", "android.log", "attempt-config.json", "provider-requests.json")]
            paths += list((directory / "config/Isolde").glob("home-layout*.json"))
            paths += list((directory / "config/Isolde/skins").glob("*.json"))
            for path in paths[:128]:
                if path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(directory.resolve()):
                    if path.stat().st_size > 8 * 1024**2:
                        continue
                    archive.add(path, arcname=str(Path(directory.name) / path.relative_to(directory)), recursive=False)


def write_report(run):
    """A flat, allowlisted bundle; never the source tree, payload, caches or credentials."""
    with tarfile.open(run / "report.tar.gz.tmp", "w:gz") as archive:
        for path in sorted(run.iterdir()):
            if path.is_file() and not path.is_symlink() and (path.suffix in (".log", ".json", ".xml")
                    or re.fullmatch(r"evidence-(studio|lounge|pastel|pages|repairs|apps|clients|vpn|shell|playlists|android)-[0-9]+\.tar\.gz", path.name)
                    or re.fullmatch(r"studio-test-[a-z0-9-]+\.png", path.name)):
                archive.add(path, arcname=path.name, recursive=False)
    (run / "report.tar.gz.tmp").replace(run / "report.tar.gz")


@contextlib.contextmanager
def exclusive_lock(path):
    import fcntl
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError("BUSY: another owned run is active. Use Status; do not delete locks or start another build.")
        yield


def interrupted(signum, _frame):
    # selectors treats InterruptedError as a retryable interrupted system call.
    # Cancellation must escape that retry and reach the stage/service finally.
    raise RuntimeError("Oracle interrupted by signal " + str(signum))


def vpn_platform():
    """Public installed-package/NetworkManager metadata, never account storage."""
    def query(arguments):
        result=subprocess.run(arguments,capture_output=True,text=True,timeout=5)
        return result.stdout[:65536] if result.returncode==0 else ""
    version=query(["dpkg-query","-W","-f=${Version}","surfshark"]).strip()
    files=query(["dpkg-query","-L","surfshark"]).splitlines() if version else []
    metadata=[]
    for name in files:
        if not (name.startswith(("/usr/bin/","/usr/share/applications/","/usr/lib/systemd/system/","/lib/systemd/system/")) and (name.endswith((".desktop",".service")) or name.startswith("/usr/bin/"))):continue
        path=Path(name)
        if not path.is_file():continue
        entry={"path":name}
        if path.stat().st_size<=16384:
            content=path.read_bytes()
            if name.endswith((".desktop",".service")):
                entry["declarations"]=[line for line in content.decode("utf-8","replace").splitlines() if line.startswith(("Exec=","ExecStart=","BusName="))][:8]
            elif content.startswith(b"#!"):
                entry["launcher"]=content.decode("utf-8","replace")
        metadata.append(entry)
    connections=[]
    for line in query(["nmcli","-t","-f","UUID,TYPE,DEVICE","connection","show"]).splitlines()[:128]:
        cells=line.split(":")
        if len(cells)==3 and cells[1] in {"wireguard","vpn"}:connections.append({"uuid":cells[0],"type":cells[1],"device":cells[2]})
    bindings=query(["python3","-c","import gi;gi.require_version('NM','1.0');from gi.repository import NM;print(NM.MAJOR_VERSION)"]).strip()
    proton=query(["dpkg-query","-W","-f=${Status} ${Version}","proton-vpn-gnome-desktop"]).strip()
    proton_files=query(["dpkg-query","-L","proton-vpn-cli"]).splitlines()
    permissions=query(["nmcli","-t","-f","PERMISSION,VALUE","general","permissions"]).splitlines()
    reasons=[]
    for line in query(["journalctl","-u","NetworkManager","--since=-10min","-n","200","--no-pager","-o","cat"]).splitlines():
        match=re.search(r"\(ivpn[0-9a-f]{10}\).*reason '([a-z-]+)'",line)
        if match:reasons.append(match[1])
    routing={}
    for family in ("-4","-6"):
        rules=json.loads(query(["ip","-j",family,"rule","show"]) or "[]")
        routing[family]=[{k:r[k] for k in ("priority","table","fwmark","fwmask","not","suppress_prefixlength") if k in r} for r in rules[:64]]
    return {"installedVersion":version,"protonPackage":proton,"protonCliFiles":[p for p in proton_files if p.startswith('/usr/bin/')],"nmBindings":bindings,"packageFiles":metadata[:32],"vpnConnections":connections,"networkManagerPermissions":permissions,"recentIsoldeActivationReasons":reasons[-20:],"routingPolicy":routing}


def cancel_owned(root, run_id):
    path = root / "runs" / run_id / "status.json"
    if not path.is_file():
        return {"cancelled": False, "reason": "unknown run"}
    state = json.loads(path.read_text())
    pid = state.get("pid", 0)
    proc = Path("/proc") / str(pid)
    if state.get("state") != "running" or not proc.is_dir():
        return {"cancelled": False, "reason": "run already ended"}
    command = (proc / "cmdline").read_bytes().decode().split("\0")
    owned_driver = any(p.startswith(str(root / "bin" / "driver-")) and p.endswith(".py") for p in command)
    start_ticks = (proc / "stat").read_text().split()[21]
    if (not owned_driver or "--run-id" not in command or command[command.index("--run-id") + 1] != run_id
            or state.get("processStartTicks") != start_ticks):
        raise RuntimeError("Refusing to signal a process whose ownership/start identity does not match")
    # SIGINT also unwinds older guardians whose SIGTERM handler raised the
    # retryable InterruptedError. Ownership and process-start checks stay above.
    os.kill(pid, signal.SIGINT)
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        state = json.loads(path.read_text())
        if state.get("state") != "running":
            return {"cancelled": True, "state": state}
        if not proc.is_dir():
            state.update(state="failed", error="Cancelled owned runner", updated=time.time())
            atomic_json(path, state)
            return {"cancelled": True, "state": state}
        time.sleep(.2)
    raise RuntimeError("Owned runner has not finished cleanup; preserve evidence and inspect Status")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["check", "test", "visual", "deploy", "provision", "configure", "diagnose", "status", "recover", "cancel"])
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--suite", choices=SUITES, default="all")
    parser.add_argument("--case", choices=CASES, default="studio")
    parser.add_argument("--installed", action="store_true")
    parser.add_argument("--native-app", choices=["freetube", "cbc-gem", "steam"], default="freetube")
    parser.add_argument("--configure-target",choices=["native","surfshark","android"],default="native")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", args.run_id):
        parser.error("Invalid run ID")
    root = Path.home() / "isolde-oracle"
    root.mkdir(exist_ok=True)
    if args.action == "status":
        path = root / "runs" / args.run_id / "status.json"
        status = json.loads(path.read_text()) if path.is_file() else {"state": "unknown", "runId": args.run_id}
        environment = dict(os.environ)
        manager = subprocess.run(["systemctl", "--user", "show-environment"], capture_output=True, text=True, timeout=10)
        for line in manager.stdout.splitlines():
            key, _, value = line.partition("=")
            if key in {"DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}: environment[key] = value
        windows = subprocess.run(["wmctrl", "-lpx"], env=environment, capture_output=True, text=True, timeout=10)
        status["windowClasses"] = [line.split(None, 5)[:4] for line in windows.stdout.splitlines()[:64]]
        status["vpnPlatform"]=vpn_platform()
        status["androidSetup"]={"runtimeExecutable":shutil.which("waydroid"),
            "runtimeConfigured":Path("/var/lib/waydroid/waydroid.cfg").is_file(),
            "apkStaged":Path("/usr/share/isolde/android/smarttube.apk").is_file(),
            "receiptPresent":Path("/var/lib/isolde/android-install.json").is_file(),
            "freeGiB":round(shutil.disk_usage(Path.home()).free/1024**3,2)}
        # Inspect only this guardian's descendant tree; never unrelated app
        # arguments, environment, account files or terminal contents.
        status["guardianAlive"]=False
        try:
            proc=Path("/proc")/str(status.get("pid",0))
            status["guardianAlive"]=(status.get("processStartTicks")==proc.joinpath("stat").read_text().split()[21]
                and str(root/"bin"/("driver-"+status.get("driverDigest","")+".py")).encode() in proc.joinpath("cmdline").read_bytes().split(b"\0"))
        except (OSError,IndexError):pass
        pending=[status.get("pid",0)] if status["guardianAlive"] else []; seen=set(); names=[]
        while pending and len(seen)<64:
            pid=pending.pop()
            if not isinstance(pid,int) or pid<2 or pid in seen:continue
            seen.add(pid)
            try:
                process=Path("/proc")/str(pid)
                names.append(process.joinpath("comm").read_text().strip()[:40])
                children=process.joinpath("task",str(pid),"children").read_text()[:4096]
                pending.extend(int(p) for p in children.split() if p.isdigit())
            except (OSError,ValueError):pass
        status["androidSetup"]["ownedProcessNames"]=names
        print(json.dumps(status))
        return
    if args.action == "cancel":
        print(json.dumps(cancel_owned(root, args.run_id)), flush=True)
        return
    oracle = Oracle(root, args.run_id)
    acquired = False
    try:
        with exclusive_lock(root / "oracle.lock"):
            acquired = True
            signal.signal(signal.SIGTERM, interrupted)
            signal.signal(signal.SIGINT, interrupted)
            signal.signal(signal.SIGHUP, interrupted)
            oracle.update(action=args.action)
            if args.action == "check":
                oracle.check()
            elif args.action == "test":
                oracle.test(args.suite)
            elif args.action == "visual":
                oracle.visual(args.case, args.installed)
            elif args.action == "deploy":
                oracle.deploy()
            elif args.action == "provision":
                oracle.provision()
            elif args.action == "diagnose":
                oracle.diagnose(args.native_app)
            elif args.action == "configure":
                oracle.configure_native(args.configure_target)
            elif args.action == "recover":
                lease = root / "service-lease.json"
                if lease.is_file():
                    record = json.loads(lease.read_text())
                    if oracle.links() != record["releases"]:
                        raise RuntimeError("Release links changed; inspect manually before recovery")
                    with exclusive_lock(Path("/tmp/isolde-home-components-acceptance.lock")):
                        oracle.update(service=oracle.restore(record["service"]))
                        lease.unlink()
            oracle.update(state="passed", stage="finished")
    except Exception as error:
        if acquired:
            oracle.update(state="failed", error=str(error))
        print("FAIL " + str(error), flush=True)
        sys.exit(1)
    finally:
        if acquired:
            write_report(oracle.run)
    print("VM_RESULT " + json.dumps(oracle.state), flush=True)


if __name__ == "__main__":
    main()
