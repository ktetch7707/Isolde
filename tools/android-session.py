#!/usr/bin/env python3
"""Exclusive profile-owned Android app session; no root or global stop command."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time
import importlib.util
import select

stopping = False


class Stopped(Exception): pass


def check_stop():
    if stopping: raise Stopped()


def emit(event, **fields):
    print(json.dumps({"event": event, **fields}), flush=True)


def private_directory(path):
    if path.resolve() != path: raise RuntimeError("Android storage is redirected; choose an ordinary local profile directory.")
    path.mkdir(parents=True, exist_ok=True, mode=0o700)
    info = path.stat()
    if info.st_uid != os.getuid() or info.st_mode & 0o077:
        raise RuntimeError("Android profile storage must be private to this Linux user.")


def profile_root(profile, config, data):
    if profile != "default" and not re.fullmatch(r"[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}", profile):
        raise ValueError("Invalid Android profile identity.")
    selection = config/"Isolde/profiles.json"
    if selection.is_symlink() or (selection.exists() and selection.stat().st_size > 1024*1024):
        raise ValueError("Invalid profile selection document.")
    if selection.exists() and json.loads(selection.read_text()).get("active", "default") != profile:
        raise ValueError("The selected Isolde profile changed. Return Home and try again.")
    parent = data/"Isolde/android-profiles"
    private_directory(parent)
    root = parent/profile
    private_directory(root)
    return root


def owned(session, pid, data):
    return (str(session.get("pid")) == str(pid) and str(session.get("user_id")) == str(os.getuid())
            and str(session.get("waydroid_data")) == str(data))


def save_packages(path, packages):
    fd, temporary = tempfile.mkstemp(prefix=".packages-", dir=path.parent)
    try:
        with os.fdopen(fd, "w") as stream:
            json.dump(packages, stream); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try: os.fsync(directory)
        finally: os.close(directory)
    finally:
        Path(temporary).unlink(missing_ok=True)


def command(env, *args, timeout=60):
    check_stop()
    with tempfile.TemporaryFile() as output:
        child = subprocess.Popen(["/usr/bin/waydroid", "--details-to-stdout", *args], env=env,
                                 stdout=output, stderr=subprocess.STDOUT)
        deadline = time.monotonic()+timeout
        try:
            while child.poll() is None:
                check_stop()
                if time.monotonic() > deadline: raise RuntimeError("Android did not respond in time. Return Home and retry.")
                if output.tell() > 1024*1024: raise RuntimeError("Android returned too much diagnostic data.")
                time.sleep(.1)
            if child.returncode: raise RuntimeError("Android could not complete the requested operation. Return Home and retry.")
            output.seek(0); raw = output.read(1024*1024+1)
            if len(raw) > 1024*1024: raise RuntimeError("Android returned too much diagnostic data.")
            return raw.decode(errors="replace")
        finally:
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=3)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=3)


def store_package(app_id, version, profile, data_home):
    """Resolve only a checksummed APK inside the selected profile's package store."""
    if not re.fullmatch(r"fdroid-[a-f0-9]{24}", app_id) or not re.fullmatch(r"[0-9]{1,18}", version):
        raise ValueError("Invalid Store package identity.")
    base = data_home/"Isolde/packages/profiles"/profile/app_id/version
    if base.resolve() != base:
        raise ValueError("Store package storage is redirected.")
    manifest, apk = base/"manifest.json", base/"app.apk"
    for path, maximum in ((manifest, 65536), (apk, 128*1024**2)):
        if path.is_symlink() or not path.is_file() or path.stat().st_uid != os.getuid() or not 0 < path.stat().st_size <= maximum:
            raise ValueError("Store package is missing or unsafe. Reinstall it from the Store.")
    app = json.loads(manifest.read_text())
    package = app.get("packageName", "")
    expected = "fdroid-"+hashlib.sha256(package.encode()).hexdigest()[:24]
    if (app.get("schemaVersion") != 1 or app.get("kind") != "fdroid" or app.get("id") != app_id
            or expected != app_id or app.get("version") != version
            or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+", package)
            or not re.fullmatch(r"[a-f0-9]{64}", app.get("sha256", ""))
            or hashlib.sha256(apk.read_bytes()).hexdigest() != app["sha256"]):
        raise ValueError("Store package verification failed. Reinstall it from the Store.")
    return app, apk


