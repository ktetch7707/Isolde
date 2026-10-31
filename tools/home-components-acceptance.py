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
    parser.add_argument("--collections", action="store_true", help="Exercise real collection registry/session lifecycle in a private profile")
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
    if args.collections:
        original_bytes, collection_fixture = prepare_collections(evidence, executable)
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
            if args.collections:
                exercise_collections(evidence, executable, layout, original_bytes, collection_fixture, key, pointer, run, env, lambda: process.poll(), window)
                return
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

                def page_menu(index):
                    # The named history row sits above the first page-menu item.
                    key("Up", 30); key("Down"); key("Down", index); key("Return")

                if args.desktop:
                    def apply_at(column):
                        key("Escape"); page_menu(7); key("Return")
                        expect(lambda: next(item for item in json.loads(layout.read_text())["components"]
                                            if item["id"] == "clock")["placement"]["x"] == column,
                               "Desktop fixture Apply at column " + str(column))
                        return layout.read_bytes()

                    def reopen_clock():
                        key("Escape"); key("Return")  # Close/reopen the actual editor.
                        page_menu(4)  # Named Layers.
                        key("Up", 30); key("Down"); key("Return")  # Explicit Clock, not runtime focus's source.

                    key("Return"); key("Return"); key("Right"); key("Return")
                    remote = apply_at(2)
                    reopen_clock()
                    key("Return"); key("Return"); key("Left"); key("Return")
                    baseline = apply_at(1)
                    reopen_clock()
                    capture("baseline")
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
                    reopen_clock()
                    capture("pointerapplied")

                    # Author the ordinary packaged 4x2 input group, then move
                    # one of its children through the actual nested canvas.
                    key("Escape"); page_menu(2)
                    key("Down", 2); key("Right"); key("Return"); key("Down", 3); key("Return")
                    key("Left", 20); key("Down"); key("Return")
                    key("Escape"); page_menu(7); key("Return")
                    expect(lambda: len(json.loads(layout.read_text())["components"]) == 11,
                           "Desktop nested group saved")
                    nested_baseline = layout.read_bytes()
                    panel = next(item for item in json.loads(nested_baseline)["components"]
                                 if item.get("componentType") == "container")
                    child = next(item for item in json.loads(nested_baseline)["components"]
                                 if item.get("parentId") == panel["id"] and item["placement"]["x"] == 0
                                 and item["placement"]["y"] == 0)

                    def select_child():
                        page_menu(4)
                        key("Up", 30); key("Down", 2); key("Right"); key("Down"); key("Return")

                    def save_child(column, leave_child=True):
                        if leave_child:
                            key("Escape", 2)  # Child -> parent -> page menu.
                        page_menu(7); key("Return")
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
                    key("Escape", 2); key("Up", 30); key("Left"); key("Return"); key("Down")  # Named Undo.
                    assert save_child(0, False) == nested_baseline, "Pointer Undo did not restore the complete group"
                    select_child()
                    pointer(4, *nested_start); pointer(6, *nested_end, held=True); pointer(5, *nested_end, held=True)
                    assert save_child(1) == nested_remote, "Nested pointer and remote saved documents differ"
                    select_child()
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
                    key("Escape"); key("Up", 30); key("Left"); key("Return")  # Named Undo above the page menu.
                    key("Down"); key("Down", 2); key("Right")  # Add menu origin; same selected canvas item.
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

                key("Escape"); key("Up", 30); key("Left"); key("Return")  # Named Undo.
                key("Right"); key("Return"); key("Down")  # Named Redo, then page menu.
                page_menu(7); key("Down", 2); key("Return")  # Review & Apply -> Try interaction.
                assert layout.read_bytes() == original_bytes
                capture("studio")
                key("Escape")
                assert layout.read_bytes() == original_bytes
                key("Return"); key("Return")  # Same Review & Apply origin -> atomic Apply.
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
                key("Escape"); page_menu(4)  # Named Layers.
                key("Down", 2); key("Right")  # Expand the group without changing its data.
                capture("layers")
                assert layout.read_bytes() == applied
                key("Escape"); page_menu(7); key("Return")  # Review & Apply from the still-open page menu.
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
                page_menu(3)  # Named Page appearance from the Apply menu origin.
                key("Down", 2); key("Return"); key("Return")  # Layout -> scrolling checkbox.
                key("Return")  # Preview enabled; no saved setting yet.
                capture("scrollingoption")
                assert layout.read_bytes() == grouped_bytes
                key("Down"); key("Return"); key("Escape", 2)  # Done -> canvas.
                key("Escape"); page_menu(4)  # Named Layers.
                key("Down"); key("Return")  # Select Clock by its stable layer order.
                key("Return"); key("Return"); key("Down", 15)
                capture("scrolling")  # Three viewport heights below the first row, same card size.
                assert layout.read_bytes() == grouped_bytes
                key("Escape")  # Complete gesture/viewport cancellation.
                key("Return"); key("Return"); key("Down", 15); key("Return")
                key("Escape"); page_menu(7); key("Return")  # Atomic Review & Apply.
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
            if args.collections:
                print("COLLECTIONFAILURE_READY "+str(evidence),flush=True)
                deadline=time.monotonic()+60
                while not (evidence/"collectionfailure-captured").exists() and time.monotonic()<deadline:time.sleep(.1)
            else:
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


