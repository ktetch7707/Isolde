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
    parser.add_argument("--guide", action="store_true", help="Configure an isolated real IPTV Simple channel, then import and tune it through the Guide")
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
        # Kodi's native import resolves exported episode entries to these
        # export-relative filenames. Supply actual fixture media there rather
        # than relying on an ignored filenameandpath override in the XML.
        for season, number in ((0,1),(1,1),(1,2)):
            os.link(clip,exported_art/f"s{season:02}e{number:02}.avi")
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
    if args.guide:
        guide_profile=config/'profiles'/profile;guide_profile.mkdir(parents=True)
        (guide_profile/'home-layout.json').write_text(json.dumps({'schemaVersion':1,'categories':[{'id':'apps','name':'Apps'}],'items':[{'kind':'action','id':'guide-entry','name':'TV Guide','symbol':'TV','size':'standard','categoryId':'apps','action':{'schemaVersion':1,'type':'open-guide'}}]}))
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
                source_channels=wait(lambda:rpc(directory,'PVR.GetChannels',{'channelgroupid':'alltv','properties':['channelnumber','locked','hidden','clientid'],'limits':{'start':0,'end':50}}).get('channels'),'Real IPTV Simple channel ready',seconds=60)
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
                run('wmctrl','-i','-a',shell,env=env);guide_capture('guidereturn')
                assert imported_channels()==[cached]
                for path,digest in hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,path
                (evidence/'result.json').write_text(json.dumps({'passed':True,'freshProfile':True,'normalProfileUsed':False,'pvrAddon':'pvr.iptvsimple','sourceChannels':len(source_channels),'sourceListings':len(source_epg),'remoteImport':True,'actualPvrPlayback':True,'returned':True,'mediaUnchanged':True,'syntheticSource':True,'providerSubscriptionVerified':False,'captures':['guidestartup','guideorigin','guidehome','guideempty','guidesource','guidepvr','guidevideo','guidereturn'],'visualReview':'pending','profile':profile},indent=2))
                return
            shows = wait(lambda: rpc(directory, "VideoLibrary.GetTVShows", {"properties": ["title"]}).get("tvshows"), "Kodi imported TV fixture")
            show_id = next(s["tvshowid"] for s in shows if s["title"] == "Isolde Television Fixture")
            seasons = rpc(directory, "VideoLibrary.GetSeasons", {"tvshowid": show_id, "properties": ["season"]})["seasons"]
            assert {s["season"] for s in seasons} == {0, 1}
            eps = rpc(directory, "VideoLibrary.GetEpisodes", {"tvshowid": show_id, "properties": ["season", "episode", "resume"]})["episodes"]
            assert len(eps) == 3
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
