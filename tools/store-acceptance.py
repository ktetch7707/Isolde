#!/usr/bin/env python3
"""Real Store lifecycle for Chromium and Android in a fresh guest profile."""
import argparse
import ctypes
import ctypes.util
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import uuid
import pwd
import re


class AndroidAuthorization:
    """Explicitly approved, expiring single-profile Isolde-Test fixture."""
    def __init__(self, evidence, profile):
        self.evidence=Path(evidence)
        if (socket.gethostname()!='isolde-test' or self.evidence.parent!=Path('/tmp')
            or not re.fullmatch(r'isolde-components-acceptance-store-[A-Za-z0-9_-]+',self.evidence.name)
            or self.evidence.resolve()!=self.evidence or self.evidence.stat().st_uid!=os.getuid()
            or self.evidence.stat().st_mode & 0o077 or str(uuid.UUID(profile))!=profile):
            raise ValueError('Invalid temporary Store authorization identity')
        self.profile=profile
        self.rule=Path('/etc/polkit-1/rules.d')/('00-isolde-store-'+profile+'.rules')
        self.folder=self.evidence/'authorization';self.folder.mkdir(mode=0o700,exist_ok=True)
        self.receipt=Path('/var/lib/isolde/store-fixtures')/(profile+'.json')
        self.root=Path('/var/tmp')/self.evidence.name/'data/Isolde/android-profiles'/profile
        self.activation=self.evidence/'config/Isolde/profiles'/profile/'installed-apps.json'

    def package(self, enable):
        stage=self.folder/('enable' if enable else 'revoke');stage.mkdir(mode=0o700,exist_ok=True)
        metadata=stage/'DEBIAN';metadata.mkdir(exist_ok=True)
        (metadata/'control').write_text('Package: isolde-store-test-fixture\nVersion: 1.0\nArchitecture: all\nMaintainer: Isolde VM tests\nDescription: Temporary explicitly approved single-profile test authorization\n')
        config=dict(enable=enable,evidence=str(self.evidence),profile=self.profile,uid=os.getuid(),user=pwd.getpwuid(os.getuid()).pw_name,
                    root=str(self.root),activation=str(self.activation),rule=str(self.rule),receipt=str(self.receipt),subjectPid=os.getpid(),subjectStart=Path('/proc/self/stat').read_text().split()[21])
        (metadata/'postinst').write_text('#!/usr/bin/python3\nC='+repr(config)+'\n'+AUTHORIZATION_ROOT)
        (metadata/'postinst').chmod(0o755)
        package=self.folder/('enable.deb' if enable else 'revoke.deb')
        subprocess.run(['/usr/bin/dpkg-deb','--root-owner-group','--build',str(stage),str(package)],check=True,capture_output=True,timeout=30)
        return package

    def enable(self):
        self.root.parent.mkdir(parents=True,exist_ok=True,mode=0o700);self.root.parent.chmod(0o700)
        for directory in (self.root,self.activation.parent):directory.mkdir(parents=True,exist_ok=True,mode=0o700);directory.chmod(0o700)
        self.package(False)  # Prepare cleanup before enabling anything.
        (self.evidence/'authorization.json').write_text(json.dumps(dict(profile=self.profile,rule=str(self.rule),dataRoot=str(self.root))))
        subprocess.run(['sudo','-n','/usr/bin/dpkg','-i',str(self.package(True))],check=True,timeout=60)
        receipt=json.loads(self.receipt.read_text())
        assert receipt['enabled'] is True and receipt['expires']-receipt['created']==900
        assert receipt['scopeChecks']==[True,True,True,True]
        (self.evidence/'authorization-enabled.json').write_text(json.dumps(receipt,indent=2))
        print('AUTHORIZATION_ENABLED '+json.dumps(receipt),flush=True);time.sleep(2)

    def revoke(self):
        package=self.folder/'revoke.deb'
        if not package.exists():return
        subprocess.run(['sudo','-n','/usr/bin/dpkg','-i',str(package)],check=True,timeout=60)
        receipt=json.loads(self.receipt.read_text())
        assert receipt['enabled'] is False and receipt['cleanupVerified'] is True,'Temporary authorization cleanup was not verified'
        (self.evidence/'authorization-cleanup.json').write_text(json.dumps(receipt,indent=2))
        print('AUTHORIZATION_REMOVED '+json.dumps(receipt),flush=True)