# Collection acceptance stays in this helper so the SHA-addressed overlay
# cannot import an unfrozen or accidentally modified dependency.
def prepare_collections(evidence, executable):
    repository = Path(__file__).resolve().parents[1]
    presets = json.loads((repository / "config/studio-defaults.json").read_text())["componentPresets"]
    def collection(identity, y, mode, ids, capacity):
        import copy
        item = copy.deepcopy(presets["app-bar"])
        item.update(id=identity, name=identity, placement=dict(x=0,y=y,width=8,height=1))
        item["appCollection"] = dict(mode=mode, ids=ids, showStore=False)
        item["presentation"]["layout"]["visibleColumns"] = capacity
        item["presentation"]["showHeading"] = False
        return item
    items = [collection("selected-apps",0,"selected",["youtube","freetube"],1),
             collection("all-apps",1,"all",[],2)]
    import copy
    buttons=copy.deepcopy(presets["carousel"])
    buttons.update(id="buttons",name="Shortcuts",placement=dict(x=0,y=2,width=8,height=1))
    buttons["presentation"]["layout"].update(columns=3,visibleColumns=1)
    buttons["presentation"]["showHeading"]=False
    items.append(buttons)
    for i,(identity,name,action) in enumerate([
            ("button-store","App Store",dict(type="launch-app",appId="app-store")),
            ("button-client","Session client",dict(type="launch-app",appId="freetube")),
            ("button-unassigned","Unassigned",None)]):
        item=dict(id=identity,name=name,kind="widget",widgetType="button",componentType="button",parentId="buttons",placement=dict(x=i,y=0,width=1,height=1))
        if action:item["action"]=action
        items.append(item)
    items.append(dict(id="store",name="App Store",kind="widget",widgetType="button",componentType="button",placement=dict(x=0,y=3,width=2,height=1),action=dict(type="launch-app",appId="app-store")))
    history=copy.deepcopy(presets["recently-viewed"])
    history.update(id="app-openings",name="App openings",placement=dict(x=0,y=4,width=8,height=1),
        source=dict(schemaVersion=1,provider='kodi',kind='recently-viewed',libraryKind='history',libraryView='watched',libraryValue=''))
    history["presentation"].update(historyApps=["freetube"],libraryLimit=4,libraryColumns=2,libraryChrome=False,historySourceBadges=True)
    items.append(history)
    for item in items:
        item["schemaVersion"]=1
        if item["id"]!="app-openings":item["source"]=dict(schemaVersion=1,provider="shell",kind=item["componentType"])
    document=dict(schemaVersion=2,rootPage=dict(name="Home",visible=True,position=0),theme=dict(backgroundStart="#19191E",backgroundEnd="#101014",textColor="#FFFFFF",focusColor="#FFD166",surfaceColor="#28262F",motionEnabled=False),categories=[],components=items,
        composition=dict(schemaVersion=1,enabled=True,grid=dict(columns=8,rows=5,mode="screen"),headerControls=["settings"],showClock=False,navigation=[dict(id="home",name="Home")],initialTab="home"))
    package=repository/"config/packages/freetube/manifest.json"
    destination=evidence/"data/Isolde/packages/profiles/default/freetube/1.0.0/manifest.json"
    destination.parent.mkdir(parents=True);destination.write_bytes(package.read_bytes())
    state=evidence/"config/Isolde/profiles/default/installed-apps.json"
    state.parent.mkdir(parents=True)
    state.write_text(json.dumps(dict(installed=["freetube"],packageStates=dict(freetube="ready"))))
    home=evidence/"data/Isolde/native-profiles/default/freetube/home"
    home.mkdir(parents=True,mode=0o700)
    for directory in [home.parent,home.parent.parent,home.parent.parent.parent]:directory.chmod(0o700)
    client=home/"collection-client.py"
    client.write_text(COLLECTION_CLIENT)
    desktop=evidence/"data/applications/freetube.desktop";desktop.parent.mkdir(parents=True)
    desktop.write_text("[Desktop Entry]\nType=Application\nName=Collection session boundary client\nExec=/usr/bin/python3 "+str(Path.home()/"collection-client.py")+"\nTerminal=false\n")
    catalog=json.loads((repository/"config/catalog.json").read_text())
    native=[]
    for relative in catalog["packages"]:
        manifest=json.loads((repository/"config"/relative).read_text())
        app=manifest["app"]
        if app["type"] not in ("android","web") and app.get("presentationMode")!="web-kiosk":native.append(app["id"])
    return json.dumps(document).encode(),dict(state=state,home=home,nativeIndex=native.index("freetube"),document=document)


