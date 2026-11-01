#!/usr/bin/env python3
"""Review the actual installed shell through bounded guest-only remote input."""
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
    return subprocess.run(args,check=True,capture_output=True,text=True,timeout=15).stdout

def main():
    parser=argparse.ArgumentParser();parser.add_argument("executable",type=Path);args=parser.parse_args()
    if os.name!="posix" or socket.gethostname()!="isolde-test":raise RuntimeError("Guest-only shell acceptance")
    lock=open("/tmp/isolde-home-components-acceptance.lock","a")
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    profiles=json.loads((Path.home()/".config/Isolde/profiles.json").read_text())
    if profiles.get("active")!="8bdf6077-4613-4b52-9b20-d1ec7931dfcf":raise RuntimeError("Select Studio Demo before shell acceptance")
    evidence=Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-shell-"))
    env=dict(os.environ)
    for line in run("systemctl","--user","show-environment").splitlines():
        key,_,value=line.partition("=")
        if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS"}:env[key]=value
    env["QT_QPA_PLATFORM"]="xcb"
    os.environ.update({key:env[key] for key in ("DISPLAY","XAUTHORITY") if key in env})
    service=subprocess.run(["systemctl","--user","is-active","--quiet","isolde.service"]).returncode==0
    process=None
    try:
        if service:run("systemctl","--user","stop","isolde.service")
        with (evidence/"shell.log").open("w") as log:
            process=subprocess.Popen([str(args.executable.resolve())],env=env,stdout=log,stderr=log)
            deadline=time.monotonic()+25;window=None
            while time.monotonic()<deadline:
                if process.poll() is not None:raise RuntimeError("Shell exited; inspect shell.log")
                for line in run("wmctrl","-lp",).splitlines():
                    cells=line.split(None,4)
                    if len(cells)>=3 and cells[2]==str(process.pid) and re.fullmatch(r"0x[0-9a-fA-F]+",cells[0]):window=cells[0];break
                if window:break
                time.sleep(.2)
            if not window:raise RuntimeError("Installed shell created no window")
            run("wmctrl","-ia",window);time.sleep(3)
            phases=("shellnavigation","shellgallery","shellstore","shellreturn","shellambient","shellwake")
            for phase in phases:
                print(phase.upper()+"_READY "+str(evidence),flush=True)
                deadline=time.monotonic()+110
                while not (evidence/(phase+"-captured")).exists():
                    if process.poll() is not None:raise RuntimeError("Shell exited; inspect shell.log")
                    if time.monotonic()>deadline:raise RuntimeError("Shell frame review timed out: "+phase)
                    time.sleep(.1)
                time.sleep(1)
            result={"passed":True,"reviewSequenceCaptured":list(phases),"hostUiAutomationUsed":False,"layoutWrites":False,"visualReview":"pending","semanticAcceptance":"not_verified"}
            (evidence/"result.json").write_text(json.dumps(result,indent=2))
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=15)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        if service:run("systemctl","--user","start","isolde.service")
        print("EVIDENCE "+str(evidence),flush=True)

if __name__=="__main__":main()
