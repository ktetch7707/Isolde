#!/usr/bin/env python3
"""Compare a config-only Home attempt with the packaged default in a disposable profile.

Requires the VM graphical session, wmctrl and Xlib. Temporarily stops the
normal user service and restores it in finally. Never edits personal layouts.
"""
import argparse
import ctypes
import ctypes.util
import fcntl
import hashlib
import json
import os
import re
import shutil
from pathlib import Path
import subprocess
import tempfile
import time


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


def collection_review(document):
    """Saved structure identifies required reviews, never provider population.

    This journey checks save/reapply/reset. It has no provider assertions and
    must not imply collection functionality or a populated visual acceptance.
    Inspect every page because the saved skin can include linked destinations.
    """
    kinds = set()
    layouts = [document]
    while layouts:
        layout = layouts.pop()
        for item in layout.get("components", layout.get("items", [])):
            if "appCollection" in item:
                kinds.add("apps")
            if item.get("componentType", item.get("widgetType")) == "library":
                kinds.add("media")
        layouts.extend(page["layout"] for page in layout.get("pages", []) if isinstance(page.get("layout"), dict))
    return {
        "required": sorted(kinds),
        "population_verified": False if kinds else None,
        "behavior_verified": False if kinds else None,
        "missing": [f"{kind.capitalize()} collection population and behavior are not verified by save/reapply/reset."
                    for kind in sorted(kinds)],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--reference", type=Path,
                        default=None,
                        help="Retained user reference; never render this as the Home screen")
    parser.add_argument("--case", type=Path, default=Path(__file__).resolve().parents[1] / "tests/visual/home-reference",
                        help="Visual case directory with reference.jpg, recreation-config.json and optional interaction.json/wallpaper.png")
    parser.add_argument("--inspect", action="store_true", help="Pause briefly on failure for a VM screenshot")
    parser.add_argument("--skin-from-settings", action="store_true", help="Select the built-in design and save/reapply a personal skin entirely through Settings")
    parser.add_argument("--capture-library", action="store_true", help="Also pause for a screenshot of the skin library")
    parser.add_argument("--existing-profile", help="Explicit installed repair journey for the active VM acceptance profile")
    parser.add_argument("--verify-default-apps", action="store_true", help="Verify provisioned clients in the authorized VM acceptance profile")
    parser.add_argument("--skip-steam", action="store_true", help="Independent client checks; never reports Steam bootstrap verified")
    parser.add_argument("--preview-seconds", type=int, default=0, choices=range(31), help="Hold the appearance preview for visual inspection")
    args = parser.parse_args()
    case = args.case.resolve(strict=True)
    reference_bytes = (args.reference or case / "reference.jpg").read_bytes()
    reference_hash = hashlib.sha256(reference_bytes).hexdigest()
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    executable = args.executable.resolve(strict=True)
    evidence = Path(tempfile.mkdtemp(prefix="isolde-reference-config-"))
    (evidence / "reference.jpg").write_bytes(reference_bytes)
    print("Reference SHA256: " + reference_hash, flush=True)
    config = evidence / "config" / "Isolde"
    config.mkdir(parents=True)
    original = json.loads((case / "recreation-config.json").read_text(encoding="utf-8-sig"))
    interaction = json.loads((case / "interaction.json").read_text(encoding="utf-8-sig")) if (case / "interaction.json").exists() else {}
    (config / "artwork").mkdir()
    (config / "artwork/reference.jpg").write_bytes(reference_bytes)
    if (case / "wallpaper.png").exists(): shutil.copyfile(case / "wallpaper.png",config / "artwork/wallpaper.png")
    layout = config / "home-layout.json"
    original_bytes = json.dumps(original).encode()
    if not args.skin_from_settings:
        layout.write_bytes(original_bytes)
    (config / "device-setup.json").write_text(json.dumps({"version": 1, "step": 6, "complete": True}))
    (config / "Kodi.conf").write_text("[profiles]\ndefault\\kodi\\autoStart=false\n")
    (config / "Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    # SourceManager currently uses default QSettings (no organization set),
    # whereas device audio/Kodi use explicit Isolde namespaces.
    default_settings = evidence / "config" / "Unknown Organization"
    default_settings.mkdir()
    (default_settings / "Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    env = dict(os.environ)
    for line in run("systemctl", "--user", "show-environment").splitlines():
        key, _, value = line.partition("=")
        if key in {"DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}:
            env[key] = value
    if args.existing_profile:
        if not re.fullmatch(r"[a-f0-9-]{36}", args.existing_profile): raise ValueError("Invalid acceptance profile")
        normal = Path.home() / ".config/Isolde"
        profiles = json.loads((normal / "profiles.json").read_text())
        assert profiles["active"] == args.existing_profile, "Select the authorized acceptance profile first"
        assert any(p["id"] == args.existing_profile and p["name"] == "Studio Demo" for p in profiles["profiles"])
        layout = normal / "profiles" / args.existing_profile / "home-layout.json"
        assert not layout.is_symlink() and layout.resolve().is_relative_to(normal.resolve())
        original_bytes = layout.read_bytes()
        original = json.loads(original_bytes)
        (evidence / "attempt-config.json").write_bytes(original_bytes)
        env["QT_QPA_PLATFORM"] = "xcb"
    else:
        env.update(XDG_CONFIG_HOME=str(evidence / "config"), XDG_DATA_HOME=str(evidence / "data"),
                   XDG_CACHE_HOME=str(evidence / "cache"), QT_QPA_PLATFORM="xcb")
    from reference_collections import ReferenceCollections
    fixture_path = case / "collections.json"
    collection_provider = ReferenceCollections(evidence, json.loads(fixture_path.read_text()),
        Path(__file__).resolve().parents[1]) if fixture_path.exists() and not args.existing_profile else None
    x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]; x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XStringToKeysym.argtypes = [ctypes.c_char_p]; x11.XStringToKeysym.restype = ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]; x11.XKeysymToKeycode.restype = ctypes.c_uint
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    x11.XGetInputFocus.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int)]
    class KeyEvent(ctypes.Structure):
        _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong), ("send_event", ctypes.c_int),
                    ("display", ctypes.c_void_p), ("window", ctypes.c_ulong), ("root", ctypes.c_ulong),
                    ("subwindow", ctypes.c_ulong), ("time", ctypes.c_ulong), ("x", ctypes.c_int), ("y", ctypes.c_int),
                    ("x_root", ctypes.c_int), ("y_root", ctypes.c_int), ("state", ctypes.c_uint),
                    ("keycode", ctypes.c_uint), ("same_screen", ctypes.c_int)]
    class Event(ctypes.Union):
        _fields_ = [("key", KeyEvent), ("padding", ctypes.c_long * 24)]
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.POINTER(Event)]
    # Set before opening Xlib: it reads the process environment itself.
    os.environ.update({key: env[key] for key in ("DISPLAY", "XAUTHORITY") if key in env})
    display = x11.XOpenDisplay(env.get("DISPLAY", "").encode())
    if not display:
        raise RuntimeError("Cannot open the VM X11 display")
    # XWayland can list a window before it is focusable. Do not let Xlib's
    # default fatal handler bypass the finally block and service restoration.
    error_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
    error_handler = error_type(lambda _display, _event: 0)
    x11.XSetErrorHandler.argtypes = [error_type]
    x11.XSetErrorHandler(error_handler)

    def key(name, count=1):
        code = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name.encode()))
        if not code:
            raise RuntimeError(f"No keycode for {name}")
        for _ in range(count):
            # Window-targeted X11 events work on XWayland without injecting
            # compositor-global shortcuts into other applications.
            for event_type, mask in ((2, 1), (3, 2)):
                event = Event()
                event.key = KeyEvent(type=event_type, send_event=1, display=display, window=int(window, 16),
                                     time=int(time.monotonic()*1000) & 0xffffffff, keycode=code, same_screen=1)
                x11.XSendEvent(display, int(window, 16), 1, mask, ctypes.byref(event))
            x11.XFlush(display)
            time.sleep(0.09)
        time.sleep(0.4)

    def expect(predicate, message, seconds=8):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("The shell exited; inspect " + str(evidence / "shell.log"))
            try:
                if predicate():
                    print(message, flush=True)
                    return
            except (OSError, ValueError, KeyError):
                pass
            time.sleep(0.1)
        raise RuntimeError(message + " did not complete; evidence: " + str(evidence))

    active = subprocess.run(["systemctl", "--user", "is-active", "--quiet", "isolde.service"]).returncode == 0
    process = None
    desktop_probe = None
    try:
        if active:
            run("systemctl", "--user", "stop", "isolde.service")
        with (evidence / "shell.log").open("w") as log:
            process = subprocess.Popen([str(executable)], env=env, stdout=log, stderr=log)
            window = None
            for _ in range(80):
                for line in run("wmctrl", "-lp", env=env).splitlines():
                    fields = line.split(None, 4)
                    if len(fields) >= 3 and fields[2] == str(process.pid):
                        window = fields[0]
                        break
                if window:
                    break
                if process.poll() is not None:
                    raise RuntimeError("The shell failed to start; " + str(evidence))
                time.sleep(0.1)
            if not window:
                raise RuntimeError("The shell did not create an X11 window")
            run("wmctrl", "-i", "-a", window, env=env)
            time.sleep(1)
            x11.XSetInputFocus(display, int(window, 16), 2, 0)
            x11.XFlush(display)
            time.sleep(1)
            focused = ctypes.c_ulong(); revert = ctypes.c_int()
            x11.XGetInputFocus(display, ctypes.byref(focused), ctypes.byref(revert))
            print(f"Acceptance window {window}, keyboard focus {focused.value:#x}", flush=True)
            def captured(phase):
                print(phase.upper() + "_READY " + str(evidence), flush=True)
                deadline = time.monotonic() + 150
                while not (evidence / (phase + "-captured")).exists():
                    if process.poll() is not None: raise RuntimeError("Shell exited")
                    if time.monotonic() > deadline: raise RuntimeError("Screenshot confirmation timed out")
                    time.sleep(0.2)
            if args.existing_profile:
                household = Path.home() / ".config/Isolde/home-layout.json"
                household_before = household.read_bytes()
                pending = [c for c in original["components"] if c["id"] in {"music", "games", "guide"}
                           and c["action"]["type"] != "open-page"]
                if pending:
                    key("Home"); key("Up", 30); key("Left", 3); key("Right"); key("Return")
                    key("Up", 10); key("Down"); key("Return")
                    more = 3 + len(original.get("pages", [])) + int(original.get("rootPage", {}).get("visible", True))
                    key("Down", more); key("Return"); key("Down", 8); key("Return")
                    key("Down", 2); key("Right", 4); key("Return")
                    captured("updatepreview")
                    assert layout.read_bytes() == original_bytes
                    key("Escape")
                    assert layout.read_bytes() == original_bytes
                    key("Return"); key("Return")
                    expect(lambda: layout.read_bytes() != original_bytes, "Explicit preserving starter-page update saved")
                    updated = json.loads(layout.read_bytes())
                    for name, value in original.items():
                        if name not in {"components", "pages"}: assert updated[name] == value, name
                    before_components = {c["id"]: c for c in original["components"]}
                    for c in updated["components"]:
                        previous = before_components[c["id"]]
                        if c["id"] in {p["id"] for p in pending}:
                            assert c["action"]["type"] == "open-page"
                            assert {k:v for k,v in c.items() if k not in {"action", "source"}} == {k:v for k,v in previous.items() if k not in {"action", "source"}}
                        else: assert c == previous
                    assert updated["pages"][:len(original.get("pages", []))] == original.get("pages", [])
                    assert household.read_bytes() == household_before
                    key("Escape", 3); key("Home")
                captured("updated")
                post_update = layout.read_bytes()
                receipt = Path.home()/".config/Isolde/default-apps-completed.json"
                if receipt.is_file():
                    collection = next((c for c in original["components"] if c.get("appCollection", {}).get("mode") == "all" and not c.get("appCollection", {}).get("section")), None)
                    required = json.loads((Path(__file__).resolve().parents[1]/"config/default-apps.json").read_text())["apps"]
                    installed = json.loads((Path.home()/".config/Isolde/installed-apps.json").read_text())["installed"]
                    assert set(required) <= set(installed)
                    capacity = (collection.get("presentation", {}).get("layout", {}).get("visibleColumns", 4)) if collection else 0
                    if collection and len(required) > capacity:
                        key("Home"); key("Up", 30); key("Down", 4); key("Left", 30); key("Right", capacity-1); key("Return")
                        captured("appsoverflow")
                        key("Escape")
                        assert layout.read_bytes() == post_update
                if args.verify_default_apps:
                    assert receipt.is_file(), "Default applications have not been provisioned"
                    def starter(column):
                        key("Home"); key("Up", 30); key("Down", 3); key("Left", 10)
                        if column: key("Right", column)
                        key("Return")
                    steam_home = Path.home()/".local/share/Isolde/native-profiles"/args.existing_profile/"steam/home"
                    def steam_ready():
                        folders = [steam_home/".steam/debian-installation", steam_home/".local/share/Steam", steam_home/".steam/steam"]
                        client = any(p.resolve().is_relative_to(steam_home.resolve()) and (p/"ubuntu12_32/steam").is_file() and (p/"package/steam_client_ubuntu12.installed").is_file() for p in folders)
                        windows=subprocess.run(["wmctrl","-lx"],env=env,text=True,capture_output=True)
                        return client and windows.returncode==0 and "steam" in windows.stdout.lower()
                    if not args.skip_steam:
                        starter(4); captured("steamgames"); key("Return")
                        time.sleep(1); captured("steampermission")
                        time.sleep(3); captured("steaminstall")
                        expect(steam_ready, "Steam client finished its first download and opened its own UI", 300)
                        time.sleep(20)
                        captured("steam"); key("Home")
                    # Registry order comes from declarative packaged definitions.
                    source = Path(__file__).resolve().parents[1]
                    definitions = json.loads((source/"config/apps.json").read_text())
                    packages = {}
                    for relative in json.loads((source/"config/catalog.json").read_text())["packages"]:
                        package = json.loads((source/"config"/relative).read_text())
                        packages[package["app"]["id"]] = package["app"]
                    definitions += [packages[app_id] for app_id in installed if app_id in packages]
                    ids = [a["id"] for a in definitions if a["type"] in {"web", "desktop"}]
                    for app_id, phase in (("freetube", "freetube"), ("cbc-gem", "streaming")):
                        state_path=Path.home()/".config/Isolde/installed-apps.json"
                        if json.loads(state_path.read_text()).get("packageStates",{}).get(app_id)=="failed":
                            # Exercise the visible, explicit Retry Setup action;
                            # never erase failure records behind the application's back.
                            key("Home"); key("Menu"); key("Down")
                            key("Down",len(definitions)+list(packages).index(app_id))
                            captured(phase+"retry"); key("Return")
                            expect(lambda: json.loads(state_path.read_text()).get("packageStates",{}).get(app_id)=="ready", "Explicit Retry Setup completed: "+app_id)
                            key("Home")
                        index = ids.index(app_id)
                        assert collection and index < capacity-1, "Default test client is outside this saved collection's visible slots"
                        key("Home"); key("Up", 30); key("Down", 4); key("Left", 30); key("Right", index); key("Return")
                        time.sleep(1); captured(phase+"permission")
                        marker = "freetube" if app_id == "freetube" else "isolde-cbc-gem"
                        expect(lambda: marker in run("wmctrl", "-lx", env=env).lower(), "Private native client opened: "+app_id, 30)
                        captured(phase)
                        expect(lambda: int(run("xprop","-root","_NET_ACTIVE_WINDOW",env=env).split()[-1],16)==int(window,16), "Physical Home restored Isolde focus: "+app_id)
                        captured(phase+"return")
                    assert household.read_bytes() == household_before
                    assert layout.read_bytes() == post_update
                    (evidence/"result.json").write_text(json.dumps({"passed": True, "native_clients": True, "steam_client_downloaded": not args.skip_steam, "subscriptionPlaybackVerified": False, "household_unchanged": True}))
                    return
                # The portal belongs to the real compositor. Host-side bounded
                # VM key input follows review of its permission screenshot.
                key("Home"); key("Up", 30); key("Down", 2); key("Left", 10); key("Right", 4); key("Return")
                captured("desktoppermission")
                expect(lambda: "Map State: IsUnMapped" in run("xwininfo", "-id", window), "Desktop handoff hid the shell only after a granted shortcut")
                probe_name = "isolde-return-" + str(os.getpid())
                desktop_probe = subprocess.Popen(["xmessage", "-name", probe_name, "-center", "Desktop return test: Home must return to Isolde from this independent application."], env=env, stdout=log, stderr=log)
                def probe_window():
                    for line in run("wmctrl", "-lx", env=env).splitlines():
                        fields = line.split(None, 4)
                        if len(fields) >= 3 and fields[2].split(".")[0] == probe_name: return fields[0]
                    return ""
                expect(lambda: bool(probe_window()), "Independent desktop application opened")
                probe = probe_window(); run("wmctrl", "-i", "-a", probe, env=env)
                x11.XSetInputFocus(display, int(probe, 16), 2, 0); x11.XFlush(display)
                def probe_focused():
                    target = ctypes.c_ulong(); mode = ctypes.c_int()
                    x11.XGetInputFocus(display, ctypes.byref(target), ctypes.byref(mode))
                    return target.value == int(probe, 16)
                expect(probe_focused, "Return control is unfocused; independent desktop app owns input")
                captured("desktop")
                expect(lambda: "Map State: IsViewable" in run("xwininfo", "-id", window), "Compositor Home shortcut returned to Isolde")
                captured("desktopreturned")
                assert household.read_bytes() == household_before
                (evidence / "result.json").write_text(json.dumps({"passed": True, "profile": args.existing_profile,
                    "preserving_update": True, "household_unchanged": True, "real_compositor_return": True}))
                return
            if args.skin_from_settings:
                # Begin with the packaged default. No Home layout is injected.
                assert not layout.exists()
                key("Up",30); key("Right"); key("Return")  # Recovery -> Edit Home.
                key("Down",4); key("Return"); key("Down",8); key("Return")  # Studio -> More -> Saved skins.
                if args.capture_library: captured("library")
                preset_index = 2 if case.name == "home-pastel-reference" else 1
                key("Down",preset_index); key("Return"); key("Return")
                expect(layout.exists,"Built-in skin applied from Settings")
                key("Right"); key("Return")  # Save current as new skin.
                key("Down",20); key("Return")  # Accept the default name with the remote keyboard.
                skin_directory = config / "skins"
                expect(lambda: len(list(skin_directory.glob("user-*.json"))) == 1,"Personal skin saved from Settings")
                before_switch = layout.read_bytes()
                key("Left"); key("Up",30); key("Return"); key("Return")  # Apply default.
                expect(lambda: not json.loads(layout.read_bytes()).get("composition"),"Default selected in skin library")
                key("Down",3); key("Return"); key("Return")  # Reapply personal skin.
                expect(lambda: layout.read_bytes() == before_switch,"Saved skin round trip passed")
                key("Escape"); key("Escape"); key("Escape")
                key("Home")  # Capture the destination after the explicit shell return, independent of queued Back events.
                original_bytes = layout.read_bytes()
            for name in interaction.get("initialKeys",["Right"]): key(name)
            assert layout.read_bytes() == original_bytes
            (evidence / "attempt-config.json").write_bytes(original_bytes)
            if collection_provider:
                document = json.loads(original_bytes)
                history = [item for item in document["components"] if item.get("family") == "recently-viewed"]
                apps = [item for item in document["components"] if "appCollection" in item]
                assert len(history) == 1 and len(apps) == 1
                assert history[0]["source"]["kind"] == "recently-viewed"
                assert history[0]["presentation"]["libraryKind"] == "history"
                assert not any(item.get("customLogo") for item in document["components"]
                               if item.get("categoryId") in {"favorites", "recent"})
                expect(lambda: any(r["method"] == "Isolde.GetArtwork" for r in collection_provider.requests),
                       "Production history collection requested local artwork")
                collection_provider.verify_history()
            captured("custom")
            if collection_provider:
                capacity = apps[0].get("presentation", {}).get("layout", {}).get("visibleColumns", 4)
                eligible = [app for app in collection_provider.installed]
                assert len(eligible) > capacity, "Collection fixture must exercise app overflow"
                key("Home"); key("Up", 30); key("Down", 4)
                key("Left", 30); key("Right", capacity-1); key("Return")
                captured("appsoverflow")
                key("Escape")
                captured("appsoverflowreturn")
                assert layout.read_bytes() == original_bytes, "Gallery browsing must not write the layout"
                def starter(column):
                    key("Home"); key("Up", 30); key("Down", 3)
                    key("Left", 10)
                    if column: key("Right", column)
                    key("Return")
                starter(3)
                expect(lambda: any(r["method"] == "AudioLibrary.GetAlbums" for r in collection_provider.requests),
                       "Music page requested Kodi albums")
                captured("music")
                key("Return")
                expect(lambda: any(r["method"] == "AudioLibrary.GetSongs" for r in collection_provider.requests),
                       "Album activation requested its Kodi tracks")
                captured("tracks")
                key("Escape")
                starter(4)
                captured("games")
                starter(0)
                expect(lambda: any(r["method"] == "PVR.GetChannels" for r in collection_provider.requests),
                       "Guide page requested configured Kodi channels")
                captured("guide")
                key("Home"); key("XF86Search")
                captured("search")
                key("Escape"); key("Home")
                assert layout.read_bytes() == original_bytes
            if args.skin_from_settings:
                key("Up",30); key("Left",3)
                if case.name == "home-pastel-reference": key("Right")
                key("Return")  # Header Settings, with remembered Settings selection.
                key("Up",10); key("Down"); key("Return")  # Select Home explicitly.
            else:
                for name in interaction.get("editKeys",["Up","Up","Right","Return"]): key(name)
            # Customize opens the full-size Studio directly; no legacy action wall.
            assert layout.read_bytes() == original_bytes
            if args.capture_library: captured("editor")
            if args.inspect: captured("resetmenu")
            current_document=json.loads(layout.read_text())
            more_index=3+len(current_document.get("pages",[]))+int(current_document.get("rootPage",{}).get("visible",True))
            print(f"Reset Studio More index: {more_index}",flush=True)
            key("Down",more_index); key("Return"); key("Down",9); key("Return")  # More -> Restore defaults.
            if args.inspect: captured("resetconfirm")
            key("Down"); key("Return")  # Explicit scoped confirmation.
            defaults=json.loads((Path(__file__).resolve().parent.parent/"config/default-home-layout.json").read_text())
            def reset_verified():
                if not current_document.get("pages"): return not layout.exists()
                restored=json.loads(layout.read_text())
                return (restored.get("pages")==current_document["pages"]
                    and restored.get("homePageId","")==current_document.get("homePageId","")
                    and not restored.get("composition")
                    and {i["id"] for i in restored["components"]}=={i["id"] for i in defaults["items"]}
                    and restored["categories"]==defaults["categories"]
                    and all(str(restored["theme"].get(k)).lower()==str(v).lower() for k,v in defaults["theme"].items()))
            expect(reset_verified, "Reset Home to packaged defaults with other pages preserved passed")
            key("Escape"); key("Escape")
            key("Up", 30); key("Down")  # Capture the first default item, not retained bottom focus.
            captured("default")
            assert reset_verified()
            collections = collection_review(json.loads(original_bytes))
            (evidence / "result.json").write_text(json.dumps({"configuration_match": None, "component_support": None,
                "reference_sha256": reference_hash, "visual_review_required": True,
                "functional_journey_passed": True, "visual_acceptance": "pending",
                "semantic_acceptance": "not_verified", "collections": collections,
                "case": str(case),
                "settings_skin_round_trip": args.skin_from_settings,
                "saved_skin_survives_reset": bool(list((config / "skins").glob("user-*.json"))) if args.skin_from_settings else None,
                "reset_verified": True, "executable": str(executable),
                "provider_fixture": bool(collection_provider),
                "provider_requests_verified": bool(collection_provider),
                "fixture_only": ["Deterministic providers do not prove installed Kodi or playback" if collection_provider else "Individually authored media labels/art are illustrative, not provider history", "Input/guide setup routes do not qualify their backends"], "missing": collections["missing"]}, indent=2))
            print("Evidence: " + str(evidence), flush=True)
    except Exception:
        if args.inspect:
            if process and process.poll() is None:
                captured("failed")
            print("Inspect failed UI now: " + str(evidence), flush=True)
            time.sleep(25)
        raise
    finally:
        if desktop_probe and desktop_probe.poll() is None:
            desktop_probe.terminate()
            try: desktop_probe.wait(timeout=3)
            except subprocess.TimeoutExpired: desktop_probe.kill(); desktop_probe.wait()
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
        x11.XCloseDisplay(display)
        if collection_provider:
            collection_provider.close()
        if active:
            run("systemctl", "--user", "start", "isolde.service")


if __name__ == "__main__":
    main()
