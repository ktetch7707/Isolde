"""Send bounded D-pad events to the disposable Guide acceptance window."""

import atexit
import ctypes
import ctypes.util
import re
import time


class KeyEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("serial", ctypes.c_ulong),
        ("send_event", ctypes.c_int),
        ("display", ctypes.c_void_p),
        ("window", ctypes.c_ulong),
        ("root", ctypes.c_ulong),
        ("subwindow", ctypes.c_ulong),
        ("time", ctypes.c_ulong),
        ("x", ctypes.c_int),
        ("y", ctypes.c_int),
        ("x_root", ctypes.c_int),
        ("y_root", ctypes.c_int),
        ("state", ctypes.c_uint),
        ("keycode", ctypes.c_uint),
        ("same_screen", ctypes.c_int),
    ]


class Event(ctypes.Union):
    _fields_ = [("key", KeyEvent), ("padding", ctypes.c_long * 24)]


_connection = None


def open_display(name):
    """Open the fixture display once with the X11 argument layouts set explicitly."""
    library = ctypes.CDLL(ctypes.util.find_library("X11"))
    library.XOpenDisplay.argtypes = [ctypes.c_char_p]
    library.XOpenDisplay.restype = ctypes.c_void_p
    library.XStringToKeysym.argtypes = [ctypes.c_char_p]
    library.XStringToKeysym.restype = ctypes.c_ulong
    library.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    library.XKeysymToKeycode.restype = ctypes.c_uint
    library.XSendEvent.argtypes = [
        ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.POINTER(Event)
    ]
    library.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    library.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = library.XOpenDisplay(name.encode())
    if not display:
        raise RuntimeError("No acceptance display")
    atexit.register(library.XCloseDisplay, display)
    return library, display


def send(environment, window, name):
    """Send a complete press and release to the named window, avoiding global input."""
    global _connection
    if name not in {"Up", "Down", "Left", "Right", "Return"} or not re.fullmatch(r"0x[0-9a-f]+", window, re.I):
        raise ValueError("Invalid remote acceptance input")
    if _connection is None:
        _connection = open_display(environment["DISPLAY"])

    library, display = _connection
    window_id = int(window, 16)
    code = library.XKeysymToKeycode(display, library.XStringToKeysym(name.encode()))
    for kind, mask in ((2, 1), (3, 2)):
        event = Event()
        event.key = KeyEvent(
            type=kind, send_event=1, display=display, window=window_id, keycode=code, same_screen=1,
        )
        library.XSendEvent(display, window_id, 1, mask, ctypes.byref(event))
        library.XSync(display, 0)
        time.sleep(0.06)
