#!/usr/bin/env python3
"""Account-free external Chrome page boundary; real sandbox, pipe bridge and X keys.

Uses an owned private Xvfb display, never the logged-in desktop. The page reports
ordinary DOM observations over loopback; no debugger/evaluation interface exists.
"""
import ctypes
import ctypes.util
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid
import zlib


PAGE = r'''<!doctype html><meta charset="utf-8"><title>Isolde remote boundary</title>
<style>body{font:26px sans-serif;background:#f8f6fb;color:#332c44;margin:40px;height:2200px}
button,input{font:26px sans-serif;width:220px;height:60px;display:block;margin:20px}
#bottom{position:absolute;top:1800px}h1{font-size:30px}</style>
<h1>External Chrome · controlled account-free page</h1>
<button id="start">First control</button><input id="entry" aria-label="Text entry">
<button id="next" onclick="location.href='second'">Next page</button>
<button id="bottom" onclick="this.textContent='Activated'">Bottom control</button>
<script>let lastKey='';addEventListener('keydown',event=>{lastKey=event.key},true);
function fits(selector){const panel=document.querySelector(selector);if(!panel)return false;const r=panel.getBoundingClientRect();return [...panel.querySelectorAll('button')].every(b=>{const k=b.getBoundingClientRect();return k.left>=r.left&&k.right<=r.right&&k.top>=r.top&&k.bottom<=r.bottom})}
setInterval(()=>fetch('observe',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
 path:location.pathname,selected:document.activeElement.id,value:document.getElementById('entry').value,
 keyboard:!!document.querySelector('[data-isolde-remote-keyboard]'),menu:!!document.querySelector('[data-isolde-remote-menu]'),
 remoteHint:document.documentElement.innerText.includes('D-pad Move highlight'),focused:document.hasFocus(),lastKey,
 keyboardFits:fits('[data-isolde-remote-keyboard]'),menuFits:fits('[data-isolde-remote-menu]'),
 canGoBack:globalThis.navigation?navigation.canGoBack:null,historyLength:history.length,
 scroll:scrollY,bottom:document.getElementById('bottom').textContent})}).catch(()=>{}),80);
</script>'''


