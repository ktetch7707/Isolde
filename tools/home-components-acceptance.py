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
    parser.add_argument("--studio", action="store_true", help="Exercise draft canvas, remote editing, history, preview and Apply")
    parser.add_argument("--desktop", action="store_true", help="Compare actual X11 pointer drag and remote saved output in the Studio fixture")
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
    # XWayland can list a window before it is focusable. Without a custom
    # error handler Xlib's default handler exits the process, which would skip
    # the finally block and leave the user service stopped.
    error_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
    x11_errors = []
    error_handler = error_type(lambda _display, _event: x11_errors.append(1) or 0)
    x11.XSetErrorHandler.argtypes = [error_type]
    x11.XSetErrorHandler(error_handler)

    def focus_window():
        # The window may not be focusable yet; activating it through the
        # window manager and retrying is enough for XSendEvent delivery.
        focused = ctypes.c_ulong(); revert = ctypes.c_int()
        for _ in range(20):
            run("wmctrl", "-i", "-a", window, env=env)
            x11.XSetInputFocus(display, int(window, 16), 2, 0)
            x11.XFlush(display)
            time.sleep(0.25)
            x11.XGetInputFocus(display, ctypes.byref(focused), ctypes.byref(revert))
            if focused.value == int(window, 16):
                return focused.value
        return focused.value

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

    def pointer(event_type, x, y, held=False):
        # Window-targeted events stay inside this disposable application's X11
        # window. KeyEvent and XButtonEvent share the same wire field layout.
        geometry = next(line.split() for line in run("wmctrl", "-lG", env=env).splitlines()
                        if line.split()[0] == window)
        origin_x, origin_y, width, height = map(int, geometry[2:6])
        px, py = round(x * width), round(y * height)
        event = Event()
        event.key = KeyEvent(type=event_type, send_event=1, display=display, window=int(window, 16),
                             x=px, y=py, x_root=origin_x+px, y_root=origin_y+py,
                             state=256 if held else 0, keycode=0 if event_type == 6 else 1, same_screen=1)
        mask = {4:4, 5:8, 6:64}[event_type]
        x11.XSendEvent(display, int(window, 16), 1, mask, ctypes.byref(event))
        x11.XFlush(display)
        time.sleep(0.15)

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
            focused = focus_window()
            print(f"Acceptance window {window}, keyboard focus {focused:#x}", flush=True)
            if args.studio:
                def capture(phase):
                    print(f"{phase.upper()}_READY {evidence}", flush=True)
                    deadline = time.monotonic() + 60
                    while not (evidence / f"{phase}-captured").exists():
                        if time.monotonic() > deadline:
                            raise RuntimeError("Capture acknowledgement timed out: " + phase)
                        if process.poll() is not None:
                            raise RuntimeError("Studio exited during capture")
                        time.sleep(0.1)

                key("Return")  # Existing missing source -> Home editor.
                capture("entry")  # Real Customize Home entry must already be the page-first Studio.
                assert layout.read_bytes() == original_bytes
                key("Down", 2); key("Return"); key("Return"); key("Return")  # Page menu Add -> conversion preview -> accept.
                assert layout.read_bytes() == original_bytes
                key("Return")  # Open the catalog after accepting explicit conversion.
                capture("catalog")
                key("Down", 2); key("Right"); key("Return"); key("Return")  # Widgets -> Clock ghost.
                key("Return")  # Add suggests the first free cell; one draft operation.
                assert layout.read_bytes() == original_bytes
                capture("edit")  # Actual full-size editing surface, without a Preview step.

                if args.desktop:
                    def apply_at(column):
                        key("Escape"); key("Up", 30); key("Down", 5); key("Return"); key("Return")
                        expect(lambda: next(item for item in json.loads(layout.read_text())["components"]
                                            if item["id"] == "clock")["placement"]["x"] == column,
                               "Desktop fixture Apply at column " + str(column))
                        return layout.read_bytes()

                    def reopen_clock():
                        key("Escape"); key("Return")  # Close/reopen the actual editor.
                        key("Down", 4); key("Return"); key("Down", 2); key("Return")  # More -> Layers.
                        key("Up", 30); key("Down"); key("Return")  # Explicit Clock, not runtime focus's source.

                    key("Return"); key("Return"); key("Right"); key("Return")
                    remote = apply_at(2)
                    reopen_clock()
                    capture("baseline")
                    key("Return"); key("Return"); key("Left"); key("Return")
                    baseline = apply_at(1)
                    reopen_clock()
                    # Fixture-relative centers: the selected Clock's full card,
                    # then one free cell right. No tiny resize handle is used.
                    start, end = (0.225, 0.246), (0.335, 0.246)
                    pointer(4, *start); pointer(6, *end, held=True)
                    capture("pointer")
                    assert layout.read_bytes() == baseline
                    key("Escape"); pointer(5, *end, held=True)
                    capture("pointercancel")
                    assert layout.read_bytes() == baseline
                    pointer(4, *start); pointer(6, *end, held=True); pointer(5, *end, held=True)
                    assert layout.read_bytes() == baseline
                    pointer_saved = apply_at(2)
                    assert pointer_saved == remote, "Pointer and remote saved documents differ"
                    capture("pointerapplied")

                    # Author the ordinary packaged 4x2 input group, then move
                    # one of its children through the actual nested canvas.
                    key("Up", 30); key("Down", 2); key("Return")
                    key("Down", 2); key("Right"); key("Return"); key("Down", 3); key("Return")
                    key("Left", 20); key("Down"); key("Return")
                    key("Escape"); key("Up", 30); key("Down", 5); key("Return"); key("Return")
                    expect(lambda: len(json.loads(layout.read_text())["components"]) == 11,
                           "Desktop nested group saved")
                    nested_baseline = layout.read_bytes()
                    panel = next(item for item in json.loads(nested_baseline)["components"]
                                 if item.get("componentType") == "container")
                    child = next(item for item in json.loads(nested_baseline)["components"]
                                 if item.get("parentId") == panel["id"] and item["placement"]["x"] == 0
                                 and item["placement"]["y"] == 0)

                    def select_child():
                        key("Up", 30); key("Down", 4); key("Return"); key("Down", 2); key("Return")
                        key("Up", 30); key("Down", 2); key("Right"); key("Down"); key("Return")

                    def save_child(column, leave_child=True):
                        if leave_child:
                            key("Escape", 2)  # Child -> parent -> page menu.
                        key("Up", 30); key("Down", 5); key("Return"); key("Return")
                        expect(lambda: next(item for item in json.loads(layout.read_text())["components"]
                                            if item["id"] == child["id"])["placement"]["x"] == column,
                               "Nested child saved at column " + str(column))
                        return layout.read_bytes()

                    select_child()
                    key("Return"); key("Return"); key("Right"); key("Return")
                    nested_remote = save_child(1)
                    select_child()
                    key("Return"); key("Return"); key("Left"); key("Return")
                    assert save_child(0) == nested_baseline, "Remote nested return did not restore all neighbors"
                    select_child()
                    nested_start, nested_end = (0.122, 0.486), (0.227, 0.486)
                    pointer(4, *nested_start); pointer(6, *nested_end, held=True)
                    capture("nestedpointer")
                    assert layout.read_bytes() == nested_baseline
                    key("Escape"); pointer(5, *nested_end, held=True)
                    capture("nestedcancel")
                    assert layout.read_bytes() == nested_baseline
                    pointer(4, *nested_start); pointer(6, *nested_end, held=True); pointer(5, *nested_end, held=True)
                    key("Escape", 2); key("Up", 30); key("Down", 4); key("Return"); key("Return")  # More -> Undo.
                    assert save_child(0, False) == nested_baseline, "Pointer Undo did not restore the complete group"
                    select_child()
                    pointer(4, *nested_start); pointer(6, *nested_end, held=True); pointer(5, *nested_end, held=True)
                    assert save_child(1) == nested_remote, "Nested pointer and remote saved documents differ"
                    capture("nestedapplied")
                    (evidence / "result.json").write_text(json.dumps({
                        "passed": True, "journey": "desktop", "executable": str(executable),
                        "pointer_remote_saved_bytes_equal": True, "held_drag_back_no_write": True,
                        "nested_pointer_remote_saved_bytes_equal": True, "nested_undo_exact": True,
                        "nested_held_drag_back_no_write": True,
                        "apply_required": True, "user_usability_acceptance": "pending"}, indent=2))
                    print("Evidence: " + str(evidence), flush=True)
                    return

                def undo_operation():
                    key("Escape"); key("Down", 2); key("Return"); key("Return")  # More -> Undo.
                    key("Up", 2); key("Right")  # Restore Add menu origin and the selected canvas item.
                    assert layout.read_bytes() == original_bytes

                key("Return"); key("Return"); key("Left")  # Moving into occupied space previews neighbors making room.
                capture("reflow")
                key("Escape")
                assert layout.read_bytes() == original_bytes
                key("Return"); key("Return"); key("Left"); key("Return")
                undo_operation()  # The whole reflow is one undo operation.

                key("Return"); key("Down"); key("Return")  # Edit component.
                key("Down", 2); key("Return"); key("Return")  # Layout -> Size sliders.
                key("Right")  # Width slider increases with live preview.
                capture("resize")
                key("Escape"); key("Escape"); key("Escape")  # Cancel size, fields, inspector.
                assert layout.read_bytes() == original_bytes
                key("Return"); key("Return"); key("Right", 20); key("Escape")
                assert layout.read_bytes() == original_bytes

                key("Escape"); key("Down", 2); key("Return"); key("Return")  # Pages -> More -> Undo.
                key("Return"); key("Down"); key("Return")  # More -> Redo.
                key("Return"); key("Down", 4); key("Return")  # Optional Try interaction.
                assert layout.read_bytes() == original_bytes
                capture("studio")
                key("Escape")
                assert layout.read_bytes() == original_bytes
                key("Down"); key("Return"); key("Return")  # Apply summary -> atomic Apply.
                expect(lambda: len(json.loads(layout.read_text()).get("components", [])) == 2,
                       "Studio page menu/direct editing/history/Try interaction/Apply passed")
                applied = layout.read_bytes()
                assert Path(str(layout) + ".v1.backup.json").read_bytes() == original_bytes
                assert all("placement" in item for item in json.loads(applied)["components"])
                placed = {item["id"]: item["placement"] for item in json.loads(applied)["components"]}
                assert placed["personal-source"] == {"x": 0, "y": 0, "width": 1, "height": 1}
                assert placed["clock"] == {"x": 1, "y": 0, "width": 1, "height": 1}
                key("Escape"); key("Return")  # Close and reopen Studio from the legacy entry.
                assert layout.read_bytes() == applied
                capture("applied")
                key("Down", 2); key("Return"); key("Down", 2); key("Right"); key("Return"); key("Down", 3); key("Return")  # Widgets -> packaged eight-slot group.
                key("Left", 2); key("Down"); key("Return")  # Place below the original components, with room beside the inspector.
                assert layout.read_bytes() == applied
                capture("group")
                key("Return"); key("Down"); key("Return")  # Context -> Inspector.
                capture("inspector")  # Controls fit beside this fixture group.
                key("Return")  # Content properties.
                capture("content")
                key("Escape")  # Return to property groups.
                key("Down", 5); key("Return"); key("Down", 7); key("Return")  # More -> Edit group contents.
                assert layout.read_bytes() == applied
                capture("inside")
                key("Escape")  # Exact containing group selection.
                key("Escape"); key("Down", 2); key("Return"); key("Down", 2); key("Return")  # More -> Layers.
                key("Down", 2); key("Right")  # Expand the group without changing its data.
                capture("layers")
                assert layout.read_bytes() == applied
                key("Escape"); key("Down"); key("Return"); key("Return")  # Layers returns to the still-open page menu.
                expect(lambda: len(json.loads(layout.read_text()).get("components", [])) == 11,
                       "Nested input slots render and apply atomically")
                grouped = json.loads(layout.read_text())["components"]
                panel = next(item for item in grouped if item.get("componentType") == "container")
                children = [item for item in grouped if item.get("parentId") == panel["id"]]
                assert len(children) == 8 and panel["requiredCapability"] == "capture-inputs"
                assert panel["presentation"]["layout"]["columns"] == 4
                assert panel["presentation"]["layout"]["rows"] == 2
                assert {(item["placement"]["x"], item["placement"]["y"]) for item in children} == {(x, y) for y in range(2) for x in range(4)}
                grouped_bytes = layout.read_bytes()
                key("Up", 2); key("Return")  # Page appearance from the Apply menu origin.
                key("Down", 2); key("Return"); key("Return")  # Layout -> scrolling checkbox.
                key("Return")  # Preview enabled; no saved setting yet.
                capture("scrollingoption")
                assert layout.read_bytes() == grouped_bytes
                key("Down"); key("Return"); key("Escape", 2)  # Done -> canvas.
                key("Escape"); key("Down"); key("Return"); key("Down", 2); key("Return")  # More -> Layers.
                key("Down"); key("Return")  # Select Clock by its stable layer order.
                key("Return"); key("Return"); key("Down", 15)
                capture("scrolling")  # Three viewport heights below the first row, same card size.
                assert layout.read_bytes() == grouped_bytes
                key("Escape")  # Complete gesture/viewport cancellation.
                key("Return"); key("Return"); key("Down", 15); key("Return")
                key("Escape"); key("Down"); key("Return"); key("Return")  # Atomic Apply.
                expect(lambda: json.loads(layout.read_text())["composition"].get("verticalScrolling") is True,
                       "Scrolling page saved through the normal Studio checkbox")
                scrolling_document = json.loads(layout.read_text())
                assert next(item for item in scrolling_document["components"] if item["id"] == "clock")["placement"]["y"] == 15
                key("Escape", 2); key("Down", 2)  # Close Studio and Settings; runtime navigation reveals the offscreen Clock.
                capture("scrollinghome")
                (evidence / "result.json").write_text(json.dumps({
                    "passed": True, "journey": "studio", "layout": str(layout),
                    "executable": str(executable), "preview_cancel_preserved_bytes": True,
                    "reopen_preserved_bytes": True, "user_usability_acceptance": "pending",
                    "remote_collision_cancel_swap_undo": True, "remote_resize_cancel_accept_undo": True,
                    "bounded_repeat_cancel": True, "nested_group_remote_layers_apply": True,
                    "scrolling_checkbox_preview_cancel_apply": True, "three_viewport_page": True,
                    "eight_slot_layout_only": True, "physical_input_qualification": "not claimed"
                }, indent=2))
                print("Evidence: " + str(evidence), flush=True)
                return
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
