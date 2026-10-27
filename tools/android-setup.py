#!/usr/bin/python3
"""Explicit in-Store Android setup, using only an installed root-owned entrypoint.

The unprivileged UI packages the existing bounded provisioner just as VPN setup
packages its support helper. Only dpkg and the installed entrypoint elevate;
user-writable Python is never run as root. Cancel waits for each system step.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager

SUPPORT = Path("/usr/lib/isolde-android-setup")
FILES = ("android-setup.py", "android-store-transaction.py", "provision-android.py", "provision-default-apps.py", "config/android-runtime.json")
cancelled = False


def emit(event, **fields):
    print(json.dumps({"event": event, **fields}), flush=True)


def cancel(*unused):
    global cancelled
    cancelled = True


def check_cancel(job):
    global cancelled
    if os.geteuid() != 0 and select.select([sys.stdin], [], [], 0)[0]:
        if sys.stdin.readline().strip() == "cancel": cancelled = True
    if cancelled and os.geteuid() != 0: (job/"cancel").touch()
    if cancelled or (job/"cancel").exists():
        raise RuntimeError("Android setup cancelled at a safe checkpoint. Retry setup to continue.")


def trusted_support(root=SUPPORT):
    if root.resolve() != root: raise ValueError("Android setup support is redirected")
    for path in (root, *root.parents, *(root/name for name in FILES), root/"config"):
        info = path.stat()
        if path.is_symlink() or info.st_uid != 0 or info.st_mode & 0o022:
            raise ValueError("Android administrator support must be root-owned and protected")


def platform():
    if sys.platform != "linux": raise RuntimeError("Android setup is available only on the Linux appliance")
    values = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    if values.get("ID", "").strip('"') != "ubuntu" or values.get("VERSION_ID", "").strip('"') != "26.04":
        raise RuntimeError("This Android setup requires Ubuntu 26.04")


def run_step(arguments, job, timeout):
    check_cancel(job)
    # Do not terminate dpkg/apt/initialization halfway through a system write.
    # Cancellation is delivered by the marker and honored at the next checkpoint.
    with tempfile.TemporaryFile() as output:
        child = subprocess.Popen(arguments, stdout=output, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        deadline = time.monotonic()+timeout
        try:
            while child.poll() is None:
                if os.geteuid() != 0:
                    try: check_cancel(job)
                    except RuntimeError: pass
                if time.monotonic()>deadline:
                    raise RuntimeError("Android setup step timed out. Wait for package management to finish before retrying.")
                if output.tell()>8*1024**2:
                    raise RuntimeError("Android setup returned too much diagnostic output")
                time.sleep(.1)
            if child.returncode:
                raise RuntimeError("Administrator approval was cancelled or Android setup could not complete. Retry setup.")
        finally:
            # A system step is never killed by the remote Cancel action.
            # Its fixed command has its own bounded provisioner timeout.
            if child.poll() is None:
                child.wait(timeout=max(1, timeout))
    check_cancel(job)


def install_support(source, job):
    payload = {}
    for name in FILES:
        path = source/name
        if not path.is_file() or path.is_symlink() or path.stat().st_size>2*1024**2:
            raise ValueError("Android setup payload is missing or unsafe")
        payload[name] = path.read_bytes()
    try:
        trusted_support()
        if all((SUPPORT/name).read_bytes()==data for name,data in payload.items()): return
    except (OSError, ValueError): pass
    emit("status", message="Administrator approval: install the Android setup support package.")
    with tempfile.TemporaryDirectory(prefix="isolde-android-support-") as temporary:
        root = Path(temporary); staging = root/"payload"
        for name, data in payload.items():
            path = staging/SUPPORT.relative_to("/")/name
            path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data); path.chmod(0o755 if name=="android-setup.py" else 0o644)
        metadata = staging/"DEBIAN"; metadata.mkdir()
        version = "1.0+"+hashlib.sha256(b"".join(payload.values())).hexdigest()[:16]
        (metadata/"control").write_text("Package: isolde-android-support\nVersion: "+version+
            "\nArchitecture: all\nMaintainer: Isolde\nDescription: Isolde Android setup support\n")
        for path in (staging, *(p for p in staging.rglob("*") if p.is_dir())): path.chmod(0o755)
        package = root/"isolde-android-support.deb"
        run_step(["/usr/bin/dpkg-deb", "--root-owner-group", "--build", str(staging), str(package)], job, 60)
        run_step(["/usr/bin/pkexec", "/usr/bin/dpkg", "-i", str(package)], job, 180)
    trusted_support()


@contextmanager
def android_setup_lock(runtime,caller):
    import fcntl
    path=runtime/"isolde-android.lock"
    try:
        fd=os.open(path,os.O_CREAT|os.O_EXCL|os.O_RDWR|os.O_NOFOLLOW,0o600)
        os.fchown(fd,caller,runtime.stat().st_gid)
    except FileExistsError:
        fd=os.open(path,os.O_RDWR|os.O_NOFOLLOW)
    try:
        if os.fstat(fd).st_uid!=caller: raise ValueError("Invalid Android session lock owner")
        try: fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: raise RuntimeError("Close the active Android session before setup; it will not be interrupted")
        yield
    finally: os.close(fd)


def root_setup(job, software):
    trusted_support()
    caller = os.environ.get("PKEXEC_UID", "")
    if not caller.isdigit() or int(caller)==0:
        raise ValueError("Use the Store's Android setup confirmation")
    runtime = Path("/run/user")/caller
    if job.parent != runtime or not re.fullmatch(r"isolde-android-setup-[a-f0-9]{32}", job.name):
        raise ValueError("Invalid Android setup job")
    if job.resolve()!=job or job.stat().st_uid!=int(caller) or job.stat().st_mode & 0o077:
        raise ValueError("Android setup job must be private to its requester")
    with android_setup_lock(runtime,int(caller)):
        run_root_setup(job,software)


def run_root_setup(job,software):
    spec = importlib.util.spec_from_file_location("android_setup_provisioner", SUPPORT/"provision-android.py")
    provision = importlib.util.module_from_spec(spec); spec.loader.exec_module(provision)
    original_run = provision.shared.run
    original_download = provision.shared.download
    def checked_run(*arguments, **kwargs):
        check_cancel(job)
        emit("status", message="Android setup: "+str(arguments[0])+". Cancel waits for this system step.")
        # Keep apt/dpkg's human output out of the JSON status channel.
        if kwargs.get("capture_output"):
            result = original_run(*arguments, **kwargs)
        else:
            with tempfile.TemporaryFile() as output:
                kwargs.setdefault("stdout", output); kwargs.setdefault("stderr", subprocess.STDOUT)
                result = original_run(*arguments, **kwargs)
                if output.tell()>8*1024**2: raise RuntimeError("Android setup diagnostic output exceeded its limit")
        check_cancel(job); return result
    def checked_download(*arguments, **kwargs):
        check_cancel(job); emit("status", message="Downloading verified Android setup files...")
        result = original_download(*arguments, **kwargs)
        check_cancel(job); return result
    provision.shared.run = checked_run; provision.shared.download = checked_download
    check_cancel(job)
    provision.require_idle_runtime(Path("/var/lib/waydroid/waydroid.cfg").is_file())
    provision.system(SUPPORT, software)
    check_cancel(job)
    emit("completed", ready=True)


def owned_directory_anchor(path, caller):
    """Hold every path component without following symlinks, then validate owner."""
    if not path.is_absolute() or ".." in path.parts: raise ValueError("Invalid Android profile path")
    fd=os.open("/",os.O_RDONLY|os.O_DIRECTORY)
    try:
        for name in path.parts[1:]:
            next_fd=os.open(name,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=fd)
            os.close(fd);fd=next_fd
        info=os.fstat(fd)
        if info.st_uid!=caller or info.st_mode & 0o077: raise ValueError("Android transaction directory is not private to its caller")
        return fd
    except BaseException:
        os.close(fd);raise


def root_transaction(args):
    import fcntl
    trusted_support()
    caller=os.environ.get("PKEXEC_UID","")
    if not caller.isdigit() or int(caller)==0: raise ValueError("Use the Store to request this Android transaction")
    profile=args.profile or ""
    if profile!="default" and not re.fullmatch(r"[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}",profile): raise ValueError("Invalid Android profile identity")
    root,activation=args.profile_root,args.activation
    if (root is None or activation is None or root.name!=profile or root.parent.name!="android-profiles"
        or root.parent.parent.name!="Isolde" or activation.name!="installed-apps.json"
        or activation.parent.name!=profile or activation.parent.parent.name!="profiles"
        or activation.parent.parent.parent.name!="Isolde"): raise ValueError("Unexpected Android transaction paths")
    root_fd=owned_directory_anchor(root,int(caller));activation_fd=None;lock_fd=None
    try:
        activation_fd=owned_directory_anchor(activation.parent,int(caller))
        anchor=Path("/proc/self/fd")/str(root_fd)
        state=Path("/proc/self/fd")/str(activation_fd)/"installed-apps.json"
        storage=Path("/var/lib/isolde/android-transactions")/caller/hashlib.sha256(str(root).encode()).hexdigest()
        # No backup path is user-controlled or writable by an unprivileged caller.
        for directory in reversed((storage,*storage.parents)):
            if directory.exists():
                info=directory.stat()
                if directory.is_symlink() or info.st_uid!=0 or info.st_mode & 0o022: raise ValueError("Unsafe Android backup storage")
            else: directory.mkdir(mode=0o700)
        if storage.stat().st_dev!=os.fstat(root_fd).st_dev:
            raise ValueError("Safe Android rollback requires Android data and appliance backup storage on the same filesystem")
        lock_fd=os.open(storage/"lock",os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
        try: fcntl.flock(lock_fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: raise RuntimeError("Android backup/recovery is already running; wait and retry")
        provision_spec=importlib.util.spec_from_file_location("transaction_provisioner",SUPPORT/"provision-android.py")
        provision=importlib.util.module_from_spec(provision_spec);provision_spec.loader.exec_module(provision)
        provision.require_idle_runtime(True)
        spec=importlib.util.spec_from_file_location("root_android_transaction",SUPPORT/"android-store-transaction.py")
        transaction=importlib.util.module_from_spec(spec);spec.loader.exec_module(transaction)
        def checkpoint():
            if (anchor/".store-cancel").exists(): raise RuntimeError("Android transaction cancelled before mutation")
        if args.transaction_action=="prepare":
            token=transaction.prepare(anchor,state,checkpoint,storage=storage)
            marker=anchor/".store-recovery"
            fd=os.open(marker,os.O_CREAT|os.O_WRONLY|os.O_TRUNC|os.O_NOFOLLOW,0o600)
            try:
                os.write(fd,token.encode());os.fsync(fd);os.fchown(fd,int(caller),os.fstat(root_fd).st_gid)
            finally: os.close(fd)
            transaction.sync(anchor)
            emit("transaction",token=token)
        else:
            if not (storage/"transaction").exists() and (anchor/".store-recovery").exists():
                marker=anchor/".store-recovery"
                if marker.is_symlink() or marker.stat().st_size!=32: raise ValueError("Invalid Android recovery marker")
                outcome_path=storage/"outcome.json"
                if not outcome_path.is_file() or outcome_path.stat().st_size>4096: raise ValueError("Android rollback evidence is missing; preserve storage and retry recovery")
                outcome=json.loads(outcome_path.read_text())
                if outcome.get("token")!=marker.read_text() or type(outcome.get("committed")) is not bool:
                    raise ValueError("Android recovery outcome does not match this operation")
                committed=outcome["committed"]
            else:
                committed=transaction.recover(anchor,state,storage=storage)
            (anchor/".store-recovery").unlink(missing_ok=True);transaction.sync(anchor)
            emit("transaction",committed=bool(committed))
    finally:
        os.close(root_fd)
        if activation_fd is not None: os.close(activation_fd)
        if lock_fd is not None: os.close(lock_fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--software-rendering", action="store_true")
    parser.add_argument("--root-job", type=Path)
    parser.add_argument("--transaction-action",choices=("prepare","recover"))
    parser.add_argument("--profile")
    parser.add_argument("--profile-root",type=Path)
    parser.add_argument("--activation",type=Path)
    args = parser.parse_args()
    platform()
    if os.geteuid()==0:
        if args.transaction_action:
            if args.root_job or args.software_rendering: raise ValueError("Ambiguous Android administrator operation")
            root_transaction(args);return
        if not args.root_job: raise ValueError("Missing Store setup job")
        root_setup(args.root_job, args.software_rendering); return
    if args.root_job or args.transaction_action or args.profile_root or args.activation or args.profile:
        raise ValueError("Administrator job is not a user operation")
    runtime = Path(os.environ["XDG_RUNTIME_DIR"])
    if runtime.resolve()!=runtime or runtime.stat().st_uid!=os.getuid(): raise ValueError("Invalid desktop runtime directory")
    import uuid
    job = runtime/("isolde-android-setup-"+uuid.uuid4().hex); job.mkdir(mode=0o700)
    try:
        install_support(Path(__file__).resolve().parent, job)
        emit("status", message="Administrator approval: download and configure Android. Existing profiles are preserved.")
        arguments = ["/usr/bin/pkexec", str(SUPPORT/"android-setup.py"), "--root-job", str(job)]
        if args.software_rendering: arguments.append("--software-rendering")
        # Forward only the root helper's bounded JSON events, discarding apt output.
        child = subprocess.Popen(arguments, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
        deadline = time.monotonic()+3000
        completed = False
        try:
            while child.poll() is None:
                try: check_cancel(job)
                except RuntimeError: pass
                if time.monotonic()>deadline: raise RuntimeError("Android setup timed out; retry after the current system step finishes")
                if select.select([child.stdout], [], [], .1)[0]:
                    line = child.stdout.readline(65537)
                    if len(line)>65536: raise ValueError("Oversized Android setup status")
                    event = json.loads(line) if line.strip() else {}
                    if event.get("event")=="completed": completed=event.get("ready") is True
                    if event: print(json.dumps(event), flush=True)
            for line in child.stdout:
                if len(line)>65536: raise ValueError("Oversized Android setup status")
                event=json.loads(line)
                if event.get("event")=="completed": completed=event.get("ready") is True
                print(json.dumps(event),flush=True)
            if child.returncode or not completed: raise RuntimeError("Android setup did not finish. Review administrator approval and retry.")
        finally:
            if child.poll() is None:
                (job/"cancel").touch()  # Root owns system-step cleanup; never kill a package manager.
                child.wait(timeout=1900)
    finally:
        shutil.rmtree(job)


if __name__=="__main__":
    signal.signal(signal.SIGTERM,cancel); signal.signal(signal.SIGINT,cancel)
    try: main()
    except Exception as error:
        emit("error",message=str(error)[:512]); raise SystemExit(1)
