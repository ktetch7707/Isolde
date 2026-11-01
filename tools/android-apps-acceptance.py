#!/usr/bin/env python3
"""Review installed All Apps -> Android app -> remote Home inside Isolde-Test."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=15).stdout


def main():
    parser=argparse.ArgumentParser();parser.add_argument("executable",type=Path);args=parser.parse_args()
    if os.name!="posix" or socket.gethostname()!="isolde-test": raise RuntimeError("Guest-only Android All Apps acceptance")
    lock=open("/tmp/isolde-home-components-acceptance.lock","a");fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    profiles=json.loads((Path.home()/".config/Isolde/profiles.json").read_text())
    if profiles.get("active")!="8bdf6077-4613-4b52-9b20-d1ec7931dfcf": raise RuntimeError("Select Studio Demo before Android All Apps acceptance")
    evidence=Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-android-apps-"))
    env=dict(os.environ)
    for line in run("systemctl","--user","show-environment").splitlines():
        key,_,value=line.partition("=")
        if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS","WAYLAND_DISPLAY"}:env[key]=value
    env["QT_QPA_PLATFORM"]="xcb"
    process=None
    print("Fixture evidence: "+str(evidence),flush=True)
    try:
        run("systemctl","--user","stop","isolde.service")
        with (evidence/"shell.log").open("w") as log:
            process=subprocess.Popen([str(args.executable.resolve())],env=env,stdout=log,stderr=log)
            deadline=time.monotonic()+25;window=None
            while time.monotonic()<deadline:
                if process.poll() is not None:raise RuntimeError("Isolde exited; inspect shell.log")
                discovered=subprocess.run(["wmctrl","-lp"],env=env,capture_output=True,text=True,timeout=5)
                # XWayland can precede publication of its client list. Keep the
                # same bounded window wait; never launch a second Isolde process.
                if discovered.returncode:
                    (evidence/"window-discovery.json").write_text(json.dumps({"exit":discovered.returncode,"error":discovered.stderr[:2048]}))
                for line in discovered.stdout.splitlines() if discovered.returncode==0 else []:
                    cells=line.split(None,4)
                    if len(cells)>=3 and cells[2]==str(process.pid) and re.fullmatch(r"0x[0-9a-fA-F]+",cells[0]):window=cells[0];break
                if window:break
                time.sleep(.2)
            if not window:raise RuntimeError("Installed Isolde created no window")
            subprocess.run(["wmctrl","-ia",window],env=env,check=True,timeout=10);time.sleep(3)
            phases=("androidnavigation","androidgallery","androidlaunch","androidpermission","androidopen","androidremote","androidreturn")
            for phase in phases:
                if phase=="androidopen":
                    deadline=time.monotonic()+240
                    while 'Android app session ready "smarttube"' not in (evidence/"shell.log").read_text():
                        if time.monotonic()>deadline:raise RuntimeError("All Apps did not launch SmartTube; inspect shell.log")
                        time.sleep(.5)
                    time.sleep(30)
                print(phase.upper()+"_READY "+str(evidence),flush=True)
                deadline=time.monotonic()+110
                while not (evidence/(phase+"-captured")).exists():
                    if process.poll() is not None:raise RuntimeError("Isolde exited; inspect shell.log")
                    if time.monotonic()>deadline:raise RuntimeError("Android frame review timed out: "+phase)
                    time.sleep(.1)
                time.sleep(2)
            import dbus
            bus=dbus.SystemBus();manager=dbus.Interface(bus.get_object("id.waydro.Container","/ContainerManager"),"id.waydro.ContainerManager")
            if manager.GetSession(timeout=5):raise RuntimeError("Android container did not stop after remote Home")
            result={"passed":True,"sharedSessionLaunch":True,"ownedSessionStopped":True,"reviewSequenceCaptured":list(phases),
                    "hostUiAutomationUsed":False,"layoutWrites":False,"playbackVerified":False,"visualReview":"pending","semanticAcceptance":"not_verified"}
            (evidence/"result.json").write_text(json.dumps(result,indent=2))
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=25)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        print("EVIDENCE "+str(evidence),flush=True)


if __name__=="__main__":main()
