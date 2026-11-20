"""Present or minimize the unique Kodi X11/XWayland window."""

import ctypes
import subprocess
import sys
import time


def inventory():
    """Resolve Kodi's current XID because fullscreen can recreate its window."""
    result = subprocess.run(["wmctrl", "-lx"], capture_output=True, text=True, timeout=2, check=True)
    return [
        int(fields[0], 16)
        for line in result.stdout.splitlines()
        if len(fields := line.split()) >= 3 and fields[2].lower() == "kodi.kodi"
    ]


def wait_for_kodi(deadline):
    """Allow a bounded wait for Kodi to map its native window."""
    ids = inventory()
    while not ids and time.monotonic() < deadline:
        time.sleep(0.1)
        ids = inventory()
    return ids


def minimize(window_id):
    """Iconify the selected window so Kodi can keep its existing session."""
    x11 = ctypes.CDLL("libX11.so.6")
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XDefaultScreen.argtypes = [ctypes.c_void_p]
    x11.XIconifyWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int]
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(None)
    if not display:
        return 4
    try:
        if not x11.XIconifyWindow(display, window_id, x11.XDefaultScreen(display)):
            return 5
        x11.XSync(display, False)
    finally:
        x11.XCloseDisplay(display)
    return 0


def main():
    """Operate only when exactly one Kodi window owns the requested action."""
    if len(sys.argv) != 2 or sys.argv[1] not in ("show", "hide"):
        return 2
    show = sys.argv[1] == "show"
    deadline = time.monotonic() + 3
    ids = wait_for_kodi(deadline) if show else inventory()
    if len(ids) != 1:
        return 3
    if not show:
        return minimize(ids[0])

    window = hex(ids[0])
    subprocess.run(["wmctrl", "-i", "-a", window], timeout=2, check=True)
    subprocess.run(["wmctrl", "-i", "-r", window, "-b", "add,fullscreen"], timeout=2, check=True)
    time.sleep(0.2)
    ids = wait_for_kodi(deadline)
    if len(ids) != 1:
        return 3
    subprocess.run(["wmctrl", "-i", "-a", hex(ids[0])], timeout=2, check=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError):
        sys.exit(1)
