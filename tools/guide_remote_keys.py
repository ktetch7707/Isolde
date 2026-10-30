"""Bounded D-pad events for the disposable VM acceptance window only."""
import atexit
import ctypes
import ctypes.util
import re
import time

_connection=None


def send(environment,window,name):
    global _connection
    if name not in {'Up','Down','Left','Right','Return'} or not re.fullmatch(r'0x[0-9a-f]+',window,re.I):raise ValueError('Invalid remote acceptance input')
    if _connection is None:
        library=ctypes.CDLL(ctypes.util.find_library('X11'))
        library.XOpenDisplay.argtypes=[ctypes.c_char_p];library.XOpenDisplay.restype=ctypes.c_void_p
        library.XStringToKeysym.argtypes=[ctypes.c_char_p];library.XStringToKeysym.restype=ctypes.c_ulong
        library.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];library.XKeysymToKeycode.restype=ctypes.c_uint
        class KeyEvent(ctypes.Structure):
            _fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send_event',ctypes.c_int),('display',ctypes.c_void_p),('window',ctypes.c_ulong),('root',ctypes.c_ulong),('subwindow',ctypes.c_ulong),('time',ctypes.c_ulong),('x',ctypes.c_int),('y',ctypes.c_int),('x_root',ctypes.c_int),('y_root',ctypes.c_int),('state',ctypes.c_uint),('keycode',ctypes.c_uint),('same_screen',ctypes.c_int)]
        class Event(ctypes.Union):_fields_=[('key',KeyEvent),('padding',ctypes.c_long*24)]
        library.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
        library.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int];library.XCloseDisplay.argtypes=[ctypes.c_void_p]
        display=library.XOpenDisplay(environment['DISPLAY'].encode())
        if not display:raise RuntimeError('No acceptance display')
        atexit.register(lambda:library.XCloseDisplay(display));_connection=(library,display,KeyEvent,Event)
    library,display,KeyEvent,Event=_connection
    code=library.XKeysymToKeycode(display,library.XStringToKeysym(name.encode()))
    for kind,mask in ((2,1),(3,2)):
        event=Event();event.key=KeyEvent(type=kind,send_event=1,display=display,window=int(window,16),keycode=code,same_screen=1)
        library.XSendEvent(display,int(window,16),1,mask,ctypes.byref(event))
    library.XSync(display,0);time.sleep(0.06)
