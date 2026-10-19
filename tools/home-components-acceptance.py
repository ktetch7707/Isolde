#!/usr/bin/env python3
"""Exercise the built shell through X11 remote keys in a disposable profile.

Requires the VM graphical session, wmctrl and Xlib. Temporarily stops the
normal user service and restores it in finally. Never edits personal layouts.
"""
import argparse
import ctypes
import ctypes.util
import fcntl
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--inspect", action="store_true", help="Pause briefly on failure for a VM screenshot")
    parser.add_argument("--preview-seconds", type=int, default=0, choices=range(31), help="Hold the appearance preview for visual inspection")
    args = parser.parse_args()
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    executable = args.executable.resolve(strict=True)
    evidence = Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-"))
    config = evidence / "config" / "Isolde"
    config.mkdir(parents=True)
    original = {"schemaVersion": 1, "theme": {"backgroundStart": "#111A32", "backgroundEnd": "#080C18"},
                "items": [{"id": "personal-source", "kind": "app", "appId": "missing-source", "size": "wide", "opacity": 0.65}]}
    layout = config / "home-layout.json"
    original_bytes = json.dumps(original).encode()
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
                                     keycode=code, same_screen=1)
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
            process = subprocess.Popen([str(executable), "--windowed"], env=env, stdout=log, stderr=log)
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
            x11.XSetInputFocus(display, int(window, 16), 2, 0)
            x11.XFlush(display)
            time.sleep(1)
            focused = ctypes.c_ulong(); revert = ctypes.c_int()
            x11.XGetInputFocus(display, ctypes.byref(focused), ctypes.byref(revert))
            print(f"Acceptance window {window}, keyboard focus {focused.value:#x}", flush=True)
            key("Return")  # Missing tile routes to the source editor.
            key("Right", 20)
            key("Return")  # Repair overlay.
            key("Escape")  # Cancel must preserve the original.
            assert layout.read_bytes() == original_bytes
            key("Return"); key("Return")  # Reopen, bind first compatible app (Movies).
            expect(lambda: json.loads(layout.read_text())["components"][0]["appId"] == "movies", "Remote source repair and v2 migration passed")
            assert Path(str(layout) + ".v1.backup.json").read_bytes() == original_bytes
            repaired = json.loads(layout.read_text())["components"][0]
            assert repaired["id"] == "personal-source" and repaired["opacity"] == 0.65 and repaired["size"] == "wide"
            key("Escape"); key("Escape")  # Editor → Settings → Home, no Home button.
            key("Up"); key("Right"); key("Return")  # Persistent Edit Home control.
            key("Left", 17); key("Return")  # Action 20 → 3: remove final tile.
            expect(lambda: json.loads(layout.read_text())["components"] == [], "Remote removal of final launcher passed")
            key("Escape"); key("Escape")
            key("Return")  # Empty Home still exposes Edit Home.
            key("Right", 14); key("Return")  # Action 3 → 17: add clock.
            expect(lambda: json.loads(layout.read_text())["components"][0]["componentType"] == "clock", "Empty Home recovery without a Home button passed")
            key("Right", 4); key("Return")  # Action 17 → 21: appearance preview.
            before_preview = layout.read_bytes()
            key("Down", 7); key("Right", 2)
            assert layout.read_bytes() == before_preview
            print("Appearance preview ready", flush=True)
            time.sleep(args.preview_seconds)
            key("Escape")
            assert layout.read_bytes() == before_preview
            key("Return"); key("Down", 7); key("Right", 2)
            key("Down", 4); key("Return")  # Reduced motion.
            key("Down"); key("Return")  # Apply.
            expect(lambda: json.loads(layout.read_text())["theme"]["typographyScale"] == 1.1
                and json.loads(layout.read_text())["theme"]["motionEnabled"] is False,
                "Remote appearance preview, cancel and apply passed")
            assert json.loads(layout.read_text())["components"][0]["componentType"] == "clock"
            key("Escape"); key("Escape")
            (evidence / "result.json").write_text(json.dumps({"passed": True, "layout": str(layout), "executable": str(executable)}, indent=2))
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