COLLECTION_CLIENT = r'''
import ctypes as c, ctypes.util, os, json
from pathlib import Path
x=c.CDLL(ctypes.util.find_library("X11"))
x.XOpenDisplay.argtypes=[c.c_char_p];x.XOpenDisplay.restype=c.c_void_p
x.XDefaultRootWindow.argtypes=[c.c_void_p];x.XDefaultRootWindow.restype=c.c_ulong
x.XCreateSimpleWindow.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_uint,c.c_ulong,c.c_ulong];x.XCreateSimpleWindow.restype=c.c_ulong
for name in ["XMapWindow","XDestroyWindow","XClearWindow"]:getattr(x,name).argtypes=[c.c_void_p,c.c_ulong]
x.XStoreName.argtypes=[c.c_void_p,c.c_ulong,c.c_char_p]
x.XSelectInput.argtypes=[c.c_void_p,c.c_ulong,c.c_long]
x.XFlush.argtypes=[c.c_void_p]
x.XDefaultGC.argtypes=[c.c_void_p,c.c_int];x.XDefaultGC.restype=c.c_void_p
x.XSetForeground.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
x.XDrawString.argtypes=[c.c_void_p,c.c_ulong,c.c_void_p,c.c_int,c.c_int,c.c_char_p,c.c_int]
x.XNextEvent.argtypes=[c.c_void_p,c.c_void_p]
x.XLookupKeysym.argtypes=[c.c_void_p,c.c_int];x.XLookupKeysym.restype=c.c_ulong
class Hint(c.Structure):_fields_=[("res_name",c.c_char_p),("res_class",c.c_char_p)]
x.XSetClassHint.argtypes=[c.c_void_p,c.c_ulong,c.POINTER(Hint)]
d=x.XOpenDisplay(None)
if not d:raise RuntimeError("No isolated display")
w=x.XCreateSimpleWindow(d,x.XDefaultRootWindow(d),80,80,900,480,2,0xFFD166,0x202634)
x.XSetClassHint(d,w,c.byref(Hint(b"freetube",b"FreeTube")))
x.XStoreName(d,w,b"Isolde collection client - Right changes state, Escape returns")
x.XSelectInput(d,w,1|32768);x.XMapWindow(d,w);x.XFlush(d)
p=Path.home()/"collection-client-state.json"
previous=json.loads(p.read_text()) if p.exists() else {}
def save():
 temporary=p.with_suffix(".tmp");temporary.write_text(json.dumps(state));temporary.replace(p)
state=dict(launches=previous.get("launches",0)+1,inputs=0,exited=False,namespace=os.environ.get("ISOLDE_NATIVE_NAMESPACE"),home=str(Path.home()),window=int(w))
save()
event=(c.c_long*24)()
gc=x.XDefaultGC(d,0);x.XSetForeground(d,gc,0xFFFFFF)
def paint():
 x.XClearWindow(d,w)
 for y,text in [(70,"ISOLDE COLLECTION SESSION BOUNDARY"),(120,"Real native window / isolated profile"),(170,"Right presses: "+str(state["inputs"])),(220,"Escape exits normally and returns to the same gallery item")]:
  message=text.encode();x.XDrawString(d,w,gc,50,y,message,len(message))
 x.XFlush(d)
while True:
 x.XNextEvent(d,c.byref(event))
 if c.cast(c.byref(event),c.POINTER(c.c_int))[0]==12:paint();continue
 if c.cast(c.byref(event),c.POINTER(c.c_int))[0]!=2:continue
 key=x.XLookupKeysym(c.byref(event),0)
 if key==0xff53:state["inputs"]+=1;x.XStoreName(d,w,("Isolde collection client - inputs "+str(state["inputs"])).encode());save();paint()
 if key==0xff1b:
  state["exited"]=True;save();break
x.XDestroyWindow(d,w);x.XFlush(d)
'''


