#!/usr/bin/env python3
"""Exercise the built shell through X11 remote keys in a disposable profile.

Requires the VM graphical session, wmctrl and Xlib. Temporarily stops the
normal user service and restores it in finally. Never edits personal layouts.
"""
import argparse
import ctypes
import ctypes.util
import fcntl
import hashlib
import importlib.util
import re
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


def studio_page_menu(layout, key, action, extra_pages=0):
    """Use the shipped named menu order, including T22 reuse and T23 safety."""
    document = json.loads(layout.read_text())
    pages = len(document.get("pages", [])) + int(document.get("rootPage", {}).get("visible", True)) + extra_pages
    offsets = {"add": 0, "appearance": 1, "layers": 2, "reuse": 3,
               "designs": 4, "editorSettings": 5, "safety": 6, "apply": 7, "edit": 8}
    index = 0 if action == "new" else 1 if action == "page" else 1 + pages + offsets[action]
    key("Up", 30); key("Down"); key("Down", index); key("Return")


def studio_legacy_menu(layout, key, index):
    actions = {0:"new", 1:"page", 2:"add", 3:"appearance", 4:"layers",
               5:"designs", 6:"editorSettings", 7:"apply", 8:"edit"}
    studio_page_menu(layout, key, actions[index])


def capture_x11_window(x11, display, window, path):
    """Capture the owned installed window; XWayland's root drawable is not readable."""
    from PIL import Image
    class Frame(ctypes.Structure):
        _fields_=[("width",ctypes.c_int),("height",ctypes.c_int),("xoffset",ctypes.c_int),("format",ctypes.c_int),
            ("data",ctypes.c_void_p),("byte_order",ctypes.c_int),("bitmap_unit",ctypes.c_int),
            ("bitmap_bit_order",ctypes.c_int),("bitmap_pad",ctypes.c_int),("depth",ctypes.c_int),
            ("bytes_per_line",ctypes.c_int),("bits_per_pixel",ctypes.c_int),
            ("red_mask",ctypes.c_ulong),("green_mask",ctypes.c_ulong),("blue_mask",ctypes.c_ulong)]
    x11.XGetGeometry.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(ctypes.c_ulong),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint)]
    x11.XGetGeometry.restype=ctypes.c_int
    owner=ctypes.c_ulong();left=ctypes.c_int();top=ctypes.c_int()
    width=ctypes.c_uint();height=ctypes.c_uint();border=ctypes.c_uint();depth=ctypes.c_uint()
    if not x11.XGetGeometry(display,window,ctypes.byref(owner),ctypes.byref(left),ctypes.byref(top),ctypes.byref(width),ctypes.byref(height),ctypes.byref(border),ctypes.byref(depth)):
        raise RuntimeError("Cannot measure owned X11 acceptance window")
    if not (0<width.value<=3840 and 0<height.value<=2160):raise RuntimeError("Unsupported acceptance window bounds")
    x11.XGetImage.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_uint,ctypes.c_uint,ctypes.c_ulong,ctypes.c_int]
    x11.XGetImage.restype=ctypes.POINTER(Frame)
    x11.XDestroyImage.argtypes=[ctypes.POINTER(Frame)];x11.XDestroyImage.restype=ctypes.c_int
    frame=x11.XGetImage(display,window,0,0,width.value,height.value,ctypes.c_ulong(-1).value,2)
    if not frame:raise RuntimeError("Cannot read owned X11 acceptance window")
    try:
        data=frame.contents
        if data.byte_order!=0 or data.bits_per_pixel not in (24,32) or (data.red_mask,data.green_mask,data.blue_mask)!=(0xff0000,0xff00,0xff):
            raise RuntimeError("Unsupported X11 acceptance pixel format")
        if data.width!=width.value or data.height!=height.value or not (data.width*data.bits_per_pixel//8<=data.bytes_per_line<=data.width*4+64):
            raise RuntimeError("Invalid X11 acceptance image geometry")
        raw=ctypes.string_at(data.data,data.bytes_per_line*data.height)
        Image.frombytes("RGB",(data.width,data.height),raw,"raw","BGRX" if data.bits_per_pixel==32 else "BGR",data.bytes_per_line,1).save(path)
    finally:x11.XDestroyImage(frame)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--inspect", action="store_true", help="Pause briefly on failure for a VM screenshot")
    parser.add_argument("--studio", action="store_true", help="Exercise draft canvas, remote editing, history, preview and Apply")
    parser.add_argument("--catalog", action="store_true", help="Exercise real remote image import, framing, GIF Apply and reopen")
    parser.add_argument("--collections", action="store_true", help="Exercise real collection registry/session lifecycle in a private profile")
    parser.add_argument("--themes-controls-only", action="store_true", help="Review Audio/Ambient values and explicit private draft discard")
    parser.add_argument("--themes-custom-only", action="store_true", help="Complete only the custom palette and private policy controls")
    parser.add_argument("--themes-faults-only", action="store_true", help="Review installed loading, RPC error and retry under three palettes")
    parser.add_argument("--themes-only", action="store_true", help="Review shared shell surfaces under three private Home/origin palettes")
    parser.add_argument("--search-only", action="store_true", help="Verify denied-network local Search and exact recovery without replaying Studio design workflows")
    parser.add_argument("--designs-only", action="store_true", help="Exercise templates, reuse, portability, reference imagery and restore in a separate bounded journey")
    parser.add_argument("--desktop", action="store_true", help="Compare actual X11 pointer drag and remote saved output in the Studio fixture")
    parser.add_argument("--preview-seconds", type=int, default=0, choices=range(31), help="Hold the appearance preview for visual inspection")
    args = parser.parse_args()
    if args.themes_faults_only or args.themes_custom_only or args.themes_controls_only: args.themes_only = True
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    executable = args.executable.resolve(strict=True)
    evidence = Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-"))
    config = evidence / "config" / "Isolde"
    config.mkdir(parents=True)
    original = {"schemaVersion": 1, "theme": {"backgroundStart": "#111A32", "backgroundEnd": "#080C18"},
                "items": [{"id": "personal-source", "kind": "app", "appId": "missing-source", "size": "wide", "opacity": 0.65}]}
    layout = config / "home-layout.json"
    if args.designs_only:
        original["composition"]={"schemaVersion":1,"enabled":True,"grid":{"columns":4,"rows":3,"mode":"screen"}}
        original["items"][0].update(name="Design source",placement={"x":0,"y":0,"width":1,"height":1},presentation={"labelColor":"#e89ab0","radius":16})
    original_bytes = json.dumps(original).encode()
    if args.collections:
        original_bytes, collection_fixture = prepare_collections(evidence, executable)
    if args.catalog:
        prepare_catalog(evidence)
    theme_provider = prepare_theme_provider(evidence) if args.themes_only else None
    if args.themes_only:
        original = theme_document(2 if args.themes_custom_only else 0)
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

    def focus_window(target_window):
        # The window may not be focusable yet; activating it through the
        # window manager and retrying is enough for XSendEvent delivery.
        focused = ctypes.c_ulong(); revert = ctypes.c_int()
        for _ in range(20):
            run("wmctrl", "-i", "-a", target_window, env=env)
            x11.XSetInputFocus(display, int(target_window, 16), 2, 0)
            x11.XFlush(display)
            time.sleep(0.25)
            x11.XGetInputFocus(display, ctypes.byref(focused), ctypes.byref(revert))
            if focused.value == int(target_window, 16):
                return focused.value
        raise RuntimeError("The private acceptance window could not acquire focus")

    input_trace = []
    capture_trace = []
    journey_started = time.monotonic()

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
            if len(input_trace) >= 5000: raise RuntimeError("Acceptance input trace exceeded its bound")
            input_trace.append({"kind":"key", "name":name, "elapsedSeconds":time.monotonic()-journey_started})
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
        if len(input_trace) >= 5000: raise RuntimeError("Acceptance input trace exceeded its bound")
        input_trace.append({"kind":"pointer", "type":event_type, "x":x, "y":y, "elapsedSeconds":time.monotonic()-journey_started})
        mask = {4:4, 5:8, 6:64}[event_type]
        x11.XSendEvent(display, int(window, 16), 1, mask, ctypes.byref(event))
        x11.XFlush(display)
        time.sleep(0.15)

    offline_module = None
    if args.designs_only or args.search_only:
        # Reuse the frozen production-boundary fixture; deny only the child,
        # preserving the oracle transport and local display/session sockets.
        module_path = Path(__file__).with_name("guide-soak-acceptance.py")
        spec = importlib.util.spec_from_file_location("studio_offline_fixture", module_path)
        offline_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(offline_module)
    active = subprocess.run(["systemctl", "--user", "is-active", "--quiet", "isolde.service"]).returncode == 0
    process = None
    try:
        if active:
            run("systemctl", "--user", "stop", "isolde.service")
        with (evidence / "shell.log").open("w") as log:
            def start_shell():
                command = [str(executable), "--windowed"]
                child_options = {}
                if offline_module is not None:
                    command = [sys.executable, "-c", offline_module.SOCKET_PROBE, str(evidence), str(executable)]
                    child_options["preexec_fn"] = offline_module.install_socket_filter
                child = subprocess.Popen(command, env=env, stdout=log, stderr=log, **child_options)
                try:
                    child_window = None
                    for _ in range(80):
                        try:
                            listing=run("wmctrl", "-lp", env=env)
                        except subprocess.CalledProcessError as error:
                            diagnostic=(error.stdout or "")+(error.stderr or "")
                            if error.returncode!=1 or (diagnostic.strip() and "Cannot get client list properties" not in diagnostic):
                                print("WMCTRL_DISCOVERY_ERROR "+repr({"code":error.returncode,"stdout":error.stdout,"stderr":error.stderr}),flush=True)
                                raise
                            listing=""  # No client list during bounded private-window startup.
                        for line in listing.splitlines():
                            fields = line.split(None, 4)
                            if len(fields) >= 3 and fields[2] == str(child.pid):
                                child_window = fields[0]
                                break
                        if child_window:
                            break
                        if child.poll() is not None:
                            raise RuntimeError("The shell failed to start; " + str(evidence))
                        time.sleep(0.1)
                    if not child_window:
                        child.terminate(); child.wait(timeout=10)
                        raise RuntimeError("The shell did not create an X11 window")
                    run("wmctrl", "-i", "-a", child_window, env=env)
                    return child, child_window, focus_window(child_window)
                except BaseException:
                    # Startup has not assigned the outer process yet.
                    if child.poll() is None:
                        child.terminate()
                        try:
                            child.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            child.kill(); child.wait(timeout=5)
                    raise

            process, window, focused = start_shell()
            print(f"Acceptance window {window}, keyboard focus {focused:#x}", flush=True)
            if args.catalog:
                exercise_catalog(evidence, executable, layout, original_bytes, key, run, env, lambda: process.poll())
                return
            if args.collections:
                exercise_collections(evidence, executable, layout, original_bytes, collection_fixture, key, pointer, run, env, lambda: process.poll(), window)
                return
            if args.studio:
                def capture(phase):
                    capture_trace.append({"phase":phase,"actions":len(input_trace),"elapsedSeconds":time.monotonic()-journey_started,"layoutSha256":hashlib.sha256(layout.read_bytes()).hexdigest()})
                    print(f"{phase.upper()}_READY {evidence}", flush=True)
                    deadline = time.monotonic() + 60
                    while not (evidence / f"{phase}-captured").exists():
                        if time.monotonic() > deadline:
                            raise RuntimeError("Capture acknowledgement timed out: " + phase)
                        if process.poll() is not None:
                            raise RuntimeError("Studio exited during capture")
                        time.sleep(0.1)

                if args.themes_only:
                    native_directory = evidence / "data/Isolde/native-profiles/default/kodi/home/.kodi/userdata"
                    native_before = {str(f.relative_to(native_directory)):hashlib.sha256(f.read_bytes()).hexdigest() for f in native_directory.glob("*.xml")}
                    phases = []
                    def snap(tag, name):
                        phase=tag+name;time.sleep(.35);capture(phase);phases.append(phase)
                        assert layout.read_bytes()==original_bytes,"Theme route rewrote fixture layout"
                    def origin():
                        key("Home");key("Up",6);key("Down");key("Left",4);key("Return");key("Up",6);key("Down");key("Left",4)
                    def settings_section(index):
                        origin();key("Up",6);key("Return");key("Up",8);key("Down",index);key("Return")
                    for sequence,case in enumerate((2,) if args.themes_custom_only else range(3)):
                        tag=("dark","pastel","custom")[case]
                        if sequence:
                            process.terminate();process.wait(timeout=10)
                            original_bytes=json.dumps(theme_document(case)).encode();layout.write_bytes(original_bytes)
                            process,window,focused=start_shell()
                        if args.themes_controls_only:
                            for row,name in ((1,"audio"),(2,"ambient")):
                                settings_section(4);key("Up",12);key("Down",row);key("Return");snap(tag,name);key("Escape")
                            key("Home");snap(tag,"return")
                            continue
                        if args.themes_faults_only:
                            origin();key("Down");key("Right");key("Return");snap(tag,"gallery");key("Escape")
                            origin();key("Return")
                            expect(lambda:theme_provider.completed_movies>0,"Installed movie boundary ready")
                            key("Up",20);key("Right",20)
                            theme_provider.mode="loading";theme_provider.release.clear()
                            previous=theme_provider.completed_movies;key("Return")
                            # Capture the real installed X11 frame before host transport
                            # acknowledgement can outlast the production RPC deadline.
                            capture_x11_window(x11,display,int(window,16),evidence/(tag+"loading-frame.png"))
                            theme_provider.release.set()
                            expect(lambda:theme_provider.completed_movies>previous,"Loading request completed")
                            snap(tag,"loading")
                            theme_provider.mode="error";previous=theme_provider.expected_errors
                            key("Return");expect(lambda:theme_provider.expected_errors>previous,"Controlled RPC outage returned")
                            snap(tag,"error")
                            theme_provider.mode="normal";previous=theme_provider.completed_movies
                            key("Return");expect(lambda:theme_provider.completed_movies>previous,"Installed retry recovered")
                            snap(tag,"retry");key("Home");snap(tag,"return")
                            continue
                        snap(tag,"home")
                        origin();snap(tag,"origin")
                        key("XF86Search");snap(tag,"search")
                        key("Return");snap(tag,"searchkeyboard");key("Escape");key("Escape")
                        snap(tag,"searchreturn")
                        origin();key("Down");key("Right");key("Return");snap(tag,"gallery");key("Escape");snap(tag,"galleryreturn")
                        origin();key("Return")
                        expect(lambda:any(r["method"]=="VideoLibrary.GetMovies" for r in theme_provider.requests),"Theme Movies uses production Kodi client")
                        snap(tag,"movies");key("Menu");snap(tag,"details");key("Escape")
                        key("Up",20);key("Right");key("Return");snap(tag,"shows")
                        key("Return");snap(tag,"seasons");key("Return");snap(tag,"episodes")
                        settings_section(0);snap(tag,"network")
                        key("Up",12);key("Down",2);key("Return");snap(tag,"networkkeyboard");key("Escape");key("Escape")
                        for section,name in ((2,"sources"),(3,"remotes"),(5,"profiles"),(6,"vpn"),(7,"storepreferences")):
                            settings_section(section);snap(tag,name)
                        settings_section(4);snap(tag,"system")
                        for row,name in ((0,"display"),(1,"audio"),(2,"ambient"),(6,"media"),(7,"components"),(8,"inputs")):
                            settings_section(4);key("Up",12);key("Down",row);key("Return");snap(tag,name)
                            if row==6:
                                key("Up",12);key("Down",6);key("Return");snap(tag,"folders")
                                key("Escape");key("Down",2);key("Return");snap(tag,"import");key("Escape")
                            key("Escape")
                        origin();key("Right",2);key("Return");snap(tag,"apps")
                        key("Menu");snap(tag,"appdialog");key("Escape")
                        key("Up");key("Left",12);key("Return");snap(tag,"appkeyboard")
                        key("Escape");key("Home");snap(tag,"return")
                    if args.themes_faults_only:
                        native_after={str(f.relative_to(native_directory)):hashlib.sha256(f.read_bytes()).hexdigest() for f in native_directory.glob("*.xml")}
                        assert native_before==native_after and native_before
                        assert not theme_provider.errors,theme_provider.errors
                        (evidence/"result.json").write_text(json.dumps(dict(passed=True,journey="themefaults",freshProfile=True,normalProfileUsed=False,palettes=["dark","pastel","custom"],phases=phases,layoutBytesUnchanged=True,nativeKodiBefore=native_before,nativeKodiAfter=native_after,controlledFaults=3,loadingFrames={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in evidence.glob("*loading-frame.png")}),indent=2))
                        return
                    key("Home");key("Up",20);key("Right");key("Return")
                    studio_page_menu(layout,key,"appearance")
                    key("Down",5);key("Return");key("Return");key("Down",2);key("Return")
                    key("Down");key("Return");key("Down");key("Return");snap("custom","policy")
                    key("Menu");snap("custom","reset")
                    key("Return");key("Down");key("Return")  # Keep an explicit dirty custom draft to discard.
                    key("Escape",4);key("Down");key("Return");key("Down");key("Return");snap("custom","cancel")
                    native_after={str(f.relative_to(native_directory)):hashlib.sha256(f.read_bytes()).hexdigest() for f in native_directory.glob("*.xml")}
                    assert native_before==native_after and native_before,"Appearance changed native Kodi fixture settings"
                    assert not theme_provider.errors,theme_provider.errors
                    (evidence/"result.json").write_text(json.dumps({"passed":True,"journey":"themecontrols" if args.themes_controls_only else "themecustom" if args.themes_custom_only else "themes","executable":str(executable),"freshProfile":True,"normalProfileUsed":False,"palettes":["custom"] if args.themes_custom_only else ["dark","pastel","custom"],"phases":phases,"fixtureOnly":True,"nativeKodiBefore":native_before,"nativeKodiAfter":native_after,"layoutBytesUnchanged":True},indent=2))
                    print("Evidence: "+str(evidence),flush=True)
                    return
                if args.search_only:
                    key("Up",30); capture("searchhome")
                    key("Menu"); key("Return"); offline_module.remote_text(key,"settings")
                    time.sleep(1); capture("offlinesearch")
                    assert layout.read_bytes() == original_bytes
                    key("Escape"); capture("offlinerecovery")
                    assert layout.read_bytes() == original_bytes
                    network=json.loads((evidence/"network-denial.json").read_text())
                    assert network["socketErrnos"] == [101,101] and network["unixSocketAvailable"] is True
                    (evidence/"result.json").write_text(json.dumps({"passed":True,"journey":"studio-search","freshProfile":True,"normalProfileUsed":False,"network":network},indent=2))
                    print("Evidence: "+str(evidence),flush=True)
                    return
                key("Return")  # Existing missing source -> Home editor.
                capture("entry")  # Real Customize Home entry must already be the page-first Studio.
                assert layout.read_bytes() == original_bytes
                if args.designs_only:
                    designs=exercise_studio_designs(evidence,layout,key,capture,expect,enter_from_settings=False)
                    saved_before_restart=layout.read_bytes()
                    process.terminate();process.wait(timeout=10)
                    process,window,focused=start_shell()
                    time.sleep(1);key("Up",2);capture("coldreopen")
                    assert layout.read_bytes()==saved_before_restart,"Cold reopen rewrote the saved design"
                    designs["coldReopenPreservedBytes"]=True
                    key("Home"); key("Menu"); key("Return")
                    offline_module.remote_text(key, "settings")
                    time.sleep(1); capture("offlinesearch")
                    assert layout.read_bytes() == saved_before_restart
                    key("Escape"); capture("offlinerecovery")
                    designs["offlineLocalSearchJourney"] = True
                    network = json.loads((evidence / "network-denial.json").read_text())
                    assert network["socketErrnos"] == [101, 101] and network["unixSocketAvailable"] is True
                    designs["offlineSocketFilter"] = True
                    (evidence/"result.json").write_text(json.dumps({"passed":True,"journey":"studio-designs","executable":str(executable),"freshProfile":True,"normalProfileUsed":False,"designs":designs},indent=2))
                    print("Evidence: "+str(evidence),flush=True)
                    return
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
                    studio_legacy_menu(layout, key, index)

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
                studio_page_menu(layout,key,"safety"); key("Return")  # Named Try, versions & safety -> Try interaction.
                assert layout.read_bytes() == original_bytes
                capture("studio")
                key("Escape")
                assert layout.read_bytes() == original_bytes
                archive_block = Path(str(layout) + ".studio-versions.json")
                assert not archive_block.exists()
                archive_block.mkdir()  # Private-profile production filesystem failure boundary.
                try:
                    page_menu(7); key("Return")
                    capture("applyfailure")
                    assert layout.read_bytes() == original_bytes, "Failed Apply changed the saved layout"
                finally:
                    archive_block.rmdir()
                key("Up"); key("Return")  # Retry Apply, retaining the complete draft.
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
                page_menu(4); key("Down"); key("Return")  # Layers selects the original Clock.
                key("Return"); key("Down",3); key("Return")  # Original component context -> Delete.
                capture("deleteconfirm")
                assert layout.read_bytes() == applied
                key("Down"); key("Return"); capture("deletedraft")
                assert layout.read_bytes() == applied
                key("Escape"); key("Up",30); key("Left"); key("Return"); key("Down")
                capture("deleteundo")
                page_menu(7); key("Return")
                expect(lambda:layout.read_bytes()==applied,"Delete then Undo restored exact applied component bytes")
                key("Escape"); key("Return")
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
                colors = exercise_studio_colors(evidence,layout,key,capture,expect)
                workflows = exercise_studio_workflows(evidence,layout,key,capture,expect)
                (evidence / "result.json").write_text(json.dumps({
                    "passed": True, "journey": "studio", "layout": str(layout),
                    "executable": str(executable), "preview_cancel_preserved_bytes": True,
                    "reopen_preserved_bytes": True, "atomicFailureRetry": True, "deleteUndoExactBytes": True, "user_usability_acceptance": "pending",
                    "remote_collision_cancel_swap_undo": True, "remote_resize_cancel_accept_undo": True,
                    "bounded_repeat_cancel": True, "nested_group_remote_layers_apply": True,
                    "scrolling_checkbox_preview_cancel_apply": True, "three_viewport_page": True,
                    "eight_slot_layout_only": True, "physical_input_qualification": "not claimed", "colorPicker": colors, "workflows": workflows
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
        if theme_provider:
            theme_provider.close()
        x11.XCloseDisplay(display)
        if active:
            run("systemctl", "--user", "start", "isolde.service")
        result_path = evidence / "result.json"
        if result_path.is_file():
            result = json.loads(result_path.read_text())
            result["inputTrace"] = input_trace
            result["captureTrace"] = capture_trace
            result["actualInputActions"] = len(input_trace)
            result["journeyElapsedSeconds"] = time.monotonic()-journey_started
            result_path.write_text(json.dumps(result,indent=2))


def theme_palettes():
    return [
        dict(backgroundStart="#19191e",backgroundEnd="#101014",surfaceColor="#202126",selectedColor="#353039",textColor="#f5f1f3",mutedColor="#ccc6cf",focusColor="#c994a5",motionEnabled=True,highContrast=False),
        dict(backgroundStart="#f5edf3",backgroundEnd="#e9d8e3",surfaceColor="#f5edf3",selectedColor="#e9d8e3",textColor="#241d2b",mutedColor="#514b67",focusColor="#6d3b69",motionEnabled=True,highContrast=False),
        dict(backgroundStart="#0c3935",backgroundEnd="#081f2d",surfaceColor="#0c3935",selectedColor="#f4e8ce",elevatedColor="#f4e8ce",inputColor="#f4e8ce",textColor="#f4e8ce",mutedColor="#f4e8ce",focusColor="#ffdf78",motionEnabled=False,highContrast=True),
    ]


def theme_document(index):
    palettes=theme_palettes()
    composition=dict(schemaVersion=1,enabled=True,grid=dict(columns=4,rows=3,mode="screen"),pageNavigation="none")
    def item(identity,kind,x,**values):
        if kind=="app":
            values["action"]=dict(schemaVersion=1,type="launch-app",appId=values["appId"])
            values["source"]=dict(schemaVersion=1,provider="apps",kind="app",id=values["appId"])
        else:values["source"]=dict(schemaVersion=1,provider="shell",kind="page",id=values["action"]["pageId"])
        return dict(schemaVersion=1,componentType="launcher",id=identity.lower().replace(" ","-"),kind=kind,name=identity,placement=dict(x=x,y=0,width=1,height=1),**values)
    origin=dict(schemaVersion=2,theme=palettes[(index+1)%3],composition=composition,categories=[],components=[
        item("Movies","app",0,appId="movies"),item("Shows","app",1,appId="shows"),item("Apps","app",2,appId="app-store")])
    preset=json.loads((Path(__file__).resolve().parents[1]/"config/studio-defaults.json").read_text())["componentPresets"]["app-bar"]
    preset.update(schemaVersion=1,id="theme-apps",name="Apps",source=dict(schemaVersion=1,provider="shell",kind="container"),placement=dict(x=0,y=1,width=4,height=1))
    preset["appCollection"]=dict(mode="all",ids=[],showStore=False)
    preset["presentation"]["layout"]["visibleColumns"]=2
    origin["components"].append(preset)
    designated=dict(schemaVersion=2,theme=palettes[index],composition=composition,categories=[],components=[
        item("Open themed page","action",0,action=dict(schemaVersion=1,type="open-page",pageId="theme-origin"))])
    return dict(schemaVersion=2,theme=palettes[(index+2)%3],composition=composition,categories=[],components=designated["components"],
        homePageId="theme-home",pages=[dict(id="theme-home",name="Palette Home",layout=designated),
                                     dict(id="theme-origin",name="Theme origin",layout=origin)])


def prepare_theme_provider(evidence):
    from reference_collections import ReferenceCollections
    repository=Path(__file__).resolve().parents[1]
    fixture=json.loads((repository/"tests/visual/home-pastel-reference/collections.json").read_text())
    class ThemeProvider(ReferenceCollections):
        mode="normal"
        completed_movies=0
        expected_errors=0
        def __init__(self,*args):
            import threading
            self.release=threading.Event();self.release.set()
            super().__init__(*args)
        def serve(self):
            import socket
            while not self.stopped.is_set():
                try:connection,_=self.socket.accept()
                except socket.timeout:continue
                except OSError:return
                with connection:
                    connection.settimeout(2)
                    try:
                        message=b""
                        while not message.endswith(b"\n") and len(message)<=65536:
                            chunk=connection.recv(65536)
                            if not chunk:raise ValueError("Incomplete theme fixture request")
                            message+=chunk
                        request=json.loads(message);self.requests.append(request)
                        movies=request["method"]=="VideoLibrary.GetMovies"
                        response={"jsonrpc":"2.0","id":request["id"]}
                        if movies and self.mode=="loading":
                            if not self.release.wait(15):raise ValueError("Theme loading gate not released")
                        if movies and self.mode=="error":
                            response["error"]={"code":-32000,"message":"Isolated theme outage"};self.expected_errors+=1
                        else:
                            response["result"]=self.reply(request)
                            if movies:self.completed_movies+=1
                        connection.sendall(json.dumps(response).encode()+b"\n")
                    except (OSError,ValueError,KeyError) as error:self.errors.append(str(error))
        def reply(self,request):
            method=request["method"]
            if method in {"VideoLibrary.GetTVShows","VideoLibrary.GetSeasons"}:
                key="tvshows" if method.endswith("TVShows") else "seasons"
                identity="tvshowid" if key=="tvshows" else "seasonid"
                return {key:[{identity:7,"tvshowid":7,"title":"Isolated theme television", "season":1,"episode":1,"watchedepisodes":0}],"limits":{"start":0,"end":1,"total":1}}
            return super().reply(request)
    provider=ThemeProvider(evidence,fixture,repository)
    native=evidence/"data/Isolde/native-profiles/default/kodi/home/.kodi/userdata"
    for name in ("guisettings.xml","sources.xml"):
        (native/name).write_text("<isolated-theme-fixture>keep native Kodi owned data</isolated-theme-fixture>")
    return provider


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





def exercise_studio_colors(evidence,layout,key,capture,expect):
    baseline=layout.read_bytes()
    phases=[]
    def shot(name):
        phases.append(name);capture(name)
    def menu(index):
        studio_legacy_menu(layout,key,index)
    def page_color():
        menu(3);key('Return');key('Return') # Colors, then Background top / solid.
    def apply():
        key('Escape',2);menu(7);key('Return')
    key('Up',30);key('Right');key('Return') # Permanent Edit Home recovery.
    page_color();key('Down');key('Right',2);key('Return');shot('colorpreview')
    assert layout.read_bytes()==baseline
    key('Escape');assert layout.read_bytes()==baseline
    key('Return');key('Down');key('Right',2);key('Return');key('Down',10);key('Up',2);key('Return')
    assert layout.read_bytes()==baseline
    apply()
    expect(lambda:json.loads(layout.read_text()).get('theme',{}).get('backgroundStart')=='#e89ab0','Color Apply persisted the authored page background')
    saved=layout.read_bytes();doc=json.loads(saved)
    assert doc['studioAppearance']['recentColors']==['#e89ab0']
    shot('colorapplied')
    page_color();key('Right');shot('colorrecent') # Recent category; no layout writes.
    assert layout.read_bytes()==saved
    key('Escape');key('Return');key('Down',10);key('Up',3);key('Return');shot('colorreset')
    assert layout.read_bytes()==saved
    key('Escape');assert layout.read_bytes()==saved
    key('Escape',2);menu(6);key('Down');key('Return') # Editor settings -> Studio colors.
    key('Down',2);key('Return');key('Down');key('Return');shot('chromepreview') # Text -> Snow.
    assert layout.read_bytes()==saved
    key('Down',10);key('Up',2);key('Return');apply()
    expect(lambda:json.loads(layout.read_text()).get('studioAppearance',{}).get('textColor')=='#ffffff','Studio color Apply persisted')
    saved=layout.read_bytes();doc=json.loads(saved)
    assert doc['studioAppearance']['recentColors']==['#ffffff','#e89ab0']
    shot('chromeapplied')
    key('Escape');key('Return') # Reopen at the exact restored Edit Home origin.
    page_color();key('Right');shot('colorreopened')
    assert layout.read_bytes()==saved
    return dict(privateCancel=True,atomicApply=True,recentSaved=True,resetCancel=True,studioChrome=True,reopenPreserved=True,captures=phases)

def exercise_studio_workflows(evidence,layout,key,capture,expect):
    saved=layout.read_bytes();phases=[]
    def shot(name):
        phases.append(name);capture(name)
    def menu(index):
        studio_legacy_menu(layout,key,index)
    def apply():
        key('Escape',2);menu(7);key('Return')
    key('Escape',3);menu(4);key('Up',30);key('Return') # Layers -> first authored shortcut.
    key('Return');key('Down');key('Return');key('Down',2);key('Return');key('Return') # Edit -> Size & layout -> Outer size.
    key('Down',6);key('Return');key('Up',6);key('Right');shot('sizepreview')
    assert layout.read_bytes()==saved
    key('Escape');assert layout.read_bytes()==saved
    key('Return');key('Down',6);key('Return');key('Up',6);key('Right');key('Down',2);key('Return');apply()
    def sized():
        item=next(i for i in json.loads(layout.read_text())['components'] if i['id']=='personal-source')
        return item['placement']['width']==2 and item['placement']['height']==2 and item['presentation']['sizeAspectLocked'] is True
    expect(sized,'Proportional outer size applied atomically');saved=layout.read_bytes();shot('sizeapplied')
    menu(3);key('Down',4);key('Return') # Page appearance -> Header.
    fields=[f for f in json.loads((Path(__file__).resolve().parents[1]/'config/studio-defaults.json').read_text())['inspector']['pageFields'] if f['tab']=='Header' and not f.get('advanced')]
    def field(name):
        key('Up',30);key('Down',next(i for i,f in enumerate(fields) if f['key']==name));key('Return')
    field('headerMode');key('Up',30);key('Down');key('Return')
    field('showClock');shot('headerpreview');assert layout.read_bytes()==saved
    field('order');key('Up',30);key('Down',3);key('Return');field('position');key('Up',30);key('Down');key('Return');shot('headerorder');assert layout.read_bytes()==saved
    apply()
    expect(lambda:json.loads(layout.read_text())['composition'].get('headerMode')=='identity' and json.loads(layout.read_text())['composition'].get('showClock') is True and json.loads(layout.read_text())['composition']['header']['order']==['clock','title','controls'] and json.loads(layout.read_text())['composition']['header']['position']=='bottom','Authored header applied through shared model')
    saved=layout.read_bytes();shot('headerapplied')
    menu(4);key('Down',30);key('Return');shot('headerreopened');assert layout.read_bytes()==saved
    key('Escape',2);menu(6);key('Return');key('Right');shot('canvaszoom');assert layout.read_bytes()==saved
    key('Escape');key('Down',2);key('Return');shot('resetcancel');key('Escape');assert layout.read_bytes()==saved
    return dict(proportionalSize=True,sizeCancel=True,headerApply=True,headerRecovery=True,cameraNoWrite=True,resetCancel=True,captures=phases)

def exercise_studio_designs(evidence, layout, key, capture, expect, enter_from_settings=True):
    """Actual product controls; fixture assertions read only this private profile."""
    phases = []
    def shot(name):
        phases.append(name); capture(name)
    def named(action, extra=0):
        studio_page_menu(layout,key,action,extra)
    def name_digit(digit):
        key("Right",digit-1); key("Return"); key("Down",5); key("Return")
    def apply(extra=0):
        named("apply",extra); key("Return")
    def current():
        return json.loads(layout.read_text())
    def page_document():
        return current()["pages"][-1]["layout"]
    def remote_text(value, clear=False):
        source=(Path(__file__).resolve().parents[1]/"qml/OnScreenKeyboard.qml").read_text()
        keys=json.loads(re.search(r'\n\s*: (\["1"[^\n]+\])',source).group(1))
        current_index=0
        def choose(index):
            nonlocal current_index
            rows=index//10-current_index//10
            if rows:
                key("Down" if rows>0 else "Up",abs(rows))
                current_index=max(0,min(len(keys)-1,current_index+10*rows))
            columns=index-current_index
            if columns:key("Right" if columns>0 else "Left",abs(columns))
            current_index=index;key("Return")
        if clear:choose(keys.index("Clear"))
        for character in value:choose(keys.index(character))
        choose(keys.index("Done"))
    def background_rose():
        named("appearance"); key("Return"); key("Return")
        key("Down"); key("Right",2); key("Return")
        key("Down",10); key("Up",2); key("Return")  # Explicit Use color; Back would cancel the swatch.
        key("Escape",2); apply()
    baseline=layout.read_bytes()
    if enter_from_settings:key("Escape")  # Leave the preceding editor-settings workflow.
    named("reuse"); key("Return"); name_digit(1)
    shot("sharedstyle")
    assert layout.read_bytes()==baseline
    key("Up"); key("Return")  # Accept the one draft definition.
    key("Down"); key("Return"); key("Return")  # Choose the saved style, keep default Cancel.
    shot("sharedstyleimpact")
    assert layout.read_bytes()==baseline
    key("Up"); key("Return"); key("Escape"); apply()
    expect(lambda:len(current()["composition"]["designLibrary"]["styles"])==1,"Named shared style applied through remote controls")
    saved=layout.read_bytes();shot("sharedstyleapplied")
    assert any(node.get("presentation",{}).get("styleId") for node in current()["components"])
    named("reuse");key("Down",7);key("Return");name_digit(1)
    shot("savedcomponent");assert layout.read_bytes()==saved
    key("Up");key("Return");key("Down",9);key("Return");key("Return")
    shot("linkedinstance");assert layout.read_bytes()==saved
    key("Up");key("Return");key("Escape");apply()
    expect(lambda:any(node.get("linkedDesign") for node in current()["components"]),"Linked instance placed in a free cell and applied")
    linked=next(node for node in current()["components"] if node.get("linkedDesign"));linked_id=linked["id"]
    assert linked_id not in {node["id"] for node in json.loads(saved)["components"]}
    saved=layout.read_bytes();shot("linkedapplied")
    named("layers");key("Down",100);key("Up",2);key("Return")  # Last component, before reading-order/Header commands.
    named("reuse");key("Down",11);key("Return");shot("detacheddraft")
    assert layout.read_bytes()==saved
    key("Up");key("Return");key("Escape");apply()
    expect(lambda:not next(node for node in current()["components"] if node["id"]==linked_id).get("linkedDesign"),"Detach preserved independent content and identity")
    original_root=current()["components"]
    shot("detachedapplied")
    named("new");name_digit(2)
    # The unsaved page is already visible in Studio's menu; do not derive its count from applied bytes.
    named("apply",1);key("Return")
    expect(lambda:len(current().get("pages",[]))==1,"Created a named destination through the normal remote page flow")
    original_ids=[node["id"] for node in original_root]
    templates=json.loads((Path(__file__).resolve().parents[1]/"config/skins/templates.json").read_text())["templates"]
    for index,template in enumerate(templates):
        saved=layout.read_bytes();named("designs");key("Return");key("Down",3+index);key("Return");key("Return")
        phase="template"+["compact","scrolling","poster","pastel"][index];shot(phase)
        assert layout.read_bytes()==saved  # Preview is private and Cancel remains the default.
        key("Up");key("Return");apply()
        expect(lambda:page_document().get("components",[]),"Current-page template applied: "+template["name"])
        assert current()["components"]==original_root
        assert [node["id"] for node in current()["components"]]==original_ids
        background_rose()
        expect(lambda:page_document()["theme"]["backgroundStart"]=="#e89ab0","Customized template color through named swatches: "+template["name"])
        assert current()["components"]==original_root
        shot(phase+"customized")
    # A named version is durable metadata; choosing it previews without changing applied bytes.
    saved=layout.read_bytes();named("safety");key("Down",2);key("Return");key("Return");name_digit(3)
    archive=Path(str(layout)+".studio-versions.json")
    expect(lambda:any(entry["name"]=="3" for entry in json.loads(archive.read_text())["versions"]),"Named milestone saved in the isolated profile")
    assert layout.read_bytes()==saved
    versions=json.loads(archive.read_text())["versions"];version_index=next(i for i,entry in enumerate(versions) if entry["name"]=="3")
    key("Up",30);key("Down",3+version_index);key("Return");shot("versioncompare")
    assert layout.read_bytes()==saved
    key("Return")  # Cancel is the initial review selection.
    assert layout.read_bytes()==saved
    key("Escape")
    named("editorSettings");key("Down",2);key("Return");key("Down");key("Return")
    shot("defaultsrestore")
    assert layout.read_bytes()==saved
    apply()
    expect(lambda:layout.read_bytes()!=saved,"Current-page defaults saved through normal remote controls")
    assert current()["components"]==original_root
    shot("defaultsapplied")
    named("safety");key("Down",2);key("Return")
    versions=json.loads(archive.read_text())["versions"];version_index=next(i for i,entry in enumerate(versions) if entry["name"]=="3")
    key("Up",30);key("Down",3+version_index);key("Return");shot("versionrestore")
    key("Up");key("Return");apply()
    expect(lambda:layout.read_bytes()==saved,"Named current-page restore recovered the exact previous design")
    shot("versionrestored")
    from PIL import Image
    with tempfile.TemporaryDirectory(prefix="i",dir="/tmp") as temporary:
        directory=Path(temporary)
        assert directory.resolve()==directory and directory.parent==Path("/tmp")
        portable=directory/"p.json"
        named("designs");key("Down",2);key("Return");key("Return")
        remote_text(str(portable))
        expect(portable.is_file,"Portable current-page export saved through the remote keyboard")
        package=portable.read_bytes()
        assert str(directory).encode() not in package
        (evidence/"portable-export.json").write_bytes(package)
        shot("portableexport");key("Escape")
        saved=layout.read_bytes();named("designs");key("Down");key("Return");key("Down");key("Return")
        remote_text(str(directory),clear=True)
        key("Down",2);key("Return");key("Return");shot("portablepreview")
        assert layout.read_bytes()==saved
        key("Return");assert layout.read_bytes()==saved  # Initial Cancel.
        key("Return");key("Up");key("Return");apply()
        assert current()["components"]==original_root
        shot("portableapplied")
        saved=layout.read_bytes()
        Image.new("RGB",(80,45),(80,130,180)).save(directory/"r.png")
        named("designs");key("Down",3);key("Return");key("Return");key("Down");key("Return")
        remote_text(str(directory),clear=True)
        key("Down",2);key("Return");shot("privatereference")
        assert layout.read_bytes()==saved
        key("Down",2);key("Return");shot("referencehidden")
        key("Down");key("Return");shot("referencecleared")
        assert layout.read_bytes()==saved
        key("Escape",2)
    key("Escape",2)
    return {"remoteSharedStyle":True,"sharedImpactPreview":True,"myComponents":True,"linkedInsertDetach":True,
            "freshIdentities":True,"allFourTemplatesCustomized":True,"currentPagePreservedRoot":True,
            "namedMilestone":True,"versionPreviewCancelNoWrite":True,"defaultsApplyVersionRestoreExact":True,"remotePortableExportImport":True,"privateReferenceChooseHideRemove":True,"captures":phases}


def prepare_catalog(evidence):
    from PIL import Image, ImageDraw
    directory=evidence/'inputs'/'00-Catalog inputs';directory.mkdir(parents=True)
    (directory/'00-unreadable.gif').write_bytes(b'GIF89a invalid fixture')
    frames=[]
    for color,text in [('#305e9c','FIRST'),('#a64764','SECOND')]:
        image=Image.new('RGB',(160,96),color);draw=ImageDraw.Draw(image)
        draw.rectangle((16,16,143,79),outline='white',width=4);draw.text((45,42),text,fill='white');frames.append(image)
    frames[0].save(directory/'01-animation.gif',save_all=True,append_images=frames[1:],duration=500,loop=0)
    (evidence/'config/user-dirs.dirs').write_text('XDG_PICTURES_DIR="'+str(directory.parent)+'"\n')


def exercise_catalog(evidence,executable,layout,original,key,run,env,poll):
    phases=[]
    options=json.loads((Path(__file__).resolve().parents[1]/'config/studio-defaults.json').read_text())
    def expect(predicate,message,timeout=20):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            if poll() is not None:raise RuntimeError('Catalog shell exited')
            if predicate():return
            time.sleep(.15)
        raise RuntimeError(message)
    def capture(phase):
        phases.append(phase);print(phase.upper()+'_READY '+str(evidence),flush=True)
        expect(lambda:(evidence/(phase+'-captured')).exists(),'Capture acknowledgement: '+phase,60)
    def page_menu(index):
        studio_legacy_menu(layout,key,index)
    def fields(tab,name,advanced=False,start_tabs=False,kind="image"):
        tabs=['Content','Appearance','Layout','Actions','Accessibility','More']
        if not start_tabs:key('Escape')
        key('Up',10);key('Down',tabs.index(tab));key('Return')
        candidates=[f for f in options['inspector']['fields'] if f['tab']==tab and (advanced or not f.get('advanced')) and (not f.get('types') or kind in f['types']) and kind not in f.get('excludeTypes',[]) and not f.get('families')]
        index=next(i for i,f in enumerate(candidates) if f['key']==name)
        key('Down',index);key('Return')
    assert sorted(p.name for p in (evidence/'inputs/00-Catalog inputs').iterdir())==['00-unreadable.gif','01-animation.gif']
    key('Return');capture('catalogentry')
    key('Down',2);key('Return');key('Return');key('Return');key('Return')
    index=next(i for i,f in enumerate(options['catalog']) if f['id']=='image')
    key('Down',index//2)
    if index%2:key('Right')
    key('Return');capture('imagestyles');key('Return');key('Return');key('Return');capture('imagebrowser')
    key('Return');capture('imageinvalid');assert layout.read_bytes()==original
    key('Down');key('Return');capture('imagepreview');key('Return')
    key('Down');key('Return');key('Return');capture('imageplaced')
    assert layout.read_bytes()==original
    key('Return');key('Down');key('Return');capture('imageinspector')
    # Inspector opens on Content. The ordinary fields are derived from its packaged schema.
    fields('Content','animateArtwork',start_tabs=True);capture('imagemotiondraft')
    assert layout.read_bytes()==original
    fields('Appearance','crop');capture('imageframing')
    key('Right');key('Return');key('Down',3);key('Right');capture('imageframingdraft')
    key('Escape');assert layout.read_bytes()==original
    fields('Appearance','crop');key('Right');key('Return');key('Down',6);key('Return')
    key('Escape');key('Escape');page_menu(7);key('Return')
    expect(lambda:layout.read_bytes()!=original,'GIF Apply did not persist the staged draft')
    saved=layout.read_bytes();doc=json.loads(saved);image=next(i for i in doc['components'] if i['componentType']=='image')
    assert image['presentation']['animateArtwork'] is True and image['customLogo'].endswith('.gif')
    assert image['presentation']['artFocalX']==.55
    managed=layout.parent/image['customLogo'];assert managed.read_bytes()==(evidence/'inputs/00-Catalog inputs/01-animation.gif').read_bytes()
    capture('imageapplied')
    def add_family(family,choice=0):
        page_menu(2)
        index=next(i for i,f in enumerate(options['catalog']) if f['id']==family)
        key('Down',index//2)
        if index%2:key('Right')
        key('Return')
        if family=='shape':key('Down',choice);key('Return')
        elif family in ('text','button'):key('Return')
        if family=='button':key('Down',3);key('Return') # Named Open Settings, no execution in a draft.
        key('Return')
    add_family('shape');capture('catalogshape')
    add_family('text');capture('catalogtext')
    key('Return');key('Down');key('Return')
    fields('Content','bodyText',start_tabs=True,kind='text');capture('textkeyboard')
    key('Return');key('Down',5);key('Return') # Append 1, then ordinary on-screen Done.
    fields('Content','visibleLabel',kind='text');key('Return');key('Down',5);key('Return')
    fields('Appearance','labelPosition',kind='text');key('Return');capture('textindependent')
    key('Escape');key('Escape')
    add_family('button');capture('catalogbutton')
    add_family('shape',5);capture('catalogcontainer')
    page_menu(4);capture('cataloglayers');key('Down');key('Return') # Select the decorative Image through Layers.
    key('Return');key('Down');key('Return')
    fields('Actions','action',start_tabs=True)
    key('Down',len(options['actions'])+2);key('Return');capture('imagelink')
    key('Escape');key('Escape');page_menu(7);key('Return')
    expect(lambda:len(json.loads(layout.read_text())['components'])==6,'Primitive catalog Apply did not persist all six real nodes')
    saved=layout.read_bytes();doc=json.loads(saved)
    by_type={i['componentType']:i for i in doc['components']}
    assert {'shape','container','button','image','text'}.issubset(by_type)
    text=by_type['text'];assert text['presentation']['bodyText']=='Your text1' and text['presentation']['visibleLabel']=='1' and text['name']=='Text'
    assert text['presentation']['labelPosition']=='inside'
    assert by_type['button']['action']['type']=='settings'
    assert by_type['image']['action']['type']=='root-page'
    assert by_type['container']['presentation']['layout'] and not by_type['shape'].get('parentId')
    capture('catalogapplied')
    key('Escape');key('Escape');key('Escape');capture('imageruntimeone');time.sleep(.55);capture('imageruntimetwo')
    assert layout.read_bytes()==saved
    key('Up',12);key('Right');key('Return');capture('imagereopened')
    assert layout.read_bytes()==saved
    (evidence/'result.json').write_text(json.dumps(dict(passed=True,journey='catalog',executable=str(executable),freshProfile=True,remoteImport=True,invalidRefused=True,privateDraft=True,framingCancel=True,atomicGifApply=True,reopenPreserved=True,remotePrimitives=True,independentText=True,linkedImage=True,layersSelection=True,managedGifSha256=hashlib.sha256(managed.read_bytes()).hexdigest(),captures=phases),indent=2))
    print('Evidence: '+str(evidence),flush=True)
if __name__ == "__main__":
    main()