AUTHORIZATION_ROOT=r'''
import json,os,pwd,re,socket,stat,subprocess,time
from pathlib import Path
if os.geteuid()!=0 or socket.gethostname()!='isolde-test':raise RuntimeError('Isolde VM root fixture only')
profile=C['profile'];uid=C['uid'];base=Path(C['evidence'])
if not re.fullmatch(r'[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}',profile) or uid<=0 or pwd.getpwuid(uid).pw_name!=C['user']:raise ValueError('Invalid fixture caller')
if base.parent!=Path('/tmp') or not re.fullmatch(r'isolde-components-acceptance-store-[A-Za-z0-9_-]+',base.name):raise ValueError('Invalid fixture root')
if base.resolve()!=base or base.stat().st_uid!=uid or base.stat().st_mode & 0o077:raise ValueError('Fixture root is not private')
root=Path('/var/tmp')/base.name/'data/Isolde/android-profiles'/profile;activation=base/'config/Isolde/profiles'/profile/'installed-apps.json'
rule=Path('/etc/polkit-1/rules.d')/('00-isolde-store-'+profile+'.rules')
receipt=Path('/var/lib/isolde/store-fixtures')/(profile+'.json')
if [str(root),str(activation),str(rule),str(receipt)]!=[C['root'],C['activation'],C['rule'],C['receipt']]:raise ValueError('Fixture paths differ')
for p in (rule.parent,*rule.parent.parents):
    s=p.stat()
    if p.is_symlink() or s.st_uid!=0 or s.st_mode & 0o022:raise ValueError('Unsafe policy directory')
unit='isolde-store-fixture-'+profile
if C['enable']:
    helper=Path('/usr/lib/isolde-android-setup/android-setup.py')
    for p in (helper,*helper.parents):
        s=p.stat()
        if p.is_symlink() or s.st_uid!=0 or s.st_mode & 0o022:raise ValueError('Unsafe production helper')
    for directory in (root,activation.parent):
        if directory.resolve()!=directory or directory.stat().st_uid!=uid or directory.stat().st_mode & 0o077:raise ValueError('Unsafe fixture profile')
    sessions=json.loads(subprocess.check_output(['/usr/bin/loginctl','list-sessions','--json=short'],text=True,timeout=10))
    active=None
    for session in sessions:
        name=str(session['session'])
        if not re.fullmatch(r'[A-Za-z0-9_-]+',name):continue
        fields=dict(line.split('=',1) for line in subprocess.check_output(['/usr/bin/loginctl','show-session',name,'-p','User','-p','Remote','-p','Active'],text=True,timeout=10).splitlines())
        if fields=={'User':str(uid),'Remote':'no','Active':'yes'}:active=name;break
    if active is None:raise RuntimeError('Fixture user has no active local VM session')
    now=int(time.time());expires=now+900
    commands=[str(helper)+' --transaction-action '+action+' --profile '+profile+' --profile-root '+str(root)+' --activation '+str(activation) for action in ('prepare','recover')]
    # Values contain no whitespace; ambiguous argv grouping is still refused by
    # the production parser, which accepts no positional/unknown arguments.
    js='polkit.addRule(function(action,subject){\n'
    js+='if(action.id!=="org.freedesktop.policykit.exec" || action.lookup("program")!=='+json.dumps(str(helper))+' || action.lookup("user")!=="root" || subject.user!=='+json.dumps(C['user'])+' || Date.now()>='+str(expires*1000)+') return;\n'
    js+='var command=action.lookup("command_line"); if('+json.dumps(commands)+'.indexOf(command)<0) return;\n'
    js+='var fields=polkit.spawn(["/usr/bin/loginctl","show-session",'+json.dumps(active)+',"-p","User","-p","Remote","-p","Active"]);\n'
    js+='if(fields.indexOf("User='+str(uid)+'\\n")>=0 && fields.indexOf("Remote=no\\n")>=0 && fields.indexOf("Active=yes\\n")>=0) return polkit.Result.YES;\n});\n'
    # Independent monotonic root timer precedes rule publication; the rule's
    # wall-clock deadline is an additional limit, never the sole cleanup.
    subprocess.run(['/usr/bin/systemd-run','--unit='+unit,'--on-active=900s','/usr/bin/rm','-f','--',str(rule)],check=True,timeout=15)
    fd=os.open(rule,os.O_CREAT|os.O_EXCL|os.O_WRONLY|os.O_NOFOLLOW,0o644)
    with os.fdopen(fd,'w') as stream:stream.write(js);stream.flush();os.fsync(stream.fileno())
    # Only trusted callers can send action details to CheckAuthorization. Probe
    # as root, but bind the subject to the exact live unprivileged test process.
    pid=C['subjectPid'];proc=Path('/proc')/str(pid)
    if proc.stat().st_uid!=uid or proc.joinpath('stat').read_text().split()[21]!=C['subjectStart']:raise ValueError('Fixture subject changed')
    identity=str(pid)+','+C['subjectStart']+','+str(uid);checks=[];time.sleep(2)
    probes=[(command,True) for command in commands]+[(commands[0].replace(profile,'00000000-0000-0000-0000-000000000000'),False),(commands[0].replace('--transaction-action prepare','--root-job'),False)]
    for command,allowed in probes:
        checked=subprocess.run(['/usr/bin/pkcheck','--action-id','org.freedesktop.policykit.exec','--process',identity,'--detail','program',str(helper),'--detail','user','root','--detail','command_line',command],capture_output=True,text=True,timeout=10)
        checks.append((checked.returncode==0)==allowed)
        if not checks[-1]:raise RuntimeError('Fixture scope probe failed: '+str(checked.returncode)+' '+checked.stderr[-1024:])
    value=dict(enabled=True,created=now,expires=expires,profile=profile,uid=uid,activeLocalSession=active,rule=str(rule),commands=commands,timer=unit+'.timer',scopeChecks=checks)
else:
    rule.unlink(missing_ok=True)
    subprocess.run(['/usr/bin/systemctl','stop',unit+'.timer',unit+'.service'],check=False,timeout=15,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    value=dict(enabled=False,removed=int(time.time()),profile=profile,uid=uid,rule=str(rule),cleanupVerified=not rule.exists())
    special=[];seen=0
    for parent,directories,names in os.walk(root/'data',followlinks=False):
        for name in directories+names:
            seen+=1
            if seen>100000:raise ValueError('Fixture diagnostic exceeds file bound')
            p=Path(parent)/name;mode=p.lstat().st_mode
            if not (stat.S_ISREG(mode) or stat.S_ISDIR(mode) or stat.S_ISLNK(mode)) and len(special)<32:
                special.append(dict(path=str(p.relative_to(root/'data')),type=stat.S_IFMT(mode)))
    value['specialDataInodes']=special
receipt.parent.mkdir(mode=0o755,parents=True,exist_ok=True)
for p in (receipt.parent,*receipt.parent.parents):
    s=p.stat()
    if p.is_symlink() or s.st_uid!=0 or s.st_mode & 0o022:raise ValueError('Unsafe fixture receipt storage')
fd=os.open(receipt,os.O_CREAT|os.O_TRUNC|os.O_WRONLY|os.O_NOFOLLOW,0o644)
with os.fdopen(fd,'w') as stream:json.dump(value,stream);stream.flush();os.fsync(stream.fileno())
'''


