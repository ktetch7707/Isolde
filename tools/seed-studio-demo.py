#!/usr/bin/env python3
"""One-shot, VM-only Studio Demo library. Never opens or edits a Kodi database.

Creates a fresh Isolde profile and local synthetic media, then uses Kodi's
native import UI and internal JSON-RPC to seed that profile's viewing state.
The installed executable/release is unchanged. Existing profiles are untouched.
"""
import argparse
import base64
import ctypes
import ctypes.util
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time
import uuid
from xml.sax.saxutils import escape


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, timeout=30, **kwargs).stdout


def atomic(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".seed-tmp")
    with temporary.open("xb") as stream:
        stream.write(content); stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary, path)
    descriptor = os.open(path.parent, os.O_DIRECTORY)
    try: os.fsync(descriptor)
    finally: os.close(descriptor)


def rpc(directory, method, params=None):
    descriptor = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(5); client.connect(f"/proc/self/fd/{descriptor}/rpc.sock")
            client.sendall((json.dumps({"jsonrpc":"2.0", "id":1, "method":method, "params":params or {}})+"\n").encode())
            response = b""
            while not response.endswith(b"\n"):
                part = client.recv(65536)
                if not part or len(response)+len(part)>4194304: raise RuntimeError("Invalid bridge response")
                response += part
            reply = json.loads(response)
            if "error" in reply: raise RuntimeError(str(reply["error"]))
            return reply["result"]
    finally: os.close(descriptor)


def key(window, name, env):
    # The same targeted X11 event mechanism as tv-library-acceptance.py.
    # libX11 reads authentication from this process, not subprocess.run's env.
    os.environ.update({k:env[k] for k in ("DISPLAY","XAUTHORITY") if k in env})
    x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
    x11.XOpenDisplay.argtypes=[ctypes.c_char_p]; x11.XOpenDisplay.restype=ctypes.c_void_p
    x11.XStringToKeysym.argtypes=[ctypes.c_char_p]; x11.XStringToKeysym.restype=ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong]; x11.XKeysymToKeycode.restype=ctypes.c_uint
    class KeyEvent(ctypes.Structure):
        _fields_=[("type",ctypes.c_int),("serial",ctypes.c_ulong),("send_event",ctypes.c_int),("display",ctypes.c_void_p),
                  ("window",ctypes.c_ulong),("root",ctypes.c_ulong),("subwindow",ctypes.c_ulong),("time",ctypes.c_ulong),
                  ("x",ctypes.c_int),("y",ctypes.c_int),("x_root",ctypes.c_int),("y_root",ctypes.c_int),
                  ("state",ctypes.c_uint),("keycode",ctypes.c_uint),("same_screen",ctypes.c_int)]
    class Event(ctypes.Union): _fields_=[("key",KeyEvent),("padding",ctypes.c_long*24)]
    x11.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
    x11.XFlush.argtypes=[ctypes.c_void_p]; x11.XCloseDisplay.argtypes=[ctypes.c_void_p]
    x11.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
    errors=[]
    callback_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p)
    callback=callback_type(lambda _display,_error: errors.append(True) or 0)
    x11.XSetErrorHandler.argtypes=[callback_type]; x11.XSetErrorHandler(callback)
    display=x11.XOpenDisplay(env["DISPLAY"].encode())
    if not display: raise RuntimeError("No graphical VM session")
    try:
        code=x11.XKeysymToKeycode(display,x11.XStringToKeysym(name.encode()))
        for kind, mask in ((2,1),(3,2)):
            event=Event(); event.key=KeyEvent(type=kind,send_event=1,display=display,window=int(window,16),keycode=code,same_screen=1)
            x11.XSendEvent(display,int(window,16),1,mask,ctypes.byref(event))
        x11.XSync(display,0)
        if errors: raise RuntimeError("Kodi window changed during targeted input")
    finally: x11.XCloseDisplay(display)


