#!/usr/bin/env python3
"""OnFailure handler. UI is separate from the main application's data/runtime."""
import argparse
import fcntl
import importlib.util
from pathlib import Path
import subprocess
import sys

spec = importlib.util.spec_from_file_location("installer", Path(__file__).with_name("install-appliance.py"))
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


def systemctl(*args):
    return subprocess.run(["systemctl", "--user", *args], check=True, text=True, capture_output=True).stdout.strip()


def previous_available(prefix):
    try:
        previous = installer.checked_release(prefix, "previous")
        return previous is not None and previous != installer.checked_release(prefix, "current")
    except (RuntimeError, OSError):
        return False


def recover(prefix, choice, control=systemctl):
    if choice not in (10, 11):
        raise RuntimeError("Unknown recovery action")
    # Never change a release underneath an app started elsewhere while UI was open.
    if control("show", "isolde.service", "--property=ActiveState", "--value") not in ("failed", "inactive"):
        raise RuntimeError("Isolde is already starting or running")
    with (prefix / ".install.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if choice == 11:
            if not previous_available(prefix):
                raise RuntimeError("No previous release available")
            installer.rollback(prefix)
        control("reset-failed", "isolde.service")
        control("start", "isolde.service")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--ui", required=True, type=Path)
    args = parser.parse_args()
    # OnFailure can be queued during another successful start. Do not cover it.
    if systemctl("show", "isolde.service", "--property=ActiveState", "--value") != "failed":
        return
    failed = False
    while True:
        command = [str(args.ui)]
        if previous_available(args.prefix):
            command.append("--previous")
        if failed:
            command.append("--error")
        choice = subprocess.run(command).returncode
        if choice not in (10, 11):
            raise RuntimeError("Recovery interface exited unexpectedly; inspect the recovery service journal")
        try:
            recover(args.prefix, choice)
            return
        except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
            print(f"Recovery action failed: {error}", file=sys.stderr, flush=True)
            # Another successful launch should remove recovery rather than cover it.
            if systemctl("show", "isolde.service", "--property=ActiveState", "--value") == "active":
                return
            failed = True


if __name__ == "__main__":
    main()
