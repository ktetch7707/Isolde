"""Typed source authoring. Kodi's managed builtin owns sources and database writes."""
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import secrets
import stat
import time

LIMIT=1024*1024
FIELDS={"name","path","category","content","scraper","recursive","folderNames","noUpdate","exclude","remove"}

class Sources:
    def __init__(self, profile, execute, builtin, managed=False, roots=()):
        self.profile=Path(profile);self.execute=execute;self.builtin=builtin;self.managed=managed
        self.directory=self.profile/'isolde'
        self.roots=tuple(roots)
        if len(self.roots)>128 or any(not isinstance(root,str) or not root.startswith('/') or root=='/' or len(root)>4096 for root in self.roots):raise ValueError("Invalid allowed media roots")
        if managed and (self.directory/'source-journal.json').exists():
            with self.locked() as directory:self.recover(directory)

    @contextlib.contextmanager
    def locked(self):
        if not self.managed:raise ValueError("Source editing requires managed Isolde mode")
        self.directory.mkdir(mode=0o700,exist_ok=True)
        directory=os.open('/',os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        try:
            for component in self.directory.absolute().parts[1:]:
                child=os.open(component,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=directory)
                os.close(directory);directory=child
            info=os.fstat(directory)
            if info.st_uid!=os.getuid() or info.st_mode&0o077:raise ValueError("Source control directory must be private")
            fcntl.flock(directory,fcntl.LOCK_EX)
            yield directory
        finally:os.close(directory)

    def read(self,directory,name):
        fd=os.open(name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=directory)
        with os.fdopen(fd,'rb') as stream:
            info=os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or info.st_size>LIMIT:raise ValueError("Unsafe source control file")
            data=stream.read(LIMIT+1)
        if len(data)>LIMIT:raise ValueError("Source control document exceeds limit")
        return json.loads(data)

    def write(self,directory,name,value):
        data=json.dumps(value,separators=(',',':'),ensure_ascii=True).encode()
        if len(data)>LIMIT:raise ValueError("Source control document exceeds limit")
        temporary='.source-'+secrets.token_hex(16)
        fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=directory)
        try:
            with os.fdopen(fd,'wb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
            os.replace(temporary,name,src_dir_fd=directory,dst_dir_fd=directory);os.fsync(directory)
        finally:
            try:os.unlink(temporary,dir_fd=directory)
            except FileNotFoundError:pass

    def native(self,directory,operation,**values):
        token=secrets.token_hex(16)
        self.write(directory,'source-request.json',dict(version=1,token=token,operation=operation,**values))
        self.builtin('IsoldeSources('+token+')')
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            try:reply=self.read(directory,'source-reply.json')
            except FileNotFoundError:reply=None
            if isinstance(reply,dict) and reply.get('token')==token:
                if reply.get('version')!=1 or reply.get('ok') is not True or not isinstance(reply.get('sources'),list) or len(reply['sources'])>128 or type(reply.get('sourceFileExists')) is not bool:
                    raise ValueError("Kodi source operation failed; refresh or recover before retrying")
                return reply
            time.sleep(.04)
        raise ValueError("Managed Kodi source support is unavailable or did not respond; previous transaction requires recovery before another change")

    def finish(self,directory):
        # Journal first: a crash during disposal must never roll back a committed edit.
        try:os.unlink('source-journal.json',dir_fd=directory)
        except FileNotFoundError:pass
        os.fsync(directory)
        for name in ('source-config-backup.xml','source-native-backup.json'):
            try:
                info=os.stat(name,dir_fd=directory,follow_symlinks=False)
                if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid():raise ValueError("Unsafe source recovery file")
                os.unlink(name,dir_fd=directory)
            except FileNotFoundError:pass
        os.fsync(directory)

    def recover(self,directory):
        try:journal=self.read(directory,'source-journal.json')
        except FileNotFoundError:
            self.finish(directory);return
        if not isinstance(journal,dict) or set(journal)!={'version','hadConfig'} or journal['version']!=1 or type(journal['hadConfig']) is not bool:raise ValueError("Invalid source recovery journal")
        self.native(directory,'restore',hadConfig=journal['hadConfig'])
        self.finish(directory)

    def rpc(self,method,params):
        response=json.loads(self.execute(json.dumps(dict(jsonrpc='2.0',id=1,method=method,params=params))))
        if 'error' in response:raise ValueError("Kodi source capability query failed")
        return response.get('result')

    @staticmethod
    def revision(reply):
        return hashlib.sha256(json.dumps(reply['sources'],sort_keys=True,separators=(',',':')).encode()).hexdigest()

    def allowed(self,path):
        return any(isinstance(root,str) and root.startswith('/') and root!='/' and (path.rstrip('/')==root.rstrip('/') or path.startswith(root.rstrip('/')+'/')) for root in self.roots)

    def inventory(self,reply):
        rows=[]
        for source in reply['sources']:
            if not isinstance(source,dict) or not isinstance(source.get('path'),str) or not isinstance(source.get('name'),str):raise ValueError("Invalid Kodi source inventory")
            path=source['path'];local=path.startswith('/') and '\x00' not in path
            row={key:source.get(key) for key in FIELDS if key!='remove'}
            row.update(path=path if local else '',name=source['name'][:160],editable=local and source.get('singlePath') is True and self.allowed(path))
            rows.append(row)
        scrapers=[]
        for content,kind in (('movies','xbmc.metadata.scraper.movies'),('tvshows','xbmc.metadata.scraper.tvshows'),('musicvideos','xbmc.metadata.scraper.musicvideos')):
            response=self.rpc('Addons.GetAddons',dict(type=kind,enabled=True,properties=['name']))
            addons=response.get('addons',[]) if isinstance(response,dict) else []
            if not isinstance(addons,list) or len(addons)>128:raise ValueError("Scraper inventory exceeds limit")
            for addon in addons:
                if isinstance(addon,dict) and isinstance(addon.get('addonid'),str) and isinstance(addon.get('name'),str):scrapers.append(dict(id=addon['addonid'],name=addon['name'][:160],content=content))
        return dict(sources=rows,scrapers=scrapers,revision=self.revision(reply),roots=list(self.roots))

    def draft(self,value,reply):
        if not isinstance(value,dict) or set(value)!=FIELDS:raise ValueError("Invalid source draft")
        for key in ('recursive','folderNames','noUpdate','exclude','remove'):
            if type(value[key]) is not bool:raise ValueError("Source flags must be booleans")
        name,path,category,content,scraper=(value[key] for key in ('name','path','category','content','scraper'))
        if not all(isinstance(item,str) for item in (name,path,category,content,scraper)) or not 1<=len(name)<=160 or any(ord(c)<32 for c in name) or len(path)>4096 or not self.allowed(path):raise ValueError("Choose an allowed local media folder and a bounded name")
        if not path.startswith('/') or os.path.normpath(path)!=path.rstrip('/') or os.path.realpath(path).rstrip('/')!=path.rstrip('/'):raise ValueError("Linked or unsafe source paths are not allowed")
        existing=next((source for source in reply['sources'] if source.get('path')==path and source.get('category')==category),None)
        if not value['remove'] and any(source.get('category')==category and source.get('path')!=path and str(source.get('name','')).casefold()==name.casefold() for source in reply['sources']):raise ValueError("Another source already uses that name; choose a different name")
        if existing and existing.get('singlePath') is not True:raise ValueError("Existing multipath sources are preserved")
        if value['remove']:
            if not existing:raise ValueError("That source no longer exists")
        elif not os.path.isdir(path):raise ValueError("Connect this media folder before saving its source settings")
        if category not in ('video','music') or content not in ('','movies','tvshows','musicvideos','music') or (category=='music')!=(content=='music') or len(scraper)>160:raise ValueError("Unsupported content or scraper")
        if category=='music' and (scraper or not value['recursive'] or value['folderNames'] or value['noUpdate'] or value['exclude']):raise ValueError("Music sources use Kodi's recursive embedded-tag scanner")
        if category=='video' and content and not value['remove']:
            choices=self.inventory(reply)['scrapers']
            if not any(row['id']==scraper and row['content']==content for row in choices):raise ValueError("Choose an installed Kodi scraper for this content")
        if not content and scraper:raise ValueError("A source without content cannot select a scraper")
        if not existing and not value['remove'] and len(reply['sources'])>=128:raise ValueError("Source limit reached")
        return dict(value)

    def handle(self,method,params):
        with self.locked() as directory:
            self.recover(directory)
            current=self.native(directory,'get')
            if method=='Isolde.GetSources' and not params:return self.inventory(current)
            if method not in ('Isolde.PreviewSource','Isolde.ApplySource') or set(params)!=({'source'} if method=='Isolde.PreviewSource' else {'source','revision'}):raise ValueError("Invalid source operation")
            draft=self.draft(params['source'],current)
            revision=self.revision(current)
            if method=='Isolde.PreviewSource':return dict(source=draft,revision=revision,preview=True)
            if params['revision']!=revision:raise ValueError("Sources changed after preview; refresh and preview again")
            if self.rpc('Player.GetActivePlayers',{}):raise ValueError("Stop playback before changing sources")
            self.write(directory,'source-journal.json',dict(version=1,hadConfig=current['sourceFileExists']))
            try:updated=self.native(directory,'set',source=draft,expected=current['sources'])
            except Exception:
                self.recover(directory);raise
            self.finish(directory)
            return dict(applied=True,**self.inventory(updated))