def package_tool(arguments):
    with tempfile.TemporaryFile() as output:
        try:
            child=subprocess.Popen(arguments,stdout=output,stderr=subprocess.STDOUT)
        except OSError as error:
            raise RuntimeError("Android verification tools are missing. Use Store actions > Set up / repair Android.") from error
        deadline=time.monotonic()+30
        try:
            while child.poll() is None:
                check_stop()
                if time.monotonic()>deadline or output.tell()>1024**2:
                    raise RuntimeError("Android signature/metadata verification exceeded its time or output limit")
                time.sleep(.1)
            output.seek(0);data=output.read(1024**2+1)
            if child.returncode or len(data)>1024**2:
                raise RuntimeError("Android package signature or metadata verification failed")
            return data
        finally:
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=3)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=3)


def validate_store_apk(apk, app, version):
    package_tool(["/usr/bin/apksigner", "verify", str(apk)])
    metadata=package_tool(["/usr/bin/aapt", "dump", "badging", str(apk)])
    identity = re.search(rb"^package: name='([^']+)' versionCode='([^']+)'", metadata, re.MULTILINE)
    if not identity or identity.groups() != (app["packageName"].encode(), version.encode()):
        raise ValueError("Downloaded Android package identity or version does not match the verified index")


def installed_artifact(data, expected, package=None):
    """A package name alone does not prove that an asynchronous update finished."""
    parent = data/"app"
    if parent.resolve()!=parent or not parent.is_dir(): return False
    count = 0
    candidates = []
    for directory in parent.iterdir():
        if directory.is_symlink() or not directory.is_dir(): continue
        count += 1
        if count>4096: raise RuntimeError("Android package directory exceeds its limit")
        if package is None or directory.name.startswith(package+"-"): candidates.append(directory/"base.apk")
        for child in directory.iterdir():
            count += 1
            if count>4096: raise RuntimeError("Android package directory exceeds its limit")
            if not child.is_symlink() and child.is_dir() and (package is None or child.name.startswith(package+"-")): candidates.append(child/"base.apk")
    for path in candidates:
        try:
            if path.is_symlink() or not path.is_file() or not 0<path.stat().st_size<=128*1024**2: continue
            digest=hashlib.sha256()
            with path.open("rb") as stream:
                for block in iter(lambda:stream.read(65536),b""): digest.update(block)
            if digest.hexdigest()==expected: return True
        except PermissionError:
            continue
    return False