def exercise_collections(evidence, executable, layout, original, fixture, key, pointer, run, env, poll, shell_window):
    phases=[]
    def expect(predicate,message,timeout=30):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            if poll() is not None:raise RuntimeError("Collection shell exited")
            if predicate():return
            time.sleep(.15)
        raise RuntimeError(message)
    def capture(phase):
        phases.append(phase);print(phase.upper()+"_READY "+str(evidence),flush=True)
        expect(lambda:(evidence/(phase+"-captured")).exists(),"Capture acknowledgement: "+phase,60)
    receipt=fixture["home"]/"collection-client-state.json"
    def state():return json.loads(receipt.read_text()) if receipt.exists() else {}
    def native_cycle(number,phase):
        expect(lambda:state().get("launches")==number and not state().get("exited"),"Real isolated client did not launch")
        assert state()["namespace"]=="1"
        # Address the actual client window, preserving the shell key target.
        client_window=hex(state()["window"])
        run("wmctrl","-i","-a",client_window,env=env)
        send_client_key(client_window,"Right",env)
        expect(lambda:state().get("inputs")==1,"Native client did not receive Right")
        capture(phase)
        send_client_key(client_window,"Escape",env)
        expect(lambda:state().get("exited") is True,"Client did not exit normally")
        expect(lambda:'Isolated native session finished "freetube" 0' in (evidence/"shell.log").read_text(),"SessionManager did not observe normal native exit")
        time.sleep(1);run("wmctrl","-i","-a",shell_window,env=env)
        assert layout.read_bytes()==original
    def top():
        key("Up",12);key("Down",2)
    def store():
        top();key("Down",3);key("Return");time.sleep(1)
        # Initial Android grid -> header -> Device apps, then catalog order.
        key("Up");key("Left");key("Return")
        index=fixture["nativeIndex"]
        key("Down",index//2)
        if index%2:key("Right")
        key("Return")
    capture("collectionhome")
    top();key("Return");capture("selectedgallery")
    key("Right");key("Return");native_cycle(1,"collectionclient")
    capture("selectedreturn")
    # Confirm without navigation must launch the exact remembered gallery app.
    key("Return");native_cycle(2,"collectionrepeat")
    key("Escape");key("Escape");key("Down",2);key("Return");capture("buttongallery")
    key("Right");key("Return");native_cycle(3,"buttonclient")
    capture("buttonreturn")
    key("Return");native_cycle(4,"buttonrepeat")
    key("Escape");key("Escape")
    store();native_cycle(5,"storeclient");capture("collectionuninstall")
    key("Down");key("Return");key("Right");key("Return")
    expect(lambda:"freetube" not in json.loads(fixture["state"].read_text())["installed"],"Store removal failed")
    key("Escape",2);top();key("Down");capture("allremoved")
    key("Up");key("Return");capture("selectedmissing")
    key("Right");key("Return");assert state()["launches"]==5
    capture("missingrefused");key("Escape");key("Escape")
    top();key("Down",3);key("Return");key("Return");key("Right");key("Return")
    expect(lambda:"freetube" in json.loads(fixture["state"].read_text())["installed"],"Store reinstall failed")
    key("Escape",2);top();key("Down");capture("allrestored")
    assert layout.read_bytes()==original,"Registry installation authored unexpected page placements"
    key("Up");key("Return");key("Right");key("Return");native_cycle(6,"reinstalledclient")
    capture("reinstalledreturn")
    key("Escape",2);top();key("Down",4);capture("historyapp")
    key("Return");native_cycle(7,"historyclient");capture("historyreturned")
    key("Return");native_cycle(8,"historyrepeat");capture("historyrepeatreturn")
    (evidence/"result.json").write_text(json.dumps(dict(passed=True,journey="collections",executable=str(executable),freshProfile=True,actualNativeInput=True,normalExit=True,exactGalleryReturn=True,buttonReturn=True,storeRemoveReinstall=True,layoutUnchanged=True,appHistoryVerified=True,appHistorySessionOnly=True,appHistoryReopenCycles=2,physicalHardwareVerified=False,thirdPartyClientVerified=False,captures=phases,clientState=state()),indent=2))
    print("Evidence: "+str(evidence),flush=True)


def send_client_key(window,name,env):
    # Client input uses its X11 window, never compositor/global shortcuts.
    display_api=ctypes.CDLL(ctypes.util.find_library("X11"))
    display_api.XOpenDisplay.argtypes=[ctypes.c_char_p];display_api.XOpenDisplay.restype=ctypes.c_void_p
    display_api.XStringToKeysym.argtypes=[ctypes.c_char_p];display_api.XStringToKeysym.restype=ctypes.c_ulong
    display_api.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];display_api.XKeysymToKeycode.restype=ctypes.c_uint
    display_api.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.c_void_p]
    display_api.XFlush.argtypes=[ctypes.c_void_p];display_api.XCloseDisplay.argtypes=[ctypes.c_void_p]
    class Key(ctypes.Structure):
        _fields_=[("type",ctypes.c_int),("serial",ctypes.c_ulong),("send_event",ctypes.c_int),("display",ctypes.c_void_p),("window",ctypes.c_ulong),("root",ctypes.c_ulong),("subwindow",ctypes.c_ulong),("time",ctypes.c_ulong),("x",ctypes.c_int),("y",ctypes.c_int),("x_root",ctypes.c_int),("y_root",ctypes.c_int),("state",ctypes.c_uint),("keycode",ctypes.c_uint),("same_screen",ctypes.c_int)]
    d=display_api.XOpenDisplay(env["DISPLAY"].encode())
    if not d:raise RuntimeError("Cannot open client display")
    try:
        code=display_api.XKeysymToKeycode(d,display_api.XStringToKeysym(name.encode()))
        for kind,mask in [(2,1),(3,2)]:
            event=Key(type=kind,send_event=1,display=d,window=int(window,16),keycode=code,same_screen=1)
            display_api.XSendEvent(d,int(window,16),1,mask,ctypes.byref(event))
        display_api.XFlush(d)
    finally:display_api.XCloseDisplay(d)


if __name__ == "__main__":
    main()
