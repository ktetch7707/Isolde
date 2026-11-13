"""Kodi-owned smart playlists created from validated Isolde library views."""
import os
import base64
import json
import stat
import fcntl
import hashlib
from pathlib import Path
import uuid
import xml.etree.ElementTree as ET

def playlist_name(value):
    return (isinstance(value,str) and 1<=len(value)<=128 and value not in ('.','..')
            and not any(c in value for c in '/\\:%') and not any(ord(c)<32 for c in value)
            and value.lower().endswith(('.xsp','.m3u','.m3u8')))

MAX_PLAYLIST=1024*1024
JOURNAL='.isolde-playlist-recovery.json'


class Playlists:
    def __init__(self, profile, managed=False):
        self.profile = Path(profile).absolute()
        self.managed = managed
        if managed:
            directory=self._playlist_dir();os.close(directory)

    def _playlist_dir(self):
        directory=os.open("/",os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        try:
            for part in self.profile.parts[1:]:
                if part in (".",".."):raise ValueError("Invalid profile path")
                child=os.open(part,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=directory)
                os.close(directory);directory=child
            for part in ("playlists","video"):
                try:
                    os.mkdir(part,mode=0o700,dir_fd=directory);os.fsync(directory)
                except FileExistsError:pass
                child=os.open(part,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=directory)
                os.close(directory);directory=child
            fcntl.flock(directory,fcntl.LOCK_EX)
            self._recover(directory)
            return directory
        except Exception:
            os.close(directory)
            raise

    def _read(self,directory,name,limit=MAX_PLAYLIST):
        fd=os.open(name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=directory)
        try:
            info=os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or info.st_size>limit:
                raise ValueError('Playlist must be an owned regular file within the supported size limit')
            with os.fdopen(os.dup(fd),'rb') as source:raw=source.read(limit+1)
            if len(raw)>limit:raise ValueError('Playlist exceeds the supported size limit')
            return raw
        finally:os.close(fd)

    def _write(self,directory,name,data):
        temporary='.isolde-write-'+str(uuid.uuid4())
        try:
            fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=directory)
            with os.fdopen(fd,'wb') as output:
                output.write(data);output.flush();os.fsync(output.fileno())
            os.rename(temporary,name,src_dir_fd=directory,dst_dir_fd=directory)
        finally:
            try:os.unlink(temporary,dir_fd=directory)
            except FileNotFoundError:pass

    def _recover(self,directory):
        try:raw=self._read(directory,JOURNAL,4*MAX_PLAYLIST)
        except FileNotFoundError:return
        record=json.loads(raw)
        if not isinstance(record,dict) or record.get('version')!=1 or set(record)!={'version','previous'}:
            raise ValueError('Invalid playlist recovery record')
        previous=record['previous']
        if not isinstance(previous,dict) or not 1<=len(previous)<=2 or any(not playlist_name(n) for n in previous):
            raise ValueError('Invalid playlist recovery identities')
        decoded={}
        for name,value in previous.items():
            decoded[name]=None if value is None else base64.b64decode(value,validate=True)
            if decoded[name] is not None and len(decoded[name])>MAX_PLAYLIST:raise ValueError('Invalid playlist recovery size')
        for name,value in decoded.items():
            if value is None:
                try:os.unlink(name,dir_fd=directory)
                except FileNotFoundError:pass
            else:self._write(directory,name,value)
        os.fsync(directory);os.unlink(JOURNAL,dir_fd=directory);os.fsync(directory)

    def _change(self,directory,changes):
        previous={}
        for name in changes:
            try:previous[name]=base64.b64encode(self._read(directory,name)).decode('ascii')
            except FileNotFoundError:previous[name]=None
        receipt=json.dumps({'version':1,'previous':previous}).encode()
        self._write(directory,JOURNAL,receipt)
        os.fsync(directory)
        try:
            for name,value in changes.items():
                if value is None:os.unlink(name,dir_fd=directory)
                else:self._write(directory,name,value)
            os.fsync(directory)
            os.unlink(JOURNAL,dir_fd=directory)
            os.fsync(directory)
        except BaseException:
            try:os.stat(JOURNAL,dir_fd=directory,follow_symlinks=False)
            except FileNotFoundError:self._write(directory,JOURNAL,receipt)
            self._recover(directory)
            raise

    def create(self, params):
        if not self.managed or set(params) != {"name", "kind", "view", "value"}:
            raise ValueError("Managed Kodi playlist creation required")
        name, kind, view, value = (params[key] for key in ("name", "kind", "view", "value"))
        if (not isinstance(name,str) or not 1<=len(name.strip())<=80 or any(ord(c)<32 for c in name)
            or kind not in ("movies","shows") or view not in ("all","recent","continue","watched","search","tag","genre")
            or not isinstance(value,str) or len(value)>160 or any(ord(c)<32 for c in value)
            or (view in ("search","tag","genre") and not value.strip())):
            raise ValueError("Choose a supported library view and playlist name")
        root=ET.Element("smartplaylist",{"type":"movies" if kind=="movies" else "tvshows"})
        ET.SubElement(root,"name").text=name.strip()
        ET.SubElement(root,"match").text="all"
        if view in ("search","tag","genre"):
            rule=ET.SubElement(root,"rule",{"field":"title" if view=="search" else view,"operator":"contains" if view=="search" else "is"})
            ET.SubElement(rule,"value").text=value.strip()
        elif view=="continue":ET.SubElement(root,"rule",{"field":"inprogress","operator":"true"})
        elif view=="watched":
            rule=ET.SubElement(root,"rule",{"field":"lastplayed","operator":"after"})
            ET.SubElement(rule,"value").text="1970-01-01"
        ET.SubElement(root,"order",{"direction":"descending" if view in ("recent","continue","watched") else "ascending"}).text="dateadded" if view=="recent" else "lastplayed" if view in ("continue","watched") else "title"
        data=ET.tostring(root,encoding="utf-8",xml_declaration=True)
        filename="Isolde-"+str(uuid.uuid4())+".xsp"
        temporary=".isolde-playlist-"+str(uuid.uuid4())
        # Walk from / with openat/O_NOFOLLOW. Subsequent operations use the
        # directory descriptor, so replacing a pathname cannot redirect writes.
        directory=self._playlist_dir()
        published=False
        temporary_created=False
        try:
            with os.scandir(directory) as entries:
                if sum(playlist_name(entry.name) for entry in entries)>=128:raise ValueError("At most 128 saved video playlists")
            descriptor=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=directory)
            temporary_created=True
            with os.fdopen(descriptor,"wb") as output:
                output.write(data);output.flush();os.fsync(output.fileno())
            # Hard-link publication refuses collisions without overwriting an
            # existing Kodi playlist, then fsyncs its owning directory.
            os.link(temporary,filename,src_dir_fd=directory,dst_dir_fd=directory,follow_symlinks=False);published=True
            os.fsync(directory)
        except Exception:
            if published:os.unlink(filename,dir_fd=directory)
            raise
        finally:
            try:
                if temporary_created:os.unlink(temporary,dir_fd=directory)
            except FileNotFoundError:pass
            finally:os.close(directory)
        return {"saved":True,"name":name.strip(),"playlistName":filename,"kind":kind}

    def _xml(self,raw):
        if b'<!DOCTYPE' in raw.upper() or b'<!ENTITY' in raw.upper():raise ValueError('Playlist entities are unsupported')
        root=ET.fromstring(raw)
        if root.tag!='smartplaylist' or root.get('type') not in ('movies','tvshows','episodes'):
            raise ValueError('Choose a supported Kodi video smart playlist')
        return root

    def delete(self, params):
        if not self.managed or set(params)!={'name'} or not playlist_name(params.get('name')):
            raise ValueError('Managed Kodi playlist deletion required')
        directory=self._playlist_dir()
        try:
            self._read(directory,params['name'])
            self._change(directory,{params['name']:None})
        finally:os.close(directory)
        return {'deleted':True,'name':params['name']}

    def rename(self, params):
        if not self.managed or set(params)!={'name','newName'}:
            raise ValueError('Managed Kodi playlist rename required')
        name,new_name=params['name'],params['newName']
        if not playlist_name(name) or not playlist_name(new_name) or Path(name).suffix.lower()!=Path(new_name).suffix.lower():
            raise ValueError('Keep the playlist type and choose a valid new name')
        directory=self._playlist_dir()
        try:
            raw=self._read(directory,name)
            if name!=new_name:
                try:os.stat(new_name,dir_fd=directory,follow_symlinks=False)
                except FileNotFoundError:pass
                else:raise ValueError('A playlist with the new name already exists')
                if name.lower().endswith('.xsp'):
                    root=self._xml(raw);label=root.find('name')
                    if label is None:label=ET.SubElement(root,'name')
                    label.text=Path(new_name).stem
                    raw=ET.tostring(root,encoding='utf-8',xml_declaration=True)
                self._change(directory,{new_name:raw,name:None})
        finally:os.close(directory)
        return {'renamed':True,'name':name,'newName':new_name}

    def sort(self,params):
        if not self.managed or set(params)!={'name','sort','direction'} or not playlist_name(params.get('name')):
            raise ValueError('Managed Kodi smart-playlist sort required')
        if params['sort'] not in ('title','year','dateadded','lastplayed','episode') or params['direction'] not in ('ascending','descending'):
            raise ValueError('Choose a supported smart-playlist order')
        directory=self._playlist_dir()
        try:
            root=self._xml(self._read(directory,params['name']))
            if params['sort']=='episode' and root.get('type')!='episodes':raise ValueError('Episode order requires an episode playlist')
            for order in list(root.findall('order')):root.remove(order)
            ET.SubElement(root,'order',{'direction':params['direction']}).text=params['sort']
            self._change(directory,{params['name']:ET.tostring(root,encoding='utf-8',xml_declaration=True)})
        finally:os.close(directory)
        return {'reordered':True,'name':params['name']}

    def _draft(self,root,view,value):
        if (view not in ('all','search','tag','genre','continue','watched','recent') or not isinstance(value,str)
            or len(value)>160 or any(ord(c)<32 for c in value) or view in ('search','tag','genre') and not value.strip()):
            raise ValueError('Choose a supported bounded playlist rule')
        for child in list(root):
            if child.tag not in ('name',):root.remove(child)
        ET.SubElement(root,'match').text='all'
        if view in ('search','tag','genre'):
            rule=ET.SubElement(root,'rule',{'field':'title' if view=='search' else view,'operator':'contains' if view=='search' else 'is'})
            ET.SubElement(rule,'value').text=value.strip()
        elif view=='continue':ET.SubElement(root,'rule',{'field':'inprogress','operator':'true'})
        elif view=='watched':
            rule=ET.SubElement(root,'rule',{'field':'lastplayed','operator':'after'});ET.SubElement(rule,'value').text='1970-01-01'
        ET.SubElement(root,'order',{'direction':'descending' if view in ('recent','continue','watched') else 'ascending'}).text='dateadded' if view=='recent' else 'lastplayed' if view in ('continue','watched') else 'title'
        return root

    def preview(self,params,execute):
        if not self.managed or set(params)!={'name','view','value'} or not playlist_name(params.get('name')):raise ValueError('Choose a playlist rule')
        directory=self._playlist_dir()
        try:
            raw=self._read(directory,params['name'])
            root=self._draft(self._xml(raw),params['view'],params['value'])
        finally:os.close(directory)
        kind=root.get('type');method={'movies':'GetMovies','tvshows':'GetTVShows','episodes':'GetEpisodes'}[kind]
        query={'properties':['title'],'limits':{'start':0,'end':50},'sort':{'method':root.findtext('order'),'order':root.find('order').get('direction')}}
        rule=root.find('rule')
        if rule is not None:query['filter']={'field':rule.get('field'),'operator':rule.get('operator'),'value':rule.findtext('value','')}
        reply=json.loads(execute(json.dumps({'jsonrpc':'2.0','id':1,'method':'VideoLibrary.'+method,'params':query})))
        result=reply.get('result',{});items=result.get(kind);limits=result.get('limits',{})
        if (not isinstance(items,list) or len(items)>50 or limits.get('start')!=0 or limits.get('end')!=len(items)
            or type(limits.get('total')) is not int or not len(items)<=limits['total']<=10000000):raise ValueError('Invalid Kodi preview page')
        media={'movies':'movie','tvshows':'tvshow','episodes':'episode'}[kind];rows=[]
        for item in items:
            identity=item.get(media+'id');title=item.get('title',item.get('label'))
            if type(identity) is not int or identity<1 or not isinstance(title,str) or not 1<=len(title)<=160:raise ValueError('Invalid preview member')
            rows.append({'title':title,'mediaType':media,'mediaId':identity})
        return {'items':rows,'total':limits['total'],'revision':hashlib.sha256(raw).hexdigest()}

    def edit(self,params):
        if not self.managed or set(params)!={'name','view','value','revision'} or not playlist_name(params.get('name')):
            raise ValueError('Managed Kodi playlist rule edit required')
        directory=self._playlist_dir()
        try:
            raw=self._read(directory,params['name'])
            if hashlib.sha256(raw).hexdigest()!=params['revision']:raise ValueError('Playlist changed; preview the rule again')
            root=self._draft(self._xml(raw),params['view'],params['value'])
            self._change(directory,{params['name']:ET.tostring(root,encoding='utf-8',xml_declaration=True)})
        finally:os.close(directory)
        return {'edited':True,'name':params['name']}

    def _members(self,raw):
        text=raw.decode('utf-8');records=[];pending=[];header=[]
        for line in text.splitlines(keepends=True):
            if line.strip() and not line.startswith('#'):
                records.append(''.join(pending)+line);pending=[]
            elif not records and not pending and line.startswith(('#EXTM3U','#PLAYLIST:')):header.append(line)
            else:pending.append(line)
        if not 1<=len(records)<=256:raise ValueError('Choose a playlist of 1–256 members')
        return header,records,pending

    def members(self,params):
        if not self.managed or set(params)!={'name'} or not playlist_name(params.get('name')) or not params['name'].lower().endswith(('.m3u','.m3u8')):
            raise ValueError('Choose a member playlist')
        directory=self._playlist_dir()
        try:raw=self._read(directory,params['name'])
        finally:os.close(directory)
        _,records,_=self._members(raw);rows=[]
        for index,record in enumerate(records):
            lines=record.splitlines();label=next((line.partition(',')[2] for line in lines if line.startswith('#EXTINF:') and ',' in line),'')
            if not label:label='Member '+str(index+1)
            rows.append({'index':index,'title':label[:160]})
        return {'members':rows,'revision':hashlib.sha256(raw).hexdigest()}

    def reorderPlaylist(self, params):
        if not self.managed or set(params)!={'name','order','revision'} or not playlist_name(params.get('name')):
            raise ValueError('Managed Kodi member-playlist reorder required')
        order=params['order']
        if not isinstance(order,list) or not 1<=len(order)<=256 or any(type(i) is not int for i in order):
            raise ValueError('Choose a complete order of at most 256 members')
        directory=self._playlist_dir()
        try:
            name=params['name']
            if not name.lower().endswith(('.m3u','.m3u8')):raise ValueError('Use smart-playlist sort for rule-based playlists')
            raw=self._read(directory,name)
            if hashlib.sha256(raw).hexdigest()!=params['revision']:raise ValueError('Playlist changed; reload its members before editing')
            header,records,pending=self._members(raw)
            if len(records)>256 or len(order)!=len(records) or set(order)!=set(range(len(records))):
                raise ValueError('Order must include every playlist member exactly once')
            # Keep EXTINF and other per-member metadata attached to their member.
            data=(''.join(header)+''.join(records[i].rstrip('\r\n')+'\n' for i in order)+''.join(pending)).encode('utf-8')
            self._change(directory,{name:data})
        finally:os.close(directory)
        return {'reordered':True,'name':params['name'],'count':len(records)}