def administrator_transaction(action, root, activation, profile):
    """Back up Android-owned files without granting root to a user-writable script."""
    helper=Path("/usr/lib/isolde-android-setup/android-setup.py")
    for path in (helper,*helper.parents):
        if not path.exists() or path.is_symlink() or path.stat().st_uid!=0 or path.stat().st_mode & 0o022:
            raise RuntimeError("Open Store actions > Set up / repair Android to enable safe app transactions")
    activation.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
    if activation.parent.resolve()!=activation.parent: raise ValueError("App activation storage is redirected")
    activation.parent.chmod(0o700)
    marker=root/".store-cancel"
    if action=="prepare": marker.unlink(missing_ok=True)
    emit("status",message="Administrator approval may be needed for protected Android data backup or recovery.")
    with tempfile.TemporaryFile() as output:
        child=subprocess.Popen(["/usr/bin/pkexec",str(helper),"--transaction-action",action,
            "--profile",profile,"--profile-root",str(root),"--activation",str(activation)],
            stdout=output,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL)
        deadline=time.monotonic()+300
        while child.poll() is None:
            if stopping and action=="prepare": marker.touch()
            if time.monotonic()>deadline or output.tell()>65536:
                marker.touch()
                raise RuntimeError("Android backup or recovery did not finish. Preserve profile storage and retry recovery.")
            time.sleep(.1)
        output.seek(0);raw=output.read(65537)
        result=None
        for line in raw.splitlines():
            try: event=json.loads(line)
            except ValueError: continue
            if event.get("event")=="transaction": result=event
            elif event.get("event")=="error": raise RuntimeError(event.get("message","Android backup failed"))
        if child.returncode or result is None:
            raise RuntimeError("Android backup/recovery approval was cancelled or failed. Retry the operation; app data is preserved.")
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--app", required=True)
    parser.add_argument("--operation", choices=("launch", "install", "uninstall"), default="launch")
    parser.add_argument("--store-version")
    parser.add_argument("--transaction", action="store_true")
    args = parser.parse_args()
    if args.transaction and args.operation=="launch":
        raise ValueError("Store transactions are only valid for install or removal")
    if sys.platform != "linux" or os.getuid() == 0:
        raise RuntimeError("Android apps require the Linux desktop session, without administrator privileges.")
    config_path = Path(__file__).with_name("config")/"android-runtime.json"
    if config_path.stat().st_size > 65536: raise RuntimeError("Invalid bundled Android configuration.")
    config = json.loads(config_path.read_text())
    config_home = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home()/".config")))
    data_home = Path(os.environ.get("XDG_DATA_HOME", str(Path.home()/".local/share")))
    profile_root(args.profile, config_home, data_home)
    if config.get("schemaVersion") != 1:
        raise RuntimeError("Invalid Android runtime configuration.")
    if args.store_version:
        app, apk = store_package(args.app, args.store_version, args.profile, data_home)
        validate_store_apk(apk, app, args.store_version)
        package = app["packageName"]
    else:
        if args.app not in config.get("apps", {}):
            raise RuntimeError("This Android app is not in the verified installer catalog.")
        app = config["apps"][args.app]
        package = app["packageName"]
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+", package):
            raise RuntimeError("Invalid Android package identity.")
        receipt = Path("/var/lib/isolde/android-install.json")
        apk = Path("/usr/share/isolde/android")/(args.app+".apk")
        for path, maximum in ((receipt, 65536), (apk, 128*1024**2)):
            if path.is_symlink() or not path.is_file() or path.stat().st_uid != 0 or not 0 < path.stat().st_size <= maximum:
                raise RuntimeError("Android setup is incomplete. Run the Isolde installer with Android support.")
        installed = json.loads(receipt.read_text())
        if installed.get("configurationSha256") != hashlib.sha256(config_path.read_bytes()).hexdigest() or hashlib.sha256(apk.read_bytes()).hexdigest() != app["sha256"]:
            raise RuntimeError("Android package verification failed. Run the Isolde installer again.")
    runtime = Path(os.environ["XDG_RUNTIME_DIR"])
    if runtime.resolve() != runtime or runtime.stat().st_uid != os.getuid(): raise RuntimeError("Invalid desktop runtime directory.")
    fd = os.open(runtime/"isolde-android.lock", os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    if os.fstat(fd).st_uid != os.getuid(): raise RuntimeError("Invalid Android session lock.")
    lock = os.fdopen(fd, "w")
    try: fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError: raise RuntimeError("Another Android app is active. Return Home before switching.")
    import dbus
    manager = dbus.Interface(dbus.SystemBus().get_object("id.waydro.Container", "/ContainerManager"), "id.waydro.ContainerManager")
    def session(): return dict(manager.GetSession(timeout=5))
    if session() or dbus.SessionBus().name_has_owner("id.waydro.Session"):
        raise RuntimeError("Another Android session is running. Close it before launching this profile's app.")
    config_home = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home()/".config")))
    data_home = Path(os.environ.get("XDG_DATA_HOME", str(Path.home()/".local/share")))
    root = profile_root(args.profile, config_home, data_home)
    activation = config_home/"Isolde/profiles"/args.profile/"installed-apps.json"
    transaction_spec = importlib.util.spec_from_file_location("android_transaction", Path(__file__).with_name("android-store-transaction.py"))
    transaction = importlib.util.module_from_spec(transaction_spec); transaction_spec.loader.exec_module(transaction)
    transaction.recover(root, activation)  # Recover compatibility snapshots from earlier releases.
    if (root/".store-recovery").exists(): administrator_transaction("recover",root,activation,args.profile)
    token = administrator_transaction("prepare",root,activation,args.profile).get("token") if args.transaction else None
    package_receipt = root/"packages.json"
    if package_receipt.is_symlink() or (package_receipt.exists() and package_receipt.stat().st_size > 65536):
        raise RuntimeError("Invalid Android profile package receipt.")
    packages = json.loads(package_receipt.read_text()) if package_receipt.exists() else {}
    if not isinstance(packages, dict): raise RuntimeError("Invalid Android profile package receipt.")
    env = dict(os.environ)
    for key, name in (("HOME", "home"), ("XDG_DATA_HOME", "data"), ("XDG_CONFIG_HOME", "config"), ("XDG_CACHE_HOME", "cache")):
        private_directory(root/name); env[key] = str(root/name)
    if not env.get("WAYLAND_DISPLAY"):
        sockets = sorted(runtime.glob("wayland-*"))
        sockets = [p for p in sockets if p.is_socket() and p.stat().st_uid == os.getuid()]
        if len(sockets) != 1: raise RuntimeError("Android needs an active Wayland desktop. Sign into the Wayland session and retry.")
        env["WAYLAND_DISPLAY"] = sockets[0].name
    data = Path(env["XDG_DATA_HOME"])/"waydroid/data"
    child = None
    def verify():
        check_stop()
        profile_root(args.profile, config_home, data_home)
        if child.poll() is not None or not owned(session(), child.pid, data):
            raise RuntimeError("Android session ownership changed. Return Home and retry.")
    try:
        emit("starting")
        child = subprocess.Popen(["/usr/bin/waydroid", "--details-to-stdout", "session", "start"], env=env,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.monotonic()+120
        while True:
            check_stop(); state = session()
            if state and not owned(state, child.pid, data): raise RuntimeError("Android refused this profile's private session.")
            if state.get("state") == "RUNNING": break
            if child.poll() is not None or time.monotonic() > deadline: raise RuntimeError("Android could not start. Return Home and retry.")
            time.sleep(.2)
        verify()
        apps = command(env, "app", "list", timeout=90)
        if args.operation == "uninstall":
            if "packageName: "+package in apps.splitlines():
                emit("removing"); verify(); command(env, "app", "remove", package)
                deadline = time.monotonic()+60
                while True:
                    verify()
                    if "packageName: "+package not in command(env, "app", "list", timeout=15).splitlines(): break
                    if time.monotonic()>deadline: raise RuntimeError("Android did not finish removing the app. Retry removal.")
                    time.sleep(1)
            packages.pop(args.app, None); save_packages(package_receipt, packages)
            emit("verified", operation="uninstall", packageAbsent=True)
            return
        if "packageName: "+package not in apps.splitlines() or packages.get(args.app) != app["sha256"]:
            emit("installing")
            verify()
            if args.store_version: store_package(args.app, args.store_version, args.profile, data_home)
            command(env, "app", "install", str(apk), timeout=120)
            deadline = time.monotonic()+60
            while True:
                verify()
                if ("packageName: "+package in command(env, "app", "list", timeout=15).splitlines()
                        and (not args.store_version or installed_artifact(data, app["sha256"], package))): break
                if time.monotonic() > deadline: raise RuntimeError("Android did not finish installing the app. Return Home and retry.")
                time.sleep(1)
            packages[args.app] = app["sha256"]
            save_packages(package_receipt, packages)
        if args.operation == "install":
            emit("verified", operation="install", packagePresent=True)
            return
        verify(); command(env, "app", "launch", package)
        emit("ready")
        while True:
            verify(); time.sleep(.5)
    except Stopped:
        emit("cancelled", operation=args.operation)
    finally:
        if child is not None and child.poll() is None:
            child.send_signal(signal.SIGTERM if owned(session(), child.pid, data) else signal.SIGUSR1)
            try: child.wait(timeout=10)
            except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=3)
        if child is not None and owned(session(), child.pid, data):
            raise RuntimeError("Android did not release this profile's session. Retry after closing Android.")
        if token:
            if not stopping and sys.exc_info()[0] is None:
                emit("prepared", operation=args.operation, token=token,
                     packagePresent=args.operation=="install", packageAbsent=args.operation=="uninstall")
                deadline = time.monotonic()+30
                while not stopping and time.monotonic()<deadline:
                    if select.select([sys.stdin], [], [], .1)[0]:
                        sys.stdin.readline()  # Activation token on disk decides commit vs rollback.
                        break
            committed = administrator_transaction("recover",root,activation,args.profile).get("committed",False)
            if not committed:
                emit("cancelled", operation=args.operation, rolledBack=True)
        else:
            committed = True
        if committed and not stopping and sys.exc_info()[0] is None:
            emit("completed", operation=args.operation,
                 packagePresent=args.operation=="install", packageAbsent=args.operation=="uninstall")


if __name__ == "__main__":
    def stop(*unused):
        global stopping
        stopping = True
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try: main()
    except Stopped:
        emit("cancelled")
    except Exception as error:
        emit("error", message=str(error) if isinstance(error, (RuntimeError, ValueError)) else "Android setup could not be read. Run the Isolde installer with Android support and retry.")
        sys.exit(1)
