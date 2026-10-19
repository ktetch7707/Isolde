"""Bounded X11/XWayland presentation helper; refuses ambiguous Kodi windows."""
import ctypes
import subprocess
import sys

def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("show", "hide"):
        return 2
    result = subprocess.run(["wmctrl", "-lx"], capture_output=True, text=True, timeout=2, check=True)
    ids = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[2].lower() == "kodi.kodi":
            ids.append(int(fields[0], 16))
    if len(ids) != 1:
        return 3
    if sys.argv[1] == "show":
        for args in (["-i", "-r", hex(ids[0]), "-b", "add,fullscreen"], ["-i", "-a", hex(ids[0])]):
            subprocess.run(["wmctrl", *args], timeout=2, check=True)
    else:
        x11 = ctypes.CDLL("libX11.so.6")
        x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x11.XOpenDisplay.restype = ctypes.c_void_p
        x11.XDefaultScreen.argtypes = [ctypes.c_void_p]
        x11.XIconifyWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int]
        x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
        x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        display = x11.XOpenDisplay(None)
        if not display: return 4
        try:
            if not x11.XIconifyWindow(display, ids[0], x11.XDefaultScreen(display)): return 5
            x11.XSync(display, False)
        finally:
            x11.XCloseDisplay(display)
    return 0

if __name__ == "__main__":
    try: sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError): sys.exit(1)