def main():
    smoke = Path(sys.argv[1]).resolve()
    output = Path(os.environ['ISOLDE_TEST_ARTIFACTS'])
    state = {}; observations = []; frames = []; actions = []
    token = uuid.uuid4().hex
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            if not self.path.startswith('/'+token+'/'):
                self.send_error(404); return
            data = PAGE.encode(); self.send_response(200)
            self.send_header('Content-Type','text/html; charset=utf-8')
            self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
        def do_POST(self):
            length=int(self.headers.get('Content-Length','0'))
            if self.path!='/'+token+'/observe' or not 0<length<2048:
                self.send_error(400);return
            document=json.loads(self.rfile.read(length))
            state.clear();state.update(document)
            self.send_response(204);self.end_headers()
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    x=ctypes.CDLL(ctypes.util.find_library('X11'))
    xt=ctypes.CDLL(ctypes.util.find_library('Xtst'))
    x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p
    x.XCloseDisplay.argtypes=[ctypes.c_void_p]
    x.XStringToKeysym.argtypes=[ctypes.c_char_p];x.XStringToKeysym.restype=ctypes.c_ulong
    x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];x.XKeysymToKeycode.restype=ctypes.c_uint
    x.XFlush.argtypes=[ctypes.c_void_p]
    x.XDefaultRootWindow.argtypes=[ctypes.c_void_p];x.XDefaultRootWindow.restype=ctypes.c_ulong
    x.XGetImage.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_uint,ctypes.c_uint,ctypes.c_ulong,ctypes.c_int];x.XGetImage.restype=ctypes.c_void_p
    x.XGetPixel.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int];x.XGetPixel.restype=ctypes.c_ulong
    x.XDestroyImage.argtypes=[ctypes.c_void_p]
    x.XQueryTree.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(ctypes.c_ulong),ctypes.POINTER(ctypes.c_ulong),ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),ctypes.POINTER(ctypes.c_uint)]
    x.XFetchName.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(ctypes.c_char_p)]
    x.XFree.argtypes=[ctypes.c_void_p]
    x.XSetInputFocus.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong]
    x.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
    class Attributes(ctypes.Structure):
        _fields_=[('x',ctypes.c_int),('y',ctypes.c_int),('width',ctypes.c_int),('height',ctypes.c_int),('border',ctypes.c_int),('depth',ctypes.c_int),('visual',ctypes.c_void_p),('root',ctypes.c_ulong),('class_',ctypes.c_int),('bit_gravity',ctypes.c_int),('win_gravity',ctypes.c_int),('backing_store',ctypes.c_int),('backing_planes',ctypes.c_ulong),('backing_pixel',ctypes.c_ulong),('save_under',ctypes.c_int),('colormap',ctypes.c_ulong),('map_installed',ctypes.c_int),('map_state',ctypes.c_int),('all_event_masks',ctypes.c_long),('your_event_mask',ctypes.c_long),('do_not_propagate_mask',ctypes.c_long),('override_redirect',ctypes.c_int),('screen',ctypes.c_void_p)]
    x.XGetWindowAttributes.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(Attributes)]
    xt.XTestFakeKeyEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
    display=None;child=None;xvfb=None
    with tempfile.TemporaryDirectory(prefix='isolde-external-remote-') as temporary:
        root=Path(temporary);env=dict(os.environ)
        # Xvfb chooses a free display and reports its identity through a private FD.
        read,write=os.pipe()
        try:
            xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1000x720x24','-nolisten','tcp'],pass_fds=(write,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            os.close(write);write=None
            import select
            if not select.select([read],[],[],5)[0]:raise RuntimeError('Owned Xvfb did not become ready')
            number=os.read(read,32).decode().strip()
            if not number.isdecimal():raise RuntimeError('Invalid private display')
            env.pop('WAYLAND_DISPLAY',None)
            env.update(DISPLAY=':'+number,XAUTHORITY='',XDG_SESSION_TYPE='x11',XDG_DATA_HOME=str(root/'data'),XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'))
            display=x.XOpenDisplay(env['DISPLAY'].encode())
            if not display:raise RuntimeError('Cannot open owned private display')
            profile=str(uuid.uuid4())
            log=(output/'external-web-remote.log').open('w')
            child=subprocess.Popen([str(smoke),'--remote-fixture',f'http://127.0.0.1:{server.server_port}/{token}/first',profile],env=env,stdin=subprocess.PIPE,stdout=log,stderr=log)
            def expect(predicate,label,timeout=8):
                deadline=time.monotonic()+timeout
                while time.monotonic()<deadline:
                    if child.poll() is not None:raise RuntimeError('Sandbox exited: '+label)
                    if predicate():observations.append({'check':label,'observed':dict(state)});return
                    time.sleep(.08)
                raise RuntimeError(label+': '+json.dumps(state))
            def key(*names):
                for name in names:
                    code=x.XKeysymToKeycode(display,x.XStringToKeysym(name.encode()))
                    if not code:raise RuntimeError('Missing physical key '+name)
                    if not xt.XTestFakeKeyEvent(display,code,1,0):raise RuntimeError('Physical key press failed: '+name)
                    x.XSync(display,0);time.sleep(.03)
                    if not xt.XTestFakeKeyEvent(display,code,0,0):raise RuntimeError('Physical key release failed: '+name)
                    x.XSync(display,0)
                    actions.append(name);time.sleep(.1)
            def capture(name):
                # Read only our Xvfb framebuffer. Encode RGB PNG without an image dependency.
                image=x.XGetImage(display,x.XDefaultRootWindow(display),0,0,1000,720,0xffffffff,2)
                if not image:raise RuntimeError('Owned framebuffer unavailable')
                try:
                    raw=bytearray()
                    for y in range(720):
                        raw.append(0)
                        for col in range(1000):
                            pixel=x.XGetPixel(image,col,y);raw.extend(((pixel>>16)&255,(pixel>>8)&255,pixel&255))
                    def chunk(kind,data):return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
                    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',1000,720,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
                    path=output/('studio-test-external-web-'+name+'.png');path.write_bytes(png)
                    frames.append({'phase':name,'sha256':hashlib.sha256(png).hexdigest()})
                finally:x.XDestroyImage(image)
            expect(lambda:state.get('path','').endswith('/first'),'actual external page loaded',15)
            window=None;names=[];deadline=time.monotonic()+5
            while not window and time.monotonic()<deadline:
                children=ctypes.POINTER(ctypes.c_ulong)();count=ctypes.c_uint();root_window=ctypes.c_ulong();parent=ctypes.c_ulong()
                x.XQueryTree(display,x.XDefaultRootWindow(display),ctypes.byref(root_window),ctypes.byref(parent),ctypes.byref(children),ctypes.byref(count))
                try:
                    names=[]
                    for i in range(min(count.value,128)):
                        attributes=Attributes()
                        if not x.XGetWindowAttributes(display,children[i],ctypes.byref(attributes)):continue
                        name=ctypes.c_char_p()
                        if x.XFetchName(display,children[i],ctypes.byref(name)) and name:
                            names.append({'name':name.value.decode(errors='replace'),'mapped':attributes.map_state,'width':attributes.width,'height':attributes.height})
                            x.XFree(name)
                        # Ignore Chrome's unmapped group-leader/clipboard helper.
                        # This private display contains only the owned browser.
                        if attributes.map_state==2 and attributes.width>=600 and attributes.height>=400:window=children[i]
                finally:
                    if children:x.XFree(children)
                if not window:time.sleep(.1)
            if not window:raise RuntimeError('Private Chrome window not found: '+json.dumps(names))
            x.XSetInputFocus(display,window,1,0);x.XFlush(display)
            expect(lambda:state.get('remoteHint') is True,'production remote bridge is visibly ready',15)
            # Chrome may finish mapping/activating its native window after discovery.
            # Establish focus after bridge readiness, then drain the X server request
            # before the first physical key; a page-ready report is not an X barrier.
            x.XSetInputFocus(display,window,1,0);x.XSync(display,0)
            expect(lambda:state.get('focused') is True,'owned browser document has input focus')
            expect(lambda:state.get('focused') is True,'owned Chrome page has keyboard focus')
            expect(lambda:state.get('selected')=='start','production bridge establishes initial semantic focus')
            # The mutation observer may already select the first control while
            # Chrome maps its window. Up establishes the same visible origin
            # whether focus is unset or already on that first control.
            key('Up');expect(lambda:state.get('selected')=='start','semantic first control')
            key('F6');expect(lambda:state.get('selected')=='entry','custom remote semantic text field')
            key('F7');expect(lambda:state.get('keyboard') is True and state.get('keyboardFits') is True,'custom remote keyboard opens without inherited page overflow');capture('keyboard')
            key('Return','Escape');expect(lambda:state.get('keyboard') is False and state.get('value')=='','Back cancels text without writing')
            key('Return','Return','Down','Down','Down','Down','Right','Right','Right','Right','Return')
            expect(lambda:state.get('keyboard') is False and state.get('value')=='1','remote Done applies text');capture('text')
            key('F9');expect(lambda:state.get('menu') is True and state.get('menuFits') is True,'custom page actions open without inherited page overflow');capture('menu')
            key('Down','Down','Return');expect(lambda:state.get('menu') is False and state.get('scroll',0)>200,'explicit page scroll');capture('scroll')
            key('F2','Escape');expect(lambda:state.get('menu') is False,'Back closes page actions')
            key('Down');expect(lambda:state.get('selected')=='next','semantic navigation control')
            key('Return');expect(lambda:state.get('path','').endswith('/second'),'Confirm navigates to second page')
            key('Escape');expect(lambda:state.get('path','').endswith('/first'),'Back restores browser history');capture('back')
            child.stdin.write(b'expect-return\n');child.stdin.flush()
            key('F8');child.wait(timeout=8)
            if child.returncode:raise RuntimeError('Owned sandbox did not exit cleanly')
            receipt={'passed':True,'boundary':'controlled-loopback-page','browser':'actual-external-chrome','productionSandbox':True,'privateDisplay':True,'physicalXKeys':actions,'observations':observations,'frames':frames,'ownedExitVerified':True,'rootBackExitVerified':True,'normalProfileUsed':False,'visualReview':'pending'}
            (output/'external-web-remote-result.json').write_text(json.dumps(receipt,indent=2))
            print(json.dumps(receipt))
        except Exception:
            (output/'external-web-remote-failure.json').write_text(json.dumps({'state':dict(state),'observations':observations,'keys':actions},indent=2))
            if display and 'capture' in locals():
                try:capture('failure')
                except Exception:pass
            raise
        finally:
            if write is not None:os.close(write)
            os.close(read)
            if child and child.poll() is None:
                child.stdin.write(b'quit\n');child.stdin.flush()
                try:child.wait(timeout=8)
                except subprocess.TimeoutExpired:child.kill();child.wait(timeout=3)
            if display:x.XCloseDisplay(display)
            if xvfb:xvfb.terminate();xvfb.wait(timeout=5)
            server.shutdown();server.server_close()


if __name__=='__main__':main()
