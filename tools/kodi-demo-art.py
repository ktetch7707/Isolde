#!/usr/bin/env python3
"""User-requested, persistent VM demo art, scraped and cached by Kodi itself."""
import argparse
import base64
import hashlib
import json
import os
import re
from pathlib import Path
import runpy
import socket
import time
from xml.sax.saxutils import escape

PROFILE = '8bdf6077-4613-4b52-9b20-d1ec7931dfcf'
MOVIES = [('The Matrix',603),('Inception',27205),('Interstellar',157336),
          ('The Lord of the Rings: The Fellowship of the Ring',120),
          ('Spirited Away',129),('Gladiator',98),('Star Wars',11),('Toy Story',862)]
SHOWS = [('Once Upon a Time',39272),('Glee',1417)]
YEARS={603:1999,27205:2010,157336:2014,120:2001,129:2001,98:2000,11:1977,862:1995,39272:2011,1417:2009}

KODI_SCRIPT = r'''
import json,time
from pathlib import Path
import xbmc,xbmcaddon,xbmcgui,xbmcvfs
root=Path(xbmcvfs.translatePath('special://profile'))
result={'ok':False,'stage':'scope'}
def rpc(method,params=None):
    value=json.loads(xbmc.executeJSONRPC(json.dumps({'jsonrpc':'2.0','id':1,'method':method,'params':params or {}})))
    if 'error' in value:raise RuntimeError(method+': '+str(value['error']))
    return value['result']
def wait(predicate,message,seconds=100):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        value=predicate()
        if value:return value
        xbmc.sleep(500)
    raise RuntimeError(message)
def listing(kind):
    method,field=('GetMovies','movies') if kind=='movie' else ('GetTVShows','tvshows')
    return rpc('VideoLibrary.'+method,{'properties':['title','file','art','uniqueid','plot','year'],'limits':{'start':0,'end':50}}).get(field,[])
def matched(entry):
    observed=listing(entry['kind'])
    result['observed']=observed
    for item in observed:
        if item['file']!=entry['before']['file']:continue
        method,field=('GetMovieDetails','moviedetails') if entry['kind']=='movie' else ('GetTVShowDetails','tvshowdetails')
        key='movieid' if entry['kind']=='movie' else 'tvshowid'
        try:item=rpc('VideoLibrary.'+method,{key:item[key],'properties':['title','file','art','uniqueid','plot']})[field]
        except RuntimeError as error:
            if 'Invalid params' in str(error):return None
            raise
        identity=str((item.get('uniqueid') or {}).get('tmdb',''))
        if identity and identity!=str(entry['tmdb']):raise RuntimeError('Kodi resolved a different title')
        if item['title']==entry['title'] and any(any(host in item.get('art',{}).get(role,'') for host in ('image.tmdb.org','assets.fanart.tv')) for role in ('poster','fanart','landscape')):return item
    return None
def restore_state(entry,item):
    key='movieid' if entry['kind']=='movie' else 'tvshowid'
    values={key:item[key],'plot':'STUDIO DEMO CLIP — synthetic video, not the feature film or episode.\n\n'+item.get('plot','')}
    if entry['kind']=='movie':
        values.update({k:entry['before'][k] for k in ('playcount','resume','lastplayed','dateadded')})
        values['tag']=['Studio Demo Clip']
    rpc('VideoLibrary.SetMovieDetails' if entry['kind']=='movie' else 'VideoLibrary.SetTVShowDetails',values)
try:
    if json.loads((root/'studio-demo-scope.json').read_text())!={'profile':PLAN['profile'],'media':PLAN['media']}:raise RuntimeError('Not the scoped demo Kodi profile')
    for addon_id in ('metadata.themoviedb.org.python','metadata.tvshows.themoviedb.org.python'):
        addon=xbmcaddon.Addon(addon_id)
        if addon_id=='metadata.themoviedb.org.python':
            wait(lambda:xbmcaddon.Addon(addon_id).getSetting('fetch_posters') in ('true','false'),'Kodi movie artwork defaults unavailable',45)
            addon=xbmcaddon.Addon(addon_id)
        for setting in ('fetch_posters','fanart','landscape','keyart'):
            if addon.getSetting(setting) in ('true','false'):addon.setSettingBool(setting,True)
        if addon.getSetting('language'):addon.setSettingString('language','en-US')
    result['stage']='import source settings'
    xbmc.executebuiltin('ActivateWindow(mediasettings)');xbmc.sleep(700)
    for _ in range(4):
        if 'Expert' in xbmc.getInfoLabel('Control.GetLabel(20)'):break
        xbmc.executebuiltin('Action(settingslevelchange)');xbmc.sleep(300)
    xbmc.executebuiltin('ActivateWindow(home)');xbmc.sleep(300)
    xbmc.executebuiltin('ActivateWindow(mediasettings,videolibrary.import)');xbmc.sleep(500)
    for control in range(30,3000):
        if xbmc.getInfoLabel('Control.GetLabel(%s)'%control).strip().lower()=='import library':
            xbmc.executebuiltin('SetFocus(%s)'%control);xbmc.sleep(250);break
    xbmc.executebuiltin('Action(select)');xbmc.sleep(700)
    dialog=xbmcgui.getCurrentWindowDialogId()
    xbmc.executebuiltin('SetFocus(450)');xbmc.executebuiltin('Action(down)');xbmc.executebuiltin('Action(select)');xbmc.sleep(500)
    if PLAN['import'] not in xbmc.getInfoLabel('Control.GetLabel(412)'):raise RuntimeError('Unexpected source import directory')
    xbmc.executebuiltin('SendClick(%s,413)'%dialog);xbmc.sleep(1800)
    result['items']=[]
    for entry in PLAN['entries']:
        result['stage']='scrape '+entry['title']
        (root/'studio-demo-art-progress.json').write_text(json.dumps(result))
        key='movieid' if entry['kind']=='movie' else 'tvshowid'
        rpc('VideoLibrary.SetMovieDetails' if entry['kind']=='movie' else 'VideoLibrary.SetTVShowDetails',{key:entry['before'][key],'art':{name:None for name in entry['before']['art'] if name!='icon'}})
        params={key:entry['before'][key],'ignorenfo':False,'title':entry['title']}
        if entry['kind']=='tvshow':params['refreshepisodes']=False
        rpc('VideoLibrary.RefreshMovie' if entry['kind']=='movie' else 'VideoLibrary.RefreshTVShow',params)
        item=wait(lambda:matched(entry),'Kodi scraper did not resolve '+entry['title'])
        restore_state(entry,item)
        if entry['kind']=='tvshow':
            episodes=rpc('VideoLibrary.GetEpisodes',{'tvshowid':item['tvshowid'],'properties':['file','art','episode'],'limits':{'start':0,'end':50}}).get('episodes',[])
            for episode in episodes:
                original=next(e for e in entry['episodes'] if e['file']==episode['file'])
                rpc('VideoLibrary.SetEpisodeDetails',{'episodeid':episode['episodeid'],'art':{name:None for name in episode['art'] if name!='icon'}})
                rpc('VideoLibrary.RefreshEpisode',{'episodeid':episode['episodeid'],'ignorenfo':False})
                def refreshed_episode():
                    records=rpc('VideoLibrary.GetEpisodes',{'tvshowid':item['tvshowid'],'properties':['file','art','plot'],'limits':{'start':0,'end':50}}).get('episodes',[])
                    info=next((e for e in records if e['file']==original['file']),None)
                    if not info:return None
                    return info if any(any(host in v for host in ('image.tmdb.org','assets.fanart.tv')) for v in info.get('art',{}).values()) else None
                info=wait(refreshed_episode,'Episode art unavailable')
                rpc('VideoLibrary.SetEpisodeDetails',dict(episodeid=info['episodeid'],playcount=original['playcount'],resume=original['resume'],lastplayed=original['lastplayed'],plot='STUDIO DEMO CLIP — synthetic video.\n\n'+info.get('plot','')))
        result['items'].append({'title':item['title'],'tmdb':entry['tmdb'],'kind':entry['kind'],'art':item['art']})
    result.update(ok=True,stage='complete')
    xbmc.executebuiltin('ActivateWindow(home)');xbmc.executebuiltin('Minimize')
except Exception as error:
    result['error']=str(error)
    xbmc.log('Isolde demo art: '+str(error),xbmc.LOGERROR)
    rollback_errors=[]
    for entry in PLAN['entries']:
        try:
            original=entry['before'];item=wait(lambda:next((i for i in listing(entry['kind']) if i['file']==original['file']),None),'Refresh did not finish before recovery',15)
            key='movieid' if entry['kind']=='movie' else 'tvshowid'
            values={key:item[key],**{k:original[k] for k in ('title','plot','art')}}
            if entry['kind']=='movie':values.update({k:original[k] for k in ('year','playcount','resume','lastplayed','dateadded')})
            rpc('VideoLibrary.SetMovieDetails' if entry['kind']=='movie' else 'VideoLibrary.SetTVShowDetails',values)
            if entry['kind']=='tvshow':
                current=rpc('VideoLibrary.GetEpisodes',{'tvshowid':item[key],'properties':['file'],'limits':{'start':0,'end':50}}).get('episodes',[])
                for old in entry['episodes']:
                    episode=next(e for e in current if e['file']==old['file'])
                    rpc('VideoLibrary.SetEpisodeDetails',dict(episodeid=episode['episodeid'],**{k:old[k] for k in ('playcount','resume','lastplayed')}))
        except Exception as rollback_error:rollback_errors.append(str(rollback_error))
    result.update(metadataRestored=not rollback_errors,rollbackErrors=rollback_errors)
finally:
    (root/'studio-demo-art-result.json').write_text(json.dumps(result,indent=2))
'''