def verify_demo(home, profile, expected_release=None):
    profile=str(uuid.UUID(profile))
    media=home/"Videos/Isolde-Studio-Demo"/profile
    manifest=json.loads((media/"manifest.json").read_text())
    if manifest["profile"]!=profile or manifest["status"]!="ready": raise RuntimeError("Not a completed Studio Demo")
    directory=home/".local/share/Isolde/native-profiles"/profile/"kodi/home/.kodi/userdata/isolde"
    query={"properties":["title","resume","playcount","art"],"limits":{"start":0,"end":50},"sort":{"method":"lastplayed","order":"descending"},"filter":{"field":"inprogress","operator":"true","value":""}}
    continuing=rpc(directory,"VideoLibrary.GetMovies",query).get("movies",[])
    if len(continuing)!=4: raise RuntimeError("Actual Continue Watching query must return four movies")
    query.pop("filter"); query["sort"]={"method":"dateadded","order":"descending"}
    recent=rpc(directory,"VideoLibrary.GetMovies",query).get("movies",[])
    if len(recent)!=8: raise RuntimeError("Actual Recently Added query must return eight movies")
    query["filter"]={"field":"lastplayed","operator":"after","value":"1970-01-01"}
    query["sort"]={"method":"lastplayed","order":"descending"}
    watched=rpc(directory,"VideoLibrary.GetMovies",query).get("movies",[])
    expected_watched={m["title"] for m in json.loads((media/"expected.json").read_text()) if m["state"]!="unwatched"}
    if {m["label"] for m in watched}!=expected_watched or len(watched)!=6:
        raise RuntimeError("Recently watched must include all six viewed movies and exclude never-watched movies")
    art_checks=[]
    for movie in recent:
        art=rpc(directory,"Isolde.GetArtwork",{"kind":"movie","id":movie["movieid"]})
        data=base64.b64decode(art.get("data",""),validate=True)
        if not data or art.get("mime") not in {"image/png","image/jpeg"}: raise RuntimeError("Missing local artwork")
        art_checks.append({"title":movie["label"],"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
    episode_checks=[]
    for show in manifest["shows"]:
        episodes=rpc(directory,"VideoLibrary.GetEpisodes",{"tvshowid":show["tvshowid"],"properties":["episode","resume","playcount"],"limits":{"start":0,"end":50}}).get("episodes",[])
        if len(episodes)!=3 or sum(e.get("playcount",0)>0 for e in episodes)!=1 or sum(e.get("resume",{}).get("position",0)>0 for e in episodes)!=1:
            raise RuntimeError("TV demo must have one watched and one resumable episode per show")
        episode_checks.append({"show":show["label"],"episodes":episodes})
    for path,digest in manifest["preserved"].items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest()!=digest: raise RuntimeError("Household layout differs")
    installed=str((home/".local/share/isolde-appliance/current").resolve())
    expected=manifest["release"]
    if expected_release:
        if len(expected_release)!=24 or any(c not in "0123456789abcdef" for c in expected_release):
            raise RuntimeError("Expected release must be the exact 24-character deployed release ID")
        expected=str(home/".local/share/isolde-appliance/releases"/expected_release)
    if installed!=expected: raise RuntimeError("Release differs from the explicit verification baseline")
    report={"ok":True,"profile":profile,"continueWatching":[m["label"] for m in continuing],"recentlyWatched":[m["label"] for m in watched],"recentlyAdded":[m["label"] for m in recent],"artwork":art_checks,"shows":episode_checks,"householdLayoutUnchanged":True,"installedRelease":installed,"seedRelease":manifest["release"],"expectedReleaseMatched":True,"installedReleaseUnchanged":installed==manifest["release"]}
    atomic(media/"verification.json",json.dumps(report,indent=2).encode())
    print(json.dumps(report,indent=2))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute-on-isolde-test-vm",action="store_true",required=True)
    parser.add_argument("--restore-prepared-profile",type=str)
    parser.add_argument("--resume-prepared-profile",type=str)
    parser.add_argument("--verify-profile",type=str,help="Read Kodi and write fixture verification evidence; no profile switch/history changes")
    parser.add_argument("--expected-release",type=str,help="Exact deployed release ID for verification after an intentional update; seed manifest stays unchanged")
    args=parser.parse_args()
    if args.expected_release and not args.verify_profile:
        parser.error("--expected-release requires --verify-profile")
    home=Path.home()
    if sys.platform!="linux" or home!=Path("/home/isolde-dev") or not (home/"isolde-oracle").is_dir():
        raise RuntimeError("Only the existing Isolde-Test guest is supported")
    locks=[]
    for path in (home/"isolde-oracle/oracle.lock",Path("/tmp/isolde-home-components-acceptance.lock")):
        handle=path.open("a"); fcntl.flock(handle,fcntl.LOCK_EX|fcntl.LOCK_NB); locks.append(handle)
    if args.verify_profile:
        verify_demo(home,args.verify_profile,args.expected_release)
        return
    config=home/".config/Isolde"; profile_file=config/"profiles.json"
    original_profiles=profile_file.read_bytes(); profiles=json.loads(original_profiles)
    if args.restore_prepared_profile:
        restore_id=str(uuid.UUID(args.restore_prepared_profile))
        prepared=home/"Videos/Isolde-Studio-Demo"/restore_id
        if prepared.is_symlink() or not prepared.is_dir() or profiles["active"]!=restore_id:
            raise RuntimeError("Prepared profile is not the active profile")
        backup=prepared/"profiles-before.json"
        json.loads(backup.read_bytes())
        run("systemctl","--user","stop","isolde.service")
        try:
            atomic(profile_file,backup.read_bytes())
            atomic(config/"MediaAccess.conf",(prepared/"media-access-before.conf").read_bytes())
        finally: run("systemctl","--user","start","isolde.service")
        print("Restored original profile and grants; demo artifacts retained")
        return
    if any(p["name"].casefold()=="studio demo" for p in profiles["profiles"]):
        raise RuntimeError("Studio Demo already exists; refusing to duplicate/reset it")
    if run("systemctl","--user","is-active","isolde.service").strip()!="active": raise RuntimeError("Normal service must be active")
    original_release=(home/".local/share/isolde-appliance/current").resolve()
    profile=str(uuid.UUID(args.resume_prepared_profile)) if args.resume_prepared_profile else str(uuid.uuid4())
    media=home/"Videos/Isolde-Studio-Demo"/profile
    userdata=home/".local/share/Isolde/native-profiles"/profile/"kodi/home/.kodi/userdata"
    grants=config/"MediaAccess.conf"; original_grants=grants.read_bytes() if grants.exists() else b""
    if not args.resume_prepared_profile:
        if media.exists() or userdata.exists(): raise RuntimeError("New profile paths required")
        media.mkdir(parents=True); userdata.mkdir(parents=True,mode=0o700)
        atomic(media/"profiles-before.json",original_profiles)
        grants=config/"MediaAccess.conf"; original_grants=grants.read_bytes() if grants.exists() else b""
        atomic(media/"media-access-before.conf",original_grants)
        before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (config/"home-layout.json",) if p.exists()}
        manifest={"profile":profile,"name":"Studio Demo","media":str(media),"previousActive":profiles["active"],"release":str(original_release),"preserved":before,"status":"preparing"}
        atomic(media/"manifest.json",json.dumps(manifest,indent=2).encode())
        print("DEMO_MEDIA="+str(media),flush=True)
        # Tiny, genuinely playable local clips; hard links keep disk use bounded.
        clip=media/"demo.ogv"
        run("gst-launch-1.0","-q","videotestsrc","pattern=ball","num-buffers=1800","!",
            "video/x-raw,width=320,height=180,framerate=15/1","!","theoraenc","!","oggmux","!","filesink","location="+str(clip))
        from PIL import Image, ImageDraw, ImageFont
        font_path="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
        def artwork(folder,title,index):
            image=Image.new("RGB",(640,360)); draw=ImageDraw.Draw(image)
            palettes=[((249,194,207),(111,103,163)),((176,216,239),(84,112,154)),((205,224,195),(73,119,112)),((242,217,170),(169,105,130))]
            a,b=palettes[index%len(palettes)]
            for y in range(360):
                t=y/359; draw.line((0,y,640,y),fill=tuple(round(a[k]*(1-t)+b[k]*t) for k in range(3)))
            draw.ellipse((440,25,530,115),fill=(253,239,218))
            draw.polygon([(0,270),(130,130),(260,260),(390,150),(640,310),(640,360),(0,360)],fill=b)
            draw.rectangle((0,260,640,360),fill=(32,37,59))
            draw.text((25,278),title,font=ImageFont.truetype(font_path,29),fill=(250,242,246))
            draw.text((26,326),"ISOLDE STUDIO · DEMO CLIP",font=ImageFont.truetype(font_path,15),fill=(211,199,220))
            image.save(folder/"landscape.png"); image.resize((480,720)).save(folder/"poster.png")
            return folder/"landscape.png"
        titles=["Moonlit Harbor","The Glass Garden","Beyond the Clouds","Postcards from June","A Quiet Orbit","The Last Lighthouse","Wildflower Road","Sunday in the City"]
        movies=[]; expected=[]
        for index,title in enumerate(titles):
            folder=media/title; folder.mkdir(); file=folder/(title+" (2026).ogv"); os.link(clip,file)
            art=artwork(folder,title,index)
            movies.append(f"<movie><title>{escape(title)}</title><year>2026</year><runtime>2</runtime><plot>Playable synthetic Studio demo; not the feature film.</plot><genre>Studio Demo</genre><tag>Studio Demo</tag><filenameandpath>{escape(str(file))}</filenameandpath><path>{escape(str(folder))}/</path><basepath>{escape(str(folder))}/</basepath><art><poster>{escape(str(folder/'poster.png'))}</poster><thumb>{escape(str(art))}</thumb><fanart>{escape(str(art))}</fanart></art></movie>")
            expected.append({"title":title,"file":str(file),"art":str(art),"poster":str(folder/"poster.png"),"state":"continue" if index<4 else "watched" if index<6 else "unwatched"})
        shows=[]
        for index,title in enumerate(("Skyline Stories","The Weekend Diaries")):
            folder=media/title; folder.mkdir(); art=artwork(folder,title,index+2); episodes=[]
            for number in (1,2,3):
                file=folder/f"{title} S01E{number:02}.ogv"; os.link(clip,file)
                episodes.append(f"<episodedetails><title>{escape(title)} · Chapter {number}</title><showtitle>{escape(title)}</showtitle><season>1</season><episode>{number}</episode><runtime>2</runtime><filenameandpath>{escape(str(file))}</filenameandpath><path>{escape(str(folder))}/</path><file>{escape(file.name)}</file><art><thumb>{escape(str(art))}</thumb></art></episodedetails>")
            shows.append(f"<tvshow><title>{escape(title)}</title><plot>Synthetic Studio television demo.</plot><path>{escape(str(folder))}/</path><basepath>{escape(str(folder))}/</basepath><art><poster>{escape(str(folder/'poster.png'))}</poster><fanart>{escape(str(art))}</fanart></art>{''.join(episodes)}</tvshow>")
        imported=media/"import"; imported.mkdir()
        (imported/"videodb.xml").write_text("<videodb><version>1</version>"+"".join(movies+shows)+"<paths/></videodb>")
        (media/"expected.json").write_text(json.dumps(expected))
        (userdata/"keymaps").mkdir()
        (userdata/"studio-demo-scope.json").write_text(json.dumps({"profile":profile,"media":str(media)}))
        (userdata/"keymaps/studio-demo.xml").write_text('<keymap><global><keyboard><f9>RunScript(special://profile/studio-demo-import.py)</f9></keyboard></global></keymap>')
        (userdata/"mediasources.xml").write_text('<mediasources><network><location id="0">'+escape(str(imported))+'/</location></network></mediasources>')
        # Executed by Kodi itself; native UI imports, scoped API writes fixture IDs only.
        seed_script=Path(__file__).with_name("studio-demo-kodi-import.py").read_text()
        (userdata/"studio-demo-import.py").write_text("MEDIA_ROOT="+repr(str(media))+"\n"+seed_script)
    else:
        if media.is_symlink() or userdata.is_symlink(): raise RuntimeError("Prepared paths must not be links")
        if (media/"profiles-before.json").read_bytes()!=original_profiles or (media/"media-access-before.conf").read_bytes()!=original_grants:
            raise RuntimeError("Prepared profile baseline differs; do not overwrite newer settings")
        manifest=json.loads((media/"manifest.json").read_text()); before=manifest["preserved"]
        if manifest["status"]!="preparing" or manifest["profile"]!=profile: raise RuntimeError("Only a failed preparation can resume")
        seed_script=Path(__file__).with_name("studio-demo-kodi-import.py").read_text()
        (userdata/"studio-demo-import.py").write_text("MEDIA_ROOT="+repr(str(media))+"\n"+seed_script)
        print("RESUME_DEMO_MEDIA="+str(media),flush=True)
    env=dict(os.environ)
    for line in run("systemctl","--user","show-environment").splitlines():
        name,_,value=line.partition("=")
        if name in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS"}: env[name]=value
    changed=False
    try:
        # Refuse a concurrent profile/config edit before activation.
        if profile_file.read_bytes()!=original_profiles or (grants.read_bytes() if grants.exists() else b"")!=original_grants:
            raise RuntimeError("Profile configuration changed during preparation")
        run("systemctl","--user","stop","isolde.service"); changed=True
        text=original_grants.decode()
        entry=profile+"\\kodi\\folders="+str(media)+"\n"
        if "[profiles]" not in text: text+="\n[profiles]\n"+entry
        else: text=text.replace("[profiles]","[profiles]\n"+entry,1)
        atomic(grants,text.encode())
        profiles["profiles"].append({"id":profile,"name":"Studio Demo"}); profiles["active"]=profile
        atomic(profile_file,json.dumps(profiles,indent=2).encode())
        run("systemctl","--user","start","isolde.service")
        deadline=time.monotonic()+45
        while True:
            try:
                if rpc(userdata/"isolde","JSONRPC.Ping")=="pong": break
            except (FileNotFoundError,ConnectionRefusedError,socket.timeout): pass
            if time.monotonic()>deadline: raise RuntimeError("Demo Kodi bridge startup timed out")
            time.sleep(.5)
        if rpc(userdata/"isolde","VideoLibrary.GetMovies").get("movies"): raise RuntimeError("Expected empty new Kodi library")
        # The bridge can start before Kodi recreates its fullscreen window.
        time.sleep(4)
        windows=run("wmctrl","-lx",env=env).splitlines()
        window=next(line.split()[0] for line in windows if "kodi" in line.lower())
        run("wmctrl","-i","-a",window,env=env); time.sleep(.3)
        windows=run("wmctrl","-lx",env=env).splitlines()
        window=next(line.split()[0] for line in windows if "kodi" in line.lower())
        key(window,"F9",env)
        deadline=time.monotonic()+75
        while not (userdata/"studio-demo-import-result.json").exists():
            if time.monotonic()>deadline: raise RuntimeError("Native demo import timed out; inspect new profile's kodi.log")
            time.sleep(.5)
        result=json.loads((userdata/"studio-demo-import-result.json").read_text())
        if not result.get("ok"): raise RuntimeError(str(result))
        actual=rpc(userdata/"isolde","VideoLibrary.GetMovies",{"properties":["title","resume","playcount","art"],"limits":{"start":0,"end":50}}).get("movies",[])
        if len(actual)!=8 or sum(m.get("resume",{}).get("position",0)>0 for m in actual)!=4 or sum(m.get("playcount",0)>0 for m in actual)!=2:
            raise RuntimeError("Bridge verification did not match 8 movies / 4 resumable / 2 watched")
        shows_result=rpc(userdata/"isolde","VideoLibrary.GetTVShows",{"properties":["title","episode","watchedepisodes"],"limits":{"start":0,"end":50}}).get("tvshows",[])
        if len(shows_result)!=2 or sum(s.get("episode",0) for s in shows_result)!=6: raise RuntimeError("TV demo counts differ")
        for path,digest in before.items():
            if hashlib.sha256(Path(path).read_bytes()).hexdigest()!=digest: raise RuntimeError("Household layout changed")
        if (home/".local/share/isolde-appliance/current").resolve()!=original_release: raise RuntimeError("Installed release changed")
        manifest.update(status="ready",movies=actual,shows=shows_result)
        atomic(media/"manifest.json",json.dumps(manifest,indent=2).encode())
        print("READY: Studio Demo; 8 movies, 2 shows / 6 episodes; 4 movie resume points, 2 watched movies",flush=True)
        print("PROFILE="+profile,flush=True)
    except BaseException:
        if changed:
            run("systemctl","--user","stop","isolde.service")
            atomic(profile_file,original_profiles); atomic(grants,original_grants)
            run("systemctl","--user","start","isolde.service")
            print("Restored prior active profile and media grants; retained failed demo files for diagnosis",flush=True)
        raise


if __name__=="__main__": main()
