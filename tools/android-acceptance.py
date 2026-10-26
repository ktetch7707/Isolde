#!/usr/bin/env python3
"""Guest-only Android compatibility probe, not Isolde app/UI acceptance."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
import uuid


def owned(session, pid, data, uid):
    return (str(session.get("pid")) == str(pid)
            and str(session.get("user_id")) == str(uid)
            and str(session.get("waydroid_data")) == str(data))


def await_package(listing, verify, clock=time.monotonic, sleep=time.sleep, timeout=60):
    deadline = clock()+timeout
    while True:
        verify()
        if "packageName: org.smarttube.stable" in listing().splitlines(): return
        if clock() >= deadline:
            raise RuntimeError("SmartTube package absent after asynchronous installation deadline; inspect android.log")
        sleep(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.parse_args()
    if os.name != "posix" or socket.gethostname() != "isolde-test" or os.getuid() == 0:
        raise RuntimeError("Android probe requires the regular Isolde-Test guest user")
    import dbus
    desktop = subprocess.run(["systemctl", "--user", "show-environment"], check=True, capture_output=True, text=True, timeout=15).stdout
    for line in desktop.splitlines():
        key, _, value = line.partition("=")
        if key in {"XDG_RUNTIME_DIR", "WAYLAND_DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "DISPLAY", "PULSE_SERVER"}:
            os.environ[key] = value
    lock = open("/tmp/isolde-home-components-acceptance.lock", "a")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    bus = dbus.SystemBus()
    manager = dbus.Interface(bus.get_object("id.waydro.Container", "/ContainerManager"), "id.waydro.ContainerManager")
    def session():
        return dict(manager.GetSession(timeout=5))
    if session() or dbus.SessionBus().name_has_owner("id.waydro.Session"):
        raise RuntimeError("Close the existing Android session before isolated compatibility testing")
    receipt = Path("/var/lib/isolde/android-install.json")
    if receipt.is_symlink() or receipt.stat().st_uid != 0 or receipt.stat().st_size > 65536:
        raise RuntimeError("Invalid system Android receipt")
    metadata = json.loads(receipt.read_text())
    apk = Path("/usr/share/isolde/android/smarttube.apk")
    if apk.is_symlink() or not 0 < apk.stat().st_size <= 128*1024**2 or apk.stat().st_uid != 0:
        raise RuntimeError("Invalid staged Android application")
    if hashlib.sha256(apk.read_bytes()).hexdigest() != metadata.get("apkSha256"):
        raise RuntimeError("Staged Android application checksum mismatch")
    evidence = Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-android-"))
    # The probe has no Isolde profile identity and never changes profiles.json.
    # Keep its userdata for diagnosis; never recursively remove root-owned /data.
    root = Path.home()/".local/share/Isolde/android-probes"
    if root.resolve() != root or root.is_symlink():
        raise RuntimeError("Redirected Android probe directory")
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    private = root/str(uuid.uuid4())
    private.mkdir(mode=0o700)
    env = dict(os.environ)
    if not env.get("WAYLAND_DISPLAY"):
        env["WAYLAND_DISPLAY"] = "wayland-0"
    for key, name in (("HOME", "home"), ("XDG_DATA_HOME", "data"), ("XDG_CONFIG_HOME", "config"), ("XDG_CACHE_HOME", "cache")):
        path = private/name
        path.mkdir(mode=0o700)
        env[key] = str(path)
    data = Path(env["XDG_DATA_HOME"])/"waydroid/data"
    process = None
    result = {"passed": False, "compatibilityProbeOnly": True, "isoldeIntegrationVerified": False,
              "playbackVerified": False, "remoteVerified": False, "userdata": str(data)}
    print("Fixture evidence: " + str(evidence), flush=True)
    def command(*args, timeout=60):
        with tempfile.TemporaryFile() as output:
            try:
                subprocess.run(["/usr/bin/waydroid", "--details-to-stdout", *args], env=env,
                               stdout=output, stderr=subprocess.STDOUT, check=True, timeout=timeout)
            finally:
                output.seek(0)
                raw = output.read(1024*1024+1)
                text = raw[:1024*1024].decode(errors="replace")
                log.write(text); log.flush()
            if len(raw) > 1024*1024: raise RuntimeError("Oversized Android command output")
            return text
    try:
        subprocess.run(["systemctl", "--user", "stop", "isolde.service"], check=True, timeout=30)
        with (evidence/"android.log").open("w") as log:
            process = subprocess.Popen(["/usr/bin/waydroid", "--details-to-stdout", "session", "start"],
                                       env=env, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic()+120
            while True:
                state = session()
                if state and not owned(state, process.pid, data, os.getuid()):
                    raise RuntimeError("Android session ownership mismatch; refusing app operations")
                if state.get("state") == "RUNNING": break
                if process.poll() is not None: raise RuntimeError("Android session exited before boot")
                if time.monotonic() > deadline: raise RuntimeError("Android session boot timed out")
                time.sleep(.5)
            result["userdataBindingVerified"] = True
            # Listing can fail internally while CLI returns zero: require actual package state.
            command("app", "install", str(apk), timeout=120)
            def verify():
                if not owned(session(), process.pid, data, os.getuid()):
                    raise RuntimeError("Android session changed during install")
            # Upstream commits a PackageInstaller session asynchronously. CLI
            # completion acknowledges submission, not package availability.
            await_package(lambda: command("app", "list", timeout=15), verify)
            result["packageInstalled"] = True
            command("app", "launch", "org.smarttube.stable")
            time.sleep(10)
            if process.poll() is not None or not owned(session(), process.pid, data, os.getuid()):
                raise RuntimeError("Android session lost after launch request")
            result["launchRequested"] = True
            print("ANDROID_READY " + str(evidence), flush=True)
            deadline = time.monotonic()+100
            while not (evidence/"android-captured").exists():
                if time.monotonic() > deadline: raise RuntimeError("Android framebuffer capture timed out")
                time.sleep(.2)
            result["passed"] = True
    except Exception as error:
        result["error"] = str(error)
        raise
    finally:
        if process is not None and process.poll() is None:
            # Signal only the child we created. Never issue global 'session stop'.
            # SIGUSR1 ends only Waydroid's user service. SIGTERM also stops
            # its container, so use that only while binding still matches.
            process.send_signal(signal.SIGTERM if owned(session(), process.pid, data, os.getuid()) else signal.SIGUSR1)
            try: process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=5)
        state = session()
        result["ownedSessionStopped"] = not (state and process is not None and owned(state, process.pid, data, os.getuid()))
        (evidence/"result.json").write_text(json.dumps(result, indent=2))
        print("EVIDENCE " + str(evidence), flush=True)


if __name__ == "__main__":
    main()
