#!/usr/bin/env python3
"""Disposable real Games-widget -> native SuperTux -> Home acceptance.
Runtime activation is fixture setup; game registration and launch use remote UI.
"""
import argparse
import ctypes
import ctypes.util
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import time
import uuid


def main():
    parser=argparse.ArgumentParser();parser.add_argument('executable',type=Path);parser.add_argument('--emulator',action='store_true');args=parser.parse_args()
    if socket.gethostname()!='isolde-test':raise RuntimeError('Guest-only Games acceptance')
    lock=open('/tmp/isolde-home-components-acceptance.lock','a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    evidence=Path(tempfile.mkdtemp(prefix='isolde-components-acceptance-games-'));profile=str(uuid.uuid4())
    source=Path(__file__).resolve().parents[1];config=evidence/'config/Isolde';config.mkdir(parents=True)
    local=config/'profiles'/profile;local.mkdir(parents=True)
    (config/'profiles.json').write_text(json.dumps({'active':profile,'profiles':[{'id':profile,'name':'Games acceptance'}]}))
    (config/'device-setup.json').write_text(json.dumps({'version':1,'step':7,'complete':True}))
    (config/'Kodi.conf').write_text('[profiles]\n'+profile+'\\kodi\\autoStart=false\n')
    unknown=evidence/'config/Unknown Organization';unknown.mkdir()
    (unknown/'Isolde.conf').write_text('[sources]\nautomaticSourcesEnabled=false\n')
    (local/'home-layout.json').write_text(json.dumps({'schemaVersion':2,'theme':{},'composition':{'schemaVersion':1,'enabled':True,'fit':False,'columns':1,'margin':28,'gap':16,'headerMode':'identity','identity':'Games acceptance','headerControls':['settings','edit'],'pageNavigation':'none'},'categories':[{'id':'games','name':'Games','presentation':{'variant':'library','height':600,'libraryKind':'games','libraryView':'all','libraryColumns':3,'libraryLayout':'grid'}}],'components':[]}))
    if not args.emulator:
        package=evidence/'data/Isolde/packages/profiles'/profile/'supertux/1.0.0';package.mkdir(parents=True)
        shutil.copyfile(source/'config/packages/supertux/manifest.json',package/'manifest.json')
        (local/'installed-apps.json').write_text(json.dumps({'schemaVersion':1,'installed':['supertux'],'packageStates':{'supertux':'installed'},'failureReasons':{}}))
    env=dict(os.environ)
    desktop=subprocess.run(['systemctl','--user','show-environment'],check=True,capture_output=True,text=True,timeout=15).stdout
    for line in desktop.splitlines():
        key,_,value=line.partition('=')
        if key in {'DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS','WAYLAND_DISPLAY'}:env[key]=value
    env.update(XDG_CONFIG_HOME=str(evidence/'config'),XDG_DATA_HOME=str(evidence/'data'),XDG_CACHE_HOME=str(evidence/'cache'),QT_QPA_PLATFORM='xcb')
    # Managed launcher is verified fixture setup, not a custom shell command.
    applications=evidence/'data/applications';applications.mkdir(parents=True)
    (applications/'isolde-supertux.desktop').write_text('[Desktop Entry]\nType=Application\nName=SuperTux\nExec=/usr/games/supertux2 --fullscreen --renderer sdl\nTerminal=false\n')
    os.environ.update({k:env[k] for k in ('DISPLAY','XAUTHORITY') if k in env})
    x=ctypes.CDLL(ctypes.util.find_library('X11'))
    x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p
    x.XStringToKeysym.argtypes=[ctypes.c_char_p];x.XStringToKeysym.restype=ctypes.c_ulong
    x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];x.XKeysymToKeycode.restype=ctypes.c_uint
    x.XFlush.argtypes=[ctypes.c_void_p];x.XCloseDisplay.argtypes=[ctypes.c_void_p]
    class Key(ctypes.Structure):
        _fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send_event',ctypes.c_int),('display',ctypes.c_void_p),('window',ctypes.c_ulong),('root',ctypes.c_ulong),('subwindow',ctypes.c_ulong),('time',ctypes.c_ulong),('x',ctypes.c_int),('y',ctypes.c_int),('x_root',ctypes.c_int),('y_root',ctypes.c_int),('state',ctypes.c_uint),('keycode',ctypes.c_uint),('same_screen',ctypes.c_int)]
    class Event(ctypes.Union):_fields_=[('key',Key),('padding',ctypes.c_long*24)]
    x.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
    display=x.XOpenDisplay(env.get('DISPLAY','').encode())
    if not display:raise RuntimeError('Missing guest display')
    process=None;window=None;phases=[];database=local/'games.sqlite'
    def expect(predicate,message,seconds=40):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError('Isolde exited; inspect shell.log')
            if predicate():return
            time.sleep(.2)
        raise RuntimeError(message)
    def keys(*names):
        for name in names:
            code=x.XKeysymToKeycode(display,x.XStringToKeysym(name.encode()))
            for kind,mask in ((2,1),(3,2)):
                event=Event();event.key=Key(type=kind,send_event=1,display=display,window=int(window,16),keycode=code,same_screen=1)
                x.XSendEvent(display,int(window,16),1,mask,ctypes.byref(event))
            x.XFlush(display);time.sleep(.10)
        time.sleep(.3)
    def capture(phase):
        phases.append(phase);print(phase.upper()+'_READY '+str(evidence),flush=True)
        expect(lambda:(evidence/(phase+'-captured')).exists(),'Capture timed out: '+phase,130)
    def count():
        if not database.exists():return 0
        with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as db:return db.execute('SELECT count(*) FROM games').fetchone()[0]
    def type_text(text):
        # Exercise the actual OSK through D-pad/Confirm only. No text injection,
        # keyboard shortcuts, direct QML invocation or catalog seeding.
        lower=list('1234567890qwertyuiopasdfghjkl-')+['Shift']+list('zxcvbnm_')+['Del','Space','.','@','#','!','?',',',':','/','Clear','Done']
        upper=lower[:49]+['{','}','\\','Clear','Done']
        upper=[value.upper() if len(value)==1 and value.isalpha() else value for value in upper]
        current=0
        def choose(index):
            nonlocal current
            sequence=[]
            while index-current>=10:sequence.append('Down');current+=10
            while current-index>=10:sequence.append('Up');current-=10
            sequence += ['Right']*max(0,index-current)+['Left']*max(0,current-index)
            current=index;keys(*(sequence+['Return']))
        choose(lower.index('Clear'))
        for character in text:
            if character.isupper():choose(lower.index('Shift'));choose(upper.index(character))
            else:choose(lower.index('Space' if character==' ' else character))
        choose(lower.index('Done'))
    def emulator_journey():
        game_root=Path.home()/'Games/Isolde-Acceptance/webfest-1.2/webfest'
        if not (game_root/'EBOOT.PBP').is_file():raise RuntimeError('Provision the pinned Webfest fixture through Configure games first')
        def game_hashes():
            return {str(path.relative_to(game_root)):hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in game_root.rglob('*') if path.is_file()}
        before=game_hashes()
        layout_before=(local/'home-layout.json').read_bytes()
        capture('emulatorsempty');keys('Return',*(['Right']*8),'Return');capture('emulatorssetup')
        keys(*(['Down']*7+['Return']));capture('emulatorsinstallreview');keys('Right','Return')
        expected={'ppsspp','dolphin','pcsx2','retroarch-nes','retroarch-gb','retroarch-gba','retroarch-snes'}
        def activated():
            try:return expected.issubset(json.loads((local/'installed-apps.json').read_text()).get('installed',[]))
            except (OSError,ValueError):return False
        expect(activated,'Remote emulator installation/activation did not finish',90)
        if (local/'home-layout.json').read_bytes()!=layout_before:raise RuntimeError('Runtime setup changed the Games layout and remote origin')
        capture('emulatorsready')
        keys('Return',*(['Left']*8),'Return','Return');capture('emulatorsadd')
        type_text('webfest');type_text('psp');type_text(str(game_root));type_text('EBOOT.PBP');capture('emulatorsreview')
        if count()!=0:raise RuntimeError('Emulated game registration saved before review confirmation')
        keys('Return');expect(lambda:count()==1,'Emulated game registration did not commit');keys('Return');capture('emulatorsdetails')
        keys('Return');capture('emulatorspermission')
        expect(lambda:'Isolated native session launching "ppsspp"' in (evidence/'shell.log').read_text(),'PSP session did not launch',100)
        def game_window():
            return any('ppsspp' in line.lower() for line in subprocess.run(['wmctrl','-lx'],env=env,capture_output=True,text=True,timeout=5).stdout.splitlines())
        expect(game_window,'PPSSPP did not create its native game window');time.sleep(5);capture('emulatorsmenu')
        time.sleep(2);capture('emulatorsplay');time.sleep(2);capture('emulatorsaction')
        expect(lambda:not game_window(),'PSP session did not close after Home');capture('emulatorsreturn');keys('Return');capture('emulatorsrestored')
        if game_hashes()!=before:raise RuntimeError('The emulated game source changed')
        result={'passed':True,'freshProfile':True,'remoteRuntimeSetup':True,'remoteRegistration':True,'nativeGameStarted':True,'returned':True,
            'runtime':'ppsspp','fixture':'Webfest 1.2','sourceUnchanged':True,'gameplayVerified':False,'physicalControllerVerified':False,'captures':phases,'visualReview':'pending'}
        (evidence/'result.json').write_text(json.dumps(result,indent=2))
    try:
        subprocess.run(['systemctl','--user','stop','isolde.service'],check=True,timeout=30)
        with (evidence/'shell.log').open('w') as log:
            process=subprocess.Popen([str(args.executable.resolve())],env=env,stdout=log,stderr=log)
            def find_window():
                nonlocal window
                for line in subprocess.run(['wmctrl','-lp'],env=env,capture_output=True,text=True,timeout=5).stdout.splitlines():
                    parts=line.split(None,4)
                    if len(parts)>=3 and parts[2]==str(process.pid):window=parts[0];return True
                return False
            expect(find_window,'No Isolde window');subprocess.run(['wmctrl','-ia',window],env=env,check=True,timeout=10);time.sleep(2)
            if args.emulator:emulator_journey();return
            capture('gamesempty');keys('Return','Return','Return');capture('gamesadd')
            keys('Return');keys(*(['Down']*5+['Right']*9+['Return']));keys(*(['Down']*5+['Right']*9+['Return']))
            capture('gamesreview')
            if count()!=0:raise RuntimeError('Registration wrote before confirmation')
            keys('Return');expect(lambda:count()==1,'Game registration did not commit')
            keys('Return');capture('gamesdetails');keys('Return');capture('gamespermission')
            expect(lambda:'Isolated native session launching "supertux"' in (evidence/'shell.log').read_text(),'Game never launched after return permission',100)
            def game_window():
                return any('supertux2' in line.lower() for line in subprocess.run(['wmctrl','-lx'],env=env,capture_output=True,text=True,timeout=5).stdout.splitlines())
            expect(game_window,'SuperTux did not create its native window')
            time.sleep(5)
            if not game_window():raise RuntimeError('SuperTux exited before its first game frame')
            capture('gamesmenu');time.sleep(3)
            if not game_window():raise RuntimeError('SuperTux exited before gameplay review')
            capture('gamesplay')
            expect(lambda:'Native app session returned' in (evidence/'shell.log').read_text() or not any('supertux2' in line for line in subprocess.run(['wmctrl','-lx'],env=env,capture_output=True,text=True,timeout=5).stdout.splitlines()),'Game did not close after Home')
            capture('gamesreturn');keys('Return');capture('gamesrestored')
            result={'passed':True,'freshProfile':True,'remoteRegistration':True,'nativeGameStarted':True,'returned':True,'gameplayVerified':False,'physicalControllerVerified':False,'captures':phases,'visualReview':'pending'}
            (evidence/'result.json').write_text(json.dumps(result,indent=2))
    except Exception:
        if process and process.poll() is None:
            try:capture('gamesfailure')
            except Exception:pass
        raise
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=25)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        x.XCloseDisplay(display);print('EVIDENCE '+str(evidence),flush=True)


if __name__=='__main__':main()
