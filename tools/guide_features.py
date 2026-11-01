"""Remote-only file entry and feature acceptance on a fresh Guide fixture."""
from collections import deque
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import re
from pathlib import Path
import sqlite3
import threading
import time
from datetime import datetime
from xml.etree import ElementTree

PHASES=['guidefeedsetup','guidefeedpreview','guidefeedmapped','guidefeedimported','guidefeednolistings','guidefeedreturn','guidefeedauth','guidefeedoff','guidestationselect','guidestationlogo','guidestationcolor','guideappearanceapplied','guideexport','guideimportdraft','guideimportpreview','guideimported','guidetemplatesaved','guidetemplatereuse','guidecartoonrules','guidecalendarblock','guidecalendarsaved','guidemixedrules','guidemixedpreview','guidemixedcreated','guidedirectdraft','guidedirectpreview','guidedirectimported','guidefeedrefreshfailed']
KEYS=['1','2','3','4','5','6','7','8','9','0','q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k','l','-','Shift','z','x','c','v','b','n','m','_','Del','Space','.','@','#','!','?',',',':','/','Clear','Done']


def remote_text(key,text):
    """Enter through the production onscreen keyboard, using D-pad/Confirm only."""
    at=0;shift=False
    def move(target,values):
        nonlocal at
        queue=deque([(at,[])]);seen={at}
        while queue:
            index,path=queue.popleft()
            if index==target:
                for direction in path:key(direction)
                at=target;return
            for direction,next_index in [('Left',max(0,index-1)),('Right',min(len(values)-1,index+1)),('Up',max(0,index-10)),('Down',min(len(values)-1,index+10))]:
                if next_index not in seen:seen.add(next_index);queue.append((next_index,path+[direction]))
        raise AssertionError('No remote keyboard route')
    move(KEYS.index('Clear'),KEYS);key('Return')
    for char in text:
        upper=char.isupper()
        if upper:
            move(KEYS.index('Shift'),KEYS);key('Return');shift=True
        values=([value.upper() if len(value)==1 and value.isalpha() else value for value in KEYS[:-2]]+['{','}','\\','Clear','Done']) if shift else KEYS
        label='Space' if char==' ' else char
        if label not in values:raise AssertionError('Fixture text needs an unsupported remote keyboard character')
        move(values.index(label),values);key('Return');shift=False
    move(KEYS.index('Done'),KEYS);key('Return')


def definitions(database):
    with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as connection:return [json.loads(row[0]) for row in connection.execute('SELECT definition FROM channels ORDER BY position,id')]


def stream_handler(payload, requests):
    """A seekable HTTP media fixture: Kodi probes the tail before playback."""
    if not payload:raise ValueError('Media fixture must contain bytes')
    class Stream(BaseHTTPRequestHandler):
        def do_HEAD(self):self.serve(False)
        def do_GET(self):self.serve(True)
        def log_message(self,*args):pass
        def serve(self,body):
            self.connection.settimeout(5)
            code=401 if self.path=='/auth' else 200 if self.path=='/stream.ogv' else 404
            header=self.headers.get('Range','');start,end=0,len(payload)-1
            if code==200 and header:
                match=re.fullmatch(r'bytes=(\d*)-(\d*)',header) if len(header)<=128 else None
                if not match or not any(match.groups()):code=416
                elif not match[1]:
                    suffix=int(match[2]);start=max(0,len(payload)-suffix)
                    if suffix==0:code=416
                else:
                    start=int(match[1]);end=min(int(match[2]),end) if match[2] else end
                    if start>end or start>=len(payload):code=416
                if code==200 and start < len(payload) and start <= end:code=206
            requests.append({'method':self.command,'path':self.path,'status':code,'range':header[:128]})
            if len(requests)>128:del requests[:-128]
            content=payload[start:end+1] if code in (200,206) else b''
            self.send_response(code);self.send_header('Content-Type','video/ogg');self.send_header('Content-Length',str(len(content)))
            if code in (200,206):self.send_header('Accept-Ranges','bytes')
            if code==206:self.send_header('Content-Range',f'bytes {start}-{end}/{len(payload)}')
            elif code==416:self.send_header('Content-Range',f'bytes */{len(payload)}')
            self.end_headers()
            if body and content:
                try:self.wfile.write(content)
                except (BrokenPipeError,ConnectionResetError):pass
    return Stream


