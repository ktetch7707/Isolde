#!/usr/bin/env python3
"""Resume the exact disposable T11 fixture after a real twelve-hour outage.

The application and its Kodi children run in a denied network namespace. The
oracle SSH/session remains outside it. No clock adjustment or fake playback.
"""
import argparse
try:
    import fcntl
except ImportError:
    fcntl=None  # Reader contracts are host-testable; the actual journey is Linux-only.
from contextlib import closing
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sqlite3
import tempfile
import time
import uuid
from guide_lifecycle import position
from guide_features import remote_text

PHASES=['guidesoakhome','guidesoakguide','guidesoakvideo','guidesoakreturn','guidesoaksearch','guidesoakresults','guidesoakrecovery']


def slots(database):
    with closing(sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True)) as connection:
        return list(connection.execute('SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots ORDER BY channel,start_utc'))


def current(database,channel):
    wall=time.time()
    with closing(sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True)) as connection:
        return connection.execute('SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots WHERE channel=? AND start_utc<=? AND end_utc>?',(channel,wall,wall)).fetchone()


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('executable',type=Path);parser.add_argument('--receipt',type=Path,required=True);parser.add_argument('--cold-reopen',action='store_true',help='Actual stopped-process reopen, with no twelve-hour elapsed-time claim');args=parser.parse_args()
    if fcntl is None:raise RuntimeError('Use the Linux VM runner for this journey')
    wrapper=json.loads(args.receipt.read_text());receipt_path=Path(wrapper['receipt'])
    if receipt_path.is_symlink() or not re.fullmatch(r'/tmp/isolde-tv-acceptance-[A-Za-z0-9_-]+/guide-soak-ready.json',str(receipt_path)):raise RuntimeError('Unsafe twelve-hour-off receipt')
    receipt=json.loads(receipt_path.read_text());origin=receipt_path.parent;profile=receipt['profile'];database=Path(receipt['database']);executable=args.executable.resolve()
    if str(uuid.UUID(profile))!=profile or database!=origin/'config/Isolde/profiles'/profile/'guide.sqlite' or database.resolve()!=database or receipt.get('schemaVersion')!=1:raise RuntimeError('Invalid isolated soak identity')
    if hashlib.sha256(executable.read_bytes()).hexdigest()!=receipt['executableSha256'] or wrapper['executableSha256']!=receipt['executableSha256']:raise RuntimeError('Soak executable differs from the frozen candidate')
    elapsed=time.time()-receipt['stoppedAtUtc'];boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    if elapsed<0:raise RuntimeError('Shutdown receipt is in the future')
    if not args.cold_reopen and (elapsed<43200 or (boot==receipt['bootId'] and time.monotonic()-receipt['stoppedAtMonotonic']<43200)):raise RuntimeError('The real twelve-hour-off window is not complete; no clock simulation is accepted')
    existing={row[0]:list(row) for row in slots(database)}
    if any(existing.get(row[0])!=row for row in receipt['nativeSlots']):raise RuntimeError('Committed native slots changed while the fixture was off')
    fixture_spec=importlib.util.spec_from_file_location('tv_fixture_rpc',Path(__import__('guide_features').__file__).with_name('tv-library-acceptance.py'));fixture=importlib.util.module_from_spec(fixture_spec);fixture_spec.loader.exec_module(fixture);rpc=fixture.rpc;run=fixture.run
    lock=open('/tmp/isolde-home-components-acceptance.lock','a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    evidence=Path(tempfile.mkdtemp(prefix='isolde-tv-acceptance-'));print('Fixture evidence: '+str(evidence),flush=True)
    env=dict(os.environ)
    for line in run('systemctl','--user','show-environment').splitlines():
        key,_,value=line.partition('=')
        if key in {'DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS'}:env[key]=value
    env.update(XDG_CONFIG_HOME=str(origin/'config'),XDG_CACHE_HOME=str(origin/'cache'),QT_QPA_PLATFORM='xcb')
    os.environ.update({key:env[key] for key in ('DISPLAY','XAUTHORITY') if key in env})
    net_program=evidence/'denied-launch.py'
    from guide_offline import PROBE,command
    net_program.write_text(PROBE)
    active=subprocess.run(['systemctl','--user','is-active','--quiet','isolde.service']).returncode==0;process=None
    try:
        if active:run('systemctl','--user','stop','isolde.service')
        with (evidence/'shell.log').open('w') as log:
            # The first reopen consumes this uninterrupted off-time window.
            # A failed graphical attempt cannot later masquerade as another
            # twelve-hour outage using the original shutdown timestamp.
            attempt=evidence/'guide-cold-reopen-started.json' if args.cold_reopen else args.receipt.with_name('guide-soak-started.json')
            if args.cold_reopen:
                # Any reopen ends the original uninterrupted outage, including
                # failed attempts. Keep optional long-soak claims impossible.
                try:
                    marker_fd=os.open(args.receipt.with_name('guide-soak-started.json'),os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
                except FileExistsError:pass
                else:
                    with os.fdopen(marker_fd,'w') as marker:
                        json.dump({'mode':'cold-reopen','startedAtUtc':time.time(),'evidence':str(evidence)},marker);marker.flush();os.fsync(marker.fileno())
            descriptor=os.open(attempt,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
            with os.fdopen(descriptor,'w') as marker:
                json.dump({'startedAtUtc':time.time(),'elapsedOffSeconds':elapsed,'mode':'cold-reopen' if args.cold_reopen else 'twelve-hour-soak','receiptSha256':hashlib.sha256(receipt_path.read_bytes()).hexdigest(),'evidence':str(evidence)},marker);marker.flush();os.fsync(marker.fileno())
            process=subprocess.Popen([str(executable),'--windowed'] if args.cold_reopen else command(['/usr/bin/python3',str(net_program),str(evidence),os.readlink('/proc/self/ns/net'),str(executable)]),env=env,stdout=log,stderr=log)
            if args.cold_reopen:
                (evidence/'network-denial.json').write_text(json.dumps({'mode':'host-inherited','pid':process.pid,'offlineNamespace':False}))
            def wait(predicate,label,seconds=35):
                deadline=time.monotonic()+seconds
                while time.monotonic()<deadline:
                    value=predicate()
                    if value:print(label,flush=True);return value
                    if process.poll() is not None:raise RuntimeError('Offline application exited: '+str(evidence))
                    time.sleep(0.2)
                raise RuntimeError(label+' timed out')
            def descendants():
                found={process.pid};queue=[process.pid]
                while queue:
                    parent=queue.pop()
                    try:children=Path(f'/proc/{parent}/task/{parent}/children').read_text().split()
                    except FileNotFoundError:continue
                    for value in children:
                        child=int(value)
                        if child not in found:found.add(child);queue.append(child)
                        if len(found)>128:raise RuntimeError('Unbounded offline process tree')
                return found
            def windows():return [line.split(None,4) for line in run('wmctrl','-lp',env=env).splitlines() if len(line.split(None,4))==5]
            shell=wait(lambda:next((row for row in windows() if int(row[2]) in descendants() and (evidence/'network-denial.json').exists() and int(row[2])==json.loads((evidence/'network-denial.json').read_text())['pid']),None),'Cold shell window ready')
            run('wmctrl','-i','-a',shell[0],env=env);time.sleep(3)
            def key(name):
                from guide_remote_keys import send
                send(env,shell[0],name)
            def capture(phase):
                print(phase.upper()+'_READY '+str(evidence),flush=True);wait(lambda:(evidence/(phase+'-captured')).exists(),phase+' reviewed capture/input',110)
            capture('guidesoakhome')
            directory=Path.home()/'.local/share/Isolde/native-profiles'/profile/'kodi/home/.kodi/userdata/isolde'
            def bridge_ready():
                try:return rpc(directory,'JSONRPC.Version')
                except OSError:return None
            wait(bridge_ready,'Offline private Kodi bridge responds')
            channels=[]
            with __import__('sqlite3').connect('file:'+str(database)+'?mode=ro',uri=True) as connection:channels=[json.loads(row[0]) for row in connection.execute('SELECT definition FROM channels ORDER BY position,id')]
            def ready_target():
                for channel in channels:
                    if channel.get('owner'):continue
                    program=current(database,channel['id']);wall=time.time()
                    if program and program[7]=='ready' and wall-program[5]>=5 and program[6]-wall>=35:return channel
                return None
            target=wait(ready_target,'Ready local program for cold return' if args.cold_reopen else 'Ready local program for the actual twelve-hour return',seconds=140)
            target_index=next(i for i,c in enumerate(channels) if c['id']==target['id'])
            if len(channels)>64:raise RuntimeError('Unbounded cold-reopen fixture lineup')
            (evidence/'soak-target.json').write_text(json.dumps({'channel':target,'lineupIndex':target_index,'elapsedOffSeconds':None if args.cold_reopen else elapsed},indent=2))
            if args.cold_reopen:print('GUIDE_REOPEN_TARGET '+str(target_index),flush=True)
            capture('guidesoakguide')
            observed=wait(lambda:position(rpc,directory,current(database,target['id'])),'Actual cold-reopen local identity and elapsed seek verified' if args.cold_reopen else 'Actual twelve-hour-off local identity and elapsed seek verified')
            capture('guidesoakvideo');wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Cold playback Stop')
            wait(lambda:int(run('xprop','-root','_NET_ACTIVE_WINDOW',env=env).strip().split()[-1],16)==int(shell[0],16),'Offline automatic exact-window return',8)
            capture('guidesoakreturn');capture('guidesoaksearch')
            # Use only D-pad/Confirm to enter a local search on the disconnected app.
            run('wmctrl','-i','-a',shell[0],env=env)
            remote_text(key,'fixture');time.sleep(2);capture('guidesoakresults')
            matches=rpc(directory,'VideoLibrary.GetEpisodes',{'properties':['title'],'filter':{'field':'title','operator':'contains','value':'Fixture'},'limits':{'start':0,'end':50}}).get('episodes',[])
            if not matches:raise RuntimeError('Offline local search produced no fixture matches')
            capture('guidesoakrecovery')
            after={row[0]:list(row) for row in slots(database)}
            if any(after.get(row[0])!=row for row in receipt['nativeSlots']):raise RuntimeError('Restart changed committed seven-day native slots')
            proof=json.loads((evidence/'network-denial.json').read_text());result={'passed':True,'realTwelveHourOff':not args.cold_reopen,'actualColdReopen':True,'elapsedOffSeconds':None if args.cold_reopen else elapsed,'secondsSinceFixturePreparation':elapsed,'offlineNamespace':not args.cold_reopen,'actualLocalElapsedSeek':True,'committedSevenDaySlotsUnchanged':True,'localSearchMatches':len(matches),'automaticNativeReturn':True,'normalProfileUsed':False,'captures':PHASES,'playback':observed,'network':proof,'visualReview':'pending'}
            (evidence/'result.json').write_text(json.dumps(result,indent=2))
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=12)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        if active:run('systemctl','--user','start','isolde.service')


if __name__=='__main__':main()
