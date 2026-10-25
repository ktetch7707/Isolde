#!/usr/bin/env python3
"""Bounded installed VPN/CBC journey; invoked only by the VM oracle guardian."""
import argparse
import ctypes
import ctypes.util
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

def run(*args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=kwargs.pop("timeout", 15), **kwargs).stdout

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    profile = "8bdf6077-4613-4b52-9b20-d1ec7931dfcf"
    config = Path.home()/".config/Isolde"
    profiles = json.loads((config/"profiles.json").read_text())
    if profiles.get("active") != profile:
        raise RuntimeError("Select the authorized Studio Demo profile before VPN acceptance.")
    evidence = Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-vpn-"))
    print("VPN_EVIDENCE " + str(evidence), flush=True)
    support_path = Path(__file__).with_name("vpn-setup.py")
    spec = importlib.util.spec_from_file_location("vpn_support", support_path)
    support = importlib.util.module_from_spec(spec); spec.loader.exec_module(support)
    support.PROFILE = profile
    before = support.status()
    if not all(p["installed"] for p in before["providers"]):
        raise RuntimeError("Default VPN providers are not both installed.")
    env = dict(os.environ)
    for line in run("systemctl", "--user", "show-environment").splitlines():
        key, _, value = line.partition("=")
        if key in {"DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}:
            env[key] = value
    env["QT_QPA_PLATFORM"] = "xcb"
    os.environ.update({k:env[k] for k in ("DISPLAY","XAUTHORITY") if k in env})
    x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
    x11.XOpenDisplay.argtypes=[ctypes.c_char_p]; x11.XOpenDisplay.restype=ctypes.c_void_p
    x11.XStringToKeysym.argtypes=[ctypes.c_char_p]; x11.XStringToKeysym.restype=ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];x11.XKeysymToKeycode.restype=ctypes.c_uint
    x11.XFlush.argtypes=[ctypes.c_void_p];x11.XCloseDisplay.argtypes=[ctypes.c_void_p]
    class KeyEvent(ctypes.Structure):
        _fields_=[("type",ctypes.c_int),("serial",ctypes.c_ulong),("send_event",ctypes.c_int),
                  ("display",ctypes.c_void_p),("window",ctypes.c_ulong),("root",ctypes.c_ulong),
                  ("subwindow",ctypes.c_ulong),("time",ctypes.c_ulong),("x",ctypes.c_int),("y",ctypes.c_int),
                  ("x_root",ctypes.c_int),("y_root",ctypes.c_int),("state",ctypes.c_uint),
                  ("keycode",ctypes.c_uint),("same_screen",ctypes.c_int)]
    class Event(ctypes.Union):
        _fields_=[("key",KeyEvent),("padding",ctypes.c_long*24)]
    x11.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
    display=x11.XOpenDisplay(env.get("DISPLAY","").encode())
    if not display: raise RuntimeError("Guest X11 display unavailable.")
    process=None
    service=subprocess.run(["systemctl","--user","is-active","--quiet","isolde.service"]).returncode==0
    def capture(phase):
        print(phase.upper()+"_READY "+str(evidence),flush=True)
        deadline=time.monotonic()+110
        while not (evidence/(phase.lower()+"-captured")).exists():
            if time.monotonic()>deadline:raise RuntimeError("Capture acknowledgement timed out: "+phase)
            time.sleep(.1)
    def wait(predicate, message, seconds=20):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            if process and process.poll() is not None:raise RuntimeError("Shell exited. Inspect "+str(evidence/"shell.log"))
            if predicate():return
            time.sleep(.2)
        raise RuntimeError(message)
    def key(name,count=1):
        code=x11.XKeysymToKeycode(display,x11.XStringToKeysym(name.encode()))
        if not code:raise RuntimeError("Missing guest key")
        for _ in range(count):
            for kind,mask in ((2,1),(3,2)):
                event=Event();event.key=KeyEvent(type=kind,send_event=1,display=display,window=int(window,16),keycode=code,same_screen=1)
                x11.XSendEvent(display,int(window,16),1,mask,ctypes.byref(event))
            x11.XFlush(display);time.sleep(.12)
        time.sleep(.35)
    def activate():
        run("wmctrl","-i","-a",window,env=env)
        time.sleep(.5)
    def windows():
        return run("wmctrl","-lp",env=env).splitlines()
    try:
        if service:run("systemctl","--user","stop","isolde.service")
        with (evidence/"shell.log").open("w") as log:
            process=subprocess.Popen([str(args.executable.resolve()),"--launch-app=settings"],env=env,stdout=log,stderr=log)
            found=[]
            def find_shell():
                found[:]=[line.split()[0] for line in windows() if len(line.split())>=3 and line.split()[2]==str(process.pid)]
                return bool(found)
            wait(find_shell,"Shell created no window")
            window=found[0];activate();time.sleep(3)
            # Review the settled Settings frame before sending physical guest
            # navigation. Window creation alone does not imply startup is ready.
            capture("vpnnavigation")
            time.sleep(1)
            capture("vpnsetup")
            current=support.active_connections()
            country=support.observed_country(current[0]["uuid"]) if len(current)==1 else None
            if country != "CA":
                key("Down",4);key("Return");time.sleep(2)
                capture("vpnpermission")
                capture("vpnprovider")
                capture("vpnproviderstate")
                # Bounded guest Home returns through the app's granted shortcut.
                capture("vpnproviderreturn")
                time.sleep(1);activate()
                current=support.active_connections()
                country=support.observed_country(current[0]["uuid"]) if len(current)==1 else None
                if country!="CA":raise RuntimeError("Native provider is not connected to Canada; no CBC acceptance claim.")
                # Settings retains the exact Open-provider row.
                key("Down")
            else:
                key("Down",5)
            # Save connected location through production UI, with Back cancellation first.
            key("Return");capture("vpnconsent")
            owned_before={c["uuid"] for c in support.managed_connections()}
            key("Escape")
            if {c["uuid"] for c in support.managed_connections()}!=owned_before:
                raise RuntimeError("Back from location-save consent wrote a connection")
            key("Down",5);key("Return");key("Return")
            time.sleep(2);capture("vpnauthorization")
            saved=[]
            def saved_location():
                if "VPN_SETUP_FAILED" in (evidence/"shell.log").read_text():
                    raise RuntimeError("Isolde reports VPN setup failure; inspect the safe log and failure frame.")
                saved[:]=[c for c in support.managed_connections() if c["uuid"] not in owned_before and c["country"]=="CA"]
                return bool(saved) and "VPN_LOCATION_SAVED_VERIFIED" in (evidence/"shell.log").read_text()
            try:
                wait(saved_location,"Save connected location did not finish; inspect VPN screen/log.",90)
            except RuntimeError:
                capture("vpnfailure")
                raise
            wait(lambda: any(c["uuid"]==saved[0]["uuid"] for c in support.active_connections()),"Saved location did not activate")
            capture("vpnsaved")
            # Rules file is authored by the UI, never written by this harness.
            rules=config/"profiles"/profile/"app-vpn.json"
            key("Down");key("Return")  # main row 1: per-app assignment
            # Registry adds package registrations after the packaged defaults.
            # Walk actual visible application rows via bounded remote navigation,
            # looking for the rule commit after each CBC-specific capture is reviewed.
            capture("vpnapps")
            # The test helper writes only bounded guest keys reviewed against this frame.
            capture("vpnassign")
            wait(lambda: rules.is_file() and json.loads(rules.read_text()).get("routes",{}).get("cbc-gem")==saved[0]["uuid"],
                 "Assign CBC Gem to the saved Canada location in the per-app UI.",90)
            capture("vpnassigned")
            # Save a second country through the same product workflow, then
            # prove CBC changes from US to CA rather than rechecking active CA.
            key("Escape")
            support.connection_action("disconnect",saved[0]["uuid"])
            key("Down",4);key("Return");time.sleep(2)
            for phase in ("vpnuspermission","vpnusprovider","vpnussearch","vpnusconnect"):
                capture(phase)
            capture("vpnusreturn");time.sleep(1);activate()
            current=support.active_connections()
            if len(current)!=1 or support.observed_country(current[0]["uuid"])!="US":
                raise RuntimeError("Native provider is not connected to the United States; no automatic switching claim.")
            key("Down");key("Return");capture("vpnusconsent");key("Return")
            time.sleep(2);capture("vpnusauthorization")
            saved_us=[]
            def saved_us_location():
                if "VPN_SETUP_FAILED" in (evidence/"shell.log").read_text():
                    raise RuntimeError("Isolde reports US location-save failure; inspect its safe log.")
                saved_us[:]=[c for c in support.managed_connections() if c["uuid"] not in owned_before and c["country"]=="US"]
                return bool(saved_us)
            wait(saved_us_location,"US location did not save",90)
            wait(lambda:any(c["uuid"]==saved_us[0]["uuid"] for c in support.active_connections()),"Saved US location did not activate")
            capture("vpnussaved")
            key("Home")
            # Trigger the real registered app using the production startup launch path.
            process.terminate();process.wait(timeout=15)
            previous_windows={line.split()[0] for line in windows()}
            process=subprocess.Popen([str(args.executable.resolve()),"--launch-app=cbc-gem"],env=env,stdout=log,stderr=log)
            time.sleep(2);capture("cbcvpnpermission")
            wait(lambda:"VPN_LOCATION_VERIFIED cbc-gem CA" in (evidence/"shell.log").read_text(),"CBC launch did not verify Canada",90)
            wait(lambda:any(line.split()[0] not in previous_windows and ("gem.cbc.ca" in line.lower() or ("cbc" in line.lower() and "gem" in line.lower())) for line in windows()),
                 "The real CBC Gem client window did not open",45)
            time.sleep(8);capture("cbcvpn")
            active=support.active_connections()
            if len(active)!=1 or support.observed_country(active[0]["uuid"])!="CA":
                raise RuntimeError("CBC is not using the verified Canadian connection")
            result={"passed":True,"defaultProvidersInstalled":True,"uiAuthoredRule":True,"nativeClientOpened":True,"app":"cbc-gem","country":"CA",
                    "countryVerified":True,"savedCountries":["CA","US"],"automaticSwitchFrom":"US","browserUsedForSetup":False,"hostUiAutomationUsed":False,
                    "playbackVerified":False,"visualReview":"pending"}
            (evidence/"result.json").write_text(json.dumps(result,indent=2))
            print("RESULT "+json.dumps(result),flush=True)
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=15)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        x11.XCloseDisplay(display)
        if service:run("systemctl","--user","start","isolde.service")
        print("EVIDENCE "+str(evidence),flush=True)

if __name__=="__main__":main()
