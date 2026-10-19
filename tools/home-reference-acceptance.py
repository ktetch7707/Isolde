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
import shutil
from pathlib import Path
import subprocess
import tempfile
import time


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


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
    env.update(XDG_CONFIG_HOME=str(evidence / "config"), XDG_DATA_HOME=str(evidence / "data"),
               XDG_CACHE_HOME=str(evidence / "cache"), QT_QPA_PLATFORM="xcb")
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

    def expect(predicate, message):
        deadline = time.monotonic() + 8
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
            if args.skin_from_settings:
                # Begin with the packaged default. No Home layout is injected.
                assert not layout.exists()
                key("Up",30); key("Right"); key("Return")  # Recovery -> Edit Home.
                key("Right",23); key("Return")  # Settings -> Home skins.
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
            captured("custom")
            if args.skin_from_settings:
                key("Up",30); key("Left",3)
                if case.name == "home-pastel-reference": key("Right")
                key("Return")  # Header Settings, with remembered Settings selection.
                key("Up",10); key("Down"); key("Return")  # Select Home explicitly.
                key("Left",30)  # Home also remembers its last action.
            else:
                for name in interaction.get("editKeys",["Up","Up","Right","Return"]): key(name)
            key("Right", 22); key("Return")  # New remote component-layout editor.
            key("Escape")  # Opening/closing the editor must preserve the config.
            assert layout.read_bytes() == original_bytes
            if args.skin_from_settings:
                key("Right",2); key("Return")  # Detailed structured editor.
                if args.capture_library: captured("editor")
                key("Escape"); key("Left",2)
                assert layout.read_bytes() == original_bytes
            key("Left", 3); key("Return")  # Existing Restore defaults operation (22 → 19).
            expect(lambda: not layout.exists(), "Reset to packaged defaults passed")
            key("Escape"); key("Escape")
            key("Up", 30); key("Down")  # Capture the first default item, not retained bottom focus.
            captured("default")
            assert not layout.exists()
            (evidence / "result.json").write_text(json.dumps({"configuration_match": None, "component_support": True,
                "reference_sha256": reference_hash, "visual_review_required": True,
                "case": str(case),
                "settings_skin_round_trip": args.skin_from_settings,
                "saved_skin_survives_reset": bool(list((config / "skins").glob("user-*.json"))) if args.skin_from_settings else None,
                "reset_verified": True, "executable": str(executable),
                "fixture_only": ["Movie labels/progress are visual fixtures; launch opens the real library", "HDMI 4 and guide open setup; real backends remain unqualified"], "missing": []}, indent=2))
            print("Evidence: " + str(evidence), flush=True)
    except Exception:
        if args.inspect:
            print("Inspect failed UI now: " + str(evidence), flush=True)
            time.sleep(25)
        raise
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
        x11.XCloseDisplay(display)
        if active:
            run("systemctl", "--user", "start", "isolde.service")


if __name__ == "__main__":
    main()
