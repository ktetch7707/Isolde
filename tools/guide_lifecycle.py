"""Additional bounded, real-player T11 acceptance; fresh VM fixture only."""
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import time

PHASES = ['guidecontinuousready','guidecontinuousvideo','guidecontinuousnext','guidesurfed','guidesurfreturn',
          'guidedeletedready','guidedeletedfile','guidedeletedreturn','guiderepairselect','guiderepaircontext',
          'guiderepairreview','guiderepairconfirm','guiderepaired']


def slots(database):
    with sqlite3.connect('file:'+str(database)+'?mode=ro', uri=True) as connection:
        return list(connection.execute('SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots ORDER BY channel,start_utc'))


def current(database, channel):
    wall=time.time()
    with sqlite3.connect('file:'+str(database)+'?mode=ro', uri=True) as connection:
        row=connection.execute('SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots WHERE channel=? AND start_utc<=? AND end_utc>?',(channel,wall,wall)).fetchone()
    return row


def position(rpc, directory, expected):
    if not expected or expected[7]!='ready':return None
    status=rpc(directory,'Isolde.PlaybackStatus')
    if status.get('state')!='playing': return None
    players=rpc(directory,'Player.GetActivePlayers')
    if len(players)!=1: return None
    player=players[0]['playerid']
    item=rpc(directory,'Player.GetItem',{'playerid':player,'properties':[]})['item']
    clock=rpc(directory,'Player.GetProperties',{'playerid':player,'properties':['time']})['time']
    observed=time.time(); actual=clock['hours']*3600+clock['minutes']*60+clock['seconds']+clock.get('milliseconds',0)/1000
    if item.get('type')!=expected[2] or item.get('id')!=expected[3] or not expected[5]<=observed<expected[6] or abs(actual-(observed-expected[5]))>3:return None
    return {'observedAtUtc':observed,'actualPositionSeconds':actual,'expectedPositionSeconds':observed-expected[5],'slot':expected,'item':{'type':item.get('type'),'id':item.get('id')},'toleranceSeconds':3}


def fixture_file(media, value):
    path=Path(value);root=media.absolute();home=Path.home().resolve()
    if root.resolve()!=root or not root.is_relative_to(home) or path.resolve()!=path.absolute() or not path.is_relative_to(root) or path.is_symlink() or not path.is_file():raise RuntimeError('Refusing a non-fixture media mutation')
    for parent in (path.parent,*path.parents):
        if parent==home:break
        if parent.is_symlink() or os.path.ismount(parent):raise RuntimeError('Refusing fixture mutation through a link or mount')
    return path


def episode_fixture(media, identity, rpc, directory):
    episodes=rpc(directory,'VideoLibrary.GetEpisodes',{'properties':['season','episode'],'limits':{'start':0,'end':50}}).get('episodes',[])
    matches=[item for item in episodes if item.get('episodeid')==identity]
    if len(matches)!=1 or (matches[0].get('season'),matches[0].get('episode')) not in {(0,1),(1,1),(1,2)}:raise RuntimeError('Missing unique known fixture episode')
    item=matches[0]
    return fixture_file(media,media/'Isolde Television Fixture'/f"Fixture S{item['season']:02}E{item['episode']:02}.ogv")


def exercise(evidence, media, database, directory, local, native, rpc, capture, wait, shell_active):
    result={};before=slots(database)
    capture('guidecontinuousready')
    first=wait(lambda:position(rpc,directory,current(database,local['id'])),'Local channel playing before natural boundary')
    capture('guidecontinuousvideo')
    def transitioned():
        expected=current(database,local['id'])
        return position(rpc,directory,expected) if expected and expected[0]!=first['slot'][0] else None
    result['transition']=wait(transitioned,'Actual natural-end channel transition and elapsed seek',seconds=135)
    capture('guidecontinuousnext')
    duplicate=next(channel for channel in native if channel['id']!=local['id'])
    result['surf']=wait(lambda:position(rpc,directory,current(database,duplicate['id'])),'D-pad channel surfing follows saved lineup',seconds=35)
    capture('guidesurfed')
    wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Surf playback stopped')
    wait(shell_active,'Automatic surf return focus',seconds=8)
    capture('guidesurfreturn')
    assert slots(database)==before,'Continuous playback or surfing changed committed slots'
    # Reserve a full review interval before removing the selected current file.
    target=current(database,local['id'])
    if target[6]-time.time()<100:
        end=target[6]
        wait(lambda:time.time()>end+1,'Next complete program for deleted-file review',seconds=102)
        target=current(database,local['id'])
    capture('guidedeletedready')
    original=episode_fixture(media,target[3],rpc,directory);backup=original.with_name(original.name+'.isolde-unavailable')
    if backup.exists():raise RuntimeError('Refusing to replace retained fixture evidence')
    original.rename(backup)
    try:
        capture('guidedeletedfile')
        def unavailable():
            rows=slots(database);affected=[row for row in rows if row[2:4]==target[2:4]]
            return rows if affected and all(row[7]=='unavailable' for row in affected) else None
        after=wait(unavailable,'Actual missing-file tune marks only matching Kodi identities unavailable')
        assert [r for r in after if r[2:4]!=target[2:4]]==[r for r in before if r[2:4]!=target[2:4]],'Missing-file feedback changed unrelated slots'
        wait(shell_active,'Automatic unavailable-file return focus',seconds=8)
        capture('guidedeletedreturn');capture('guiderepairselect');capture('guiderepaircontext');capture('guiderepairreview');capture('guiderepairconfirm')
        def repaired():
            rows=slots(database);replacement=next(row for row in rows if row[0]==target[0])
            return (rows,replacement) if replacement[7]=='ready' and replacement[2:4]!=target[2:4] else None
        repaired_rows,replacement=wait(repaired,'Reviewed repair replaces the actual missing program')
        assert [r for r in repaired_rows if r[0]!=target[0]]==[r for r in after if r[0]!=target[0]],'Targeted repair changed another slot'
        assert replacement[0:2]==target[0:2] and replacement[5:7]==target[5:7]
        capture('guiderepaired')
        result.update(deletedSlot=target,replacement=replacement,continuousTransition=True,remoteSurf=True,actualDeletedFile=True,targetedRepair=True,unaffectedSlotsPreserved=True)
        (evidence/'guide-lifecycle.json').write_text(json.dumps(result,indent=2))
        return result
    finally:
        # Only the verified fresh guest fixture's renamed hard link is restored.
        if original.exists() or backup.is_symlink() or not backup.is_file():raise RuntimeError('Fixture restore conflict; retained backup requires inspection')
        backup.rename(original)


def prepare_soak(evidence, database, profile, media, executable):
    rows=[row for row in slots(database) if row[2]!='channel']
    receipt={'schemaVersion':1,'evidence':str(evidence),'profile':profile,'media':str(media),'database':str(database),
             'stoppedAtUtc':time.time(),'stoppedAtMonotonic':time.monotonic(),
             'bootId':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
             'executableSha256':hashlib.sha256(executable.read_bytes()).hexdigest(),'nativeSlots':rows}
    (evidence/'guide-soak-ready.json').write_text(json.dumps(receipt,indent=2))
    print('T11 twelve-hour-off fixture prepared: '+str(evidence/'guide-soak-ready.json'),flush=True)
