#!/usr/bin/env python3
"""Wait for the graphical session before replacing this process with Isolde."""
import argparse
import os
from pathlib import Path
import shlex
import stat
import subprocess
import sys
import time

DISPLAY_KEYS = {'DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'XDG_SESSION_TYPE', 'XDG_CURRENT_DESKTOP', 'XDG_RUNTIME_DIR'}


def import_display(environment, text):
    result = dict(environment)
    for line in text.splitlines():
        try:
            fields = shlex.split(line)
        except ValueError:
            continue
        if len(fields) != 1 or '=' not in fields[0]:
            continue
        key, value = fields[0].split('=', 1)
        if key in DISPLAY_KEYS:
            result[key] = value
    return result


def socket_exists(path):
    try:
        return stat.S_ISSOCK(Path(path).stat().st_mode)
    except OSError:
        return False


def display_ready(environment):
    wayland = environment.get('WAYLAND_DISPLAY')
    if wayland:
        path = Path(wayland)
        if not path.is_absolute():
            runtime = environment.get('XDG_RUNTIME_DIR')
            if not runtime:
                return False
            path = Path(runtime) / path
        if socket_exists(path):
            return True
    display = environment.get('DISPLAY', '')
    if display.startswith(':'):
        number = display[1:].split('.')[0]
        return number.isdigit() and socket_exists('/tmp/.X11-unix/X' + number)
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    args = parser.parse_args()
    environment = dict(os.environ)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        try:
            result = subprocess.run(['systemctl', '--user', 'show-environment'], capture_output=True, text=True, timeout=2)
            if result.returncode == 0:
                environment = import_display(environment, result.stdout)
        except (OSError, subprocess.TimeoutExpired):
            pass
        if display_ready(environment):
            os.execve(args.executable, [args.executable], environment)
        time.sleep(.25)
    sys.exit('Isolde startup: graphical display did not become ready within 30 seconds.')


if __name__ == '__main__':
    main()