def exercise(evidence,media,database,directory,local,poster,rpc,capture,wait,shell_active,key):
    from guide_lifecycle import slots
    requests=[];payload=next(media.glob('Isolde Television Fixture/Fixture S01E01.ogv')).read_bytes()
    server=HTTPServer(('127.0.0.1',0),stream_handler(payload,requests));thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    url='http://127.0.0.1:'+str(server.server_port)
    playlist=evidence/'live.m3u';xml=evidence/'native.xml';package=evidence/'channel.json'
    xml.write_bytes((media/'fixture.xml').read_bytes())
    playlist.write_text('#EXTM3U\n#EXTINF:-1 tvg-id="unmapped",Native Live\n'+url+'/stream.ogv\n#EXTINF:-1 tvg-id="no-epg",No Listings\n'+url+'/stream.ogv\n#EXTINF:-1 tvg-id="auth",Authentication Required\n'+url+'/auth\n')
    def keys(*names):
        for name in names:key(name)
    def down(count):
        for _ in range(count):key('Down')
    def text(value):remote_text(key,str(value))
    def channels():return definitions(database)
    def channel():return next(item for item in channels() if item['id']==local['id'])
    before=slots(database)
    try:
        # Start at the compact widget's local-channel row, retained by the base journey.
        keys('Down','Menu','Up','Right','Right','Right','Return','Down','Return')
        capture('guidefeedsetup')
        key('Return');text('t11-feed');keys('Down','Return');text(playlist);keys('Down','Return');text(xml);down(3);key('Return')
        capture('guidefeedpreview');assert len(channels())==3,'Feed preview wrote channels'
        keys('Return','Down','Return');text('isolde-pvr-test');key('Escape')
        capture('guidefeedmapped')
        wait(lambda:len(channels())==6,'Reviewed native M3U/XMLTV generation imported')
        feed=[item for item in channels() if item.get('owner')=='guide-feed'];assert len(feed)==3
        mapped=next(item for item in feed if item['source']['stationId']=='unmapped');assert mapped['source']['epgId']=='isolde-pvr-test'
        with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as connection:
            broadcasts=connection.execute('SELECT title,start_utc,end_utc FROM slots WHERE channel=? ORDER BY start_utc',(mapped['id'],)).fetchall()
        assert len(broadcasts)==72 and all(end-start==3600 for _,start,end in broadcasts)
        source_times=[(program.findtext('title'),int(datetime.strptime(program.attrib['start'],'%Y%m%d%H%M%S %z').timestamp()),int(datetime.strptime(program.attrib['stop'],'%Y%m%d%H%M%S %z').timestamp())) for program in ElementTree.parse(xml).getroot().findall('programme')]
        assert broadcasts==source_times,'Native import shifted source broadcast times'
        capture('guidefeedimported')
        # Sources was the focused header; current local row remains index one.
        down(4);key('Return')
        def streaming():
            if rpc(directory,'Isolde.PlaybackStatus').get('state')!='playing':return False
            players=rpc(directory,'Player.GetActivePlayers')
            # Managed status only becomes playing after the bridge privately
            # matches Kodi's exact file URL. Public identity reads stay path-free.
            return len(players)==1 and players[0].get('type')=='video' and any(request['method']=='GET' and request['path']=='/stream.ogv' for request in requests)
        wait(streaming,'Actual no-EPG native stream plays through Kodi')
        # Keep path-free production observations even if a later authoring
        # step fails. A bridge acknowledgement alone is not visible-video proof.
        samples=[]
        for _ in range(3):
            samples.append({'state':rpc(directory,'Isolde.PlaybackStatus'),'players':rpc(directory,'Player.GetActivePlayers'),'shellActive':bool(shell_active())})
            (evidence/'guide-features.json').write_text(json.dumps({'passed':False,'stage':'native-stream-observation','samples':samples,'httpRequests':requests},indent=2))
            time.sleep(1)
        print('NATIVE_STREAM_OBSERVATION '+json.dumps({'samples':samples,'httpRequests':requests[-8:]}),flush=True)
        assert all(sample['state'].get('state')=='playing' and sample['players'] for sample in samples),'Native stream ended before visible-video review'
        capture('guidefeednolistings');wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='ended','Native stream Stop returned');wait(shell_active,'Native stream automatic focus return',seconds=8)
        capture('guidefeedreturn')
        wait(lambda:rpc(directory,'Isolde.PlaybackStatus').get('state')=='failed' and 'authentication' in rpc(directory,'Isolde.PlaybackStatus').get('reason','').lower(),'Authentication failure is distinct from connection failure')
        assert not rpc(directory,'Player.GetActivePlayers')
        capture('guidefeedauth')
        # Return to Sources, then disable only this newly imported source.
        for _ in range(6):key('Up')
        key('Return');down(5);keys('Return','Down','Return','Down','Down','Return')
        wait(lambda:all(item.get('sourceEnabled') is False for item in channels() if item.get('owner')=='guide-feed'),'Native source OFF committed')
        request_count=len(requests);cached=slots(database);capture('guidefeedoff');assert slots(database)==cached and len(requests)==request_count,'OFF browsing requested source data or changed cached listings'
        keys('Down','Return','Down','Return');wait(lambda:all(item.get('sourceEnabled',True) for item in channels() if item.get('owner')=='guide-feed'),'Native source explicitly enabled')
        keys('Escape','Escape','Down');
        for _ in range(4):key('Up')
        capture('guidestationselect')
        key('Menu');down(15);key('Return');text(poster)
        wait(lambda:bool(channel().get('logo')),'Managed station logo saved')
        key('Escape');capture('guidestationlogo');key('Menu');down(16);keys('Return','Return')
        # Named preset followed by a remote-operated custom color wheel.
        keys('Down','Return','Up','Right','Right','Down','Return','Right','Return')
        capture('guidestationcolor');down(3);key('Return');down(9);key('Return')
        wait(lambda:bool(channel().get('presentation',{}).get('surfaceColor')),'Reviewed station appearance saved')
        key('Escape');capture('guideappearanceapplied');key('Menu');down(14);keys('Return','Return');text(package);keys('Down','Return','Down')
        capture('guideexport');wait(package.is_file,'Portable package atomically exported')
        exported=json.loads(package.read_text());logo=channel()['logo'];assert logo in exported['assets'] and exported.get('slots')
        assert str(media) not in package.read_text() and '://' not in package.read_text(),'Export leaked media paths or provider URLs'
        original_ids={item['id'] for item in channels()};old_slots=slots(database)
        keys('Down','Return');down(6);keys('Return','Return');text(package);keys('Down','Return','Down','Return')
        capture('guideimportdraft');assert {item['id'] for item in channels()}==original_ids
        down(6);key('Return');capture('guideimportpreview')
        wait(lambda:len(channels())==7,'Imported channel preview saved once')
        imported=next(item for item in channels() if item['id'] not in original_ids);assert imported['logo']==logo and imported['presentation']==channel()['presentation']
        assert [row for row in slots(database) if row[1]!=imported['id']]==old_slots
        capture('guideimported')
        key('Menu');down(7);key('Return');down(8);key('Return');text('saved tv')
        template_dir=database.parent/'guide-templates';wait(lambda:template_dir.exists() and len(list(template_dir.glob('*.json')))==1,'Remote rule template saved')
        capture('guidetemplatesaved');keys('Escape','Down','Return')
        # Open a fresh channel and choose the saved template; cancellation writes nothing.
        for _ in range(8):key('Up')
        keys('Left','Left','Return','Return');down(10);key('Return');capture('guidetemplatereuse');key('Return')
        key('Return');text('Cartoon Network');down(3);key('Return');key('Down')
        keys('Right','Right','Right','Right','Down','Right','Right','Down','Return');text('Cartoon Network');key('Down')
        capture('guidecartoonrules');down(5);key('Return');down(4);key('Return');down(5);key('Return')
        capture('guidecalendarblock');keys('Down','Return','Return','Down','Return','Escape')
        # Monday/Tuesday toggles exercise the named weekday draft; retain five days.
        down(6);key('Return');key('Escape');key('Escape')
        # Back to the unsaved template draft; Preview then one Save creates its calendar.
        key('Up');key('Up');key('Return');time.sleep(1);keys('Down','Return')
        wait(lambda:len(channels())==8,'Calendar-template channel created through one Save')
        created=next(item for item in channels() if item['id'] not in original_ids and item['id']!=imported['id']);assert created.get('schedule',{}).get('blocks')
        assert created['name']=='Cartoon Network' and created['rule']=={'field':'studio','op':'contains','value':'Cartoon Network'}
        capture('guidecalendarsaved')
        # Duplicate into mixed media, then build a meaningful nested AND/OR:
        # studio contains Cartoon Network AND (movie OR episode).
        key('Menu');down(9);key('Return');key('Return');text('Mixed TV');down(2);keys('Right','Right','Down','Return','Right')
        down(3);key('Return');key('Right');keys('Down','Return','Down','Right','Down','Down','Return');text('movie');key('Escape')
        down(2);key('Return');key('Down');keys('Left','Left','Left','Down','Down','Return');text('episode');keys('Escape','Escape');down(5)
        capture('guidemixedrules');down(3);key('Return');capture('guidemixedpreview')
        wait(lambda:len(channels())==9,'Mixed movie/episode channel created with nested rules')
        mixed=next(item for item in channels() if item['name']=='Mixed TV');assert set(mixed['mediaKinds'])=={'movie','episode'} and mixed['rule']['op']=='all' and mixed['rule']['rules'][1]['op']=='any'
        with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as connection:items=json.loads(connection.execute('SELECT candidates FROM channels WHERE id=?',(mixed['id'],)).fetchone()[0])
        assert {item['mediaType'] for item in items}=={'movie','episode'} and len(items)==4
        capture('guidemixedcreated')
        # Independently configure the direct-URL/no-XMLTV route, through the
        # same visible source form. Keep Kodi's original PVR fixture untouched.
        for _ in range(10):key('Up')
        keys('Left','Left','Left','Left','Left','Right','Right','Right','Return','Down','Return')
        capture('guidedirectdraft')
        key('Return');text('t11-direct');keys('Down','Return');text('');keys('Down','Return');text('')
        keys('Down','Return');text(url+'/stream.ogv');keys('Down','Return');text('Direct Live');keys('Down','Return')
        capture('guidedirectpreview');assert len(channels())==9,'Direct source preview persisted before Import'
        keys('Down','Return');wait(lambda:len(channels())==10,'Direct URL source imported without XMLTV')
        direct=next(item for item in channels() if item.get('source',{}).get('instance')=='t11-direct')
        assert direct['name']=='Direct Live' and direct['source']['url']==url+'/stream.ogv'
        with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as connection:
            assert connection.execute('SELECT COUNT(*) FROM slots WHERE channel=?',(direct['id'],)).fetchone()[0]==0,'Direct source invented XMLTV listings'
        capture('guidedirectimported')
        # Malformed external source data must fail refresh without replacing
        # the committed lineup or cached intervals; only fixture bytes change.
        key('Return');keys('Down','Return');key('Return');text('t11-feed')
        keys('Down','Return');text(playlist);keys('Down','Return');text(xml)
        keys('Down','Return');text('');keys('Down','Return');text('')
        cached=slots(database);saved=channels();xml_bytes=xml.read_bytes()
        try:
            xml.write_text('<tv><programme');keys('Down','Return');time.sleep(1)
            capture('guidefeedrefreshfailed')
            assert channels()==saved and slots(database)==cached,'Failed source refresh changed committed cache'
        finally:xml.write_bytes(xml_bytes)
        result={key:True for key in ('nativeFeedImport','sourceTimesRetained','explicitEpgMapping','actualNoEpgPlayback','authenticationFailureVisible','sourceOffPreservesCache','managedStationLogo','remoteColorWheel','portableArtwork','portableSchedule','savedRuleTemplate','remoteCalendarPolicy')}
        result.update(cartoonNetworkOneSave=True,mixedNestedRules=True,directSourceImport=True,failedFeedRefreshPreservesCache=True)
        result.update(importedChannelId=imported['id'],calendarChannelId=created['id'],httpRequests=requests,cachedChannelCount=len(channels()))
        (evidence/'guide-features.json').write_text(json.dumps(result,indent=2));return result
    finally:
        server.shutdown();server.server_close();thread.join(timeout=5)
