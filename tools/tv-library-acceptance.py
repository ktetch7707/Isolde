#!/usr/bin/env python3
"""Opt-in real-Kodi TV acceptance. NEW synthetic profile/media only.

Kodi imports the test library through its native media-settings dialog. No
database edits, user media writes, network control listener or profile copies.
The normal Isolde service is restored even if an assertion fails.
"""
import argparse
import base64
import ctypes
import ctypes.util
import fcntl
import hashlib
import json
import os
from pathlib import Path
import socket
import signal
import struct
import subprocess
import tempfile
import time
import uuid
import zlib
from xml.sax.saxutils import escape


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


def rpc(directory, method, params=None):
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(5); client.connect(f"/proc/self/fd/{fd}/rpc.sock")
            client.sendall((json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}) + "\n").encode())
            raw = b""
            while not raw.endswith(b"\n"):
                part = client.recv(65536)
                if not part or len(raw) + len(part) > 4194304: raise RuntimeError("Invalid bridge response")
                raw += part
            reply = json.loads(raw)
            if "error" in reply: raise RuntimeError(str(reply["error"]))
            return reply["result"]
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--pages", action="store_true", help="Create and customize a page through Settings, then exercise its widget against real Kodi")
    args = parser.parse_args()
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a"); fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    profile = str(uuid.uuid4())
    evidence = Path(tempfile.mkdtemp(prefix="isolde-tv-acceptance-"))
    print("Fixture evidence: " + str(evidence) + " · profile " + profile, flush=True)
    media = Path.home() / "Videos/Isolde-Acceptance" / ("Isolde-tv-test-" + profile)
    data = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share")))
    userdata = data / "Isolde/native-profiles" / profile / "kodi/home/.kodi/userdata"
    if media.exists() or userdata.exists(): raise RuntimeError("Fixture must be new")
    media.mkdir(parents=True); userdata.mkdir(parents=True, mode=0o700)
    show = media / "Isolde Television Fixture"; show.mkdir()
    clip = show / "Fixture S01E01.ogv"
    run("gst-launch-1.0", "-q", "videotestsrc", f"num-buffers={600 if args.pages else 1800}", "!", "video/x-raw,width=320,height=180,framerate=15/1",
        "!", "theoraenc", "!", "oggmux", "!", "filesink", "location=" + str(clip))
    def chunk(name, content):
        return struct.pack(">I", len(content)) + name + content + struct.pack(">I", zlib.crc32(name + content))
    poster = show / "poster.png"
    poster.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 240, 360, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress((b"\x00" + b"\x55\x43\x99" * 240) * 360)) + chunk(b"IEND", b""))
    episodes = []
    for season, number in ((0, 1), (1, 1), (1, 2)):
        file = show / f"Fixture S{season:02}E{number:02}.ogv"
        if file != clip: file.write_bytes(clip.read_bytes())
        episodes.append(f"<episodedetails><title>Fixture {season}x{number}</title><showtitle>Isolde Television Fixture</showtitle>"
            f"<season>{season}</season><episode>{number}</episode><runtime>2</runtime><plot>Synthetic TV acceptance, no audio.</plot>"
            f"<filenameandpath>{escape(str(file))}</filenameandpath><path>{escape(str(show))}/</path><file>{file.name}</file>"
            f"<art><thumb>{escape(str(poster))}</thumb></art>"
            + ("<resume><position>20</position><total>120</total></resume>" if season == 1 and number == 2 else "") + "</episodedetails>")
    imported = media / "import"; imported.mkdir()
    (imported / "videodb.xml").write_text("<videodb><version>1</version><tvshow><title>Isolde Television Fixture</title>"
        f"<plot>Local synthetic television acceptance.</plot><path>{escape(str(show))}/</path><basepath>{escape(str(show))}/</basepath>"
        f"<art><poster>{escape(str(poster))}</poster></art>" + "".join(episodes) + "</tvshow><paths/></videodb>")
    exported_art = imported / "tvshows/Isolde_Television_Fixture"
    exported_art.mkdir(parents=True)
    (exported_art / "poster.png").write_bytes(poster.read_bytes())
    (exported_art.parent / "Isolde_Television_Fixture-poster.png").write_bytes(poster.read_bytes())
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in media.rglob("*") if p.is_file()}
    (userdata / "keymaps").mkdir()
    (userdata / "keymaps/tv-fixture.xml").write_text("<keymap><global><keyboard><f9>RunScript(special://profile/tv-fixture-import.py)</f9></keyboard></global></keymap>")
    (userdata / "mediasources.xml").write_text('<mediasources><network><location id="0">' + escape(str(imported)) + '/</location></network></mediasources>')
    (userdata / "tv-fixture-import.py").write_text('''import json, xbmc, xbmcgui
xbmc.executebuiltin("ActivateWindow(mediasettings)")
xbmc.sleep(700)
for _ in range(4):
    win = xbmcgui.Window(xbmcgui.getCurrentWindowId())
    if "Expert" in xbmc.getInfoLabel("Control.GetLabel(20)"): break
    xbmc.executebuiltin("Action(settingslevelchange)")
    xbmc.sleep(300)
xbmc.executebuiltin("ActivateWindow(home)")
xbmc.sleep(300)
xbmc.executebuiltin("ActivateWindow(mediasettings,videolibrary.import)")
xbmc.sleep(500)
for control in range(30, 3000):
    if xbmc.getInfoLabel("Control.GetLabel(%s)" % control).strip().lower() == "import library":
        xbmc.executebuiltin("SetFocus(%s)" % control)
        xbmc.sleep(250)
        break
xbmc.executebuiltin("Action(select)")
xbmc.sleep(700)
dialog = xbmcgui.getCurrentWindowDialogId()
browser = xbmcgui.Window(dialog)
xbmc.executebuiltin("SetFocus(450)")
xbmc.executebuiltin("Action(down)")
xbmc.executebuiltin("Action(select)")
xbmc.sleep(500)
selected = xbmc.getInfoLabel("Control.GetLabel(412)")
xbmc.log("ISOLDE fixture import directory: " + selected, xbmc.LOGINFO)
if ''' + repr(str(imported)) + ''' in selected:
    xbmc.executebuiltin("SendClick(%s,413)" % dialog)
    xbmc.sleep(1800)
    response = json.loads(xbmc.executeJSONRPC(json.dumps({"jsonrpc":"2.0", "id":1, "method":"VideoLibrary.GetTVShows"})))
    for show in response.get("result", {}).get("tvshows", []):
        if show.get("label") == "Isolde Television Fixture":
            xbmc.executeJSONRPC(json.dumps({"jsonrpc":"2.0", "id":2, "method":"VideoLibrary.SetTVShowDetails",
                "params":{"tvshowid":show["tvshowid"], "art":{"poster":''' + repr(str(poster)) + '''}}}))
''')
    config = evidence / "config/Isolde"; config.mkdir(parents=True)
    (config / "profiles.json").write_text(json.dumps({"active": profile, "profiles": [{"id": profile, "name": "TV acceptance"}]}))
    (config / "device-setup.json").write_text('{"version":1,"step":6,"complete":true}')
    (config / "MediaAccess.conf").write_text(f"[profiles]\n{profile}\\kodi\\folders={media}\n")
    unknown = evidence / "config/Unknown Organization"; unknown.mkdir()
    (unknown / "Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    env = dict(os.environ)
    for line in run("systemctl", "--user", "show-environment").splitlines():
        key, _, value = line.partition("=")
        if key in {"DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}: env[key] = value
    env.update(XDG_CONFIG_HOME=str(evidence / "config"), XDG_CACHE_HOME=str(evidence / "cache"), QT_QPA_PLATFORM="xcb")
    os.environ.update({k: env[k] for k in ("DISPLAY", "XAUTHORITY") if k in env})
    x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]; x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XStringToKeysym.argtypes = [ctypes.c_char_p]; x11.XStringToKeysym.restype = ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]; x11.XKeysymToKeycode.restype = ctypes.c_uint
    x11.XFlush.argtypes = [ctypes.c_void_p]; x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    class KeyEvent(ctypes.Structure):
        _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong), ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                    ("window", ctypes.c_ulong), ("root", ctypes.c_ulong), ("subwindow", ctypes.c_ulong), ("time", ctypes.c_ulong),
                    ("x", ctypes.c_int), ("y", ctypes.c_int), ("x_root", ctypes.c_int), ("y_root", ctypes.c_int),
                    ("state", ctypes.c_uint), ("keycode", ctypes.c_uint), ("same_screen", ctypes.c_int)]
    class Event(ctypes.Union): _fields_ = [("key", KeyEvent), ("padding", ctypes.c_long * 24)]
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.POINTER(Event)]
    display = x11.XOpenDisplay(env["DISPLAY"].encode())
    if not display: raise RuntimeError("No X11 display")
    errors = []
    error_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
    error_handler = error_type(lambda _display, _event: errors.append(True) or 0)
    x11.XSetErrorHandler.argtypes = [error_type]; x11.XSetErrorHandler(error_handler)
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    def interrupted(*_): raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM, interrupted)
    def key(window, name, delay=0.6, modifiers=0):
        if window == "kodi":
            window = next(r[0] for r in windows() if "Kodi" in r[-1])
        code = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name.encode()))
        for kind, mask in ((2, 1), (3, 2)):
            event = Event(); event.key = KeyEvent(type=kind, send_event=1, display=display, window=int(window, 16), keycode=code, state=modifiers, same_screen=1)
            x11.XSendEvent(display, int(window, 16), 1, mask, ctypes.byref(event))
        x11.XSync(display, 0)
        if errors: errors.clear(); raise RuntimeError("Test window changed during input; retry acceptance")
        time.sleep(delay)
    def wait(predicate, label, seconds=35):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                value = predicate()
                if value: print(label, flush=True); return value
            except (OSError, RuntimeError, KeyError, ValueError): pass
            if process.poll() is not None: raise RuntimeError("Shell exited; " + str(evidence))
            time.sleep(0.2)
        raise RuntimeError(label + " timed out; " + str(evidence))
    def windows(): return [line.split(None, 4) for line in run("wmctrl", "-lp", env=env).splitlines()]
    active = subprocess.run(["systemctl", "--user", "is-active", "--quiet", "isolde.service"]).returncode == 0
    process = None
    try:
        if active: run("systemctl", "--user", "stop", "isolde.service")
        with (evidence / "shell.log").open("w") as log:
            process = subprocess.Popen([str(args.executable.resolve())] + ([] if args.pages else ["--windowed"]) + ["--launch-app=shows"], env=env, stdout=log, stderr=log)
            shell = wait(lambda: next((r[0] for r in windows() if r[2] == str(process.pid)), None), "Shell ready")
            kodi = wait(lambda: next((r[0] for r in windows() if "Kodi" in r[-1]), None), "Kodi ready")
            directory = userdata / "isolde"
            wait(lambda: rpc(directory, "JSONRPC.Version"), "Private bridge ready")
            time.sleep(6)  # Finish startup-window replacement and managed minimization.
            kodi = next(r[0] for r in windows() if "Kodi" in r[-1])
            run("wmctrl", "-i", "-a", kodi, env=env)
            x11.XSetInputFocus(display, int(kodi, 16), 2, 0); x11.XSync(display, 0)
            time.sleep(1)
            key("kodi", "Escape"); key("kodi", "Escape")  # Dismiss first-run dialogs in this new test home.
            key("kodi", "F9")
            shows = wait(lambda: rpc(directory, "VideoLibrary.GetTVShows", {"properties": ["title"]}).get("tvshows"), "Kodi imported TV fixture")
            show_id = next(s["tvshowid"] for s in shows if s["title"] == "Isolde Television Fixture")
            seasons = rpc(directory, "VideoLibrary.GetSeasons", {"tvshowid": show_id, "properties": ["season"]})["seasons"]
            assert {s["season"] for s in seasons} == {0, 1}
            eps = rpc(directory, "VideoLibrary.GetEpisodes", {"tvshowid": show_id, "properties": ["season", "episode", "resume"]})["episodes"]
            assert len(eps) == 3
            art = wait(lambda: (a if (a := rpc(directory, "Isolde.GetArtwork", {"kind": "tvshow", "id": show_id})).get("data") else None), "Kodi local artwork ready")
            artwork = base64.b64decode(art["data"])
            assert artwork and art["mime"] in {"image/png", "image/jpeg"}
            (evidence / ("artwork.png" if art["mime"] == "image/png" else "artwork.jpg")).write_bytes(artwork)
            print("Real Kodi seasons, episodes and local artwork passed", flush=True)
            run("wmctrl", "-i", "-a", shell, env=env)
            if args.pages:
                def press(name,count=1):
                    for _ in range(count): key(shell,name,0.07)
                    time.sleep(0.4)
                def captured(phase):
                    print(phase.upper()+"_READY "+str(evidence),flush=True)
                    wait(lambda:(evidence/(phase+"-captured")).exists(),phase+" captured",seconds=150)
                def home_settings():
                    press("Home");press("Up",30);press("Left",3);press("Right");press("Return");press("Left",30)
                def edit_navigation():
                    press("Up",30);press("Right",3);press("Return")
                    press("Left",30);press("Right",24);press("Return")
                    press("Down",9);press("Right");press("Escape");press("Escape")
                press("Home");home_settings();press("Right",25);press("Return")
                press("Right",4);press("Return");press("Down",20);press("Return")
                layout=config/"profiles"/profile/"home-layout.json"
                document=wait(lambda:json.loads(layout.read_text()),"Page saved from Settings")
                page_id=document["pages"][0]["id"]
                assert document["pages"][0]["layout"]["categories"][0]["presentation"]["libraryKind"]=="shows"
                press("Left",4);press("Return");time.sleep(2)
                captured("page")
                press("Return");press("Return");time.sleep(0.7);press("Return");time.sleep(0.7);press("Return")
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="playing","Page widget real Kodi episode playback passed")
                key("kodi","Return");wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="paused","Page widget pause passed")
                key("kodi","Escape");wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="ended","Page widget stop and return passed")
                time.sleep(1.2)  # The native stop acknowledgement precedes the shell's playback-status poll.
                for _ in range(3):key(shell,"Escape",0.6)  # Episode -> seasons -> shows -> leave widget.
                press("Home")
                home_settings();press("Right",25);press("Return");press("Right",9);press("Return")
                assert json.loads(layout.read_text())["composition"]["navigation"][-1]["action"]["pageId"]==page_id
                press("Escape");press("Escape");press("Escape")
                press("Up",30);press("Down");press("Right",30);press("Return");time.sleep(1)
                captured("tab")
                edit_navigation()
                assert json.loads(layout.read_text())["pages"][0]["layout"]["composition"]["pageNavigation"]=="top"
                captured("top")
                edit_navigation()
                assert json.loads(layout.read_text())["pages"][0]["layout"]["composition"]["pageNavigation"]=="none"
                captured("standalone")
                press("Home");home_settings();press("Right",25);press("Return");press("Right",11);press("Return")
                assert json.loads(layout.read_text())["composition"]["moviePage"]==page_id
                press("Escape");press("Escape");press("Escape")
                # Ctrl+M uses the same typed Movies action as a launcher; its destination is configurable.
                key(shell,"m",modifiers=4);time.sleep(1)
                captured("direct")
                press("Home");home_settings();press("Right",23);press("Return");press("Right");press("Return");press("Down",20);press("Return")
                saved=list((config/"profiles"/profile/"skins").glob("user-*.json"))
                assert len(saved)==1 and json.loads(saved[0].read_text())["layout"]["pages"][0]["id"]==page_id
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
                (evidence/"attempt-config.json").write_bytes(layout.read_bytes())
                (evidence/"result.json").write_text(json.dumps({"passed":True,"profile":profile,"page_id":page_id,"settings_only_creation_and_customization":True,"home_tab":True,"direct_launcher_destination":True,"navigation_modes":["left","top","none"],"real_kodi_episode_play_pause_stop_return":True,"skin_includes_pages":True,"media_unchanged":True,"visual_review_required":True,"executable":str(args.executable.resolve())},indent=2))
                print("Evidence: "+str(evidence),flush=True)
                return
            # Refresh initially empty Shows using its toolbar's Refresh button.
            key(shell, "Up")
            for _ in range(5): key(shell, "Right")
            key(shell, "Return"); time.sleep(1)
            key(shell, "Down"); key(shell, "Return"); time.sleep(1)  # Show → seasons.
            key(shell, "Return"); time.sleep(1)  # Specials → episode.
            key(shell, "Return")
            wait(lambda: rpc(directory, "Isolde.PlaybackStatus").get("state") == "playing", "Remote episode playback passed")
            key("kodi", "Return")
            wait(lambda: rpc(directory, "Isolde.PlaybackStatus").get("state") == "paused", "Remote episode pause passed")
            key("kodi", "Escape")
            wait(lambda: rpc(directory, "Isolde.PlaybackStatus").get("state") == "ended", "Remote episode stop and return passed")
            key(shell, "Escape"); key(shell, "Escape")  # Episode → seasons → shows.
            for p, digest in hashes.items(): assert hashlib.sha256(Path(p).read_bytes()).hexdigest() == digest
            (evidence / "result.json").write_text(json.dumps({"passed": True, "profile": profile, "shows": len(shows), "seasons": len(seasons), "episodes": len(eps), "media_unchanged": True}, indent=2))
            print("Evidence: " + str(evidence), flush=True)
    except Exception:
        kodi_log = userdata.parent / "temp/kodi.log"
        if kodi_log.exists(): print(kodi_log.read_text(errors="replace")[-6000:], flush=True)
        if args.inspect: print("Inspect: " + str(evidence), flush=True); time.sleep(20)
        raise
    finally:
        if process and process.poll() is None:
            process.terminate()
            try: process.wait(timeout=12)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
        x11.XCloseDisplay(display)
        if active: run("systemctl", "--user", "start", "isolde.service")


if __name__ == "__main__": main()
