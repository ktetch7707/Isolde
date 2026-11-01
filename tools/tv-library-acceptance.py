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
import struct
import subprocess
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
    print("Fixture evidence: " + str(evidence) + " · profile " + profile, flush=True)
    media = Path.home() / "Videos/Isolde-Acceptance" / ("Isolde-tv-test-" + profile)
    data = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share")))
    userdata = data / "Isolde/native-profiles" / profile / "kodi/home/.kodi/userdata"
    if media.exists() or userdata.exists(): raise RuntimeError("Fixture must be new")
    media.mkdir(parents=True); userdata.mkdir(parents=True, mode=0o700)
    if args.playlists:
        playlists=userdata/"playlists/video";playlists.mkdir(parents=True)
        (playlists/"Fixture episodes.xsp").write_text('<smartplaylist type="episodes"><name>Fixture episodes</name><match>all</match><rule field="tvshow" operator="is"><value>Isolde Television Fixture</value></rule><order direction="ascending">episode</order></smartplaylist>')
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
    if args.guide:
        # Native Kodi XML import, not database writes. Supply both the declared
        # local movie and Kodi's export-art companion path (Omega ImportFromXML).
        movie=media/'Fixture Movie.ogv';os.link(clip,movie)
        guide_movies=imported/'movies';guide_movies.mkdir();os.link(clip,guide_movies/'Fixture_Movie_2026.avi')
        document=(imported/'videodb.xml').read_text().replace('<tvshow><title>','<tvshow><studio>Cartoon Network</studio><title>',1)
        document=document.replace('<paths/>',f'<movie><title>Fixture Movie</title><year>2026</year><studio>Cartoon Network</studio><runtime>2</runtime><filenameandpath>{escape(str(movie))}</filenameandpath><path>{escape(str(media))}/</path><file>{movie.name}</file><basepath>{escape(str(media))}/</basepath><art><poster>{escape(str(poster))}</poster></art></movie><paths/>')
        (imported/'videodb.xml').write_text(document)
    if args.guide or args.history:
        # Kodi's native import resolves exported episode entries to these
        # export-relative filenames. Supply actual fixture media there rather
        # than relying on an ignored filenameandpath override in the XML.
        for season, number in ((0,1),(1,1),(1,2)):
            os.link(clip,exported_art/f"s{season:02}e{number:02}.avi")
        if args.history:
            assert all((exported_art/f"s{season:02}e{number:02}.avi").is_file() for season,number in ((0,1),(1,1),(1,2)))
            print('History export-relative media fixture validated',flush=True)
    (exported_art / "poster.png").write_bytes(poster.read_bytes())
    (exported_art.parent / "Isolde_Television_Fixture-poster.png").write_bytes(poster.read_bytes())
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
    if args.movie_art:
        # Import ONLY source configuration through Kodi's supported export/import
        # format. All movie identity and artwork must come from the online scraper.
        movies = media / "movies"
        folder = movies / "Big Buck Bunny (2008)"; folder.mkdir(parents=True)
        movie_file = folder / "Big Buck Bunny (2008).ogv"
        os.link(clip, movie_file)
        (imported / "videodb.xml").write_text('<videodb><version>1</version><paths><path><url>'
            + escape(str(movies)) + '/</url><scanrecursive>1</scanrecursive><usefoldernames>true</usefoldernames>'
            '<content>movies</content><scraperpath>metadata.themoviedb.org.python</scraperpath></path></paths></videodb>')
        importer = (userdata / "tv-fixture-import.py").read_text()
        importer = importer[:importer.index('    response = json.loads')]
        # Kodi can update its bundled provider after first launch. Initialize
        # the updated provider BEFORE importing source settings, otherwise the
        # source keeps the old provider's incomplete artwork configuration.
        provider_setup = '''import xbmc, xbmcaddon, time
deadline=time.monotonic()+45
while True:
    provider=xbmcaddon.Addon("metadata.themoviedb.org.python")
    if provider.getSetting("fetch_posters") in ("true","false"): break
    if time.monotonic()>deadline: raise RuntimeError("TMDB provider artwork settings unavailable")
    xbmc.sleep(500)
for setting in ("fetch_posters","fanart","landscape","keyart"):
    if not provider.setSettingBool(setting,True): raise RuntimeError("Cannot configure TMDB "+setting)
provider.setSettingString("language","en-US")
provider.setSettingString("searchlanguage","en-US")
'''
        importer = provider_setup + importer
        importer += '\nxbmc.executeJSONRPC(' + repr(json.dumps({"jsonrpc":"2.0","id":1,"method":"VideoLibrary.Scan","params":{"directory":str(movies)+"/"}})) + ')\n'
        (userdata / "tv-fixture-import.py").write_text(importer)
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
            if args.guide:guide_capture('guidestartup')
            else:
                key("kodi", "Escape"); key("kodi", "Escape")  # Dismiss first-run dialogs in this new test home.
                key("kodi", "F9")
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
                movies = wait(lambda: rpc(directory,"VideoLibrary.GetMovies",query).get("movies"),
                              "Kodi TMDB matched named synthetic movie", seconds=150)
                assert len(movies)==1 and movies[0]["title"]=="Big Buck Bunny" and movies[0]["year"]==2008
                movie=movies[0]
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
                # Return to the shell's real Movies view, without playing the clip.
                run("wmctrl","-i","-a",shell,env=env)
                key(shell,"Home"); key(shell,"m",modifiers=4); time.sleep(1)
                key(shell,"Return"); time.sleep(2); key(shell,"Down"); time.sleep(2)
                print("MOVIEART_READY "+str(evidence),flush=True)
                wait(lambda:(evidence/"movieart-captured").exists(),"Movie artwork frame captured",seconds=110)
                for path,digest in hashes.items(): assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest, path
                (evidence/"result.json").write_text(json.dumps({"passed":True,"profile":profile,"freshProfile":True,
                    "normalProfileUsed":False,"scrapedByKodi":True,"metadataSupplied":False,"playbackVerified":False,
                    "movie":movie,"artworkBytes":len(raw),"artworkSha256":hashlib.sha256(raw).hexdigest(),
                    "artworkMime":art["mime"],"media_unchanged":True},indent=2))
                return
            shows = wait(lambda: rpc(directory, "VideoLibrary.GetTVShows", {"properties": ["title"]}).get("tvshows"), "Kodi imported TV fixture")
            show_id = next(s["tvshowid"] for s in shows if s["title"] == "Isolde Television Fixture")
            seasons = rpc(directory, "VideoLibrary.GetSeasons", {"tvshowid": show_id, "properties": ["season"]})["seasons"]
            assert {s["season"] for s in seasons} == {0, 1}
            eps = rpc(directory, "VideoLibrary.GetEpisodes", {"tvshowid": show_id, "properties": ["season", "episode", "resume"]})["episodes"]
            assert len(eps) == 3
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
                press("Return");press("Return") # Type '1' using the actual remote keyboard.
                press("Right",80);press("Return") # Bounded movement to Done.
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
                playlist_capture("playlistlibrary")
                saved_items=rpc(directory,"Isolde.GetPlaylistItems",{"name":created[0].name,"kind":"shows","start":0,"end":50})["tvshows"]
                assert {i["tvshowid"] for i in saved_items}=={show_id}
                result=rpc(directory,"Isolde.GetPlaylistItems",{"name":"Fixture episodes.xsp","kind":"shows","start":0,"end":50})
                items=result["tvshows"]
                assert len(items)==3 and {i["episodeid"] for i in items}=={i["episodeid"] for i in eps}
                assert all(i["playlistMediaType"]=="episode" and "file" not in i for i in items)
                rpc(directory,"Isolde.PlayEpisode",{"episodeid":items[0]["episodeid"],"resume":False})
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="playing","Real playlist episode playing")
                playlist_capture("playlistplaying")
                rpc(directory,"Isolde.PlaybackControl",{"action":"pause"})
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="paused","Real playlist episode paused")
                playlist_capture("playlistpaused")
                rpc(directory,"Isolde.PlaybackControl",{"action":"stop"})
                wait(lambda:rpc(directory,"Isolde.PlaybackStatus").get("state")=="ended","Real playlist episode stopped")
                run("wmctrl","-i","-a",shell,env=env);time.sleep(1)
                playlist_capture("playlistreturned")
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
                (evidence/"result.json").write_text(json.dumps({"passed":True,"profile":profile,"real_kodi_saved_playlist":True,"playlist_creation_ui_verified":True,"playlist_cancel_no_write":True,"episodes":len(items),"play_pause_stop":True,"media_unchanged":True,"normal_profile_used":False,"picker_ui_verified":False},indent=2))
                print("Evidence: "+str(evidence),flush=True);return
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
        if args.movie_art:
            try:
                (evidence/"movie-diagnostic.json").write_text(json.dumps(rpc(userdata/"isolde","VideoLibrary.GetMovies",
                    {"properties":["title","year","file","art","uniqueid"],"limits":{"start":0,"end":50}}),indent=2))
            except (OSError,RuntimeError): pass
        kodi_log = userdata.parent / "temp/kodi.log"
        if kodi_log.exists(): print(kodi_log.read_text(errors="replace")[-6000:], flush=True)
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
