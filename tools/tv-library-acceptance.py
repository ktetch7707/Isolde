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
import sqlite3
from datetime import datetime, timedelta, timezone
import signal
import importlib.util
from urllib.parse import unquote
import sys
import struct
import subprocess
from guide_features import remote_text
import tempfile
import time
import uuid
import zlib
import re
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
    parser.add_argument("--local-search", action="store_true", help="Real Kodi local search under kernel Internet-socket denial")
    parser.add_argument("--pages", action="store_true", help="Create and customize a page through Settings, then exercise its widget against real Kodi")
    parser.add_argument("--playlists", action="store_true", help="Verify saved video playlists against real Kodi in a new synthetic profile")
    parser.add_argument("--history", action="store_true", help="Verify actual Kodi playback updates independent production Recently viewed carousels")
    parser.add_argument("--movie-art", action="store_true", help="Scrape a named synthetic movie using Kodi's TMDB provider; no supplied metadata/art")
    parser.add_argument("--guide", action="store_true", help="Configure an isolated real IPTV Simple channel, then import and tune it through the Guide")
    parser.add_argument("--guide-surfaces", action="store_true", help="Also create two local channels through the UI and review compact/overlay/full return")
    parser.add_argument("--guide-lifecycle", action="store_true", help="Extend the real Guide journey with natural transitions, D-pad surfing and actual deleted-file repair")
    parser.add_argument("--guide-features", action="store_true", help="Extend the real Guide journey with native feeds, artwork, portability, templates and calendar controls")
    args = parser.parse_args()
    if args.guide_lifecycle:args.guide=args.guide_surfaces=True
    if args.guide_features:args.guide=args.guide_surfaces=True
    if args.guide_surfaces and not args.guide:parser.error("--guide-surfaces requires --guide")
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a"); fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    profile = str(uuid.uuid4())
    evidence = Path(tempfile.mkdtemp(prefix="isolde-tv-acceptance-"))
    print("Fixture evidence: " + str(evidence) + " Â· profile " + profile, flush=True)
    media = Path.home() / "Videos/Isolde-Acceptance" / ("Isolde-tv-test-" + profile)
    data = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share")))
    userdata = data / "Isolde/native-profiles" / profile / "kodi/home/.kodi/userdata"
    if media.exists() or userdata.exists(): raise RuntimeError("Fixture must be new")
    media.mkdir(parents=True); userdata.mkdir(parents=True, mode=0o700)
    if args.movie_art:
        (userdata/'advancedsettings.xml').write_text('<advancedsettings><loglevel hide="true">1</loglevel></advancedsettings>')
    if args.playlists:
        playlists=userdata/"playlists/video";playlists.mkdir(parents=True)
        (playlists/"Fixture episodes.xsp").write_text('<smartplaylist type="episodes"><name>Fixture episodes</name><match>all</match><rule field="tvshow" operator="is"><value>Isolde Television Fixture</value></rule><order direction="ascending">episode</order></smartplaylist>')
    show = media / "Isolde Television Fixture"; show.mkdir()
    clip = show / "Fixture S01E01.ogv"
    if args.movie_art:
        # Two real silent audio tracks exercise selection without making a
        # physical-output claim; twenty minutes permits default Kodi resume.
        command=['gst-launch-1.0','-q','oggmux','name=mux','!','filesink','location='+str(clip),
            'videotestsrc','num-buffers=6000','!','video/x-raw,width=320,height=180,framerate=5/1','!','theoraenc','!','queue','!','mux.']
        for _ in range(2):command+=['audiotestsrc','wave=silence','num-buffers=9375','samplesperbuffer=1024','!','audio/x-raw,rate=8000,channels=1','!','vorbisenc','!','queue','!','mux.']
        run(*command)
    else:
        run("gst-launch-1.0", "-q", "videotestsrc", f"num-buffers={600 if args.pages else 1800}", "!", "video/x-raw,width=320,height=180,framerate=15/1",
            "!", "theoraenc", "!", "oggmux", "!", "filesink", "location=" + str(clip))
    def chunk(name, content):
        return struct.pack(">I", len(content)) + name + content + struct.pack(">I", zlib.crc32(name + content))
    poster = show / "poster.png"
    poster.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 240, 360, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress((b"\x00" + b"\x55\x43\x99" * 240) * 360)) + chunk(b"IEND", b""))
    episodes = []
    fixture_episodes=[(0,1)]+[(1,n) for n in range(1,56)] if args.local_search else [(0,1),(1,1),(1,2)]
    for season, number in fixture_episodes:
        file = show / f"Fixture S{season:02}E{number:02}.ogv"
        if file != clip: os.link(clip,file) if args.local_search else file.write_bytes(clip.read_bytes())
        episodes.append(f"<episodedetails><title>Fixture {season}x{number}</title><showtitle>Isolde Television Fixture</showtitle>"
            f"<season>{season}</season><episode>{number}</episode><runtime>2</runtime><plot>Synthetic TV acceptance, no audio.</plot>"
            f"<filenameandpath>{escape(str(file))}</filenameandpath><path>{escape(str(show))}/</path><file>{file.name}</file>"
            f"<art><thumb>{escape(str(poster))}</thumb></art>"
            + ("<resume><position>20</position><total>120</total></resume>" if season == 1 and number == 2 else "") + "</episodedetails>")
    if args.playlists:
        member_file=playlists/'Member order.m3u8'
        member_paths=[show/f'Fixture S{season:02}E{number:02}.ogv' for season,number in fixture_episodes]
        member_records=[f'#EXTINF:120,Fixture member {i+1}\n{path}\n' for i,path in enumerate(member_paths)]
        member_file.write_text('#EXTM3U\n'+''.join(member_records))
        collision_file=playlists/'collision.xsp'
        collision_file.write_text('<smartplaylist type="tvshows"><name>Collision fixture</name><match>all</match><order direction="ascending">title</order></smartplaylist>')
        collision_bytes=collision_file.read_bytes()
    imported = media / "import"; imported.mkdir()
    (imported / "videodb.xml").write_text("<videodb><version>1</version><tvshow><title>Isolde Television Fixture</title>"
        f"<plot>Local synthetic television acceptance.</plot><path>{escape(str(show))}/</path><basepath>{escape(str(show))}/</basepath>"
        f"<art><poster>{escape(str(poster))}</poster></art>" + "".join(episodes) + "</tvshow><paths/></videodb>")
    exported_art = imported / "tvshows/Isolde_Television_Fixture"
    exported_art.mkdir(parents=True)
    if args.guide:
        # Native Kodi XML import, not database writes. Supply both the declared
        # local movie and Kodi's export-art companion path (Omega ImportFromXML).
        movie=media/'Fixture Movie.ogv';os.link(clip,movie)
        guide_movies=imported/'movies';guide_movies.mkdir();os.link(clip,guide_movies/'Fixture_Movie_2026.avi')
        document=(imported/'videodb.xml').read_text().replace('<tvshow><title>','<tvshow><studio>Cartoon Network</studio><title>',1)
        document=document.replace('<paths/>',f'<movie><title>Fixture Movie</title><year>2026</year><studio>Cartoon Network</studio><runtime>2</runtime><filenameandpath>{escape(str(movie))}</filenameandpath><path>{escape(str(media))}/</path><file>{movie.name}</file><basepath>{escape(str(media))}/</basepath><art><poster>{escape(str(poster))}</poster></art></movie><paths/>')
        (imported/'videodb.xml').write_text(document)
    if args.guide or args.history or args.local_search:
        # Kodi's native import resolves exported episode entries to these
        # export-relative filenames. Supply actual fixture media there rather
        # than relying on an ignored filenameandpath override in the XML.
        for season, number in fixture_episodes:
            os.link(clip,exported_art/f"s{season:02}e{number:02}.avi")
        if args.history:
            assert all((exported_art/f"s{season:02}e{number:02}.avi").is_file() for season,number in ((0,1),(1,1),(1,2)))
            print('History export-relative media fixture validated',flush=True)
    (exported_art / "poster.png").write_bytes(poster.read_bytes())
    (exported_art.parent / "Isolde_Television_Fixture-poster.png").write_bytes(poster.read_bytes())
    (userdata / "keymaps").mkdir()
    (userdata / "keymaps/tv-fixture.xml").write_text("<keymap><global><keyboard><f9>RunScript(special://profile/tv-fixture-import.py)</f9></keyboard></global></keymap>")
    (userdata / "mediasources.xml").write_text('<mediasources><network><location id="0">' + escape(str(imported)) + '/</location></network></mediasources>')
    (userdata / "tv-fixture-import.py").write_text('''import json, time, xbmc, xbmcgui
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
deadline=time.monotonic()+12
control=None
while time.monotonic()<deadline:
    control=next((index for index in range(30,3000) if xbmc.getInfoLabel("Control.GetLabel(%s)" % index).strip().lower()=="import library"),None)
    if control is not None:break
    xbmc.sleep(200)
if control is None:raise RuntimeError("Fixture bootstrap Import library control did not become ready")
xbmc.executebuiltin("SetFocus(%s)" % control)
xbmc.sleep(300)
xbmc.executebuiltin("Action(select)")
deadline=time.monotonic()+12
dialog=0
while time.monotonic()<deadline:
    dialog=xbmcgui.getCurrentWindowDialogId()
    if dialog>0:break
    xbmc.sleep(200)
if dialog<=0:raise RuntimeError("Fixture bootstrap import browser did not open")
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
    if args.movie_art:
        # Fresh source setup is performed by Isolde below, never a Kodi import dialog.
        movies = media / "movies"
        music = media / "music";music.mkdir()
        import wave
        for number in (1,2):
            song=music/f'Fixture song {number}.wav'
            with wave.open(str(song),'wb') as output:
                output.setnchannels(1);output.setsampwidth(2);output.setframerate(8000);output.writeframes(b'\0\0'*8000*60)
            tags=b'INFO'
            for tag,value in ((b'INAM',f'Fixture song {number}'),(b'IART','Isolde Fixture Artist'),(b'IPRD','Isolde Fixture Album')):
                raw=value.encode()+b'\0';tags+=tag+struct.pack('<I',len(raw))+raw+b'\0'*(len(raw)%2)
            data=song.read_bytes()+b'LIST'+struct.pack('<I',len(tags))+tags
            song.write_bytes(data[:4]+struct.pack('<I',len(data)-8)+data[8:])
        folder = movies / "Big Buck Bunny (2008)"; folder.mkdir(parents=True)
        movie_file = folder / "Big Buck Bunny (2008).ogv"
        os.link(clip, movie_file)
        # Kodi can update its bundled provider after first launch. Initialize
        # the updated provider BEFORE importing source settings, otherwise the
        # source keeps the old provider's incomplete artwork configuration.
        provider_setup = '''import xbmc, xbmcaddon, time
deadline=time.monotonic()+45
while True:
    try:
        provider=xbmcaddon.Addon("metadata.themoviedb.org.python")
        if provider.getSetting("fetch_posters") in ("true","false"): break
    except RuntimeError:
        pass  # Kodi registers bundled providers asynchronously on a fresh profile.
    if time.monotonic()>deadline: raise RuntimeError("TMDB provider artwork settings unavailable")
    xbmc.sleep(500)
for setting in ("fetch_posters","fanart","landscape","keyart"):
    if not provider.setSettingBool(setting,True): raise RuntimeError("Cannot configure TMDB "+setting)
provider.setSettingString("language","en-US")
provider.setSettingString("searchlanguage","en-US")
'''
        # Only provider default initialization remains a synthetic fixture setup.
        # It does not add sources, import a database, scan or open Kodi settings.
        (userdata / "tv-fixture-import.py").write_text(provider_setup)
        manifest=Path('/opt/isolde-managed-kodi/manifest.json')
        if not manifest.is_file() or json.loads(manifest.read_text()).get('feature')!='isolde-sources-v1':
            raise RuntimeError('Configure kodi-managed before fresh source acceptance')
    if args.guide:
        if not Path("/usr/share/kodi/addons/pvr.iptvsimple/addon.xml").is_file():
            raise RuntimeError("Run the bounded Configure guide action before this PVR acceptance case")
        playlist=media/"fixture.m3u";epg=media/"fixture.xml"
        playlist.write_text('#EXTM3U\n#EXTINF:-1 tvg-id="isolde-pvr-test" tvg-chno="901",Isolde PVR Test\n'+str(clip)+'\n')
        anchor=datetime.now(timezone.utc).replace(minute=0,second=0,microsecond=0)-timedelta(days=1)
        programs=[]
        for i in range(72):
            start=anchor+timedelta(hours=i);end=start+timedelta(hours=1)
            programs.append('<programme channel="isolde-pvr-test" start="'+start.strftime('%Y%m%d%H%M%S +0000')+'" stop="'+end.strftime('%Y%m%d%H%M%S +0000')+'"><title>Isolde PVR Test Pattern '+str(i)+'</title></programme>')
        epg.write_text('<?xml version="1.0" encoding="UTF-8"?><tv><channel id="isolde-pvr-test"><display-name>Isolde PVR Test</display-name></channel>'+''.join(programs)+'</tv>')
        addon=userdata/'addon_data/pvr.iptvsimple';addon.mkdir(parents=True)
        settings={'kodi_addon_instance_name':'Isolde PVR test','kodi_addon_instance_enabled':'true','m3uPathType':'0','m3uPath':str(playlist),'epgPathType':'0','epgPath':str(epg),'m3uCache':'false','epgCache':'false','startNum':'901','epgTimeShift':'0','epgTSOverride':'false'}
        (addon/'instance-settings-1.xml').write_text('<settings version="2">'+''.join('<setting id="'+key+'">'+escape(value)+'</setting>' for key,value in settings.items())+'</settings>')
        importer=userdata/'tv-fixture-import.py'
        importer.write_text('import xbmc\nxbmc.executeJSONRPC(\'{"jsonrpc":"2.0","id":1,"method":"Addons.SetAddonEnabled","params":{"addonid":"pvr.iptvsimple","enabled":true}}\')\nxbmc.sleep(1500)\n'+importer.read_text())
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in media.rglob("*") if p.is_file()}
    config = evidence / "config/Isolde"; config.mkdir(parents=True)
    (config / "profiles.json").write_text(json.dumps({"active": profile, "profiles": [{"id": profile, "name": "TV acceptance"}]}))
    (config / "device-setup.json").write_text('{"version":1,"step":6,"complete":true}')
    if args.movie_art:
        managed_art=config/'profiles'/profile/'artwork';managed_art.mkdir(parents=True)
        managed_poster=managed_art/(hashlib.sha256(poster.read_bytes()).hexdigest()+'.png')
        managed_poster.write_bytes(poster.read_bytes())
        (config/'profiles'/profile/'home-layout.json').write_text(json.dumps(dict(schemaVersion=1,rootPage=dict(name='Home',visible=True,position=0),categories=[dict(id='apps',name='Apps')],items=[dict(kind='app',id='movies',appId='movies',size='standard',categoryId='apps')],composition=dict(schemaVersion=1,enabled=False,headerControls=['settings']))))
    if args.history:
        import copy
        defaults=json.loads((Path(__file__).resolve().parents[1]/'config/studio-defaults.json').read_text())
        items=[]
        for identity,y,height,count,capacity in [('history-wide',0,3,5,3),('history-single',3,2,1,1)]:
            item=copy.deepcopy(defaults['componentPresets']['recently-viewed'])
            item.update(schemaVersion=1,id=identity,name=identity,placement=dict(x=0,y=y,width=8,height=height),
                source=dict(schemaVersion=1,provider='kodi',kind='recently-viewed',libraryKind='history',libraryView='watched',libraryValue=''))
            item['presentation'].update(libraryLimit=count,libraryColumns=capacity,libraryChrome=False)
            items.append(item)
        document=dict(schemaVersion=2,rootPage=dict(name='Home',visible=True,position=0),categories=[],components=items,
            composition=dict(schemaVersion=1,enabled=True,grid=dict(columns=8,rows=6,mode='screen'),headerControls=['settings'],showClock=False,navigation=[dict(id='home',name='Home')],initialTab='home'))
        history_layout=config/'profiles'/profile/'home-layout.json';history_layout.parent.mkdir(parents=True)
        history_layout.write_text(json.dumps(document));history_bytes=history_layout.read_bytes()
    if args.guide:
        guide_profile=config/'profiles'/profile;guide_profile.mkdir(parents=True)
        guide_home={'schemaVersion':1,'categories':[{'id':'apps','name':'Apps'}],'items':[{'kind':'action','id':'guide-entry','name':'TV Guide','symbol':'TV','size':'standard','categoryId':'apps','action':{'schemaVersion':1,'type':'open-guide'}}]}
        if args.guide_surfaces:
            defaults=json.loads((Path(__file__).resolve().parents[1]/'config/studio-defaults.json').read_text())
            widget=defaults['componentPresets']['tv-guide']
            widget.update(id='guide-widget',categoryId='apps',placement={'x':0,'y':1,'width':6,'height':4})
            widget['presentation']['libraryLimit']=3
            guide_home['items'][0]['placement']={'x':0,'y':0,'width':2,'height':1}
            guide_home['items'].append(widget)
            grid=dict(defaults['defaultGrid']);grid.update(columns=8,rows=6,mode='screen')
            guide_home['composition']={'schemaVersion':1,'enabled':True,'showClock':False,'grid':grid}
        (guide_profile/'home-layout.json').write_text(json.dumps(guide_home))
    (config / "MediaAccess.conf").write_text(f"[profiles]\n{profile}\\kodi\\folders={str(movies)+', '+str(music) if args.movie_art else media}\n")
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
    def windows():
        result = subprocess.run(["wmctrl", "-lp"], env=env, text=True, capture_output=True, timeout=5)
        # XWayland may exist before the compositor publishes its client list.
        # Retain diagnostics and let the existing bounded ready-wait observe it.
        if result.returncode:
            (evidence/"window-discovery.json").write_text(json.dumps({"exit":result.returncode,"error":result.stderr[:2048]}))
            return []
        return [line.split(None, 4) for line in result.stdout.splitlines() if len(line.split(None, 4)) >= 5]
    def guide_capture(phase):
        print(phase.upper()+"_READY "+str(evidence),flush=True)
        wait(lambda:(evidence/(phase+"-captured")).exists(),phase+" reviewed input/capture acknowledged",seconds=110)
    active = subprocess.run(["systemctl", "--user", "is-active", "--quiet", "isolde.service"]).returncode == 0
    process = None
    soak_ready = False
    try:
        if active: run("systemctl", "--user", "stop", "isolde.service")
        with (evidence / "shell.log").open("w") as log:
            command=[str(args.executable.resolve())] + ([] if args.pages else ["--windowed"]) + ["--launch-app=shows"]
            offline=None
            if args.local_search:
                spec=importlib.util.spec_from_file_location('local_search_offline',Path(__file__).with_name('guide-soak-acceptance.py'))
                offline=importlib.util.module_from_spec(spec);spec.loader.exec_module(offline)
                probe=offline.SOCKET_PROBE.replace("'--windowed'","'--windowed','--launch-app=shows'")
                command=[sys.executable,'-c',probe,str(evidence),str(args.executable.resolve())]
            process = subprocess.Popen(command, env=env, stdout=log, stderr=log,preexec_fn=offline.install_socket_filter if offline else None)
            shell = wait(lambda: next((r[0] for r in windows() if r[2] == str(process.pid)), None), "Shell ready")
            kodi = wait(lambda: next((r[0] for r in windows() if "Kodi" in r[-1]), None), "Kodi ready")
            directory = userdata / "isolde"
            wait(lambda: rpc(directory, "JSONRPC.Version"), "Private bridge ready")
            time.sleep(6)  # Finish startup-window replacement and managed minimization.
            kodi = next(r[0] for r in windows() if "Kodi" in r[-1])
            if not args.movie_art:
                run("wmctrl", "-i", "-a", kodi, env=env)
                x11.XSetInputFocus(display, int(kodi, 16), 2, 0); x11.XSync(display, 0)
            time.sleep(1)
            if args.guide:guide_capture('guidestartup')
            elif not args.movie_art:
                key("kodi", "Escape"); key("kodi", "Escape")  # Dismiss first-run dialogs in this new test home.
                key("kodi", "F9")
            else:
                key("kodi","F9")  # Fixture provider defaults only; never a library/source operation.
                run("wmctrl","-i","-a",shell,env=env)
                initial=rpc(directory,'Isolde.GetSources')
                initial_sources=initial['sources']
                assert not any(row['editable'] for row in initial_sources),'Fresh fixture already has a configured granted-folder source'
                initial_source_bytes=(userdata/'sources.xml').read_bytes() if (userdata/'sources.xml').exists() else None
                def source_press(name,count=1):
                    for _ in range(count):key(shell,name,.08)
                    time.sleep(.3)
                source_press('Home');source_press('Up',20);source_press('Left',10);source_press('Return')
                source_press('Up',20);source_press('Down',7);source_press('Return')
                source_press('Down',12);source_press('Return');guide_capture('mediacapabilities')
                source_press('Return');time.sleep(1)  # Capability guide's named Source setup action.
                guide_capture('sourceempty')
                choices=[row for row in initial['scrapers'] if row['content']=='movies']
                provider_index=next(i for i,row in enumerate(choices) if row['id']=='metadata.themoviedb.org.python')
                default_index=next((i for i,row in enumerate(choices) if row['id']=='metadata.local'),0)
                def source_draft():
                    source_press('Up',30);source_press('Down',2);source_press('Return');source_press('Return')
                    source_press('Down',2)
                    source_press('Return',(provider_index-default_index)%len(choices))
                    source_press('Down',5);source_press('Return');time.sleep(.7)
                source_draft();guide_capture('sourcepreview')
                assert rpc(directory,'Isolde.GetSources')['sources']==initial_sources,'Preview saved a source'
                source_press('Escape');source_press('Escape')
                assert rpc(directory,'Isolde.GetSources')['sources']==initial_sources,'Cancelled draft saved a source'
                assert ((userdata/'sources.xml').read_bytes() if (userdata/'sources.xml').exists() else None)==initial_source_bytes,'Preview/cancel rewrote Kodi source configuration'
                source_draft();source_press('Down',30);source_press('Return');source_press('Return')
                def saved_source():
                    rows=rpc(directory,'Isolde.GetSources')['sources']
                    return rows if len(rows)==len(initial_sources)+1 else None
                saved=wait(saved_source,'Source saved through Isolde preview and confirmation')
                target_source=next(row for row in saved if row['path']==str(movies)+'/')
                assert target_source['content']=='movies' and target_source['scraper']=='metadata.themoviedb.org.python',saved
                assert target_source['recursive'] is True
                guide_capture('sourcesaved')
                source_press('Down',2);source_press('Return');source_press('Down');source_press('Return')
                source_press('Down');source_press('Return',3);source_press('Down',2);source_press('Return')
                source_press('Down',30);source_press('Return');source_press('Return')
                def saved_music():
                    rows=rpc(directory,'Isolde.GetSources')['sources']
                    return rows if any(row['path']==str(music)+'/' and row['category']=='music' for row in rows) else None
                saved=wait(saved_music,'Music source saved through the same remote setup workflow')
                guide_capture('musicsource')
                # Exercise real Kodi transaction/recovery against a denied
                # sources.xml publication, solely in this disposable profile.
                source_bytes=(userdata/'sources.xml').read_bytes()
                source_index=saved.index(target_source)
                source_press('Up',20);source_press('Down',3+source_index);source_press('Return')
                source_press('Down',5);source_press('Return');source_press('Down',2);source_press('Return')
                source_press('Down',30);source_press('Return')
                original_mode=userdata.stat().st_mode&0o777
                try:
                    userdata.chmod(0o500);source_press('Return')
                    def recovery_retained():
                        request=json.loads((directory/'source-request.json').read_text())
                        reply=json.loads((directory/'source-reply.json').read_text())
                        return (directory/'source-journal.json').exists() and request.get('operation')=='restore' and reply.get('token')==request.get('token') and reply.get('ok') is False
                    wait(recovery_retained,'Native source write failure retained its recovery journal')
                    time.sleep(.6);guide_capture('sourcefailed')
                    assert (userdata/'sources.xml').read_bytes()==source_bytes,'Failed source save changed source XML'
                finally:userdata.chmod(original_mode)
                source_press('Escape');source_press('Escape');source_press('Up',20);source_press('Return')
                wait(lambda:not (directory/'source-journal.json').exists(),'Isolde Refresh recovered the interrupted native source transaction')
                assert (userdata/'sources.xml').read_bytes()==source_bytes,'Recovery did not restore exact source XML'
                recovered=rpc(directory,'Isolde.GetSources')['sources']
                (evidence/'source-recovery-inventory.json').write_text(json.dumps(dict(before=saved,after=recovered),indent=2))
                canonical=lambda rows:sorted(json.dumps(row,sort_keys=True) for row in rows)
                print('SOURCE_RECOVERY_INVENTORY '+json.dumps(dict(before=saved,after=recovered),sort_keys=True),flush=True)
                assert canonical(recovered)==canonical(saved),('Recovery changed scanner/provider settings',saved,recovered)
                saved=recovered;source_index=saved.index(target_source)
                guide_capture('sourcerecovered')
                source_press('Escape');source_press('Up',30);source_press('Down',2);source_press('Return')
                (evidence/'source-setup.json').write_text(json.dumps(dict(fresh=True,previewNoWrite=True,cancelNoWrite=True,remoteConfirmedSave=True,nativeWriteFailureRecovery=True,nativeSources=saved,kodiMenusRequired=False),indent=2))
            if args.guide:
                source_channels=wait(lambda:rpc(directory,'PVR.GetChannels',{'channelgroupid':'alltv','properties':['channelnumber','locked','hidden','clientid','uniqueid'],'limits':{'start':0,'end':50}}).get('channels'),'Real IPTV Simple channel ready',seconds=60)
                assert len(source_channels)==1 and source_channels[0]['label']=='Isolde PVR Test',source_channels
                source_channel=source_channels[0]
                source_epg=wait(lambda:rpc(directory,'PVR.GetBroadcasts',{'channelid':source_channel['channelid'],'properties':['title','starttime','endtime'],'limits':{'start':0,'end':50}}).get('broadcasts'),'Real XMLTV source listings ready',seconds=60)
                (evidence/'pvr-source.json').write_text(json.dumps({'channel':source_channel,'broadcasts':source_epg},indent=2))
                database=guide_profile/'guide.sqlite'
                def imported_channels():
                    if not database.exists():return []
                    with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
                        return [json.loads(row[0]) for row in db.execute('SELECT definition FROM channels')]
                run('wmctrl','-i','-a',shell,env=env);guide_capture('guideorigin');guide_capture('guidehome');guide_capture('guideempty')
                assert imported_channels()==[],"Source preview wrote before confirmation"
                guide_capture('guidesource')
                wait(lambda:len(imported_channels())==1,'Guide source import committed')
                cached=imported_channels()[0];assert cached['owner']=='kodi-pvr' and cached['source']['channelId']==source_channel['channelid']
                expected_title='Isolde PVR Test Pattern '+str(int((datetime.now(timezone.utc)-anchor).total_seconds()//3600))
                with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
                    current=db.execute('SELECT title FROM slots WHERE start_utc<=? AND end_utc>?',(int(time.time()),int(time.time()))).fetchone()
                assert current and current[0]==expected_title,(current,expected_title)
                guide_capture('guidepvr')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Guide tuned actual Kodi PVR channel')
                time.sleep(3);guide_capture('guidevideo')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','PVR stop returned from owned playback')
                def shell_active():
                    active_window=run('xprop','-root','_NET_ACTIVE_WINDOW',env=env,timeout=5).strip().split()[-1]
                    return int(active_window,16)==int(shell,16)
                wait(shell_active,'Automatic native playback focus returned to shell',seconds=8)
                guide_capture('guidereturn')
                assert imported_channels()==[cached]
                surface_phases=[]
                if args.guide_surfaces:
                    # Every write below is performed through the reviewed remote UI.
                    # SQLite is opened read-only solely to verify committed results.
                    guide_capture('guidelocaldraft')
                    assert len(imported_channels())==1,'Opening a local draft saved a channel'
                    guide_capture('guidelocalpreview')
                    assert len(imported_channels())==1,'Preview saved a local channel before reviewed confirmation'
                    time.sleep(2)  # Let the bounded three-item fixture preview finish before reviewing Save.
                    guide_capture('guidelocalreview')
                    wait(lambda:len(imported_channels())==2,'Local channel committed through Preview and Save')
                    local=next(c for c in imported_channels() if c.get('owner')!='kodi-pvr')
                    with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
                        candidates=json.loads(db.execute('SELECT candidates FROM channels WHERE id=?',(local['id'],)).fetchone()[0])
                        initial_slots=db.execute('SELECT id,media_type,media_id,title,start_utc,end_utc,status FROM slots WHERE channel=? ORDER BY start_utc LIMIT 50',(local['id'],)).fetchall()
                        slot_count=db.execute('SELECT COUNT(*) FROM slots WHERE channel=?',(local['id'],)).fetchone()[0]
                    assert candidates and initial_slots and slot_count>=len(initial_slots)
                    local_creation={'definition':local,'candidateCount':len(candidates),'candidates':[{k:item[k] for k in ('mediaType','mediaId','title','duration') if k in item} for item in candidates[:50]],'slotCount':slot_count,'initialSlots':initial_slots}
                    (evidence/'guide-surfaces-cache.json').write_text(json.dumps({'localCreation':local_creation},indent=2))
                    time.sleep(5)  # Require a real nonzero scheduled seek, not a restart-at-zero claim.
                    guide_capture('guidelocalcreated')
                    last_local_status=None
                    def local_playing():
                        nonlocal last_local_status
                        value=rpc(directory,'Isolde.PlaybackStatus')
                        status={k:value[k] for k in ('state','reason','home','canPause','canSeek') if k in value}
                        if status!=last_local_status:
                            (evidence/'guide-surfaces-cache.json').write_text(json.dumps({'localCreation':local_creation,'localStatus':status},indent=2));last_local_status=status
                        if value.get('state') in ('failed','replaced'):
                            raise AssertionError('Local scheduled playback '+value['state']+': '+str(value.get('reason','No playback reason supplied')))
                        return value if value.get('state')=='playing' else None
                    local_state=wait(local_playing,'Local channel began managed scheduled playback')
                    assert local_state.get('canPause') is True,'Local fixture requires known pause/OSD capability before reviewed Stop'
                    players=rpc(directory,'Player.GetActivePlayers');assert len(players)==1 and players[0].get('type')=='video',players
                    player=players[0]['playerid'];assert type(player) is int and player==1
                    item=rpc(directory,'Player.GetItem',{'playerid':player,'properties':[]})['item']
                    observed_at=time.time();position=rpc(directory,'Player.GetProperties',{'playerid':player,'properties':['time']})['time']
                    actual_seconds=position['hours']*3600+position['minutes']*60+position['seconds']+position.get('milliseconds',0)/1000
                    with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
                        current=db.execute('SELECT id,media_type,media_id,start_utc,end_utc FROM slots WHERE channel=? AND start_utc<=? AND end_utc>?',(local['id'],int(observed_at),int(observed_at))).fetchone()
                    local_playback={'observedItem':{k:item.get(k) for k in ('id','type')},'actualPositionSeconds':actual_seconds,'observedAtUtc':observed_at,'savedSlot':current,'expectedOffsetSeconds':observed_at-current[3] if current else None,'toleranceSeconds':3}
                    (evidence/'guide-surfaces-cache.json').write_text(json.dumps({'localCreation':local_creation,'localPlayback':local_playback},indent=2))
                    assert current and item.get('type')==current[1] and item.get('id')==current[2],(current,item)
                    expected_seconds=observed_at-current[3];assert expected_seconds>=5 and abs(actual_seconds-expected_seconds)<=3,(actual_seconds,expected_seconds)
                    local_playback.update(slotId=current[0],mediaType=current[1],mediaId=current[2],startUtc=current[3],endUtc=current[4],canPause=True)
                    guide_capture('guidelocalvideo')
                    guide_capture('guidelocalosd')
                    def local_ended():
                        nonlocal last_local_status
                        value=rpc(directory,'Isolde.PlaybackStatus')
                        status={k:value[k] for k in ('state','reason','home','canPause','canSeek') if k in value}
                        if status!=last_local_status:
                            (evidence/'guide-surfaces-cache.json').write_text(json.dumps({'localCreation':local_creation,'localPlayback':local_playback,'localStatus':status},indent=2));last_local_status=status
                        if value.get('state') in ('failed','replaced'):
                            raise AssertionError('Local stop acknowledgement '+value['state']+': '+str(value.get('reason','No playback reason supplied')))
                        return value.get('state')=='ended'
                    wait(local_ended,'Local scheduled playback stopped')
                    wait(shell_active,'Automatic local playback focus returned to shell',seconds=8)
                    guide_capture('guidelocalreturn')
                    guide_capture('guideduplicatedraft')
                    wait(lambda:len(imported_channels())==3,'Reviewed duplicate local channel committed')
                    lineup=imported_channels();native=[c for c in lineup if c.get('owner')!='kodi-pvr']
                    assert len(native)==2 and len({c['id'] for c in lineup})==3 and len({c['number'] for c in lineup})==3,lineup
                    assert native[0]['rule']==native[1]['rule'] and native[0]['sequence']==native[1]['sequence']
                    assert next(c for c in lineup if c.get('owner')=='kodi-pvr')==cached
                    def cached_slots():
                        with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
                            return list(db.execute('SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots ORDER BY channel,start_utc'))
                    committed_slots=cached_slots()
                    guide_capture('guidelineup')
                    guide_capture('guidewidgetentry')
                    wall=int(time.time());current_rows=[]
                    with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:
                        for channel in lineup:
                            current=db.execute('SELECT title,start_utc,end_utc FROM slots WHERE channel=? AND start_utc<=? AND end_utc>?',(channel['id'],wall,wall)).fetchone()
                            assert current,(channel,wall)
                            progress=(wall-current[1])/(current[2]-current[1]);assert 0<=progress<1
                            current_rows.append({'id':channel['id'],'number':channel['number'],'name':channel['name'],'source':'Kodi PVR' if channel.get('owner')=='kodi-pvr' else 'Local','title':current[0],'startUtc':current[1],'endUtc':current[2],'progress':progress})
                    (evidence/'guide-surfaces-cache.json').write_text(json.dumps({'atUtc':wall,'rows':current_rows,'widgetId':'guide-widget','pvrChannelId':cached['id'],'localCreatedId':local['id'],'localCreation':local_creation,'localPlayback':local_playback},indent=2))
                    for phase in ('guidecompact','guideoverlay','guideexpanded','guideoverlayreturn','guidecompactreturn'):guide_capture(phase)
                    assert imported_channels()==lineup,'Browsing surfaces changed saved channel definitions'
                    assert cached_slots()==committed_slots,'Browsing surfaces regenerated or changed committed slots'
                    soak_ready=True
                    surface_phases=['guidelocaldraft','guidelocalpreview','guidelocalreview','guidelocalcreated','guidelocalvideo','guidelocalosd','guidelocalreturn','guideduplicatedraft','guidelineup','guidewidgetentry','guidecompact','guideoverlay','guideexpanded','guideoverlayreturn','guidecompactreturn']
                lifecycle={}
                features={}
                if args.guide_lifecycle:
                    from guide_lifecycle import exercise,PHASES
                    lifecycle=exercise(evidence,media,database,directory,local,native,rpc,guide_capture,wait,shell_active)
                    surface_phases+=PHASES
                    soak_ready=True
                if args.guide_features:
                    from guide_features import exercise,PHASES
                    features=exercise(evidence,media,database,directory,local,poster,rpc,guide_capture,wait,shell_active,lambda name:key(shell,name,delay=0.06))
                    surface_phases+=PHASES
                    soak_ready=True
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,path
                result={'passed':True,'freshProfile':True,'normalProfileUsed':False,'pvrAddon':'pvr.iptvsimple','sourceChannels':len(source_channels),'sourceListings':len(source_epg),'remoteImport':True,'actualPvrPlayback':True,'returned':True,'automaticNativeReturn':True,'mediaUnchanged':True,'syntheticSource':True,'providerSubscriptionVerified':False,'captures':['guidestartup','guideorigin','guidehome','guideempty','guidesource','guidepvr','guidevideo','guidereturn']+surface_phases,'visualReview':'pending','profile':profile}
                if args.guide_surfaces:result.update(nativeChannelsCreated=2,cachedChannelCount=3,surfaceJourneyInputAcknowledged=True,committedSlotsUnchanged=True,sourceCachePreserved=True,actualLocalElapsedSeek=True,automaticLocalReturn=True)
                if args.guide_lifecycle:
                    timings=[int(value) for value in re.findall(r'T11_GUIDE_FIRST_PAINT_MS (\d+) CHANNELS 3\b',(evidence/'shell.log').read_text(errors='replace'))]
                    assert timings and max(timings)<=1000,('Populated cached Guide first paint exceeded one second',timings)
                    result.update({key:lifecycle[key] for key in ('continuousTransition','remoteSurf','actualDeletedFile','targetedRepair','unaffectedSlotsPreserved')})
                    result['cachedFirstPaintMs']=timings
                if args.guide_features:result.update(features)
                (evidence/'result.json').write_text(json.dumps(result,indent=2))
                return
            if args.movie_art:
                query = {"properties":["title","year","file","art","uniqueid"],"limits":{"start":0,"end":50}}
                movie_rows = wait(lambda: rpc(directory,"VideoLibrary.GetMovies",query).get("movies"),
                              "Kodi TMDB matched named synthetic movie", seconds=150)
                assert len(movie_rows)==1 and movie_rows[0]["title"]=="Big Buck Bunny" and movie_rows[0]["year"]==2008
                movie=movie_rows[0]
                assert movie["file"]==str(movie_file) and str(movie.get("uniqueid",{}).get("tmdb"))=="10378"
                assert any("image.tmdb.org" in value for value in movie["art"].values()), movie
                def artwork():
                    result=rpc(directory,"Isolde.GetArtwork",{"kind":"movie","id":movie["movieid"]})
                    return result if result.get("data") else None
                art=wait(artwork,"Real Kodi artwork delivered through Isolde bridge",seconds=60)
                raw=base64.b64decode(art["data"],validate=True)
                assert len(raw)>4096 and art.get("mime") in {"image/jpeg","image/png"}
                from PIL import Image
                import io
                image=Image.open(io.BytesIO(raw)); image.verify()
                (evidence/"movie-artwork.jpg").write_bytes(raw)
                # Open Movies using only the remote-operated Home card.
                run("wmctrl","-i","-a",shell,env=env)
                def open_movie_card():
                    source_press('Home');source_press('Up',20);source_press('Down');source_press('Return');time.sleep(1)
                    source_press('Up',20);source_press('Down')
                open_movie_card();time.sleep(2)
                print("MOVIEART_READY "+str(evidence),flush=True)
                wait(lambda:(evidence/"movieart-captured").exists(),"Movie artwork frame captured",seconds=110)
                def poster_capture(phase):
                    print(phase.upper()+"_READY "+str(evidence),flush=True)
                    wait(lambda:(evidence/(phase+"-captured")).exists(),phase+" captured",seconds=110)
                def title_art():
                    values=rpc(directory,"VideoLibrary.GetMovieDetails",{"movieid":movie['movieid'],"properties":["art"]})['moviedetails']['art']
                    return {name:unquote(value[8:-1]) if value.startswith('image://') and value.endswith('/') else value for name,value in values.items()}
                original_art=title_art()
                key(shell,'Menu');key(shell,'Left');time.sleep(.5);poster_capture('movieposterpreview')
                assert title_art()==original_art,'Poster preview wrote Kodi metadata'
                key(shell,'Return');key(shell,'Escape');key(shell,'Escape')
                assert title_art()==original_art,'Cancelled poster confirmation wrote Kodi metadata'
                key(shell,'Left');key(shell,'Return');key(shell,'Return')
                changed=wait(lambda: (value if (value:=title_art()).get('poster','').split('/')[-1].startswith('isolde-') else None),'Remote poster apply acknowledged by Kodi')
                reported_path=Path(changed['poster'])
                assert reported_path.parent in {userdata/'Thumbnails',Path.home()/'.kodi/userdata/Thumbnails'},reported_path
                assert re.fullmatch(r'isolde-[0-9a-f]{64}\.png',reported_path.name),reported_path
                applied_path=userdata/'Thumbnails'/reported_path.name
                assert not applied_path.is_symlink() and applied_path.is_file(),applied_path
                applied=Image.open(applied_path)
                assert applied.size==(240,360) and applied.convert('RGB').getpixel((0,0))==(85,67,153)
                assert all(changed.get(name)==value for name,value in original_art.items() if name!='poster'),'Poster edit changed unrelated title art'
                time.sleep(.5);key(shell,'Escape');key(shell,'Escape');poster_capture('movieposterapplied')
                key(shell,'Return')
                playback_trace=[]
                def fresh_movie_playing():
                    status=rpc(directory,'Isolde.PlaybackStatus')
                    if not playback_trace or status!=playback_trace[-1]:
                        playback_trace.append(status)
                        players=rpc(directory,'Player.GetActivePlayers')
                        actual=[rpc(directory,'Player.GetItem',{'playerid':row['playerid'],'properties':[]}) for row in players]
                        print('FRESH_PLAYBACK_STATE '+json.dumps(dict(status=status,players=players,items=actual)),flush=True)
                    return status.get('state')=='playing'
                wait(fresh_movie_playing,'Fresh Isolde source played its real local movie without Kodi menus')
                poster_capture('moviefreshplaying')
                key('kodi','Menu')
                controls=wait(lambda:next((row[0] for row in windows() if row[2]==str(process.pid) and row[-1]=='Isolde playback controls'),None),'Fresh native Menu opened Isolde playback controls')
                poster_capture('moviefreshcontrols')
                for _ in range(3):key(controls,'Down',.1)
                key(controls,'Return');key(controls,'Down');key(controls,'Return')
                wait(lambda:any(row['id']==1 and row['selected'] for row in rpc(directory,'Isolde.GetPlaybackOptions')['audio']),'Remote audio selection changed the real Kodi audio stream')
                poster_capture('moviefreshaudio');key(controls,'Escape')
                for _ in range(20):key(controls,'Up',.04)
                key(controls,'Down');key(controls,'Down')
                for _ in range(30):key(controls,'Return',.35)
                def player_seconds():
                    value=rpc(directory,'Player.GetProperties',{'playerid':1,'properties':['time']})['time']
                    return value.get('hours',0)*3600+value.get('minutes',0)*60+value.get('seconds',0)
                position=wait(lambda:(seconds if (seconds:=player_seconds())>180 else None),'Remote seek passed Kodi default resume threshold')
                key(controls,'Escape');key('kodi','Escape')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Fresh movie stopped through managed Back')
                def stored_resume():
                    value=rpc(directory,'VideoLibrary.GetMovieDetails',{'movieid':movie['movieid'],'properties':['resume']})['moviedetails']['resume']
                    return value if value.get('position',0)>180 else None
                resume=wait(stored_resume,'Kodi persisted actual movie resume progress')
                run('wmctrl','-i','-a',shell,env=env);poster_capture('moviefreshreturned')
                # The preference changes in Settings; a library launch must
                # remain direct even while discovery would show viewing choices.
                source_press('Home');source_press('Up',20);source_press('Left',10);source_press('Return')
                source_press('Up',20);source_press('Down',7);source_press('Return')
                source_press('Up',20);source_press('Down',10);source_press('Return');time.sleep(1)
                source_press('Down',3+source_index);source_press('Return')
                source_press('Down',5);source_press('Return');source_press('Down',2);source_press('Return')
                source_press('Down',30);source_press('Return');source_press('Return')
                wait(lambda:any(row['path']==str(movies)+'/' and row['noUpdate'] for row in rpc(directory,'Isolde.GetSources')['sources']),'Remote scanner edit applied to the populated library')
                assert stored_resume()==resume,'Editing source settings changed existing Kodi progress'
                poster_capture('sourceprogresspreserved')
                source_press('Escape');source_press('Up');source_press('Return')
                poster_capture('localplaybackon');source_press('Return');poster_capture('localplaybackoff')
                preference=(config/'Kodi.conf').read_text()
                assert 'alwaysPlayLocally=false' in preference,'Remote preference toggle was not persisted'
                open_movie_card();key(shell,'Return')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Preference OFF kept library playback direct')
                wait(lambda:player_seconds()>=resume['position']-3,'Kodi resumed from its stored position')
                poster_capture('movieresumed')
                key('kodi','Home')
                wait(lambda:(value if (value:=rpc(directory,'Isolde.PlaybackStatus')).get('state')=='ended' and value.get('home') is True else None),'Native Home stopped playback and returned to Isolde')
                run('wmctrl','-i','-a',shell,env=env);poster_capture('moviefreshhome')
                open_movie_card();source_press('Up',20);source_press('Right',20);source_press('Return')
                # Music toolbar opens albums; scan stays a separate deliberate
                # action in Media Settings, using Kodi's own tag reader.
                source_press('Home');source_press('Up',20);source_press('Left',10);source_press('Return')
                source_press('Up',20);source_press('Down',7);source_press('Return')
                source_press('Up',30);source_press('Down',11);source_press('Return')
                songs=wait(lambda:rpc(directory,'AudioLibrary.GetSongs',{'properties':['title','album','artist']}).get('songs'),'Kodi indexed real tagged music',seconds=60)
                assert len(songs)==2 and all(row.get('album')=='Isolde Fixture Album' for row in songs),songs
                open_movie_card();source_press('Up',20);source_press('Right',20);source_press('Return');time.sleep(1)
                poster_capture('musicalbums');source_press('Up',20);source_press('Down');source_press('Return');time.sleep(1)
                poster_capture('musicsongs');source_press('Up',20);source_press('Down');source_press('Return')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Remote song playback')
                key('kodi','Menu');controls=wait(lambda:next((row[0] for row in windows() if row[2]==str(process.pid) and row[-1]=='Isolde playback controls'),None),'Music controls owned by Isolde')
                poster_capture('musiccontrols');key(controls,'Return')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='paused','Music transport pause')
                key(controls,'Escape');key('kodi','Escape')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Music stopped and returned')
                run('wmctrl','-i','-a',shell,env=env);poster_capture('musicreturned')
                for path,digest in hashes.items(): assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest, path
                (evidence/"result.json").write_text(json.dumps({"passed":True,"profile":profile,"freshProfile":True,
                    "normalProfileUsed":False,"scrapedByKodi":True,"metadataSupplied":False,"playbackVerified":True,
                    "movie":movie,"artworkBytes":len(raw),"artworkSha256":hashlib.sha256(raw).hexdigest(),
                    "artworkMime":art["mime"],"media_unchanged":True,"remotePosterPreviewNoWrite":True,
                    "remotePosterCancelNoWrite":True,"remotePosterApplied":True,"unrelatedTitleArtPreserved":True,
                    "freshSourceSetupThroughIsolde":True,"sourcePreviewCancelNoWrite":True,"nativeSourceWriteFailureRecovery":True,"capabilityGuideRemoteRoute":True,"kodiMenusRequired":False,
                    "freshPlaybackReturnThroughIsolde":True,"actualKodiResumeSavedAndRestored":True,"preferenceOffLibraryDirect":True,"remoteHomeMoviesRoute":True,"realAudioTrackSelection":True,"sourceEditPreservesProgress":True,"remoteMusicSourceBrowsePlayback":True,"seekPosition":position,"storedResume":resume,
                    "captures":["mediacapabilities","sourceempty","sourcepreview","sourcesaved","sourcefailed","sourcerecovered","movieart","movieposterpreview","movieposterapplied","moviefreshplaying","moviefreshcontrols","moviefreshaudio","moviefreshreturned","sourceprogresspreserved","localplaybackon","localplaybackoff","movieresumed","moviefreshhome","musicsource","musicalbums","musicsongs","musiccontrols","musicreturned"]},indent=2))
                return
            shows = wait(lambda: rpc(directory, "VideoLibrary.GetTVShows", {"properties": ["title"]}).get("tvshows"), "Kodi imported TV fixture")
            show_id = next(s["tvshowid"] for s in shows if s["title"] == "Isolde Television Fixture")
            seasons = rpc(directory, "VideoLibrary.GetSeasons", {"tvshowid": show_id, "properties": ["season"]})["seasons"]
            assert {s["season"] for s in seasons} == {0, 1}
            eps = rpc(directory, "VideoLibrary.GetEpisodes", {"tvshowid": show_id, "properties": ["season", "episode", "resume"]})["episodes"]
            if args.local_search:
                def complete_search_import():
                    imported=[]
                    for start in (0,50):
                        imported+=rpc(directory,'VideoLibrary.GetEpisodes',{'tvshowid':show_id,'properties':['season','episode','resume'],'limits':{'start':start,'end':start+50}}).get('episodes',[])
                    return imported if len(imported)==56 else None
                eps=wait(complete_search_import,'Kodi imported all56 search fixture episodes',seconds=60)
            assert len(eps) == (56 if args.local_search else 3)
            if args.local_search:
                network=json.loads((evidence/'network-denial.json').read_text())
                assert network['socketErrnos']==[101,101] and network['unixSocketAvailable'] is True
                assert network['pid']==process.pid
                phases=[];inputs=[]
                def press(name):inputs.append(name);key(shell,name,.08)
                def capture(phase):phases.append(phase);guide_capture(phase)
                run('wmctrl','-i','-a',shell,env=env)
                press('Home');time.sleep(2);capture('localsearchorigin')
                press('s');capture('localsearchentry');press('Return')
                remote_text(press,'fixture');time.sleep(3);capture('localsearchresults')
                for _ in range(60):press('Down')
                capture('localsearchmore');press('Return');time.sleep(2);capture('localsearchnext')
                # The busy result clamps selection to the Previous Episodes row.
                press('Return');time.sleep(2);capture('localsearchprevious')
                for _ in range(60):press('Down')
                press('Return');time.sleep(2);press('Down');press('Return')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Search result starts real Kodi playback')
                capture('localsearchplaying');key('kodi','Escape')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Search playback ends')
                time.sleep(2);run('wmctrl','-i','-a',shell,env=env);capture('localsearchplayreturn')
                press('Escape');capture('localsearchreturned')
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
                (evidence/'result.json').write_text(json.dumps(dict(passed=True,journey='local-search',freshProfile=True,normalProfileUsed=False,realKodi=True,shows=shows,episodes=eps,network=network,captures=phases,actualInputActions=len(inputs),inputTrace=inputs,mediaUnchanged=True,visualReview='pending'),indent=2))
                print('Evidence: '+str(evidence),flush=True);return
            if args.history:
                phases=[]
                def history_capture(phase):
                    phases.append(phase);print(phase.upper()+"_READY "+str(evidence),flush=True)
                    wait(lambda:(evidence/(phase+"-captured")).exists(),phase+" captured",seconds=110)
                def press(name,count=1):
                    for _ in range(count):key(shell,name,.06)
                    time.sleep(.4)
                def viewed():
                    return rpc(directory,'VideoLibrary.GetEpisodes',{'properties':['title','lastplayed'],
                        'filter':{'field':'lastplayed','operator':'after','value':'1970-01-01'},
                        'sort':{'method':'lastplayed','order':'descending'},'limits':{'start':0,'end':50}}).get('episodes',[])
                assert not viewed(),'Fresh imported fixtures must not masquerade as viewed media'
                run('wmctrl','-i','-a',shell,env=env)
                history_capture('historylibrary')
                # Use the ordinary Isolde library, not a history/date setter.
                press('Up');press('Left',30);press('Right',30);press('Return');time.sleep(1)
                press('Down');press('Return');time.sleep(1);press('Return');time.sleep(1);press('Return')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Initial real episode playback')
                time.sleep(3);history_capture('historyplaying')
                key('kodi','Escape');wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Actual episode stop')
                watched=wait(viewed,'Kodi-owned lastplayed updated after actual playback')
                assert len(watched)==1 and watched[0].get('lastplayed')
                time.sleep(2);run('wmctrl','-i','-a',shell,env=env);press('Home');press('Up',12);press('Down',2)
                history_capture('historyupdated')
                press('Menu');history_capture('historyfocused');press('Escape');press('Return')
                wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Production history card reopens episode')
                history_capture('historyreplaying')
                key('kodi','Escape');wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','History playback return')
                time.sleep(2);run('wmctrl','-i','-a',shell,env=env);history_capture('historyreturned')
                press('Return');wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='playing','Exact history origin reopens without navigation')
                key('kodi','Escape');wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Repeated history playback return')
                time.sleep(2);run('wmctrl','-i','-a',shell,env=env);history_capture('historyrepeatreturn')
                assert history_layout.read_bytes()==history_bytes,'Playback/history must not rewrite the saved layout'
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
                (evidence/'result.json').write_text(json.dumps(dict(passed=True,journey='history',freshProfile=True,normalProfileUsed=False,
                    realKodiPlayback=True,kodiOwnedLastplayed=True,historyCardPlayback=True,exactHistoryReturn=True,
                    independentInstances=[dict(count=5,capacity=3),dict(count=1,capacity=1)],layoutUnchanged=True,mediaUnchanged=True,
                    watched=watched,captures=phases),indent=2))
                print('Evidence: '+str(evidence),flush=True);return
            if args.playlists:
                def playlist_capture(phase):
                    print(phase.upper()+"_READY "+str(evidence),flush=True)
                    wait(lambda:(evidence/(phase+"-captured")).exists(),phase+" captured",seconds=110)
                def press(name,count=1):
                    for _ in range(count):key(shell,name,0.05)
                    time.sleep(.3)
                run("wmctrl","-i","-a",shell,env=env)
                press("Up");press("Left",20);press("Right",8);press("Return");time.sleep(1)
                press("Up");press("Left",20);press("Right",6);press("Return")
                playlist_capture("playlistname")
                press("Escape")
                assert not list((userdata/"playlists/video").glob("Isolde-*.xsp")),"Cancel must not create a playlist"
                press("Return")
                remote_text(press,"1")  # Production keyboard layout, including its actual Done action.
                created=wait(lambda:list((userdata/"playlists/video").glob("Isolde-*.xsp")),"Playlist saved from Isolde UI")
                assert len(created)==1
                import xml.etree.ElementTree as ET
                definition=ET.fromstring(created[0].read_bytes())
                assert definition.attrib["type"]=="tvshows" and definition.findtext("name")=="1"
                playlist_capture("playlistsaved")
                press("Escape");press("Right");press("Return");time.sleep(1)
                playlist_capture("playlistcatalog")
                catalog=rpc(directory,"Isolde.GetPlaylists")["playlists"]
                assert any(p["playlistName"]=="Fixture episodes.xsp" for p in catalog)
                created_index=next(i for i,p in enumerate(catalog) if p["playlistName"]==created[0].name)
                press("Down",created_index+2);press("Return");time.sleep(1)
                saved_items=rpc(directory,"Isolde.GetPlaylistItems",{"name":created[0].name,"kind":"shows","start":0,"end":50})["tvshows"]
                assert {i["tvshowid"] for i in saved_items}=={show_id}
                result=rpc(directory,"Isolde.GetPlaylistItems",{"name":"Fixture episodes.xsp","kind":"shows","start":0,"end":50})
                items=result["tvshows"]
                assert len(items)==3 and {i["episodeid"] for i in items}=={i["episodeid"] for i in eps}
                assert all(i["playlistMediaType"]=="episode" and "file" not in i for i in items)
                def manage_playlist(name):
                    current=rpc(directory,'Isolde.GetPlaylists')['playlists']
                    index=next(i for i,row in enumerate(current) if row['playlistName']==name)
                    press('Up',30);press('Down',index+2);press('Return');time.sleep(.5)
                original_playlist=created[0].read_bytes()
                press('Down');press('Return');press('Return');press('Escape')
                assert created[0].read_bytes()==original_playlist,'Rename cancellation changed the definition'
                press('Down');press('Return');press('Return');remote_text(press,'renamed.xsp')
                renamed=playlists/'renamed.xsp'
                wait(lambda:renamed.exists() and not created[0].exists(),'Remote rename committed without losing the original')
                assert ET.fromstring(renamed.read_bytes()).findtext('name')=='renamed'
                manage_playlist(renamed.name);press('Down');press('Return');press('Return');remote_text(press,collision_file.name)
                time.sleep(2);playlist_capture('playlistcollision')
                assert renamed.exists() and collision_file.read_bytes()==collision_bytes,'Collision destroyed a playlist'
                manage_playlist(renamed.name);press('Down',4);press('Return');press('Return');press('Escape')
                rename_bytes=renamed.read_bytes()
                assert b'<rule' not in rename_bytes,'Cancelled rule draft changed the saved definition'
                # Replace rules only after a real read-only Kodi preview and explicit review.
                press('Down',4);press('Return');press('Return');press('Down');press('Return');remote_text(press,'fixture')
                press('Return');time.sleep(2);playlist_capture('playlistrules')
                assert renamed.read_bytes()==rename_bytes,'Preview wrote the playlist'
                press('Escape');press('Return');press('Return')
                wait(lambda:ET.fromstring(renamed.read_bytes()).findtext('rule/value')=='fixture','Reviewed rule replacement committed')
                manage_playlist(renamed.name);press('Down',2);press('Return');press('Down',2);press('Return');press('Down',3);press('Return');press('Down')
                sort_bytes=renamed.read_bytes();playlist_capture('playlistorder')
                assert renamed.read_bytes()==sort_bytes,'Order preview wrote the playlist'
                press('Return')
                wait(lambda:(order:=ET.fromstring(renamed.read_bytes()).find('order')) is not None and order.text=='dateadded' and order.get('direction')=='descending','Smart playlist order saved remotely')
                manage_playlist(member_file.name);press('Down',2);press('Return');time.sleep(1)
                member_before=member_file.read_bytes()
                press('Down',2);press('Return');press('Down',2);playlist_capture('playlistmembers')
                assert member_file.read_bytes()==member_before,'Moving a member wrote before Save'
                press('Escape');press('Escape')
                assert member_file.read_bytes()==member_before,'Member move cancellation changed the saved order'
                press('Down',2);press('Return');time.sleep(1);press('Down',2);press('Return');press('Down',2);press('Return');press('Return')
                expected_members=('#EXTM3U\n'+member_records[1]+member_records[2]+member_records[0]).encode()
                wait(lambda:member_file.read_bytes()==expected_members,'Complete D-pad member order saved with matching metadata')
                manage_playlist(renamed.name);press('Down',3);press('Return');press('Escape')
                assert renamed.exists(),'Delete cancellation removed the definition'
                manage_playlist(renamed.name);press('Down',3);press('Return');playlist_capture('playlistdelete');press('Return')
                wait(lambda:not renamed.exists(),'Confirmed playlist deletion removed only its definition')
                assert collision_file.read_bytes()==collision_bytes and member_file.read_bytes()==expected_members
                # Open the real episode playlist through the existing management UI.
                manage_playlist('Fixture episodes.xsp');press("Return");time.sleep(2)
                playlist_capture("playlistlibrary")
                press("Down");press("Return")
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="playing","Real playlist episode playing")
                playlist_capture("playlistplaying")
                key("kodi","Return")  # Focused on-screen Pause control.
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="paused","Real playlist episode paused")
                playlist_capture("playlistpaused")
                before_item=rpc(directory,'Player.GetItem',{'playerid':1,'properties':[]})['item']
                key('kodi','Menu')
                def controls_window():
                    return next((row[0] for row in windows() if row[2]==str(process.pid) and row[-1]=='Isolde playback controls'),None)
                controls=wait(controls_window,'Native Menu opened the owned Isolde playback controls')
                playlist_capture('playlistcontrols')
                # First playlist episode has Next, but no Previous. Exercise
                # real player presentation and subtitles through the Qt window.
                for _ in range(6):key(controls,'Down',.08)
                key(controls,'Return');key(controls,'Down');key(controls,'Return')
                wait(lambda:rpc(directory,'Isolde.GetPlaybackOptions').get('view')=='zoom','Remote picture-size choice applied by Kodi')
                playlist_capture('playlistpicture')
                key(controls,'Escape')
                for _ in range(20):key(controls,'Up',.04)
                for _ in range(5):key(controls,'Down',.08)
                key(controls,'Return');key(controls,'Return')
                assert rpc(directory,'Isolde.GetPlaybackOptions')['subtitleEnabled'] is False
                key(controls,'Escape')
                for _ in range(20):key(controls,'Up',.04)
                for _ in range(3):key(controls,'Down',.08)
                key(controls,'Return')
                def current_episode():
                    if rpc(directory,'Isolde.PlaybackStatus').get('state')!='playing':return None
                    return rpc(directory,'Player.GetItem',{'playerid':1,'properties':[]}).get('item')
                next_item=wait(lambda:(item if (item:=current_episode()) and item.get('id')!=before_item['id'] else None),'Remote Next opened another actual playlist episode')
                playlist_capture('playlistnext')
                key('kodi','Menu');controls=wait(controls_window,'Playback controls reopened on next episode')
                for _ in range(3):key(controls,'Down',.08)
                key(controls,'Return')
                wait(lambda:(item if (item:=current_episode()) and item.get('id')==before_item['id'] else None),'Remote Previous restored the original actual episode')
                key('kodi','Menu');controls=wait(controls_window,'Playback controls reopened on original episode')
                key(controls,'Escape');time.sleep(.5)
                active_window=run('xprop','-root','_NET_ACTIVE_WINDOW',env=env,timeout=5).strip().split()[-1]
                assert int(active_window,16)==int(next(row[0] for row in windows() if 'Kodi' in row[-1]),16),'Closing controls did not restore native playback focus'
                # Kodi logs rejected input during the OSD closing animation.
                # Let that animation settle before the managed Back -> Stop key.
                time.sleep(3)
                key("kodi","Escape")
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="ended","Real playlist episode stopped")
                run("wmctrl","-i","-a",shell,env=env);time.sleep(1)
                playlist_capture("playlistreturned")
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
                (evidence/"result.json").write_text(json.dumps({"passed":True,"profile":profile,"real_kodi_saved_playlist":True,"playlist_creation_ui_verified":True,"playlist_cancel_no_write":True,"rename_collision_preserved":True,"remote_rule_preview_confirm":True,"smart_sort_saved":True,"member_reorder_remote_cancel_commit":True,"delete_cancel_confirm":True,"episodes":len(items),"play_pause_stop":True,"media_unchanged":True,"normal_profile_used":False,"picker_ui_verified":False,"nativeMenuOwnedIsoldeControls":True,"remotePictureSize":True,"remoteSubtitleOff":True,"realNextPrevious":True,"controlsCloseRestoresVideoFocus":True},indent=2))
                print("Evidence: "+str(evidence),flush=True);return
            art = wait(lambda: (a if (a := rpc(directory, "Isolde.GetArtwork", {"kind": "tvshow", "id": show_id})).get("data") else None), "Kodi local artwork ready")
            artwork = base64.b64decode(art["data"])
            assert artwork and art["mime"] in {"image/png", "image/jpeg"}
            (evidence / ("artwork.png" if art["mime"] == "image/png" else "artwork.jpg")).write_bytes(artwork)
            print("Real Kodi seasons, episodes and local artwork passed", flush=True)
            run("wmctrl", "-i", "-a", shell, env=env)
            if args.pages:
                started=time.monotonic();input_trace=[];capture_trace=[]
                layout=config/"profiles"/profile/"home-layout.json"
                defaults=json.loads((Path(__file__).resolve().parents[1]/"config/default-home-layout.json").read_text())
                def press(name,count=1):
                    for _ in range(count):
                        input_trace.append({"target":"shell","name":name,"elapsedSeconds":time.monotonic()-started});key(shell,name,0.07)
                    time.sleep(0.4)
                def captured(phase):
                    print(phase.upper()+"_READY "+str(evidence),flush=True)
                    wait(lambda:(evidence/(phase+"-captured")).exists(),phase+" captured",seconds=150)
                    capture_trace.append({"phase":phase,"actions":len(input_trace),"elapsedSeconds":time.monotonic()-started})
                def menu(action,extra=0):
                    document=json.loads(layout.read_text()) if layout.exists() else defaults
                    pages=len(document.get("pages",[]))+int(document.get("rootPage",{}).get("visible",True))+extra
                    offsets={"add":0,"appearance":1,"designs":4,"apply":7}
                    index=1 if action=="root" else 1+pages+offsets[action]
                    press("Up",30);press("Down");press("Down",index);press("Return")
                def apply(extra=0):
                    menu("apply",extra);press("Return")
                def enter_editor():
                    run("wmctrl","-i","-a",shell,env=env)
                    x11.XSetInputFocus(display,int(shell,16),2,0);x11.XSync(display,0)
                    x11.XGetInputFocus.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong),ctypes.POINTER(ctypes.c_int)]
                    def focused():
                        target=ctypes.c_ulong();mode=ctypes.c_int()
                        x11.XGetInputFocus(display,ctypes.byref(target),ctypes.byref(mode))
                        return target.value==int(shell,16)
                    wait(focused,"Studio page journey owns shell focus")
                    press("Home");press("Up",30);press("Left",3);press("Right");press("Return")
                def leave_editor():
                    press("Escape",3);press("Home")
                def page_navigation(index,extra=0):
                    menu("appearance",extra);press("Down",2);press("Return")
                    press("Down",2);press("Return");press("Up",30);press("Down",index);press("Return")
                    press("Escape",2)
                enter_editor();press("Return")  # Add page, with an actual remote keyboard name.
                press("Return");press("Down",5);press("Return")
                captured("blank")
                assert not layout.exists(),"Blank page creation must stay private until Apply"
                menu("add",1);press("Down");press("Return")  # Two-column catalog: Media shelf.
                press("Down",6);press("Return");captured("shelfplacement");press("Return")  # Shows · All, accept placement.
                menu("add",1);press("Down",2);press("Right");press("Return");press("Return");press("Return")  # Widgets → Clock.
                menu("add",1);press("Down");press("Right");press("Return");press("Return");press("Return")  # App carousel → all installed apps.
                captured("blankcomponents")
                page_navigation(0,1);apply(1)
                document=wait(lambda:json.loads(layout.read_text()),"Studio blank page and real Shows shelf applied")
                assert len(document["pages"])==1
                page_id=document["pages"][0]["id"]
                page=document["pages"][0]["layout"]
                assert any(i.get("widgetType")=="clock" for i in page["components"])
                assert any(i.get("family")=="app-bar" for i in page["components"])
                assert any(i.get("source",{}).get("libraryKind")=="shows" or i.get("presentation",{}).get("libraryKind")=="shows" for i in page["components"])
                assert page["composition"]["pageNavigation"]=="left"
                menu("root");press("Return")  # Root Home page context → Edit page.
                menu("appearance");press("Down",3);press("Return");press("Down",2);press("Return")
                press("Down");press("Return");press("Escape",2);apply()
                wait(lambda:json.loads(layout.read_text())["composition"].get("moviePage")==page_id,"Named page selected as Movies destination")
                leave_editor();key(shell,"m",modifiers=4);time.sleep(2);captured("page")
                for _ in range(5):
                    if rpc(directory,"Isolde.PlaybackStatus").get("state")=="playing":break
                    press("Return");time.sleep(.7)
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="playing","Page widget real Kodi episode playback passed")
                captured("playing")
                input_trace.append({"target":"kodi","name":"Return","elapsedSeconds":time.monotonic()-started});key("kodi","Return")
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="paused","Page widget pause passed")
                input_trace.append({"target":"kodi","name":"Escape","elapsedSeconds":time.monotonic()-started});key("kodi","Escape")
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="ended","Page widget stop and return passed")
                time.sleep(1.2)
                captured("playbackreturn")
                for mode,index in [("top",1),("none",2)]:
                    enter_editor();press("Up",30);press("Down");press("Down",2);press("Return");press("Return")
                    page_navigation(index);apply()
                    wait(lambda:json.loads(layout.read_text())["pages"][0]["layout"]["composition"]["pageNavigation"]==mode,"Studio page navigation "+mode+" applied")
                    leave_editor();key(shell,"m",modifiers=4);time.sleep(1);captured("top" if mode=="top" else "standalone")
                captured("direct")
                enter_editor();press("Up",30);press("Down");press("Down",7);press("Return");press("Down",4);press("Return")
                press("Right");press("Return");press("Down",20);press("Return")
                saved=list((config/"profiles"/profile/"skins").glob("user-*.json"))
                assert len(saved)==1 and json.loads(saved[0].read_text())["layout"]["pages"][0]["id"]==page_id
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
                (evidence/"attempt-config.json").write_bytes(layout.read_bytes())
                (evidence/"result.json").write_text(json.dumps({"passed":True,"profile":profile,"page_id":page_id,"settings_only_creation_and_customization":True,"home_tab":False,"home_tab_note":"Native route/tab contracts supplement this installed direct-destination journey; no Home tab is injected or claimed.","direct_launcher_destination":True,"navigation_modes":["left","top","none"],"real_kodi_episode_play_pause_stop_return":True,"skin_includes_pages":True,"media_unchanged":True,"normal_profile_used":False,"visual_review_required":True,"executable":str(args.executable.resolve()),"inputTrace":input_trace,"captureTrace":capture_trace,"actualInputActions":len(input_trace),"journeyElapsedSeconds":time.monotonic()-started},indent=2))
                print("Evidence: "+str(evidence),flush=True)
                return
            # Refresh initially empty Shows using its toolbar's Refresh button.
            key(shell, "Up")
            for _ in range(5): key(shell, "Right")
            key(shell, "Return"); time.sleep(1)
            key(shell, "Down"); key(shell, "Return"); time.sleep(1)  # Show â†’ seasons.
            key(shell, "Return"); time.sleep(1)  # Specials â†’ episode.
            key(shell, "Return")
            wait(lambda: rpc(directory, "Isolde.PlaybackStatus").get("state") == "playing", "Remote episode playback passed")
            key("kodi", "Return")
            wait(lambda: rpc(directory, "Isolde.PlaybackStatus").get("state") == "paused", "Remote episode pause passed")
            key("kodi", "Escape")
            wait(lambda: rpc(directory, "Isolde.PlaybackStatus").get("state") == "ended", "Remote episode stop and return passed")
            key(shell, "Escape"); key(shell, "Escape")  # Episode â†’ seasons â†’ shows.
            for p, digest in hashes.items(): assert hashlib.sha256(Path(p).read_bytes()).hexdigest() == digest
            (evidence / "result.json").write_text(json.dumps({"passed": True, "profile": profile, "shows": len(shows), "seasons": len(seasons), "episodes": len(eps), "media_unchanged": True}, indent=2))
            print("Evidence: " + str(evidence), flush=True)
    except Exception:
        if args.pages and args.inspect and process and process.poll() is None:
            print("FAILED_READY "+str(evidence),flush=True)
            deadline=time.monotonic()+30
            while not (evidence/"failed-captured").exists() and time.monotonic()<deadline:
                time.sleep(.2)
        if args.movie_art:
            try:
                (evidence/"movie-diagnostic.json").write_text(json.dumps(rpc(userdata/"isolde","VideoLibrary.GetMovies",
                    {"properties":["title","year","file","art","uniqueid"],"limits":{"start":0,"end":50}}),indent=2))
            except (OSError,RuntimeError): pass
        kodi_log = userdata.parent / "temp/kodi.log"
        if kodi_log.exists():
            log_text=kodi_log.read_text(errors="replace")
            print(log_text[-6000:], flush=True)
            if args.movie_art:
                print('KODI_PLAYBACK_DIAGNOSTICS\n'+'\n'.join(line for line in log_text.splitlines()[-4000:] if any(token in line for token in ('OnPlayBack','OnAV','OnMessage','CurrentFile','TMSG_','CApplication::Play','VideoLibrary.GetMovie','Player.GetItem'))) [-24000:],flush=True)
        if args.inspect: print("Inspect: " + str(evidence), flush=True); time.sleep(20)
        raise
    finally:
        if process and process.poll() is None:
            process.terminate()
            try: process.wait(timeout=12)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
        if soak_ready:
            from guide_lifecycle import prepare_soak
            deadline=time.monotonic()+12
            while time.monotonic()<deadline:
                try:rpc(userdata/'isolde','JSONRPC.Version')
                except (OSError,RuntimeError):break
                time.sleep(0.2)
            else:raise RuntimeError('Private Kodi bridge remained active; twelve-hour-off fixture was not prepared')
            prepare_soak(evidence,database,profile,media,args.executable.resolve())
        x11.XCloseDisplay(display)
        if active: run("systemctl", "--user", "start", "isolde.service")


if __name__ == "__main__": main()
