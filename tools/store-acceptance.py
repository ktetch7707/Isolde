#!/usr/bin/env python3
"""Real Store lifecycle for Chromium and Android in a fresh guest profile."""
import argparse
import ctypes
import ctypes.util
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import uuid


def main():
    parser=argparse.ArgumentParser();parser.add_argument("executable",type=Path);parser.add_argument("--discovery-only",action="store_true");parser.add_argument("--web-only",action="store_true");parser.add_argument("--offline-matrix",action="store_true");args=parser.parse_args()
    if socket.gethostname()!="isolde-test":raise RuntimeError("Guest-only Store acceptance")
    lock=open("/tmp/isolde-home-components-acceptance.lock","a");fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    evidence=Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-store-"));profile=str(uuid.uuid4())
    config=evidence/"config/Isolde";config.mkdir(parents=True)
    (config/"profiles.json").write_text(json.dumps({"active":profile,"profiles":[{"id":profile,"name":"Store acceptance"}]}))
    (config/"device-setup.json").write_text(json.dumps({"version":1,"step":7,"complete":True}))
    (config/"Kodi.conf").write_text("[profiles]\n"+profile+"\\kodi\\autoStart=false\n")
    (config/"Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    settings=evidence/"config/Unknown Organization";settings.mkdir()
    (settings/"Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    env=dict(os.environ)
    desktop=subprocess.run(["systemctl","--user","show-environment"],check=True,capture_output=True,text=True,timeout=15).stdout
    for line in desktop.splitlines():
        key,_,value=line.partition("=")
        if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS","WAYLAND_DISPLAY"}:env[key]=value
    env.update(XDG_CONFIG_HOME=str(evidence/"config"),XDG_DATA_HOME=str(evidence/"data"),XDG_CACHE_HOME=str(evidence/"cache"),QT_QPA_PLATFORM="xcb")
    os.environ.update({k:env[k] for k in ("DISPLAY","XAUTHORITY") if k in env})
    x=ctypes.CDLL(ctypes.util.find_library("X11"))
    x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p
    x.XStringToKeysym.argtypes=[ctypes.c_char_p];x.XStringToKeysym.restype=ctypes.c_ulong
    x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];x.XKeysymToKeycode.restype=ctypes.c_uint
    x.XFlush.argtypes=[ctypes.c_void_p];x.XCloseDisplay.argtypes=[ctypes.c_void_p]
    class Key(ctypes.Structure):
        _fields_=[("type",ctypes.c_int),("serial",ctypes.c_ulong),("send_event",ctypes.c_int),("display",ctypes.c_void_p),("window",ctypes.c_ulong),("root",ctypes.c_ulong),("subwindow",ctypes.c_ulong),("time",ctypes.c_ulong),("x",ctypes.c_int),("y",ctypes.c_int),("x_root",ctypes.c_int),("y_root",ctypes.c_int),("state",ctypes.c_uint),("keycode",ctypes.c_uint),("same_screen",ctypes.c_int)]
    class Event(ctypes.Union):_fields_=[("key",Key),("padding",ctypes.c_long*24)]
    x.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
    display=x.XOpenDisplay(env.get("DISPLAY","").encode())
    if not display:raise RuntimeError("Missing guest X11 display")
    process=None;window=None;phases=[];actions=[];state=config/"profiles"/profile/"installed-apps.json"
    profile_a=profile;profile_b=str(uuid.uuid4());network_proofs=[]
    offline=None
    if args.offline_matrix:
        spec=importlib.util.spec_from_file_location('store_offline',Path(__file__).with_name('guide-soak-acceptance.py'))
        offline=importlib.util.module_from_spec(spec);spec.loader.exec_module(offline)
    def launch(log,active):
        nonlocal process,window,profile,state
        if process and process.poll() is None:
            process.terminate();process.wait(timeout=25)
        profile=active;state=config/"profiles"/profile/"installed-apps.json"
        (config/"Kodi.conf").write_text("[profiles]\n"+"".join(p+"\\kodi\\autoStart=false\n" for p in (profile_a,profile_b)))
        state.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
        (config/"profiles.json").write_text(json.dumps({"active":active,"profiles":[{"id":profile_a,"name":"Store A"},{"id":profile_b,"name":"Store B"}]}))
        command=[str(args.executable.resolve()),"--launch-app=app-store"]
        if offline:
            proof=evidence/('offline-'+str(len(network_proofs)));proof.mkdir()
            probe=offline.SOCKET_PROBE.replace("'--windowed'","'--launch-app=app-store'")
            command=[sys.executable,'-c',probe,str(proof),str(args.executable.resolve())]
        process=subprocess.Popen(command,env=env,stdout=log,stderr=log,preexec_fn=offline.install_socket_filter if offline else None)
        window=None;deadline=time.monotonic()+25
        while not window and time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError('Isolde exited before opening its Store window')
            discovered=subprocess.run(["wmctrl","-lp"],env=env,capture_output=True,text=True,timeout=5)
            for line in discovered.stdout.splitlines():
                parts=line.split(None,4)
                if len(parts)>=3 and parts[2]==str(process.pid):window=parts[0];break
            time.sleep(.2)
        if not window:raise RuntimeError('Isolde created no test window; inspect shell.log')
        subprocess.run(["wmctrl","-ia",window],env=env,check=True,timeout=10);time.sleep(3)
        if offline:
            network=json.loads((proof/'network-denial.json').read_text())
            assert network['socketErrnos']==[101,101] and network['unixSocketAvailable'] is True
            assert network['pid']==process.pid
            network_proofs.append(network)
    def installed(app):return state.is_file() and app in json.loads(state.read_text()).get("installed",[])
    def expect(predicate,message,timeout=150):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError("Isolde exited; inspect shell.log")
            if predicate():return
            time.sleep(.3)
        raise RuntimeError(message)
    def keys(*names):
        for name in names:
            if len(actions)>=5000:raise RuntimeError("Store input exceeded its bound")
            actions.append(name)
            code=x.XKeysymToKeycode(display,x.XStringToKeysym(name.encode()))
            if not code:raise RuntimeError("Missing keycode")
            for kind,mask in ((2,1),(3,2)):
                event=Event();event.key=Key(type=kind,send_event=1,display=display,window=int(window,16),keycode=code,same_screen=1)
                x.XSendEvent(display,int(window,16),1,mask,ctypes.byref(event))
            x.XFlush(display);time.sleep(.25)
        time.sleep(.5)
    def capture(phase):
        phases.append(phase);print(phase.upper()+"_READY "+str(evidence),flush=True)
        expect(lambda:(evidence/(phase+"-captured")).exists(),"Capture timed out: "+phase,100)
        time.sleep(2)
    print("Fixture evidence: "+str(evidence),flush=True)
    try:
        subprocess.run(["systemctl","--user","stop","isolde.service"],check=True,timeout=30)
        with (evidence/"shell.log").open("w") as log:
            launch(log,profile)
            if args.offline_matrix:
                def manifest(active):return evidence/'data/Isolde/packages/profiles'/active/'plex/1.0.0/manifest.json'
                def activation(active):return config/'profiles'/active/'installed-apps.json'
                def value(path):return path.read_bytes() if path.is_file() else None
                def plex():keys('Left','Up','Up','Right','Return','Down','Down')
                capture('offlineentry');plex()
                before=value(state)
                keys('Return');capture('offlinecancel');keys('Escape')
                assert value(state)==before and not manifest(profile).exists(),'Cancel wrote a Web app'
                state.parent.chmod(0o500)
                try:
                    keys('Return','Right','Return');capture('offlinewritefailed')
                    assert value(state)==before and not installed('plex') and not manifest(profile).exists(),'Failed activation left an installed package'
                finally:state.parent.chmod(0o700)
                keys('Return','Right','Return');expect(lambda:installed('plex'),'Offline retry did not install Plex')
                capture('offlineainstalled');saved_a=value(state);manifest_a=value(manifest(profile))
                assert manifest_a
                launch(log,profile_b);plex();capture('offlinebfresh')
                assert not installed('plex') and value(activation(profile_a))==saved_a and value(manifest(profile_a))==manifest_a
                keys('Return','Right','Return');expect(lambda:installed('plex'),'Second profile install failed');capture('offlinebinstalled')
                keys('Down','Return','Right','Return');expect(lambda:not installed('plex'),'Second profile removal failed');capture('offlinebremoved')
                saved_b=value(state)
                assert not manifest(profile_b).exists() and value(activation(profile_a))==saved_a and value(manifest(profile_a))==manifest_a
                launch(log,profile_a);plex();capture('offlineacold')
                assert installed('plex') and value(state)==saved_a and value(manifest(profile_a))==manifest_a
                keys('Return');time.sleep(10);capture('offlinewebopen');capture('offlinewebreturn')
                keys('Down','Return');capture('offlinearemove');keys('Return')
                assert installed('plex') and value(state)==saved_a,'Default Cancel removed the Web app'
                keys('Return','Right','Return');expect(lambda:not installed('plex'),'Offline Web removal failed');capture('offlineremoved')
                assert not manifest(profile_a).exists() and value(activation(profile_b))==saved_b
                keys('Left','Up','Up','Left','Return','Down','Return');time.sleep(3);capture('offlinerepositoryerror')
                assert not any((evidence/'cache/Isolde/profiles').glob('*/fdroid/index-v2.json')),'Disconnected repository unexpectedly obtained an index'
                keys('Menu','Return');capture('offlineaccessoff')
                keys('Escape');keys(*(['Up']*30));keys('Return');capture('offlinesettings')
                result=dict(passed=True,journey='store-offline',freshProfile=True,normalProfileUsed=False,profileIsolation=True,coldReopen=True,cancelNoWrite=True,writeFailureRollback=True,retryVerified=True,webLifecycle=True,repositoryFailureRecovered=True,manifestsRemoved=True,androidLifecycle=False,networkProofs=network_proofs,actualInputActions=len(actions),captures=phases,visualReview='pending')
                (evidence/'result.json').write_text(json.dumps(result,indent=2));return
            if args.discovery_only:
                from guide_features import remote_text
                def key(name,count=1):keys(*([name]*count))
                cache=evidence/"cache/Isolde/profiles"/profile/"fdroid"
                index=cache/"index-v2.json";archive=cache/"entry.jar"
                capture("storediscoveryentry")
                assert not index.exists(),"Opening Store unexpectedly contacted its repository"
                keys("Up","Return");capture("storediscoveryloading")
                expect(lambda:index.is_file() and archive.is_file(),"Live signed repository was not loaded",180)
                def provider_idle():
                    children=Path(f"/proc/{process.pid}/task/{process.pid}/children").read_text().split()
                    for child in children:
                        try:command=Path(f"/proc/{child}/cmdline").read_bytes()
                        except FileNotFoundError:continue
                        if b"fdroid-store.py" in command:return False
                    return True
                expect(provider_idle,"Live catalog request did not settle",60)
                capture("storediscoverybrowse")
                keys("Up","Up","Left","Left","Return");capture("storediscoverykeyboard")
                remote_text(key,"newpipe");expect(provider_idle,"Live title search did not settle",60)
                capture("storediscoveryresults")
                spec=importlib.util.spec_from_file_location("verified_store",Path(__file__).with_name("fdroid-store.py"))
                repository=importlib.util.module_from_spec(spec);spec.loader.exec_module(repository)
                document=repository.verified_index(repository.verified_entry(archive.read_bytes()),index.read_bytes())
                matches=repository.browse(document,"newpipe",0,repository.runtime_target())["results"]
                assert matches and any("newpipe" in item["name"].casefold() for item in matches),"Signed live search has no NewPipe result"
                assert not state.exists(),"Browsing installed an app without confirmation"
                keys("Escape");capture("storediscoveryreturn")
                key("Up",30);capture("storediscoverysettingsfocus")
                keys("Return");capture("storediscoverysettings")
                result=dict(passed=True,journey="store-discovery",freshProfile=True,normalProfileUsed=False,catalogOnly=True,installedAppsChanged=False,provider="fdroid-signed-v2",query="newpipe",matches=[item["packageName"] for item in matches],indexSha256=hashlib.sha256(index.read_bytes()).hexdigest(),entrySha256=hashlib.sha256(archive.read_bytes()).hexdigest(),actualInputActions=len(actions),captures=phases,visualReview="pending")
                (evidence/"result.json").write_text(json.dumps(result,indent=2));return
            if not args.web_only:
                capture("storeandroid")
                keys("Return");capture("storeandroidconsent");keys("Right","Return")
                expect(lambda:installed("smarttube"),"Store did not install the Android app")
                capture("storeandroidinstalled")
                keys("Return")
                expect(lambda:'Android app session ready "smarttube"' in (evidence/"shell.log").read_text(),"Store did not launch Android")
                time.sleep(8);capture("storeandroidopen") # Host runner sends only the VM's remote Home key.
                capture("storeandroidclosed")
                keys("Down","Return");capture("storeandroidremove");keys("Right","Return")
                expect(lambda:not installed("smarttube"),"Store did not verify Android removal")
                packages=evidence/"data/Isolde/android-profiles"/profile/"packages.json"
                if "smarttube" in json.loads(packages.read_text()):raise RuntimeError("Android receipt remains after removal")
                capture("storeandroidremoved")
            keys("Left","Up","Up","Right","Return","Down","Down")
            capture("storechromium")
            keys("Return");capture("storechromiumconsent");keys("Right","Return")
            expect(lambda:installed("plex"),"Store did not install Chromium app")
            capture("storechromiuminstalled");keys("Return");time.sleep(10)
            capture("storechromiumopen")
            capture("storechromiumclosed")
            keys("Down","Return");capture("storechromiumremove");keys("Right","Return")
            expect(lambda:not installed("plex"),"Store did not uninstall Chromium app")
            capture("storechromiumremoved")
            for app,version in (("plex","1.0.0"),) if args.web_only else (("smarttube","32.68.0"),("plex","1.0.0")):
                if (evidence/"data/Isolde/packages/profiles"/profile/app/version/"manifest.json").exists():raise RuntimeError("Uninstalled manifest remains")
            result={"passed":True,"androidLifecycle":not args.web_only,"chromiumLifecycle":True,"androidPackageRemovalVerified":not args.web_only,"profile":profile,"freshProfile":True,"normalProfileUsed":False,"playbackVerified":False,"captures":phases,"visualReview":"pending","webOnly":args.web_only,"actualInputActions":len(actions),"manifestsRemoved":True}
            (evidence/"result.json").write_text(json.dumps(result,indent=2))
    except Exception:
        # Keep the still-running failure UI reviewable before owned-process cleanup.
        if process and process.poll() is None:
            try: capture("storefailure")
            except Exception: pass
        raise
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=25)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        x.XCloseDisplay(display)
        print("EVIDENCE "+str(evidence),flush=True)


if __name__=="__main__":main()
