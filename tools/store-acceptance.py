#!/usr/bin/env python3
"""Real Store lifecycle for Chromium and Android in a fresh guest profile."""
import argparse
import ctypes
import ctypes.util
import fcntl
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import uuid


def main():
    parser=argparse.ArgumentParser();parser.add_argument("executable",type=Path);args=parser.parse_args()
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
    process=None;window=None;phases=[];state=config/"profiles"/profile/"installed-apps.json"
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
            process=subprocess.Popen([str(args.executable.resolve()),"--launch-app=app-store"],env=env,stdout=log,stderr=log)
            deadline=time.monotonic()+25
            while not window and time.monotonic()<deadline:
                discovered=subprocess.run(["wmctrl","-lp"],env=env,capture_output=True,text=True,timeout=5)
                for line in discovered.stdout.splitlines():
                    parts=line.split(None,4)
                    if len(parts)>=3 and parts[2]==str(process.pid):window=parts[0];break
                time.sleep(.2)
            if not window:raise RuntimeError("Isolde created no test window")
            subprocess.run(["wmctrl","-ia",window],env=env,check=True,timeout=10);time.sleep(3)
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
            keys("Left","Up","Right","Return","Down")
            capture("storechromium")
            keys("Return");capture("storechromiumconsent");keys("Right","Return")
            expect(lambda:installed("plex"),"Store did not install Chromium app")
            capture("storechromiuminstalled");keys("Return");time.sleep(10)
            capture("storechromiumopen")
            capture("storechromiumclosed")
            keys("Down","Return");capture("storechromiumremove");keys("Right","Return")
            expect(lambda:not installed("plex"),"Store did not uninstall Chromium app")
            capture("storechromiumremoved")
            for app,version in (("smarttube","32.68.0"),("plex","1.0.0")):
                if (evidence/"data/Isolde/packages/profiles"/profile/app/version/"manifest.json").exists():raise RuntimeError("Uninstalled manifest remains")
            result={"passed":True,"androidLifecycle":True,"chromiumLifecycle":True,"androidPackageRemovalVerified":True,"profile":profile,"freshProfile":True,"normalProfileUsed":False,"playbackVerified":False,"captures":phases,"visualReview":"pending"}
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
