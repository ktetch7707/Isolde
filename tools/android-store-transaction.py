"""Bounded, profile-local Android data rollback for Store operations.

The Android session must be stopped before prepare/recover/finish. Activation's
atomic transaction token is the crash-recovery authority; stdin is only a wakeup.
"""
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import time
import uuid

LIMIT = 8*1024**3
FILES = 100000


def atomic(path, value):
    temporary = path.with_name(path.name+".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream); stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary, path)
    sync(path.parent)


def sync(path):
    if hasattr(os, "O_DIRECTORY"):
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
        try: os.fsync(fd)
        finally: os.close(fd)


def directory(root, storage=None):
    if storage is not None:
        # Only the root-owned setup entrypoint passes held /proc/self/fd anchors.
        if os.geteuid()!=0 or not re.fullmatch(r"/proc/self/fd/[0-9]+", str(root)):
            raise ValueError("Invalid privileged Android transaction anchor")
        if storage.resolve()!=storage or storage.is_symlink(): raise ValueError("Android backup storage is redirected")
        return storage/"transaction"
    if root.resolve() != root or root.is_symlink():
        raise ValueError("Android transaction storage is redirected")
    target = root/".store-transaction"
    if target.is_symlink():
        raise ValueError("Android transaction backup is redirected")
    return target


def recover(root, activation, storage=None):
    target = directory(root, storage)
    if not target.exists(): return
    journal = target/"transaction.json"
    if not journal.exists():
        shutil.rmtree(target)  # Incomplete backup, before any runtime mutation.
        return
    if journal.is_symlink() or journal.stat().st_size > 4096:
        raise ValueError("Invalid Android transaction journal")
    value = json.loads(journal.read_text())
    token = value.get("token", "")
    if not re.fullmatch(r"[a-f0-9]{32}", token) or type(value.get("hadData")) is not bool:
        raise ValueError("Invalid Android transaction identity")
    if value.get("rollbackComplete") is True:
        if storage is not None: atomic(storage/"outcome.json",{"token":token,"committed":False})
        shutil.rmtree(target); sync(root); return False
    committed = False
    if activation.exists():
        if activation.is_symlink() or activation.stat().st_size > 1024**2:
            raise ValueError("Invalid app activation during Android recovery")
        committed = json.loads(activation.read_text()).get("storeTransaction") == token
    if not committed:
        # Keep the changed tree until the snapshot has been put back successfully.
        data, backup, discard = root/"data", target/"data", target/"discard"
        if data.is_symlink() or backup.is_symlink() or discard.is_symlink():
            raise ValueError("Android recovery data is redirected")
        if value["hadData"] and not backup.is_dir():
            # An interrupted previous recovery already renamed the backup.
            if not discard.exists() or not data.is_dir():
                raise ValueError("Android rollback backup is missing; preserve profile storage and retry recovery")
        else:
            if data.exists() and not discard.exists(): os.replace(data, discard)
            if value["hadData"]: os.replace(backup, data)
            sync(root)
        receipt = root/"packages.json"
        previous = target/"packages.json"
        if previous.is_symlink() or receipt.is_symlink(): raise ValueError("Android receipt is redirected")
        if previous.exists():
            atomic(receipt, json.loads(previous.read_text()))
            if storage is not None: os.chown(receipt, root.stat().st_uid, root.stat().st_gid)
        else: receipt.unlink(missing_ok=True)
        value["rollbackComplete"] = True
        atomic(journal, value)
    if storage is not None: atomic(storage/"outcome.json",{"token":token,"committed":committed})
    shutil.rmtree(target)
    sync(root)
    return committed


def prepare(root, activation, check_stop, runner=subprocess.Popen, storage=None):
    recover(root, activation, storage)
    target = directory(root, storage)
    data = root/"data"
    if data.is_symlink(): raise ValueError("Android profile data is redirected")
    total = count = 0
    if data.exists():
        for parent, directories, names in os.walk(data, followlinks=False):
            check_stop()
            for name in directories+names:
                info = (Path(parent)/name).lstat(); count += 1
                if stat.S_ISREG(info.st_mode): total += info.st_size
                elif not (stat.S_ISDIR(info.st_mode) or stat.S_ISLNK(info.st_mode)):
                    raise ValueError("Android data contains an unsupported live file; close Android and retry")
                if total > LIMIT or count > FILES:
                    raise ValueError("Android rollback backup exceeds 8 GiB or 100,000 files")
    if shutil.disk_usage(root).free < total+64*1024**2:
        raise ValueError("Not enough profile storage for a safe Android rollback backup")
    target.mkdir(mode=0o700)
    try:
        if data.exists():
            options={"stdout":subprocess.DEVNULL,"stderr":subprocess.DEVNULL}
            if storage is not None: options["pass_fds"]=(int(root.name),)
            child = runner(["/usr/bin/cp", "--archive", "--reflink=auto", "--", str(data), str(target/"data")], **options)
            deadline = time.monotonic()+180
            try:
                while child.poll() is None:
                    check_stop()
                    if time.monotonic() > deadline: raise ValueError("Android rollback backup timed out")
                    time.sleep(.1)
                if child.returncode: raise ValueError("Android data could not be backed up safely; no app was changed")
            finally:
                if child.poll() is None:
                    child.terminate()
                    try: child.wait(timeout=3)
                    except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=3)
            # Publish the journal only after the rollback bytes and directory
            # entries are durable, including on filesystems without reflinks.
            for parent, directories, names in os.walk(target/"data", topdown=False, followlinks=False):
                if not hasattr(os,"O_DIRECTORY"): break  # Portable host tests; appliance execution is Linux-only.
                check_stop()
                for name in names:
                    path=Path(parent)/name
                    if path.is_symlink(): continue
                    fd=os.open(path,os.O_RDONLY|getattr(os,"O_NOFOLLOW",0))
                    try: os.fsync(fd)
                    finally: os.close(fd)
                sync(Path(parent))
        receipt = root/"packages.json"
        if receipt.exists():
            if receipt.is_symlink() or receipt.stat().st_size > 65536: raise ValueError("Invalid Android package receipt")
            atomic(target/"packages.json", json.loads(receipt.read_text()))
        token = uuid.uuid4().hex
        atomic(target/"transaction.json", {"token": token, "hadData": data.exists()})
        return token
    except BaseException:
        if not (target/"transaction.json").exists(): shutil.rmtree(target)
        raise