def main():
    parser=argparse.ArgumentParser();parser.add_argument("executable",type=Path);parser.add_argument("--discovery-only",action="store_true");parser.add_argument("--web-only",action="store_true");parser.add_argument("--offline-matrix",action="store_true");parser.add_argument('--native-auth',action='store_true');parser.add_argument('--test-authorization',action='store_true');parser.add_argument('--revoke-authorization',type=Path);args=parser.parse_args()
    if socket.gethostname()!="isolde-test":raise RuntimeError("Guest-only Store acceptance")
    if args.revoke_authorization:
        record=args.revoke_authorization/'authorization.json'
        if record.is_file():AndroidAuthorization(args.revoke_authorization,json.loads(record.read_text())['profile']).revoke()
        return
    lock=open("/tmp/isolde-home-components-acceptance.lock","a");fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    evidence=Path(tempfile.mkdtemp(prefix="isolde-components-acceptance-store-"));profile=str(uuid.uuid4())
    config=evidence/"config/Isolde";config.mkdir(parents=True)
    (config/"profiles.json").write_text(json.dumps({"active":profile,"profiles":[{"id":profile,"name":"Store acceptance"}]}))
    (config/"device-setup.json").write_text(json.dumps({"version":1,"step":7,"complete":True}))
    (config/"Kodi.conf").write_text("[profiles]\n"+profile+"\\kodi\\autoStart=false\n")
    (config/"Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    settings=evidence/"config/Unknown Organization";settings.mkdir()
    (settings/"Isolde.conf").write_text("[sources]\nautomaticSourcesEnabled=false\n")
    env=dict(os.environ)
    desktop=subprocess.run(["systemctl","--user","show-environment"],check=True,capture_output=True,text=True,timeout=15).stdout
    for line in desktop.splitlines():
        key,_,value=line.partition("=")
        if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS","WAYLAND_DISPLAY"}:env[key]=value
    data_home=(Path('/var/tmp')/evidence.name/'data') if args.test_authorization else evidence/'data'
    if args.test_authorization:
        data_home.parent.mkdir(mode=0o700);data_home.mkdir(mode=0o700)
    env.update(XDG_CONFIG_HOME=str(evidence/"config"),XDG_DATA_HOME=str(data_home),XDG_CACHE_HOME=str(evidence/"cache"),QT_QPA_PLATFORM="xcb")
    os.environ.update({k:env[k] for k in ("DISPLAY","XAUTHORITY") if k in env})
    x=ctypes.CDLL(ctypes.util.find_library("X11"))
    x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p
    x.XStringToKeysym.argtypes=[ctypes.c_char_p];x.XStringToKeysym.restype=ctypes.c_ulong
    x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];x.XKeysymToKeycode.restype=ctypes.c_uint
    x.XFlush.argtypes=[ctypes.c_void_p];x.XCloseDisplay.argtypes=[ctypes.c_void_p]
    class Key(ctypes.Structure):
        _fields_=[("type",ctypes.c_int),("serial",ctypes.c_ulong),("send_event",ctypes.c_int),("display",ctypes.c_void_p),("window",ctypes.c_ulong),("root",ctypes.c_ulong),("subwindow",ctypes.c_ulong),("time",ctypes.c_ulong),("x",ctypes.c_int),("y",ctypes.c_int),("x_root",ctypes.c_int),("y_root",ctypes.c_int),("state",ctypes.c_uint),("keycode",ctypes.c_uint),("same_screen",ctypes.c_int)]
    class Event(ctypes.Union):_fields_=[("key",Key),("padding",ctypes.c_long*24)]
    x.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
    display=x.XOpenDisplay(env.get("DISPLAY","").encode())
    if not display:raise RuntimeError("Missing guest X11 display")
    process=None;window=None;application_pid=None;phases=[];actions=[];state=config/"profiles"/profile/"installed-apps.json";authorization=None
    profile_a=profile;profile_b=str(uuid.uuid4());network_proofs=[]
    offline=None
    if args.offline_matrix:
        spec=importlib.util.spec_from_file_location('store_offline',Path(__file__).with_name('guide-soak-acceptance.py'))
        offline=importlib.util.module_from_spec(spec);spec.loader.exec_module(offline)
    def launch(log,active):
        nonlocal process,window,profile,state,application_pid
        if process and process.poll() is None:
            process.terminate();process.wait(timeout=25)
        profile=active;state=config/"profiles"/profile/"installed-apps.json"
        (config/"Kodi.conf").write_text("[profiles]\n"+"".join(p+"\\kodi\\autoStart=false\n" for p in (profile_a,profile_b)))
        state.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
        (config/"profiles.json").write_text(json.dumps({"active":active,"profiles":[{"id":profile_a,"name":"Store A"},{"id":profile_b,"name":"Store B"}]}))
        command=[str(args.executable.resolve()),"--launch-app=app-store"]
        if offline:
            proof=evidence/('offline-'+str(len(network_proofs)));proof.mkdir()
            probe=offline.SOCKET_PROBE.replace("'--windowed'","'--launch-app=app-store'")
            command=[sys.executable,'-c',probe,str(proof),str(args.executable.resolve())]
        window_pid=None
        if args.native_auth:
            # A real controlling terminal lets pkexec's normal authentication
            # agent ask the user directly. Its /dev/tty input is never logged.
            launcher=evidence/'native-launch.py';pid_file=evidence/'native-app.pid'
            launcher.write_text("import os,signal,subprocess,sys\nfrom pathlib import Path\n"
                "with open(sys.argv[2],'a') as log:\n"
                " child=subprocess.Popen(sys.argv[3:],stdout=log,stderr=log)\n"
                " Path(sys.argv[1]).write_text(str(child.pid))\n"
                " signal.signal(signal.SIGTERM,lambda *unused:child.terminate())\n"
                " raise SystemExit(child.wait())\n")
            process=subprocess.Popen(['ptyxis','--standalone','--title','Isolde Store administrator approval','--',sys.executable,str(launcher),str(pid_file),str(evidence/'shell.log')]+command,env=env,stdout=log,stderr=log)
            deadline=time.monotonic()+20
            while not pid_file.exists() and process.poll() is None and time.monotonic()<deadline:time.sleep(.2)
            if not pid_file.exists():raise RuntimeError('Native Store terminal did not launch; inspect shell.log')
            window_pid=int(pid_file.read_text())
        else:
            process=subprocess.Popen(command,env=env,stdout=log,stderr=log,preexec_fn=offline.install_socket_filter if offline else None)
        application_pid=window_pid or process.pid
        window=None;deadline=time.monotonic()+25
        while not window and time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError('Isolde exited before opening its Store window')
            discovered=subprocess.run(["wmctrl","-lp"],env=env,capture_output=True,text=True,timeout=5)
            for line in discovered.stdout.splitlines():
                parts=line.split(None,4)
                if len(parts)>=3 and parts[2]==str(window_pid or process.pid):window=parts[0];break
            time.sleep(.2)
        if not window:raise RuntimeError('Isolde created no test window; inspect shell.log')
        subprocess.run(["wmctrl","-ia",window],env=env,check=True,timeout=10);time.sleep(3)
        if offline:
            network=json.loads((proof/'network-denial.json').read_text())
            assert network['socketErrnos']==[101,101] and network['unixSocketAvailable'] is True
            assert network['pid']==process.pid
            network_proofs.append(network)
    def installed(app):return state.is_file() and app in json.loads(state.read_text()).get("installed",[])
    def expect(predicate,message,timeout=150,android=False):
        started=time.monotonic()
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError("Isolde exited; inspect shell.log")
            if predicate():return
            if android and time.monotonic()-started>5:
                children=Path('/proc')/str(application_pid)/'task'/str(application_pid)/'children'
                running=False
                for pid in children.read_text().split()[:64]:
                    try:running|=any(arg.endswith(b'/android-session.py') for arg in (Path('/proc')/pid/'cmdline').read_bytes().split(b'\0'))
                    except FileNotFoundError:pass
                if not running:raise RuntimeError(message+': Android helper already exited; inspect captured failure')
            time.sleep(.3)
        raise RuntimeError(message)
    def keys(*names):
        for name in names:
            if len(actions)>=5000:raise RuntimeError("Store input exceeded its bound")
            actions.append(name)
            code=x.XKeysymToKeycode(display,x.XStringToKeysym(name.encode()))
            if not code:raise RuntimeError("Missing keycode")
            for kind,mask in ((2,1),(3,2)):
                event=Event();event.key=Key(type=kind,send_event=1,display=display,window=int(window,16),keycode=code,same_screen=1)
                x.XSendEvent(display,int(window,16),1,mask,ctypes.byref(event))
            x.XFlush(display);time.sleep(.25)
        time.sleep(.5)
    def capture(phase):
        phases.append(phase);print(phase.upper()+"_READY "+str(evidence),flush=True)
        expect(lambda:(evidence/(phase+"-captured")).exists(),"Capture timed out: "+phase,100)
        time.sleep(2)
    print("Fixture evidence: "+str(evidence),flush=True)
    try:
        subprocess.run(["systemctl","--user","stop","isolde.service"],check=True,timeout=30)
        with (evidence/"shell.log").open("w") as log:
            launch(log,profile)
            if args.offline_matrix:
                def manifest(active):return evidence/'data/Isolde/packages/profiles'/active/'plex/1.0.0/manifest.json'
                def activation(active):return config/'profiles'/active/'installed-apps.json'
                def value(path):return path.read_bytes() if path.is_file() else None
                def plex():keys('Left','Up','Up','Right','Return','Down','Down')
                capture('offlineentry');plex()
                before=value(state)
                keys('Return');capture('offlinecancel');keys('Escape')
                assert value(state)==before and not manifest(profile).exists(),'Cancel wrote a Web app'
                state.parent.chmod(0o500)
                try:
                    keys('Return','Right','Return');capture('offlinewritefailed')
                    assert value(state)==before and not installed('plex') and not manifest(profile).exists(),'Failed activation left an installed package'
                finally:state.parent.chmod(0o700)
                keys('Return','Right','Return');expect(lambda:installed('plex'),'Offline retry did not install Plex')
                capture('offlineainstalled');saved_a=value(state);manifest_a=value(manifest(profile))
                assert manifest_a
                launch(log,profile_b);plex();capture('offlinebfresh')
                assert not installed('plex') and value(activation(profile_a))==saved_a and value(manifest(profile_a))==manifest_a
                keys('Return','Right','Return');expect(lambda:installed('plex'),'Second profile install failed');capture('offlinebinstalled')
                keys('Down','Return','Right','Return');expect(lambda:not installed('plex'),'Second profile removal failed');capture('offlinebremoved')
                saved_b=value(state)
                assert not manifest(profile_b).exists() and value(activation(profile_a))==saved_a and value(manifest(profile_a))==manifest_a
                launch(log,profile_a);plex();capture('offlineacold')
                assert installed('plex') and value(state)==saved_a and value(manifest(profile_a))==manifest_a
                keys('Return');time.sleep(10);capture('offlinewebopen');capture('offlinewebreturn')
                keys('Down','Return');capture('offlinearemove');keys('Return')
                assert installed('plex') and value(state)==saved_a,'Default Cancel removed the Web app'
                keys('Return','Right','Return');expect(lambda:not installed('plex'),'Offline Web removal failed');capture('offlineremoved')
                assert not manifest(profile_a).exists() and value(activation(profile_b))==saved_b
                keys('Left','Up','Up','Left','Return','Down','Return');time.sleep(3);capture('offlinerepositoryerror')
                assert not any((evidence/'cache/Isolde/profiles').glob('*/fdroid/index-v2.json')),'Disconnected repository unexpectedly obtained an index'
                keys('Menu','Return');capture('offlineaccessoff')
                keys('Escape');keys(*(['Up']*30));keys('Return');capture('offlinesettings')
                result=dict(passed=True,journey='store-offline',freshProfile=True,normalProfileUsed=False,profileIsolation=True,coldReopen=True,cancelNoWrite=True,writeFailureRollback=True,retryVerified=True,webLifecycle=True,repositoryFailureRecovered=True,manifestsRemoved=True,androidLifecycle=False,networkProofs=network_proofs,actualInputActions=len(actions),captures=phases,visualReview='pending')
                (evidence/'result.json').write_text(json.dumps(result,indent=2));return
            if args.discovery_only:
                from guide_features import remote_text
                def key(name,count=1):keys(*([name]*count))
                cache=evidence/"cache/Isolde/profiles"/profile/"fdroid"
                index=cache/"index-v2.json";archive=cache/"entry.jar"
                capture("storediscoveryentry")
                assert not index.exists(),"Opening Store unexpectedly contacted its repository"
                keys("Up","Return");capture("storediscoveryloading")
                expect(lambda:index.is_file() and archive.is_file(),"Live signed repository was not loaded",180)
                def provider_idle():
                    children=Path(f"/proc/{process.pid}/task/{process.pid}/children").read_text().split()
                    for child in children:
                        try:command=Path(f"/proc/{child}/cmdline").read_bytes()
                        except FileNotFoundError:continue
                        if b"fdroid-store.py" in command:return False
                    return True
                expect(provider_idle,"Live catalog request did not settle",60)
                capture("storediscoverybrowse")
                keys("Up","Up","Left","Left","Return");capture("storediscoverykeyboard")
                remote_text(key,"newpipe");expect(provider_idle,"Live title search did not settle",60)
                capture("storediscoveryresults")
                spec=importlib.util.spec_from_file_location("verified_store",Path(__file__).with_name("fdroid-store.py"))
                repository=importlib.util.module_from_spec(spec);spec.loader.exec_module(repository)
                document=repository.verified_index(repository.verified_entry(archive.read_bytes()),index.read_bytes())
                matches=repository.browse(document,"newpipe",0,repository.runtime_target())["results"]
                assert matches and any("newpipe" in item["name"].casefold() for item in matches),"Signed live search has no NewPipe result"
                assert not state.exists(),"Browsing installed an app without confirmation"
                keys("Escape");capture("storediscoveryreturn")
                key("Up",30);capture("storediscoverysettingsfocus")
                keys("Return");capture("storediscoverysettings")
                result=dict(passed=True,journey="store-discovery",freshProfile=True,normalProfileUsed=False,catalogOnly=True,installedAppsChanged=False,provider="fdroid-signed-v2",query="newpipe",matches=[item["packageName"] for item in matches],indexSha256=hashlib.sha256(index.read_bytes()).hexdigest(),entrySha256=hashlib.sha256(archive.read_bytes()).hexdigest(),actualInputActions=len(actions),captures=phases,visualReview="pending")
                (evidence/"result.json").write_text(json.dumps(result,indent=2));return
            if not args.web_only:
                if args.test_authorization:
                    authorization=AndroidAuthorization(evidence,profile);authorization.enable()
                capture("storeandroid")
                before=state.read_bytes() if state.exists() else None
                keys("Return");capture("storeandroidconsent");keys('Escape')
                assert (state.read_bytes() if state.exists() else None)==before,'Android confirmation Cancel changed activation'
                keys('Return',"Right","Return")
                if args.native_auth:
                    time.sleep(3)
                    capture('storeandroidauthentication')
                expect(lambda:installed("smarttube"),"Store did not install the Android app",600 if args.native_auth else 180,android=True)
                subprocess.run(['wmctrl','-ia',window],env=env,check=True,timeout=10)
                capture("storeandroidinstalled")
                keys("Return")
                expect(lambda:'Android app session ready "smarttube"' in (evidence/"shell.log").read_text(),"Store did not launch Android")
                time.sleep(8);capture("storeandroidopen") # Host runner sends only the VM's remote Home key.
                capture("storeandroidclosed")
                before=state.read_bytes()
                keys("Down","Return");capture("storeandroidremove");keys('Return')
                assert installed('smarttube') and state.read_bytes()==before,'Default Cancel removed the Android app'
                keys('Return',"Right","Return")
                expect(lambda:not installed("smarttube"),"Store did not verify Android removal",android=True)
                packages=data_home/"Isolde/android-profiles"/profile/"packages.json"
                if "smarttube" in json.loads(packages.read_text()):raise RuntimeError("Android receipt remains after removal")
                capture("storeandroidremoved")
            keys("Left","Up","Up","Right","Return","Down","Down")
            capture("storechromium")
            keys("Return");capture("storechromiumconsent");keys("Right","Return")
            expect(lambda:installed("plex"),"Store did not install Chromium app")
            capture("storechromiuminstalled");keys("Return");time.sleep(10)
            capture("storechromiumopen")
            capture("storechromiumclosed")
            keys("Down","Return");capture("storechromiumremove");keys("Right","Return")
            expect(lambda:not installed("plex"),"Store did not uninstall Chromium app")
            capture("storechromiumremoved")
            for app,version in (("plex","1.0.0"),) if args.web_only else (("smarttube","32.68.0"),("plex","1.0.0")):
                if (data_home/"Isolde/packages/profiles"/profile/app/version/"manifest.json").exists():raise RuntimeError("Uninstalled manifest remains")
            result={"passed":True,"androidLifecycle":not args.web_only,"chromiumLifecycle":True,"androidPackageRemovalVerified":not args.web_only,"profile":profile,"freshProfile":True,"normalProfileUsed":False,"playbackVerified":False,"captures":phases,"visualReview":"pending","webOnly":args.web_only,"nativeAuthentication":args.native_auth,"actualInputActions":len(actions),"manifestsRemoved":True}
            if authorization:
                authorization.revoke();result['authorizationCleanupVerified']=True;result['testAuthorization']=True;result['androidConsentCancelNoWrite']=True
            (evidence/"result.json").write_text(json.dumps(result,indent=2))
    except Exception:
        # Keep the still-running failure UI reviewable before owned-process cleanup.
        if process and process.poll() is None:
            try: capture("storefailure")
            except Exception: pass
        raise
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=25)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        if authorization:authorization.revoke()
        x.XCloseDisplay(display)
        print("EVIDENCE "+str(evidence),flush=True)


if __name__=="__main__":main()