RESTORE_SCRIPT = r'''
import json
from pathlib import Path
import xbmc,xbmcvfs
root=Path(xbmcvfs.translatePath('special://profile'))
result={'ok':False,'stage':'restore saved demo history'}
def rpc(method,params):
    reply=json.loads(xbmc.executeJSONRPC(json.dumps({'jsonrpc':'2.0','id':1,'method':method,'params':params})))
    if 'error' in reply:raise RuntimeError(method+': '+str(reply['error']))
    return reply['result']
try:
    if json.loads((root/'studio-demo-scope.json').read_text())!={'profile':PLAN['profile'],'media':PLAN['media']}:raise RuntimeError('Scope mismatch')
    for entry in PLAN['entries']:
        kind=entry['kind'];field='movies' if kind=='movie' else 'tvshows';key='movieid' if kind=='movie' else 'tvshowid'
        rows=rpc('VideoLibrary.GetMovies' if kind=='movie' else 'VideoLibrary.GetTVShows',{'properties':['file'],'limits':{'start':0,'end':50}})[field]
        item=next(i for i in rows if i['file']==entry['before']['file'])
        if kind=='movie':rpc('VideoLibrary.SetMovieDetails',{key:item[key],**{k:entry['before'][k] for k in ('playcount','resume','lastplayed','dateadded')}})
        else:
            rows=rpc('VideoLibrary.GetEpisodes',{'tvshowid':item[key],'properties':['file'],'limits':{'start':0,'end':50}})['episodes']
            for old in entry['episodes']:
                item=next(i for i in rows if i['file']==old['file'])
                rpc('VideoLibrary.SetEpisodeDetails',{'episodeid':item['episodeid'],**{k:old[k] for k in ('playcount','resume','lastplayed')}})
    result.update(ok=True,stage='history restored')
except Exception as error:result['error']=str(error)
finally:(root/'studio-demo-art-result.json').write_text(json.dumps(result,indent=2))
'''
def checked(path, root):
    if path.resolve()!=path or not path.is_relative_to(root) or path.is_symlink():raise RuntimeError('Unsafe demo path: '+str(path))
    return path

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--receipt',type=Path,required=True);args=parser.parse_args()
    if os.name!='posix' or socket.gethostname()!='isolde-test' or Path.home()!=Path('/home/isolde-dev'):raise RuntimeError('Isolde-Test guest only')
    compile(KODI_SCRIPT,'studio-demo-art.py','exec')
    helper=runpy.run_path(str(args.source/'tools/seed-studio-demo.py'))
    run,rpc,atomic,key=(helper[n] for n in ('run','rpc','atomic','key'))
    import subprocess
    service=subprocess.run(['systemctl','--user','show','isolde.service','--property=ActiveState,SubState,Result,ExecMainStatus,NRestarts'],capture_output=True,text=True,timeout=10)
    journal=subprocess.run(['journalctl','--user','-u','isolde.service','-n','45','--no-pager','-o','cat'],capture_output=True,text=True,timeout=10)
    atomic(args.receipt.parent/'demo-art-service.json',json.dumps({'service':service.stdout,'journal':journal.stdout},indent=2).encode())
    if 'ActiveState=failed' in service.stdout:
        if 'Result=start-limit-hit' not in service.stdout:raise RuntimeError('Service failed; diagnostic collected before further changes')
        run('systemctl','--user','reset-failed','isolde.service')
        run('systemctl','--user','start','isolde.service')
        time.sleep(5)
    home=Path.home();config=home/'.config/Isolde'
    profiles=json.loads((config/'profiles.json').read_text())
    if profiles['active']!=PROFILE or not any(p['id']==PROFILE and p['name']=='Studio Demo' for p in profiles['profiles']):raise RuntimeError('Select the existing Studio Demo profile first')
    media=checked(home/'Videos/Isolde-Studio-Demo'/PROFILE,home)
    userdata=checked(home/'.local/share/Isolde/native-profiles'/PROFILE/'kodi/home/.kodi/userdata',home)
    manifest=json.loads((media/'manifest.json').read_text())
    if manifest['status']!='ready' or manifest['profile']!=PROFILE:raise RuntimeError('Completed Studio Demo required')
    restore_only=bool(manifest.get('realArtwork'))
    directory=userdata/'isolde'
    properties=['title','file','art','uniqueid','plot','year','playcount','resume','lastplayed','dateadded']
    movies=rpc(directory,'VideoLibrary.GetMovies',{'properties':properties,'limits':{'start':0,'end':50}}).get('movies',[])
    shows=rpc(directory,'VideoLibrary.GetTVShows',{'properties':['title','file','art','uniqueid','plot','year'],'limits':{'start':0,'end':50}}).get('tvshows',[])
    if len(movies)!=8 or len(shows)!=2:
        atomic(args.receipt.parent/'demo-art-library-recovery.json',json.dumps({'movies':movies,'shows':shows},indent=2).encode())
        backup=media/'real-art-before.json'
        if not backup.exists() or len(movies)>8 or len(shows)!=2:raise RuntimeError('Unexpected demo library size; recovery diagnostic saved')
        previous=json.loads(backup.read_text())
        if previous['profile']!=PROFILE or previous['media']!=str(media):raise RuntimeError('Recovery scope mismatch')
        for entry in previous['entries']:
            if entry['kind']!='movie' or any(m['file']==entry['before']['file'] for m in movies):continue
            folder=checked(Path(entry['folder']),media)
            rpc(directory,'VideoLibrary.Scan')
            deadline=time.monotonic()+100
            while time.monotonic()<deadline:
                time.sleep(1)
                movies=rpc(directory,'VideoLibrary.GetMovies',{'properties':properties,'limits':{'start':0,'end':50}}).get('movies',[])
                if any(m['file']==entry['before']['file'] for m in movies):break
            else:raise RuntimeError('Kodi could not re-index interrupted demo refresh')
        if len(movies)!=8:raise RuntimeError('Demo movie recovery incomplete')
    baseline={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [config/'profiles.json',*config.rglob('home-layout.json')]}
    release=(home/'.local/share/isolde-appliance/current').resolve()
    imported=checked(media/'art-import',media);imported.mkdir(exist_ok=True)
    plan={'profile':PROFILE,'media':str(media),'import':str(imported),'entries':[]}
    expected=json.loads((media/'expected.json').read_text())
    for old,(title,tmdb) in zip(expected,MOVIES):
        original=next(m for m in movies if m['file']==old['file']);file=checked(Path(original['file']),media)
        plan['entries'].append(dict(kind='movie',title=title,tmdb=tmdb,before=original,folder=str(file.parent),nfo=str(file.with_suffix('.nfo'))))
    for original,(title,tmdb) in zip(sorted(shows,key=lambda s:s['file']),SHOWS):
        folder=checked(Path(original['file']).parent if Path(original['file']).is_file() else Path(original['file']),media)
        episodes=rpc(directory,'VideoLibrary.GetEpisodes',{'tvshowid':original['tvshowid'],'properties':['file','playcount','resume','lastplayed'],'limits':{'start':0,'end':50}}).get('episodes',[])
        if len(episodes)!=3 or any(not checked(Path(e['file']),media).is_file() for e in episodes):raise RuntimeError('Demo episode scope differs')
        plan['entries'].append(dict(kind='tvshow',title=title,tmdb=tmdb,before=original,folder=str(folder),nfo=str(folder/'tvshow.nfo'),episodes=episodes))
    backup=media/'real-art-before.json'
    if backup.exists():
        previous=json.loads(backup.read_text())
        for entry in previous['entries']:
            current=next(i for i in (movies if entry['kind']=='movie' else shows) if i['file']==entry['before']['file'])
            keys=()
            differences={k:{'before':entry['before'][k],'current':current[k]} for k in keys if current[k]!=entry['before'][k]}
            if current['title'] not in (entry['before']['title'],entry['title']) or current['year'] not in (entry['before']['year'],YEARS[entry['tmdb']]):differences['identity']='Unexpected demo metadata'
            for name,value in current['art'].items():
                if value!=entry['before']['art'].get(name) and not value.startswith('image://video@') and not any(host in value for host in ('image.tmdb.org','assets.fanart.tv')):differences['art.'+name]=value
            if differences:
                atomic(args.receipt.parent/'demo-art-state-difference.json',json.dumps({'file':current['file'],'differences':differences},indent=2).encode())
                raise RuntimeError('Previous demo state differs; collected exact fields for recovery')
        for entry in plan['entries']:
            saved=next(e for e in previous['entries'] if e['before']['file']==entry['before']['file'])
            if entry['kind']=='movie':
                entry['before'].update({k:saved['before'][k] for k in ('playcount','resume','lastplayed','dateadded')})
            else:entry['episodes']=saved['episodes']
        atomic(media/('real-art-attempt-'+str(time.time_ns())+'.json'),json.dumps(plan,indent=2).encode())
    else:atomic(backup,json.dumps(plan,indent=2).encode())
    xml=[]
    for entry in plan['entries']:
        nfo=checked(Path(entry['nfo']),media)
        identity=('https://www.themoviedb.org/'+('movie/' if entry['kind']=='movie' else 'tv/')+str(entry['tmdb'])+'\n').encode()
        if nfo.exists():
            if nfo.read_bytes()!=identity:raise RuntimeError('Preserve existing NFO instead of overwriting')
        else:atomic(nfo,identity)
        for name in ('poster.png','landscape.png'):
            old=checked(Path(entry['folder'])/name,media)
            if old.exists() and not restore_only:old.rename(old.with_name(name+'.original-placeholder'))
        content='movies' if entry['kind']=='movie' else 'tvshows'
        addon='metadata.themoviedb.org.python' if entry['kind']=='movie' else 'metadata.tvshows.themoviedb.org.python'
        xml.append('<path><url>'+escape(entry['folder']+'/')+'</url><scanrecursive>0</scanrecursive><usefoldernames>true</usefoldernames><content>'+content+'</content><scraperpath>'+addon+'</scraperpath></path>')
    atomic(imported/'videodb.xml',('<videodb><version>1</version><paths>'+''.join(xml)+'</paths></videodb>').encode())
    sources=userdata/'mediasources.xml';sources_before=sources.read_bytes()
    result_file=userdata/'studio-demo-art-result.json'
    if result_file.exists():
        if json.loads(result_file.read_text()).get('ok') and not restore_only:raise RuntimeError('Demo art already completed; refusing to reset it')
        result_file.rename(result_file.with_name('studio-demo-art-failed-'+str(time.time_ns())+'.json'))
    atomic(media/'real-art-mediasources-before.xml',sources_before)
    run('systemctl','--user','stop','isolde.service')
    try:
        atomic(sources,('<mediasources><network><location id="0">'+escape(str(imported))+'/</location></network></mediasources>').encode())
        atomic(userdata/'studio-demo-art.py',('PLAN='+repr(plan)+'\n'+(RESTORE_SCRIPT if restore_only else KODI_SCRIPT)).encode())
        atomic(userdata/'keymaps/studio-demo-art.xml',b'<keymap><global><keyboard><f8>RunScript(special://profile/studio-demo-art.py)</f8></keyboard></global></keymap>')
    finally:run('systemctl','--user','start','isolde.service')
    def wait(predicate,seconds=45):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            try:
                value=predicate()
                if value:return value
            except (FileNotFoundError,ConnectionRefusedError,socket.timeout):pass
            time.sleep(.5)
        raise RuntimeError('Demo Kodi operation timed out')
    wait(lambda:rpc(directory,'JSONRPC.Ping')=='pong');time.sleep(4)
    env=dict(os.environ)
    for line in run('systemctl','--user','show-environment').splitlines():
        k,_,v=line.partition('=')
        if k in ('DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS'):env[k]=v
    window=next(line.split()[0] for line in run('wmctrl','-lx',env=env).splitlines() if 'kodi' in line.lower())
    run('wmctrl','-i','-a',window,env=env);key(window,'F8',env)
    last=''
    try:
        deadline=time.monotonic()+780
        while not result_file.exists():
            progress=userdata/'studio-demo-art-progress.json'
            if progress.exists():
                stage=json.loads(progress.read_text()).get('stage','')
                if stage!=last:print('KODI '+stage,flush=True);last=stage
            if time.monotonic()>deadline:raise RuntimeError('Kodi scraper timed out; retained original fixture backup')
            time.sleep(1)
        result=json.loads(result_file.read_text());atomic(args.receipt,json.dumps(result,indent=2).encode())
        if not result.get('ok'):raise RuntimeError(str(result))
        checks=[]
        for kind,method,field,idkey in [('movie','GetMovies','movies','movieid'),('tvshow','GetTVShows','tvshows','tvshowid')]:
            records=rpc(directory,'VideoLibrary.'+method,{'properties':['title','art','uniqueid'],'limits':{'start':0,'end':50}})[field]
            for record in records:
                image=rpc(directory,'Isolde.GetArtwork',{'kind':kind,'id':record[idkey]});raw=base64.b64decode(image.get('data',''),validate=True)
                if len(raw)<10000:raise RuntimeError('Real Kodi artwork unavailable: '+record['label'])
                from PIL import Image
                import io
                output=io.BytesIO();Image.open(io.BytesIO(raw)).save(output,format='PNG')
                target=args.receipt.parent/('studio-test-demo-art-'+kind+'-'+str(record[idkey])+'.png');atomic(target,output.getvalue())
                checks.append(dict(title=record['label'],kind=kind,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
        result.update(bridgeArtwork=checks,profile=PROFILE,release=str(release))
        current=rpc(directory,'VideoLibrary.GetMovies',{'properties':['file','title','art'],'limits':{'start':0,'end':50}})['movies']
        for old in expected:
            item=next(m for m in current if m['file']==old['file']);old.update(title=item['title'],art=item['art'].get('fanart',''),poster=item['art'].get('poster',''))
        atomic(media/'expected.json',json.dumps(expected,indent=2).encode())
        manifest['movies']=current
        manifest['shows']=rpc(directory,'VideoLibrary.GetTVShows',{'properties':['title','episode','watchedepisodes'],'limits':{'start':0,'end':50}})['tvshows']
        manifest.update(realArtwork=True,artworkProvider='Kodi TMDB scraper',artworkReceipt=str(args.receipt))
        atomic(media/'manifest.json',json.dumps(manifest,indent=2).encode())
        for path,checksum in baseline.items():
            if hashlib.sha256(Path(path).read_bytes()).hexdigest()!=checksum:raise RuntimeError('A profile layout changed during demo art setup')
        if (home/'.local/share/isolde-appliance/current').resolve()!=release:raise RuntimeError('Installed release changed')
        result.update(layoutsUnchanged=True,syntheticMedia=True);atomic(args.receipt,json.dumps(result,indent=2).encode())
        print('READY: Kodi scraped real metadata/art for eight demo movies and two shows',flush=True)
    finally:
        if not result_file.exists() or not json.loads(result_file.read_text()).get('ok'):
            for entry in plan['entries']:
                for name in ('poster.png','landscape.png'):
                    original=checked(Path(entry['folder'])/name,media);saved=original.with_name(name+'.original-placeholder')
                    if saved.exists() and not original.exists():saved.rename(original)
        atomic(sources,sources_before)
        run('systemctl','--user','restart','isolde.service')
    wait(lambda:rpc(directory,'JSONRPC.Ping')=='pong')
    if result.get('ok'):
        atomic(args.receipt.parent/'demo-art-history-after.json',json.dumps(rpc(directory,'VideoLibrary.GetMovies',{'properties':properties,'limits':{'start':0,'end':50}}),indent=2).encode())
        current=rpc(directory,'VideoLibrary.GetMovies',{'properties':properties,'limits':{'start':0,'end':50}})['movies']
        original=json.loads(backup.read_text())
        for entry in original['entries']:
            if entry['kind']=='movie':
                item=next(m for m in current if m['file']==entry['before']['file'])
                for field in ('playcount','resume','lastplayed','dateadded'):
                    if item[field]!=entry['before'][field]:raise RuntimeError('Saved movie history differs: '+entry['title']+' '+field)
            else:
                show=next(s for s in manifest['shows'] if s['title']==entry['title'])
                episodes=rpc(directory,'VideoLibrary.GetEpisodes',{'tvshowid':show['tvshowid'],'properties':['file','playcount','resume','lastplayed','art'],'limits':{'start':0,'end':50}})['episodes']
                if len(episodes)!=3:raise RuntimeError('Episode count changed')
                for old in entry['episodes']:
                    item=next(e for e in episodes if e['file']==old['file'])
                    for field in ('playcount','resume','lastplayed'):
                        if item[field]!=old[field]:raise RuntimeError('Saved episode history differs: '+field)
                    if not item['art']:raise RuntimeError('Episode art missing after restart')
        result['watchHistoryUnchanged']=True
        result['continueWatchingCount']=sum(m['resume']['position']>0 for m in current)
        result['restartVerified']=True
        atomic(args.receipt,json.dumps(result,indent=2).encode())

if __name__=='__main__':main()
